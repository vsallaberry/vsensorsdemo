/*
 * Copyright (C) 2017-2019 Vincent Sallaberry
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
 * Test program for libvsensors and vlib.
 */
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "vlib/log.h"
#include "vlib/logpool.h"
#include "vlib/options.h"
#include "vlib/util.h"
#include "vlib/time.h"
#include "vlib/thread.h"

#include "libvsensors/sensor.h"

#include "version.h"

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS(BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2019")

static const opt_options_desc_t s_opt_desc[] = {
    { OPT_ID_SECTION, NULL, "options",  "Options:" },
    { 'h', "help",      "[filter[,...]]","show usage\n" },
    { 'V', "version",   NULL,           "show version"  },
    { 'l', "log-level", "level",        "NOT_IMPLEMENTED - "
                                        "Set log level [module1=]level1[@file1][,...]." },
	{ 's', "source",    "[project/file]","show source (shell pattern allowed, including '**')." },
#   ifdef _TEST
    { 'T', "test",      "[test[,...]]", "Perform tests. The next options will "
                                        "be received by test parsing method." },
#   endif
	{ OPT_ID_END, NULL, NULL, NULL }
};

enum FLAGS {
    FLAG_NONE = 0,
};

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    logpool_t *     logs;
} options_t;

#ifdef _TEST
int                         test_describe_filter(int short_opt, const char * arg, int * i_argv,
                                                 const opt_config_t * opt_config);
unsigned int                test_getmode(const char *arg);
int                         test(int argc, const char *const* argv, unsigned int test_mode);
#endif

/** parse_option_first_pass() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option_first_pass(int opt, const char *arg, int *i_argv,
                                   const opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    (void) i_argv;

    switch (opt) {
        case 'l':
            if ((options->logs = logpool_create_from_cmdline(options->logs, arg, NULL)) == NULL)
                return OPT_ERROR(OPT_EBADARG);
            break ;
        case OPT_ID_END:
            break ;
    }

    return OPT_CONTINUE(1);
}

int opt_filter_source(FILE * out, const char * arg, ...);
#ifdef _TEST
void opt_set_source_filter_bufsz(size_t bufsz);
#endif

/** parse_option() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    static const char * const modules_FIXME[] = { "main", "vlib", "cpu", "network", NULL };
    options_t *options = (options_t *) opt_config->user_data;

    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        /* This is the option dynamic description for opt_usage() */
        switch (opt & OPT_OPTION_FLAG_MASK) {
#           ifdef _TEST
            case 'T':
                return test_describe_filter(opt, arg, i_argv, opt_config);
#           endif
            case 'l':
                return log_describe_option((char *)arg, i_argv, modules_FIXME, NULL, NULL);
            case 'h':
                return opt_describe_filter(opt, arg, i_argv, opt_config);
            default:
                return OPT_EXIT_OK(0);
        }
        return OPT_CONTINUE(1);
    }
    /* This is the option parsing */
    switch (opt) {
        case 'h':
            return opt_usage(OPT_EXIT_OK(0), opt_config, arg);
        case 'V':
            fprintf(stdout, "%s\n\nWith:\n   %s\n   %s\n\n",
                    opt_config->version_string, libvsensors_get_version(), vlib_get_version());
            return OPT_EXIT_OK(0);
        case 's':
            return opt_filter_source(stdout, arg,
                                     vsensorsdemo_get_source,
                                     vlib_get_source,
                                     libvsensors_get_source,
                                     NULL);
#       ifdef _TEST
        case 'T':
            options->test_mode |= test_getmode(arg);
            if (options->test_mode == 0) {
                return OPT_ERROR(2);
            }
            *i_argv = opt_config->argc; // ignore following options to make them parsed by test()
            break ;
#       endif
    }
    return OPT_CONTINUE(1);
}

static int sensors_watch_loop(options_t * opts, sensor_ctx_t * sctx, log_t * log, FILE * out);

