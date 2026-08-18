#include "server.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mdns.h"

#include "cam_capture.h"

static const char *TAG = "server";

#define SERVER_REQUEST_LEN 12

// Largest payload we are willing to buffer on the receive side. Anything
// bigger means the stream is out of sync, since no known message comes close.
#define SERVER_MAX_RX_PAYLOAD 64

// How many requests may wait for a frame. Beyond that the client is outrunning
// the device and the extra requests are refused.
#define SERVER_REQUEST_QUEUE_LEN 8

#define SERVER_MDNS_INSTANCE "AV Source"
#define SERVER_MDNS_SERVICE "_wendy_lite_av_source"
#define SERVER_MDNS_PROTO "_tcp"

static int s_listen = -1;
static int s_client = -1;
static TaskHandle_t s_task;

// Incoming bytes, parsed message by message.
static uint8_t s_rx[SERVER_HEADER_LEN + SERVER_MAX_RX_PAYLOAD];
static size_t s_rx_len;

// Requests wait here, oldest first, while frames are captured and sent out.
static uint32_t s_requests[SERVER_REQUEST_QUEUE_LEN];
static size_t s_request_head;
static size_t s_request_count;

// Frame being chunked out, owned by this module.
static uint8_t *s_frame;
static size_t s_frame_len;
static size_t s_frame_sent;
static bool s_last_built;
static uint32_t s_chunk;
static uint32_t s_frame_num;
static uint32_t s_frame_ts;
static uint32_t s_frame_request_id;

// Message being written to the socket, possibly across several send() calls.
static uint8_t s_msg[SERVER_MAX_MSG_LEN];
static size_t s_msg_len;
static size_t s_msg_off;

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static bool push_request(uint32_t request_id)
{
    if (s_request_count == SERVER_REQUEST_QUEUE_LEN) {
        return false;
    }

    s_requests[(s_request_head + s_request_count) % SERVER_REQUEST_QUEUE_LEN] = request_id;
    s_request_count++;
    return true;
}

// Only valid while the queue is not empty.
static uint32_t pop_request(void)
{
    uint32_t request_id = s_requests[s_request_head];
    s_request_head = (s_request_head + 1) % SERVER_REQUEST_QUEUE_LEN;
    s_request_count--;
    return request_id;
}

static void release_frame(void)
{
    if (s_frame) {
        heap_caps_free(s_frame);
        s_frame = NULL;
    }
    s_frame_len = 0;
    s_frame_sent = 0;
    s_last_built = false;
    s_chunk = 0;
    s_msg_len = 0;
    s_msg_off = 0;
}

static void close_client(void)
{
    if (s_client < 0) {
        return;
    }

    close(s_client);
    s_client = -1;
    s_rx_len = 0;
    s_request_head = 0;
    s_request_count = 0;
    release_frame();
}

static void accept_client(void)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int fd = accept(s_listen, (struct sockaddr *)&addr, &addr_len);
    if (fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "accept failed: %d", errno);
        }
        return;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "cannot make the client socket non-blocking: %d", errno);
        close(fd);
        return;
    }

    // A whole message fits in one segment, so Nagle would only ever delay the
    // tail of a frame while waiting for data that is not coming.
    int nodelay = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    s_client = fd;
    s_rx_len = 0;
    s_request_head = 0;
    s_request_count = 0;
    s_frame_num = 0;

    ESP_LOGI(TAG, "client connected from %s:%u", inet_ntoa(addr.sin_addr), (unsigned)ntohs(addr.sin_port));
}

static void handle_request(const uint8_t *payload)
{
    uint8_t channel = payload[0];
    if (channel != SERVER_CHANNEL_VIDEO) {
        ESP_LOGW(TAG, "unsupported channel %u", (unsigned)channel);
        return;
    }

    // The number of frames and the delay are carried by the protocol but not
    // honored yet: one request yields exactly one frame.
    uint32_t request_id = get_u32(payload + 8);

    if (!push_request(request_id)) {
        ESP_LOGW(TAG, "dropping request 0x%08x, %d are already queued",
                 (unsigned)request_id, SERVER_REQUEST_QUEUE_LEN);
    }
}

