/*
 * Copyright (C) 2020,2023 Vincent Sallaberry
 * vsensorsdemo <https://github.com/vsallaberry/vsensorsdemo>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 * Test program for libvsensors and vlib.
 */
#ifndef VSENSORSDEMO_VSENSORS_H
# define VSENSORSDEMO_VSENSORS_H

#include "vlib/options.h"
#include "vlib/logpool.h"
#include "vlib/term.h"
#include "vlib/slist.h"

/* vsensors types */
enum FLAGS {
    FLAG_NONE               = 0,
    FLAG_FALLBACK_DISPLAY   = 1 << 0,
    FLAG_SENSOR_PRINT       = 1 << 1,
    FLAG_SENSOR_LIST        = 1 << 2,
    FLAG_SB_ONLYWATCHED     = 1 << 3,
};

typedef struct {
    unsigned int    flags;
    logpool_t *     logs;
    char            version_string[512];
    unsigned long   timeout;
    unsigned long   sensors_timer;
    shlist_t        watchs;
    shlist_t        sb_watchs;
    shlist_t        writes;
    #ifdef _TEST
    unsigned int    test_mode;
    unsigned int    test_args_start;
    #endif
} options_t;

# ifdef __cplusplus
extern "C" {
# endif

/** functions prototypes */

int             vsensors_log_loop(
                    options_t *         opts,
                    sensor_ctx_t *      sctx,
                    log_t *             log,
                    FILE *              out);

int             vsensors_screen_loop(
                    options_t *         opts,
                    sensor_ctx_t *      sctx,
                    log_t *             log,
                    FILE *              out);

# ifdef __cplusplus
}
# endif

#endif /* ! VSENSORSDEMO_VSENSORS_H */


