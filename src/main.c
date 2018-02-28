/*
 * Copyright (C) 2017-2018 Vincent Sallaberry
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
#include <errno.h>

#include "vlib/log.h"
#include "vlib/options.h"
#include "vlib/util.h"

#include "libvsensors/sensor.h"

#include "version.h"

#define VERSION_STRING OPT_VERSION_STRING(BUILD_APPNAME, APP_VERSION, "git:" BUILD_GITREV) \
                       "\n\n" OPT_LICENSE_GPL3PLUS("Vincent Sallaberry", "2017-2018")

static const opt_options_desc_t s_opt_desc[] = {
    { 'h', "help",      NULL,           "show usage"  },
    { 'V', "version",   NULL,           "show version"  },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n" \
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
	{ 's', "source",    NULL,           "show source" },
#   ifdef _TEST
    { 'T', "test",      "[test[,test]...]",  "test mode, default: all. The next options will "
                                             "be received by test parsing method." },
#   endif
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
static volatile sig_atomic_t s_running = 1;
/** signal handler */
static void sig_handler(int sig) {
    if (sig == SIGINT)
        s_running = 0;
}

#ifdef _TEST
/** strings corresponding of unit tests ids , for command line */
enum testmode_t {
    TEST_all = 0,
    TEST_sizeof,
    TEST_options,
    TEST_ascii,
    TEST_bench,
    TEST_hash,
    TEST_sensorvalue,
    TEST_log,
    TEST_account,
};
static const char * const s_testmode_str[] = {
    "all", "sizeof", "options", "ascii", "bench", "hash", "sensorvalue", "log", "account", NULL
};
static unsigned int test_getmode(const char *arg);
#endif

/** parse_option() : option callback of type opt_option_callback_t. see vlib/options.h */
static int parse_option(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        /* This is the option dynamic description for opt_usage() */
        switch (opt & ~OPT_DESCRIBE_OPTION) {
#           ifdef _TEST
            case 'T': {
                int n = 0, ret;
                char sep[2]= { 0, 0 };
                if ((ret = snprintf(((char *)arg) + n, *i_argv - n, "\ntest modes: '")) > 0)
                    n += ret;
                for (const char *const* mode = s_testmode_str; *mode; mode++, *sep = ',')
                    if ((ret = snprintf(((char *)arg) + n, *i_argv - n, "%s%s", sep, *mode)) > 0)
                        n += ret;
                if ((ret = snprintf(((char *)arg) + n, *i_argv - n, "'")) > 0)
                    n += ret;
                *i_argv = n;
                break ;
            }
#           endif
            default:
                return OPT_EXIT_OK(0);
        }
        return OPT_CONTINUE(1);
    }
    /* This is the option parsing */
    switch (opt) {
        case 'h':
            return opt_usage(OPT_EXIT_OK(0), opt_config);
        case 'V':
            fprintf(stdout, "%s\n\nWith:\n   %s\n   %s\n\n",
                    opt_config->version_string, libvsensors_get_version(), vlib_get_version());
            return OPT_EXIT_OK(0);
        case 's': {
            const char *const*const srcs[] = { vsensorsdemo_get_source(), vlib_get_source(), libvsensors_get_source() };
            for (size_t i = 0; i < (sizeof(srcs) / sizeof(*srcs)); i++)
                for (const char *const* line = srcs[i]; *line; line++)
                    fprintf(stdout, "%s", *line);
            return OPT_EXIT_OK(0);
        }
        case 'l':
            if (arg != NULL && *arg != '-') {
                options->logs = log_create_from_cmdline(options->logs, arg, NULL);
                (*i_argv)++;
            }
            break ;
#       ifdef _TEST
        case 'T':
            options->test_mode |= test_getmode(arg);
            if (options->test_mode == 0) {
                return OPT_ERROR(2);
            }
            *i_argv = opt_config->argc; // ignore following options to make them parsed by test()
            break ;
#       endif
        default:
            return OPT_ERROR(1);
    }
    return OPT_CONTINUE(1);
}

