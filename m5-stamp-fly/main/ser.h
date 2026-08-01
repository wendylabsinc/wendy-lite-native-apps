/*
 *  ser.h
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#ifndef _SER_H_
#define _SER_H_

#include "mod.h"
#include "cli.h"


/*** globals ***/

extern const struct cli_io ser_io;
extern const struct mod ser_mod;


/*** functions ***/

void set_init(void);
int  ser_getc(void);
void ser_write(const void *buf, int len);
void ser_putc(char c);


#endif
