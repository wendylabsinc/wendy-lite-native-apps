#include "tls_server.h"

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "mbedtls/ssl.h"

#include "payload.h"
#include "wendy_conf.h"

#define TLS_SERVER_BACKLOG     1
/* The handshake is what needs the room, not the send loop. Same figure as the
 * wendy_server task, which does the same job. */
#define TLS_SERVER_TASK_STACK  8192
#define TLS_SERVER_TASK_PRIO   5

/* A client that opens a socket and then says nothing must not park the task
 * forever. esp_tls polls the elapsed time between retries, so the socket needs
 * its own receive timeout too, or mbedtls_net_recv() blocks and the handshake
 * deadline can never be reached. */
#define TLS_SERVER_HANDSHAKE_TIMEOUT_MS 10000
#define TLS_SERVER_SEND_TIMEOUT_MS      10000
#define TLS_SERVER_DRAIN_MS             2000

static const char *TAG = "tls_server";

static void _set_timeout(int fd, int option, int ms)
{
    struct timeval tv = {
        .tv_sec  = ms / 1000,
        .tv_usec = (ms % 1000) * 1000,
    };
    setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv));
}

/* Sends PAYLOAD_TOTAL_BYTES over the session. Returns what was actually
 * written, which is short only when the peer went away mid-transfer. */
static size_t _stream(esp_tls_t *tls)
{
    const uint8_t *chunk = payload_chunk();
    size_t sent = 0;

    while (sent < PAYLOAD_TOTAL_BYTES) {
        size_t remaining = PAYLOAD_TOTAL_BYTES - sent;
        size_t left = remaining < PAYLOAD_CHUNK_BYTES ? remaining : PAYLOAD_CHUNK_BYTES;
        const uint8_t *p = chunk;

        /* esp_tls_conn_write() fragments internally into records of
         * CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN and returns early when the socket
         * fills up, so resume where it stopped.
         *
         * Resuming with exactly (p + n, left - n) is a correctness
         * requirement, not a style choice. A short return leaves mbedTLS
         * holding an encrypted record it has not flushed, and its contract is
         * that the retry repeats the same call; ssl_write_real() then flushes
         * that held record but reports a count derived from the length of the
         * *new* call. Asking for anything other than the exact remainder would
         * therefore be told a byte count that does not match what went out,
         * and the stream would silently drift. */
        while (left > 0) {
            ssize_t n = esp_tls_conn_write(tls, p, left);
            if (n < 0) {
                if (n == ESP_TLS_ERR_SSL_WANT_WRITE || n == ESP_TLS_ERR_SSL_WANT_READ) {
                    /* Only reachable once the send timeout has fired, so yield
                     * rather than spinning the CPU against a stalled client. */
                    vTaskDelay(1);
                    continue;
                }
                ESP_LOGE(TAG, "write failed after %u bytes: -0x%04x",
                         (unsigned)sent, (unsigned)(-n));
                return sent;
            }
            if (n == 0) {
                /* Not expected for a non-zero length; bail rather than spin. */
                ESP_LOGE(TAG, "write returned 0 after %u bytes", (unsigned)sent);
                return sent;
            }
            p += n;
            left -= n;
            sent += n;
        }
    }

    return sent;
}

/* esp_tls_server_session_delete() only cleans up and frees: it neither tells
 * the peer the session is over nor closes the socket, so do both here. Without
 * the close_notify, a client such as `openssl s_client` reports the transfer
 * as an unexpected EOF rather than a clean shutdown. */
