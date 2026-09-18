#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Plain TCP. Deliberately different from the port wendy_server holds (5054). */
#define TCP_SERVER_PORT 9000

/**
 * Start the plain-TCP bandwidth server task. It listens on TCP_SERVER_PORT and
 * sends PAYLOAD_TOTAL_BYTES to each client in turn, then closes. Call after
 * payload_init().
 */
esp_err_t tcp_server_start(void);

#ifdef __cplusplus
}
#endif
