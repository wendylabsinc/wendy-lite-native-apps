/*
 *  reg.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _REG_H_
#define _REG_H_

#include <stdint.h>
#include <string.h>
#include "mod.h" // cross referenced


/*** types ***/

enum reg_type {
    REG_TYPE_UNDEFINED = 0,
    REG_TYPE_BOOL,
    REG_TYPE_I32,
    REG_TYPE_U32,
    REG_TYPE_F32,
};

union reg_val {
    bool b;
    int32_t i32;
    uint32_t u32;
    float f;
};

struct reg_ctx {
    intptr_t tag;
};

struct reg_def {
    enum reg_type type;
    const char *name;
    void *value;
    struct reg_ctx ctx;
    const char *help;
    void (* get)(const struct reg_def *def, struct reg_ctx ctx, void *val);
    void (* set)(const struct reg_def *def, struct reg_ctx ctx, const void *val);
};

struct reg {
    const struct reg_def *def;
    struct reg_ctx ctx;
};


/*** functions ***/

const char *reg_type_str(const struct reg_def *def);
void reg_set_from_str(const struct reg_def *def, struct reg_ctx ctx, const char *val_str);
void reg_print(const struct reg_def *def, struct reg_ctx ctx);
void reg_help(const struct reg_def *reg_lst, int reg_cnt, int indent);
void reg_fake_getter(const struct reg_def *def, struct reg_ctx ctx, void *val);
void reg_fake_setter(const struct reg_def *def, struct reg_ctx ctx, const void *val);


/*** inline functions ***/

static inline size_t reg_size(const struct reg_def *def)
{
    switch (def->type) {
        case REG_TYPE_BOOL:
            return sizeof(bool);
        case REG_TYPE_I32:
            return sizeof(int32_t);
        case REG_TYPE_U32:
            return sizeof(uint32_t);
        case REG_TYPE_F32:
            return sizeof(float);
        default:
            return 0;
    }
    return 4;
}

static inline bool reg_get_bool(const struct reg_def *reg, struct reg_ctx ctx)
{
    bool ret;
    if (reg->get)
        reg->get(reg, ctx, &ret);
    else
        memcpy(&ret, reg->value, sizeof(ret));
    return ret;
}

static inline int32_t reg_get_i32(const struct reg_def *reg, struct reg_ctx ctx)
{
    int32_t ret;
    if (reg->get)
        reg->get(reg, ctx, &ret);
    else
        memcpy(&ret, reg->value, sizeof(ret));
    return ret;
}

static inline uint32_t reg_get_u32(const struct reg_def *reg, struct reg_ctx ctx)
{
    uint32_t ret;
    if (reg->get)
        reg->get(reg, ctx, &ret);
    else
        memcpy(&ret, reg->value, sizeof(ret));
    return ret;
}

static inline float reg_get_f32(const struct reg_def *reg, struct reg_ctx ctx)
{
    float ret;
    if (reg->get)
        reg->get(reg, ctx, &ret);
    else
        memcpy(&ret, reg->value, sizeof(ret));
    return ret;
}

static inline void reg_set_bool(const struct reg_def *reg, struct reg_ctx ctx, bool val)
{
    if (reg->set)
        reg->set(reg, ctx, &val);
    else
        memcpy(reg->value, &val, sizeof(val));
}

static inline void reg_set_i32(const struct reg_def *reg, struct reg_ctx ctx, int32_t val)
{
    if (reg->set)
        reg->set(reg, ctx, &val);
    else
        memcpy(reg->value, &val, sizeof(val));
}

static inline void reg_set_u32(const struct reg_def *reg, struct reg_ctx ctx, uint32_t val)
{
    if (reg->set)
        reg->set(reg, ctx, &val);
    else
        memcpy(reg->value, &val, sizeof(val));
}

static inline void reg_set_f32(const struct reg_def *reg, struct reg_ctx ctx, float val)
{
    if (reg->set)
        reg->set(reg, ctx, &val);
    else
        memcpy(reg->value, &val, sizeof(val));
}

static inline const struct reg_def *reg_lookup(const struct reg_def *reg_lst, int reg_cnt, const char *reg_name, struct reg_ctx *ctx_out)
{
    for (int i=0; i<reg_cnt; i++) {
        const struct reg_def *reg_def = reg_lst + i;
        if (!strcmp(reg_def->name, reg_name)) {
            *ctx_out = reg_def->ctx;
            return reg_def;
        }
    }
    return NULL;
}


#endif
