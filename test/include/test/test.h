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
 * tests for vsensorsdemo, libvsensors, vlib.
 * The testing part was previously in main.c. To see previous history of
 * vsensorsdemo tests, look at main.c history (git log -r eb571ec4a src/main.c).
 */
/* ** TESTS ***********************************************************************************/
#ifndef VSENSORSDEMO_TEST_TEST_H
#define VSENSORSDEMO_TEST_TEST_H

#include <stdio.h>

#include "vlib/options.h"
#include "vlib/logpool.h"
#include "vlib/test.h"

/* ********************************************************************/
# ifdef __cplusplus
extern "C" {
# endif

/** */
int             test_vsensorsdemo_get_source(
                    FILE *                  out,
                    char *                  buffer,
                    unsigned int            buffer_size,
                    void **                 ctx);

/** */
int             test_describe_filter(
                    int                     short_opt,
                    const char *            arg,
                    int *                   i_argv,
                    const opt_config_t *    opt_config);

/** */
unsigned long   test_getmode(const char *arg);

/** */
int             test(
                    int                     argc,
                    const char *const*      argv,
                    unsigned long           test_mode,
                    logpool_t **            logpool);

# ifdef __cplusplus
}
# endif
/* ********************************************************************/

#endif
