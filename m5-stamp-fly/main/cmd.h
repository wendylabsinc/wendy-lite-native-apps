/*
 *  cmd.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _CMD_H_
#define _CMD_H_

#include "mod.h" // cross referenced


/*** types ***/

struct cmd_ctx {
    intptr_t tag;
};

struct cmd_def {
    const char *name;
    struct cmd_ctx ctx;
    const char *usage;
    const char *help;
    void (* exec)(const struct cmd_def *cmd, struct cmd_ctx ctx, struct mod_arg_iterator *arg_it);
};

struct cmd {
    const struct cmd_def *def;
    struct cmd_ctx ctx;
};


/*** prototypes ***/

void cmd_help(const struct cmd_def *cmd_list, int cmd_count, int indent);


/*** inline functions ***/

static inline const struct cmd_def *cmd_lookup(const struct cmd_def *cmd_lst, int cmd_cnt, const char *cmd_name, struct cmd_ctx *ctx_out)
{
    int i;

    for (i=0; i<cmd_cnt; i++) {
        const struct cmd_def *cmd_def = cmd_lst + i;
        if (!strcmp(cmd_def->name, cmd_name)) {
            *ctx_out = cmd_def->ctx;
            return cmd_def;
        }
    }
    return NULL;
}


#endif
