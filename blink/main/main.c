#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "wendy_core.h"

// Onboard user LED
#if CONFIG_IDF_TARGET_ESP32P4
// DFRobot FireBeetle 2 ESP32-P4 (DFR1172): GPIO 3, active high
#define USER_LED_GPIO 3
#define USER_LED_ON 1
#else
// Seeed XIAO ESP32S3: GPIO 21, active low
#define USER_LED_GPIO 21
#define USER_LED_ON 0
#endif

void app_main(void)
{
    ESP_ERROR_CHECK(wendy_core_init());
    int count = 0;

    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << USER_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));

    bool on = false;
    while (true) {
        on = !on;
        ESP_ERROR_CHECK(gpio_set_level(USER_LED_GPIO, on ? USER_LED_ON : !USER_LED_ON));
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Cycle %d\n", count++);
    }
}
