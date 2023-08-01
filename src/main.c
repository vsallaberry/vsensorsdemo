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
 * Test program for libvsensors and vlib.
 */
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>

#include "vlib/log.h"
#include "vlib/logpool.h"
#include "vlib/options.h"
#include "vlib/util.h"
#include "vlib/time.h"
#include "vlib/term.h"

#include "libvsensors/sensor.h"

#ifdef _TEST
# include "test/test.h"
#endif

#include "version.h"
#include "vsensors.h"

/*************************************************************************/
#define AUTHOR              "Vincent Sallaberry"
#define COPYRIGHT_DATE      "2017-2020,2023"
#define VERSION_STRING_L    OPT_VERSION_STRING_GPL3PLUS_L( \
                                "%s" BUILD_APPNAME, APP_VERSION "%s", \
                                "git:" BUILD_GITREV, AUTHOR, COPYRIGHT_DATE)

#define VERSION_STRING      OPT_VERSION_STRING_GPL3PLUS( \
                                "%s" BUILD_APPNAME, APP_VERSION "%s", \
                                "git:" BUILD_GITREV, AUTHOR, COPYRIGHT_DATE)

/*************************************************************************/
/** IDs of long options which do not have a corresponding short option */
enum {
    VSO_TIMEOUT             = OPT_ID_USER,
    VSO_FALLBACK_DISPLAY,
};
/** options array */
static const opt_options_desc_t s_opt_desc[] = {
    /* -------------------------------------------------------------------- */
    { OPT_ID_SECTION, NULL, "gen-opts", "\nGeneric options:" },
    OPT_DESC_HELP       ('h',"help"),
    OPT_DESC_VERSION    ('V',"version"),
    OPT_DESC_LOGLEVEL   ('L',"log-level"),
    OPT_DESC_COLOR      ('C',"color"),
    OPT_DESC_SOURCE     ('S', "source"),
    #ifdef _TEST
    { 'T', "test",      "[test[,...]]", "Perform all (default) or given tests(~ to exclude).\r"
                                        "The next options will be received by test parsing method\r" },
    #endif
    /* -------------------------------------------------------------------- */
    { OPT_ID_SECTION+1, NULL, "sensors-opts", "\nSensors options:" },
    { 'l', "list-sensors", NULL,    "list sensors and exit" },
    { 'p', "print-sensors", NULL,   "print sensors values and exit(can be used with -w)" },
    { 't',                  "sensor-timer", "ms",
                            "sensor update interval: <ms> milliseconds" },
    { 'w', "watch", "pattern",
           "watch a specific sensor with format:<family/name>. fnmatch(3) pattern. \r"
           "Only Watch parameters (eg: timer) preceding this option are applied. "
           "This option can be used several times." },
    { 'b', "watch-bar", "[d]:watch",
           "add a watch '[desc]:<family>/label' to statusbar "
           "(default Temp|Fan|mem%|cpus% - same behavior than --watch option)." },
    { 'W', "write", "pattern=value", "write a sensor value if supported by the sensor" },
    { 'O', "only-watched", NULL,
                            "only watched sensors can be added to status-bar." },
    { VSO_TIMEOUT,          "timeout", "ms",
                            "exit sensor loop after <ms> milliseconds" },
    { VSO_FALLBACK_DISPLAY, "display-fallback", NULL,
                            "force fallback simple display loop" },
    /* -------------------------------------------------------------------- */
    { OPT_ID_SECTION+2, NULL, "desc",
        "\nDescription:\n" "  " BUILD_APPNAME " is a demo program for libvsensors and vlib "
        "but contains also tests for vlib and libvsensors." },
    /* -------------------------------------------------------------------- */
    { OPT_ID_END, NULL, NULL, NULL }
};

/*************************************************************************/
/** struct storing --watch options datas */
typedef struct {
    const char *        pattern;
    sensor_watch_t      watch;
} vsensors_watcharg_t;

