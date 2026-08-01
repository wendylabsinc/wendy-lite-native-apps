/*
 *  mod.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include "mod.h"


/**
 * Initialize an arg iterator used to parse the given command line.
 * After initialization, the user has to call mod_arg_iterator_next() for
 * each argument, including the first.
 */
void mod_arg_iterator_init(struct mod_arg_iterator *arg_iterator, char *cmd_line)
{
    arg_iterator->p = cmd_line;
    arg_iterator->name = NULL;
    arg_iterator->sep = 0;
}

/**
 * Move the argument iterator to the next argument.
 */
const char *mod_arg_iterator_next(struct mod_arg_iterator *arg_iterator)
{
    char *p = arg_iterator->p;

    // skip empty leading chars
    for (;;) {
        uint8_t c = *p;

        if (c == 0) {
            arg_iterator->name = NULL;
            arg_iterator->sep = 0;
            goto end;
        }
        if (c > 32)
            break;

        p++;
    }

    // this is the beginning of the arg
    arg_iterator->name = p;

    // search end of arg
    for (;;) {
        uint8_t c = *p;

        if (c == 0) {
            arg_iterator->sep = 0;
            goto end;
        }

        if (c == '=' || c == '^' || c == '$') {
            *p = 0;
            p++;
            arg_iterator->sep = c;
            goto end;
        }

        if (c > 0 && c <= 32) {
            *p = 0;
            p++;

            // search for a separator other than a space
            for (;;) {
                c = *p;
                if (c == '=' || c == '^' || c == '$') {
                    p++;
                    arg_iterator->sep = c;
                    goto end;
                }
                if (c <= 0 || c > 32) {
                    arg_iterator->sep = 0;
                    goto end;
                }
                p++;
            }
        }

        p++;
    }

end:
    arg_iterator->p = p;
    return arg_iterator->name;
}

/**
 * Return a string with the given number of spaces. The returned string is static
 * and its length cannot exceed 64 spaces.
 */
const char *mod_spaces(int space_count)
{
    static const char spaces[] = {
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        0
    };
    if (space_count > sizeof(spaces) - 1)
        space_count = sizeof(spaces) - 1;
    return spaces + sizeof(spaces) - 1 - space_count;
}
