/*
 * Copyright (C) 2017 Vincent Sallaberry
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
 * Test program for libvsensors.
 */
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vlib/log.h"
#include "vlib/options.h"
#include "vlib/util.h"

#include "libvsensors/sensor.h"

#include "build.h"

#define VERSION_STRING OPT_VERSION_STR "\n\n" \
    "Copyright (C) 2017 Vincent Sallaberry.\n" \
    "This is free software; see the source for copying conditions.  There is NO\n" \
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."

static const opt_options_desc_t s_opt_desc[] = {
    { 'h', "help",      NULL,           "show usage"  },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n" \
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
#ifdef INCLUDE_SOURCE
	{ 's', "source",    NULL,           "show source" },
#endif
#ifdef _TEST
    { 'T', "test",      "[test_mode]",  "test mode. Default: 1." },
#endif
	{ 0, NULL, NULL, NULL }
};

enum FLAGS {
    FLAG_NONE = 0,
};

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    slist_t *       logs;
} options_t;

/** global running state used by signal handler */
static int s_running = 1;
/** signal handler */
static void sig_handler(int sig) {
    if (sig == SIGINT)
        s_running = 0;
}

/** parse_option() : option callback of type opt_option_callback_t. see options.h */
static int parse_option(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    switch (opt) {
    case 'h':
        return opt_usage(0, opt_config);
#ifdef INCLUDE_SOURCE
	case 's': {
        const char *const* vsensorsdemo_get_source();
        const char *const* vlib_get_source();
        const char *const* libvsensors_get_source();
        const char *const*const srcs[] = { vsensorsdemo_get_source(), vlib_get_source(), libvsensors_get_source(), NULL };
        for (const char *const*const* src = srcs; *src; src++) {
            for (const char *const* line = *src; *line; line++) {
	            fprintf(stdout, "%s", *line);
            }
        }
	    return 0;
    }
#endif
    case 'l':
        if (arg != NULL && *arg != '-') {
            options->logs = xlog_create_from_cmdline(options->logs, arg, NULL);
            (*i_argv)++;
        }
        break ;
#ifdef _TEST
    case 'T':
        options->test_mode = 1;
        if (arg != NULL && *arg != '-') {
            options->test_mode = atoi(arg);
        }
        *i_argv = opt_config->argc; // ignore following options to make them parsed by test()
        break ;
#endif
	default:
	   return -1;
    }
    return 1;
}

int main(int argc, const char *const* argv) {
    log_ctx_t *     log         = xlog_create(NULL);
    options_t       options     = { .flags = FLAG_NONE, .test_mode = 0, .logs = slist_prepend(NULL, log) };
    opt_config_t    opt_config  = { argc, argv, parse_option, s_opt_desc, VERSION_STRING, &options };

    FILE * const    out         = stdout;
    int             result;

    /* Manage program options */
    if ((result = opt_parse_options(&opt_config)) <= 0) {
    	return -result;
    }

#ifdef _TEST
    int test(int argc, const char *const* argv, options_t *options);
    /* Test entry point, will stop program with -result if result is negative or null. */
    if ((options.test_mode > 0) != 0
    &&  (result = test(argc, argv, &options)) <= 0) {
        return -result;
    }
#endif

    /* get main module log */
    LOG_INFO(log, "Starting...\n");

    /* Init Sensors and get list */
    struct sensor_ctx_s *sctx = sensor_init(NULL);
    slist_t * list = sensor_list_get(sctx);

    /* display sensors */
    for (slist_t *elt = list; elt; elt = elt->next) {
        sensor_desc_t * desc = (sensor_desc_t *) elt->data;
        fprintf(out, "%s\n", desc->label);
    }

    /* select sensors to watch */
    sensor_watch_t watch = { .update_interval_ms = 1000, .callback = NULL, };
    sensor_watch_add(NULL, &watch, sctx);
    LOG_INFO(log, "pgcd: %lu\n", sensor_watch_pgcd(sctx));

    /* watch sensors updates */
    int t_ms=0;
    char buf[11];
    const int sleep_ms = 500;
    const int max_time_ms = 1000 * 3600;
    signal(SIGINT, sig_handler);
    while (t_ms < max_time_ms && s_running) {
        /* get the list of updated sensors */
        slist_t *updates = sensor_update_get(sctx);

        /* print updates */
        printf("%05d: updates = %d", t_ms, slist_length(updates));
        SLIST_FOREACH_DATA(updates, sensor, sensor_sample_t *) {
            sensor_value_tostring(&sensor->value, buf, sizeof(buf));
            printf(" %s:%s", sensor->desc->label, buf);
        }
        printf("\n");

        /* free updates */
        sensor_update_free(updates);

        /* wait next tick */
        usleep (sleep_ms*1000);
        t_ms += sleep_ms;
    }

    LOG_INFO(log, "exiting...\n");
    /* Free sensor data */
    sensor_free(sctx);
    xlog_list_free(options.logs);

    return 0;
}