/*************************************************************************/
/** parse_option() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option(int opt, const char *arg, int *i_argv, opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;

    if (opt == OPT_ID_START && *(options->version_string) == 0) {
        int fd = opt_config->log != NULL && opt_config->log->out != NULL
                 ? fileno(opt_config->log->out) : STDOUT_FILENO;
        /* do this now and not before in order to not initialize terminal earlier */
        snprintf(options->version_string, PTR_COUNT(options->version_string),
                 VERSION_STRING, vterm_color(fd, VCOLOR_BOLD), vterm_color(fd, VCOLOR_RESET));
        return OPT_CONTINUE(1);
    }
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
        /* custom actions for generic options: */
        case 'V':
            fprintf(stdout, VERSION_STRING_L "\n\nWith:\n   %s\n   %s\n\n",
                    vterm_color(STDOUT_FILENO, VCOLOR_BOLD),
                    vterm_color(STDOUT_FILENO, VCOLOR_RESET),
                    libvsensors_get_version(), vlib_get_version());
            return OPT_EXIT_OK(0);
        case 'S':
            opt_filter_source(stdout, arg, vsensorsdemo_get_source,
                              vlib_get_source, libvsensors_get_source,
#                            ifdef _TEST
                              test_vsensorsdemo_get_source,
#                            endif
                              NULL);
            return OPT_EXIT_OK(0);
        /* ---------------------------
         * vsensorsdemo other options: */
        case 'w': {
            vsensors_watcharg_t * warg = calloc(1, sizeof(vsensors_watcharg_t));
            warg->pattern = arg;
            warg->watch = SENSOR_WATCH_INITIALIZER(options->sensors_timer, NULL);
            options->watchs.head = slist_appendto(options->watchs.head,
                                                  warg, &(options->watchs.tail));
            break ;
        }
        case 'W': {
            options->writes.head = slist_appendto(options->writes.head,
                                                  (char *) arg, &(options->writes.tail));
            break ;
        }
        case VSO_FALLBACK_DISPLAY:
            options->flags |= FLAG_FALLBACK_DISPLAY;
            break ;
        case VSO_TIMEOUT:
            if (vstrtoul(arg, NULL, 0, &options->timeout) != 0)
                return OPT_ERROR(3);
            break ;
        case 'O':
            options->flags |= FLAG_SB_ONLYWATCHED;
            break ;
        case 'b':
            if (arg == NULL || strchr(arg, ':') == NULL) {
                return OPT_ERROR(3);
            }
            options->sb_watchs.head
                = slist_appendto(options->sb_watchs.head,
                                 (void *) arg, &(options->sb_watchs.tail));
            break ;
        case 't':
            if (vstrtoul(arg, NULL, 0, &options->sensors_timer) != 0)
                return OPT_ERROR(3);
            break ;
        case 'p':
            options->flags |= FLAG_SENSOR_PRINT;
            break ;
        case 'l':
            options->flags |= FLAG_SENSOR_LIST;
            break ;
        case OPT_ID_ARG:
            return OPT_ERROR(OPT_EOPTARG);
        /* --------------------------- */
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
static int vsensors_free(int exit_status, options_t * opts, log_t * log,
                         sensor_ctx_t * sctx) {

    if (log != NULL) {
        LOG_INFO(log, "exiting...");
    }
    sensor_free(sctx);
    slist_free(opts->watchs.head, NULL);
    slist_free(opts->sb_watchs.head, NULL);
    slist_free(opts->writes.head, NULL);
    vterm_enable(0);
    logpool_free(opts->logs);

    return exit_status;
}

