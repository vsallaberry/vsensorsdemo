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
#ifdef _TEST
# include "vlib/test.h"
#endif

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
    /* -------------------------------------------------------------------- */
    { OPT_ID_SECTION, NULL, "gen-opts", "\nGeneric options:" },
    OPT_DESC_HELP       ('h',"help"),
    OPT_DESC_VERSION    ('V',"version"),
    OPT_DESC_LOGLEVEL   ('l',"log-level"),
    OPT_DESC_COLOR      ('C',"color"),
    OPT_DESC_SOURCE     ('s', "source"),
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
/** parse_option() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option(int opt, const char *arg, int *i_argv, opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    log_t * log = logpool_getlog(options->logs, BUILD_APPNAME, LPG_TRUEPREFIX);

    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        /* This is the option dynamic description for opt_usage() */
        switch (opt & OPT_OPTION_FLAG_MASK) {
            #ifdef _TEST
            case 'T':
                return test_describe_filter(opt, arg, i_argv, opt_config);
            #else
            (void) i_argv;
            #endif
        }
        return OPT_EXIT_OK(0);
    }
    /* This is the option parsing */
    switch (opt) {
        /* generic options managed by vlib (opt_parse_generic_pass_*()) */
        case 'V':
            fprintf(stdout, "%s\n\nWith:\n   %s\n   %s\n\n",
                    VERSION_STRING_L, libvsensors_get_version(), vlib_get_version());
            return OPT_EXIT_OK(0);
        case 's':
            opt_filter_source(stdout, arg, vsensorsdemo_get_source,
                              vlib_get_source, libvsensors_get_source, NULL);
            return OPT_EXIT_OK(0);
        /* ---------------------------
         * vsensorsdemo other options: */
        case 'w':
            LOG_WARN(log, "-w (watch-sensor) NOT implemented, ignoring argument '%s'",
                     arg ? arg : "(null)");
            break ;
        case VSO_FALLBACK_DISPLAY:
            options->flags |= FLAG_FALLBACK_DISPLAY;
            break ;
        case VSO_TIMEOUT:
            if (vstrtoul(arg, NULL, 0, &options->timeout) != 0)
                return OPT_ERROR(3);
            break ;
        case VSO_SENSOR_TIMER:
            if (vstrtoul(arg, NULL, 0, &options->sensors_timer) != 0)
                return OPT_ERROR(3);
            break ;
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

/*************************************************************************/
int main(int argc, const char *const* argv) {
    log_t *         log;
    FILE * const    out         = stdout;
    int             result;
    options_t       options     = {
        .flags = FLAG_NONE,
        .timeout = 0, .sensors_timer = 1000,
        .logs = logpool_create(),
        .version_string = VERSION_STRING
        #ifdef _TEST
        , .test_mode = 0, .test_args_start = 0
        #endif
    };
    opt_config_t    opt_config  = OPT_INITIALIZER(argc, argv, parse_option,
                                                  s_opt_desc, VERSION_STRING, &options);

    static const char * const modules_FIXME[] = {
        BUILD_APPNAME, LOG_VLIB_PREFIX_DEFAULT, LOG_OPTIONS_PREFIX_DEFAULT,
        SENSOR_LOG_PREFIX, "cpu", "network", "memory", "disk", "smc", "battery",
        #ifdef _TEST
        TESTPOOL_LOG_PREFIX, "<test-name>",
        #endif
        NULL
    };

    /* Manage program options */
    if (OPT_IS_EXIT(result = opt_parse_generic(&opt_config, NULL, &options.logs, modules_FIXME))) {
        vterm_enable(0);
        logpool_free(options.logs);
        return OPT_EXIT_CODE(result);
    }

    /* get main module log */
    log = logpool_getlog(options.logs, BUILD_APPNAME, LPG_TRUEPREFIX);
    result = log->level; log->level = LOG_LVL_NB;
    vlog_strings(LOG_LVL_INFO, log, __FILE__, __func__, __LINE__, VERSION_STRING);
    log->level = result;

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
    LOG_INFO(log, "exiting...");
    sensor_free(sctx);
    vterm_enable(0);
    logpool_free(options.logs);

    return 0;
}

/*************************************************************************/

#ifndef APP_INCLUDE_SOURCE
static s_app_no_source_string
    = "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
      BUILD_APPNAME " source not included in this build.\n";

int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
    return vdecode_buffer(out, buffer, buffer_size, ctx,
                          s_app_no_source_string, strlen(s_app_no_source_string));
}
#endif

/*************************************************************************/

