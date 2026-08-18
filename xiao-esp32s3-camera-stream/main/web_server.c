#include "web_server.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "face_cam.h"
#include "qr_scan.h"

static const char *TAG = "web_server";

#define WEB_SERVER_PORT 80
/* Large enough for FACE_CAM_MAX_FACES fully populated face entries. */
#define FACES_JSON_MAX 2560

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server;
static SemaphoreHandle_t s_mutex;
/* Last captured frames, owned by this module; valid when jpeg != NULL. */
static face_cam_frame_t s_latest;
static qr_scan_frame_t s_latest_qr;

static int format_faces_json(const face_cam_frame_t *frame, char *buf, size_t size)
{
    size_t off = 0;

#define APPEND(...)                                                 \
    do {                                                            \
        int n_ = snprintf(buf + off, size - off, __VA_ARGS__);      \
        if (n_ < 0 || (size_t)n_ >= size - off) {                   \
            return -1;                                              \
        }                                                           \
        off += n_;                                                  \
    } while (0)

    APPEND("{\"width\":%d,\"height\":%d,\"faces\":[", frame->width, frame->height);
    for (int i = 0; i < frame->num_faces; i++) {
        const face_info_t *f = &frame->faces[i];
        APPEND("%s{\"score\":%.3f,\"box\":{\"x1\":%d,\"y1\":%d,\"x2\":%d,\"y2\":%d}",
               i ? "," : "", (double)f->score, f->x1, f->y1, f->x2, f->y2);
        if (f->has_keypoints) {
            APPEND(",\"keypoints\":{"
                   "\"left_eye\":[%d,%d],\"right_eye\":[%d,%d],\"nose\":[%d,%d],"
                   "\"mouth_left\":[%d,%d],\"mouth_right\":[%d,%d]}",
                   f->left_eye.x, f->left_eye.y, f->right_eye.x, f->right_eye.y,
                   f->nose.x, f->nose.y, f->mouth_left.x, f->mouth_left.y,
                   f->mouth_right.x, f->mouth_right.y);
        }
        APPEND("}");
    }
    APPEND("]}");

#undef APPEND

    return (int)off;
}

static esp_err_t send_json(httpd_req_t *req, const char *json, int len)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, len);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t capture_get_handler(httpd_req_t *req)
{
    face_cam_frame_t frame;
    esp_err_t err = face_cam_capture(&frame);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "capture failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
    }

    char json[FACES_JSON_MAX];
    int len = format_faces_json(&frame, json, sizeof(json));
    if (len < 0) {
        face_cam_frame_free(&frame);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "faces json too large");
    }

    ESP_LOGI(TAG, "captured %d bytes, %d face(s)", (int)frame.jpeg_len, frame.num_faces);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    face_cam_frame_free(&s_latest);
    s_latest = frame;
    xSemaphoreGive(s_mutex);

    return send_json(req, json, len);
}

/* Serves *slot_jpeg (a "latest frame" JPEG owned by this module). Copies it
 * out under the mutex so a concurrent capture can't free it mid-send. */
static esp_err_t serve_jpeg_slot(httpd_req_t *req, uint8_t *const *slot_jpeg, const size_t *slot_len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t *jpeg = NULL;
    size_t len = *slot_len;
    bool have_frame = *slot_jpeg != NULL;
    if (have_frame) {
        jpeg = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
        if (jpeg) {
            memcpy(jpeg, *slot_jpeg, len);
        }
    }
    xSemaphoreGive(s_mutex);

    if (!have_frame) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "nothing captured yet");
    }
    if (!jpeg) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }

    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t err = httpd_resp_send(req, (const char *)jpeg, len);
    heap_caps_free(jpeg);
    return err;
}

static esp_err_t image_get_handler(httpd_req_t *req)
{
    return serve_jpeg_slot(req, &s_latest.jpeg, &s_latest.jpeg_len);
}

static esp_err_t faces_get_handler(httpd_req_t *req)
{
    char json[FACES_JSON_MAX];
    int len = -1;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool have_frame = s_latest.jpeg != NULL;
    if (have_frame) {
        len = format_faces_json(&s_latest, json, sizeof(json));
    }
    xSemaphoreGive(s_mutex);

    if (!have_frame) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "nothing captured yet, GET /capture first");
    }
    if (len < 0) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "faces json too large");
    }
    return send_json(req, json, len);
}

/* Appends src to buf at *off as a quoted JSON string, escaping '"', '\' and
 * non-ASCII/control bytes. Returns false when buf is too small. */
static bool json_append_string(char *buf, size_t size, size_t *off, const char *src, size_t src_len)
{
    size_t o = *off;
    if (o + 1 >= size) {
        return false;
    }
    buf[o++] = '"';
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (o + 2 >= size) {
                return false;
            }
            buf[o++] = '\\';
            buf[o++] = (char)c;
        } else if (c < 0x20 || c >= 0x80) {
            if (o + 6 >= size) {
                return false;
            }
            o += snprintf(buf + o, size - o, "\\u%04x", c);
        } else {
            if (o + 1 >= size) {
                return false;
            }
            buf[o++] = (char)c;
        }
    }
    if (o + 1 >= size) {
        return false;
    }
    buf[o++] = '"';
    buf[o] = '\0';
    *off = o;
    return true;
}

