#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the camera (via cam_capture_init()) and starts the internal
 * task that will drive capture/send cycles once a stream is requested. */
esp_err_t cam_loop_init(void);

/* Latches (client_id, channel_id) as the stream to run. Only the most
 * recent call matters if the task hasn't picked up a previous one yet; an
 * already-running stream is left alone until it stops on its own. */
void cam_loop_start(int client_id, uint32_t channel_id);

/* Stops the stream matching (client_id, channel_id) as soon as possible,
 * or cancels it if it was only latched and not yet started. No-op if
 * neither matches. */
void cam_loop_stop(int client_id, uint32_t channel_id);

#ifdef __cplusplus
}
#endif
