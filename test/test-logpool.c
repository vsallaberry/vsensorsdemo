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
#include <sys/stat.h>
#include <fcntl.h>

#include "vlib/logpool.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST LOG POOL *************** */
#define POOL_LOGGER_PREF        "pool-logger"
#define POOL_LOGGER_ALL_PREF    "pool-logger-all"

typedef struct {
    logpool_t *             logpool;
    char                    fileprefix[PATH_MAX];
    volatile sig_atomic_t   running;
    unsigned int            nb_iter;
} logpool_test_updater_data_t;

static void * logpool_test_logger(void * vdata) {
    logpool_test_updater_data_t * data = (logpool_test_updater_data_t *) vdata;
    logpool_t *     logpool = data->logpool;
    log_t *         log = logpool_getlog(logpool, POOL_LOGGER_PREF, LPG_TRUEPREFIX);
    log_t *         alllog = logpool_getlog(logpool, POOL_LOGGER_ALL_PREF, LPG_TRUEPREFIX);
    unsigned long   count = 0;

    while (data->running) {
        LOG_INFO(log, "loop #%lu", count);
        LOG_INFO(alllog, "loop #%lu", count);
        ++count;
    }
    return NULL;
}

static void * logpool_test_updater(void * vdata) {
    logpool_test_updater_data_t * data = (logpool_test_updater_data_t *) vdata;
    logpool_t *     logpool = data->logpool;
    log_t *         log = logpool_getlog(logpool, POOL_LOGGER_PREF, LPG_TRUEPREFIX);
    unsigned long   count = 0;
    log_t           newlog;
    char            logpath[PATH_MAX*2];
    int             all_fd;

    newlog.flags = log->flags;
    newlog.level = LOG_LVL_INFO;
    newlog.out = NULL;
    newlog.prefix = strdup(log->prefix);
    all_fd = open(data->fileprefix, O_RDONLY);

    while (count < data->nb_iter) {
        for (int i = 0; i < 10; ++i) {
            snprintf(logpath, sizeof(logpath), "%s-%03lu.log", data->fileprefix, count % 20);
            newlog.flags &= ~(LOG_FLAG_COLOR | LOG_FLAG_PID);
            if (count % 10 == 0)
                newlog.flags |= (LOG_FLAG_COLOR | LOG_FLAG_PID);
            logpool_add(logpool, &newlog, logpath);
        }
        usleep(20);
        if (count % 500 == 0) {
            struct stat st;
            if (fstat(all_fd, &st) == 0 && st.st_size > 100*1000000) {
                break ;
            }
        }
        ++count;
    }
    data->running = 0;
    free(newlog.prefix);
    close(all_fd);
    return NULL;
}

