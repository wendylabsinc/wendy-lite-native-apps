#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "frames.h"
#include "rgb_led.h"

#include "wendy_core.h"
#include "wendy_com.h"

#include "cam_loop.h"

// Onboard WS2812 RGB LED
#if CONFIG_IDF_TARGET_ESP32C5
// ESP32-C5-DevKitC-1
#define RGB_LED_GPIO 27
#else
// ESP32-C6-DevKitC-1
// ESP32-C6-DevKitM-1
// ESP32-C61-DevKitC-1
#define RGB_LED_GPIO 8
#endif

static const char *TAG = "main";

static enum wcom_sensor_link_result sensor_link_get_manifest(struct wcom_sensor_descriptor *sensors,
                                                               size_t max_sensors,
                                                               size_t *sensor_count)
{
    if (max_sensors < 1) {
        *sensor_count = 0;
        return WCOM_SENSOR_LINK_OK;
    }

    // The frames embedded by tools/gen-frames.sh, advertised as a camera.
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

// Walks the frames embedded by EMBED_FILES. Every JPEG must open with the
// SOI marker 0xFF 0xD8, so this also proves the linker symbols resolved to
// the right offsets and that flash .rodata is readable as-is.
static void log_frames(void)
{
    for (size_t i = 0; i < frames_count(); i++) {
        struct frame_span f = frames_get(i);
        if (f.data == NULL || f.size < 2) {
            ESP_LOGE(TAG, "frame %u is missing", (unsigned)i);
            continue;
        }
        ESP_LOGI(TAG, "frame %u: %u bytes, starts %02x %02x%s",
                 (unsigned)i, (unsigned)f.size, f.data[0], f.data[1],
                 (f.data[0] == 0xFF && f.data[1] == 0xD8) ? "" : " -- not a JPEG!");
    }
}

void app_main(void)
{
    wendy_core_register_sensor_link_source(&sensor_link_delegate);
    ESP_ERROR_CHECK(wendy_core_init());
    ESP_ERROR_CHECK(cam_loop_init());

    log_frames();

    ESP_ERROR_CHECK(rgb_led_init(RGB_LED_GPIO, 1));

    bool on = false;
    while (true) {
        on = !on;
        if (on) {
            ESP_ERROR_CHECK(rgb_led_set(0, 24, 24, 0));
        } else {
            ESP_ERROR_CHECK(rgb_led_clear());
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
