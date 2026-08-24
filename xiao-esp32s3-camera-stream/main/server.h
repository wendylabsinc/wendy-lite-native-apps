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

/* Type 1 is reserved for future use. */
#define SERVER_MSG_TYPE_HANDSHAKE 0
#define SERVER_MSG_TYPE_COMMAND 2
#define SERVER_MSG_TYPE_REQUEST 3
#define SERVER_MSG_TYPE_DATA 4

#define SERVER_VERSION_MAJOR 1
#define SERVER_VERSION_MINOR 1

#define SERVER_COMMAND_HEADER_LEN 8

#define SERVER_COMMAND_PING 0
#define SERVER_COMMAND_CHANNELS 1

/* Bits of the chunk and flags field of a command. The chunk number occupies the
 * low 12 bits; only single chunk commands, numbered 0, are supported. */
#define SERVER_COMMAND_FLAG_ANSWER_EXPECTED 0x8000
#define SERVER_COMMAND_FLAG_ANSWER 0x4000
#define SERVER_COMMAND_FLAG_ERROR 0x2000
#define SERVER_COMMAND_FLAG_LAST_CHUNK 0x1000
#define SERVER_COMMAND_CHUNK_MASK 0x0fff

#define SERVER_CHANNEL_VIDEO 1
#define SERVER_CHANNEL_CATEGORY_VIDEO 1

/* The only variant this version of the protocol defines, so the one every
 * channel is enumerated with. */
#define SERVER_CHANNEL_VARIANT_VIDEO 0

/* What the channel enumeration reports for the video channel. 'MJPG', and the
 * frame size the camera is configured for in cam_capture.c. */
#define SERVER_MEDIA_TYPE_MJPG 0x4d4a5047
#define SERVER_VIDEO_WIDTH 800
#define SERVER_VIDEO_HEIGHT 600

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
