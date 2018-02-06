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

#include "vlib/log.h"
#include "vlib/options.h"
#include "vlib/util.h"

#include "libvsensors/sensor.h"

#include "version.h"

#define VERSION_STRING \
    BUILD_APPNAME " v" APP_VERSION ", built on " \
    __DATE__ ", " __TIME__ " from git-rev " BUILD_GITREV "\n\n" \
    "Copyright (C) 2017-2018 Vincent Sallaberry.\n" \
    "This is free software; see the source for copying conditions.  There is NO\n" \
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."

static const opt_options_desc_t s_opt_desc[] = {
    { 'h', "help",      NULL,           "show usage"  },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n" \
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
	{ 's', "source",    NULL,           "show source" },
#   ifdef _TEST
    { 'T', "test",      "[test_mode]",  "test mode. Default: 1." },
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
static sig_atomic_t s_running = 1;
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
};
static const char * s_testmode_str[] = { "all", "sizeof", "options", "ascii", "bench", "hash", "sensorvalue", "log", NULL };
#endif

/** parse_option() : option callback of type opt_option_callback_t. see options.h */
static int parse_option(int opt, const char *arg, int *i_argv, const opt_config_t * opt_config) {
    options_t *options = (options_t *) opt_config->user_data;
    switch (opt) {
    case 'h':
        return opt_usage(0, opt_config);
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
    case 'l':
        if (arg != NULL && *arg != '-') {
            options->logs = xlog_create_from_cmdline(options->logs, arg, NULL);
            (*i_argv)++;
        }
        break ;
#   ifdef _TEST
    case 'T': {
        char * endptr;
        options->test_mode = 0xffffffff;
        if (arg != NULL && *arg != '-') {
            options->test_mode = strtol(arg, &endptr, 0);
            if (endptr == arg) {
                const char * token, * next = arg;
                size_t len, i;
                while ((len = strtok_ro_r(&token, ",", &next, NULL, 0)) > 0) {
                    for (i = 0; s_testmode_str[i]; i++) {
                        if (!strncasecmp(s_testmode_str[i], token, len)) {
                            options->test_mode |= (i == TEST_all ? 0xffffffffU : (1U << i));
                            break ;
                        }
                    }
                    if (s_testmode_str[i] == NULL) {
                        fprintf(stderr, "warning, unreconized test id '"); fwrite(token, 1, len, stderr); fputs("'\n", stderr);
                    }
                }
            }
        }
        *i_argv = opt_config->argc; // ignore following options to make them parsed by test()
        break ;
    }
#   endif
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
        xlog_list_free(options.logs);
    	return -result;
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
    slist_t * list = sensor_list_get(sctx);

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
    xlog_list_free(options.logs);

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

#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

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
    FILE *          out;
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

/* *************** SIZEOF information ********************** */
#define PSIZEOF(type) LOG_INFO(NULL, "%20s: %zu", #type, sizeof(type))

static int test_sizeof(const options_test_t * opts) {
    int     nerrors = 0;
    (void)  opts;

    //printf("char % 10s: %d\n", fuck(char), sizeof(char));
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
    PSIZEOF(log_ctx_t);
    return nerrors;
}

/* *************** ASCII and TEST LOG BUFFER *************** */

static int test_ascii(options_test_t * opts) {
    int     nerrors = 0;
    int     result;
    char    ascii[256];
    (void)  opts;
    ssize_t n;

    LOG_INFO(NULL, ">>> %s\n", __func__);

    for (result = -128; result <= 127; result++) {
        ascii[result + 128] = result;
    }

    n = LOG_BUFFER(0, NULL, ascii, 256, "ascii> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : xlog_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, NULL, 256, "null_01> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : xlog_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, NULL, 0, "null_02> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : xlog_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, NULL, ascii, 0, "ascii_sz=0> ");
    if (n <= 0) {
        LOG_ERROR(NULL, "%s ERROR : xlog_buffer returns %z, expected >0", __func__, n);
        ++nerrors;
    }

    return nerrors;
}

/* *************** TEST OPTIONS *************** */

static int test_parse_options(int argc, const char *const* argv, options_test_t * opts) {
    opt_config_t    opt_config_test = { argc, argv, parse_option_test, s_opt_desc_test, VERSION_STRING, opts };
    int             result = opt_parse_options(&opt_config_test);

    fprintf(opts->out, "opt_parse_options() result: %d\n", result);
    if (result <= 0) {
        fprintf(opts->out, "%s(): ERROR opt_parse_options() expected >0, got %d\n", __func__, result);
        return 1;
    }
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

    LOG_INFO(NULL, ">>> %s\n", __func__);

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
    return errors;
}

/* *************** BENCH SENSOR VALUES CAST *************** */

static int test_sensor_value(options_test_t * opts) {
    BENCH_DECL(t);
    unsigned long r;
    unsigned long i;
    unsigned long nb_op = 50000000;
    (void) opts;

    LOG_INFO(NULL, ">>> %s\n", __func__);

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
        BENCH_STOP_PRINT(t, "sensor_value_toint r=%lu ", r);

        BENCH_START(t);
        for (i=0, r=0; i < nb_op; i++) {
            s1.value.data.i = r++;
            r += sensor_value_todouble(&s1.value) < sensor_value_todouble(&s2.value);
        }
        BENCH_STOP_PRINT(t, "sensor_value_todouble ");
        BENCH_START(t);
        BENCH_STOP_PRINT(t, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p ul=%lu ", r, "STRING", 'Z', (void*)&r, r);
    }
    return 0;
}

/* *************** TEST LOG THREAD *************** */
#define N_TEST_THREADS  100
#define PIPE_IN         0
#define PIPE_OUT        1
typedef struct {
    pthread_t           tid;
    log_ctx_t *         log;
} test_log_thread_t;

static void * log_thread(void * data) {
    test_log_thread_t * ctx = (test_log_thread_t *) data;
    unsigned long       tid = (unsigned long) pthread_self();
    int                 nerrors = 0;

    LOG_INFO(ctx->log, "Starting %s (tid:%lu)", ctx->log->prefix, tid);

    for (int i = 0; i < 1000; i++) {
        LOG_INFO(ctx->log, "Thread #%lu Loop #%d", tid, i);
    }
    return (void *) ((long)nerrors);
}

static void * pipe_log_thread(void * data) {
    FILE **     fpipe = (FILE **) data;
    char        buf[1024];
    size_t      n;
    int         fd_pipein = fileno(fpipe[PIPE_IN]);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while ((n = read(fd_pipein, buf, sizeof(buf))) > 0) {
        if (n > 0 && fwrite(buf, 1, n, fpipe[PIPE_OUT]) != n) {
            fprintf(stderr, "%s(): Error fwrite(fpipe): %s\n", __func__, strerror(errno));
        }
    }
    return NULL;
}

static int test_log_thread(options_test_t * opts) {
    const char * const  files[] = { "stdout", "/tmp/test_thread.log", NULL };
    test_log_thread_t   threads[N_TEST_THREADS];
    log_ctx_t           logs[N_TEST_THREADS];
    log_ctx_t           log = { .level = LOG_LVL_SCREAM, .flags = LOG_FLAG_DEFAULT | LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE | LOG_FLAG_LOC_TAIL };
    char                prefix[20];
    pthread_t           pipe_tid;
    int                 nerrors = 0;
    int                 i;
    (void)              opts;

    i = 0;
    for (const char * const * filename = files; *filename; filename++, i++) {
        FILE *  file;
        FILE *  fpipeout = NULL;
        FILE *  fpipein = NULL;
        int     p[2] = { -1, -1 };
        int     fd_pipein = -1;
        void *  thread_ret;
        BENCH_TM_DECL(t0);

        LOG_INFO(NULL, ">>> %s : file %s", __func__, *filename);
        BENCH_TM_START(t0);

        /* Create pipe to redirect stdout & stderr to pipe log file (fpipeout) */
        if ((!strcmp("stderr", *filename) && (file = stderr)) || (!strcmp(*filename, "stdout") && (file = stdout))) {
            FILE * fpipe[2];
            char path[PATH_MAX];

            snprintf(path, sizeof(path), "/tmp/test_thread_%s_%d.log", *filename, i);
            fpipeout = fopen(path, "w");
            if (fpipeout == NULL) {
                LOG_ERROR(NULL, "%s(): Error: create cannot create '%s': %s", __func__, path, strerror(errno));
                nerrors++;
                continue ;
            } else if (pipe(p) < 0) {
                LOG_ERROR(NULL, "%s(): ERROR pipe: %s", __func__, strerror(errno));
                nerrors++;
                fclose(fpipeout);
                continue ;
            } else if (dup2(p[PIPE_OUT], fileno(file)) < 0 || p[PIPE_IN] < 0) {
                LOG_ERROR(NULL, "%s(): ERROR dup2: %s", __func__, strerror(errno));
                nerrors++;
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                continue ;
            } else if ((fpipein = fdopen(p[PIPE_IN], "r")) == NULL) {
                LOG_ERROR(NULL, "%s(): ERROR, cannot fdopen p[PIPE_IN]: %s", __func__, strerror(errno));
                nerrors++;
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
        } else if ((file = fopen(*filename, "w")) == NULL)  {
            LOG_ERROR(NULL, "%s(): Error: create cannot create '%s': %s", __func__, *filename, strerror(errno));
            nerrors++;
            continue ;
        }

        /* Create log threads and launch them */
        log.out = file;
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
        /* terminate log_pipe thread */
        if (fd_pipein >= 0) {
            pthread_cancel(pipe_tid);
            thread_ret = NULL;
            pthread_join(pipe_tid, &thread_ret);
            fclose(fpipein); // fclose will close p[PIPE_IN]
            close(p[PIPE_OUT]);
            fclose(fpipeout);
        } else {
            fclose(file);
        }
        BENCH_TM_STOP(t0);
        LOG_INFO(NULL, "duration : %ld.%03lds", BENCH_TM_GET(t0) / 1000, BENCH_TM_GET(t0) % 1000);
    }
    /* compare logs */
    system("sed -e 's/^[^[]*//' -e s/'Thread #[0-9]*/Thread #X/' -e 's/tid:[0-9]*/tid:X/'"
           " /tmp/test_thread.log | sort > /tmp/test_thread_filtered.log");
    system("sed -e 's/^[^[]*//' -e s/'Thread #[0-9]*/Thread #X/' -e 's/tid:[0-9]*/tid:X/'"
           " /tmp/test_thread_stdout_0.log | sort > /tmp/test_thread_stdout_0_filtered.log");
    if (system("diff -q /tmp/test_thread_filtered.log /tmp/test_thread_stdout_0_filtered.log") != 0) {
        LOG_ERROR(NULL, "%s(): Error during logs comparison", __func__);
        nerrors++;
    }
    LOG_INFO(NULL, "<- %s(): ending with %d error(s).", __func__, nerrors);
    return nerrors;
}

/* *************** TEST BENCH *************** */

static int test_bench(options_test_t *opts) {
    BENCH_DECL(t0);
    BENCH_TM_DECL(tm0);
    const int step_ms = 1000;
    const unsigned char margin = 20;
    int nerrors = 0;

    fprintf(opts->out, "%s(): starting...\n", __func__);
    (void) opts;
    for (int i=0; i< 5000 / step_ms; i++) {
        time_t tm = time(NULL);
        BENCH_TM_START(tm0);
        BENCH_START(t0);
        while (time(NULL) <= tm)
            ;
        BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);
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
    return nerrors;
}

/* *************** TEST MAIN FUNC *************** */

int test(int argc, const char *const* argv, options_t *options) {
    options_test_t  options_test    = { .flags = FLAG_NONE, .test_mode = 0, .logs = NULL, .out = stderr };
    int             errors = 0;

    fprintf(options_test.out, "\n>>> TEST MODE: %d\n\n", options->test_mode);

    /* Manage test program options */
    if ((options->test_mode & (1 << TEST_options)) != 0)
        errors += test_parse_options(argc, argv, &options_test);

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
    if ((options->test_mode & (1 << TEST_bench)) != 0)
        errors += test_sensor_value(&options_test);

    /* Test Log in multiple threads */
    if ((options->test_mode & (1 << TEST_log)) != 0)
        errors += test_log_thread(&options_test);

    // *****************************************************************
    fprintf(options_test.out, "\n<<< END of Tests : %d error(s).\n\n", errors);

    return -errors;
}
#endif /* ! ifdef _TEST */