int main(int argc, const char *const* argv) {
    log_t *         log         = log_create(NULL);
    options_t       options     = { .flags = FLAG_NONE, .test_mode = 0,
                                    .logs = logpool_create() };
    opt_config_t    opt_config  = { argc, argv, parse_option_first_pass, s_opt_desc,
                                    OPT_FLAG_DEFAULT, VERSION_STRING, &options, NULL }; //log };
    FILE * const    out         = stdout;
    int             result;

    // TODO : temporarily add log to logpool
    logpool_add(options.logs, log);

    /* Manage program options */
    if (OPT_IS_EXIT(result = opt_parse_options_2pass(&opt_config, parse_option))) {
        logpool_free(options.logs);
        return OPT_EXIT_CODE(result);
    }

    vlib_thread_valgrind(argc, argv);

#   ifdef _TEST
    /* Test entry point, will stop program with -result if result is negative or null. */
    if (options.test_mode != 0 && (result = test(argc, argv, options.test_mode)) <= 0) {
        logpool_free(options.logs);
        return -result;
    }
#   endif

    /* get main module log */
    LOG_INFO(log, "Starting...");

    /* Init Sensors and get list */
    struct sensor_ctx_s *sctx = sensor_init(NULL);
    LOG_DEBUG(log, "sensor_init() result: 0x%lx", (unsigned long) sctx);

    slist_t * list = sensor_list_get(sctx);
    LOG_DEBUG(log, "sensor_list_get() result: 0x%lx", (unsigned long) list);

    /* display sensors */
    for (slist_t *elt = list; elt; elt = elt->next) {
        sensor_desc_t * desc = (sensor_desc_t *) elt->data;
        fprintf(out, "%s\n", desc->label);
    }

    /* select sensors to watch */
    sensor_watch_t watch = { .update_interval_ms = 1000, .callback = NULL, };
    sensor_watch_add(NULL, &watch, sctx);

    /* RUN THE MAIN WATCH LOOP */
    sensors_watch_loop(&options, sctx, log, out);

    /* Free sensor data */
    sensor_free(sctx);
    logpool_free(options.logs);

    return 0;
}

/** global running state used by signal handler */
static volatile sig_atomic_t s_running = 1;
/** signal handler */
static void sig_handler(int sig) {
    if (sig == SIGINT)
        s_running = 0;
}

static int sensors_watch_loop(options_t * opts, sensor_ctx_t * sctx, log_t * log, FILE * out) {
    /* install timer and signal handlers */
    char        buf[11];
    const int   timer_ms = 500;
    const int   max_time_sec = 60;
    int         sig;
    sigset_t    waitsig;
    struct sigaction sa = { .sa_handler = sig_handler, .sa_flags = SA_RESTART }, sa_bak;
    struct itimerval timer_bak, timer = {
        .it_value =    { .tv_sec = 0, .tv_usec = 1 },
        .it_interval = { .tv_sec = timer_ms / 1000, .tv_usec = (timer_ms % 1000) * 1000 },
    };
    struct timeval elapsed = { .tv_sec = 0, .tv_usec = 0 };
    (void) opts;
    (void) out;
#   ifdef _TEST
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
    LOG_INFO(log, "sensor_watch_pgcd: %lu", sensor_watch_pgcd(sctx));
#   ifdef _TEST
    BENCH_TM_START(tm0);
    BENCH_START(t0);
#   endif
    while (elapsed.tv_sec < max_time_sec && s_running) {
        /* wait next tick */
        if (sigwait(&waitsig, &sig) < 0)
            LOG_ERROR(log, "sigwait(): %s\n", strerror(errno));

#       ifdef _TEST
        BENCH_STOP(t0); t = BENCH_GET_US(t0);
        BENCH_TM_STOP(tm0); tm = BENCH_TM_GET(tm0);
        BENCH_START(t0);
        BENCH_TM_START(tm1);
#       endif

        /* get the list of updated sensors */
        slist_t *updates = sensor_update_get(sctx, &elapsed);

        /* print updates */
        printf("%03ld.%06ld", (long) elapsed.tv_sec, (long) elapsed.tv_usec);
#       ifdef _TEST
        printf(" (abs:%ldms rel:%ldus clk:%ldus)", tm, t1, t);
#       endif
        printf(": updates = %d", slist_length(updates));
        SLIST_FOREACH_DATA(updates, sensor, sensor_sample_t *) {
            sensor_value_tostring(&sensor->value, buf, sizeof(buf));
            printf(" %s:%s", sensor->desc->label, buf);
        }
        printf("\n");

        /* free updates */
        sensor_update_free(updates);

        /* increment elapsed time */
        elapsed.tv_usec += timer_ms * 1000;
        if (elapsed.tv_usec >= 1000000) {
            elapsed.tv_sec += (elapsed.tv_usec / 1000000);
            elapsed.tv_usec %= 1000000;
        }
#       ifdef _TEST
        BENCH_TM_STOP(tm1); t1 = BENCH_TM_GET_US(tm1);
#       endif
    }

    LOG_INFO(log, "exiting...");
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

#ifndef APP_INCLUDE_SOURCE
# define APP_NO_SOURCE_STRING "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
                              BUILD_APPNAME " source not included in this build.\n"
int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
    return vdecode_buffer(out, buffer, buffer_size, ctx,
                          APP_NO_SOURCE_STRING, sizeof(APP_NO_SOURCE_STRING) - 1);
}
#endif

typedef int     (*opt_getsource_t)(FILE *, char *, unsigned, void **);

