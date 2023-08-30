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

#include "vlib/util.h"
#include "vlib/time.h"
#include "vlib/thread.h"
#include "vlib/logpool.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST LOG THREAD *************** */
#define N_TEST_THREADS  100
#define PIPE_IN         0
#define PIPE_OUT        1
typedef struct {
    pthread_t           tid;
    log_t *             log;
} test_log_thread_t;

static void * log_thread(void * data) {
    test_log_thread_t * ctx = (test_log_thread_t *) data;
    unsigned long       tid = (unsigned long) pthread_self();
    int                 nerrors = 0;

    LOG_INFO(ctx->log, "Starting %s (tid:%lu)", ctx->log->prefix, tid);

    for (int i = 0; i < 1000; i++) {
        LOG_INFO(ctx->log, "Thread #%lu Loop #%d", tid, i);
    }
    ctx->log->out = NULL;
    return (void *) ((long)nerrors);
}

static const int                s_stop_signal   = SIGHUP;
static volatile sig_atomic_t    s_pipe_stop     = 0;

static void pipe_sighdl(int sig) {
    if (sig == s_stop_signal) { // && (si->si_pid == 0 || si->si_pid == pid))
        s_pipe_stop = 1;
    }
}

static void * pipe_log_thread(void * data) {
    char                buf[4096];
    struct sigaction    sa          = { .sa_handler = pipe_sighdl, .sa_flags = 0 };
    sigset_t            sigmask;
    FILE **             fpipe       = (FILE **) data;
    ssize_t             n;
    unsigned long       nerrors     = 0;
    int                 fd_pipein   = fileno(fpipe[PIPE_IN]);
    int                 fd_pipeout  = fileno(fpipe[PIPE_OUT]);
    int                 o_nonblock  = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, s_stop_signal);
    sigaction(s_stop_signal, &sa, NULL);
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
    while (1) {
        errno = 0;
        /* On signal reception, set read_NON_BLOCK so that we exit as soon as
         * no more data is available. */
        if (s_pipe_stop && !o_nonblock) {
            o_nonblock = 1;
            while ((n = fcntl(fd_pipein, F_SETFL, O_NONBLOCK)) < 0) {
                if (errno != EINTR) {
                    fpipe[PIPE_IN] = NULL;
                    return (void *) (nerrors + 1);
                }
            }
        }
        /* Read data, exit if none.
         * On EINTR, reloop and sighandler set pipestop to unblock read fd.*/
        if ((n = read(fd_pipein, buf, sizeof(buf))) < 0 && errno == EINTR) {
            continue ;
        } else if (n <= 0)
            break ;
        /* Write data. On EINTR, retry. sighandler will set pipestop to unblock read fd. */
        while (write(fd_pipeout, buf, n) < 0) {
            if (errno != EINTR) {
                nerrors++;
                break ;
            } else {
                /* EINTR: conitinue */
            }
        }
    }
    fpipe[PIPE_IN] = NULL;
    return (void *) nerrors;
}