/* ** TESTS ***********************************************************************************/
#ifdef _TEST

#include <sys/time.h>

// FIXME remove those specific includes
#include "smc.h"
#include "memory.h"
#include "cpu.h"
#include "network.h"
#include "disk.h"

#include "vlib/hash.h"
#include "vlib/util.h"


static const opt_options_desc_t s_opt_desc_test[] = {
    { 'h', "help",      NULL,           "show usage" },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n"
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
    { 'T', "test",      "[test_mode]",  "test mode. Default: 1." },
    {  -1, "long-only1", "", "" },
    {  -127, "long-only2", "", "" },
    {  -128, "long-only3", "", "" },
    {  -129, "long-only4", "Patata patata patata.", "Potato potato potato potato." },
    {  128, "long-only5", "Taratata taratata taratata.", "Tirititi tirititi tirititi\nTorototo torototo" },
    {  256, "long-only6", "", "" },
	{ 0, NULL, NULL, NULL }
};

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    slist_t *       logs;
} options_test_t;

/** parse_option() : option callback of type opt_option_callback_t. see options.h */
static int parse_option_test(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    options_test_t *options = (options_test_t *) opt_config->user_data;
    switch (opt) {
    case 'h':
        return opt_usage(0, opt_config);
    case 'l':
        if (arg != NULL && *arg != '-') {
            options->logs = xlog_create_from_cmdline(options->logs, arg, NULL);
            (*i_argv)++;
        }
        break ;
    case 'T':
        options->test_mode = 1;
        if (arg != NULL && *arg != '-') {
            options->test_mode = atoi(arg);
            (*i_argv)++;
        }
        break ;
    case 0:
        printf("Argument: '%s'\n", arg);
        break ;
    case -1:
    case -127:
    case -128:
    case -129:
    case 128:
    case 256:
        printf("longopt:%d\n", opt);
        break ;
    case (char)-129:
        printf("longopt (char)-129:%d\n", opt);
        break ;
	default:
	   return -1;
    }
    return 1;
}

static int sensor_value_toint(sensor_value_t * value) {
    int r=1;
    switch (value->type) {
        case SENSOR_VALUE_INT:
            r--;
            break ;
        default:
            r = r * (r/r) - 1;
           break ;
    }
    return (value->data.i - r);
}

static int hash_print_stats(hash_t * hash, FILE * file) {
    int n = 0;
    int tmp;
    hash_stats_t stats;

    if (hash_stats_get(hash, &stats) != HASH_SUCCESS) {
        fprintf(file, "error, cannot get hash stats\n");
        return -1;
    }
    if ((tmp =
        fprintf(file, "Hash %p\n"
                       " size       : %u\n"
                       " flags      : %08x\n"
                       " elements   : %u\n"
                       " indexes    : %u\n"
                       " index_coll : %u\n"
                       " collisions : %u\n",
                       (void*) hash,
                       stats.hash_size, stats.hash_flags,
                       stats.n_elements, stats.n_indexes,
                       stats.n_indexes_with_collision, stats.n_collisions)) < 0) {
        return -1;
    }
    n += tmp;
    return n;
}

