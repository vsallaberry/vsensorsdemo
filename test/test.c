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
#include <sys/stat.h>
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
//#include "vsensors.h"
#include "test/test.h"
#include "test_private.h"

#define TEST_EXPERIMENTAL_PARALLEL // TODO

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS("TEST-" BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2020,2023")

void *          test_options(void * vdata);
void *          test_tests(void * vdata);
void *          test_optusage(void * vdata);
void *          test_sizeof(void * vdata);
void *          test_ascii(void * vdata);
void *          test_colors(void * vdata);
void *          test_bench(void * vdata);
void *          test_math(void * vdata);
void *          test_list(void * vdata);
void *          test_hash(void * vdata);
void *          test_rbuf(void * vdata);
void *          test_avltree(void * vdata);
void *          test_sensor_value(void * vdata);
void *          test_sensor_plugin(void * vdata);
void *          test_account(void * vdata);
void *          test_bufdecode(void * vdata);
void *          test_srcfilter(void * vdata);
void *          test_logpool(void * vdata);
void *          test_job(void * vdata);
void *          test_thread(void * vdata);
void *          test_log_thread(void * vdata);
void *          test_optusage_stdout(void * vdata);

static const struct {
    const char *    name;
    void *          (*fun)(void*);
    unsigned int    mask; /* set of tests preventing this one to run */
} s_testconfig[] = { // same order as enum test_private.h/test_mode_t
    { "all",                NULL,               0 },
    { "options",            test_options,       0 },
    { "tests",              test_tests,         0 },
    { "optusage",           test_optusage,      0 },
    { "sizeof",             test_sizeof,        0 },
    { "ascii",              test_ascii,         0 },
    { "color",              test_colors,        0 },
    { "math",               test_math,          0 },
    { "list",               test_list,          0 },
    { "hash",               test_hash,          0 },
    { "rbuf",               test_rbuf,          0 },
    { "tree",               test_avltree,       0 },
    { "sensorvalue",        test_sensor_value,  0 },
    { "sensorplugin",       test_sensor_plugin, 0 },
    { "account",            test_account,       0 },
    { "bufdecode",          test_bufdecode,     0 },
    { "srcfilter",          test_srcfilter,     0 },
    { "logpool",            test_logpool,       0 },
    { "job",                test_job,           0 },
    { "bench",              test_bench,         TEST_MASK_ALL },
    { "vthread",            test_thread,        TEST_MASK_ALL },
    { "log",                test_log_thread,    TEST_MASK_ALL },
    /* Excluded from all */
    { "bigtree",            NULL,               0 },
    { "optusage_big",       NULL,               0 },
    { "optusage_stdout",    test_optusage_stdout,TEST_MASK_ALL },
    { "logpool_big",        NULL,               0 },
#ifdef TEST_EXPERIMENTAL_PARALLEL
    { "EXPERIMENTAL_parallel", NULL,            0 },
#endif
    { NULL, NULL, 0 } /* Must be last */
};

int test_describe_filter(int short_opt, const char * arg, int * i_argv,
                         const opt_config_t * opt_config) {
    int             n = 0, ret;
    char            sep[2]= { 0, 0 };
    (void) short_opt;
    (void) opt_config;

    n += VLIB_SNPRINTF(ret, (char *) arg + n, *i_argv - n,
                         "- test modes: '");

    for (unsigned int i = 0; i < PTR_COUNT(s_testconfig)
                             && s_testconfig[i].name != NULL; ++i, *sep = ',') {
        const char * mode = s_testconfig[i].name;
        n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s", sep, mode);
    }

    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n,
                       "' (fnmatch(3) pattern). \r- Excluded from '%s':'",
                       s_testconfig[TEST_all].name);
    *sep = 0;
    for (unsigned i = TEST_excluded_from_all; i < TEST_NB; i++, *sep = ',') {
        if (i < PTR_COUNT(s_testconfig)) {
            n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s",
                    sep, s_testconfig[i].name);
        }
    }
    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "'");

    *i_argv = n;
    return OPT_CONTINUE(1);
}

