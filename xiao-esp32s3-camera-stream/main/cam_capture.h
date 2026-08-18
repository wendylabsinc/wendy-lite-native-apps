#pragma once

/* Shared ownership of the camera driver for the vision modules (face_cam,
 * qr_scan). Owns esp_camera exclusively — mutually exclusive with camera.c's
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

/* Grabs the newest frame and returns a PSRAM copy of its JPEG. The caller
 * owns *jpeg and releases it with free()/heap_caps_free(). width/height are
 * the frame dimensions in pixels. Safe to call from multiple tasks. */
esp_err_t cam_capture_jpeg(uint8_t **jpeg, size_t *jpeg_len, int *width, int *height);

#ifdef __cplusplus
}
#endif
