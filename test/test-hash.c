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
#include "vlib/hash.h"
#include "vlib/test.h"
#include "vlib/options.h"
#include "vlib/time.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST HASH *************** */

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS("TEST-" BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2020")

static int hash_print_stats(hash_t * hash, FILE * file) {
    int n = 0;
    int tmp;
    hash_stats_t stats;

    if (hash_stats_get(hash, &stats) != HASH_SUCCESS) {
        fprintf(file, "error, cannot get hash stats\n");
        return -1;
    }
    if ((tmp =
        fprintf(file, "Hash %p\n"
                       " size       : %u\n"
                       " flags      : %08x\n"
                       " elements   : %u\n"
                       " indexes    : %u\n"
                       " index_coll : %u\n"
                       " collisions : %u\n",
                       (void*) hash,
                       stats.hash_size, stats.hash_flags,
                       stats.n_elements, stats.n_indexes,
                       stats.n_indexes_with_collision, stats.n_collisions)) < 0) {
        return -1;
    }
    n += tmp;
    return n;
}

static void test_one_hash_insert(
                    hash_t *hash, const char * str,
                    const options_test_t * opts, testgroup_t * test) {
    log_t * log = test != NULL ? test->log : NULL;
    int     ins;
    char *  fnd;
    (void) opts;

    ins = hash_insert(hash, (void *) str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, " hash_insert [%s]: %d, ", str, ins);
    }

    fnd = (char *) hash_find(hash, str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, "find: [%s]\n", fnd);
    }

    TEST_CHECK2(test, "insert [%s] is %d, got %d", ins == HASH_SUCCESS, str, HASH_SUCCESS, ins);

    TEST_CHECK2(test, "hash_find(%s) is %s, got %s", fnd != NULL && strcmp(fnd, str) == 0,
                str, str, STR_CHECKNULL(fnd));
}

void * test_hash(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *               test = TEST_START(opts->testpool, "HASH");
    log_t *                     log = test != NULL ? test->log : NULL;
    hash_t *                    hash;
    static const char * const   hash_strs[] = {
        VERSION_STRING, "a", "z", "ab", "ac", "cxz", "trz", NULL
    };

    hash = hash_alloc(HASH_DEFAULT_SIZE, 0, hash_ptr, hash_ptrcmp, NULL);
    TEST_CHECK(test, "hash_alloc not NULL", hash != NULL);
    if (hash != NULL) {
        if (log->level >= LOG_LVL_INFO) {
            fprintf(log->out, "hash_ptr: %08x\n", hash_ptr(hash, opts));
            fprintf(log->out, "hash_ptr: %08x\n", hash_ptr(hash, hash));
            fprintf(log->out, "hash_str: %08x\n", hash_str(hash, log->out));
            fprintf(log->out, "hash_str: %08x\n", hash_str(hash, VERSION_STRING));
        }
        hash_free(hash);
    }

    for (unsigned int hash_size = 1; hash_size < 200; hash_size += 100) {
        TEST_CHECK2(test, "hash_alloc(sz=%u) not NULL",
                (hash = hash_alloc(hash_size, 0, hash_str, (hash_cmp_fun_t) strcmp, NULL)) != NULL,
                hash_size);
        if (hash == NULL) {
            continue ;
        }

        for (const char *const* strs = hash_strs; *strs; strs++) {
            test_one_hash_insert(hash, *strs, opts, test);
        }

        if (log->level >= LOG_LVL_INFO) {
            TEST_CHECK(test, "hash_print_stats OK", hash_print_stats(hash, log->out) > 0);
        }
        hash_free(hash);
    }

    if ((opts->test_mode & TEST_MASK(TEST_bighash)) != 0) {
        unsigned int hash_sizes[] = { 1000, 500000, 1000000 };
        srand(time(NULL));
        for (unsigned int i = 0; i < PTR_COUNT(hash_sizes); ++i) {
            const size_t nb = 10*1000*1000;
            unsigned int hash_size = hash_sizes[i];
            BENCHS_DECL(tm_bench, cpu_bench);
            TEST_CHECK2(test, "big hash_alloc(sz=%u, nb=%zu) not NULL",
                        (hash = hash_alloc(hash_size, HASH_FLAG_DOUBLES, hash_ptr, hash_ptrcmp, NULL)) != NULL,
                         hash_size, nb);

            BENCHS_START(tm_bench, cpu_bench);
            for (size_t i = 0; i < nb; ++i) {
                void * value = (void*)(((size_t)rand()) % (nb * 10UL));
                if (hash_insert(hash, value) != 0) {
                    TEST_CHECK2(test, "error big hash_insert(sz=%u,nb=%zu,elt=%lx)", 0, hash_size, nb, (unsigned long) value);
                }
            }
            BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "big hash_inserts(sz=%u,nb=%zu)", hash_size, nb);

            if (log->level >= LOG_LVL_VERBOSE) {
                TEST_CHECK(test, "hash_print_stats OK", hash_print_stats(hash, log->out) > 0);
            }

            BENCHS_START(tm_bench, cpu_bench);
            hash_free(hash);
            BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "big hash_free(sz=%u, nb=%zu)", hash_size, nb);
        }
    }

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

