/*
 *  cmd.c
 *
 *  Copyright (c) 2019 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include <stdio.h>
#include "cmd.h"


void cmd_help(const struct cmd_def *cmd_list, int cmd_count, int indent)
{
    int col1 = 0;
    for (int i=0; i<cmd_count; i++) {
        int name_len = (int)strlen(cmd_list[i].name);
        if (name_len > col1)
            col1= name_len;
    }
    for (int i=0; i<cmd_count; i++) {
        int name_len = (int)strlen(cmd_list[i].name);
        const char *help = cmd_list[i].help ? cmd_list[i].help : "";
        if (cmd_list[i].usage) {
            printf("%s%s%s %s\n", mod_spaces(indent), cmd_list[i].name, mod_spaces(col1 - name_len), cmd_list[i].usage);
            printf("%s %s\n", mod_spaces(indent + col1 - name_len), help);
        } else {
            printf("%s%s%s %s\n", mod_spaces(indent), cmd_list[i].name, mod_spaces(col1 - name_len), help);
        }
    }
}
