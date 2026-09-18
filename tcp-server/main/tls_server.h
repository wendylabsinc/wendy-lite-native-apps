#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TLS, one port above the plain server, so the two are easy to compare. */
#define TLS_SERVER_PORT 9001

/**
 * Start the TLS bandwidth server task. Same payload as tcp_server, wrapped in
 * TLS using the self-signed certificate wendy_conf embeds. No client
 * certificate is requested or verified. Call after payload_init(), and after
 * wendy_core_init() has run wendy_conf_init().
 */
esp_err_t tls_server_start(void);

#ifdef __cplusplus
}
#endif
