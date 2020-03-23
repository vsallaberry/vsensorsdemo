/*
 * Copyright (C) 2020 Vincent Sallaberry
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

/* vsensors types */
enum FLAGS {
    FLAG_NONE               = 0,
    FLAG_FALLBACK_DISPLAY   = 1 << 0,
    FLAG_LOG_OPT_ERROR      = 1 << 1,
    FLAG_COLOR_OPT_ERROR    = 1 << 2
};

typedef struct {
    unsigned int    flags;
    vterm_flag_t    term_flags;
    logpool_t *     logs;
    unsigned long   timeout;
    unsigned long   sensors_timer;
    #ifdef _TEST
    unsigned int    test_mode;
    unsigned int    test_args_start;
    #endif
} options_t;

# ifdef __cplusplus__
extern "C" {
# endif

/** functions prototypes */

int             vsensors_log_loop(
                    options_t *         opts,
                    sensor_ctx_t *      sctx,
                    slist_t *           watchs,
                    log_t *             log,
                    FILE *              out);

int             vsensors_screen_loop(
                    options_t *         opts,
                    sensor_ctx_t *      sctx,
                    slist_t *           watchs,
                    log_t *             log,
                    FILE *              out);

# ifdef _TEST
int             test_describe_filter(
                    int                     short_opt,
                    const char *            arg,
                    int *                   i_argv,
                    const opt_config_t *    opt_config);

unsigned int    test_getmode(const char *arg);

int             test(
                    int                     argc,
                    const char *const*      argv,
                    unsigned int            test_mode,
                    logpool_t **            logpool);
# endif

# ifdef __cplusplus__
}
# endif

#endif /* ! VSENSORSDEMO_VSENSORS_H */