static void on_readable(void)
{
    ssize_t received = recv(s_client, s_rx + s_rx_len, sizeof(s_rx) - s_rx_len, 0);
    if (received == 0) {
        ESP_LOGI(TAG, "client disconnected");
        close_client();
        return;
    }
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;
        }
        ESP_LOGW(TAG, "recv failed: %d", errno);
        close_client();
        return;
    }

    s_rx_len += (size_t)received;

    while (s_rx_len >= SERVER_HEADER_LEN) {
        if (s_rx[0] != SERVER_MAGIC) {
            ESP_LOGE(TAG, "bad magic 0x%02x, dropping client", s_rx[0]);
            close_client();
            return;
        }

        uint8_t type = s_rx[1];
        size_t payload_len = get_u16(s_rx + 2);
        if (payload_len > SERVER_MAX_RX_PAYLOAD) {
            // Not something we can skip over reliably, the stream is lost.
            ESP_LOGE(TAG, "payload of %u bytes is too large, dropping client", (unsigned)payload_len);
            close_client();
            return;
        }
        if (s_rx_len < SERVER_HEADER_LEN + payload_len) {
            break;
        }

        if (type == SERVER_MSG_TYPE_REQUEST && payload_len == SERVER_REQUEST_LEN) {
            handle_request(s_rx + SERVER_HEADER_LEN);
        } else {
            ESP_LOGW(TAG, "ignoring message type %u with a %u byte payload",
                     (unsigned)type, (unsigned)payload_len);
        }

        size_t consumed = SERVER_HEADER_LEN + payload_len;
        s_rx_len -= consumed;
        memmove(s_rx, s_rx + consumed, s_rx_len);
    }
}

// Grabs a frame and arms the chunking state. This blocks the select loop for
// the duration of the capture, which is acceptable with a single client.
static void capture_frame(uint32_t request_id)
{
    uint8_t *jpeg = NULL;
    size_t jpeg_len = 0;
    int width = 0;
    int height = 0;

    esp_err_t err = cam_capture_jpeg(&jpeg, &jpeg_len, &width, &height);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cam_capture_jpeg failed: %s", esp_err_to_name(err));
        return;
    }

    s_frame = jpeg;
    s_frame_len = jpeg_len;
    s_frame_sent = 0;
    s_last_built = false;
    s_chunk = 0;
    s_frame_ts = (uint32_t)esp_timer_get_time();
    s_frame_request_id = request_id;
    s_msg_len = 0;
    s_msg_off = 0;

    ESP_LOGI(TAG, "frame %u: %u bytes, %dx%d, request 0x%08x",
             (unsigned)s_frame_num, (unsigned)jpeg_len, width, height, (unsigned)s_frame_request_id);
}

static void build_chunk(void)
{
    size_t remaining = s_frame_len - s_frame_sent;
    size_t data_len = remaining > SERVER_MAX_CHUNK_DATA ? (size_t)SERVER_MAX_CHUNK_DATA : remaining;
    bool last = data_len == remaining;
    uint32_t chunk = s_chunk & 0x7fffff;

    s_msg[0] = SERVER_MAGIC;
    s_msg[1] = SERVER_MSG_TYPE_DATA;
    put_u16(s_msg + 2, (uint16_t)(SERVER_DATA_HEADER_LEN + data_len));

    uint8_t *payload = s_msg + SERVER_HEADER_LEN;
    payload[0] = SERVER_CHANNEL_VIDEO;
    // last_chunk is the top bit of the 24 bit word holding the chunk number.
    payload[1] = (uint8_t)((last ? 0x80 : 0x00) | ((chunk >> 16) & 0x7f));
    payload[2] = (uint8_t)(chunk >> 8);
    payload[3] = (uint8_t)chunk;
    put_u32(payload + 4, s_frame_num);
    put_u32(payload + 8, s_frame_ts);
    // The host timestamp mirrors the frame one until the clocks are synced.
    put_u32(payload + 12, s_frame_ts);
    put_u32(payload + 16, s_frame_request_id);
    memcpy(payload + SERVER_DATA_HEADER_LEN, s_frame + s_frame_sent, data_len);

    s_msg_len = SERVER_HEADER_LEN + SERVER_DATA_HEADER_LEN + data_len;
    s_msg_off = 0;
    s_frame_sent += data_len;
    s_last_built = last;
    s_chunk++;
}