unsigned int test_getmode(const char *arg) {
    char token0[128];
    char * endptr = NULL;
    const unsigned int test_mode_all = TEST_MASK(TEST_excluded_from_all) - 1; //0xffffffffU;
    unsigned int test_mode = test_mode_all;
    if (arg != NULL) {
        errno = 0;
        test_mode = strtol(arg, &endptr, 0);
        if (errno != 0 || endptr == NULL || *endptr != 0) {
            const char * token, * next = arg;
            size_t len, i;
            while ((len = strtok_ro_r(&token, ",~", &next, NULL, 0)) > 0 || *next) {
                int is_pattern;
                len = strn0cpy(token0, token, len, sizeof(token0) / sizeof(*token0));
                is_pattern = (fnmatch_patternidx(token0) >= 0);
                for (i = 0; i < PTR_COUNT(s_testconfig) && s_testconfig[i].name != NULL; i++) {
                    if ((is_pattern && 0 == fnmatch(token0, s_testconfig[i].name, FNM_CASEFOLD))
                    ||  (!is_pattern && (0 == strncasecmp(s_testconfig[i].name, token0, len)
                                         &&  s_testconfig[i].name[len] == 0))) {
                        if (token > arg && *(token - 1) == '~') {
                            test_mode &= (i == TEST_all ? ~test_mode_all : ~TEST_MASK(i));
                        } else {
                            test_mode |= (i == TEST_all ? test_mode_all : TEST_MASK(i));
                        }
                        if ((++is_pattern) == 1)
                            break ;
                    }
                }
                if (s_testconfig[i].name == NULL && is_pattern <= 1) {
                    fprintf(stderr, "%s%serror%s: %sunreconized test id '",
                            vterm_color(STDERR_FILENO, VCOLOR_RED),
                            vterm_color(STDERR_FILENO, VCOLOR_BOLD),
                            vterm_color(STDERR_FILENO, VCOLOR_RESET),
                            vterm_color(STDERR_FILENO, VCOLOR_BOLD));
                    fwrite(token, 1, len, stderr);
                    fprintf(stderr, "'%s\n", vterm_color(STDERR_FILENO, VCOLOR_RESET));
                    return 0;
                }
            }
        }
    }
    return test_mode;
}

/* create a temp directory if not done already, and return its path */
static char                 s_tests_tmpdir[PATH_MAX * 2] = { 0, };
static const char * const   s_tests_tmpdir_base = "/tmp";

const char * test_tmpdir() {
    if (*s_tests_tmpdir == 0) {
        int ret;
        const char * base = getenv("VLIB_TEST_TMPDIR_BASE");
        if (base == NULL)
            base = s_tests_tmpdir_base;
        snprintf(s_tests_tmpdir, sizeof(s_tests_tmpdir), "%s/%s-tests.XXXXXX", base, BUILD_APPNAME);
        ret = mkdir(base, S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret == 0)
            chmod(base, S_IRWXU | S_IRWXG | S_IRWXO);
        if ((ret != 0 && errno != EEXIST)
        ||  mkdtemp(s_tests_tmpdir) == NULL) {
            LOG_WARN(NULL, "cannot create tmpdir %s: %s. Trying in current dir...", s_tests_tmpdir, strerror(errno));
            snprintf(s_tests_tmpdir, sizeof(s_tests_tmpdir), "tmp-%s-tests", BUILD_APPNAME);
            if (mkdir(s_tests_tmpdir, S_IRWXU) != 0 && errno != EEXIST)
                return NULL;
        }
    }
    return s_tests_tmpdir;
}

int test_clean_tmpdir() {
    if (*s_tests_tmpdir != 0) {
        if (rmdir(s_tests_tmpdir) == 0) {
            *s_tests_tmpdir = 0;
            /*int errno_save = errno;
            rmdir(s_tests_tmpdir_base);
            errno = errno_save;*/
        } else {
            return -1;
        }
    }
    return 0;
}

/* used by slist, avltree and other tests */
int intcmp(const void * a, const void *b) {
    return (long)a - (long)b;
}

/* test vsersion string */
const char * test_version_string() {
    static const char * ver_str = VERSION_STRING;
    return ver_str;
}

#ifdef TEST_EXPERIMENTAL_PARALLEL
/** for parallel tests */
typedef struct {
    vjob_t *        job;
    unsigned int    testidx;
} testjob_t;

