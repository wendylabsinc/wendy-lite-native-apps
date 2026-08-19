#pragma once

/* Binary TCP frame server. Listens on SERVER_PORT, serves one client at a
 * time and drives both directions of the connection from a single select()
 * loop.
 *
 * The wire format is specified in ../PROTOCOL.md, which is the reference for
 * message layouts, field semantics and error handling. The constants below
 * must agree with it. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVER_PORT 3333

#define SERVER_MAGIC 0xAF
#define SERVER_HEADER_LEN 4

/* Types 1 and 2 are reserved for future use. */
#define SERVER_MSG_TYPE_HANDSHAKE 0
#define SERVER_MSG_TYPE_REQUEST 3
#define SERVER_MSG_TYPE_DATA 4

#define SERVER_VERSION_MAJOR 1
#define SERVER_VERSION_MINOR 0

#define SERVER_CHANNEL_VIDEO 1

/* Header and payload together, so a message still fits in a single TCP
 * segment. */
#define SERVER_MAX_MSG_LEN 1408
#define SERVER_DATA_HEADER_LEN 20
#define SERVER_MAX_CHUNK_DATA (SERVER_MAX_MSG_LEN - SERVER_HEADER_LEN - SERVER_DATA_HEADER_LEN)

/* Initializes the camera, binds the listening socket, advertises the service
 * over mDNS and starts the server task. The task runs for the lifetime of the
 * application: there is no counterpart to stop it. */
esp_err_t server_start(void);

#ifdef __cplusplus
}
#endif
