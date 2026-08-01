#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "wendy_core.h"

#include "camera.h"
#include "face_cam.h"
#include "qr_scan.h"
#include "rom_print.h"
#include "web_server.h"

// Onboard user LED (GPIO 21 on the XIAO ESP32S3), active low: LOW = on
#define USER_LED_GPIO 21

void app_main(void)
{
    // Before wendy_core_init(), which hands USB Serial/JTAG to wendy_usj.
    rom_print_usb_disable();

    ESP_ERROR_CHECK(wendy_core_init());

    rom_print_report();

    int count = 0;

    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << USER_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));

//    ESP_ERROR_CHECK(camera_start());

    ESP_ERROR_CHECK(face_cam_init());
    ESP_ERROR_CHECK(qr_scan_init());
    ESP_ERROR_CHECK(web_server_start());

    bool on = false;
    while (true) {
        on = !on;
        ESP_ERROR_CHECK(gpio_set_level(USER_LED_GPIO, on ? 0 : 1));
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Cycle %d\n", count++);
    }
}
