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

// *****************************************************************************
// test vthread
// *****************************************************************************

#define PIPETHREAD_STR      "Test Start Loop\n"
#define PIPETHREAD_BIGSZ    (((PIPE_BUF) * 7) + (((PIPE_BUF) / 3) + 5))
typedef struct {
    log_t *         log;
    unsigned int    nb_try;
    unsigned int    nb_bigtry;
    unsigned int    nb_ok;
    unsigned int    nb_bigok;
    unsigned int    nb_error;
    char *          bigpipebuf;
    vthread_t * target;
    int             pipe_fdout;
    ssize_t         offset;
} pipethread_ctx_t;

static int  piperead_callback(
                vthread_t *         vthread,
                vthread_event_t     event,
                void *                  event_data,
                void *                  user_data) {
    ssize_t                 ret;
    ssize_t                 n       = 0;
    int                     fd      = (int)((long) event_data);
    pipethread_ctx_t *      pipectx = (pipethread_ctx_t *) user_data;
    char                    buffer[PIPE_BUF];
    unsigned int            header_sz = 1;
    (void) vthread;
    (void) event;

    LOG_DEBUG(pipectx->log, "%s(): enter", __func__);
    while (1) {
        header_sz=1;
        while ((ret = read(fd, buffer, header_sz)) < 0 && errno == EINTR)
            ; /* nothing but loop */
        if (ret <= 0) {
            break ;
        }
        if (*buffer == 0 || pipectx->offset < PIPETHREAD_BIGSZ) {
            LOG_DEBUG(pipectx->log, "%s(): big pipe loop", __func__);
            /* bigpipebuf */
            if (*buffer == 0 && pipectx->offset >= PIPETHREAD_BIGSZ)
                pipectx->offset = 0;
            pipectx->offset += header_sz;
            while (pipectx->offset < PIPETHREAD_BIGSZ) {
                LOG_DEBUG(pipectx->log, "%s(): big pipe loop 1 %zd", __func__, pipectx->offset);
                ssize_t i;
                size_t toread = (pipectx->offset + PIPE_BUF > PIPETHREAD_BIGSZ
                                 ? PIPETHREAD_BIGSZ - pipectx->offset : PIPE_BUF) - header_sz;
                while ((ret = read(fd, buffer + header_sz, toread)) < 0
                        && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                    ; /* nothing but loop */
                if (ret <= 0) {
                    LOG_WARN(pipectx->log, "error bigpipe read error: %s", strerror(errno));
                    break ;
                }
                LOG_DEBUG(pipectx->log, "%s(): big pipe loop 2 %zd ovflw %d",
                            __func__, pipectx->offset, pipectx->offset +ret > PIPETHREAD_BIGSZ);
                for (i = 0; i < ret; i++) {
                    if (buffer[i] != (char)((pipectx->offset - header_sz + i) % 256)) {
                        ++pipectx->nb_error;
                        LOG_ERROR(pipectx->log, "error: bad big pipe message (idx %zd sz %zd)",
                                  i, ret);
                        break ;
                    }
                }
                pipectx->offset += ret;
                if (i == ret && pipectx->offset == PIPETHREAD_BIGSZ) {
                    ++pipectx->nb_bigok;
                    LOG_DEBUG(pipectx->log, "%s(): big pipe loop OK", __func__);
                }
                header_sz = 0;
            }
        } else {
            LOG_DEBUG(pipectx->log, "%s(): little pipe loop", __func__);
            while ((ret = read(fd, buffer + header_sz, sizeof(PIPETHREAD_STR) - 1 - header_sz))
                     < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                ; /* nothing but loop */
            if (ret <= 0) {
                LOG_ERROR(pipectx->log, "error: bad pipe message size");
                ++pipectx->nb_error;
                break ;
            }
            ret += header_sz;
            LOG_DEBUG_BUF(pipectx->log, buffer, ret, "%s(): ", __func__);
            if (ret != sizeof(PIPETHREAD_STR) - 1 || memcmp(buffer, PIPETHREAD_STR, ret) != 0) {
                LOG_ERROR(pipectx->log, "error: bad pipe message");
                ++pipectx->nb_error;
            } else {
                ++pipectx->nb_ok;
            }
            n += ret;
        }
    }
    LOG_DEBUG(pipectx->log, "%s(): big pipe loop exit %zd", __func__, pipectx->offset);
    return 0;
}

static int  thread_loop_pipe_write(
                vthread_t *         vthread,
                vthread_event_t     event,
                void *                  event_data,
                void *                  user_data) {
    pipethread_ctx_t * data = (pipethread_ctx_t *) user_data;
    (void) vthread;
    (void) event_data;
    (void) event;

    LOG_DEBUG(data->log, "%s(): enter", __func__);

    ++(data->nb_try);
    if (vthread_pipe_write(data->target, data->pipe_fdout, PIPETHREAD_STR,
                sizeof(PIPETHREAD_STR) - 1) != sizeof(PIPETHREAD_STR) - 1) {
        LOG_ERROR(data->log, "error vthread_pipe_write(pipestr): %s", strerror(errno));
        ++(data->nb_error);
    }

    ++(data->nb_bigtry);
    if (vthread_pipe_write(data->target, data->pipe_fdout, data->bigpipebuf,
            PIPETHREAD_BIGSZ) != PIPETHREAD_BIGSZ) {
        LOG_ERROR(data->log, "error vthread_pipe_write(bigpipebuf): %s", strerror(errno));
        ++(data->nb_error);
    }

    return 0;
}

void * test_thread(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test            = TEST_START(opts->testpool, "THREAD");
    log_t *         log             = test != NULL ? test->log : NULL;
    vthread_t * vthread;
    void *          thread_result   = (void *) 1UL;
    const long      bench_margin_ms = 400;
    long            bench;

    BENCH_TM_DECL(t);

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 0, kill before start");
    TEST_CHECK(test, "vthread_create(t=0,kill+0)",
               (vthread = vthread_create(0, log)) != NULL);

    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vthread_stop(t=0,kill+0)",
               vthread_stop(vthread) == thread_result);

    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 500, start and kill after 1s");
    TEST_CHECK(test, "vthread_create(t=500,kill+1)",
               (vthread = vthread_create(500, log)) != NULL);
    TEST_CHECK(test, "vthread_start(t=500,kill+1)",
               vthread_start(vthread) == 0);

    sched_yield();
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vthread_stop(t=500,kill+1)",
               vthread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, start and kill after 1s");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vthread_create(t=0,kill+1)",
               (vthread = vthread_create(0, log)) != NULL);
    TEST_CHECK(test, "vthread_start(t=0,kill+1)",
               vthread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vthread_stop(t=0,kill+1)",
               vthread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0 exit_sig SIGALRM, start and kill after 1s");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vthread_create(t=0,sig=alrm,kill+1)",
               (vthread = vthread_create(0, log)) != NULL);
    TEST_CHECK(test, "vthread_exit_signal(ALRM)",
               vthread_set_exit_signal(vthread, SIGALRM) == 0);
    TEST_CHECK(test, "vthread_start(t=0,sig=alrm,kill+1)",
               vthread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vthread_stop(t=0,sig=alrm,kill+1)",
               vthread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, exit_sig SIGALRM after 500ms, "
                   "start and kill after 500 more ms");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vthread_create(t=0,sig=alrm+.5,kill+1)",
               (vthread = vthread_create(0, log)) != NULL);
    TEST_CHECK(test, "vthread_start(t=0,sig=alrm+.5,kill+1)",
               vthread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    usleep(500000);
    TEST_CHECK(test, "vthread_exit_signal(ALRM)",
               vthread_set_exit_signal(vthread, SIGALRM) == 0);
    usleep(500000);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vthread_stop(t=0,sig=alrm+.5,kill+1)",
               vthread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating multiple threads");
    const unsigned      nthreads = 50;
    int                 pipefd = -1;
    vthread_t *     vthreads[nthreads];
    log_t               logs[nthreads];
    pipethread_ctx_t    all_pipectx[nthreads];
    pipethread_ctx_t    pipectx = { log, 0, 0, 0, 0, 0, NULL, NULL, -1, PIPETHREAD_BIGSZ };

    /* init big pipe buf */
    TEST_CHECK(test, "malloc(bigpipebuf)",
               (pipectx.bigpipebuf = malloc(PIPETHREAD_BIGSZ)) != NULL);
    if (pipectx.bigpipebuf != NULL) {
        pipectx.bigpipebuf[0] = 0;
        for (unsigned i = 1; i < PIPETHREAD_BIGSZ; ++i) {
            pipectx.bigpipebuf[i] = (char) (i % 256);
        }
    }
    for (size_t i = 0; i < sizeof(vthreads) / sizeof(*vthreads); i++) {
        logs[i].prefix = strdup("thread000");
        snprintf(logs[i].prefix + 6, 4, "%03zu", i);
        logs[i].level = log->level;
        logs[i].out = log->out;
        logs[i].flags = LOG_FLAG_DEFAULT;
        TEST_CHECK2(test, "vthread_create(#%zu)",
                ((vthreads[i] = vthread_create(i % 5 == 0 ? 10 : 0, &logs[i])) != NULL), i);
        if (vthreads[i] != NULL) {
            all_pipectx[i] = pipectx;
            all_pipectx[i].log = &logs[i];
            if (i == 0) {
                TEST_CHECK2(test, "vthread_pipe_create(#%zu)",
                    ((pipefd = vthread_pipe_create(vthreads[i], piperead_callback,
                                                      &(all_pipectx[0]))) >= 0), i);
                all_pipectx[0].pipe_fdout = pipefd;
                all_pipectx[0].target = vthreads[0];
                pipectx.pipe_fdout = pipefd;
                pipectx.target = vthreads[0];
            }
            if (i && i % 5 == 0) {
                TEST_CHECK2(test, "vlib_register_event(#%zu,pipe_write,proc_start)",
                    vthread_register_event(vthreads[i], VTE_PROCESS_START, NULL,
                            thread_loop_pipe_write, &(all_pipectx[i])) == 0, i);
            }
            TEST_CHECK2(test, "vthread_start(#%zu)",
                       vthread_start(vthreads[i]) == 0, i);
            ++(all_pipectx[0].nb_try);
            TEST_CHECK2(test, "vthread_pipe_write(#%zu,small)",
                vthread_pipe_write(vthreads[0], pipefd, PIPETHREAD_STR,
                                       sizeof(PIPETHREAD_STR) - 1) == sizeof(PIPETHREAD_STR) - 1, i);

            ++(all_pipectx[0].nb_bigtry);
            TEST_CHECK2(test, "vthread_pipe_write(#%zu,big)",
                vthread_pipe_write(vthreads[0], pipefd, pipectx.bigpipebuf, PIPETHREAD_BIGSZ)
                        == PIPETHREAD_BIGSZ, i);
        }
    }
    sleep(2);
    for (int i = sizeof(vthreads) / sizeof(*vthreads) - 1; i >= 0; i--) {
        if (vthreads[i] != NULL) {
            TEST_CHECK2(test, "vthread_stop(#%d)",
                        vthread_stop(vthreads[i]) == thread_result, i);
        }
        free(logs[i].prefix);
        if (i != 0) {
            if (i == 1) {
                fsync(pipectx.pipe_fdout);
                fflush(NULL);
                sleep(1);
            }
            all_pipectx[0].nb_try += all_pipectx[i].nb_try;
            all_pipectx[0].nb_bigtry += all_pipectx[i].nb_bigtry;
            all_pipectx[0].nb_ok += all_pipectx[i].nb_ok;
            all_pipectx[0].nb_bigok += all_pipectx[i].nb_bigok;
            all_pipectx[0].nb_error += all_pipectx[i].nb_error;
        }
    }

    if (pipectx.bigpipebuf != NULL)
        free(pipectx.bigpipebuf);

    TEST_CHECK2(test, "pipethread: %u/%u msgs, %u/%u bigmsgs, %u errors",
        (bench = (all_pipectx[0].nb_ok == all_pipectx[0].nb_try
                  && all_pipectx[0].nb_bigok == all_pipectx[0].nb_bigtry
                  && all_pipectx[0].nb_error == 0)) != 0,
        all_pipectx[0].nb_ok, all_pipectx[0].nb_try,
        all_pipectx[0].nb_bigok, all_pipectx[0].nb_bigtry,
        all_pipectx[0].nb_error);

    if (bench == 0) {
        if (test != NULL) test->n_errors += all_pipectx[0].nb_error;
    } else {
        LOG_INFO(log, "%s(): %u msgs, %u bigmsgs, 0 error.", __func__,
                 all_pipectx[0].nb_ok, all_pipectx[0].nb_bigok);
    }
    sleep(2);

    return VOIDP(TEST_END(test) + (test == NULL && bench == 0
                                   ? 1 + all_pipectx[0].nb_error : 0));
}

#endif /* ! ifdef _TEST */

