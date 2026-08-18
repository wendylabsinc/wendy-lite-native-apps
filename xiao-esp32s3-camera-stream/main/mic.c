#include "mic.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/i2s_pdm.h"
#include "esp_log.h"

static const char *TAG = "mic";

// Onboard PDM MEMS microphone on the XIAO ESP32S3 Sense expansion board.
// PDM RX is only supported on I2S_NUM_0 on the S3.
#define MIC_PIN_CLK GPIO_NUM_42
#define MIC_PIN_DIN GPIO_NUM_41

#define MIC_SAMPLE_RATE_HZ 16000
#define MIC_READ_SAMPLES 1024

static i2s_chan_handle_t s_rx_handle;
static TaskHandle_t s_task;
static volatile bool s_running;
static SemaphoreHandle_t s_done;

// Called once per chunk of captured audio. Extension point for real processing.
static void audio_handler(const int16_t *buf, size_t samples)
{
    (void)buf;
    ESP_LOGI(TAG, "mic: %u samples", (unsigned)samples);
}

static void mic_task(void *arg)
{
    (void)arg;

    static int16_t buf[MIC_READ_SAMPLES];

    while (s_running) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx_handle, buf, sizeof(buf), &bytes_read, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
            continue;
        }
        audio_handler(buf, bytes_read / sizeof(int16_t));
    }

    s_task = NULL;
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

esp_err_t mic_start(void)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_done) {
        s_done = xSemaphoreCreateBinary();
        if (!s_done) {
            return ESP_ERR_NO_MEM;
        }
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_PIN_CLK,
            .din = MIC_PIN_DIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    err = i2s_channel_init_pdm_rx_mode(s_rx_handle, &pdm_rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_pdm_rx_mode failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return err;
    }

    err = i2s_channel_enable(s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return err;
    }

    s_running = true;
    if (xTaskCreate(mic_task, "mic", 4096, NULL, 5, &s_task) != pdPASS) {
        s_running = false;
        i2s_channel_disable(s_rx_handle);
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void mic_stop(void)
{
    if (!s_task) {
        return;
    }

    s_running = false;
    // Wait for the task to leave the loop before tearing the driver down.
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "mic task did not stop");
        return;
    }

    i2s_channel_disable(s_rx_handle);
    i2s_del_channel(s_rx_handle);
    s_rx_handle = NULL;
}
