#pragma once

/* Binary TCP frame server. Listens on SERVER_PORT, serves one client at a
 * time and drives both directions of the connection from a single select()
 * loop.
 *
 * Everything is exchanged as messages: a 4 byte header followed by a payload,
 * every multi-byte field in network order (big-endian).
 *
 *   offset 0: magic (SERVER_MAGIC)
 *   offset 1: message type
 *   offset 2: payload size (16 bits)
 *
 * Type 1, request, client to device, 12 byte payload:
 *
 *   offset  0: channel (SERVER_CHANNEL_VIDEO is the only one for now)
 *   offset  1: number of frames (ignored for now, always treated as 1)
 *   offset  2: reserved (2 bytes)
 *   offset  4: delay in microseconds since the last frame (ignored for now)
 *   offset  8: request id, echoed back in every chunk of the frame
 *
 * Type 2, data, device to client, 20 byte payload plus image data:
 *
 *   offset  0: channel
 *   offset  1: last_chunk flag (1 bit) then chunk number (23 bits)
 *   offset  4: frame number
 *   offset  8: frame timestamp in microseconds
 *   offset 12: host timestamp in microseconds
 *   offset 16: request id
 *   offset 20: image data
 *
 * A video frame is a JPEG image sent as a run of chunks numbered from 0, the
 * last one carrying the last_chunk flag, so that no message ever exceeds
 * SERVER_MAX_MSG_LEN. */

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

#define SERVER_MSG_TYPE_REQUEST 1
#define SERVER_MSG_TYPE_DATA 2

#define SERVER_CHANNEL_VIDEO 1

/* Header and payload together, so a message still fits in a single TCP
 * segment. */
#define SERVER_MAX_MSG_LEN 1280
#define SERVER_DATA_HEADER_LEN 20
#define SERVER_MAX_CHUNK_DATA (SERVER_MAX_MSG_LEN - SERVER_HEADER_LEN - SERVER_DATA_HEADER_LEN)

/* Initializes the camera, binds the listening socket, advertises the service
 * over mDNS and starts the server task. The task runs for the lifetime of the
 * application: there is no counterpart to stop it. */
esp_err_t server_start(void);

#ifdef __cplusplus
}
#endif