unsigned long check_test_jobs(options_test_t * opts, log_t * log, shlist_t * jobs,
                              unsigned int * nb_jobs, unsigned long * current_tests) {
    unsigned long nerrors = 0;
    testjob_t * tjob;
    unsigned int nb_jobs_orig = *nb_jobs;
    (void) opts;

    if (jobs->head == NULL) {
        LOG_VERBOSE(log, "%s(): No job running", __func__);
        return 0;
    }

    tjob = (testjob_t *) jobs->head->data;

    LOG_VERBOSE(log, "WAITING for 1 termination (%u running)", *nb_jobs);

    while (nb_jobs_orig == *nb_jobs) {
        for (slist_t * list = jobs->head, * prev = NULL; list != NULL; /*no_incr*/) {
            tjob = (testjob_t *) (list->data);
            vjob_t * vjob = tjob->job;
            if (vjob_done(vjob)) {
                slist_t *   to_free = list;
                void *      result = vjob_free(vjob);

                if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT) {
                    LOG_ERROR(log, "error: cannot get job result for '%s'",
                            s_testconfig[tjob->testidx].name);
                    ++nerrors;
                } else {
                    LOG_VERBOSE(log, "TEST JOB '%s' finished with result %ld",
                                s_testconfig[tjob->testidx].name, (long) result);
                    nerrors += (long) result;
                }
                --(*nb_jobs);
                *current_tests &= ~TEST_MASK(tjob->testidx);
                if (list == jobs->tail) {
                    jobs->tail = prev;
                }
                list = list->next;
                if (prev == NULL) {
                    jobs->head = list;
                } else {
                    prev->next = list;
                }
                slist_free_1(to_free, free);
            } else {
                prev = list;
                list = list->next;
            }
        }
        sleep(1);
    }
    return nerrors;
}
#endif

/* *************** TEST MAIN FUNC *************** */

int test_options_init(int argc, const char *const* argv, options_test_t * opts);

