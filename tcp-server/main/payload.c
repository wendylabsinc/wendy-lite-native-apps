#include "payload.h"

#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "payload";

static uint8_t *_chunk = NULL;

esp_err_t payload_init(void)
{
    if (_chunk) {
        return ESP_OK;
    }

    _chunk = malloc(PAYLOAD_CHUNK_BYTES);
    if (!_chunk) {
        ESP_LOGE(TAG, "cannot allocate the %u byte chunk, %u bytes of heap left",
                 (unsigned)PAYLOAD_CHUNK_BYTES, (unsigned)esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }

    /* PAYLOAD_CHUNK_BYTES is a whole number of patterns, so this lands exactly
     * on the end of the buffer. */
    for (size_t off = 0; off < PAYLOAD_CHUNK_BYTES; off += PAYLOAD_PATTERN_LEN) {
        memcpy(_chunk + off, PAYLOAD_PATTERN, PAYLOAD_PATTERN_LEN);
    }

    ESP_LOGI(TAG, "%u byte chunk of \"%s\", %u bytes per transfer, %u bytes of heap left",
             (unsigned)PAYLOAD_CHUNK_BYTES, PAYLOAD_PATTERN,
             (unsigned)PAYLOAD_TOTAL_BYTES, (unsigned)esp_get_free_heap_size());

    return ESP_OK;
}

const uint8_t *payload_chunk(void)
{
    return _chunk;
}
