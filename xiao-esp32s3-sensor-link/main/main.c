#include <stdio.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wendy_core.h"
#include "wendy_com.h"

#include "cam_loop.h"
#include "cam_capture.h"
#include "mic.h"
#include "server.h"

// Onboard user LED (GPIO 21 on the XIAO ESP32S3), active low: LOW = on
#define USER_LED_GPIO 21
static atomic_bool camera_ready;
static atomic_bool microphone_ready;

static enum wcom_sensor_link_result sensor_link_get_manifest(struct wcom_sensor_descriptor *sensors,
                                                               size_t max_sensors,
                                                               size_t *sensor_count)
{
    *sensor_count = 0;
    if (camera_ready && *sensor_count < max_sensors) {
        sensors[(*sensor_count)++] = (struct wcom_sensor_descriptor){
            .channel_id = 0,
            .input_id = 0,
            .name = "camera",
            .format_kind = WCOM_SENSOR_FORMAT_VIDEO,
            .format.video = {
                .codec = wendy_lite_sensorlink_VideoFormat_Codec_MJPEG,
                .width = 800,
                .height = 600,
                .fps = 0,
            },
        };
    }
    if (microphone_ready && *sensor_count < max_sensors) {
        sensors[(*sensor_count)++] = (struct wcom_sensor_descriptor){
            .channel_id = 1,
            // The pinned SDK can stream only one input at a time.
            .input_id = 0,
            .name = "microphone",
            .format_kind = WCOM_SENSOR_FORMAT_AUDIO,
            .format.audio = {
                .codec = wendy_lite_sensorlink_AudioFormat_Codec_PCM_S16LE,
                .sample_rate = MIC_SAMPLE_RATE_HZ,
                .channels = 1,
            },
        };
    }
    return WCOM_SENSOR_LINK_OK;
}

static enum wcom_sensor_link_result sensor_link_subscribe(int client_id, const uint32_t *channel_ids, size_t count)
{
    if (count == 0) {
        return WCOM_SENSOR_LINK_OK;
    }
    if (count != 1 || !((channel_ids[0] == 0 && camera_ready) ||
                        (channel_ids[0] == 1 && microphone_ready))) {
        return WCOM_SENSOR_LINK_FAIL;
    }
    return cam_loop_start(client_id, channel_ids[0]) ? WCOM_SENSOR_LINK_OK : WCOM_SENSOR_LINK_FAIL;
}

static enum wcom_sensor_link_result sensor_link_unsubscribe(int client_id, const uint32_t *channel_ids, size_t count)
{
    if (count == 0) {
        return WCOM_SENSOR_LINK_OK;
    }
    if (count != 1 || channel_ids[0] > 1) {
        return WCOM_SENSOR_LINK_FAIL;
    }
    cam_loop_stop(client_id, channel_ids[0]);
    return WCOM_SENSOR_LINK_OK;
}

static void sensor_link_disconnected(int client_id)
{
    cam_loop_stop(client_id, 0);
    cam_loop_stop(client_id, 1);
}

static const struct wcom_sensor_link_delegate sensor_link_delegate = {
    .on_sensor_link_get_manifest = sensor_link_get_manifest,
    .on_sensor_link_subscribe = sensor_link_subscribe,
    .on_sensor_link_unsubscribe = sensor_link_unsubscribe,
    .on_sensor_link_disconnected = sensor_link_disconnected,
};

void app_main(void)
{
    // Reserve contiguous internal DMA memory before WiFi and TLS tasks fragment it.
    esp_err_t camera_err = cam_capture_init();
    camera_ready = camera_err == ESP_OK;
    if (!camera_ready) {
        ESP_LOGE("main", "Camera unavailable (%s); device management remains active",
                 esp_err_to_name(camera_err));
    }

    esp_err_t mic_err = mic_init();
    microphone_ready = mic_err == ESP_OK;
    if (!microphone_ready) {
        ESP_LOGE("main", "Microphone unavailable (%s)", esp_err_to_name(mic_err));
    }
    ESP_ERROR_CHECK(cam_loop_init());

    wendy_core_register_sensor_link_source(&sensor_link_delegate);
    ESP_ERROR_CHECK(wendy_core_init());

    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << USER_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));

    // ESP_ERROR_CHECK(server_start());

    bool on = false;
    while (true) {
        on = !on;
        ESP_ERROR_CHECK(gpio_set_level(USER_LED_GPIO, on ? 0 : 1));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
