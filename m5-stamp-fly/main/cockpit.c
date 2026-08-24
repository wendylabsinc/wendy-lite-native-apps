/*
 *  cockpit.c
 *
 *  Copyright (c) 2026 Gabriele Mondada.
 *  This software is distributed under the terms of the MIT license.
 *  See https://opensource.org/licenses/MIT
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gmutil.h"
#include "cockpit.h"
#include "abc_app.h"
#include "abc_cli.h"
#include "abc_con.h"
#include "abc_core.h"
#include "ctrl.h"
#include "abc_gen.h"


/*** globals ***/

const struct mod *mods[] = {
    &con_mod,
    &core_mod,
    &ctrl_mod,
    &gen_mod,
};

const struct app app = {
    .name = "fly",
    .version = "1.0.0",
    .module_list = mods,
    .module_count = GMU_ARRAY_LEN(mods),
};


/*** functions ***/

void cockpit_main(void)
{
    cli_set_io(&con_io);
    app_init(&app);
    app_start();
    for (;;) {
        app_cycle();
        // yield so IDLE0 runs: the task WDT panics after 5s otherwise
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void cockpit_task(void *arg)
{
    cockpit_main();
}

void cockpit_init(void)
{
    xTaskCreatePinnedToCore(cockpit_task, "cockpit", 8192, NULL, 2, NULL, 0);
}