void * test_log_thread(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *       test = TEST_START(opts->testpool, "LOG");
    log_t *             log = test != NULL ? test->log : NULL;
    unsigned int        n_test_threads = N_TEST_THREADS;
    const char *        tmpdir = test_tmpdir();
    const char * const  files[] = { "stdout", "one_file", NULL };
    slist_t             *filepaths = NULL;
    const size_t        cmdsz = 4 * PATH_MAX;
    char *              cmd;
    int                 i_file;
    int                 ret;

    char                path[PATH_MAX];
    FILE *              file;
    FILE *              fpipeout = NULL;
    FILE *              fpipein = NULL;
    test_log_thread_t   * threads;
    log_t               * logs;
    log_t               all_logs[(sizeof(files)/sizeof(*files)) * n_test_threads];
    test_log_thread_t   all_threads[(sizeof(files)/sizeof(*files)) * n_test_threads];
    log_t               thlog = {
                            .level = LOG_LVL_SCREAM,
                            .flags = LOG_FLAG_DEFAULT | LOG_FLAG_FILE | LOG_FLAG_FUNC
                                | LOG_FLAG_LINE | LOG_FLAG_LOC_TAIL };
    char                prefix[20];
    pthread_t           pipe_tid;
    int                 p[2];
    int                 fd_pipein;
    int                 fd_backup;
    void *              thread_ret;
    FILE *              fpipe[2];

    if (vthread_valgrind(0, NULL)) { /* dirty workaround */
        n_test_threads = 10;
    }

    fflush(NULL); /* First of all ensure all previous messages are flushed */
    i_file = 0;
    for (const char * const * filename = files; *filename; filename++, ++i_file) {
        BENCH_TM_DECL(tm0);
        BENCH_DECL(t0);

        threads = &(all_threads[i_file * n_test_threads]);
        logs = &(all_logs[i_file * n_test_threads]);

        file = fpipeout = fpipein = NULL;
        fd_pipein = fd_backup = -1;
        p[0] = p[1] = -1;

        /* generate unique tst file name for the current loop */
        snprintf(path, sizeof(path), "%s/test_thread_%s_%d_%u_%lx.log", tmpdir,
                 *filename, i_file, (unsigned int) getpid(), (unsigned long) time(NULL));
        filepaths = slist_prepend(filepaths, strdup(path));
        LOG_INFO(log, ">>> logthread: test %s (%s)", *filename, path);

        /* Create pipe to redirect stdout & stderr to pipe log file (fpipeout) */
        if ((!strcmp("stderr", *filename) && (file = stderr))
        ||  (!strcmp(*filename, "stdout") && (file = stdout))) {
            TEST_CHECK2(test, "create file '%s'",
                        (fpipeout = fopen(path, "w")) != NULL, path);
            if (fpipeout == NULL) {
                continue ;
            }
            TEST_CHECK(test, "pipe()", (ret = pipe(p)) == 0);
            if (ret != 0) {
                fclose(fpipeout);
                continue ;
            }
            TEST_CHECK(test, "pipe fd, dup(), dup2()",
                (ret = (p[PIPE_OUT] < 0 || (fd_backup = dup(fileno(file))) < 0
                        || dup2(p[PIPE_OUT], fileno(file)) < 0)) == 0);
            if (ret != 0) {
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                close(fd_backup);
                continue ;
            }
            TEST_CHECK(test, "fdopen(p[PIPE_IN])",
                       (fpipein = fdopen(p[PIPE_IN], "r")) != NULL);
            if (fpipein == NULL) {
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                continue ;
            }
            /* create thread reading data from pipe, and writing to pipe log file */
            fd_pipein = fileno(fpipein);
            fpipe[PIPE_IN] = fpipein;
            fpipe[PIPE_OUT] = fpipeout;
            TEST_CHECK(test, "pthread_create(pipe)",
                       (ret = pthread_create(&pipe_tid, NULL, pipe_log_thread, fpipe)) == 0);
            if (ret == 0 && vthread_valgrind(0, NULL)) {
                pthread_detach(pipe_tid);
            }
        } else {
            TEST_CHECK2(test, "create file '%s'", (file = fopen(path, "w")) != NULL, path);
            if (file == NULL)
                continue ;
        }

        /* Create log threads and launch them */
        thlog.out = file;
        BENCH_TM_START(tm0);
        BENCH_START(t0);
        for (unsigned int i = 0; i < n_test_threads; ++i) {
            memcpy(&(logs[i]), &thlog, sizeof(log_t));
            snprintf(prefix, sizeof(prefix), "THREAD%05d", i);
            logs[i].prefix = strdup(prefix);
            threads[i].log = &logs[i];
            TEST_CHECK2(test, "pthread_create(#%u)",
                        (ret = pthread_create(&(threads[i].tid), NULL,
                                              log_thread, &(threads[i]))) == 0, i);
            if (ret != 0) {
                threads[i].log = NULL;
            } else if (vthread_valgrind(0, NULL)) {
                pthread_detach(threads[i].tid);
            }
        }

        /* wait log threads to terminate */
        LOG_INFO(log, "Waiting logging threads...");
        for (unsigned int i = 0; i < n_test_threads; ++i) {
            thread_ret = NULL;
            if (vthread_valgrind(0, NULL)) {
                while(threads[i].log->out != NULL) {
                    sleep(1);
                }
            } else {
                pthread_join(threads[i].tid, &thread_ret);
            }
            TEST_CHECK2(test, "log_thread #%u errors=0", (unsigned long) thread_ret == 0, i);
            if (threads[i].log->prefix) {
                char * to_free = threads[i].log->prefix;
                threads[i].log->prefix = "none";
                free(to_free);
            }
        }
        LOG_INFO(log, "logging threads finished.");
        fflush(file);
        BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);

        /* terminate log_pipe thread */
        if (fd_pipein >= 0) {
            fflush(NULL);
            LOG_INFO(log, "Waiting pipe thread...");
            thread_ret = NULL;
            if (vthread_valgrind(0, NULL)) {
                s_pipe_stop = 1;
                sleep(2);
                while (fpipe[PIPE_IN] != NULL) {
                    sleep(1);
                    close(fd_pipein);
                }
                sleep(2);
                LOG_INFO(log, "[valgrind] pipe thread finished...");
                sleep(1);
            } else {
                pthread_kill(pipe_tid, s_stop_signal);
                pthread_join(pipe_tid, &thread_ret);
            }
            LOG_INFO(log, "pipe thread finished.");
            TEST_CHECK(test, "pipe_thread nerrors=0", (unsigned long) thread_ret == 0);
            /* restore redirected file */
            TEST_CHECK(test, "dup2() restore", dup2(fd_backup, fileno(file)) >= 0);
            /* cleaning */
            fclose(fpipein); // fclose will close p[PIPE_IN]
            close(p[PIPE_OUT]);
            close(fd_backup);
            fclose(fpipeout);
            /* write something in <file> and it must not be redirected anymore */
            ret = fprintf(file, "*** checking %s is not anymore redirected ***\n\n", *filename);
            TEST_CHECK(test, "fprintf stdout", ret > 0);
        } else {
            fclose(file);
        }
        LOG_INFO(log, "duration : %ld.%03lds (cpus %ld.%03lds)",
                 BENCH_TM_GET(tm0) / 1000, BENCH_TM_GET(tm0) % 1000,
                 BENCH_GET(t0) / 1000, BENCH_GET(t0) % 1000);
    }

    /* compare logs */
    LOG_INFO(log, "checking logs...");
    TEST_CHECK(test, "malloc shell command", (cmd = malloc(cmdsz)) != NULL);
    if (cmd != NULL) {
        size_t n = 0;
        /* construct a shell script iterating on each file, filter them to remove timestamps
         * and diff them */
        n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n, "ret=true; prev=; for f in ");
        SLIST_FOREACH_DATA(filepaths, spath, char *) {
            n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n, "%s ", spath);
        }
        n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n,
              "; do sed -e 's/^[.:0-9[:space:]]*//' -e s/'Thread #[0-9]*/Thread #X/' "
              "       -e 's/tid:[0-9]*/tid:X/'"
              "       \"$f\" | sort > \"${f%%.log}_filtered.log\"; "
              "     if test -n \"$prev\"; then "
              "         diff -u \"${prev%%.log}_filtered.log\" \"${f%%.log}_filtered.log\" "
              "           || { echo \"diff error (filtered $prev <> $f)\"; ret=false; };"
              "         grep -E '\\*\\*\\* checking .* "
              "                  is not anymore redirected \\*\\*\\*' \"${f}\""
              "           && { echo \"error: <${f}> pipe has not been restored\"; ret=false; };"
              "         rm \"$prev\" \"${prev%%.log}_filtered.log\";"
              "     fi; prev=$f; "
              " done; test -e \"$prev\" && rm \"$prev\" \"${prev%%.log}_filtered.log\"; $ret");
        TEST_CHECK(test, "log comparison", system(cmd) == 0);
        free(cmd);
    }
    slist_free(filepaths, free);
    fflush(stdout);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

