/*
 *  cli.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "cli.h"
#include "mod.h"
#include "gmutil.h"
#include "core.h"


/*** literals ***/

#define ESC_HANDLER_MAX     4

#define CMD_BUF_LEN 128
#if (CMD_BUF_LEN - 1) & CMD_BUF_LEN
#error CMD_BUF_LEN must be a power of two
#endif
#define CMD_BUF_MSK (CMD_BUF_LEN - 1)

#define ECHO_DISABLED       0
#define ECHO_ENABLED        1
#define ECHO_LINE_DISABLED  2


/*** types ***/

struct esc_handler {
    bool fast;
    void (* func)(void);
};


/*** globals ***/

static char _prefix;
static int _echo = ECHO_ENABLED;
static struct esc_handler _esc_handlers[ESC_HANDLER_MAX];
static bool _slow_esc_pending = false;
static struct cli_io _io;


/*** functions ***/

static void _fire_esc_handlers(bool fast)
{
    for (int i=0; i<ESC_HANDLER_MAX; i++) {
        struct esc_handler h = _esc_handlers[i];
        if (h.func && h.fast == fast)
            h.func();
    }
}

void cli_loop(void)
{
    if (_slow_esc_pending) {
        _slow_esc_pending = false;
        _fire_esc_handlers(false);
    }
}

void cli_set_io(const struct cli_io *io)
{
    _io = *io;
}

void cli_add_esc_handler(void (* handler)(void))
{
    for (int i=0; i<ESC_HANDLER_MAX; i++) {
        if (!_esc_handlers[i].func) {
            _esc_handlers[i].func = handler;
            _esc_handlers[i].fast = false;
            return;
        }
    }
}

void cli_remove_esc_handler(void (* handler)(void))
{
    for (int i=0; i<ESC_HANDLER_MAX; i++) {
        if (_esc_handlers[i].func == handler && !_esc_handlers[i].fast) {
            _esc_handlers[i].func = NULL;
            return;
        }
    }
}

/**
 * A fast handler is an handler that can be invoked directly from an interrupt
 * routine.
 */
void cli_add_fast_esc_handler(void (* handler)(void))
{
    for (int i=0; i<ESC_HANDLER_MAX; i++) {
        if (!_esc_handlers[i].func) {
            _esc_handlers[i].func = handler;
            _esc_handlers[i].fast = true;
            return;
        }
    }
}

void cli_remove_fast_esc_handler(void (* handler)(void))
{
    for (int i=0; i<ESC_HANDLER_MAX; i++) {
        if (_esc_handlers[i].func == handler && _esc_handlers[i].fast) {
            _esc_handlers[i].func = NULL;
            return;
        }
    }
}

void cli_fire_esc_handlers(void)
{
    if (core_interrupt_level() > 0) {
        _fire_esc_handlers(true);
        _slow_esc_pending = true;
    } else {
        _fire_esc_handlers(true);
        _fire_esc_handlers(false);
    }
}

/**
 * Get the command line entered by the user on the console.
 * Return a negative value on error, zero if no line is available,
 * one if a line is available.
 * The returned line does not contain any CR or LR character.
 */
int cli_get_line(char *buf, int max)
{
    static char cmd_buf[CMD_BUF_LEN];
    static int cmd_in;

    // is there a char ?
    int c = _io.read_char();
    if (c < 0)
        return 0;

    if (c == 3) {
        // got ctrl-c
        cmd_in = 0;
        if (_echo == ECHO_ENABLED)
            _io.write_char('\n');
        if (_echo == ECHO_LINE_DISABLED)
            _echo = ECHO_ENABLED;
    }

    if (cmd_in == 0 && (c == '$' || c == '^') && _echo == ECHO_ENABLED)
        _echo = ECHO_LINE_DISABLED;

    if (c == '\n') {
        strlcpy(buf, cmd_buf, GMU_MIN(cmd_in + 1, max));
        cmd_in = 0;
        if (_echo == ECHO_ENABLED)
            _io.write_char('\n');
        if (_echo == ECHO_LINE_DISABLED)
            _echo = ECHO_ENABLED;
        return 1;
    }

    // handle back space
    if (c == 0x7f) {
        if (cmd_in == 0)
            return 0;
        if (_echo == ECHO_ENABLED)
            _io.write_buf("\010\040\010", 3);
        cmd_in--;
        return 0;
    }

    if (cmd_in < CMD_BUF_LEN)
        cmd_buf[cmd_in++] = c;

    if (_echo == ECHO_ENABLED)
        _io.write_char(c);

    return 0;
}

void cli_expect_response(char prefix)
{
    _prefix = prefix;
}

bool cli_is_response_expected(void)
{
    return _prefix != 0;
}

void cli_prefix_response(void)
{
    if (_prefix != 0) {
        printf("%c", _prefix);
        _prefix = 0;
    }
}

void cli_respond_nothing(void)
{
    if (_prefix != 0) {
        printf("%c\n", _prefix);
        _prefix = 0;
    }
}


/*** module ***/

const struct mod cli_mod = {
    .name = "cli",
    .description = "command line interface",
    .init_level = 3,
    .loop = cli_loop,
};
