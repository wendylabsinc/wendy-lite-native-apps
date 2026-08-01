/*
 *  vl53lx_wire.cpp
 *
 *  See vl53lx_wire.h. Uses Wire1, which sensor_init() sets up on the
 *  StampFly sensor bus (SDA 3 / SCL 4, 400 kHz).
 *
 *  Lock discipline: Wire1's internal mutex is taken by beginTransmission()
 *  and only released by endTransmission(true) or by requestFrom() after a
 *  deferred endTransmission(false) — so every path below must reach one of
 *  those before returning, or any other Wire1 user deadlocks forever.
 */

#include "vl53lx_wire.h"
#include <Wire.h>
#include <stdio.h>

/* Rate-limited: a NACKing device gets polled ~30 Hz and would spam the log. */
static void log_failure(const char *op, uint8_t addr)
{
    static int budget = 5;
    if (budget > 0) {
        budget--;
        printf("vl53lx: i2c %s to addr 0x%02X failed%s\n", op, addr,
               budget ? "" : " (further i2c errors muted)");
    }
}

int vl53lx_wire_write(uint8_t addr, const uint8_t *data, uint32_t count)
{
    Wire1.beginTransmission(addr);
    size_t written = Wire1.write(data, count);
    uint8_t err = Wire1.endTransmission(true);
    if (written != count || err != 0) {
        log_failure("write", addr);
        return -1;
    }
    return 0;
}

int vl53lx_wire_write_read(uint8_t addr, const uint8_t *wdata, uint32_t wcount,
                           uint8_t *rdata, uint32_t rcount)
{
    Wire1.beginTransmission(addr);
    if (Wire1.write(wdata, wcount) != wcount) {
        Wire1.endTransmission(true);  // complete the transaction to free the lock
        log_failure("short write", addr);
        return -1;
    }
    // deferred: nothing is sent until requestFrom(), which issues the write,
    // a repeated start, and the read as one transaction
    if (Wire1.endTransmission(false) != 0) {
        Wire1.endTransmission(true);  // force completion to free the lock
        log_failure("write", addr);
        return -1;
    }
    if (Wire1.requestFrom((uint16_t)addr, (size_t)rcount, true) != rcount) {
        log_failure("read", addr);
        return -1;
    }
    for (uint32_t i = 0; i < rcount; i++)
        rdata[i] = Wire1.read();
    return 0;
}
