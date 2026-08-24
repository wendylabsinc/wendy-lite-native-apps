/*
 *  abc_con.c
 *
 *  Copyright (c) 2026 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "abc_con.h"
#include "abc_cli.h"
#include "abc_mod.h"


/*** globals ***/

const struct cli_io con_io = {
    .read_char = con_getc,
    .write_char = con_putc,
    .read_buf = NULL,
    .write_buf = con_write,
};


/*** functions ***/

void con_init(void)
{
    // non-blocking stdin so con_getc() can poll; fileno(stdin) rather than
    // STDIN_FILENO because wendy_stdio freopens stdin onto its own console
    // VFS, so the stream's fd is no longer 0
    int fd = fileno(stdin);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void con_write(const void *buf, int len)
{
    // the console VFS already translates LF to CRLF
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

void con_putc(char c)
{
    con_write(&c, 1);
}

int con_getc(void)
{
    static int skip_c = -1;
    uint8_t c;

    for (;;) {
        int rc = read(fileno(stdin), &c, 1);
        if (rc <= 0)
            return -1;

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

const struct mod con_mod = {
    .name = "con",
    .description = "stdio console",
    .init_level = 3,
    .init = con_init,
};
