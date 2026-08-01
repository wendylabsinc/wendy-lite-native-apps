/*
 *  gmutil.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _GMUTIL_H_
#define _GMUTIL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"


/*** literals ***/

#ifdef __cplusplus
#define GMU_C_BEGIN extern "C" {
#define GMU_C_END   }
#else
#define GMU_C_BEGIN
#define GMU_C_END
#endif

#define GMU_ASYM_DIV(n, d) (((n)<0) ? (-((-(n)-1)/(d)+1)) : ((n)/(d)))
#define GMU_ASYM_MOD(n, d) (((n)<0) ? ((d)-1-((-(n)-1)%(d))) : ((n)%(d)))

#define GMU_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define GMU_MAX(a, b) (((a) < (b)) ? (b) : (a))

#define GMU_ARRAY_LEN(array) (sizeof(array) / sizeof(*(array)))


/*** inline functions ***/

static inline void *gmu_ptr_add(void *ptr, int offset)
{
    return (char *)ptr + offset;
}

/**
 * Integer 32-bit signed addition with roll over.
 * This is useful only when gcc -fwrapv option is missing.
 */
static inline int32_t gmu_add_s32(int32_t a, int32_t b)
{
    return (uint32_t)a + (uint32_t)b;
}

/**
 * Integer 32-bit signed subtraction with roll over.
 * This is useful only when gcc -fwrapv option is missing.
 */
static inline int32_t gmu_sub_s32(int32_t a, int32_t b)
{
    return (uint32_t)a - (uint32_t)b;
}

static inline bool gmu_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool gmu_get_as_bool(const void *p)
{
    bool ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

static inline int32_t gmu_get_as_i32(const void *p)
{
    int ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

static inline int64_t gmu_get_as_i64(const void *p)
{
    int64_t ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

static inline float gmu_get_as_f32(const void *p)
{
    float ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

static inline double gmu_get_as_f64(const void *p)
{
    double ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

static inline void gmu_set_as_bool(void *p, bool v)
{
    memcpy(p, &v, sizeof(v));
}

static inline void gmu_set_as_i32(void *p, int32_t v)
{
    memcpy(p, &v, sizeof(v));
}

static inline void gmu_set_as_i64(void *p, int64_t v)
{
    memcpy(p, &v, sizeof(v));
}

static inline void gmu_set_as_f32(void *p, float v)
{
    memcpy(p, &v, sizeof(v));
}

static inline void gmu_set_as_f64(void *p, double v)
{
    memcpy(p, &v, sizeof(v));
}

static inline int32_t gmu_mtime_ms(void)
{
    return core_get_tick();
}


#endif
