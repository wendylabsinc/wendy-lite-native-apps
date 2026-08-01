/*
 *  ser.cpp
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <Arduino.h>

extern "C" {
#include "ser.h"
#include "cli.h"
#include "mod.h"
}

// CLI port: USBSerial (USB Serial/JTAG) by default; override with -DSER_PORT=Serial for UART0
#ifndef SER_PORT
#define SER_PORT USBSerial
#endif

/*** globals ***/

const struct cli_io ser_io = {
    .read_char = ser_getc,
    .write_char = ser_putc,
    .read_buf = nullptr,
    .write_buf = ser_write,
};

/*** functions ***/

void ser_init(void)
{
    SER_PORT.begin(115200);
}

void ser_write(const void *buf, int len)
{
    uint8_t *ibuf = (uint8_t *)buf;
    uint8_t obuf[2 * len];
    int o = 0;

    for (int i=0; i<len; i++) {
        uint8_t c = ibuf[i];
        if (c == 10)
            obuf[o++] = 13;
        obuf[o++] = c;
    }

    SER_PORT.write(obuf, o);
}

void ser_putc(char c)
{
    ser_write(&c, 1);
}

int ser_getc(void)
{
    static int skip_c = -1;
    uint8_t c;

    for (;;) {
        int rc = SER_PORT.read();
        if (rc < 0)
            return -1;
            
        c = (uint8_t)rc;

        if (c == 27) {
            cli_fire_esc_handlers();
            skip_c = -1;
            continue;
        }

        if ((int)c == skip_c) {
            skip_c = -1;
            continue;
        }

        if (c == '\r') {
            skip_c = '\n';
            return '\n';
        }

        if (c == '\n') {
            skip_c = '\r';
            return '\n';
        }

        skip_c = -1;
        return c;
    }
}


/*** module ***/

const struct mod ser_mod = {
    .name = "ser",
    .description = "serial line",
    .init_level = 3,
    .init = ser_init,
};
