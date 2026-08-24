/*
 *  abc_gen.c
 *
 *  Copyright (c) 2026 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "gmutil.h"
#include "abc_gen.h"
#include "abc_app.h"
#include "abc_core.h"
#include "abc_cli.h"


/*** types ***/

enum gen_shape {
    GEN_SHAPE_NONE = 0,
    GEN_SHAPE_RAMP,
    GEN_SHAPE_SIN,
};


/*** globals ***/

static int _shape = GEN_SHAPE_NONE;
static const struct reg_def *_reg;
static struct reg_ctx _reg_ctx;
static float _start_value;
static float _final_value;
static int32_t _start_tick;
static int32_t _duration;


/*** functions ***/

static float _reg_read(const struct reg_def *reg, struct reg_ctx ctx)
{
    switch (reg->type) {
        case REG_TYPE_I32:
            return (float)reg_get_i32(reg, ctx);
        case REG_TYPE_U32:
            return (float)reg_get_u32(reg, ctx);
        default:
            return reg_get_f32(reg, ctx);
    }
}

static void _reg_write(const struct reg_def *reg, struct reg_ctx ctx, float value)
{
    switch (reg->type) {
        case REG_TYPE_I32:
            reg_set_i32(reg, ctx, (int32_t)lroundf(value));
            break;
        case REG_TYPE_U32:
            reg_set_u32(reg, ctx, (uint32_t)lroundf(value));
            break;
        default:
            reg_set_f32(reg, ctx, value);
            break;
    }
}

static void _gen_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    const char *arg;

    mod_arg_iterator_next(arg_it);
    arg = arg_it->name;
    if (!arg) {
        printf("missing argument\n");
        return;
    }

    struct reg_ctx reg_ctx;
    const struct reg_def *reg_def = app_reg_lookup(arg, &reg_ctx);
    if (!reg_def) {
        printf("unknown register '%s'\n", arg);
        return;
    }
    if (reg_def->type != REG_TYPE_F32 && reg_def->type != REG_TYPE_I32 && reg_def->type != REG_TYPE_U32) {
        printf("bad register type\n");
        return;
    }

    mod_arg_iterator_next(arg_it);
    arg = arg_it->name;
    if (!arg) {
        printf("missing argument\n");
        return;
    }
    float final_value = strtof(arg, NULL);

    mod_arg_iterator_next(arg_it);
    arg = arg_it->name;
    if (!arg) {
        printf("missing argument\n");
        return;
    }
    float time = strtof(arg, NULL);

    _shape = GEN_SHAPE_NONE;
    if (time <= 0.0f) {
        _reg_write(reg_def, reg_ctx, final_value);
        return;
    }

    _reg = reg_def;
    _reg_ctx = reg_ctx;
    _start_value = _reg_read(reg_def, reg_ctx);
    _final_value = final_value;
    _duration = (int32_t)(time * 1000.0f);
    _start_tick = core_get_tick();
    _shape = (int)ctx.tag;
}

static void _esc_handler(void)
{
    _shape = GEN_SHAPE_NONE;
}

static void _init(void)
{
    cli_add_fast_esc_handler(_esc_handler);
}

static void _loop(void)
{
    if (_shape == GEN_SHAPE_NONE)
        return;
    int32_t elapsed = gmu_sub_s32(core_get_tick(), _start_tick);
    if (elapsed >= _duration) {
        _reg_write(_reg, _reg_ctx, _final_value);
        _shape = GEN_SHAPE_NONE;
        return;
    }
    float u = (float)elapsed / (float)_duration;
    if (_shape == GEN_SHAPE_SIN)
        u = (1.0f - cosf((float)M_PI * u)) * 0.5f;
    _reg_write(_reg, _reg_ctx, _start_value + (_final_value - _start_value) * u);
}


/*** module ***/

static const struct cmd_def _cmds[] = {
    {
        .name = "gramp",
        .ctx = { .tag = GEN_SHAPE_RAMP },
        .usage = "gramp <reg> <value> <time>",
        .help = "ramp register linearly to value in time seconds",
        .exec = _gen_cmd,
    },
    {
        .name = "gsin",
        .ctx = { .tag = GEN_SHAPE_SIN },
        .usage = "gsin <reg> <value> <time>",
        .help = "move register to value along an s-curve in time seconds",
        .exec = _gen_cmd,
    },
};

const struct mod gen_mod = {
    .name = "gen",
    .description = "signal generator",
    .init_level = 5,
    .init = _init,
    .loop = _loop,
    .cmd_list = _cmds,
    .cmd_count = GMU_ARRAY_LEN(_cmds),
};