int main(int argc, const char *const* argv) {
    log_t *         log         = log_create(NULL);
    options_t       options     = { .flags = FLAG_NONE, .test_mode = 0, .logs = slist_prepend(NULL, log) };
    opt_config_t    opt_config  = { argc, argv, parse_option, s_opt_desc, VERSION_STRING, &options };
    FILE * const    out         = stdout;
    int             result;

    /* Manage program options */
    if (OPT_IS_EXIT(result = opt_parse_options(&opt_config))) {
        log_list_free(options.logs);
        return OPT_EXIT_CODE(result);
    }

#   ifdef _TEST
    int test(int argc, const char *const* argv, options_t *options);
    /* Test entry point, will stop program with -result if result is negative or null. */
    if ((options.test_mode > 0) != 0
    &&  (result = test(argc, argv, &options)) <= 0) {
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
    LOG_INFO(log, "pgcd: %lu", sensor_watch_pgcd(sctx));

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

    LOG_INFO(log, "exiting...");
    /* Free sensor data */
    sensor_free(sctx);
    log_list_free(options.logs);

    return 0;
}

#ifndef APP_INCLUDE_SOURCE
const char *const* vsensorsdemo_get_source() {
    static const char * const source[] = { "vsensorsdemo source not included in this build.\n", NULL };
    return source;
}
#endif

/* ** TESTS ***********************************************************************************/
#ifdef _TEST

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>

#include "vlib/hash.h"
#include "vlib/util.h"
#include "vlib/time.h"
#include "vlib/account.h"

enum {
    long1 = OPT_ID_USER,
    long2,
    long3,
    long4,
    long5,
    long6
};

static const opt_options_desc_t s_opt_desc_test[] = {
    { 'a', NULL,        NULL,           "test NULL long_option" },
    { 'h', "help",      NULL,           "show usage" },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n"
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
    { 'T', "test",      "[test_mode]",  "test mode. Default: 1." },
    { 'N', "--NULL-desc", NULL, NULL },
    /* those are bad IDs, but tested as allowed before OPT_DESCRIBE_OPTION feature introduction */
    {  -1, "long-only-1", "", "" },
    {  -127, "long-only-2", "", "" },
    {  -128, "long-only-3", "", "" },
    {  -129, "long-only-4", "Patata patata patata.", "Potato potato potato potato." },
    {  128, "long-only-5", "Taratata taratata taratata.", "Tirititi tirititi tirititi\nTorototo torototo" },
    {  256, "long-only-6", "", "" },
    /* correct ids using OPT_ID_USER */
    {  long1, "long-only1", "", "" },
    {  long2, "long-only2", "", "abcdedfghijkl mno pqrstuvwxyz ABCDEF GHI JK LMNOPQRSTU VWXYZ 01234567890" },
    {  long3, "long-only3", NULL, NULL },
    {  long4, "long-only4", "Patata patata patata.", "Potato potato potato potato." },
    {  long5, "long-only5", "Taratata taratata taratata.", "Tirititi tirititi tirititi\nTorototo torototo abcdedfghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890" },
    {  long6, "long-only6", "", "" },
    /* end of table */
	{ 0, NULL, NULL, NULL }
};

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    slist_t *       logs;
    FILE *          out;
} options_test_t;

