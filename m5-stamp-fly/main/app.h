/*
 *  app.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _APP_H_
#define _APP_H_

#include "mod.h"


/*** types ***/

struct app {
    const char *name;
    const char *version;
    const struct mod **module_list;
    int module_count;
};


/*** prototypes ***/

void app_init(const struct app *app);
void app_start(void);
void app_cycle(void);
void app_run(void);
void app_wait(int ms);
const struct reg_def *app_reg_lookup(const char *reg_name, struct reg_ctx *ctx_out);
const struct cmd_def *app_cmd_lookup(const char *cmd_name, struct cmd_ctx *ctx_out);


#endif
