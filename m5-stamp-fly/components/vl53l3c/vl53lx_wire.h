/*
 *  vl53lx_wire.h
 *
 *  I2C primitives for the VL53LX platform layer, implemented on Arduino
 *  Wire1. The legacy driver/i2c.h API used before cannot coexist with the
 *  new i2c_master driver that Wire uses in arduino-esp32 3.x: ESP-IDF
 *  aborts at boot if both are linked into the binary.
 */

#ifndef VL53LX_WIRE_H
#define VL53LX_WIRE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* single write transaction; returns 0 on success */
int vl53lx_wire_write(uint8_t addr, const uint8_t *data, uint32_t count);

/* write then repeated-start read, one transaction; returns 0 on success */
int vl53lx_wire_write_read(uint8_t addr, const uint8_t *wdata, uint32_t wcount,
                           uint8_t *rdata, uint32_t rcount);

#ifdef __cplusplus
}
#endif

#endif