/*************************************************************************/
static int vsensors_list(options_t * opts, log_t * log, sensor_ctx_t * sctx) {
    FILE *          out = stdout;
    char            propval[128];
    int             n;
    #ifndef _DEBUF
    (void) log;
    #endif

    sensor_lock(sctx, SENSOR_LOCK_READ);
    SLISTC_FOREACH_DATA(sensor_list_get(sctx), desc, sensor_desc_t *) {
        const char * fam = desc->family->info->name;
        const char * lab = STR_CHECKNULL(desc->label);

        if ((opts->flags & FLAG_SENSOR_LIST) == 0) {
            LOG_DEBUG(log, "SENSOR %s/%s", fam, lab);
            continue ;
        }
        n = fprintf(out, "%s/%s", fam, lab);
        while (n > 0 && n++ < 40)
            fputc(' ', out);
        fprintf(out, " type:%s", sensor_value_type_name(desc->type));
        for (sensor_property_t * property = desc->properties;
                SENSOR_PROPERTY_VALID(property); ++property) {
            sensor_value_tostring(&(property->value), propval, sizeof(propval)/sizeof(*propval));
            fprintf(out, " %s:%s", property->name, propval);
        }
        fputc('\n', out);
    }
    sensor_unlock(sctx);

    return 0;
}

/*************************************************************************/
static int vsensors_print(options_t * opts, log_t * log, sensor_ctx_t * sctx) {
    FILE *  out = stdout;
    char    value[128];
    int     n;
    (void)  opts;
    (void)  log;

    sensor_lock(sctx, SENSOR_LOCK_READ);
    SLISTC_FOREACH_DATA(sensor_watch_list_get(sctx), sample, sensor_sample_t *) {
        sensor_update_check(sample, NULL);
        sensor_value_tostring(&(sample->value), value, sizeof(value)/sizeof(*value));
        n = fprintf(out, "%s/%s ", sample->desc->family->info->name,
                    STR_CHECKNULL(sample->desc->label));
        while (n >= 0 && n++ < 40) {
            fputc(' ', out);
        }
        fprintf(out, " %s\n", value);
    }
    sensor_unlock(sctx);

    return 0;
}

/*************************************************************************/
typedef struct {
    sensor_ctx_t *  sctx;
    log_t *         log;
    options_t *     options;
    sensor_value_t  value;
    size_t          nerror;
    size_t          nok;    
} vsensors_write_data_t;

static sensor_status_t vsensors_write_visit(const sensor_desc_t * sensor, void * vdata) {
    vsensors_write_data_t * data = (vsensors_write_data_t *) vdata;
    int fd = data->log && data->log->out && (data->log->flags & LOG_FLAG_COLOR) != 0 ? fileno(data->log->out) : -1;

    if (sensor == NULL) {
        return SENSOR_SUCCESS;
    }    
    if (sensor->family->info->write != NULL) {
        if (sensor->family->info->write(sensor, &data->value) == SENSOR_SUCCESS) {
            LOG_INFO(data->log, "%sWrite successful%s: '%s%s%s' = '%s'",
                     vterm_color(fd, VCOLOR_GREEN), vterm_color(fd, VCOLOR_RESET),
                     vterm_color(fd, VCOLOR_YELLOW), sensor->label,
                     vterm_color(fd, VCOLOR_RESET), data->value.data.b.buf);
            ++data->nok;
        } else {
            LOG_ERROR(data->log, "%sWrite failed%s: '%s%s%s' != '%s' (%s)",
                      vterm_color(fd, VCOLOR_RED), vterm_color(fd, VCOLOR_RESET),
                      vterm_color(fd, VCOLOR_YELLOW), sensor->label, 
                      vterm_color(fd, VCOLOR_RESET), data->value.data.b.buf, strerror(errno));
            ++data->nerror;
        }
    } else {
        LOG_ERROR(data->log, "%swrite is not supported%s by the sensor '%s%s%s'",
                  vterm_color(fd, VCOLOR_YELLOW), vterm_color(fd, VCOLOR_RESET),
                  vterm_color(fd, VCOLOR_YELLOW),
                  sensor ? sensor->label : "(null)", vterm_color(fd, VCOLOR_RESET));
        ++data->nerror;
    }
    return SENSOR_SUCCESS;
}

