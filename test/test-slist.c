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

# define WR_SLIST_DATA_TYPE long

static slist_t * wr_slist_insert_sorted_sized(slist_t * l, void * d, int(*cmpfun)(const void *, const void *)) {
    WR_SLIST_DATA_TYPE value = (WR_SLIST_DATA_TYPE) ((size_t) d);
    return slist_insert_sorted_sized(l, &value, sizeof(WR_SLIST_DATA_TYPE), cmpfun);
}

static slist_t * wr_slist_prepend_sized(slist_t * l, void * d) {
    WR_SLIST_DATA_TYPE value = (WR_SLIST_DATA_TYPE) ((size_t) d);
    return slist_prepend_sized(l, &value, sizeof(WR_SLIST_DATA_TYPE));
}

static slist_t * wr_slist_append_sized(slist_t * l, void * d) {
    WR_SLIST_DATA_TYPE value = (WR_SLIST_DATA_TYPE) ((size_t) d);
    return slist_append_sized(l, &value, sizeof(WR_SLIST_DATA_TYPE));
}

static slist_t * wr_slist_remove_sized(slist_t * l, const void * d, int(*cmpfun)(const void *, const void *), void(*freefun)(void*)) {
    WR_SLIST_DATA_TYPE value = (WR_SLIST_DATA_TYPE) ((size_t) d);
    return slist_remove_sized(l, &value, cmpfun, freefun);
}

static int wr_intcmp_sized(const void * a, const void * b) {
    return intcmp((void*)(*((WR_SLIST_DATA_TYPE*)a)), (void*)(*((WR_SLIST_DATA_TYPE*)b)));
}

void * test_list(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "LIST");
    log_t *         log = test != NULL ? test->log : NULL;
    slist_t *       list;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 7, 4, 1 };
    const size_t    intssz = sizeof(ints)/sizeof(*ints);
    long            prev;
    FILE *          out;

    slist_t * (*wr_slist_insert_sorted)(slist_t *, void *, int(*)(const void *, const void *))
        = slist_insert_sorted;
    slist_t * (*wr_slist_prepend)(slist_t *, void *)
        = slist_prepend;
    slist_t * (*wr_slist_append)(slist_t *, void *)
        = slist_append;
    slist_t * (*wr_slist_remove)(slist_t *, const void *, int(*)(const void *, const void *), void(*)(void*))
        = slist_remove;
    slist_cmp_fun_t wr_cmpfun = intcmp;

    for (int testid = 0; testid < 2; ++testid) {
        LOG_INFO(log, "slist%s tests", testid == 0 ? "" : "_sized");
        list = NULL;

        /* insert_sorted */
        for (size_t i = 0; i < intssz; i++) {
            TEST_CHECK2(test, "%d. slist_insert_sorted(%d) not NULL",
                (list = wr_slist_insert_sorted(list, (void*)((long)ints[i]), wr_cmpfun)) != NULL, testid, ints[i]);
            if (log->level >= LOG_LVL_INFO) {
                out = log_getfile_locked(log);
                fprintf(out, "%02d> ", ints[i]);
                switch(testid) {
                    case 0:  SLIST_FOREACH_DATA (list, a, long) fprintf(out, "%ld ", a); break ;
                    default: SLIST_FOREACH_PDATA(list, pa, long *) fprintf(out, "%ld ", *pa); break ;
                }
                fputc('\n', out);
                funlockfile(out);
            }
        }
        TEST_CHECK2(test, "%d. slist_length", slist_length(list) == intssz, testid);

        /* prepend, append, remove */
        list = wr_slist_prepend(list, (void*)((long)-2));
        list = wr_slist_prepend(list, (void*)((long)0));
        list = wr_slist_prepend(list, (void*)((long)-1));
        list = wr_slist_append(list, (void*)((long)-4));
        list = wr_slist_append(list, (void*)((long)20));
        list = wr_slist_append(list, (void*)((long)15));
        TEST_CHECK2(test, "%d. slist_length", slist_length(list) == intssz + 6, testid);

        if (log->level >= LOG_LVL_INFO) {
            out = log_getfile_locked(log);
            fprintf(out, "after prepend/append:");
            switch(testid) {
                case 0:  SLIST_FOREACH_DATA (list, a, long) fprintf(out, "%ld ", a); break ;
                default: SLIST_FOREACH_PDATA(list, pa, long *) fprintf(out, "%ld ", *pa); break ;
            }
            fputc('\n', out);
            funlockfile(out);
        }
        /* we have -1, 0, -2, ..., 20, -4, 15, we will remove -1(head), then -2(in the middle),
         * then 15 (tail), then -4(in the middle), and the list should still be sorted */
        list = wr_slist_remove(list, (void*)((long)-1), wr_cmpfun, NULL);
        list = wr_slist_remove(list, (void*)((long)-2), wr_cmpfun, NULL);
        list = wr_slist_remove(list, (void*)((long)15), wr_cmpfun, NULL);
        list = wr_slist_remove(list, (void*)((long)-4), wr_cmpfun, NULL);
        TEST_CHECK2(test, "%d. slist_length", slist_length(list) == intssz + 2, testid);

        if (log->level >= LOG_LVL_INFO) {
            out = log_getfile_locked(log);
            fprintf(out, "after remove:");
            switch(testid) {
                case 0:  SLIST_FOREACH_DATA (list, a, long) fprintf(out, "%ld ", a); break ;
                default: SLIST_FOREACH_PDATA(list, pa, long *) fprintf(out, "%ld ", *pa); break ;
            }
            fputc('\n', out);
            funlockfile(out);
        }

        prev = 0;
        SLIST_FOREACH_ELT(list, elt) {
            long *p, a = (testid == 0 ? (long) SLIST_DATA(elt) : *(p=SLIST_PDATA(elt)));
            TEST_CHECK2(test, "%d. elt(%ld) >= prev(%ld)", a >= prev, testid, a, prev);
            prev = a;
        }
        slist_free(list, NULL);
        TEST_CHECK2(test, "%d. last elt(%ld) is 20", prev == 20, testid, prev);

        /* check SLIST_FOREACH_DATA MACRO */
        TEST_CHECK2(test, "%d. prepends", (list = wr_slist_prepend(
                                     wr_slist_prepend(NULL, (void*)((long)2)),(void*)((long)1))) != NULL, testid);
        TEST_CHECK2(test, "%d. list_length is 2", slist_length(list) == 2, testid);
        prev = 0;
        // must duplicate loop in order to test loop break
        if (testid == 0) {
        SLIST_FOREACH_DATA(list, value, long) {
            LOG_INFO(log, "%d. loop #%ld val=%ld", testid, prev, value);
            if (prev == 1)
                break ;
            ++prev;
            if (prev < 5)
                continue ;
            prev *= 10;
        }
        } else {
        SLIST_FOREACH_PDATA(list, pvalue, long *) {
            LOG_INFO(log, "%d. loop #%ld val=%ld", testid, prev, *pvalue);
            if (prev == 1)
                break ;
            ++prev;
            if (prev < 5)
                continue ;
            prev *= 10;
        }
        }
        TEST_CHECK2(test, "%d. SLIST_FOREACH_DATA break", prev == 1, testid);
        slist_free(list, NULL);

        wr_slist_insert_sorted = wr_slist_insert_sorted_sized;
        wr_slist_prepend = wr_slist_prepend_sized;
        wr_slist_append = wr_slist_append_sized;
        wr_slist_remove = wr_slist_remove_sized;
        wr_cmpfun = wr_intcmp_sized;
    }

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

