#include "cam_capture.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "frames.h"

static const char *TAG = "cam_capture";

// Geometry of the frames produced by tools/gen-frames.sh -- keep in sync.
#define CAM_CAPTURE_WIDTH  800
#define CAM_CAPTURE_HEIGHT 600

// Next frame to hand out, guarded by s_mutex.
static size_t s_next;

static SemaphoreHandle_t s_mutex;
static bool s_initialized;

esp_err_t cam_capture_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (frames_count() == 0) {
        ESP_LOGE(TAG, "no frames embedded in this build");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "serving %u embedded frames at %dx%d", (unsigned)frames_count(),
             CAM_CAPTURE_WIDTH, CAM_CAPTURE_HEIGHT);
    return ESP_OK;
}

esp_err_t cam_capture_retrieve_jpeg_frame(const uint8_t **jpeg, size_t *jpeg_len, int *width, int *height)
{
    if (!jpeg || !jpeg_len || !width || !height) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // The mutex only keeps the round-robin cursor consistent across tasks:
    // the frame it selects needs no protection at all.
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    struct frame_span frame = frames_get(s_next);
    s_next = (s_next + 1) % frames_count();
    xSemaphoreGive(s_mutex);

    if (!frame.data) {
        ESP_LOGE(TAG, "capture failed");
        return ESP_FAIL;
    }

    // No copy, and nothing to allocate: the frame sits in flash for the
    // lifetime of the app.
    *jpeg = frame.data;
    *jpeg_len = frame.size;
    *width = CAM_CAPTURE_WIDTH;
    *height = CAM_CAPTURE_HEIGHT;
    return ESP_OK;
}

void cam_capture_release_frame(const uint8_t *jpeg)
{
    // Nothing to hand back -- see the contract in cam_capture.h. A real
    // camera returns the frame buffer to its driver here.
    (void)jpeg;
}