static int vsensors_write(options_t * opts, log_t * log, sensor_ctx_t * sctx) {
    char                    pattern[128];
    vsensors_write_data_t   data = { .sctx = sctx, .log = log, .options=opts, 
                                     .nerror = 0, .nok = 0 };

    pattern[sizeof(pattern)-1] = 0;

    sensor_lock(sctx, SENSOR_LOCK_WRITE);
    SLISTC_FOREACH_DATA(opts->writes.head, warg, char *) {
        char * equal = strchr(warg, '=');
        size_t len, saved_nerror;
        sensor_watch_t watch = SENSOR_WATCH_INITIALIZER(0, NULL);
        
        if (equal == NULL) {
            LOG_WARN(log, "bad write argument '%s', missing '='", warg);
            continue ;
        }
        len = equal - warg;
        if (len > sizeof(pattern)-1)
            len = sizeof(pattern)-1;
        strncpy(pattern, warg, len);
        pattern[len] = 0;
        ++equal;
        SENSOR_VALUE_INIT_STR(data.value, equal);
        LOG_VERBOSE(log, "WRITE: '%s' = '%s'", pattern, equal);

        data.nok = 0;
        saved_nerror = data.nerror;

        /*FIXME not pretty */ sensor_watch_add(sctx, pattern, SSF_DEFAULT, &watch);
                              sensor_init_wait(sctx, 1); /* ! FIXME not pretty */
        sensor_visit(sctx, pattern, SSF_DEFAULT, vsensors_write_visit, &data);

        if (data.nok == 0 && data.nerror == saved_nerror) {
            LOG_ERROR(log, "no sensor could satisfy the pattern '%s'", pattern);
            ++data.nerror;
        }
    }
    sensor_unlock(sctx);

    return data.nerror;
}

