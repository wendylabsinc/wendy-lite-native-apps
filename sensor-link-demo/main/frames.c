#include "frames.h"

#include "esp_log.h"

/*
 * The build emits _binary_<basename>_start/_end for every file listed in
 * EMBED_FILES, with the basename mangled into a C identifier: the directory
 * is not part of the symbol, so frames/frame_0.jpg becomes
 * _binary_frame_0_jpg_start. Unlike EMBED_TXTFILES there is no appended NUL
 * byte, so the payload length is exactly end - start.
 */
extern const uint8_t frame_0_jpg_start[] asm("_binary_frame_0_jpg_start");
extern const uint8_t frame_0_jpg_end[]   asm("_binary_frame_0_jpg_end");
extern const uint8_t frame_1_jpg_start[] asm("_binary_frame_1_jpg_start");
extern const uint8_t frame_1_jpg_end[]   asm("_binary_frame_1_jpg_end");
extern const uint8_t frame_2_jpg_start[] asm("_binary_frame_2_jpg_start");
extern const uint8_t frame_2_jpg_end[]   asm("_binary_frame_2_jpg_end");
extern const uint8_t frame_3_jpg_start[] asm("_binary_frame_3_jpg_start");
extern const uint8_t frame_3_jpg_end[]   asm("_binary_frame_3_jpg_end");

static const char *TAG = "frames";

static const struct {
    const uint8_t *start;
    const uint8_t *end;
} s_frames[] = {
    { .start = frame_0_jpg_start, .end = frame_0_jpg_end },
    { .start = frame_1_jpg_start, .end = frame_1_jpg_end },
    { .start = frame_2_jpg_start, .end = frame_2_jpg_end },
    { .start = frame_3_jpg_start, .end = frame_3_jpg_end },
};

size_t frames_count(void)
{
    return sizeof(s_frames) / sizeof(s_frames[0]);
}

struct frame_span frames_get(size_t index)
{
    if (index >= frames_count()) {
        ESP_LOGE(TAG, "frame %u out of range, %u embedded",
                 (unsigned)index, (unsigned)frames_count());
        return (struct frame_span){ .data = NULL, .size = 0 };
    }

    return (struct frame_span){
        .data = s_frames[index].start,
        .size = (size_t)(s_frames[index].end - s_frames[index].start),
    };
}