void * test_logpool(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test        = TEST_START(opts->testpool, "LOGPOOL");
    log_t *         log         = test != NULL ? test->log : NULL;
    log_t           log_tpl     = { log->level,
                                    LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE | LOG_FLAG_PID,
                                    log->out, NULL };
    logpool_t *     logpool     = NULL;
    log_t *         testlog;
    int             ret;

    if (test == NULL) {
        return VOIDP(1);
    }
    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(opts->logs));

    TEST_CHECK(test, "logpool_create()", (logpool = logpool_create()) != NULL);

    log_tpl.prefix = NULL; /* add a default log instance */
    TEST_CHECK(test, "logpool_add(NULL)", logpool_add(logpool, &log_tpl, NULL) != NULL);
    log_tpl.prefix = "TOTO*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_PID | LOG_FLAG_ABS_TIME;
    TEST_CHECK(test, "logpool_add(*)", logpool_add(logpool, &log_tpl, NULL) != NULL);
    log_tpl.prefix = "test/*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_TID | LOG_FLAG_ABS_TIME;
    TEST_CHECK(test, "logpool_add(tests/*)", logpool_add(logpool, &log_tpl, NULL) != NULL);

    const char * const prefixes[] = {
        "vsensorsdemo", "test", "tests", "tests/avltree", NULL
    };

    int flags[] = { LPG_NODEFAULT, LPG_NONE, LPG_TRUEPREFIX, LPG_NODEFAULT, INT_MAX };
    for (int * flag = flags; *flag != INT_MAX; flag++) {
        for (const char * const * prefix = prefixes; *prefix; prefix++) {

            /* logpool_getlog() */
            testlog = logpool_getlog(logpool, *prefix, *flag);

            /* checking logpool_getlog() result */
            if ((*flag & LPG_NODEFAULT) != 0 && flag == flags) {
                TEST_CHECK2(test, "logpool_getlog NULL with LPG_NODEFAULT prefix <%s>",
                            testlog == NULL, STR_CHECKNULL(*prefix));

            } else {
                TEST_CHECK2(test, "logpool_getlog ! NULL with flag:%d prefix <%s>",
                            testlog != NULL, *flag, STR_CHECKNULL(*prefix));
            }
            if (testlog != NULL || log->level >= LOG_LVL_INFO) {
                LOG_INFO(testlog, "CHECK LOG <%-20s> getflg:%d ptr:%p pref:%s",
                        *prefix, *flag, (void *) testlog,
                        testlog ? STR_CHECKNULL(testlog->prefix) : STR_NULL);
            }
            ret = ! ((*flag & LPG_NODEFAULT) != 0 && testlog != NULL
            &&  (   ((*prefix == NULL || testlog->prefix == NULL) && testlog->prefix != *prefix)
                 || (*prefix != NULL && testlog->prefix != NULL
                        && strcmp(testlog->prefix, *prefix) != 0) ));

            TEST_CHECK2(test, "logpool_getlog() returns required prefix with LPG_NODEFAULT"
                        " prefix <%s>", ret != 0, STR_CHECKNULL(*prefix));
        }
    }
    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(logpool));
    logpool_print(logpool, log);

    /* ****************************************************************** */
    /* test logpool on-the-fly log update */
    /* ****************************************************************** */
    const char * tmpdir = test_tmpdir();
    logpool_test_updater_data_t data;
    pthread_t tid_l1, tid_l2, tid_l3, tid_u;
    char firstfileprefix[PATH_MAX*2];
    char check_cmd[PATH_MAX*6];
    size_t len, lensuff;

    LOG_INFO(log, "LOGPOOL: checking multiple threads logging while being updated...");
    /* init thread data */
    data.logpool = logpool;
    data.running = 1;
    if ((opts->test_mode & TEST_MASK(TEST_logpool_big)) != 0) {
        data.nb_iter = 8000;
    } else {
        data.nb_iter = 1000;
    }
    len = VLIB_SNPRINTF(ret, firstfileprefix, sizeof(firstfileprefix), "%s/logpool-test-XXXXXX", tmpdir);
    lensuff = str0cpy(firstfileprefix + len, "-INIT.log", sizeof(firstfileprefix) - len);
    if (mkstemps(firstfileprefix, lensuff) < 0) {
        LOG_WARN(log, "mkstemps error: %s", strerror(errno));
        str0cpy(firstfileprefix, "logpool-test-INIT.log", sizeof(firstfileprefix));
    }
    strn0cpy(data.fileprefix, firstfileprefix, len, sizeof(data.fileprefix));
    /* get default log instance and update it, so that loggers first log in '...INIT' file */
    TEST_CHECK(test, "logpool_getlog(NULL)",
               (testlog = logpool_getlog(logpool, NULL, LPG_NONE)) != NULL);
    testlog->level = LOG_LVL_INFO;
    testlog->flags &= ~LOG_FLAG_PID;
    testlog->flags |= LOG_FLAG_TID;
    testlog->prefix = NULL;
    testlog->out = NULL;
    TEST_CHECK(test, "logpool_add(NULL)", logpool_add(logpool, testlog, firstfileprefix) != NULL);
    logpool_print(logpool, log);
    /* init global logpool-logger log instance */
    log_tpl.level = LOG_LVL_INFO;
    log_tpl.prefix = POOL_LOGGER_ALL_PREF;
    log_tpl.out = NULL;
    log_tpl.flags = testlog->flags;
    TEST_CHECK(test, "logpool_add(" POOL_LOGGER_PREF ")",
               logpool_add(logpool, &log_tpl, data.fileprefix) != NULL);
    /* launch threads */
    TEST_CHECK(test, "thread1", pthread_create(&tid_l1, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "thread2", pthread_create(&tid_l2, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "thread3", pthread_create(&tid_l3, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "threadU", pthread_create(&tid_u, NULL, logpool_test_updater, &data) == 0);
    if (vthread_valgrind(0, NULL)) {
        while (data.running) {
            sleep(1);
        }
        sleep(2);
    } else {
        pthread_join(tid_l1, NULL);
        pthread_join(tid_l2, NULL);
        pthread_join(tid_l3, NULL);
        pthread_join(tid_u, NULL);
    }
    fflush(NULL);

    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(logpool));
    logpool_print(logpool, log);

    if ((opts->test_mode & TEST_MASK(TEST_logpool_big)) != 0) {
        LOG_INFO(log, "LOGPOOL: checking logs...");
        /* check logs versus global logpool-logger-all global log */
        snprintf(check_cmd, sizeof(check_cmd),
                 "ret=false; "
                 "sed -e 's/^[^]]*]//' '%s-'*.log | sort > '%s_concat_filtered.log'; "
                 "rm -f '%s-'*.log; "
                 "sed -e 's/^[^]]*]//' '%s' | sort "
                 "  | diff -ruq '%s_concat_filtered.log' - "
                 "    && ret=true; ",
                 data.fileprefix, data.fileprefix, data.fileprefix, data.fileprefix,
                 data.fileprefix);
        TEST_CHECK(test, "logpool thread logs comparison",
            *data.fileprefix != 0 && system(check_cmd) == 0);
    }
    snprintf(check_cmd, sizeof(check_cmd), "rm -f '%s' '%s-'*.log '%s'_*_filtered.log",
             data.fileprefix, data.fileprefix, data.fileprefix);
    TEST_CHECK(test, "logpool log deletion", system(check_cmd) == 0);

    /* free logpool and exit */
    LOG_INFO(log, "LOGPOOL: freeing...");
    logpool_free(logpool);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

