#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the camera and starts the capture loop task. */
esp_err_t camera_start(void);

/* Stops the capture loop and deinitializes the camera. */
void camera_stop(void);

#ifdef __cplusplus
}
#endif
