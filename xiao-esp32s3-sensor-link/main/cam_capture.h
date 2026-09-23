#pragma once

/* Owns the camera driver on behalf of server.c's JPEG streaming. Owns
 * esp_camera exclusively — mutually exclusive with camera.c's
 * camera_start(). */

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the camera driver (JPEG, SVGA). Idempotent: returns ESP_OK if
 * the driver is already up, so every consumer module can call it. */
esp_err_t cam_capture_init(void);

/* Grabs the newest frame. The JPEG is NOT copied: *jpeg borrows the driver's
 * frame buffer and stays valid until cam_capture_release_frame(). Every
 * successful call must be paired with exactly one release, and frames should
 * be held briefly -- the driver only owns fb_count of them. width/height are
 * the frame dimensions in pixels. Safe to call from multiple tasks. */
esp_err_t cam_capture_retrieve_jpeg_frame(const uint8_t **jpeg, size_t *jpeg_len, int *width, int *height);

/* Hands a frame from cam_capture_retrieve_jpeg_frame() back to the camera
 * driver. The acquire/release pairing is the contract, not an implementation
 * detail: it is what lets a caller work unchanged against a source whose
 * frames must be given back (this one) and one whose frames live in flash and
 * need no releasing at all (senser-link-demo). */
void cam_capture_release_frame(const uint8_t *jpeg);

#ifdef __cplusplus
}
#endif
