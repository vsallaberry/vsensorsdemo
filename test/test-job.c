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

#include "vlib/job.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* ************************************************************************ */
/* JOB TESTS */
/* ************************************************************************ */
static const unsigned int   s_PR_JOB_LOOP_NB = 3;
static const unsigned int   s_PR_JOB_LOOP_SLEEPMS = 500;

typedef struct {
    log_t *         log;
    pthread_t       tid;
    unsigned int    pr_job_counter;
} pr_job_data_t;

static int vsleep_ms(unsigned long time_ms) {
    if (time_ms >= 1000) {
        unsigned int to_sleep = time_ms / 1000;
        while ((to_sleep = sleep(to_sleep)) != 0)
            ; /* nothing but loop */
    }
    while (usleep((time_ms % 1000) * 1000) < 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

static void * pr_job(void * vdata) {
    pr_job_data_t * data = (pr_job_data_t *) vdata;
    log_t *         log = data->log;
    unsigned int    i;

    data->tid = pthread_self(); /* only for tests */
    data->pr_job_counter = 0; /* only for tests */
    for (i = 0; i < s_PR_JOB_LOOP_NB; ++i, ++data->pr_job_counter) {
        // pthread_cancel can interrupt logging while file locked, prevent that
        // here for now, and maybe later directly in vlib/log.c: TODO
        vjob_killmode(0, 0, NULL, NULL);
        if (i != data->pr_job_counter) {
            LOG_ERROR(log, "%s(): error bad s_pr_job_counter %u, expected %u",
                    __func__, data->pr_job_counter, i);
            break ;
        }
        LOG_INFO(log, "%s(): #%d", __func__, i);
        vjob_killmode(1, 0, NULL, NULL);
        vsleep_ms(s_PR_JOB_LOOP_SLEEPMS);
    }
    LOG_INFO(log, "%s(): finished.", __func__);
    return (void *)((unsigned long) i);
}

void * test_job(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "JOB");
    log_t *         log  = test != NULL ? test->log : NULL;
    vjob_t *        job = NULL;
    void *          ret;
    unsigned int    state;
    pr_job_data_t   data = { .log = log };

    if (test == NULL) {
        return VOIDP(1);
    }

    LOG_INFO(log, "* run and waitandfree...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        LOG_INFO(log, "waiting...");
        ret = vjob_waitandfree(job);
        LOG_INFO(log, "vjob_waitandfree(): ret %ld", (long)ret);
        TEST_CHECK2(test, "job_counter = %u, got %u, retval %ld",
            data.pr_job_counter == s_PR_JOB_LOOP_NB
              && (unsigned int)((unsigned long)ret) == data.pr_job_counter,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run and wait...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        LOG_INFO(log, "waiting...");
        ret = vjob_wait(job);
        LOG_INFO(log, "vjob_wait(): ret %ld", (long)ret);
        TEST_CHECK2(test, "vjob_state done, got %x",
            ((state = vjob_state(job)) & (VJS_DONE | VJS_INTERRUPTED)) == VJS_DONE, state);

        TEST_CHECK2(test, "vjob_free(): job_counter = %u, got %u, retval %ld",
            vjob_free(job) == ret
                && data.pr_job_counter == s_PR_JOB_LOOP_NB
                && (unsigned int)((unsigned long)ret) == data.pr_job_counter,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run, wait a bit and kill before end...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        vsleep_ms(s_PR_JOB_LOOP_SLEEPMS * 2);
        LOG_INFO(log, "killing...");
        ret = vjob_kill(job);
        TEST_CHECK2(test, "vjob_state INTERRUPTED, got %x",
            ((state = vjob_state(job)) & (VJS_DONE | VJS_INTERRUPTED)) == (VJS_INTERRUPTED), state);

        LOG_INFO(log, "vjob_kill(): ret %ld", (long)ret);
        TEST_CHECK2(test, "job_counter < %u, got %u, retval %ld",
            data.pr_job_counter < s_PR_JOB_LOOP_NB && ret == VJOB_NO_RESULT,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);

        TEST_CHECK2(test, "kill & wait more times same result, got %lu",
            vjob_kill(job) == ret && vjob_wait(job) == ret
                && vjob_kill(job) == ret && vjob_wait(job) == ret, (unsigned long)(ret));

        vjob_free(job);
    }

    LOG_INFO(log, "* run and killandfree without delay...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        ret = vjob_killandfree(job);
        LOG_INFO(log, "vjob_killandfree(): ret %ld", (long)ret);
        TEST_CHECK2(test, "job_counter < %u, got %u, retval %ld",
            data.pr_job_counter < s_PR_JOB_LOOP_NB && ret == VJOB_NO_RESULT,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run and kill without delay...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        ret = vjob_kill(job);
        TEST_CHECK2(test, "vjob_state must be INTERRUPTED, got %x",
            ((state = vjob_state(job)) & (VJS_DONE | VJS_INTERRUPTED)) == (VJS_INTERRUPTED), state);

        LOG_INFO(log, "vjob_kill(): ret %ld", (long)ret);
        TEST_CHECK2(test, "job_counter < %u, got %u, retval %ld",
            vjob_free(job) == ret
                && data.pr_job_counter < s_PR_JOB_LOOP_NB && ret == VJOB_NO_RESULT,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run and free without delay...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        ret = vjob_free(job);
        LOG_INFO(log, "vjob_free(): ret %ld", (long)ret);
        TEST_CHECK2(test, "job_counter < %u, got %u, retval %ld",
            data.pr_job_counter < s_PR_JOB_LOOP_NB && ret == VJOB_NO_RESULT,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run and loop on vjob_done(), then free...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_run", (job = vjob_run(pr_job, &data)) != NULL);
    if (job != NULL) {
        while(!vjob_done(job)) {
            vsleep_ms(10);
        }
        TEST_CHECK2(test, "vjob_state DONE, got %x",
            ((state = vjob_state(job)) & (VJS_DONE | VJS_INTERRUPTED)) == VJS_DONE, state);

        ret = vjob_free(job);
        TEST_CHECK2(test, "job_counter = %u, got %u, retval %ld",
            data.pr_job_counter == s_PR_JOB_LOOP_NB
                && (unsigned int)((unsigned long)ret) == data.pr_job_counter,
            s_PR_JOB_LOOP_NB, data.pr_job_counter, (long)ret);
    }

    LOG_INFO(log, "* run, detach, and wait...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "run + detach", (job = vjob_run(pr_job, &data)) != NULL);
    if (job) {
        TEST_CHECK(test, "vjob_detach", vjob_detach(job) == 0);
        vsleep_ms((s_PR_JOB_LOOP_NB + 2) * s_PR_JOB_LOOP_SLEEPMS);
        TEST_CHECK2(test, "job has finished, counter %u", (data.pr_job_counter == s_PR_JOB_LOOP_NB), data.pr_job_counter);
    }

    LOG_INFO(log, "* run, kill, and detach should fail...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "run + kill, detach", (job = vjob_run(pr_job, &data)) != NULL);
    if (job) {
        vsleep_ms(2 * s_PR_JOB_LOOP_SLEEPMS);
        TEST_CHECK2(test, "vjob_kill ret %lx", (ret = vjob_kill(job)) == VJOB_NO_RESULT, (unsigned long) ret);
        TEST_CHECK(test, "vjob_detach", vjob_detach(job) < 0);
        TEST_CHECK2(test, "job has not finished, counter %u", (data.pr_job_counter < s_PR_JOB_LOOP_NB), data.pr_job_counter);
        TEST_CHECK2(test, "vjob_free ret %lx", (ret = vjob_free(job)) == VJOB_NO_RESULT, (unsigned long) ret);
    }

    LOG_INFO(log, "* run, let it finish, and detach should fail...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "run + finish, detach", (job = vjob_run(pr_job, &data)) != NULL);
    if (job) {
        vsleep_ms((s_PR_JOB_LOOP_NB + 2) * s_PR_JOB_LOOP_SLEEPMS);
        TEST_CHECK(test, "vjob_detach", vjob_detach(job) < 0);
        TEST_CHECK2(test, "job has finished, counter %u", (data.pr_job_counter == s_PR_JOB_LOOP_NB), data.pr_job_counter);
        TEST_CHECK2(test, "vjob_free ret %lx", (ret = vjob_free(job)) == (void *)(unsigned long)data.pr_job_counter, (unsigned long) ret);
    }

    LOG_INFO(log, "* run, wait, and detach should fail...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "run + wait, detach", (job = vjob_run(pr_job, &data)) != NULL);
    if (job) {
        TEST_CHECK2(test, "vjob_wait ret %lx", (ret = vjob_wait(job)) == (void *)(unsigned long) data.pr_job_counter, (unsigned long) ret);
        TEST_CHECK(test, "vjob_detach", vjob_detach(job) < 0);
        TEST_CHECK2(test, "job has finished, counter %u", (data.pr_job_counter == s_PR_JOB_LOOP_NB), data.pr_job_counter);
        TEST_CHECK2(test, "vjob_free ret %lx", (ret = vjob_free(job)) == (void *)(unsigned long) data.pr_job_counter, (unsigned long) ret);
    }

    LOG_INFO(log, "* runandfree, and wait...");
    data.pr_job_counter = 0;
    TEST_CHECK(test, "vjob_runabdfree",  (vjob_runandfree(pr_job, &data) == 0));
    vsleep_ms((s_PR_JOB_LOOP_NB + 2) * s_PR_JOB_LOOP_SLEEPMS);
    TEST_CHECK2(test, "job has finished, counter %u", (data.pr_job_counter == s_PR_JOB_LOOP_NB), data.pr_job_counter);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

