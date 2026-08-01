/*
 *  mod.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include "reg.h" // cross referenced
#include "cmd.h" // cross referenced

#ifndef _MOD_H_
#define _MOD_H_

#include <stdbool.h>

struct reg_ctx;
struct cmd_ctx;

struct mod_arg_iterator {
    char *p;
    const char *name;
    int sep;
};

struct mod {
    const char *name;
    const char *description;
    int init_level;

    void (* init)(void);
    void (* start)(void);
    void (* loop)(void);
    const struct reg_def *reg_list;
    int reg_count;
    const struct cmd_def *cmd_list;
    int cmd_count;
    const struct reg_def *(* reg_lookup)(const char *reg_name, struct reg_ctx *ctx_out);
    const struct cmd_def *(* cmd_lookup)(const struct mod *mod, const char *cmd_name, struct cmd_ctx *ctx_out);
    void (* reg_help)(void);
    void (* cmd_help)(void);
};


void mod_arg_iterator_init(struct mod_arg_iterator *arg_iterator, char *cmd_line);
const char *mod_arg_iterator_next(struct mod_arg_iterator *arg_iterator);
const char *mod_spaces(int space_count);


#endif