/*************************************************************************/
int main(int argc, const char *const* argv) {
    log_t *         log         = NULL;
    FILE * const    out         = stdout;
    sensor_ctx_t *  sctx        = NULL;
    int             result;
    options_t       options     = {
        .flags = FLAG_NONE,
        .timeout = 0, .sensors_timer = 1000,
        .watchs = SHLIST_INITIALIZER(), .sb_watchs = SHLIST_INITIALIZER(),
        .writes = SHLIST_INITIALIZER(),
        .logs = logpool_create(), .version_string = { 0, }
        #ifdef _TEST
        , .test_mode = 0, .test_args_start = 0
        #endif
    };
    opt_config_t    opt_config  = OPT_INITIALIZER(argc, argv, parse_option,
                                    s_opt_desc, options.version_string, &options);

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
        return vsensors_free(OPT_EXIT_CODE(result), &options, log, NULL);
    }

    /* get main module log */
    log = logpool_getlog(options.logs, BUILD_APPNAME, LPG_TRUEPREFIX);
    result = log->level; log->level = LOG_LVL_NB;
    vlog_strings(LOG_LVL_INFO, log, __FILE__, __func__, __LINE__, VERSION_STRING, "", "");
    log->level = result;

    #ifdef _TEST
    /* Test entry point, will stop program with -result if result is negative or null. */
    if (options.test_mode != 0
    && (result = test(1+ argc - options.test_args_start, argv + options.test_args_start - 1,
                      options.test_mode, &options.logs)) <= 0) {
        return vsensors_free(-result, &options, log, NULL);
    }
    #endif

    LOG_INFO(log, "Starting...");

    /* Init Sensors */
    sctx = sensor_init(options.logs, SIF_DEFAULT);
    LOG_DEBUG(log, "sensor_init() result: 0x%lx", (unsigned long) sctx);
    if (sctx == NULL) {
        LOG_ERROR(log, "cannot initialize sensors");
        return vsensors_free(1, &options, log, sctx);
    }

    if ((options.flags & (FLAG_SENSOR_LIST)) == 0
    &&  options.writes.head != NULL && options.watchs.head == NULL) {
    
    }
    
    /* select sensors to watch */
    if ((options.writes.head == NULL)// && (options.flags & (FLAG_SENSOR_LIST)) == 0)
    ||  (options.flags & FLAG_SENSOR_PRINT) != 0) {
        sensor_lock(sctx, SENSOR_LOCK_WRITE);
        if (options.watchs.head == NULL) {
            /* watch all sensors */
            sensor_watch_t watch;
            watch = SENSOR_WATCH_INITIALIZER(options.sensors_timer, NULL);
            sensor_watch_add_desc(sctx, NULL, SSF_DEFAULT, &watch);
        } else {
            /* watch sensors given in the command line */
            SLIST_FOREACH_DATA(options.watchs.head, warg, vsensors_watcharg_t *) {
                if (sensor_watch_add(sctx, warg->pattern,
                                     SSF_DEFAULT, &(warg->watch)) != SENSOR_SUCCESS) {
                    LOG_WARN(log, "no match for watch '%s'", warg->pattern);
                } else {
                    LOG_VERBOSE(log, "added watch %s (%lu.%03lus)", warg->pattern,
                                (unsigned long) warg->watch.update_interval.tv_sec,
                                (unsigned long) warg->watch.update_interval.tv_usec/1000);
                }

            }
            slist_free(options.watchs.head, free);
            options.watchs = SHLIST_INITIALIZER();
        }
        result = slist_length(sensor_watch_list_get(sctx));
        sensor_unlock(sctx);
        LOG_INFO(log, "%d sensor%s in watch list", result, result > 1 ? "s" : "");
    }

    /* if listing, printing or write requested, wait until all is loaded */
    if ((options.flags & (FLAG_SENSOR_LIST | FLAG_SENSOR_PRINT)) != 0
    ||  options.writes.head != NULL) {
        sensor_init_wait(sctx, (options.flags & FLAG_SENSOR_LIST) == 0);
    }

    int ret = 0;
    const slist_t * list = sensor_list_get(sctx);
    LOG_DEBUG(log, "sensor_list_get() result: 0x%lx", (unsigned long) list);
    LOG_INFO(log, "%d sensors available", slist_length(list));

    /* list supported sensors if requested */
    if ((options.flags & FLAG_SENSOR_LIST) != 0 || LOG_CAN_LOG(log, LOG_LVL_DEBUG)) {
        ret |= vsensors_list(&options, log, sctx);
    }
    
    /* Print sensors values if requested */
    if ((options.flags & FLAG_SENSOR_PRINT) != 0) {
        ret |= vsensors_print(&options, log, sctx);
    }

    /* write sensors if requested */
    if (options.writes.head != NULL) {
        ret |= vsensors_write(&options, log, sctx);
    }

    /* exit if list/print/write was requested */
    if ((options.flags & (FLAG_SENSOR_LIST | FLAG_SENSOR_PRINT)) != 0
    ||  options.writes.head != NULL) {
        return vsensors_free(ret, &options, log, sctx);
    }

    /* RUN THE MAIN WATCH LOOP */
    if ((options.flags & FLAG_FALLBACK_DISPLAY) != 0
    || vsensors_screen_loop(&options, sctx, log, out) != 0)
        vsensors_log_loop(&options, sctx, log, out);

    /* Free sensor data, logpool and terminal resources */
    return vsensors_free(0, &options, log, sctx);
}

/*************************************************************************/

#ifndef APP_INCLUDE_SOURCE
static const char * s_app_no_source_string
    = "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
      BUILD_APPNAME " source not included in this build.\n";

int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
    return vdecode_buffer(out, buffer, buffer_size, ctx,
                          s_app_no_source_string, strlen(s_app_no_source_string));
}
#endif

/*************************************************************************/