/* Returns a heap JSON string (caller frees) or NULL on alloc failure. */
static char *format_codes_json(const qr_scan_frame_t *frame, int *out_len)
{
    // Worst case: every payload byte escaped as \u00xx.
    size_t size = 256;
    for (int i = 0; i < frame->num_codes; i++) {
        size += 224 + frame->codes[i].payload_len * 6;
    }
    char *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buf) {
        return NULL;
    }

    size_t off = 0;

#define APPEND(...)                                                 \
    do {                                                            \
        int n_ = snprintf(buf + off, size - off, __VA_ARGS__);      \
        if (n_ < 0 || (size_t)n_ >= size - off) {                   \
            goto fail;                                              \
        }                                                           \
        off += n_;                                                  \
    } while (0)

    APPEND("{\"width\":%d,\"height\":%d,\"codes\":[", frame->width, frame->height);
    for (int i = 0; i < frame->num_codes; i++) {
        const qr_code_info_t *c = &frame->codes[i];
        APPEND("%s{\"corners\":[[%d,%d],[%d,%d],[%d,%d],[%d,%d]],\"decoded\":%s",
               i ? "," : "",
               c->corners[0].x, c->corners[0].y, c->corners[1].x, c->corners[1].y,
               c->corners[2].x, c->corners[2].y, c->corners[3].x, c->corners[3].y,
               c->decoded ? "true" : "false");
        if (c->decoded) {
            APPEND(",\"payload\":");
            if (!json_append_string(buf, size, &off, c->payload, c->payload_len)) {
                goto fail;
            }
        } else {
            APPEND(",\"error\":");
            if (!json_append_string(buf, size, &off, c->error, strlen(c->error))) {
                goto fail;
            }
        }
        APPEND("}");
    }
    APPEND("]}");

#undef APPEND

    *out_len = (int)off;
    return buf;

fail:
    heap_caps_free(buf);
    return NULL;
}

static esp_err_t qr_capture_get_handler(httpd_req_t *req)
{
    qr_scan_frame_t frame;
    esp_err_t err = qr_scan_capture(&frame);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "QR scan failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "QR scan failed");
    }

    int len = 0;
    char *json = format_codes_json(&frame, &len);
    if (!json) {
        qr_scan_frame_free(&frame);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }

    ESP_LOGI(TAG, "captured %d bytes, %d QR code(s)", (int)frame.jpeg_len, frame.num_codes);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    qr_scan_frame_free(&s_latest_qr);
    s_latest_qr = frame;
    xSemaphoreGive(s_mutex);

    esp_err_t send_err = send_json(req, json, len);
    heap_caps_free(json);
    return send_err;
}

static esp_err_t qr_image_get_handler(httpd_req_t *req)
{
    return serve_jpeg_slot(req, &s_latest_qr.jpeg, &s_latest_qr.jpeg_len);
}

static esp_err_t qr_codes_get_handler(httpd_req_t *req)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool have_frame = s_latest_qr.jpeg != NULL;
    int len = 0;
    char *json = NULL;
    if (have_frame) {
        json = format_codes_json(&s_latest_qr, &len);
    }
    xSemaphoreGive(s_mutex);

    if (!have_frame) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "nothing scanned yet, GET /qr/capture first");
    }
    if (!json) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    esp_err_t err = send_json(req, json, len);
    heap_caps_free(json);
    return err;
}

static void log_server_url(const esp_ip4_addr_t *ip)
{
    ESP_LOGI(TAG, "web server ready: http://" IPSTR "/", IP2STR(ip));
}

static void got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    const ip_event_got_ip_t *event = event_data;
    log_server_url(&event->ip_info.ip);
}

/* Logs the URL right away if the interface already has an address;
 * got_ip_handler covers the case where the lease arrives later. */
static void log_server_url_if_up(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        netif = esp_netif_get_default_netif();
    }

    esp_netif_ip_info_t ip_info;
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        log_server_url(&ip_info.ip);
    } else {
        ESP_LOGI(TAG, "web server started on port %d, waiting for an IP address", WEB_SERVER_PORT);
    }
}

esp_err_t web_server_start(void)
{
    if (s_server) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    // Off the default 32768 in case another esp_http_server instance runs.
    config.ctrl_port = 32769;
    // Face detection / QR scanning run in the handler task.
    config.stack_size = 10240;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 12;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get_handler},
        {.uri = "/capture", .method = HTTP_GET, .handler = capture_get_handler},
        {.uri = "/image.jpg", .method = HTTP_GET, .handler = image_get_handler},
        {.uri = "/faces.json", .method = HTTP_GET, .handler = faces_get_handler},
        {.uri = "/qr/capture", .method = HTTP_GET, .handler = qr_capture_get_handler},
        {.uri = "/qr/image.jpg", .method = HTTP_GET, .handler = qr_image_get_handler},
        {.uri = "/qr/codes.json", .method = HTTP_GET, .handler = qr_codes_get_handler},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &uris[i]));
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IP event handler registration failed: %s", esp_err_to_name(err));
    }
    log_server_url_if_up();

    return ESP_OK;
}
