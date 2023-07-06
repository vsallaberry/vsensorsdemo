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
#ifndef VSENSORSDEMO_TEST_TEST_PRIVATE_H
#define VSENSORSDEMO_TEST_TEST_PRIVATE_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <fnmatch.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <math.h>

#include "vlib/hash.h"
#include "vlib/util.h"
#include "vlib/options.h"
#include "vlib/time.h"
#include "vlib/account.h"
#include "vlib/thread.h"
#include "vlib/rbuf.h"
#include "vlib/avltree.h"
#include "vlib/logpool.h"
#include "vlib/term.h"
#include "vlib/job.h"
#include "vlib/test.h"

#include "test/test.h"

#define VOIDP(n)    ((void *) ((long) (n)))

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    logpool_t *     logs;
    testpool_t *    testpool;
    FILE *          out;
    int             columns;
    pthread_t       main;
    int             argc;
    const char*const* argv;
} options_test_t;

/** strings corresponding of unit tests ids , for command line, in same order as s_testmode_str */
enum testmode_t {
    TEST_all = 0,
    TEST_options,
    TEST_tests,
    TEST_optusage,
    TEST_sizeof,
    TEST_ascii,
    TEST_color,
    TEST_bench,
    TEST_math,
    TEST_list,
    TEST_hash,
    TEST_rbuf,
    TEST_tree,
    TEST_sensorvalue,
    TEST_sensorplugin,
    TEST_account,
    TEST_bufdecode,
    TEST_srcfilter,
    TEST_logpool,
    TEST_job,
    TEST_vthread,
    TEST_log,
    /* starting from here, tests are not included in 'all' by default */
    TEST_excluded_from_all,
    TEST_bigtree = TEST_excluded_from_all,
    TEST_optusage_big,
    TEST_optusage_stdout,
    TEST_logpool_big,
    TEST_PARALLEL,
    TEST_NB /* Must be LAST ! */
};
#define TEST_MASK(id)       (1UL << ((unsigned int) (id)))
#define TEST_MASK_ALL       ((1UL << (sizeof(unsigned int) * 8)) - 1)

/* ********************************************************************/
# ifdef __cplusplus__
extern "C" {
# endif

const char *    test_version_string();

int             intcmp(const void * a, const void *b);

const char *    test_tmpdir();
int             test_clean_tmpdir();

# ifdef __cplusplus__
}
# endif
/* ********************************************************************/


#endif
