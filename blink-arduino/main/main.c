#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"

#include "wendy_core.h"

// Onboard user LED (GPIO 21 on the XIAO ESP32S3), active low: LOW = on
#define USER_LED_GPIO 21

/*
 * UART0 belongs to wendy_stdio (console) and USB-Serial/JTAG to wendy_usj:
 * never call Serial.begin() / USBSerial.begin() from Arduino code.
 */

/*
 * Strong override of arduino-esp32's weak btInUse(): this app uses NimBLE
 * directly, not an Arduino BT library, so without this initArduino() would
 * btMemRelease(BT_MODE_BLE) and break any later NimBLE init.
 */
bool btInUse(void)
{
    return true;
}

void setup(void)
{
    pinMode(USER_LED_GPIO, OUTPUT);
    digitalWrite(USER_LED_GPIO, HIGH); // off (active low)
}

void loop(void)
{
    static bool on = false;
    on = !on;
    digitalWrite(USER_LED_GPIO, on ? LOW : HIGH);
    delay(500);
}

static void loop_task(void *arg)
{
    setup();
    for (;;)
        loop();
}

void app_main(void)
{
    // initArduino() resets log levels and runs nvs_flash_init() (idempotent),
    // so run it before wendy comes up.
    initArduino();

    ESP_ERROR_CHECK(wendy_core_init());

    // Same parameters the autostart core uses for its loopTask:
    // prio 1, 8 KB stack, pinned to ARDUINO_RUNNING_CORE (core 1).
    xTaskCreateUniversal(loop_task, "loopTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);

    // Returning lets the main task self-delete, freeing its stack.
}
