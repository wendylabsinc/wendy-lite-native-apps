#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The repeating unit of the test payload. Self-delimiting, so a client can see
 * at a glance that the stream arrived intact. */
#define PAYLOAD_PATTERN     "hello-world-"
#define PAYLOAD_PATTERN_LEN 12u

/* What every connection receives before the server closes it: 10 MiB. Not a
 * multiple of PAYLOAD_PATTERN_LEN, so the stream ends mid-pattern on "hell". */
#define PAYLOAD_TOTAL_BYTES (10u * 1024u * 1024u)

/* Bytes handed to a single send() / esp_tls_conn_write() call.
 *
 * lwIP parks the calling task once per send(), not once per send-buffer
 * refill, so this size sets how many times a 10 MiB transfer bounces through
 * the scheduler: 320 times here, against 1821 if we sent one TCP send buffer
 * (CONFIG_LWIP_TCP_SND_BUF_DEFAULT, 5760 bytes) at a time. Doubling it again
 * would halve that once more, but 160 wakeups spread over ten seconds is
 * already far below the noise floor of a WiFi measurement, and the memory is
 * better left to the TLS handshake.
 *
 * A multiple of PAYLOAD_PATTERN_LEN, so every chunk starts at pattern offset 0
 * and the repetition stays continuous across chunk boundaries. */
#define PAYLOAD_CHUNK_BYTES (2730u * PAYLOAD_PATTERN_LEN) /* 32760, just under 32 KiB */

/**
 * Build the shared chunk. Call once from app_main() after wendy_core_init(),
 * before starting any server.
 */
esp_err_t payload_init(void);

/**
 * PAYLOAD_CHUNK_BYTES of repeating PAYLOAD_PATTERN. Read-only and shared by
 * every server task; NULL until payload_init() has succeeded.
 */
const uint8_t *payload_chunk(void);

#ifdef __cplusplus
}
#endif
