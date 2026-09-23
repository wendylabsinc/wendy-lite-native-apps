#pragma once

/* Stands in for a camera: serves the JPEG frames embedded in the firmware
 * image (see frames.h and tools/gen-frames.sh) instead of driving a sensor.
 * Successive captures walk the embedded frames round-robin, so a stream
 * animates rather than repeating one still image. */

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the fake camera. Idempotent: returns ESP_OK if it is already
 * up, so every consumer module can call it. */
esp_err_t cam_capture_init(void);

/* Grabs the next frame. The JPEG is NOT copied: *jpeg points straight into the
 * firmware image and stays valid until cam_capture_release_frame(). Every
 * successful call must be paired with exactly one release. width/height are
 * the frame dimensions in pixels. Safe to call from multiple tasks. */
esp_err_t cam_capture_retrieve_jpeg_frame(const uint8_t **jpeg, size_t *jpeg_len, int *width, int *height);

/* Releases a frame from cam_capture_retrieve_jpeg_frame(). These frames are
 * embedded in the firmware image, so there is nothing to give back and this
 * does nothing -- call it anyway. The acquire/release pairing is the contract,
 * not an implementation detail: honour it and the same caller works unchanged
 * against a real camera, whose frames must go back to the driver. */
void cam_capture_release_frame(const uint8_t *jpeg);

#ifdef __cplusplus
}
#endif
