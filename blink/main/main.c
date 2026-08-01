#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "wendy_core.h"

// Onboard user LED (GPIO 21 on the XIAO ESP32S3), active low: LOW = on
#define USER_LED_GPIO 21

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
        ESP_ERROR_CHECK(gpio_set_level(USER_LED_GPIO, on ? 0 : 1));
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Cycle %d\n", count++);
    }
}
