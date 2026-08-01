/*
 *  app.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "gmutil.h"
#include "app.h"
#include "cli.h"
#include "mod.h"
#include "cmd.h"
#include "reg.h"


/*** literals ***/

#ifndef APP_GIT_VERSION
#define APP_GIT_VERSION "?"
#endif
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE "?"
#endif
#ifndef APP_BUILD_USER
#define APP_BUILD_USER "?"
#endif


/*** globals ***/

static const struct app *_app;
static const struct mod **_mod_lst;
static int _mod_cnt;


/*** functions ***/

void app_loop(void)
{
    const struct mod **beg = _mod_lst;
    const struct mod **end = _mod_lst + _mod_cnt;
    for (const struct mod **it = beg; it != end; it++) {
        const struct mod *mod = *it;        
        if (mod->loop)
            mod->loop();
    }
}

void app_process_cmd(struct mod_arg_iterator *arg_it)
{
    const char *arg;
    int sep;

    // get first arg and separator
    arg = arg_it->name;
    sep = arg_it->sep;

    // fast commands
    if (!strcmp(arg, "f123")) {
        return;
    }

    // register lookup
    struct reg_ctx reg_ctx;
    const struct reg_def *reg_def = app_reg_lookup(arg, &reg_ctx);
    if (reg_def) {
        if (sep == '=') {
            // set register command
            mod_arg_iterator_next(arg_it);
            arg = arg_it->name;
            if (arg)
                reg_set_from_str(reg_def, reg_ctx, arg);
            else
                printf("missing argument\n");
        } else {
            // get register command
            cli_prefix_response();
            reg_print(reg_def, reg_ctx);
            printf("\n");
        }
        return;
    }

    // command lookup
    struct cmd_ctx cmd_ctx;
    const struct cmd_def *cmd_def = app_cmd_lookup(arg, &cmd_ctx);
    if (cmd_def) {
        cmd_def->exec(cmd_def, cmd_ctx, arg_it);
        return;
    }
    
    // hard-coded commands
    if (!strcmp(arg, "help")) {
        mod_arg_iterator_next(arg_it);
        arg = arg_it->name;
        if (!arg) {
            printf("modules:\n");
            const struct mod **beg = _mod_lst;
            const struct mod **end = _mod_lst + _mod_cnt;
            for (const struct mod **it = beg; it != end; it++) {
                const struct mod *mod = *it;
                const char *desc = mod->description;
                if (!desc)
                    desc = "";
                printf(" %s\t%s\n", mod->name, desc);
            }
            printf(
                "commands:\n"
                " help *       show all registers and commands\n"
                " help <mod>   show registers and commands for a given module\n"
                " ver          show firmware version\n"
                " xver         show extended version\n"
                " wait <ms>    wait for a given number of milliseconds\n"
                " echo <text>  print the given text\n"
            );
        } else {
            const struct mod **beg = _mod_lst;
            const struct mod **end = _mod_lst + _mod_cnt;
            for (const struct mod **it = beg; it != end; it++) {
                const struct mod *mod = *it;        
                if (!strcmp(arg, "*") || !strcmp(arg, mod->name)) {
                    if (mod->reg_help) {
                        mod->reg_help();
                    } else if (mod->reg_list) {
                        printf("module %s - registers\n", mod->name);
                        reg_help(mod->reg_list, mod->reg_count, 1);
                    }
                    if (mod->cmd_help) {
                        mod->cmd_help();
                    } else if (mod->cmd_list) {
                        printf("module %s - commands\n", mod->name);
                        cmd_help(mod->cmd_list, mod->cmd_count, 1);
                    }
                }
            }
        }
    } else if (!strcmp(arg, "ver")) {
        cli_prefix_response();
        printf("%s\n", _app->version);
        return;
    } else if (!strcmp(arg, "xver")) {
        printf("version:    %s\n", _app->version);
        printf("git:        " APP_GIT_VERSION "\n");
        printf("build date: " APP_BUILD_DATE "\n");
        printf("user:       " APP_BUILD_USER "\n");
        return;
    } else if (!strcmp(arg, "wait")) {
        int t = 1000;
        mod_arg_iterator_next(arg_it);
        arg = arg_it->name;
        if (arg != NULL)
            t = (int)(strtof(arg, NULL) * 1000.0f);
        app_wait(t);
    } else if (!strcmp(arg, "echo")) {
        cli_prefix_response();
        printf("%s\n", arg_it->p);
    } else {
        printf("unknown command '%s'\n", arg);
    }
}

