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
#include "vlib/term.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "vsensors.h"

/*************************************************************************/
#define AUTHOR              "Vincent Sallaberry"
#define COPYRIGHT_DATE      "2017-2020"
#define VERSION_STRING_L    OPT_VERSION_STRING_GPL3PLUS_L(BUILD_APPNAME, APP_VERSION, \
                                "git:" BUILD_GITREV, AUTHOR, COPYRIGHT_DATE)

#define VERSION_STRING      OPT_VERSION_STRING_GPL3PLUS( \
                                BUILD_APPNAME, APP_VERSION, \
                                "git:" BUILD_GITREV, AUTHOR, COPYRIGHT_DATE)

/*************************************************************************/
/** IDs of long options which do not have a corresponding short option */
enum {
    VSO_TIMEOUT             = OPT_ID_USER,
    VSO_SENSOR_TIMER,
    VSO_FALLBACK_DISPLAY
};
/** options array */
static const opt_options_desc_t s_opt_desc[] = {
    { OPT_ID_SECTION, NULL, "gen-opts",  "\nGeneric options:" },
    { 'h', "help",      "[filter[,...]]","summary or full usage of filter, use '-hh'\r" },
    { 'V', "version",   NULL,           "show version"  },
    { 'l', "log-level", "level",        "log level "
                                        "[mod1=]lvl1[@file1][:flag1[|..]][,..]\r" },
    { 'C', "color",     "[yes|no]",     "force colors to 'yes' or 'no'" },
    { 's', "source",    "[[:]pattern]","show source - pattern: <project/file> or :<text>"
                                       " (fnmatch(3) shell pattern)." },
    #ifdef _TEST
    { 'T', "test",      "[test[,...]]", "Perform all (default) or given tests.\rThe next "
                                        "options will be received by test parsing method\r" },
    #endif
    { OPT_ID_SECTION, NULL, "sensors-opts",  "\nSensors options:" },
    { VSO_TIMEOUT, "timeout", "ms",     "exit sensor loop after <ms> milliseconds" },
    { VSO_SENSOR_TIMER, "sensor-timer", "ms","sensor update interval: <ms> milliseconds" },
    { VSO_FALLBACK_DISPLAY, "display-fallback", NULL, "force fallback simple display loop" },
    { 'w', "watch",     "sensor",       "watch a specific sensor with format:<family/name>" },
    { OPT_ID_SECTION+1, NULL, "desc",   "\nDescription:\n"
        "  " BUILD_APPNAME " is a demo program for libvsensors and vlib "
        "but contains also tests for vlib and libvsensors." },
    { OPT_ID_END, NULL, NULL, NULL }
};

/*************************************************************************/
/** parse_option_first_pass() : option callback of type opt_option_callback_t.
 *                              see vlib/options.h */
static int parse_option_first_pass(int opt, const char *arg, int *i_argv,
                                   opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    (void) i_argv;

    switch (opt) {
        case 'l':
            if ((options->logs = logpool_create_from_cmdline(options->logs, arg, NULL)) == NULL)
                options->flags |= FLAG_LOG_OPT_ERROR;
            break ;
        case 'C':
            if (arg == NULL || !strcasecmp(arg, "yes"))
                options->term_flags = VTF_FORCE_COLORS;
            else if (!strcasecmp(arg, "no"))
                options->term_flags = VTF_NO_COLORS;
            else
                options->flags |= FLAG_COLOR_OPT_ERROR;
            break ;
        case OPT_ID_END:
            /* set vlib log instance if given explicitly in command line */
            log_set_vlib_instance(logpool_getlog(options->logs, "vlib", LPG_NODEFAULT));
            /* set options log instance if given explicitly in command line */
            opt_config->log = logpool_getlog(options->logs, "options", LPG_NODEFAULT);
            /* set terminal flags if requested (colors forced) */
            if (options->term_flags != VTF_DEFAULT) {
                vterm_free();
                vterm_init(STDOUT_FILENO, options->term_flags);
            }
            break ;
    }

    return OPT_CONTINUE(1);
}

/*************************************************************************/
/** parse_option() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option(int opt, const char *arg, int *i_argv, opt_config_t * opt_config) {
    static const char * const modules_FIXME[] = {
        BUILD_APPNAME, "vlib", "options",
        "sensors", "cpu", "network", "memory", "disk", "smc", "battery",
        #ifdef _TEST
        "tests",
        #endif
        NULL
    };
    options_t *options = (options_t *) opt_config->user_data;
    log_t * log = logpool_getlog(options->logs, BUILD_APPNAME, LPG_TRUEPREFIX);

    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        /* This is the option dynamic description for opt_usage() */
        switch (opt & OPT_OPTION_FLAG_MASK) {
            #ifdef _TEST
            case 'T':
                return test_describe_filter(opt, arg, i_argv, opt_config);
            #endif
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
        /* error which occured in first pass */
        case 'l':
            if ((options->flags & FLAG_LOG_OPT_ERROR) != 0)
                return OPT_ERROR(OPT_EBADARG);
            break ;
        case 'C':
            if ((options->flags & FLAG_COLOR_OPT_ERROR) != 0)
               return OPT_ERROR(OPT_EBADARG);
            break ;
        /* remaining options */
        case 'h':
            return opt_usage(OPT_EXIT_OK(0), opt_config, arg);
        case 'V':
            fprintf(stdout, "%s\n\nWith:\n   %s\n   %s\n\n",
                    VERSION_STRING_L, libvsensors_get_version(), vlib_get_version());
            return OPT_EXIT_OK(0);
        case 's':
            return opt_filter_source(stdout, arg,
                                     vsensorsdemo_get_source,
                                     vlib_get_source,
                                     libvsensors_get_source,
                                     NULL);
        case 'w':
            LOG_WARN(log, "-w (watch-sensor) NOT implemented, ignoring argument '%s'",
                     arg ? arg : "(null)");
            break ;
        case VSO_FALLBACK_DISPLAY:
            options->flags |= FLAG_FALLBACK_DISPLAY;
            break ;
        case VSO_TIMEOUT: {
            char * end = NULL;
            unsigned long res;
            errno = 0;
            res = strtoul(arg, &end, 0);
            if (end == NULL || *end != 0 || errno != 0)
                return OPT_ERROR(3);
            options->timeout = res;
            break ;
        }
        case VSO_SENSOR_TIMER: {
            char * end = NULL;
            unsigned long res;
            errno = 0;
            res = strtoul(arg, &end, 0);
            if (end == NULL || *end != 0 || errno != 0)
                return OPT_ERROR(3);
            options->sensors_timer = res;
            break ;
        }
        case OPT_ID_ARG:
            return OPT_ERROR(OPT_EOPTARG);
        #ifdef _TEST
        case 'T':
            options->test_mode |= test_getmode(arg);
            if (options->test_mode == 0) {
                return OPT_ERROR(2);
            }
            if (options->test_args_start == 0)
                options->test_args_start = (*i_argv) + 1;
            *i_argv = opt_config->argc; // ignore following options to make them parsed by test()
            break ;
        #endif
    }
    return OPT_CONTINUE(1);
}

