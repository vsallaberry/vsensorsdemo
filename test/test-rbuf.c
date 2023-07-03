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
#include <stdlib.h>

#include "vlib/rbuf.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST STACK *************** */
enum {
    SPT_NONE        = 0,
    SPT_PRINT       = 1 << 0,
    SPT_REPUSH      = 1 << 1,
};
static int rbuf_pop_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags,
                         testgroup_t * test) {
    log_t * log = test != NULL ? test->log : NULL;
    size_t i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "rbuf_push(%d)", rbuf_push(rbuf, (void*)((long)ints[i])) == 0, ints[i]);
    }
    TEST_CHECK2(test, "rbuf_size is %zu", (i = rbuf_size(rbuf)) == intssz, intssz);

    i = intssz - 1;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long t = (long) rbuf_top(rbuf);
        long p = (long) rbuf_pop(rbuf);
        int ret;

        ret = (t == p && p == ints[i]);

        if ((flags & SPT_PRINT) != 0
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_top(%ld) = rbuf_pop(%ld) = %d", ret != 0, t, p, ints[i]);

        if (i == 0) {
            if (j == 1) {
                i = intssz / 2 - intssz % 2;
                j = 2;
            }else if ( j == 2 ) {
                ret = (rbuf_size(rbuf) == 0);

                if ((flags & SPT_PRINT) != 0
                && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK(test, "rbuf_size is 0", ret != 0);
            }
        } else {
            --i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", t);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                ret = (rbuf_push(rbuf, (void*)((long)ints[j])) == 0);

                if ((flags & SPT_PRINT) != 0
                &&  (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK2(test, "rbuf_push(%d)", ret != 0, ints[j]);
            }
            i = intssz - 1;
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);

    return 0;
}
static int rbuf_dequeue_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags,
                             testgroup_t * test) {
    log_t *         log = test != NULL ? test->log : NULL;
    size_t          i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "rbuf_push(%d)",
                    rbuf_push(rbuf, (void*)((long)ints[i])) == 0, ints[i]);
    }
    TEST_CHECK2(test, "rbuf_size = %zu, got %zu",
                (i = rbuf_size(rbuf)) == intssz, intssz, i);
    i = 0;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long b = (long) rbuf_bottom(rbuf);
        long d = (long) rbuf_dequeue(rbuf);
        int ret = (b == d && b == ints[i]);

        if ((ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel)))
        && (flags & SPT_PRINT) != 0) fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_bottom(%ld) = rbuf_dequeue(%ld) = %d",
                    ret != 0, b, d, ints[i]);

        if (i == intssz - 1) {
            ret = (rbuf_size(rbuf) == ((j == 1 ? 1 : 0) * intssz));

            if ((flags & SPT_PRINT) != 0
            && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                fputc('\n', log->out);

            TEST_CHECK(test, "end rbuf_size", ret != 0);
            j = 2;
            i = 0;
        } else {
            ++i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", b);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                ret = (rbuf_push(rbuf, (void*)((long)ints[j])) == 0);

                if ((flags & SPT_PRINT) != 0
                && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK2(test, "rbuf_push(%d)", ret != 0, ints[j]);
            }
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);
    TEST_CHECK(test, "all dequeued", i == 0);

    return 0;
}

