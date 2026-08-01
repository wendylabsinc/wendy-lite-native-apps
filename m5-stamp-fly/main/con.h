/*
 *  con.h
 *
 *  Copyright (c) 2026 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _CON_H_
#define _CON_H_

#include "mod.h"
#include "cli.h"


/*** globals ***/

extern const struct cli_io con_io;
extern const struct mod con_mod;


/*** functions ***/

void con_init(void);
int  con_getc(void);
void con_write(const void *buf, int len);
void con_putc(char c);


#endif
