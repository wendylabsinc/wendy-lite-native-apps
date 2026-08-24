/*
 *  abc_core.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include "abc_core.h"
#include "gmutil.h"
// under ESP-IDF, use the native API even when arduino-esp32 is in the build
#if defined(ARDUINO) && !defined(ESP_PLATFORM)
#include <Arduino.h>
#else
#include "esp_timer.h"
#include "esp_system.h"
#endif

/*** globals ***/


/*** prototypes ***/

#if defined(ESP8266)
void __real_system_restart_local();
#endif


/*** functions ***/

static void _cpu_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    core_print_cpu_info();
}

static void _mem_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    core_print_mem_info();
}

static void _memfill_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    core_fill_heap_and_stack();
}

static void _memdump_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    core_dump_heap_and_stack();
}

static void _reset_cmd(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it)
{
    core_system_reset();
}

static void _tick_reg_get(const struct reg_def *def, struct reg_ctx ctx, void *val)
{
    int32_t ret = core_get_tick();
    memcpy(val, &ret, sizeof(ret));  
}

void core_init(void)
{
}

void core_start(void)
{
    if (sizeof(unsigned long) != 4)
        printf("WARNING: bad resolution for core_get_tick()\n");
}

void core_print_cpu_info(void)
{
}

void core_print_mem_info(void)
{
}

void core_fill_heap_and_stack(void)
{
}

void core_dump_heap_and_stack(void)
{
}

void core_system_reset(void)
{
#if defined(ESP8266)
    __real_system_restart_local();
#elif defined(ESP32) || defined(ESP_PLATFORM)
    esp_restart();
#endif
}

int32_t core_get_tick(void)
{
#if defined(ARDUINO) && !defined(ESP_PLATFORM)
    return millis();
#else
    return (int32_t)(esp_timer_get_time() / 1000);
#endif
}

int core_interrupt_level(void)
{
   return 0; // TODO
}


/*** module ***/

static struct reg_def _regs[] = {
    {
        .type = REG_TYPE_I32,
        .name = "tick",
        .get = _tick_reg_get,
        .set = reg_fake_setter,
    },
};

static const struct cmd_def _cmds[] = {
    {
        .name = "cpu",
        .help = "show cpu info",
        .exec = _cpu_cmd,
    }, {
        .name = "mem",
        .help = "show memory info",
        .exec = _mem_cmd,
    }, {
        .name = "memfill",
        .help = "fill stack + heap",
        .exec = _memfill_cmd,
    }, {
        .name = "memdump",
        .help = "dump stack + heap",
        .exec = _memdump_cmd,
    }, {
        .name = "reset",
        .help = "CPU reset",
        .exec = _reset_cmd,
    }
};

const struct mod core_mod = {
    .name = "core",
    .description = "core features",
    .init_level = 0,
    .init = core_init,
    .start = core_start,
    .reg_list = _regs,
    .reg_count = GMU_ARRAY_LEN(_regs),
    .cmd_list = _cmds,
    .cmd_count = GMU_ARRAY_LEN(_cmds),
};