void * test_rbuf(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "RBUF");
    log_t *         log = test != NULL ? test->log : NULL;
    rbuf_t *        rbuf;
    const int       ints[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    size_t          i, iter;
    int             ret;

    /* rbuf_pop 31 elts */
    LOG_INFO(log, "* rbuf_pop tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    TEST_CHECK(test, "rbuf_create", rbuf != NULL);

    for (iter = 0; iter < 10; iter++) {
        rbuf_pop_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, test);
    }
    for (iter = 0; iter < 10; iter++) {
        rbuf_pop_test(rbuf, ints, intssz,
                iter == 0 ? SPT_PRINT | SPT_REPUSH : SPT_REPUSH, test);
    }
    rbuf_free(rbuf);

    /* rbuf_dequeue 31 elts */
    LOG_INFO(log, "* rbuf_dequeue tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    TEST_CHECK(test, "rbuf_create(dequeue)", rbuf != NULL);

    for (iter = 0; iter < 10; iter++) {
        rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, test);
    }
    for (iter = 0; iter < 10; iter++) {
        rbuf_dequeue_test(rbuf, ints, intssz,
                       iter == 0 ? SPT_PRINT | SPT_REPUSH : SPT_REPUSH, test);
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));

    /* rbuf_dequeue 4001 elts */
    int tab[4001];
    size_t tabsz = sizeof(tab) / sizeof(tab[0]);
    LOG_INFO(log, "* rbuf_dequeue tests2 tab[%zu]", tabsz);
    for (i = 0; i < tabsz; i++) {
        tab[i] = i;
    }
    for (iter = 0; iter < 1000; iter++) {
        rbuf_dequeue_test(rbuf, tab, tabsz, SPT_NONE, test);
    }
    for (iter = 0; iter < 1000; iter++) {
        rbuf_dequeue_test(rbuf, tab, tabsz, SPT_REPUSH, test);
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf RBF_OVERWRITE, rbuf_get, rbuf_set */
    LOG_INFO(log, "* rbuf OVERWRITE tests");
    rbuf = rbuf_create(5, RBF_DEFAULT | RBF_OVERWRITE);
    TEST_CHECK(test, "rbuf_create(overwrite)", rbuf != NULL);

    for (i = 0; i < 3; i++) {
        TEST_CHECK0(test, "rbuf_push(%zu,overwrite)",
            (rbuf_push(rbuf, (void *)((long)((i % 5) + 1))) == 0), "rbuf_push==0", (i % 5) + 1);
    }
    i = 1;
    while(rbuf_size(rbuf) != 0) {
        size_t b = (size_t) rbuf_bottom(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, 0);
        size_t d = (size_t) rbuf_dequeue(rbuf);
        int ret = (b == i && g == i && d == i);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_get(%zd) = rbuf_top = rbuf_dequeue = %zu",
                ret != 0, i - 1, i);

        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", i);
        i++;
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    TEST_CHECK2(test, "3 elements dequeued, got %zd", i == 4, i - 1);

    for (i = 0; i < 25; i++) {
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "#%zu(%zu) ", i, (i % 5) + 1);

        ret = (rbuf_push(rbuf, (void *)((size_t) ((i % 5) + 1))) == 0);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_push(%zu,overwrite)", ret != 0, (i % 5) + 1);
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);

    TEST_CHECK2(test, "rbuf_size = 5, got %zu", (i = rbuf_size(rbuf)) == 5, i);
    TEST_CHECK(test, "rbuf_set(7,7) ko", rbuf_set(rbuf, 7, (void *) 7L) == -1);

    i = 5;
    while (rbuf_size(rbuf) != 0) {
        size_t t = (size_t) rbuf_top(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, i-1);
        size_t p = (size_t) rbuf_pop(rbuf);

        ret = (t == i && g == i && p == i);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_get(%zd) = rbuf_top = rbuf_pop = %zu", ret != 0, i - 1, i);

        i--;
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", p);
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    TEST_CHECK2(test, "5 elements treated, got %zd", i == 0, 5 - i);

    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf_set with RBF_OVERWRITE OFF */
    LOG_INFO(log, "* rbuf_set tests with increasing buffer");
    rbuf = rbuf_create(1, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    TEST_CHECK(test, "rbuf_create(increasing)", rbuf != NULL);
    TEST_CHECK2(test, "rbuf_size = 0, got %zu", (i = rbuf_size(rbuf)) == 0, i);

    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(1,1)", rbuf_set(rbuf, 1, (void *) 1) == 0);
        TEST_CHECK2(test, "rbuf_size = 2, got %zu", (i = rbuf_size(rbuf)) == 2, i);
    }
    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(3,3)", rbuf_set(rbuf, 3, (void *) 3) == 0);
        TEST_CHECK2(test, "rbuf_size = 4, got %zu", (i = rbuf_size(rbuf)) == 4, i);
    }
    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(5000,5000)", rbuf_set(rbuf, 5000, (void*) 5000) == 0);
        TEST_CHECK2(test, "rbuf_size = 5001, got %zu", (i = rbuf_size(rbuf)) == 5001, i);
    }
    TEST_CHECK(test, "rbuf_reset", rbuf_reset(rbuf) == 0);

    for (iter = 1; iter <= 5000; iter++) {
        TEST_CHECK2(test, "rbuf_set(%zu)", rbuf_set(rbuf, iter-1, (void*) iter) == 0, iter);
        TEST_CHECK2(test, "rbuf_size = %zu, got %zu", (i = rbuf_size(rbuf)) == iter, iter, i);
    }
    for (iter = 0; rbuf_size(rbuf) != 0; iter++) {
        size_t g = (size_t) rbuf_get(rbuf, 5000-iter-1);
        size_t t = (size_t) rbuf_top(rbuf);
        size_t p = (size_t) rbuf_pop(rbuf);
        i = rbuf_size(rbuf);
        TEST_CHECK2(test, "top(%zu) = get(#%zd=%zu) = pop(%zu) = %zd "
                          "and rbuf_size(%zu) = %zd",
            (t == p && g == p && p == 5000-iter && i == 5000-iter-1),
            t, 5000-iter-1, g, p, 5000-iter, i, 5000-iter-1);
    }
    TEST_CHECK2(test, "5000 elements treated, got %zu", iter == 5000, iter);

    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

