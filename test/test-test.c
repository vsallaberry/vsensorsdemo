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

#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* ************************************************************************ */
/* TEST TESTS */
/* ************************************************************************ */
void * test_tests(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    /* except this 'fake' global test, tests here are done manually (no bootstrap) */
    testgroup_t *   globaltest  = TEST_START(opts->testpool, "TEST");
    log_t *         log         = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned long   nerrors     = 0;
    unsigned long   n_tests     = 0;
    unsigned long   n_ok        = 0;
    unsigned long   testend_ret;

    /* Hi, this is the only test where checks must be done manually
     * as it is checking the vlib testpool framework.
     * In all other tests, the vlib testpool framework should be used. */

    LOG_INFO(log, ">>> TEST tests (manual)");

    /* create test testpool */
    testpool_t *    tests = tests_create(opts->logs, TPF_DEFAULT);
    testgroup_t *   test;
    unsigned int    old_flags = INT_MAX;
    log_level_t     old_level = INT_MAX;
    unsigned int    expected_err;

    /*** TESTS01 ***/
    test = TEST_START(tests, "TEST01");
    expected_err = 1;

    if (test == NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR: TEST_START(TEST01) returned NULL");
    } else {
        ++n_ok;
        old_flags = test->log->flags;
        old_level = test->log->level;
        test->log->flags &= ~LOG_FLAG_COLOR;
        if (log->level < LOG_LVL_INFO) test->log->level = 0;
    }
    ++n_tests;

    TEST_CHECK(test, "CHECK01", 1 == 1);
    TEST_CHECK(test, "CHECK02", (errno = EAGAIN) && 1 == 0);

    testend_ret = TEST_END2(test, "%s", test && test->n_errors == expected_err
                                         ? " AS EXPECTED" : "");
    if (test != NULL) { test->log->flags = old_flags; test->log->level = old_level; }

    if (test != NULL
    && (test->n_errors != expected_err || test->n_tests != 2 || test->n_errors != testend_ret
        || test->n_ok != (test->n_tests - test->n_errors)
        || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST01 test_end/n_tests/n_errors/n_ok counters mismatch");
    } else ++n_ok;
    ++n_tests;

    if (test != NULL) {
        n_ok += test->n_ok + test->n_ok; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    /*** TESTS02 ***/
    test = TEST_START(tests, "TEST02");
    expected_err = 2;

    if (test == NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR: TEST_START(TEST02) returned NULL");
    } else {
        ++n_ok;
        old_flags = test->log->flags;
        old_level = test->log->level;
        test->log->flags &= ~LOG_FLAG_COLOR;
        test->flags |= TPF_STORE_RESULTS | TPF_BENCH_RESULTS;
        if (log->level < LOG_LVL_INFO) test->log->level = 0;
    }
    ++n_tests;

    TEST_CHECK(test, "CHECK01", 1 == 1);
    TEST_CHECK(test, "CHECK02", 1 == 0);
    if (test != NULL) test->ok_loglevel = LOG_LVL_INFO;
    TEST_CHECK2(test, "CHECK03 checking %s", 1 == 0, "something");
    TEST_CHECK2(test, "CHECK04 checking %s", (errno = ENOMEM) && 1 == 1 + 2*7 - 14,
                      "something else");
    TEST_CHECK(test, "CHECK05 usleep", usleep(12345) == 0);
    TEST_CHECK(test, "CHECK06", 2 == 2);
    testend_ret = TEST_END2(test, "%s", test && test->n_errors == expected_err
                                         ? " AS EXPECTED" : "");
    if (test != NULL) { test->log->flags = old_flags; test->log->level = old_level; }

    if (test != NULL
    &&  (test->n_errors != expected_err || test->n_tests != 6 || test->n_errors != testend_ret
         || test->n_ok != (test->n_tests - test->n_errors)
         || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST02 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;
    ++n_tests;
    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    expected_err = 0;
    test = TEST_START(tests, "TEST03");

    ++n_tests;
    if (test != NULL
    &&  (test->n_errors != 0 || test->n_tests != 0
         || test->n_ok != (test->n_tests - test->n_errors)
         || (test->flags & TPF_FINISHED) != 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST03 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;

    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
        LOG_INFO(test->log, NULL);
        /* release logpool log to have clean counters (because TEST_END() not called) */
        ++n_tests;
        if (logpool_release(opts->logs, test->log) < 0
        && (errno != EACCES || (test->flags & TPF_LOGTRUEPREFIX) != 0)) {
            ++nerrors;
            LOG_ERROR(log, "error: logpool_release(%s): <0: %s flg:%x",
                      test->name, strerror(errno),test->flags);
        } else ++n_ok;
    }

    expected_err = 0;
    test = TEST_START(tests, "TEST04");
    testend_ret = TEST_END(test);

    if (test != NULL
    &&  (test->n_errors != 0 || test->n_tests != 0
         || test->n_ok != (test->n_tests - test->n_errors) || testend_ret != test->n_errors
         || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST04 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;
    ++n_tests;
    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    ++n_tests;
    log_t newlog;
    log_t * oldlog = log_set_vlib_instance(&newlog);
    if (oldlog != NULL) {
        newlog = *oldlog;
        newlog.flags &= ~(LOG_FLAG_COLOR);
        if (log->level < LOG_LVL_INFO) newlog.level = 0;
        else newlog.level = LOG_LVL_INFO;
    }
    ++n_tests;
    if (TEST_END(test) != testend_ret) {
        ++nerrors;
    } else ++n_ok;

    test = NULL;
    if (TEST_START((testpool_t*)(NULL), "TEST05") != NULL
    ||  TEST_START((testpool_t*)(test), "TEST05") != NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_START(NULL) did not return NULL");
    } else ++n_ok;
    ++n_tests;
    if (TEST_END(test) == 0) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_END(NULL) returned 0");
    } else ++n_ok;
    TEST_CHECK(test, "CHECK01: TEST_CHECK with NULL test", (expected_err = 98) && (1 == 1));
    ++n_tests;
    if (expected_err != 98) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK(test, "CHECK02: TEST_CHECK with NULL test", (++expected_err) && 1 == 2);
    ++n_tests;
    if (expected_err != 99) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK2(test, "CHECK03: TEST_CHECK2 with NULL %s", (++expected_err) && 1 == 1, "test");
    ++n_tests;
    if (expected_err != 100) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK2(test, "CHECK04: TEST_CHECK2 with NULL T%s", (++expected_err) && 1 == 2, "est");
    ++n_tests;
    if (expected_err != 101) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    LOG_INFO(log, NULL);
    log_set_vlib_instance(oldlog);

    tests_print(tests, TPR_DEFAULT);

    if (tests_free(tests) != 0) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, test_free() error");
    } else ++n_ok;
    ++n_tests;

    LOG_INFO(log, "<- %s(): ending with %lu error%s (manual).\n",
             __func__, nerrors, nerrors > 1 ? "s" : "");

    if (globaltest != NULL) {
        globaltest->n_errors += nerrors;
        globaltest->n_tests += n_tests;
        globaltest->n_ok += n_ok;
    }
    logpool_release(opts->logs, log); /* necessary because logpool_getlog() called here */
    return VOIDP(TEST_END(globaltest) + (globaltest == NULL ? nerrors : 0));
}

#endif /* ! ifdef _TEST */

