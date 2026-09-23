#include "cam_loop.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "cam_capture.h"
#include "wendy_core.h"

static const char *TAG = "cam_loop";

#define FRAME_DONE_BIT (1 << 0)
#define STOP_BIT (1 << 1)
#define START_BIT (1 << 2)

static TaskHandle_t s_task;
static EventGroupHandle_t s_events;

// Guards the pending/active target fields below.
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool s_pending_valid;
static int s_pending_client_id;
static uint32_t s_pending_channel_id;

static bool s_active_valid;
static int s_active_client_id;
static uint32_t s_active_channel_id;

static void frame_done_cb(uint32_t channel_id)
{
    xEventGroupSetBits(s_events, FRAME_DONE_BIT);
}

static void cam_loop_task(void *arg)
{
    for (;;) {
        xEventGroupWaitBits(s_events, START_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

        taskENTER_CRITICAL(&s_lock);
        bool valid = s_pending_valid;
        int client_id = s_pending_client_id;
        uint32_t channel_id = s_pending_channel_id;
        if (valid) {
            s_pending_valid = false;
            s_active_valid = true;
            s_active_client_id = client_id;
            s_active_channel_id = channel_id;
        }
        taskEXIT_CRITICAL(&s_lock);

        if (!valid) {
            // Latched start was cancelled by a race with cam_loop_stop()
            // before we got here.
            continue;
        }

        ESP_LOGI(TAG, "starting stream for client %d channel %" PRIu32, client_id, channel_id);
        xEventGroupClearBits(s_events, STOP_BIT);
        wendy_core_sensor_stream_begin(client_id, channel_id);

        const uint8_t *pending_jpeg = NULL;

        for (;;) {
            if (xEventGroupGetBits(s_events) & STOP_BIT) {
                break;
            }

            const uint8_t *jpeg;
            size_t jpeg_len;
            int width, height;
            esp_err_t err = cam_capture_retrieve_jpeg_frame(&jpeg, &jpeg_len, &width, &height);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "cam_capture_retrieve_jpeg_frame failed: %s", esp_err_to_name(err));
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }

            if (pending_jpeg) {
                // A previous frame is still in flight; wait for its done
                // callback before reusing the slot. Nothing to wait for on
                // the very first frame of a stream.
                EventBits_t bits = xEventGroupWaitBits(s_events, FRAME_DONE_BIT | STOP_BIT, pdFALSE, pdFALSE,
                                                        portMAX_DELAY);
                if (bits & STOP_BIT) {
                    cam_capture_release_frame(jpeg);
                    break;
                }

                cam_capture_release_frame(pending_jpeg);
            } else if (xEventGroupGetBits(s_events) & STOP_BIT) {
                // Nothing in flight to wait for, but a stop may have arrived
                // while we were capturing.
                cam_capture_release_frame(jpeg);
                break;
            }

            pending_jpeg = jpeg;
            xEventGroupClearBits(s_events, FRAME_DONE_BIT);
            wendy_core_send_jpeg_frame(client_id, channel_id, jpeg, jpeg_len, esp_timer_get_time(), frame_done_cb);
        }

        if (pending_jpeg) {
            // The last frame sent may still be in flight; wait for its done
            // callback before handing the buffer back.
            xEventGroupWaitBits(s_events, FRAME_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
            cam_capture_release_frame(pending_jpeg);
        }

        wendy_core_sensor_stream_end(client_id, channel_id);
        ESP_LOGI(TAG, "stopped stream for client %d channel %" PRIu32, client_id, channel_id);

        taskENTER_CRITICAL(&s_lock);
        s_active_valid = false;
        taskEXIT_CRITICAL(&s_lock);
    }
}

esp_err_t cam_loop_init(void)
{
    esp_err_t err = cam_capture_init();
    if (err != ESP_OK) {
        return err;
    }

    s_events = xEventGroupCreate();
    if (!s_events) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(cam_loop_task, "cam_loop", 4096, NULL, 5, &s_task) != pdPASS) {
        vEventGroupDelete(s_events);
        s_events = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void cam_loop_start(int client_id, uint32_t channel_id)
{
    taskENTER_CRITICAL(&s_lock);
    s_pending_valid = true;
    s_pending_client_id = client_id;
    s_pending_channel_id = channel_id;
    taskEXIT_CRITICAL(&s_lock);

    xEventGroupSetBits(s_events, START_BIT);
}

void cam_loop_stop(int client_id, uint32_t channel_id)
{
    bool stop_active = false;

    taskENTER_CRITICAL(&s_lock);
    if (s_pending_valid && s_pending_client_id == client_id && s_pending_channel_id == channel_id) {
        s_pending_valid = false;
    }
    if (s_active_valid && s_active_client_id == client_id && s_active_channel_id == channel_id) {
        stop_active = true;
    }
    taskEXIT_CRITICAL(&s_lock);

    if (stop_active) {
        xEventGroupSetBits(s_events, STOP_BIT);
    }
}