#include <stdarg.h>
#include <fnmatch.h>

#define FILE_PATTERN    "\n/* #@@# FILE #@@# "

#define OPT_FILTER_BUFSZ_DEFAULT (sizeof(FILE_PATTERN) - 1 + PATH_MAX + 5)
#ifdef _TEST
static size_t s_opt_filter_bufsz = OPT_FILTER_BUFSZ_DEFAULT;
void opt_set_source_filter_bufsz(size_t bufsz) {
    s_opt_filter_bufsz = bufsz ? bufsz : OPT_FILTER_BUFSZ_DEFAULT;
}
#else
static const size_t s_opt_filter_bufsz = OPT_FILTER_BUFSZ_DEFAULT;
#endif

int opt_filter_source(FILE * out, const char * arg, ...) {
    va_list         valist;
    opt_getsource_t getsource;

    va_start(valist, arg);
    if (arg == NULL) {
        while ((getsource = va_arg(valist, opt_getsource_t)) != NULL) {
            getsource(out, NULL, 0, NULL);
        }
        va_end(valist);
        return OPT_EXIT_OK(0);
    }

    void *              ctx             = NULL;
    const char *        search          = FILE_PATTERN;
    size_t              filtersz        = sizeof(FILE_PATTERN) - 1;
    const size_t        bufsz           = s_opt_filter_bufsz;
    char                buffer[bufsz];
    char *              pattern;
    size_t              patlen          = strlen(arg);
    int                 fnm_flag        = FNM_CASEFOLD;

    /* handle search in source content rather than on file names */
    if (*arg == ':') {
        search = "\n";
        filtersz = 1;
        --patlen;
        pattern = strdup(arg + 1);
    } else {
        /* build search pattern */
        if ((pattern = malloc(sizeof(char) * (patlen + 10))) == NULL) {
            return OPT_ERROR(1);
        }
        strcpy(pattern, arg);
        strcpy(pattern + patlen, " \\*/\n*");
        if (strchr(arg, '/') != NULL && strstr(arg, "**") == NULL) {
            fnm_flag |= FNM_PATHNAME | FNM_LEADING_DIR;
        }
    }

    while ((getsource = va_arg(valist, opt_getsource_t)) != NULL) {
        ssize_t n, n_sav = 1;
        size_t  bufoff = 0;
        int     found = 0;

        while (n_sav > 0 && (n = n_sav = getsource(NULL, buffer + bufoff,
                                                   bufsz - 1 - bufoff, &ctx)) >= 0) {
            char *  newfile;
            char *  bufptr = buffer;

            n += bufoff;
            buffer[n] = 0;
            do {
                if ((newfile = strstr(bufptr, search)) != NULL) {
                    /* FILE PATTERN found */
                    /* write if needed bytes preceding the file pattern, then skip them */
                    if (found && newfile > bufptr) {
                        fwrite(bufptr, sizeof(char), newfile - bufptr, out);
                    }
                    n -= (newfile - bufptr);
                    bufptr = newfile;
                    newfile += filtersz;
                    /* checks whether PATH_MAX fits in current buffer position */
                    if (newfile + PATH_MAX > buffer + bufsz - 1
                    &&  n_sav > 0 && strchr(newfile, '\n') == NULL) {
                        /* shift pattern at beginning of buffer to catch truncated files */
                        memmove(buffer, bufptr, n);
                        bufoff = n;
                        break ;
                    }
                    bufoff = 0;
                    char * t = NULL;
                    if (*arg == ':' && (t = strchr(newfile, '\n')) != NULL) *t = 0;
                    found = (fnmatch(pattern, newfile, fnm_flag) == 0);
                    if (*arg == ':' && t != NULL) *t = '\n';
                } else if (n_sav > 0 && filtersz > 0) {
                    /* FILE PATTERN not found */
                    /* shift filtersz-1 last bytes to start of buffer to get truncated patterns */
                    bufoff = (size_t) n >= filtersz - 1 ? filtersz - 1 : n;
                    n -= bufoff;
                } else
                    bufoff = 0;
                if (found) {
                    fwrite(bufptr, sizeof(char), newfile ? newfile - bufptr : n, out);
                }
                if (newfile)
                    n -= (newfile - bufptr);
                else
                    memmove(buffer, bufptr + n, bufoff);
                bufptr = newfile;
            } while (newfile != NULL);
        }
        /* release resources */
        getsource(NULL, NULL, 0, &ctx);
        if (ctx != NULL) {
            LOG_ERROR(NULL, "error: ctx after vdecode_buffer should be NULL");
        }
    }
    va_end(valist);
    free(pattern);
    return OPT_EXIT_OK(0);
}

