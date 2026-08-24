/*
 *  abc_core.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _ABC_CORE_H_
#define _ABC_CORE_H_

#include <stdint.h>
#ifndef __cplusplus
#include <stdatomic.h>
#endif
#include "abc_mod.h"


/*** globals ***/

extern const struct mod core_mod;


/*** prototypes ***/

void core_init(void);
void core_print_cpu_info(void);
void core_print_mem_info(void);
void core_fill_heap_and_stack(void);
void core_dump_heap_and_stack(void);
void core_system_reset(void);
int32_t core_get_tick(void);
int core_interrupt_level(void);


#endif
