#include "camera.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

// OV2640 on the XIAO ESP32S3 Sense expansion board (DVP interface)
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
};

static TaskHandle_t s_task;
static volatile bool s_running;
static SemaphoreHandle_t s_done;

// Called once per captured frame. Extension point for real processing.
static void frame_handler(const uint8_t *buf, size_t len)
{
    (void)buf;
    ESP_LOGI(TAG, "frame: %u bytes", (unsigned)len);
}

static void capture_task(void *arg)
{
    (void)arg;

    while (s_running) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        frame_handler(fb->buf, fb->len);
        esp_camera_fb_return(fb);
    }

    s_task = NULL;
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

esp_err_t camera_start(void)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_done) {
        s_done = xSemaphoreCreateBinary();
        if (!s_done) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_camera_init(&s_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_running = true;
    if (xTaskCreate(capture_task, "capture", 4096, NULL, 5, &s_task) != pdPASS) {
        s_running = false;
        esp_camera_deinit();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void camera_stop(void)
{
    if (!s_task) {
        return;
    }

    s_running = false;
    // Wait for the task to leave the loop before tearing the driver down.
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "capture task did not stop");
        return;
    }

    esp_camera_deinit();
}
