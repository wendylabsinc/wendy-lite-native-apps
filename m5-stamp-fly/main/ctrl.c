/*
 *  ctrl.c
 *
 *  Copyright (c) 2026 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include "gmutil.h"
#include "ctrl.h"
#include "cli.h"
#include "rc.hpp"


/*** globals ***/

static bool _stop = false;
static uint32_t _on_count;
static uint32_t mode;
extern volatile uint8_t Mode;
static uint32_t seq = 1; // autosstart
static uint32_t seq_time;


/*** functions ***/

static void _on(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    _stop = false;
    _on_count = 100;
}

static void _esc_handler(void)
{
    _stop = true;
    _on_count = 0;
}

static void _init(void)
{
    cli_add_fast_esc_handler(_esc_handler);
}

static void _loop(void)
{
    mode = Mode;
    if (_on_count > 0) {
        _on_count--;
        Stick[BUTTON_ARM] = (_on_count > 0) ? 1 : 0;
    }
    if (_stop) {
        seq = 0;
    } else {
        Connect_flag = 0;
    }

    if (seq == 1) {
        printf("start sequence\n");
        seq_time = core_get_tick();
        seq = 2;
    }
    if (seq == 2) {
        uint32_t d = core_get_tick() - seq_time;
        if (d >= 6000) {
            seq = 3;
        }
    }
    if (seq == 3) {
        printf("take-off\n");
        Stick[ALTCONTROLMODE] = 4;
        _on_count = 100;
        seq_time = core_get_tick();
        seq = 4;
    }
    if (seq == 4) {
        uint32_t d = core_get_tick() - seq_time;
        if (d >= 4000) {
            seq = 5;
        }
    }
    if (seq == 5) {
        printf("landing\n");
        _on_count = 100;
        seq = 0;
    }
}


/*** module ***/

static struct reg_def _regs[] = {
    {
        .type = REG_TYPE_U32,
        .name = "seq",
        .value = &seq,
        .help = "current sequence step",
    },
    {
        .type = REG_TYPE_U32,
        .name = "mode",
        .value = &mode,
        .help = "current flight mode",
    },
    {
        .type = REG_TYPE_BOOL,
        .name = "stop",
        .value = &_stop,
        .help = "emergency stop, set by esc key",
    },
    {
        .type = REG_TYPE_U32,
        .name = "connect",
        .value = (void *)&Connect_flag,
        .help = "connection status",
    },
    {
        .type = REG_TYPE_F32,
        .name = "ru",
        .value = (void *)&Stick[RUDDER],
        .help = "rudder",
    },
    {
        .type = REG_TYPE_F32,
        .name = "el",
        .value = (void *)&Stick[ELEVATOR],
        .help = "elevator",
    },
    {
        .type = REG_TYPE_F32,
        .name = "th",
        .value = (void *)&Stick[THROTTLE],
        .help = "throttle",
    },
    {
        .type = REG_TYPE_F32,
        .name = "ai",
        .value = (void *)&Stick[AILERON],
        .help = "aileron",
    },
    {
        .type = REG_TYPE_F32,
        .name = "log",
        .value = (void *)&Stick[LOG],
    },
    {
        .type = REG_TYPE_F32,
        .name = "dpad_up",
        .value = (void *)&Stick[DPAD_UP],
    },
    {
        .type = REG_TYPE_F32,
        .name = "dpad_down",
        .value = (void *)&Stick[DPAD_DOWN],
    },
    {
        .type = REG_TYPE_F32,
        .name = "dpad_left",
        .value = (void *)&Stick[DPAD_LEFT],
    },
    {
        .type = REG_TYPE_F32,
        .name = "dpad_right",
        .value = (void *)&Stick[DPAD_RIGHT],
    },
    {
        .type = REG_TYPE_F32,
        .name = "arm",
        .value = (void *)&Stick[BUTTON_ARM],
    },
    {
        .type = REG_TYPE_F32,
        .name = "flip",
        .value = (void *)&Stick[BUTTON_FLIP],
    },
    {
        .type = REG_TYPE_F32,
        .name = "cmode",
        .value = (void *)&Stick[CONTROLMODE],
        .help = "control mode: 0=angle, 1=rate",
    },
    {
        .type = REG_TYPE_F32,
        .name = "altmode",
        .value = (void *)&Stick[ALTCONTROLMODE],
        .help = "altitude control mode: 4=auto, 5=manual", // any other value is manual as well
    },
};

static const struct cmd_def _cmds[] = {
    {
        .name = "on",
        .exec = _on,
    }
};

const struct mod ctrl_mod = {
    .name = "ctrl",
    .description = "flight control",
    .init_level = 5,
    .init = _init,
    .loop = _loop,
    .reg_list = _regs,
    .reg_count = GMU_ARRAY_LEN(_regs),
    .cmd_list = _cmds,
    .cmd_count = GMU_ARRAY_LEN(_cmds),
};
