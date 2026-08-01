#include "cam_capture.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "cam_capture";

// OV2640 on the XIAO ESP32S3 Sense expansion board (DVP interface).
// Same wiring as camera.c, duplicated on purpose: that module is kept
// as-is and only one of the two may own the esp_camera driver.
#define CAM_PIN_PWDN  -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK  10
#define CAM_PIN_SIOD  40
#define CAM_PIN_SIOC  39
#define CAM_PIN_D7    48
#define CAM_PIN_D6    11
#define CAM_PIN_D5    12
#define CAM_PIN_D4    14
#define CAM_PIN_D3    16
#define CAM_PIN_D2    18
#define CAM_PIN_D1    17
#define CAM_PIN_D0    15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF  47
#define CAM_PIN_PCLK  13

static const camera_config_t s_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
    .sccb_i2c_port = -1,
};

static SemaphoreHandle_t s_mutex;
static bool s_initialized;

esp_err_t cam_capture_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_camera_init(&s_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // The OV2640 on this board delivers a horizontally mirrored image, which
    // QR decoding cannot tolerate (and face coordinates would be flipped).
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor && sensor->set_hmirror) {
        sensor->set_hmirror(sensor, 1);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "camera ready");
    return ESP_OK;
}

esp_err_t cam_capture_jpeg(uint8_t **jpeg, size_t *jpeg_len, int *width, int *height)
{
    if (!jpeg || !jpeg_len || !width || !height) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    esp_err_t err = ESP_FAIL;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "capture failed");
        goto done;
    }

    // Copy the JPEG so the frame buffer can go back to the driver.
    uint8_t *buf = heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM);
    if (!buf) {
        esp_camera_fb_return(fb);
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    memcpy(buf, fb->buf, fb->len);
    *jpeg = buf;
    *jpeg_len = fb->len;
    *width = fb->width;
    *height = fb->height;
    esp_camera_fb_return(fb);
    err = ESP_OK;

done:
    xSemaphoreGive(s_mutex);
    return err;
}
