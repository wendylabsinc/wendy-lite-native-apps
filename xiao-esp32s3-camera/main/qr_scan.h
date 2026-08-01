#pragma once

/* Camera capture + QR-code detection/decoding (quirc).
 *
 * Reusable module with no dependency on the web server: qr_scan_capture()
 * grabs one JPEG frame from the camera, locates every QR code in it and
 * decodes their payloads. Codes that are located but fail to decode (e.g.
 * ECC failure) are still reported with their corner positions. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
} qr_point_t;

#define QR_SCAN_MAX_CODES 8

typedef struct {
    /* The four corners of the code, from top-left, clockwise. Always valid. */
    qr_point_t corners[4];
    bool decoded;
    /* NUL-terminated payload, NULL when !decoded; freed by qr_scan_frame_free(). */
    char *payload;
    size_t payload_len;
    /* Static decode error string when !decoded, NULL otherwise. */
    const char *error;
} qr_code_info_t;

typedef struct {
    /* JPEG as produced by the sensor; release with qr_scan_frame_free(). */
    uint8_t *jpeg;
    size_t jpeg_len;
    /* Frame dimensions; corner coordinates are in this pixel space. */
    int width;
    int height;
    int num_codes;
    qr_code_info_t codes[QR_SCAN_MAX_CODES];
} qr_scan_frame_t;

/* Initializes the camera driver (shared with face_cam) and quirc. */
esp_err_t qr_scan_init(void);

/* Captures one frame and scans it for QR codes. Blocking; safe to call from
 * multiple tasks (serialized internally). On success the caller owns *out
 * and must release it with qr_scan_frame_free(). */
esp_err_t qr_scan_capture(qr_scan_frame_t *out);

/* Frees the buffers held by *out (safe on a zeroed struct). */
void qr_scan_frame_free(qr_scan_frame_t *out);

#ifdef __cplusplus
}
#endif