void app_process_cli_line(char *cmd)
{
    struct mod_arg_iterator arg_it;
    mod_arg_iterator_init(&arg_it, cmd);

    mod_arg_iterator_next(&arg_it);

    if (!arg_it.name)
        return; // empty command

    // does the command line start by a separator?
    if (arg_it.name[0] == 0) {
        int sep = arg_it.sep;
        mod_arg_iterator_next(&arg_it);
        if (!arg_it.name)
            return; // empty command
        if (sep == '$')
            cli_expect_response('$');
    }

    app_process_cmd(&arg_it);

    cli_respond_nothing();
}

void app_init(const struct app *app)
{
    _app = app;
    _mod_lst = app->module_list;
    _mod_cnt = app->module_count;

    int init_level = 0;
    int init_count = _mod_cnt;
    while (init_count) {
        const struct mod **beg = _mod_lst;
        const struct mod **end = _mod_lst + _mod_cnt;
        for (const struct mod **it = beg; it != end; it++) {
            const struct mod *mod = *it;
            if (mod->init_level == init_level) {
                if (mod->init)
                    mod->init();
                init_count--;
            }
        }
        init_level++;
    }
}

void app_start(void)
{
    printf("\n%s %s\n", _app->name, _app->version);

    const struct mod **beg = _mod_lst;
    const struct mod **end = _mod_lst + _mod_cnt;
    for (const struct mod **it = beg; it != end; it++) {
        const struct mod *mod = *it;        
        if (mod->start)
            mod->start();
    }
}

void app_cycle(void)
{
    app_loop();

    char cmd[128];
    int rv = cli_get_line(cmd, sizeof(cmd));
    if (rv == 0) {
        // CPUwfi(); => not safe!
    } else if (rv < 0) {
        printf("error %d\n", rv);
    } else {
        app_process_cli_line(cmd);
    }
}

void app_run(void)
{
    for (;;) {
        app_cycle();
    }
}

void app_wait(int ms) 
{
    int target = gmu_add_s32(gmu_mtime_ms(), ms);
    for (;;) {
        int d = gmu_sub_s32(target, gmu_mtime_ms());
        if (d <= 0)
            break;
        app_loop();
    }
}

const struct reg_def *app_reg_lookup(const char *reg_name, struct reg_ctx *ctx_out)
{
    const struct mod **beg = _mod_lst;
    const struct mod **end = _mod_lst + _mod_cnt;
    for (const struct mod **it = beg; it != end; it++) {
        const struct mod *mod = *it;        
        if (mod->reg_lookup) {
            struct reg_ctx ctx;
            const struct reg_def *reg_def = mod->reg_lookup(reg_name, &ctx);
            if (reg_def) {
                *ctx_out = ctx;
                return reg_def;
            }
        } else if (mod->reg_list) {
            struct reg_ctx ctx;
            const struct reg_def *reg_def = reg_lookup(mod->reg_list, mod->reg_count, reg_name, &ctx);
            if (reg_def) {
                *ctx_out = ctx;
                return reg_def;
            }
        }
    }
    return NULL;
}

const struct cmd_def *app_cmd_lookup(const char *cmd_name, struct cmd_ctx *ctx_out)
{
    const struct mod **beg = _mod_lst;
    const struct mod **end = _mod_lst + _mod_cnt;
    for (const struct mod **it = beg; it != end; it++) {
        const struct mod *mod = *it;        
        if (mod->cmd_lookup) {
            struct cmd_ctx ctx;
            const struct cmd_def *cmd_def = mod->cmd_lookup(mod, cmd_name, &ctx);
            if (cmd_def) {
                *ctx_out = ctx;
                return cmd_def;
            }
        } else if (mod->cmd_list) {
            struct cmd_ctx ctx;
            const struct cmd_def *cmd_def = cmd_lookup(mod->cmd_list, mod->cmd_count, cmd_name, &ctx);
            if (cmd_def) {
                *ctx_out = ctx;
                return cmd_def;
            }
        }
    }
    return NULL;
}
