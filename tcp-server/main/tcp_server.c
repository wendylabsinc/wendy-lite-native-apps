#include "tcp_server.h"

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "payload.h"

#define TCP_SERVER_BACKLOG     1
#define TCP_SERVER_TASK_STACK  4096
#define TCP_SERVER_TASK_PRIO   5

/* How long to wait for the peer to close after we are done sending. */
#define TCP_SERVER_DRAIN_MS    2000

/* A client that connects and then stops reading would otherwise park the task
 * in send() forever, and no later client could be served. */
#define TCP_SERVER_SEND_TIMEOUT_MS 10000

static const char *TAG = "tcp_server";

static void _set_timeout(int fd, int option, int ms)
{
    struct timeval tv = {
        .tv_sec  = ms / 1000,
        .tv_usec = (ms % 1000) * 1000,
    };
    setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv));
}

/* Sends PAYLOAD_TOTAL_BYTES to fd. Returns what actually reached the stack,
 * which is short only when the peer went away mid-transfer. */
static size_t _stream(int fd)
{
    const uint8_t *chunk = payload_chunk();
    size_t sent = 0;

    while (sent < PAYLOAD_TOTAL_BYTES) {
        size_t remaining = PAYLOAD_TOTAL_BYTES - sent;
        size_t left = remaining < PAYLOAD_CHUNK_BYTES ? remaining : PAYLOAD_CHUNK_BYTES;
        const uint8_t *p = chunk;

        /* send() may take less than it was offered. Resume where it stopped
         * rather than restarting the chunk, or the pattern breaks. */
        while (left > 0) {
            ssize_t n = send(fd, p, left, 0);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    ESP_LOGW(TAG, "client stopped reading after %u bytes", (unsigned)sent);
                } else {
                    ESP_LOGE(TAG, "send() failed after %u bytes: %d", (unsigned)sent, errno);
                }
                return sent;
            }
            if (n == 0) {
                /* Not expected for a non-zero length; bail rather than spin. */
                ESP_LOGE(TAG, "send() returned 0 after %u bytes", (unsigned)sent);
                return sent;
            }
            p += n;
            left -= n;
            sent += n;
        }
    }

    return sent;
}

/* Half-closes, then waits for the peer's own close. Without the drain, data
 * still sitting unread in the receive queue makes lwIP abort the connection
 * with an RST, and the client loses the tail of the transfer. */
static void _close_gracefully(int fd)
{
    shutdown(fd, SHUT_WR);
    _set_timeout(fd, SO_RCVTIMEO, TCP_SERVER_DRAIN_MS);

    char sink[64];
    while (recv(fd, sink, sizeof(sink), 0) > 0) {
        /* discard whatever the client still had in flight */
    }

    close(fd);
}

static void _log_result(size_t sent, int64_t us)
{
    if (us <= 0) {
        us = 1;
    }

    /* bytes * 8 bits / microseconds gives Mbit/s; scale by 1000 for kbit/s and
     * keep it integer, so the log costs no float formatting. */
    unsigned kbps = (unsigned)((uint64_t)sent * 8000u / (uint64_t)us);

    ESP_LOGI(TAG, "sent %u bytes in %u ms (%u kbit/s)",
             (unsigned)sent, (unsigned)(us / 1000), kbps);
}

static void _server_task(void *arg)
{
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
        .sin_port        = htons(TCP_SERVER_PORT),
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_fd, TCP_SERVER_BACKLOG) < 0) {
        ESP_LOGE(TAG, "listen() failed: %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    /* Binding before the station has an address is fine: lwIP accepts it and
     * connections start arriving once the DHCP lease lands. */
    ESP_LOGI(TAG, "listening on port %d", TCP_SERVER_PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            ESP_LOGE(TAG, "accept() failed: %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "client %s connected", inet_ntoa(client_addr.sin_addr));

        _set_timeout(client_fd, SO_SNDTIMEO, TCP_SERVER_SEND_TIMEOUT_MS);

        /* Every segment but the last is full-MSS, so Nagle has nothing to
         * coalesce; all it can do is hold the short final segment back while
         * the FIN waits behind it. */
        int nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        int64_t started = esp_timer_get_time();
        size_t sent = _stream(client_fd);
        int64_t elapsed = esp_timer_get_time() - started;

        _close_gracefully(client_fd);
        _log_result(sent, elapsed);
    }
}

esp_err_t tcp_server_start(void)
{
    if (!payload_chunk()) {
        ESP_LOGE(TAG, "payload_init() must run first");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ok = xTaskCreate(_server_task, "bw_tcp", TCP_SERVER_TASK_STACK, NULL,
                                TCP_SERVER_TASK_PRIO, NULL);

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
