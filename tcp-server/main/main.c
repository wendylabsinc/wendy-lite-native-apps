#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rgb_led.h"

#include "wendy_core.h"

#include "payload.h"
#include "tcp_server.h"
#include "tls_server.h"

static const char *TAG = "main";

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

void app_main(void)
{
    // Brings up WiFi and, with it, the conf partition the TLS server reads its
    // certificate from, so nothing below may run before this returns.
    ESP_ERROR_CHECK(wendy_core_init());

    ESP_ERROR_CHECK(rgb_led_init(RGB_LED_GPIO, 1));
    ESP_ERROR_CHECK(payload_init());
    ESP_ERROR_CHECK(tcp_server_start());
    ESP_ERROR_CHECK(tls_server_start());

    bool on = false;
    while (true) {
        on = !on;
        // Log and carry on rather than ESP_ERROR_CHECK: an RMT hiccup under
        // network load must not abort the firmware mid-measurement.
        esp_err_t err = on ? rgb_led_set(0, 0, 0, 24) : rgb_led_clear();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "rgb_led: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
