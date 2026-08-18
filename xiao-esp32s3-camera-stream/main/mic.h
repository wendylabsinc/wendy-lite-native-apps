#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the onboard PDM microphone and starts the capture loop task. */
esp_err_t mic_start(void);

/* Stops the capture loop and deinitializes the microphone. */
void mic_stop(void);

#ifdef __cplusplus
}
#endif