int test(int argc, const char *const* argv, unsigned int test_mode, logpool_t ** logpool) {
    options_test_t  options_test    = { .flags = 0, .test_mode = test_mode, .main=pthread_self(),
                                        .testpool = NULL, .logs = logpool ? *logpool : NULL,
                                        .argc = argc, .argv = argv };

    log_t *         log             = logpool_getlog(options_test.logs,
                                                     TESTPOOL_LOG_PREFIX, LPG_TRUEPREFIX);
    const char *    tmpdir;
    unsigned int    errors = 0;
    char const **   test_argv = NULL;
    shlist_t        jobs = SHLIST_INITIALIZER();
    sigset_t        sigset_bak;
#ifdef TEST_EXPERIMENTAL_PARALLEL
    unsigned int    nb_jobs = 0;
    unsigned long   current_tests = 0;
#endif

    LOG_INFO(log, NULL);
    if ((tmpdir = test_tmpdir()) == NULL) {
        LOG_WARN(log, "cannot create tmpdir: %s", strerror(errno));
    } else {
        LOG_INFO(log, "created tmpdir '%s'.", tmpdir);
    }

    LOG_INFO(log, NULL);
    LOG_INFO(log, ">>> TEST MODE: 0x%x, %u CPUs\n", test_mode, vjob_cpu_nb());

    if ((test_argv = malloc(sizeof(*test_argv) * argc)) != NULL) {
        test_argv[0] = BUILD_APPNAME;
        for (int i = 1; i < argc; ++i) {
            test_argv[i] = argv[i];
        }
        options_test.argv = test_argv;
    }

    test_options_init(argc, argv, &options_test); // not mandatory ?
    argv = options_test.argv;

    if (options_test.test_mode == 0) {
        logpool_release(options_test.logs, log);
        if (logpool)
            *logpool = options_test.logs;
        if (test_argv != NULL)
            free(test_argv);
        return OPT_ERROR(OPT_EOPTARG);
    }
    test_mode |= options_test.test_mode;
    options_test.out = log->out;

    /* get term columns */
    if ((options_test.columns
                = vterm_get_columns(log && log->out ? fileno(log->out) : STDERR_FILENO)) <= 0) {
        options_test.columns = 80;
    }

    /* create testpool */
    options_test.testpool = tests_create(options_test.logs,
                                         TPF_STORE_ERRORS | TPF_TESTOK_SCREAM | TPF_LOGTRUEPREFIX);

    /* Block SIGALRM by default to not disturb at least test_bench() */
    if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0) {
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGALRM);
        if (sigprocmask(SIG_SETMASK, &sigset, &sigset_bak) != 0) {
            ++errors;
            LOG_ERROR(log, "cannot block SIGALRM: %s", strerror(errno));
        }
    }

    /* Run all requested tests */
    for (unsigned int testidx = 0; testidx < PTR_COUNT(s_testconfig); ++testidx ) {
        if (s_testconfig[testidx].name == NULL || s_testconfig[testidx].fun == NULL) {
            continue ;
        }
        if ((test_mode & TEST_MASK(testidx)) != 0) {
#ifdef TEST_EXPERIMENTAL_PARALLEL // TODO
            if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0) {
                /* ********************************************************
                 * MULTI THREADED TESTS
                 *********************************************************/
                testjob_t * tjob;

                do {
                    int waitneeded = 0;
                    SLIST_FOREACH_DATA(jobs.head, it_tjob, testjob_t *) {
                        if ((s_testconfig[it_tjob->testidx].mask & TEST_MASK(testidx)) != 0) {
                            waitneeded = 1;
                            break ;
                        }
                    }
                    if (waitneeded == 0 && (current_tests & s_testconfig[testidx].mask) == 0) {
                        break ;
                    }
                    errors += check_test_jobs(&options_test, log, &jobs, &nb_jobs, &current_tests);
                } while (jobs.head != NULL);

                LOG_VERBOSE(log, "RUNNING test job '%s'...", s_testconfig[testidx].name);
                if ((tjob = malloc(sizeof(*tjob))) == NULL
                || (tjob->job = vjob_run(s_testconfig[testidx].fun, &options_test)) == NULL) {
                    if (tjob != NULL)
                        free(tjob);
                    LOG_ERROR(log, "error: cannot run job for test '%s'", s_testconfig[testidx].name);
                } else {
                    tjob->testidx = testidx;
                    current_tests |= TEST_MASK(testidx);
                    ++nb_jobs;
                    if ((jobs.head = slist_appendto(jobs.head, tjob, &jobs.tail)) != NULL
                    &&  nb_jobs > vjob_cpu_nb()) {
                        errors += check_test_jobs(&options_test, log, &jobs, &nb_jobs, &current_tests);
                    }
                }
            } else
#endif
            {
                /* ********************************************************
                 * SINGLE THREADED TESTS
                 *********************************************************/
                LOG_VERBOSE(log, "running test '%s'...", s_testconfig[testidx].name);
                errors += (long) (s_testconfig[testidx].fun(&options_test));
            }
        }
    }
    SLIST_FOREACH_DATA(jobs.head, tjob, testjob_t *) {
        void * result = vjob_waitandfree(tjob->job);
        if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT) {
            LOG_ERROR(log, "error: cannot get test job result");
            ++errors;
        } else {
            errors += (long) result;
        }
    }
    slist_free(jobs.head, free);

    if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0
    &&  sigprocmask(SIG_SETMASK, &sigset_bak, NULL) != 0) {
        ++errors;
        LOG_ERROR(log, "cannot block SIGALRM: %s", strerror(errno));
    }

    /* test Tree */
    /*if ((test_mode & (TEST_MASK(TEST_tree) | TEST_MASK(TEST_bigtree) | TEST_MASK(TEST_tree_MT))) != 0) {
        vjob_t * job = NULL;
        if ((test_mode & TEST_MASK(TEST_tree_MT)) != 0) {
            job = vjob_run(test_avltree, &options_test);
        }
        errors += (long) test_avltree(&options_test);

        if (job != NULL) {
            void * result = vjob_waitandfree(job);
            if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT)
                ++errors;
            else
                errors += (long) result;
        }
    }*/

    /* print tests results and free testpool */
    tests_print(options_test.testpool, TPR_PRINT_ERRORS | TPR_PRINT_OK);
    tests_print(options_test.testpool, TPR_PRINT_GROUPS);
    tests_free(options_test.testpool);

    /* ***************************************************************** */
    LOG_INFO(log, "<<< END of Tests : %u error(s).\n", errors);

    if (test_clean_tmpdir() != 0) {
        LOG_WARN(log, "cannot clean tmpdir %s: %s", tmpdir ? tmpdir : "(null)", strerror(errno));
    }

    logpool_release(options_test.logs, log);

    if (test_argv != NULL) {
        free(test_argv);
    }

    /* just update logpool, don't free it: it will be done by caller */
    if (logpool) {
        *logpool = options_test.logs;
    }

    return -errors;
}

#ifndef APP_INCLUDE_SOURCE
static const char * s_app_no_source_string
    = "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
             BUILD_APPNAME " source not included in this build.\n";

int test_vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
        return vdecode_buffer(out, buffer, buffer_size, ctx,
                              s_app_no_source_string, strlen(s_app_no_source_string));
}
#endif

#endif /* ! ifdef _TEST */

