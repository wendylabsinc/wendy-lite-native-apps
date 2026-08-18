#pragma once

/* Development/test HTTP server exposing the face_cam module:
 *   GET /            minimal test page (capture button + overlay)
 *   GET /capture     captures a frame, runs face detection, returns the
 *                    detection results as JSON
 *   GET /image.jpg   JPEG of the last captured frame
 *   GET /faces.json  detection results of the last captured frame
 *
 * The server URL is logged once the network interface has an IP address. */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the HTTP server on port 80. face_cam_init() must have succeeded. */
esp_err_t web_server_start(void);

#ifdef __cplusplus
}
#endif
