#pragma once

/* Camera capture + ESP-WHO (esp-dl) human face detection.
 *
 * Reusable module with no dependency on the web server: face_cam_capture()
 * grabs one JPEG frame from the camera, runs face detection on it and
 * returns both the JPEG and the detected faces. Owns the esp_camera driver
 * (mutually exclusive with camera.c's camera_start()). */

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
} face_point_t;

typedef struct {
    float score;
    /* Bounding box, top-left (x1,y1) to bottom-right (x2,y2). */
    int x1, y1, x2, y2;
    /* Landmarks, valid when has_keypoints is true (MSR+MNP model). */
    bool has_keypoints;
    face_point_t left_eye;
    face_point_t right_eye;
    face_point_t nose;
    face_point_t mouth_left;
    face_point_t mouth_right;
} face_info_t;

#define FACE_CAM_MAX_FACES 8

typedef struct {
    /* JPEG as produced by the sensor; release with face_cam_frame_free(). */
    uint8_t *jpeg;
    size_t jpeg_len;
    /* Frame dimensions; face coordinates are in this pixel space. */
    int width;
    int height;
    int num_faces;
    face_info_t faces[FACE_CAM_MAX_FACES];
} face_cam_frame_t;

/* Initializes the camera driver and the face detection model. */
esp_err_t face_cam_init(void);

/* Captures one frame and runs face detection on it. Blocking (typically a
 * few hundred ms); safe to call from multiple tasks (serialized internally).
 * On success the caller owns *out and must release it with
 * face_cam_frame_free(). */
esp_err_t face_cam_capture(face_cam_frame_t *out);

/* Frees the buffers held by *out (safe on a zeroed struct). */
void face_cam_frame_free(face_cam_frame_t *out);

#ifdef __cplusplus
}
#endif