int test(int argc, const char *const* argv, options_t *options) {
    FILE * const    out         = stdout;
    hash_t *        hash;
    options_test_t  options_test    = { .flags = FLAG_NONE, .test_mode = 0, .logs = NULL };
    opt_config_t    opt_config_test = { argc, argv, parse_option_test, s_opt_desc_test, OPT_VERSION_STR, &options_test };
    int             result;
    const char *    str;
    char            ascii[256];

    fprintf(out, "TEST MODE: %d\n", options->test_mode);

    /* ascii */
    for (result = -128; result <= 127; result++) {
        ascii[result + 128] = result;
    }
    xlog_buffer(0, NULL, ascii, 256, "ascii> ");

    /* Manage test program options */
    result = opt_parse_options(&opt_config_test);
    fprintf(out, "opt_parse_options() result: %d\n", result);

    /* test Hash */
    hash = hash_alloc(HASH_DEFAULT_SIZE, 0, hash_ptr, hash_ptrcmp, NULL);
    printf("hash_ptr: %08x\n", hash_ptr(hash, argv));
    printf("hash_ptr: %08x\n", hash_ptr(hash, options));
    printf("hash_str: %08x\n", hash_str(hash, *argv));
    printf("hash_str: %08x\n", hash_str(hash, VERSION_STRING));
    hash_free(hash);

    for (result = 1; result < 200; result += 100) {
        hash = hash_alloc(result, 0, hash_str, (hash_cmp_fun_t) strcmp, NULL);
        str = *argv; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = VERSION_STRING; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "a"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "z"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "ab"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "ac"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "cxz"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        str = "trz"; printf(" hash_insert '%s': %d, ", str, hash_insert(hash, (void *) str)); printf("find: %s\n", (char *) hash_find(hash, str));
        hash_print_stats(hash, stdout);
        hash_free(hash);
    }

    /* test sensors */
    smc_print();

    const char *mem_args[] = { "", "-w1", "1" };
    memory_print(3, mem_args);
    network_print();

    /*unsigned long pgcd(unsigned long a, unsigned long b);
    int a[] = {5, 36, 1, 0, 900, 15, 18};
    int b[] = {2, 70, 0, 1, 901, 18, 15};
    for (int i = 0; i < sizeof(a)/sizeof(*a); i++) {
        printf("a:%d b:%d p:%d\n", a[i], b[i],pgcd(a[i],b[i]));
    }*/
    //struct timeval t0, t1, tt;
    BENCH_DECL(t);
    unsigned long r;
    unsigned long i;
    unsigned long nb_op = 50000000;
    sensor_value_t v1 = { .type = SENSOR_VALUE_INT, .data.i = 1000000 };
    sensor_value_t v2 = { .type = SENSOR_VALUE_INT, .data.i = 2108091 };
    sensor_sample_t s1 = { .value = v1 };
    sensor_sample_t s2 = { .value = v2 };

    for(int j=0; j < 3; j++) {
        BENCH_START(t);
        for (i=0, r=0; i < nb_op; i++) {
            s1.value.data.i = r++;
            r += sensor_value_toint(&s1.value) < sensor_value_toint(&s2.value);
        }
        BENCH_PRINT(t, "sensor_value_toint r=%lu ", r);

        BENCH_START(t);
        for (i=0, r=0; i < nb_op; i++) {
            s1.value.data.i = r++;
            r += sensor_value_todouble(&s1.value) < sensor_value_todouble(&s2.value);
        }
        BENCH_PRINT(t, "sensor_value_todouble ");
        BENCH_PRINT(t, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p ul=%lu ", r, "STRING", 'Z', (void*)&r, r);
    }
    // *****************************************************************
    printf("** MEMORY BEFORE ALLOC\n");
    memory_print();

    return 0;
}
#endif /* ! ifdef _TEST */