static void _close_gracefully(esp_tls_t *tls)
{
    mbedtls_ssl_context *ssl = esp_tls_get_ssl_context(tls);
    if (ssl) {
        int ret;
        while ((ret = mbedtls_ssl_close_notify(ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                break;
            }
        }
    }

    int fd = -1;
    if (esp_tls_get_conn_sockfd(tls, &fd) == ESP_OK && fd >= 0) {
        shutdown(fd, SHUT_WR);

        /* Read off the client's own close_notify and anything else it sent.
         * Data left unread in the receive queue is what makes lwIP answer
         * close() with an RST, discarding whatever is still queued to send. */
        _set_timeout(fd, SO_RCVTIMEO, TLS_SERVER_DRAIN_MS);
        char sink[64];
        while (recv(fd, sink, sizeof(sink), 0) > 0) {
            /* discard */
        }

        close(fd);
    }

    esp_tls_server_session_delete(tls);
}

static void _log_result(size_t sent, int64_t us)
{
    if (us <= 0) {
        us = 1;
    }

    unsigned kbps = (unsigned)((uint64_t)sent * 8000u / (uint64_t)us);

    ESP_LOGI(TAG, "sent %u bytes in %u ms (%u kbit/s)",
             (unsigned)sent, (unsigned)(us / 1000), kbps);
}

static void _server_task(void *arg)
{
    /* The wendy_conf getters are only valid once wendy_core_init() has run
     * wendy_conf_init(), which app_main() guarantees before starting us.
     *
     * Always the built-in self-signed certificate, never the provisioned
     * identity that wendy_server would prefer: this firmware is a bandwidth
     * target, so it authenticates nothing. The spans are DER, embedded with
     * EMBED_FILES and therefore not NUL-terminated -- pass the size exactly. */
    struct wendy_conf_span cert = wendy_conf_get_default_certificate();
    struct wendy_conf_span key = wendy_conf_get_default_private_key();

    esp_tls_cfg_server_t cfg = {
        .servercert_buf   = cert.data,
        .servercert_bytes = cert.size,
        .serverkey_buf    = key.data,
        .serverkey_bytes  = key.size,
        /* no cacert_buf: no client certificate is requested or verified */
        .tls_handshake_timeout_ms = TLS_SERVER_HANDSHAKE_TIMEOUT_MS,
    };

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port        = htons(TLS_SERVER_PORT),
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_fd, TLS_SERVER_BACKLOG) < 0) {
        ESP_LOGE(TAG, "listen() failed: %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "listening on port %d, no client certificate required", TLS_SERVER_PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            ESP_LOGE(TAG, "accept() failed: %d", errno);
            continue;
        }

        esp_tls_t *tls = esp_tls_init();
        if (!tls) {
            ESP_LOGE(TAG, "esp_tls_init() failed");
            close(client_fd);
            continue;
        }

        _set_timeout(client_fd, SO_RCVTIMEO, TLS_SERVER_HANDSHAKE_TIMEOUT_MS);

        if (esp_tls_server_session_create(&cfg, client_fd, tls) != 0) {
            ESP_LOGE(TAG, "TLS handshake with %s failed", inet_ntoa(client_addr.sin_addr));
            close(client_fd);
            esp_tls_server_session_delete(tls);
            continue;
        }

        /* Worth one line: a client that negotiates ChaCha20-Poly1305 measures
         * software crypto, while AES-GCM uses the chip's accelerator. Without
         * this the two results look inexplicably different. */
        ESP_LOGI(TAG, "client %s connected, %s", inet_ntoa(client_addr.sin_addr),
                 mbedtls_ssl_get_ciphersuite(esp_tls_get_ssl_context(tls)));

        _set_timeout(client_fd, SO_SNDTIMEO, TLS_SERVER_SEND_TIMEOUT_MS);

        int nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        int64_t started = esp_timer_get_time();
        size_t sent = _stream(tls);
        int64_t elapsed = esp_timer_get_time() - started;

        _close_gracefully(tls);
        _log_result(sent, elapsed);
    }
}

esp_err_t tls_server_start(void)
{
    if (!payload_chunk()) {
        ESP_LOGE(TAG, "payload_init() must run first");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ok = xTaskCreate(_server_task, "bw_tls", TLS_SERVER_TASK_STACK, NULL,
                                TLS_SERVER_TASK_PRIO, NULL);

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