/** parse_option() : option callback of type opt_option_callback_t. see options.h */
static int parse_option_test(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    options_test_t *options = (options_test_t *) opt_config->user_data;
    switch (opt) {
    case 'h':
        return opt_usage(OPT_EXIT_OK(0), opt_config);
    case 'l':
        if (arg != NULL && *arg != '-') {
            options->logs = log_create_from_cmdline(options->logs, arg, NULL);
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
    case OPT_ID_ARG:
        printf("Single argument: '%s'\n", arg);
        break ;
    /* those are bad IDs, but tested as allowed before OPT_DESCRIBE_OPTION feature introduction */
    case -1:
    case -127:
    case -128:
    case -129:
    case 128:
    case 256:
        printf("longopt_legacy:%d\n", opt);
        break ;
    case (char)-129:
        printf("longopt_legacy (char)-129:%d\n", opt);
        break ;
    /* correct ids using OPT_USER_ID */
    case long1:
    case long2:
    case long3:
    case long4:
    case long5:
    case long6:
        printf("longopt:%d\n", opt);
        break ;
	default:
	   return OPT_ERROR(1);
    }
    return OPT_CONTINUE(1);
}

static unsigned int test_getmode(const char *arg) {
    char * endptr;
    const unsigned int test_mode_all = 0xffffffffU;
    unsigned int test_mode = test_mode_all;
    if (arg != NULL && *arg != '-') {
        test_mode = strtol(arg, &endptr, 0);
        if (endptr == arg) {
            const char * token, * next = arg;
            size_t len, i;
            while ((len = strtok_ro_r(&token, ",", &next, NULL, 0)) > 0 || *next) {
                for (i = 0; s_testmode_str[i]; i++) {
                    if (!strncasecmp(s_testmode_str[i], token, len) && s_testmode_str[i][len] == 0) {
                        test_mode |= (i == TEST_all ? test_mode_all : (1U << i));
                        break ;
                    }
                }
                if (s_testmode_str[i] == NULL) {
                    fprintf(stderr, "unreconized test id '"); fwrite(token, 1, len, stderr); fputs("'\n", stderr);
                    return 0;
                }
            }
        }
    }
    return test_mode;
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

/* *************** SIZEOF information ********************** */
#define PSIZEOF(type) LOG_INFO(NULL, "%20s: %zu", #type, sizeof(type))

static int test_sizeof(const options_test_t * opts) {
    int     nerrors = 0;
    (void)  opts;

    LOG_INFO(NULL, ">>> SIZE_OF tests");
    PSIZEOF(char);
    PSIZEOF(unsigned char);
    PSIZEOF(short);
    PSIZEOF(unsigned short);
    PSIZEOF(int);
    PSIZEOF(unsigned int);
    PSIZEOF(long);
    PSIZEOF(unsigned long);
    PSIZEOF(long long);
    PSIZEOF(unsigned long long);
    PSIZEOF(uint16_t);
    PSIZEOF(int16_t);
    PSIZEOF(uint32_t);
    PSIZEOF(int32_t);
    PSIZEOF(uint64_t);
    PSIZEOF(int64_t);
    PSIZEOF(size_t);
    PSIZEOF(time_t);
    PSIZEOF(float);
    PSIZEOF(double);
    PSIZEOF(long double);
    PSIZEOF(char *);
    PSIZEOF(unsigned char *);
    PSIZEOF(void *);
    PSIZEOF(log_t);
    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** ASCII and TEST LOG BUFFER *************** */

static int test_ascii(options_test_t * opts) {
    int     nerrors = 0;
    int     result;
    char    ascii[256];
    (void)  opts;
    ssize_t n;

    LOG_INFO(NULL, ">>> ASCII/LOG_BUFFER tests");

    for (result = -128; result <= 127; result++) {
        ascii[result + 128] = result;
    }

    n = LOG_BUFFER(0, NULL, ascii, 256, "ascii> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : log_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, NULL, 256, "null_01> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : log_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, NULL, 0, "null_02> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : log_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, ascii, 0, "ascii_sz=0> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : log_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }
    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** TEST OPTIONS *************** */

static int test_parse_options(int argc, const char *const* argv, options_test_t * opts) {
    opt_config_t    opt_config_test = { argc, argv, parse_option_test, s_opt_desc_test, VERSION_STRING, opts };
    int             result = opt_parse_options(&opt_config_test);

    LOG_INFO(NULL, ">>> opt_parse_options() result: %d", result);
    if (result <= 0) {
        LOG_ERROR(NULL, "ERROR opt_parse_options() expected >0, got %d", result);
        return 1;
    }
    LOG_INFO(NULL, NULL);
    return 0;
}

/* *************** TEST HASH *************** */

static void test_one_hash_insert(hash_t *hash, const char * str, options_test_t * opts, unsigned int * errors) {
    FILE *  out = opts->out;
    int     ins;
    char *  fnd;

    ins = hash_insert(hash, (void *) str);
    fprintf(out, " hash_insert [%s]: %d, ", str, ins);

    fnd = (char *) hash_find(hash, str);
    fprintf(out, "find: [%s]\n", fnd);

    if (ins != HASH_SUCCESS) {
        fprintf(out, " %s(): ERROR hash_insert [%s]: got %d, expected HASH_SUCCESS(%d)\n", __func__, str, ins, HASH_SUCCESS);
        ++(*errors);
    }
    if (fnd == NULL || strcmp(fnd, str)) {
        fprintf(out, " %s(): ERROR hash_find [%s]: got %s\n", __func__, str, fnd ? fnd : "NULL");
        ++(*errors);
    }
}

static int test_hash(options_test_t * opts) {
    hash_t *                    hash;
    FILE *                      out = opts->out;
    unsigned int                errors = 0;
    static const char * const   hash_strs[] = {
        VERSION_STRING, "a", "z", "ab", "ac", "cxz", "trz", NULL
    };

    LOG_INFO(NULL, ">>> HASH TESTS");

    hash = hash_alloc(HASH_DEFAULT_SIZE, 0, hash_ptr, hash_ptrcmp, NULL);
    if (hash == NULL) {
        fprintf(out, "%s(): ERROR Hash null\n", __func__);
        errors++;
    } else {
        fprintf(out, "hash_ptr: %08x\n", hash_ptr(hash, opts));
        fprintf(out, "hash_ptr: %08x\n", hash_ptr(hash, hash));
        fprintf(out, "hash_str: %08x\n", hash_str(hash, out));
        fprintf(out, "hash_str: %08x\n", hash_str(hash, VERSION_STRING));
        hash_free(hash);
    }

    for (unsigned int hash_size = 1; hash_size < 200; hash_size += 100) {
        if ((hash = hash_alloc(hash_size, 0, hash_str, (hash_cmp_fun_t) strcmp, NULL)) == NULL) {
            fprintf(out, "%s(): ERROR hash_alloc() sz:%d null\n", __func__, hash_size);
            errors++;
            continue ;
        }

        for (const char *const* strs = hash_strs; *strs; strs++) {
            test_one_hash_insert(hash, *strs, opts, &errors);
        }

        if (hash_print_stats(hash, out) <= 0) {
            fprintf(out, "%s(): ERROR hash_print_stat sz:%d returns <= 0, expected >0\n", __func__, hash_size);
            errors++;
        }
        hash_free(hash);
    }
    LOG_INFO(NULL, NULL);
    return errors;
}

/* *************** BENCH SENSOR VALUES CAST *************** */

static int test_sensor_value(options_test_t * opts) {
    BENCH_DECL(t);
    unsigned long r;
    unsigned long i;
    unsigned long nb_op = 50000000;
    (void) opts;

    LOG_INFO(NULL, ">>> SENSOR VALUE TESTS");

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
        BENCH_STOP_PRINTF(t, "sensor_value_toint    r=%08lx ", r);

        BENCH_START(t);
        for (i=0, r=0; i < nb_op; i++) {
            s1.value.data.i = r++;
            r += sensor_value_todouble(&s1.value) < sensor_value_todouble(&s2.value);
        }
        BENCH_STOP_PRINTF(t, "sensor_value_todouble %-10s ", "");
    }
    LOG_INFO(NULL, NULL);
    return 0;
}

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
    fflush(ctx->log->out);
    return (void *) ((long)nerrors);
}

static const int                s_stop_signal   = SIGHUP;
static volatile sig_atomic_t    s_pipe_stop     = 0;

static void pipe_sighdl(int sig, siginfo_t * si, void * p) {
    static pid_t pid = 0;
    if (sig == 0 && pid == 0 && p == NULL) {
        pid = getpid();
        s_pipe_stop = 0;
        return ;
    }
    if (sig == s_stop_signal && (si->si_pid == 0 || si->si_pid == pid))
        s_pipe_stop = 1;
}

static void * pipe_log_thread(void * data) {
    char                buf[4096];
    struct sigaction    sa          = { .sa_sigaction = pipe_sighdl, .sa_flags = SA_SIGINFO };
    FILE **             fpipe       = (FILE **) data;
    ssize_t             n;
    unsigned long       nerrors     = 0;
    int                 fd_pipein   = fileno(fpipe[PIPE_IN]);
    int                 fd_pipeout  = fileno(fpipe[PIPE_OUT]);
    int                 o_nonblock  = 0;

    pipe_sighdl(0, NULL, NULL); /* init handler with my pid */
    sigemptyset(&sa.sa_mask);
    sigaction(s_stop_signal, &sa, NULL);
    while (1) {
        errno = 0;
        /* On signal reception, set read_NON_BLOCK so that we exit as soon as
         * no more data is available. */
        if (s_pipe_stop && !o_nonblock) {
            o_nonblock = 1;
            while ((n = fcntl(fd_pipein, F_SETFL, O_NONBLOCK, &o_nonblock)) < 0)
                if (errno != EINTR) return (void *) (nerrors + 1);
        }
        /* Read data, exit if none. On EINTR, reloop and sighandler set pipestop to unblock read fd.*/
        if ((n = read(fd_pipein, buf, sizeof(buf))) < 0 && errno == EINTR) {
            continue ;
        } else if (n <= 0)
            break ;
        /* Write data. On EINTR, retry. sighandler will set pipestop to unblock read fd. */
        while (write(fd_pipeout, buf, n) < 0) {
            if (errno != EINTR) {
                nerrors++;
                break ;
            }
        }
    }
    return (void *) nerrors;
}

static int test_log_thread(options_test_t * opts) {
    const char * const  files[] = { "stdout", "one_file", NULL };
    slist_t             *filepaths = NULL;
    const size_t        cmdsz = 4 * PATH_MAX;
    char *              cmd;
    int                 nerrors = 0;
    int                 i;
    (void)              opts;

    LOG_INFO(NULL, ">>> LOG THREAD TESTS");
    fflush(NULL); /* First of all ensure all previous messages are flushed */
    i = 0;
    for (const char * const * filename = files; *filename; filename++, i++) {
        char                path[PATH_MAX];
        FILE *              file;
        FILE *              fpipeout = NULL;
        FILE *              fpipein = NULL;
        test_log_thread_t   threads[N_TEST_THREADS];
        log_t               logs[N_TEST_THREADS];
        log_t               log = { .level = LOG_LVL_SCREAM, .flags = LOG_FLAG_DEFAULT | LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE | LOG_FLAG_LOC_TAIL };
        char                prefix[20];
        pthread_t           pipe_tid;
        int                 p[2] = { -1, -1 };
        int                 fd_pipein = -1;
        int                 fd_backup = -1;
        void *              thread_ret;
        FILE *              fpipe[2];
        BENCH_TM_DECL(tm0);
        BENCH_DECL(t0);

        /* generate unique tst file name for the current loop */
        snprintf(path, sizeof(path), "/tmp/test_thread_%s_%d_%u_%lx.log",
                 *filename, i, (unsigned int) getpid(), (unsigned long) time(NULL));
        filepaths = slist_prepend(filepaths, strdup(path));
        LOG_INFO(NULL, ">>> logthread: test %s (%s)", *filename, path);

        /* Create pipe to redirect stdout & stderr to pipe log file (fpipeout) */
        if ((!strcmp("stderr", *filename) && (file = stderr)) || (!strcmp(*filename, "stdout") && (file = stdout))) {
            fpipeout = fopen(path, "w");
            if (fpipeout == NULL) {
                LOG_ERROR(NULL, "error: create cannot create '%s': %s", path, strerror(errno));
                ++nerrors;
                continue ;
            } else if (pipe(p) < 0) {
                LOG_ERROR(NULL, "ERROR pipe: %s", strerror(errno));
                ++nerrors;
                fclose(fpipeout);
                continue ;
            } else if (p[PIPE_OUT] < 0 || (fd_backup = dup(fileno(file))) < 0 || dup2(p[PIPE_OUT], fileno(file)) < 0) {
                LOG_ERROR(NULL, "ERROR dup2: %s", strerror(errno));
                ++nerrors;
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                close(fd_backup);
                continue ;
            } else if ((fpipein = fdopen(p[PIPE_IN], "r")) == NULL) {
                LOG_ERROR(NULL, "ERROR, cannot fdopen p[PIPE_IN]: %s", strerror(errno));
                ++nerrors;
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                continue ;
            }
            /* create thread reading data from pipe, and writing to pipe log file */
            fd_pipein = fileno(fpipein);
            fpipe[PIPE_IN] = fpipein;
            fpipe[PIPE_OUT] = fpipeout;
            pthread_create(&pipe_tid, NULL, pipe_log_thread, fpipe);
        } else if ((file = fopen(path, "w")) == NULL)  {
            LOG_ERROR(NULL, "Error: create cannot create '%s': %s", path, strerror(errno));
            ++nerrors;
            continue ;
        }

        /* Create log threads and launch them */
        log.out = file;
        BENCH_TM_START(tm0);
        BENCH_START(t0);
        for (unsigned int i = 0; i < N_TEST_THREADS; i++) {
            logs[i] = log;
            snprintf(prefix, sizeof(prefix), "THREAD%05d", i);
            logs[i].prefix = strdup(prefix);
            threads[i].log = &logs[i];
            pthread_create(&threads[i].tid, NULL, log_thread, &threads[i]);
        }
        /* wait log threads to terminate */
        for (unsigned int i = 0; i < N_TEST_THREADS; i++) {
            thread_ret = NULL;
            pthread_join(threads[i].tid, &thread_ret);
            nerrors += (unsigned long) thread_ret;
            if (threads[i].log->prefix)
                free(threads[i].log->prefix);
        }
        BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);
        /* terminate log_pipe thread */
        if (fd_pipein >= 0) {
            fflush(NULL);
            pthread_kill(pipe_tid, SIGHUP);
            thread_ret = NULL;
            pthread_join(pipe_tid, &thread_ret);
            nerrors += (unsigned long) thread_ret;
            /* restore redirected file */
            if (dup2(fd_backup, fileno(file)) < 0) {
                LOG_ERROR(NULL, "ERROR dup2 restore: %s", strerror(errno));
            }
            /* cleaning */
            fclose(fpipein); // fclose will close p[PIPE_IN]
            close(p[PIPE_OUT]);
            close(fd_backup);
            fclose(fpipeout);
            /* write something in <file> and it must not be redirected anymore */
            if (fprintf(file, "*** checking %s is not anymore redirected ***\n\n", *filename) <= 0) {
                LOG_ERROR(NULL, "ERROR fprintf(checking %s): %s", *filename, strerror(errno));
                ++nerrors;
            }
        } else {
            fclose(file);
        }
        LOG_INFO(NULL, "duration : %ld.%03lds (clock %ld.%03lds)",
                 BENCH_TM_GET(tm0) / 1000, BENCH_TM_GET(tm0) % 1000,
                 BENCH_GET(t0) / 1000, BENCH_GET(t0) % 1000);
    }
    /* compare logs */
    LOG_INFO(NULL, "checking logs...");
    if ((cmd = malloc(cmdsz)) == NULL) {
        LOG_ERROR(NULL, "Error, cannot allocate memory for shell command: %s", strerror(errno));
        ++nerrors;
    } else {
        size_t n = 0;
        /* construct a shell script iterating on each file, filter them to remove timestamps
         * and diff them */
        n += snprintf(cmd + n, cmdsz - n, "ret=true; prev=; for f in ");
        SLIST_FOREACH_DATA(filepaths, spath, char *) {
            n += snprintf(cmd + n, cmdsz - n, "%s ", spath);
        }
        n += snprintf(cmd + n, cmdsz - n,
                      "; do sed -e 's/^[.:0-9[:space:]]*//' -e s/'Thread #[0-9]*/Thread #X/' -e 's/tid:[0-9]*/tid:X/'"
                      "       \"$f\" | sort > \"${f%%.log}_filtered.log\"; "
                      "     if test -n \"$prev\"; then "
                      "         diff -u \"${prev%%.log}_filtered.log\" \"${f%%.log}_filtered.log\" "
                      "           || { echo \"diff error (filtered $prev <> $f)\"; ret=false; };"
                      "         grep -E '\\*\\*\\* checking .* is not anymore redirected \\*\\*\\*' \"${f}\""
                      "           && { echo \"error: <${f}> pipe has not been restored\"; ret=false; };"
                      "         rm \"$prev\" \"${prev%%.log}_filtered.log\";"
                      "     fi; prev=$f; "
                      " done; test -e \"$prev\" && rm \"$prev\" \"${prev%%.log}_filtered.log\"; $ret");
        if (system(cmd) != 0) {
            LOG_ERROR(NULL, "Error during logs comparison");
            ++nerrors;
        }
        free(cmd);
    }
    slist_free(filepaths, free);
    LOG_INFO(NULL, "<- %s(): ending with %d error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST BENCH *************** */
static volatile sig_atomic_t s_bench_stop;
static void bench_sighdl(int sig) {
    (void)sig;
    s_bench_stop = 1;
}
static int test_bench(options_test_t *opts) {
    BENCH_DECL(t0);
    BENCH_TM_DECL(tm0);
    const int step_ms = 1000;
    const unsigned char margin = 25;
    int nerrors = 0;
    (void) opts;

    LOG_INFO(NULL, ">>> BENCH TESTS...");
    BENCH_START(t0);
    BENCH_STOP_PRINTF(t0, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ", 12UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    BENCH_START(t0);
    BENCH_STOP_LOG(t0, NULL, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ", 40UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    BENCH_STOP_PRINT(t0, LOG_WARN, NULL, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ", 98UL, "STRING", 'Z', (void*)&nerrors, nerrors);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, NULL, "// fake-fmt-check LOG%s //", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, NULL, "// fake-fmt-check LOG %d //", 54);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF%s || ", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF %d || ", 65);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, NULL, "__/ fake-fmt-check PRINT=LOG_WARN \\__ ", 0);
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, NULL, "__/ fake-fmt-check PRINT=LOG_WARN %d \\__ ", 2);
    LOG_INFO(NULL, NULL);

    for (int i=0; i< 5000 / step_ms; i++) {
        struct sigaction sa_bak, sa = { .sa_handler = bench_sighdl, .sa_flags = SA_RESTART };
        struct itimerval timer_bak, timer = { .it_value     = { .tv_sec = step_ms / 1000, .tv_usec = (step_ms % 1000) * 1000 },
                                              .it_interval  = { 0, 0 } };
        s_bench_stop = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, &sa_bak) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "sigaction(): %s\n", strerror(errno));
        }
        if (setitimer(ITIMER_REAL, &timer, &timer_bak) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "setitimer(): %s\n", strerror(errno));
        }

        BENCH_TM_START(tm0);
        BENCH_START(t0);
        while (s_bench_stop == 0)
            ;
        BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);

        if (sigaction(SIGALRM, &sa_bak, NULL) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "restore sigaction(): %s\n", strerror(errno));
        }
        if (setitimer(ITIMER_REAL, &timer_bak, NULL) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "restore setitimer(): %s\n", strerror(errno));
        }

        if (i > 0 && (BENCH_GET(t0) < ((step_ms * (100-margin)) / 100)
                      || BENCH_GET(t0) > ((step_ms * (100+margin)) / 100))) {
            fprintf(opts->out, "Error: BAD bench %lu, expected %d with margin %u%%\n",
                    (unsigned long)(BENCH_GET(t0)), step_ms, margin);
            nerrors++;
        }
        if (i > 0 && (BENCH_TM_GET(tm0) < ((step_ms * (100-margin)) / 100)
                      || BENCH_TM_GET(tm0) > ((step_ms * (100+margin)) / 100))) {
            fprintf(opts->out, "Error: BAD TM_bench %lu, expected %d with margin %u%%\n",
                    (unsigned long)(BENCH_TM_GET(tm0)), step_ms, margin);
            nerrors++;
        }

    }
    if (nerrors == 0) {
        fprintf(opts->out, "-> %s() OK.\n", __func__);
    }
    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** TEST ACCOUNT *************** */
