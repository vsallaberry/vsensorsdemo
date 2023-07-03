/*
 * Copyright (C) 2017-2020 Vincent Sallaberry
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
#include "vlib/util.h"
#include "vlib/slist.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST LIST *************** */

void * test_list(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "LIST");
    log_t *         log = test != NULL ? test->log : NULL;
    slist_t *       list = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 7, 4, 1 };
    const size_t    intssz = sizeof(ints)/sizeof(*ints);
    long            prev;
    FILE *          out;

    /* insert_sorted */
    for (size_t i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "slist_insert_sorted(%d) not NULL",
            (list = slist_insert_sorted(list, (void*)((long)ints[i]), intcmp)) != NULL, ints[i]);
        if (log->level >= LOG_LVL_INFO) {
            out = log_getfile_locked(log);
            fprintf(out, "%02d> ", ints[i]);
            SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
            fputc('\n', out);
            funlockfile(out);
        }
    }
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz);

    /* prepend, append, remove */
    list = slist_prepend(list, (void*)((long)-2));
    list = slist_prepend(list, (void*)((long)0));
    list = slist_prepend(list, (void*)((long)-1));
    list = slist_append(list, (void*)((long)-4));
    list = slist_append(list, (void*)((long)20));
    list = slist_append(list, (void*)((long)15));
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz + 6);

    if (log->level >= LOG_LVL_INFO) {
        out = log_getfile_locked(log);
        fprintf(out, "after prepend/append:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
        fputc('\n', out);
        funlockfile(out);
    }
    /* we have -1, 0, -2, ..., 20, -4, 15, we will remove -1(head), then -2(in the middle),
     * then 15 (tail), then -4(in the middle), and the list should still be sorted */
    list = slist_remove(list, (void*)((long)-1), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-2), intcmp, NULL);
    list = slist_remove(list, (void*)((long)15), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-4), intcmp, NULL);
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz + 2);

    if (log->level >= LOG_LVL_INFO) {
        out = log_getfile_locked(log);
        fprintf(out, "after remove:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
        fputc('\n', out);
        funlockfile(out);
    }

    prev = 0;
    SLIST_FOREACH_DATA(list, a, long) {
        TEST_CHECK2(test, "elt(%ld) >= prev(%ld)", a >= prev, a, prev);
        prev = a;
    }
    slist_free(list, NULL);
    TEST_CHECK2(test, "last elt(%ld) is 20", prev == 20, prev);

    /* check SLIST_FOREACH_DATA MACRO */
    TEST_CHECK(test, "prepends", (list = slist_prepend(slist_prepend(NULL, (void*)((long)2)),
                                                      (void*)((long)1))) != NULL);
    TEST_CHECK(test, "list_length is 2", slist_length(list) == 2);
    prev = 0;
    SLIST_FOREACH_DATA(list, value, long) {
        LOG_INFO(log, "loop #%ld val=%ld", prev, value);
        if (prev == 1)
            break ;
        ++prev;
        if (prev < 5)
            continue ;
        prev *= 10;
    }
    TEST_CHECK(test, "SLIST_FOREACH_DATA break", prev == 1);
    slist_free(list, NULL);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

