/*
 *  reg.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "reg.h"
#include "mod.h"


const char *reg_type_str(const struct reg_def *def)
{
    switch (def->type) {
        case REG_TYPE_BOOL:
            return "bool";
        case REG_TYPE_I32:
            return "i32";
        case REG_TYPE_U32:
            return "u32";
        case REG_TYPE_F32:
            return "f32";
        default:
            return "?";
    }
}

void reg_set_from_str(const struct reg_def *def, struct reg_ctx ctx, const char *val_str)
{
    switch (def->type) {
        case REG_TYPE_BOOL: {
            bool val = strtol(val_str, NULL, 0) != 0;
            reg_set_bool(def, ctx, val);
            return;
        }
        case REG_TYPE_I32: {
            int32_t val = (int32_t)strtol(val_str, NULL, 0);
            reg_set_i32(def, ctx, val);
            return;
        }
        case REG_TYPE_U32: {
            uint32_t val = (uint32_t)strtoul(val_str, NULL, 0);
            reg_set_u32(def, ctx, val);
            return;
        }
        case REG_TYPE_F32: {
            float val = strtof(val_str, NULL);
            reg_set_f32(def, ctx, val);
            return;
        }
        default:
            printf("bad register type\n");
            return;
    }
}

void reg_print(const struct reg_def *def, struct reg_ctx ctx)
{
    switch (def->type) {
        case REG_TYPE_BOOL:
            printf("%d", (int)reg_get_bool(def, ctx));
            return;
        case REG_TYPE_I32:
            printf("%d", (int)reg_get_i32(def, ctx));
            return;
        case REG_TYPE_U32:
            printf("%u", (unsigned)reg_get_u32(def, ctx));
            return;
        case REG_TYPE_F32:
            printf("%g", (double)reg_get_f32(def, ctx));
            return;
        default:
            printf("bad register type\n");
            return;
    }
}

void reg_help(const struct reg_def *reg_lst, int reg_cnt, int indent)
{
    int i;
    int name_width = 0;
    int type_width = 0;

    for (i=0; i<reg_cnt; i++) {
        const struct reg_def *def = reg_lst + i;
        size_t len = strlen(def->name);
        if (name_width < len)
            name_width = len;
        len = strlen(reg_type_str(def));
        if (type_width < len)
            type_width = len;
    }

    for (i=0; i<reg_cnt; i++) {
        const struct reg_def *def = reg_lst + i;
        const char *type = reg_type_str(def);
        const char *help = def->help;
        if (!help)
            help = "";
        printf("%s%s%s(%s)%s%s\n", mod_spaces(indent),
            def->name, mod_spaces(name_width + 1 - strlen(def->name)),
            type, mod_spaces(type_width + 3 - strlen(type)),
            help
        );
    }
}

/**
 * This getter always return zero.
 */
void reg_fake_getter(const struct reg_def *def, struct reg_ctx ctx, void *val)
{
    memset(val, 0, reg_size(def));
}

/**
 * This setter does nothing. Used as setter for read-only registers.
 */
void reg_fake_setter(const struct reg_def *def, struct reg_ctx ctx, const void *val)
{
    // do nothing
}
