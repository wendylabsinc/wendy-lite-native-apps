#include "qr_scan.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "img_converters.h"
#include "quirc.h"

#include "cam_capture.h"

static const char *TAG = "qr_scan";

static SemaphoreHandle_t s_mutex;
static struct quirc *s_quirc;
static int s_quirc_w, s_quirc_h;

esp_err_t qr_scan_init(void)
{
    if (s_quirc) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = cam_capture_init();
    if (err != ESP_OK) {
        return err;
    }

    // Sized lazily on the first capture (quirc_resize allocates w*h bytes,
    // which lands in PSRAM through CONFIG_SPIRAM_USE_MALLOC).
    s_quirc = quirc_new();
    if (!s_quirc) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "QR scanning ready");
    return ESP_OK;
}

/* Fills the quirc buffer with the luma of the decoded JPEG. */
static esp_err_t decode_to_gray(const uint8_t *jpeg, size_t jpeg_len, int width, int height)
{
    uint8_t *rgb = heap_caps_malloc((size_t)width * height * 3, MALLOC_CAP_SPIRAM);
    if (!rgb) {
        return ESP_ERR_NO_MEM;
    }
    if (!fmt2rgb888(jpeg, jpeg_len, PIXFORMAT_JPEG, rgb)) {
        ESP_LOGE(TAG, "jpeg decode failed");
        heap_caps_free(rgb);
        return ESP_FAIL;
    }

    int qw, qh;
    uint8_t *gray = quirc_begin(s_quirc, &qw, &qh);
    const uint8_t *p = rgb;
    for (int i = 0; i < qw * qh; i++, p += 3) {
        gray[i] = (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
    }
    heap_caps_free(rgb);
    return ESP_OK;
}

static esp_err_t scan_frame(qr_scan_frame_t *out)
{
    uint8_t *jpeg = NULL;
    size_t jpeg_len = 0;
    int width = 0, height = 0;
    esp_err_t err = cam_capture_jpeg(&jpeg, &jpeg_len, &width, &height);
    if (err != ESP_OK) {
        return err;
    }

    // Too big for the calling task's stack (quirc_data alone is ~9 KB).
    struct quirc_code *code = malloc(sizeof(*code));
    struct quirc_data *data = malloc(sizeof(*data));
    if (!code || !data) {
        err = ESP_ERR_NO_MEM;
        goto done;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_quirc_w != width || s_quirc_h != height) {
        if (quirc_resize(s_quirc, width, height) < 0) {
            err = ESP_ERR_NO_MEM;
            goto done_locked;
        }
        s_quirc_w = width;
        s_quirc_h = height;
    }

    err = decode_to_gray(jpeg, jpeg_len, width, height);
    if (err != ESP_OK) {
        goto done_locked;
    }
    quirc_end(s_quirc);

    int count = quirc_count(s_quirc);
    if (count > QR_SCAN_MAX_CODES) {
        ESP_LOGW(TAG, "dropping codes beyond %d (found %d)", QR_SCAN_MAX_CODES, count);
        count = QR_SCAN_MAX_CODES;
    }

    for (int i = 0; i < count; i++) {
        qr_code_info_t *info = &out->codes[out->num_codes];
        quirc_extract(s_quirc, i, code);
        for (int c = 0; c < 4; c++) {
            info->corners[c].x = code->corners[c].x;
            info->corners[c].y = code->corners[c].y;
        }

        quirc_decode_error_t derr = quirc_decode(code, data);
        if (derr == QUIRC_SUCCESS) {
            info->payload = heap_caps_malloc(data->payload_len + 1, MALLOC_CAP_SPIRAM);
            if (!info->payload) {
                err = ESP_ERR_NO_MEM;
                goto done_locked;
            }
            memcpy(info->payload, data->payload, data->payload_len);
            info->payload[data->payload_len] = '\0';
            info->payload_len = data->payload_len;
            info->decoded = true;
        } else {
            info->decoded = false;
            info->error = quirc_strerror(derr);
        }
        out->num_codes++;
    }

    out->jpeg = jpeg;
    out->jpeg_len = jpeg_len;
    out->width = width;
    out->height = height;
    jpeg = NULL;
    err = ESP_OK;

done_locked:
    xSemaphoreGive(s_mutex);
done:
    free(code);
    free(data);
    if (err != ESP_OK) {
        heap_caps_free(jpeg);
        qr_scan_frame_free(out);
    }
    return err;
}

typedef struct {
    qr_scan_frame_t *out;
    esp_err_t err;
    SemaphoreHandle_t done;
} scan_ctx_t;

static void scan_worker(void *arg)
{
    scan_ctx_t *ctx = arg;
    ctx->err = scan_frame(ctx->out);
    xSemaphoreGive(ctx->done);
    // Parked here (holding nothing) until qr_scan_capture() deletes us.
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

/* quirc_decode() puts a ~9 KB struct datastream on the stack, more than
 * typical caller tasks (e.g. esp_http_server handlers) have. Run the scan on
 * a dedicated task whose stack lives in PSRAM so callers don't need to know. */
#define QR_SCAN_TASK_STACK (24 * 1024)

esp_err_t qr_scan_capture(qr_scan_frame_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!s_quirc) {
        return ESP_ERR_INVALID_STATE;
    }

    scan_ctx_t ctx = {
        .out = out,
        .err = ESP_FAIL,
        .done = xSemaphoreCreateBinary(),
    };
    if (!ctx.done) {
        return ESP_ERR_NO_MEM;
    }

    TaskHandle_t task = NULL;
    if (xTaskCreateWithCaps(scan_worker, "qr_scan", QR_SCAN_TASK_STACK, &ctx,
                            uxTaskPriorityGet(NULL), &task,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        vSemaphoreDelete(ctx.done);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(ctx.done, portMAX_DELAY);
    vTaskDeleteWithCaps(task);
    vSemaphoreDelete(ctx.done);
    return ctx.err;
}

void qr_scan_frame_free(qr_scan_frame_t *out)
{
    if (!out) {
        return;
    }
    for (int i = 0; i < out->num_codes; i++) {
        heap_caps_free(out->codes[i].payload);
    }
    heap_caps_free(out->jpeg);
    memset(out, 0, sizeof(*out));
}