static ssize_t log_strings(log_t * log, log_level_t lvl,
                           const char * file, const char * func, int line,
                           const char * strings) {
    const char * token, * next = VERSION_STRING;
    size_t len;
    ssize_t ret = 0;
    FILE * out;

    if (log == NULL || log->out == NULL || strings == NULL)
        return 0;

    out = log_getfile_locked(log);
    while ((len = strtok_ro_r(&token, "\n", &next, NULL, 0)) > 0) {
        ret += log_header(lvl, log, file, func, line);
        ret += fwrite(token, 1, len, out);
        if (fputc('\n', out) != EOF)
            ++ret;
    }
    funlockfile(out);
    return ret;
}

/*************************************************************************/
int main(int argc, const char *const* argv) {
    log_t *         log;
    FILE * const    out         = stdout;
    int             result;
    options_t       options     = {
        .flags = FLAG_NONE, .term_flags = VTF_DEFAULT,
        .timeout = 0, .sensors_timer = 1000,
        .logs = logpool_create()
        #ifdef _TEST
        , .test_mode = 0, .test_args_start = 0
        #endif
    };
    opt_config_t    opt_config  = OPT_INITIALIZER(argc, argv, parse_option_first_pass,
                                                  s_opt_desc, VERSION_STRING, &options);

    /* Manage program options */
    if (OPT_IS_EXIT(result = opt_parse_options_2pass(&opt_config, parse_option))) {
        vterm_enable(0);
        logpool_free(options.logs);
        return OPT_EXIT_CODE(result);
    }

    /* get main module log */
    log = logpool_getlog(options.logs, BUILD_APPNAME, LPG_TRUEPREFIX);
    log_strings(log, LOG_LVL_INFO, __FILE__, __func__, __LINE__, VERSION_STRING);

    #ifdef _TEST
    /* Test entry point, will stop program with -result if result is negative or null. */
    if (options.test_mode != 0
    && (result = test(1+ argc - options.test_args_start, argv + options.test_args_start - 1,
                      options.test_mode, &options.logs)) <= 0) {
        vterm_enable(0);
        logpool_free(options.logs);
        return -result;
    }
    #endif

    LOG_INFO(log, "Starting...");

    /* Init Sensors and get list */
    struct sensor_ctx_s *sctx = sensor_init(options.logs);
    LOG_DEBUG(log, "sensor_init() result: 0x%lx", (unsigned long) sctx);

    slist_t * list = sensor_list_get(sctx);
    LOG_DEBUG(log, "sensor_list_get() result: 0x%lx", (unsigned long) list);

    /* display sensors */
    LOG_INFO(log, "%d sensors available", slist_length(list));
    if (log->level >= LOG_LVL_VERBOSE) {
        SLIST_FOREACH_DATA(list, desc, sensor_desc_t *) {
            const char * fam = desc && desc->family && desc->family->info
                               && desc->family->info->name
                               ? desc->family->info->name : "(null)";
            const char * lab = desc && desc->label ? desc->label : "(null)";
            LOG_VERBOSE(log, "SENSOR %s/%s", fam, lab);
        }
    }

    /* select sensors to watch */
    sensor_watch_t watch = { .update_interval_ms = options.sensors_timer,
                             .callback = NULL, };
    slist_t * watchs = sensor_watch_add(NULL, &watch, sctx);

    /* RUN THE MAIN WATCH LOOP */
    if ((options.flags & FLAG_FALLBACK_DISPLAY) != 0
    || vsensors_screen_loop(&options, sctx, watchs, log, out) != 0)
        vsensors_log_loop(&options, sctx, watchs, log, out);

    /* Free sensor data, logpool and terminal resources */
    sensor_free(sctx);
    vterm_enable(0);
    logpool_free(options.logs);

    return 0;
}

/*************************************************************************/

#ifndef APP_INCLUDE_SOURCE
# define APP_NO_SOURCE_STRING "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
                              BUILD_APPNAME " source not included in this build.\n"
int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
    return vdecode_buffer(out, buffer, buffer_size, ctx,
                          APP_NO_SOURCE_STRING, sizeof(APP_NO_SOURCE_STRING) - 1);
}
#endif

/*************************************************************************/