#define TEST_ACC_USER "nobody"
#define TEST_ACC_GROUP "nogroup"
static int test_account(options_test_t *opts) {
    struct passwd   pw;
    struct group    gr;
    int             nerrors = 0;
    char *          buffer = NULL;
    char *          bufbak;
    size_t          bufsz = 0;
    uid_t           uid;
    gid_t           gid;
    int             ret;
    (void) opts;

    LOG_INFO(NULL, ">>> ACCOUNT TESTS...");

    /* pwfindid_r/grfindid_r with NULL buffer */
    if ((ret = pwfindid_r(TEST_ACC_USER, &uid, NULL, NULL)) != 0) {
        LOG_ERROR(NULL, "error pwfindid_r(\"" TEST_ACC_USER "\", &uid, NULL, NULL) returns %d, uid:%d", ret, (int) uid);
        nerrors++;
    }
    if ((ret = grfindid_r(TEST_ACC_GROUP, &gid, NULL, NULL)) != 0) {
        LOG_ERROR(NULL, "error grfindid_r(\"" TEST_ACC_GROUP "\", &gid, NULL, NULL) returns %d, gid:%d", ret, (int) gid);
        nerrors++;
    }
    if ((ret = pwfindid_r("__**UserNoFOOUUnd**!!", &uid, NULL, NULL)) == 0) {
        LOG_ERROR(NULL, "error pwfindid_r(\"__**UserNoFOOUUnd**!!\", &uid, NULL, NULL) returns OK, expected error");
        nerrors++;
    }
    if ((ret = grfindid_r("__**GroupNoFOOUUnd**!!", &gid, NULL, NULL)) == 0) {
        LOG_ERROR(NULL, "error grfindid_r(\"__**GroupNoFOOUUnd**!!\", &gid, NULL, NULL) returns OK, expected error");
        nerrors++;
    }
    if ((ret = pwfindid_r(TEST_ACC_USER, &uid, &buffer, NULL)) == 0) {
        LOG_ERROR(NULL, "error pwfindid_r(\"" TEST_ACC_USER "\", &uid, &buffer, NULL) returns OK, expected error");
        nerrors++;
    }
    if ((ret = grfindid_r(TEST_ACC_GROUP, &gid, &buffer, NULL)) == 0) {
        LOG_ERROR(NULL, "error grfindid_r(\"" TEST_ACC_GROUP "\", &gid, &buffer, NULL) returns OK, expected error");
        nerrors++;
    }

    /* pwfindid_r/grfindid_r with shared buffer */
    if ((ret = pwfindid_r(TEST_ACC_USER, &uid, &buffer, &bufsz)) != 0 ||  buffer == NULL) {
        LOG_ERROR(NULL, "error pwfindid_r(\"" TEST_ACC_USER "\", &uid, &buffer, &bufsz) "
                        "returns %d, uid:%d, buffer:0x%lx bufsz:%lu",
                  ret, (int) uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    bufbak = buffer;

    if ((ret = grfindid_r(TEST_ACC_GROUP, &gid, &buffer, &bufsz)) != 0 || buffer != bufbak) {
        LOG_ERROR(NULL, "error grfindid_r(\"" TEST_ACC_GROUP "\", &gid, &buffer, &bufsz) "
                        "returns %d, gid:%d, buffer:0x%lx bufsz:%lu",
                  ret, (int) gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r with shared buffer */
    if ((ret = pwfind_r(TEST_ACC_USER, &pw, &buffer, &bufsz)) != 0 || buffer != bufbak) {
        LOG_ERROR(NULL, "error pwfind_r(\"" TEST_ACC_USER "\", &uid, &buffer, &bufsz) "
                        "returns %d, uid:%d, buffer:0x%lx bufsz:%lu",
                  ret, (int) pw.pw_uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    if ((ret = grfind_r(TEST_ACC_GROUP, &gr, &buffer, &bufsz)) != 0 || buffer != bufbak) {
        LOG_ERROR(NULL, "error grfind_r(\"" TEST_ACC_GROUP "\", &gid, &buffer, &bufsz) "
                        "returns %d, gid:%d, buffer:0x%lx bufsz:%lu",
                  ret, (int) gr.gr_gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r with NULL buffer */
    if ((ret = pwfind_r(TEST_ACC_USER, &pw, NULL, &bufsz)) == 0) {
        LOG_ERROR(NULL, "error pwfind_r(\"" TEST_ACC_USER "\", &pw, NULL, &bufsz) returns OK, expected error");
        nerrors++;
    }
    if ((ret = grfind_r(TEST_ACC_GROUP, &gr, NULL, &bufsz)) == 0) {
        LOG_ERROR(NULL, "error grfind_r(\"" TEST_ACC_GROUP"\", &gr, NULL, &bufsz) returns OK, expected error");
        nerrors++;
    }

    if (buffer != NULL) {
        free(buffer);
    }

    if (nerrors == 0) {
        LOG_INFO(NULL, "-> %s() OK.", __func__);
    }
    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** TEST MAIN FUNC *************** */

int test(int argc, const char *const* argv, options_t *options) {
    options_test_t  options_test    = { .flags = FLAG_NONE, .test_mode = 0, .logs = NULL, .out = stderr };
    int             errors = 0;

    LOG_INFO(NULL, ">>> TEST MODE: 0x%x\n", options->test_mode);

    /* Manage test program options */
    if ((options->test_mode & (1 << TEST_options)) != 0)
        errors += !OPT_IS_EXIT_OK(test_parse_options(argc, argv, &options_test));

    /* sizeof */
    if ((options->test_mode & (1 << TEST_sizeof)) != 0)
        errors += test_sizeof(&options_test);

    /* ascii */
    if ((options->test_mode & (1 << TEST_ascii)) != 0)
        errors += test_ascii(&options_test);

    /* test Bench */
    if ((options->test_mode & (1 << TEST_bench)) != 0)
        errors += test_bench(&options_test);

    /* test Hash */
    if ((options->test_mode & (1 << TEST_hash)) != 0)
        errors += test_hash(&options_test);

    /* test sensors */
    //smc_print();

    //const char *mem_args[] = { "", "-w1", "1" };
    //memory_print(3, mem_args);
    //network_print();

    /*unsigned long pgcd(unsigned long a, unsigned long b);
    int a[] = {5, 36, 1, 0, 900, 15, 18};
    int b[] = {2, 70, 0, 1, 901, 18, 15};
    for (int i = 0; i < sizeof(a)/sizeof(*a); i++) {
        printf("a:%d b:%d p:%d\n", a[i], b[i],pgcd(a[i],b[i]));
    }*/
    //struct timeval t0, t1, tt;

    /* Bench sensor value */
    if ((options->test_mode & (1 << TEST_sensorvalue)) != 0)
        errors += test_sensor_value(&options_test);

    /* Test vlib account functions */
    if ((options->test_mode & (1 << TEST_account)) != 0)
        errors += test_account(&options_test);

    /* Test Log in multiple threads */
    if ((options->test_mode & (1 << TEST_log)) != 0)
        errors += test_log_thread(&options_test);

    // *****************************************************************
    LOG_INFO(NULL, "<<< END of Tests : %d error(s).\n", errors);

    return -errors;
}
#endif /* ! ifdef _TEST */

