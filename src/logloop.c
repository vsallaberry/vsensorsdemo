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
 * Test program for libvsensors and vlib / log sensors display loop.
 * This code was previously in src/main.c under sensors_watch_loop()
 * function. See git log src/main.c for history.
 */
#include <unistd.h>
#include <sys/select.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include "vlib/log.h"
#include "vlib/time.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "vsensors.h"

/** global running state used by signal handler */
static volatile sig_atomic_t s_running = 1;
/** signal handler */
static void sig_handler(int sig) {
    if (sig == SIGINT)
        s_running = 0;
}

int vsensors_log_loop(
                options_t *     opts,
                sensor_ctx_t *  sctx,
                log_t *         log,
                FILE *          out) {
    /* install timer and signal handlers */
    char        buf[11];
    const int   timer_ms = 500;
    int         sig;
    sigset_t    waitsig;
    struct sigaction sa = { .sa_handler = sig_handler, .sa_flags = SA_RESTART }, sa_bak;
    struct itimerval timer_bak, timer = {
        .it_value =    { .tv_sec = 0, .tv_usec = 1 },
        .it_interval = { .tv_sec = timer_ms / 1000, .tv_usec = (timer_ms % 1000) * 1000 },
    };
    struct timeval elapsed = { .tv_sec = 0, .tv_usec = 0 };
    (void) out;
#   ifdef _DEBUG
    BENCH_DECL(t0);
    BENCH_TM_DECL(tm0);
    BENCH_TM_DECL(tm1);
    long t, tm, t1 = 0;
#   endif

    sigemptyset(&waitsig);
    sigaddset(&waitsig, SIGALRM);
    sigprocmask(SIG_BLOCK, &waitsig, NULL);
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGALRM);
    if (sigaction(SIGINT, &sa, &sa_bak) < 0)
        LOG_ERROR(log, "sigaction(): %s", strerror(errno));
    if (setitimer(ITIMER_REAL, &timer, &timer_bak) < 0) {
        LOG_ERROR(log, "setitimer(): %s", strerror(errno));
    }

    /* watch sensors updates */
    LOG_INFO(log, "sensor_watch_pgcd: %lu", (unsigned long) sensor_watch_pgcd(sctx, NULL, 0));
#   ifdef _DEBUG
    BENCH_TM_START(tm0);
    BENCH_START(t0);
#   endif
    while (s_running && (opts->timeout <= 0 || elapsed.tv_sec * 1000UL + elapsed.tv_usec / 1000UL <= opts->timeout)) {
        /* wait next tick */
        if (sigwait(&waitsig, &sig) < 0)
            LOG_ERROR(log, "sigwait(): %s\n", strerror(errno));

#       ifdef _DEBUG
        BENCH_STOP(t0); t = BENCH_GET_US(t0);
        BENCH_TM_STOP(tm0); tm = BENCH_TM_GET(tm0);
        BENCH_START(t0);
        BENCH_TM_START(tm1);
#       endif

        /* get the list of updated sensors */
        slist_t *updates = sensor_update_get(sctx, &elapsed);

        /* print updates */
        LOG_DEBUG(log,
            "%03ld.%06ld"
            " (abs:%ldms rel:%ldus clk:%ldus)"
            ": updates = %d",
            (long) elapsed.tv_sec, (long) elapsed.tv_usec,
            tm, t1, t,
            slist_length(updates));

        LOG_INFO(log, "sensors updates = %d", slist_length(updates));
        SLIST_FOREACH_DATA(updates, sensor, sensor_sample_t *) {
            sensor_value_tostring(&sensor->value, buf, sizeof(buf) / sizeof(*buf));
            LOG_INFO(log, "  %10s/%s%-25s%s: %s%12s%s", sensor->desc->family->info->name,
                     vterm_color(fileno(log->out), VCOLOR_YELLOW),
                     sensor->desc->label,
                     vterm_color(fileno(log->out), VCOLOR_RESET),
                     vterm_color(fileno(log->out), VCOLOR_GREEN), buf,
                     vterm_color(fileno(log->out), VCOLOR_RESET));
        }

        /* free updates */
        sensor_update_free(updates);

        /* increment elapsed time */
        elapsed.tv_usec += timer_ms * 1000;
        if (elapsed.tv_usec >= 1000000) {
            elapsed.tv_sec += (elapsed.tv_usec / 1000000);
            elapsed.tv_usec %= 1000000;
        }
#       ifdef _DEBUG
        BENCH_TM_STOP(tm1); t1 = BENCH_TM_GET_US(tm1);
#       endif
    }

    LOG_INFO(log, "exiting logloop...");
    /* uninstall timer */
    if (setitimer(ITIMER_REAL, &timer_bak, NULL) < 0) {
        LOG_ERROR(log, "restore setitimer(): %s", strerror(errno));
    }
    /* uninstall signals */
    if (sigaction(SIGINT, &sa_bak, NULL) < 0) {
        LOG_ERROR(log, "restore signals(): %s", strerror(errno));
    }

    return 0;
}

