/*
 * Copyright (C) 2017-2020,2023 Vincent Sallaberry
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
 * + The testing part was firstly in main.c. To see previous history of
 * vsensorsdemo tests, look at main.c history (git log -r eb571ec4a src/main.c).
 * + after e21034ae04cd0674b15a811d2c3cfcc5e71ddb7f, test was moved
 *   from src/test.c to test/test.c.
 * + use 'git log --name-status --follow HEAD -- src/test.c' (or test/test.c)
 */
/* ** TESTS ***********************************************************************************/
#ifndef _TEST
extern int ___nothing___; /* empty */
#else
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

#include "libvsensors/sensor.h"

#include "version.h"
#include "test_private.h"

/* *************** SIZEOF information ********************** */
#define PSIZEOF(type, log)              \
            LOG_INFO(log, "%32s: %zu", #type, sizeof(type))
#define POFFSETOF(type, member, log)    \
            LOG_INFO(log, "%32s: %zu", "off(" #type "." #member ")", offsetof(type, member))


#define TYPESTR2(type) #type
#define TYPESTR(type) TYPESTR2(type)
#define TYPEMINSTR(type) TYPESTR(type##_MIN)
#define TYPEMAXSTR(type) TYPESTR(type##_MAX)
#define PMINMAX(type, log)              \
            do { \
                LOG_INFO(log, "%32s: min:%30s", #type, TYPEMINSTR(type)); \
                LOG_INFO(log, "%32s: max:%30s", #type, TYPEMAXSTR(type)); \
            } while(0);

union testlongdouble_u {
    double d;
    long double ld;
};
/* compiler extends struct to 16 bytes instead of 8 because of
 * struct boundary on long double (16 bytes) */
struct testlongdouble_s {
    union {
        long double ld;
        double d;
    } u;
    long int a;
};
struct testlongdoublepacked_s {
    char type;
    union {
        double d;
        long double ld __attribute__((__packed__));
    } u;
};

void * test_sizeof(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "SIZE_OF");
    log_t *         log = test != NULL ? test->log : NULL;

    PSIZEOF(char, log);
    PSIZEOF(unsigned char, log);
    PSIZEOF(short, log);
    PSIZEOF(unsigned short, log);
    PSIZEOF(int, log);
    PSIZEOF(unsigned int, log);
    PSIZEOF(long, log);
    PSIZEOF(unsigned long, log);
    PSIZEOF(long long, log);
    PSIZEOF(unsigned long long, log);
    PSIZEOF(uint16_t, log);
    PSIZEOF(int16_t, log);
    PSIZEOF(uint32_t, log);
    PSIZEOF(int32_t, log);
    PSIZEOF(uint64_t, log);
    PSIZEOF(int64_t, log);
    PSIZEOF(intmax_t, log);
    PSIZEOF(uintmax_t, log);
    PSIZEOF(size_t, log);
    PSIZEOF(time_t, log);
    PSIZEOF(float, log);
    PSIZEOF(double, log);
    PSIZEOF(long double, log);
    PSIZEOF(union testlongdouble_u, log);
    PSIZEOF(struct testlongdouble_s, log);
    PSIZEOF(struct testlongdoublepacked_s, log);
    PSIZEOF(char *, log);
    PSIZEOF(unsigned char *, log);
    PSIZEOF(void *, log);
    PSIZEOF(struct timeval, log);
    PSIZEOF(struct timespec, log);
    PSIZEOF(fd_set, log);
    PSIZEOF(log_t, log);
    PSIZEOF(avltree_t, log);
    PSIZEOF(avltree_node_t, log);
    PSIZEOF(avltree_visit_context_t, log);
    PSIZEOF(vterm_screen_ev_data_t, log);
    PSIZEOF(sensor_watch_t, log);
    PSIZEOF(sensor_value_t, log);
    PSIZEOF(sensor_sample_t, log);
    POFFSETOF(avltree_node_t, left, log);
    POFFSETOF(avltree_node_t, right, log);
    POFFSETOF(avltree_node_t, data, log);
    POFFSETOF(avltree_node_t, balance, log);
    PMINMAX(INT, log);
    PMINMAX(UINT, log);
    PMINMAX(INT64, log);
    PMINMAX(UINT64, log);
    PMINMAX(INTMAX, log);
    PMINMAX(UINTMAX, log);

    avltree_node_t * node, * nodes[1000];
    unsigned char a = CHAR_MAX;
    for (unsigned i = 0; i < 1000; i++) {
        unsigned char b;
        node = malloc(sizeof(avltree_node_t));
        for (b = 0; b < 16; b++)
            if ((((unsigned long)node) & (1 << b)) != 0) break ;
        nodes[i] = node;
        if (b < a) a = b;
    }
    LOG_INFO(log, "alignment of node: %u bytes", a + 1);
    TEST_CHECK(test, "align > 1", a + 1 > 1);
    for (unsigned i = 0; i < 1000; i++) {
        if (nodes[i]) free(nodes[i]);
    }

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

