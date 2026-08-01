/*
 *  cli.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _CLI_H_
#define _CLI_H_

#include <stdbool.h>


/*** types ***/

struct cli_io {
    int  (* read_char)(void);
    void (* write_char)(char c);
    int  (* read_buf)(const void *buf, int max);
    void (* write_buf)(const void *buf, int len);
};


/*** globals ***/

extern const struct mod cli_mod;


/*** functions ***/

void cli_loop(void);
void cli_set_io(const struct cli_io *io);
void cli_add_esc_handler(void (* handler)(void));
void cli_remove_esc_handler(void (* handler)(void));
void cli_add_fast_esc_handler(void (* handler)(void));
void cli_remove_fast_esc_handler(void (* handler)(void));
void cli_fire_esc_handlers(void);
int cli_get_line(char *buf, int max);
void cli_expect_response(char prefix);
bool cli_is_response_expected(void);
void cli_prefix_response(void);
void cli_respond_nothing(void);


#endif
