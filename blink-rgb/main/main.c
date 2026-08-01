#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include "wendy_core.h"

// Onboard WS2812 RGB LED (GPIO 8 on ESP32-C6 devkits)
#define RGB_LED_GPIO 8

void app_main(void)
{
    ESP_ERROR_CHECK(wendy_core_init());
    int count = 0;

    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    bool on = false;
    while (true) {
        on = !on;
        if (on) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 24, 0));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        } else {
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Cycle %d\n", count++);
    }
}
