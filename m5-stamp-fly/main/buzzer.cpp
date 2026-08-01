/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "Arduino.h"
#include <driver/ledc.h>
#include "buzzer.h"

const int buzzerPin = 40;
const int channel   = 5;

void setup_pwm_buzzer(void) {
    ledcAttachChannel(buzzerPin, 4000, 8, channel);  // 配置PWM通道并绑定到GPIO：频率4000Hz，分辨率8位
}

void buzzer_sound(uint32_t frequency, uint32_t duration_ms) {
    ledcWriteTone(buzzerPin, frequency);
    ledcWrite(buzzerPin, 127);

    vTaskDelay(duration_ms / portTICK_PERIOD_MS);

    ledcWriteTone(buzzerPin, 0);
}

void beep(void) {
    buzzer_sound(4000, 100);
}

void start_tone(void) {
    buzzer_sound(NOTE_D1, 200);
    buzzer_sound(NOTE_D5, 200);
    buzzer_sound(NOTE_D3, 200);
    buzzer_sound(NOTE_D4, 200);
}