static void on_writable(void)
{
    while (s_frame) {
        if (s_msg_off == s_msg_len) {
            if (s_last_built) {
                release_frame();
                s_frame_num++;
                return;
            }
            build_chunk();
        }

        ssize_t sent = send(s_client, s_msg + s_msg_off, s_msg_len - s_msg_off, 0);
        if (sent > 0) {
            s_msg_off += (size_t)sent;
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // Send buffer full, select() tells us when to resume.
            return;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        ESP_LOGW(TAG, "send failed: %d", errno);
        close_client();
        return;
    }
}

static void server_task(void *arg)
{
    (void)arg;

    while (true) {
        fd_set read_fds;
        fd_set write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        int max_fd;
        if (s_client < 0) {
            FD_SET(s_listen, &read_fds);
            max_fd = s_listen;
        } else {
            // Only one client at a time: while one is connected the listening
            // socket stays out of the set and further connections wait in the
            // backlog.
            FD_SET(s_client, &read_fds);
            if (s_frame) {
                FD_SET(s_client, &write_fds);
            }
            max_fd = s_client;
        }

        if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            ESP_LOGE(TAG, "select failed: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (s_client < 0) {
            if (FD_ISSET(s_listen, &read_fds)) {
                accept_client();
            }
            continue;
        }

        if (FD_ISSET(s_client, &read_fds)) {
            on_readable();
        }
        if (s_client >= 0 && FD_ISSET(s_client, &write_fds)) {
            on_writable();
        }
        if (s_client >= 0 && s_request_count > 0 && !s_frame) {
            capture_frame(pop_request());
        }
    }
}

esp_err_t server_start(void)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = cam_capture_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cam_capture_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listen < 0) {
        ESP_LOGE(TAG, "socket failed: %d", errno);
        return ESP_FAIL;
    }

    int reuse = 1;
    setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_listen, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind to port %d failed: %d", SERVER_PORT, errno);
        close(s_listen);
        s_listen = -1;
        return ESP_FAIL;
    }

    if (listen(s_listen, 1) < 0) {
        ESP_LOGE(TAG, "listen failed: %d", errno);
        close(s_listen);
        s_listen = -1;
        return ESP_FAIL;
    }

    int flags = fcntl(s_listen, F_GETFL, 0);
    if (flags < 0 || fcntl(s_listen, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "cannot make the listening socket non-blocking: %d", errno);
        close(s_listen);
        s_listen = -1;
        return ESP_FAIL;
    }

    // mDNS is already up, wendy_core initializes it once WiFi is connected. A
    // failure here only makes the server harder to find, so it is not fatal.
    err = mdns_service_add(SERVER_MDNS_INSTANCE, SERVER_MDNS_SERVICE, SERVER_MDNS_PROTO, SERVER_PORT, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_service_add failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "registered mDNS service %s.%s", SERVER_MDNS_SERVICE, SERVER_MDNS_PROTO);
    }

    if (xTaskCreate(server_task, "server", 4096, NULL, 5, &s_task) != pdPASS) {
        close(s_listen);
        s_listen = -1;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "listening on port %d", SERVER_PORT);
    return ESP_OK;
}
