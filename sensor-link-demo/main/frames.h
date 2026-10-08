#pragma once

/*
 * Access to the demo JPEG frames embedded in the firmware image.
 *
 * The frames are produced by tools/gen-frames.sh and linked into flash
 * .rodata by EMBED_FILES (see CMakeLists.txt), so the spans handed out here
 * point straight at the memory-mapped image: valid for the lifetime of the
 * app, never copied, never freed.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One embedded JPEG frame. The bytes are owned by the firmware image. */
struct frame_span {
    const uint8_t *data;
    size_t size;
};

/**
 * Number of frames embedded in this build.
 */
size_t frames_count(void);

/**
 * Look up one embedded frame.
 * @param index  Frame index, 0-based
 * @return       The frame's bytes and length, or an empty span
 *               ({NULL, 0}) if index is out of range
 */
struct frame_span frames_get(size_t index);

#ifdef __cplusplus
}
#endif
