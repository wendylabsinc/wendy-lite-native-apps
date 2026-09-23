#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "wendy_core.h"
#include "wendy_com.h"

#include "cam_loop.h"
#include "camera.h"
#include "mic.h"
#include "server.h"

// Onboard user LED (GPIO 21 on the XIAO ESP32S3), active low: LOW = on
#define USER_LED_GPIO 21

static enum wcom_sensor_link_result sensor_link_get_manifest(struct wcom_sensor_descriptor *sensors,
                                                               size_t max_sensors,
                                                               size_t *sensor_count)
{
    if (max_sensors < 1) {
        *sensor_count = 0;
        return WCOM_SENSOR_LINK_OK;
    }

    sensors[0] = (struct wcom_sensor_descriptor){
        .channel_id = 0,
        .kind = wendy_lite_sensorlink_SensorDescriptor_Kind_CAMERA,
        .name = "camera",
        .format_kind = WCOM_SENSOR_FORMAT_VIDEO,
        .format.video = {
            .codec = wendy_lite_sensorlink_VideoFormat_Codec_MJPEG,
            .width = 800,
            .height = 600,
            .fps = 0,
        },
    };
    *sensor_count = 1;
    return WCOM_SENSOR_LINK_OK;
}

static enum wcom_sensor_link_result sensor_link_subscribe(int client_id, const uint32_t *channel_ids, size_t count)
{
    if (count == 0) {
        return WCOM_SENSOR_LINK_OK;
    }
    if (count != 1 || channel_ids[0] != 0) {
        return WCOM_SENSOR_LINK_FAIL;
    }
    cam_loop_start(client_id, channel_ids[0]);
    return WCOM_SENSOR_LINK_OK;
}

static enum wcom_sensor_link_result sensor_link_unsubscribe(int client_id, const uint32_t *channel_ids, size_t count)
{
    if (count == 0) {
        return WCOM_SENSOR_LINK_OK;
    }
    if (count != 1 || channel_ids[0] != 0) {
        return WCOM_SENSOR_LINK_FAIL;
    }
    cam_loop_stop(client_id, channel_ids[0]);
    return WCOM_SENSOR_LINK_OK;
}

static void sensor_link_disconnected(int client_id)
{
    cam_loop_stop(client_id, 0);
}

static const struct wcom_sensor_link_delegate sensor_link_delegate = {
    .on_sensor_link_get_manifest = sensor_link_get_manifest,
    .on_sensor_link_subscribe = sensor_link_subscribe,
    .on_sensor_link_unsubscribe = sensor_link_unsubscribe,
    .on_sensor_link_disconnected = sensor_link_disconnected,
};

void app_main(void)
{
    wendy_core_register_sensor_link_source(&sensor_link_delegate);
    ESP_ERROR_CHECK(wendy_core_init());
    ESP_ERROR_CHECK(cam_loop_init());

    int count = 0;

    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << USER_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));

    // ESP_ERROR_CHECK(server_start());
    // ESP_ERROR_CHECK(mic_start());

    bool on = false;
    while (true) {
        on = !on;
        ESP_ERROR_CHECK(gpio_set_level(USER_LED_GPIO, on ? 0 : 1));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
