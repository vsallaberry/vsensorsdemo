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

#include "vlib/hash.h"
#include "vlib/util.h"
#include "vlib/options.h"
#include "vlib/time.h"
#include "vlib/account.h"
#include "vlib/thread.h"
#include "vlib/rbuf.h"
#include "vlib/avltree.h"
#include "vlib/logpool.h"
#include "vlib/term.h"
#include "vlib/job.h"
#include "vlib/test.h"

#include "libvsensors/sensor.h"

#include "version.h"
//#include "vsensors.h"
#include "test/test.h"
#include "test_private.h"

#define TEST_EXPERIMENTAL_PARALLEL // TODO

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS("TEST-" BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2020")

static void *   test_options(void * vdata);
static void *   test_tests(void * vdata);
static void *   test_optusage(void * vdata);
static void *   test_sizeof(void * vdata);
static void *   test_ascii(void * vdata);
static void *   test_colors(void * vdata);
static void *   test_bench(void * vdata);
void *          test_math(void * vdata);
static void *   test_list(void * vdata);
static void *   test_hash(void * vdata);
static void *   test_rbuf(void * vdata);
static void *   test_avltree(void * vdata);
static void *   test_sensor_value(void * vdata);
void *          test_sensor_plugin(void * vdata);
static void *   test_account(void * vdata);
static void *   test_bufdecode(void * vdata);
static void *   test_srcfilter(void * vdata);
static void *   test_logpool(void * vdata);
static void *   test_job(void * vdata);
static void *   test_thread(void * vdata);
static void *   test_log_thread(void * vdata);
static void *   test_optusage_stdout(void * vdata);

/** strings corresponding of unit tests ids , for command line, in same order as s_testmode_str */
enum testmode_t {
    TEST_all = 0,
    TEST_options,
    TEST_tests,
    TEST_optusage,
    TEST_sizeof,
    TEST_ascii,
    TEST_color,
    TEST_bench,
    TEST_math,
    TEST_list,
    TEST_hash,
    TEST_rbuf,
    TEST_tree,
    TEST_sensorvalue,
    TEST_sensorplugin,
    TEST_account,
    TEST_bufdecode,
    TEST_srcfilter,
    TEST_logpool,
    TEST_job,
    TEST_vthread,
    TEST_log,
    /* starting from here, tests are not included in 'all' by default */
    TEST_excluded_from_all,
    TEST_bigtree = TEST_excluded_from_all,
    TEST_optusage_big,
    TEST_optusage_stdout,
    TEST_logpool_big,
    TEST_PARALLEL,
    TEST_NB /* Must be LAST ! */
};
#define TEST_MASK(id)       (1UL << ((unsigned int) (id)))
#define TEST_MASK_ALL       ((1UL << (sizeof(unsigned int) * 8)) - 1)
static const struct {
    const char *    name;
    void *          (*fun)(void*);
    unsigned int    mask; /* set of tests preventing this one to run */
} s_testconfig[] = {
    { "all",                NULL,               0 },
    { "options",            test_options,       0 },
    { "tests",              test_tests,         0 },
    { "optusage",           test_optusage,      0 },
    { "sizeof",             test_sizeof,        0 },
    { "ascii",              test_ascii,         0 },
    { "color",              test_colors,        0 },
    { "math",               test_math,          0 },
    { "list",               test_list,          0 },
    { "hash",               test_hash,          0 },
    { "rbuf",               test_rbuf,          0 },
    { "tree",               test_avltree,       0 },
    { "sensorvalue",        test_sensor_value,  0 },
    { "sensorplugin",       test_sensor_plugin, 0 },
    { "account",            test_account,       0 },
    { "bufdecode",          test_bufdecode,     0 },
    { "srcfilter",          test_srcfilter,     0 },
    { "logpool",            test_logpool,       0 },
    { "job",                test_job,           0 },
    { "bench",              test_bench,         TEST_MASK_ALL },
    { "vthread",            test_thread,        TEST_MASK_ALL },
    { "log",                test_log_thread,    TEST_MASK_ALL },
    /* Excluded from all */
    { "bigtree",            NULL,               0 },
    { "optusage_big",       NULL,               0 },
    { "optusage_stdout",    test_optusage_stdout,TEST_MASK_ALL },
    { "logpool_big",        NULL,               0 },
#ifdef TEST_EXPERIMENTAL_PARALLEL
    { "EXPERIMENTAL_parallel", NULL,            0 },
#endif
    { NULL, NULL, 0 } /* Must be last */
};

enum {
    long1 = OPT_ID_USER,
    long2,
    long3,
    long4,
    long5,
    long6,
    long7
};

static const opt_options_desc_t s_opt_desc_test[] = {
    { OPT_ID_SECTION, NULL, "options", "Options:" },
    { 'a', NULL,        NULL,           "test NULL long_option" },
    { 'h', "help",      "[filter1[,...]]",     "show usage\r" },
    { 'h', "show-help", NULL,           NULL },
    { 'T', NULL, "[tests]", NULL },
    { OPT_ID_USER,"help=null", NULL,  "show null options" },
    { 'l', "log-level", "level",        "Set log level "
                                        "[module1=]level1[@file1][:flag1[|flag2]][,...]\r" },
    { OPT_ID_SECTION+1, NULL, "null", "\nNULL desc tests:" },
    { 'N', "--NULL-desc", NULL, NULL },
    { OPT_ID_ARG, NULL, "simpleargs0", "[simple_args0]" },
    /* those are bad IDs, but tested as allowed before OPT_DESCRIBE_OPTION feature introduction */
    { OPT_ID_SECTION+2, NULL, "longonly_legacy", "\nlong-only legacy tests:" },
    {  -1, "long-only-1", "", "" },
    {  -127, "long-only-2", "", "" },
    {  -128, "long-only-3", "", "" },
    {  -129, "long-only-4", "Patata patata patata.", "Potato potato potato potato." },
    {  128, "long-only-5", "Taratata taratata taratata.", "Tirititi tirititi tirititi\nTorototo torototo" },
    {  256, "long-only-6", "", "" },
    /* correct ids using OPT_ID_USER */
    { OPT_ID_SECTION+3, NULL, "longonly", "\nlong-only tests:" },
    {  long1, "long-only1", "", "" },
    {  long2, "long-only2", "", "abcdedfghijkl mno pqrstuvwxyz ABCDEF GHI JK LMNOPQRSTU VWXYZ 01234567890" },
    {  long3, "long-only3", NULL, NULL },
    {  long4, "long-only4", "Patata patata patata.", "Potato potato potato potato." },
    {  long5, "long-only5", "Taratata taratata taratata.", "Tirititi tirititi tirititi\n"
                            "Torototo torototo abcdedfghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890" },
    {  long4, "long-only4-alias", NULL, NULL },
    {  long6, "long-only6", "", "" },
    {  long7, "long-only7", "Taratata taratata taratata toroto.", NULL },
    {  long4, "long-only4-alias-2", NULL, NULL },
    { OPT_ID_SECTION+4, NULL, "fakeshort", "fake short options" },
    { 'b', NULL,        NULL,           NULL },
    { 'c', NULL,        NULL,           NULL },
    { 'd', NULL,        NULL,           NULL },
    { 'e', NULL,        NULL,           NULL },
    { 'f', NULL,        NULL,           NULL },
    { 'g', NULL,        NULL,           NULL },
    { 'h', NULL,        NULL,           NULL },
    { 'i', NULL,        NULL,           NULL },
    { 'j', NULL,        NULL,           NULL },
    { 'k', NULL,        NULL,           NULL },
    { 'l', NULL,        NULL,           NULL },
    { 'm', NULL,        NULL,           NULL },
    { 'n', NULL,        NULL,           NULL },
    { 'o', NULL,        NULL,           NULL },
    { 'p', NULL,        NULL,           NULL },
    { 'q', NULL,        NULL,           NULL },
    { 'r', NULL,        NULL,           NULL },
    { 't', NULL,        NULL,           NULL },
    { 'u', NULL,        NULL,           NULL },
    { 'v', NULL,        NULL,           NULL },
    { 'w', NULL,        NULL,           NULL },
    { 'x', NULL,        NULL,           NULL },
    { 'y', NULL,        NULL,           NULL },
    { 'z', NULL,        NULL,           NULL },
    { 'A', NULL,        NULL,           NULL },
    { 'B', NULL,        NULL,           NULL },
    { 'C', NULL,        NULL,           NULL },
    { 'D', NULL,        NULL,           NULL },
    { 'E', NULL,        NULL,           NULL },
    { 'F', NULL,        NULL,           NULL },
    { 'G', NULL,        NULL,           NULL },
    { 'H', NULL,        NULL,           NULL },
    { 'I', NULL,        NULL,           NULL },
    { 'K', NULL,        NULL,           NULL },
    { 'L', NULL,        NULL,           NULL },
    { 'M', NULL,        NULL,           NULL },
    { 'N', NULL,        NULL,           NULL },
    { 'O', NULL,        NULL,           NULL },
    { 'P', NULL,        NULL,           NULL },
    { 'Q', NULL,        NULL,           NULL },
    { 'R', NULL,        NULL,           NULL },
    { 'S', NULL,        NULL,           NULL },
    { 'U', NULL,        NULL,           NULL },
    { 'V', NULL,        NULL,           NULL },
    { 'W', NULL,        NULL,           NULL },
    { 'X', NULL,        NULL,           NULL },
    { 'Y', NULL,        NULL,           NULL },
    { 'Z', NULL,        NULL,           NULL },
    { OPT_ID_SECTION+5, NULL, "arguments", "\nArguments:" "aaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa" "aaaaaaaaaaaaaaaaaaaaaaaaa" },
    { OPT_ID_ARG+1, NULL, "[program [arguments]]", "program and its arguments" },
    { OPT_ID_ARG+2, NULL, "[second_program [arguments]]", "program and its arguments - " },
    /* end of table */
    { 0, NULL, NULL, NULL }
};

/** parse_option() : option callback of type opt_option_callback_t. see options.h */
static int parse_option_test(int opt, const char *arg, int *i_argv, opt_config_t * opt_config) {
    options_test_t *options = (options_test_t *) opt_config->user_data;
    (void) options;
    (void) i_argv;
    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        switch (opt & OPT_OPTION_FLAG_MASK) {
            case 'h':
                return opt_describe_filter(opt, arg, i_argv, opt_config);
            case 'l':
                return log_describe_option((char *)arg, i_argv, NULL, NULL, NULL);
            case OPT_ID_SECTION:
                snprintf((char*)arg, *i_argv,
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa "
                         "uuuuuuuuuuuuuuuuu uuuuuuuuuuu uuuuuuuu uuuuuu 00000000 111 "
                         "222222222 3 4444444444 5555555555 66666 77 888 8 999999990");
                break ;
            case OPT_ID_ARG+2:
                snprintf((char*)arg, *i_argv,
                        "000 uuuuuuuuuuuuuuuuu uuuuuuuuuuu uuuuuuuu uuuuuu 00000000 111 "
                        "222222222 3 4444444444 5555555555 66666 77 888 8 999999990");
                break ;
            default:
                return OPT_EXIT_OK(0);
        }
        return OPT_CONTINUE(1);
    }
    switch (opt) {
    case 'T': {
        unsigned int testid;
        if ((testid = test_getmode(arg)) == 0) {
            options->test_mode = 0;
            return OPT_ERROR(2);
        }
        options->test_mode |= testid;
        break ;
    }
    case 'w': case 'C':
        break ;
    case 'h':
        return opt_usage(OPT_EXIT_OK(0), opt_config, arg);
    case 'l':
        /* TODO/FIXME: currently, updating logs which have already been retrieved
         * with logpool_getlog(), makes the previous ones unusable without
         * a new call to logpool_getlog()
        if ((options->logs = logpool_create_from_cmdline(options->logs, arg, NULL)) == NULL)
            return OPT_ERROR(OPT_EBADARG);*/
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

int test_describe_filter(int short_opt, const char * arg, int * i_argv,
                         const opt_config_t * opt_config) {
    int             n = 0, ret;
    char            sep[2]= { 0, 0 };
    (void) short_opt;
    (void) opt_config;

    n += VLIB_SNPRINTF(ret, (char *) arg + n, *i_argv - n,
                         "- test modes: '");

    for (unsigned int i = 0; i < PTR_COUNT(s_testconfig)
                             && s_testconfig[i].name != NULL; ++i, *sep = ',') {
        const char * mode = s_testconfig[i].name;
        n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s", sep, mode);
    }

    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n,
                       "' (fnmatch(3) pattern). \r- Excluded from '%s':'",
                       s_testconfig[TEST_all].name);
    *sep = 0;
    for (unsigned i = TEST_excluded_from_all; i < TEST_NB; i++, *sep = ',') {
        if (i < PTR_COUNT(s_testconfig)) {
            n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s",
                    sep, s_testconfig[i].name);
        }
    }
    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "'");

    *i_argv = n;
    return OPT_CONTINUE(1);
}

unsigned int test_getmode(const char *arg) {
    char token0[128];
    char * endptr = NULL;
    const unsigned int test_mode_all = TEST_MASK(TEST_excluded_from_all) - 1; //0xffffffffU;
    unsigned int test_mode = test_mode_all;
    if (arg != NULL) {
        errno = 0;
        test_mode = strtol(arg, &endptr, 0);
        if (errno != 0 || endptr == NULL || *endptr != 0) {
            const char * token, * next = arg;
            size_t len, i;
            while ((len = strtok_ro_r(&token, ",~", &next, NULL, 0)) > 0 || *next) {
                int is_pattern;
                len = strn0cpy(token0, token, len, sizeof(token0) / sizeof(*token0));
                is_pattern = (fnmatch_patternidx(token0) >= 0);
                for (i = 0; i < PTR_COUNT(s_testconfig) && s_testconfig[i].name != NULL; i++) {
                    if ((is_pattern && 0 == fnmatch(token0, s_testconfig[i].name, FNM_CASEFOLD))
                    ||  (!is_pattern && (0 == strncasecmp(s_testconfig[i].name, token0, len)
                                         &&  s_testconfig[i].name[len] == 0))) {
                        if (token > arg && *(token - 1) == '~') {
                            test_mode &= (i == TEST_all ? ~test_mode_all : ~TEST_MASK(i));
                        } else {
                            test_mode |= (i == TEST_all ? test_mode_all : TEST_MASK(i));
                        }
                        if ((++is_pattern) == 1)
                            break ;
                    }
                }
                if (s_testconfig[i].name == NULL && is_pattern <= 1) {
                    fprintf(stderr, "%s%serror%s: %sunreconized test id '",
                            vterm_color(STDERR_FILENO, VCOLOR_RED),
                            vterm_color(STDERR_FILENO, VCOLOR_BOLD),
                            vterm_color(STDERR_FILENO, VCOLOR_RESET),
                            vterm_color(STDERR_FILENO, VCOLOR_BOLD));
                    fwrite(token, 1, len, stderr);
                    fprintf(stderr, "'%s\n", vterm_color(STDERR_FILENO, VCOLOR_RESET));
                    return 0;
                }
            }
        }
    }
    return test_mode;
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
#define PSIZEOF(type, log)              \
            LOG_INFO(log, "%32s: %zu", #type, sizeof(type))
#define POFFSETOF(type, member, log)    \
            LOG_INFO(log, "%32s: %zu", "off(" #type "." #member ")", offsetof(type, member))


#define TYPESTR2(type) #type
#define TYPESTR(type) TYPESTR2(type)
#define TYPEMINSTR(type) TYPESTR(type##_MIN)
#define TYPEMAXSTR(type) TYPESTR(type##_MAX)
#define PMINMAX(type, log)              \
            do { \
                LOG_INFO(log, "%32s: min:%30s", #type, TYPEMINSTR(type)); \
                LOG_INFO(log, "%32s: max:%30s", #type, TYPEMAXSTR(type)); \
            } while(0);

union testlongdouble_u {
    double d;
    long double ld;
};
/* compiler extends struct to 16 bytes instead of 8 because of
 * struct boundary on long double (16 bytes) */
struct testlongdouble_s {
    union {
        long double ld;
        double d;
    } u;
    long int a;
};
struct testlongdoublepacked_s {
    char type;
    union {
        double d;
        long double ld __attribute__((__packed__));
    } u;
};

static void * test_sizeof(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "SIZE_OF");
    log_t *         log = test != NULL ? test->log : NULL;

    PSIZEOF(char, log);
    PSIZEOF(unsigned char, log);
    PSIZEOF(short, log);
    PSIZEOF(unsigned short, log);
    PSIZEOF(int, log);
    PSIZEOF(unsigned int, log);
    PSIZEOF(long, log);
    PSIZEOF(unsigned long, log);
    PSIZEOF(long long, log);
    PSIZEOF(unsigned long long, log);
    PSIZEOF(uint16_t, log);
    PSIZEOF(int16_t, log);
    PSIZEOF(uint32_t, log);
    PSIZEOF(int32_t, log);
    PSIZEOF(uint64_t, log);
    PSIZEOF(int64_t, log);
    PSIZEOF(intmax_t, log);
    PSIZEOF(uintmax_t, log);
    PSIZEOF(size_t, log);
    PSIZEOF(time_t, log);
    PSIZEOF(float, log);
    PSIZEOF(double, log);
    PSIZEOF(long double, log);
    PSIZEOF(union testlongdouble_u, log);
    PSIZEOF(struct testlongdouble_s, log);
    PSIZEOF(struct testlongdoublepacked_s, log);
    PSIZEOF(char *, log);
    PSIZEOF(unsigned char *, log);
    PSIZEOF(void *, log);
    PSIZEOF(struct timeval, log);
    PSIZEOF(struct timespec, log);
    PSIZEOF(fd_set, log);
    PSIZEOF(log_t, log);
    PSIZEOF(avltree_t, log);
    PSIZEOF(avltree_node_t, log);
    PSIZEOF(avltree_visit_context_t, log);
    PSIZEOF(vterm_screen_ev_data_t, log);
    PSIZEOF(sensor_watch_t, log);
    PSIZEOF(sensor_value_t, log);
    PSIZEOF(sensor_sample_t, log);
    POFFSETOF(avltree_node_t, left, log);
    POFFSETOF(avltree_node_t, right, log);
    POFFSETOF(avltree_node_t, data, log);
    POFFSETOF(avltree_node_t, balance, log);
    PMINMAX(INT, log);
    PMINMAX(UINT, log);
    PMINMAX(INT64, log);
    PMINMAX(UINT64, log);
    PMINMAX(INTMAX, log);
    PMINMAX(UINTMAX, log);

    avltree_node_t * node, * nodes[1000];
    unsigned char a = CHAR_MAX;
    for (unsigned i = 0; i < 1000; i++) {
        unsigned char b;
        node = malloc(sizeof(avltree_node_t));
        for (b = 0; b < 16; b++)
            if ((((unsigned long)node) & (1 << b)) != 0) break ;
        nodes[i] = node;
        if (b < a) a = b;
    }
    LOG_INFO(log, "alignment of node: %u bytes", a + 1);
    TEST_CHECK(test, "align > 1", a + 1 > 1);
    for (unsigned i = 0; i < 1000; i++) {
        if (nodes[i]) free(nodes[i]);
    }

    return VOIDP(TEST_END(test));
}

/* *************** ASCII and TEST LOG BUFFER *************** */

static void * test_ascii(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "ASCII_LOGBUF");
    log_t *         log = test != NULL ? test->log : NULL;
    char            ascii[256];
    ssize_t         n;

    for (int i_ascii = -128; i_ascii <= 127; ++i_ascii) {
        ascii[i_ascii + 128] = i_ascii;
    }

    if (!LOG_CAN_LOG(log, LOG_LVL_INFO)) {
        LOG_WARN(log, "%s(): cannot perform full tests, log level too low", __func__);
        TEST_CHECK(test, "LOG_BUFFER(ascii)",
                   (n = LOG_BUFFER(LOG_LVL_INFO, log, ascii, 256, "ascii> ")) == 0);
        TEST_CHECK2(test, "LOG_BUFFER(NULL)%s",
                    (n = LOG_BUFFER(LOG_LVL_INFO, log, NULL, 256, "null_01> ")) == 0,"");
        return VOIDP(TEST_END(test));
    }

    TEST_CHECK(test, "LOG_BUFFER(ascii)", (n = LOG_BUFFER(0, log, ascii, 256, "ascii> ")) > 0);
    TEST_CHECK2(test, "LOG_BUFFER(NULL)%s", (n = LOG_BUFFER(0, log, NULL, 256, "null_01> ")) > 0,"");
    TEST_CHECK(test, "LOG_BUFFER(NULL2)", (n = LOG_BUFFER(0, log, NULL, 0, "null_02> ")) > 0);
    TEST_CHECK(test, "LOG_BUFFER(sz=0)", (n = LOG_BUFFER(0, log, ascii, 0, "ascii_sz=0> ")) > 0);

    return VOIDP(TEST_END(test));
}

/* *************** TEST COLORS *****************/
static void * test_colors(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "COLORS");
    log_t *         log = test != NULL ? test->log : NULL;
    FILE *          out;
    int             fd;

    if (!LOG_CAN_LOG(log, LOG_LVL_INFO)) {
        LOG_WARN(log, "%s(): cannot perform full tests, log level too low", __func__);
    } else for (int f=VCOLOR_FG; f < VCOLOR_BG; f++) {
        for (int s=VCOLOR_STYLE; s < VCOLOR_RESERVED; s++) {
            out = log_getfile_locked(log);
            fd = fileno(out);
            log_header(LOG_LVL_INFO, log, __FILE__, __func__, __LINE__);
            fprintf(out, "hello %s%sWORLD",
                     vterm_color(fd, s), vterm_color(fd, f));
            for (int b=VCOLOR_BG; b < VCOLOR_STYLE; b++) {
                fprintf(out, "%s %s%s%sWORLD", vterm_color(fd, VCOLOR_RESET),
                        vterm_color(fd, f), vterm_color(fd, s), vterm_color(fd, b));
            }
            fprintf(out, "%s!\n", vterm_color(fd, VCOLOR_RESET));
            funlockfile(out);
        }
    }
    fd = fileno((log && log->out) ? log->out : stderr);
    LOG_INFO(log, "%shello%s %sworld%s !", vterm_color(fd, VCOLOR_GREEN),
             vterm_color(fd, VCOLOR_RESET), vterm_color(fd, VCOLOR_RED),
             vterm_color(fd, VCOLOR_RESET));

    LOG_VERBOSE(log, "res:\x1{green}");
    LOG_VERBOSE(log, "res:\x1{green}");

    LOG_BUFFER(LOG_LVL_INFO, log, vterm_color(fd, VCOLOR_RESET),
               vterm_color_size(fd, VCOLOR_RESET), "vcolor_reset ");

    int has_colors = vterm_has_colors(STDOUT_FILENO);
    unsigned int maxsize = vterm_color_maxsize(STDOUT_FILENO);

    TEST_CHECK(test, "color_maxsize(-1)", vterm_color_maxsize(-1) == 0);
    if (has_colors) {
        TEST_CHECK(test, "color_maxsize(stdout)", maxsize > 0);
    } else {
        TEST_CHECK(test, "color_maxsize(stdout)", maxsize == 0);
    }
    for (int c = VCOLOR_FG; c <= VCOLOR_EMPTY; ++c) {
        if (has_colors) {
            TEST_CHECK2(test, "color_size(stdout, %d)",
                        vterm_color_size(STDOUT_FILENO, c) <= maxsize, c);
        } else {
            TEST_CHECK2(test, "color_size(stdout, %d)", vterm_color_size(STDOUT_FILENO, c) == 0, c);
        }
        TEST_CHECK2(test, "color_size(-1, %d)", vterm_color_size(-1, c) == 0, c);
    }
    //TODO log to file and check it does not contain colors

    return VOIDP(TEST_END(test));
}

/* *************** TEST OPTIONS *************** */

#define OPTUSAGE_PIPE_END_LINE  "#$$^& TEST_OPT_USAGE_END &^$$#" /* no EOL here! */

typedef struct {
    opt_config_t *  opt_config;
    const char *    filter;
    int             ret;
} optusage_data_t;

void * optusage_run(void * vdata) {
    optusage_data_t *   data = (optusage_data_t *) vdata ;
    FILE *              out;

    data->ret = opt_usage(0, data->opt_config, data->filter);

    out = log_getfile_locked(data->opt_config->log);
    fprintf(out, "\n%s\n", OPTUSAGE_PIPE_END_LINE);
    fflush(out);
    funlockfile(out);

    return NULL;
}

/* test with options logging to file */
static void * test_optusage(void * vdata) {
    options_test_t *opts = (options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTUSAGE");
    log_t *         log = test != NULL ? test->log : NULL;

    int             argc = opts->argc;
    const char*const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                            s_opt_desc_test, VERSION_STRING, opts);
    int             columns, desc_minlen, desc_align;

    log_t   optlog = { .level = LOG_LVL_INFO, .out = NULL, .prefix = "opttests",
                       .flags = LOG_FLAG_LEVEL | LOG_FLAG_MODULE };

    char    tmpname[PATH_MAX];
    int     tmpfdout, tmpfdin;
    FILE *  tmpfilein = NULL, * tmpfileout = NULL;
    char *  line;
    size_t  line_allocsz;
    ssize_t len;
    size_t  n_lines = 0, n_chars = 0, chars_max = 0;
    size_t  n_lines_all = 0, n_chars_all = 0, chars_max_all = 0;
    int     pipefd[2];
    int     ret;

    if (test == NULL) {
        return VOIDP(1);
    }

    TEST_CHECK(test, "pipe creation",
        (ret = (pipe(pipefd) == 0 && (tmpfileout = fdopen((tmpfdout = pipefd[1]), "w")) != NULL
         && (tmpfilein = fdopen((tmpfdin = pipefd[0]), "r")) != NULL)));
    if (ret == 0 || tmpfilein == NULL || tmpfileout == NULL) {
        return VOIDP(TEST_END(test));
    }
    tmpfdin = pipefd[0];
    str0cpy(tmpname, "pipe", sizeof(tmpname)/sizeof(*tmpname));

    /* update opt_config with testlog instance */
    opt_config_test.log = &optlog;
    optlog.out = tmpfileout;

    /** don't set because O_NONBLOCK issues with glibc getline
    while (0&&(fcntl(tmpfdin, F_SETFL, O_NONBLOCK)) < 0) {
        if (errno == EINTR) continue;
        LOG_ERROR(log, "%s(): exiting: fcntl(pipe,nonblock) failed : %s",
            __func__, strerror(errno));
        return ++nerrors;
    }
    */

    if ((line = malloc(1024 * sizeof(char))) != NULL) {
        line_allocsz = 1024 * sizeof(char);
    } else {
        line_allocsz = 0;
    }

    /* columns are 80 + log header size with options logging */
    columns = 80 + log_header(LOG_LVL_INFO, &optlog, __FILE__, __func__, __LINE__);
    fputc('\n', tmpfileout); fflush(tmpfileout); getline(&line, &line_allocsz, tmpfilein);

    static const unsigned int flags[] = {
        OPT_FLAG_DEFAULT,
        OPT_FLAG_NONE,
        OPT_FLAG_TRUNC_EOL,
        OPT_FLAG_TRUNC_COLS,
        OPT_FLAG_TRUNC_EOL | OPT_FLAG_TRUNC_COLS | OPT_FLAG_MIN_DESC_ALIGN,
    };

    static const char * desc_heads[] = { "", OPT_USAGE_DESC_HEAD, " : ",  NULL };
    static const char * opt_heads[] =  { "", OPT_USAGE_OPT_HEAD,  "--> ", NULL };
    static const char * filters[] = { NULL, "options", "all", (void *) -1 };

    for (unsigned int i_flg = 0; i_flg < sizeof(flags) / sizeof(*flags); ++i_flg) {
        opt_config_test.flags = flags[i_flg] | OPT_FLAG_MACROINIT;
        for (desc_minlen = 0; desc_minlen < columns + 5; ++desc_minlen) {
            /* skip some tests if not in big test mode */
            if ((opts->test_mode & TEST_MASK(TEST_optusage_big)) == 0
                    && (desc_minlen > 5 && desc_minlen < columns - 5)
                    && desc_minlen != 15 && desc_minlen != 80
                    && desc_minlen != 20 && desc_minlen != 30 && desc_minlen != 40
                    && desc_minlen != OPT_USAGE_DESC_MINLEN)
                continue ;

            for (desc_align = 0; desc_align < columns + 5; ++desc_align) {
                /* skip some tests if not in big test mode */
                if ((opts->test_mode & TEST_MASK(TEST_optusage_big)) == 0
                        && (desc_align > 5 && desc_align < columns - 5)
                        && desc_align != 15 && desc_align != 80
                        && desc_align != 18 && desc_align != 30
                        && desc_align != OPT_USAGE_DESC_ALIGNMENT)
                    continue ;

                const char * const * opt_head = opt_heads, *const* desc_head;
                for (desc_head = desc_heads; *desc_head; ++desc_head, ++opt_head) {

                    for (const char *const* filter = filters; *filter != (void*)-1; ++filter) {
                        optusage_data_t data = { &opt_config_test, *filter, 0 };
                        pthread_t       tid;
                        fd_set          set_in;

                        opt_config_test.log->out = tmpfileout;
                        opt_config_test.desc_minlen = desc_minlen;
                        opt_config_test.desc_align = desc_align;
                        opt_config_test.opt_head = *opt_head;
                        opt_config_test.desc_head = *desc_head;

                        LOG_SCREAM(log,
                         "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d}): checking",
                            *filter ? *filter : "",
                            flags[i_flg], i_flg, *opt_head, *desc_head,
                            desc_minlen, desc_align);

                        /* launch opt_usage() in a thread to avoid pipe full */
                        TEST_CHECK(test, "opt_usage thread creation",
                                (ret = pthread_create(&tid, NULL, optusage_run, &data)) == 0);
                        if (ret != 0) {
                            break ;
                        }

                        /* check content of options log file */
                        ret = 0;
                        while (ret >= 0) {
                            FD_ZERO(&set_in);
                            FD_SET(fileno(tmpfilein),&set_in);
                            ret = select(fileno(tmpfilein)+1, &set_in, NULL, NULL, NULL);
                            if (ret < 0) {
                                if (errno == EINTR) continue;
                                break ;
                            }
                            if (ret == 0) continue;

                            while ((len = getline(&line, &line_allocsz, tmpfilein)) > 0) {
                                /* ignore ending '\n' */
                                if (line[len-1] == '\n') {
                                    line[len-1] = 0;
                                    --len;
                                } else if (len < (ssize_t)line_allocsz) {
                                    line[len] = 0;
                                }
                                LOG_SCREAM(log, "optusage: line #%zd '%s'", len, line);

                                if (!strcmp(line, OPTUSAGE_PIPE_END_LINE)) {
                                    ret = -1;
                                    break ;
                                }

                                /* update counters */
                                if (*filter != NULL) {
                                    ++n_lines_all;
                                    if ((size_t) len > chars_max_all) chars_max_all = len;
                                    n_chars_all += len;
                                } else {
                                    ++n_lines;
                                    if ((size_t) len > chars_max) chars_max = len;
                                    n_chars += len;
                                }

                                /* check bad characters in line */
                                for (ssize_t i = 0; i < len; ++i) {
                                    int c = line[i];
                                    if (!isprint(c) || c == 0 ||  c == '\n' || c == '\r') {
                                        line[i] = '?';
                                        TEST_CHECK2(test,
                                            "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})"
                                            " printable character? 0x%02x : '%s'", (0),
                                            *filter ? *filter : "",
                                            flags[i_flg], i_flg, *opt_head, *desc_head,
                                            desc_minlen, desc_align, (unsigned int)line[i], line);
                                    }
                                }
                                /* check length of line */
                                /* note that current implementation does not force spitting words
                                 * that do not contain delimiters, then length of lines will
                                 * be checked only if (OPT_FLAG_TRUNC_COLS is ON without filter)
                                 * or if (desc align < 40 and filter is 'options' (a section
                                 * that contains delimiters) */
                                if (len > columns
                                &&  ((*filter == NULL
                                        && (opt_config_test.flags & OPT_FLAG_TRUNC_COLS) != 0)
                                     || (desc_align < 40 && *filter && !strcmp(*filter, "options")))
                                            && strstr(line, OPT_VERSION_STRING("TEST-" BUILD_APPNAME,
                                                      APP_VERSION, "git:" BUILD_GITREV)) == NULL) {
                                    TEST_CHECK2(test,
                                     "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})"
                                        ": line len (%zd) <= max (%d)  : '%s'", (0),
                                        *filter ? *filter : "",
                                        flags[i_flg], i_flg, *opt_head, *desc_head,
                                        desc_minlen, desc_align, len, columns, line);
                                }
                            } /* getline */
                        } /* select */

                        pthread_join(tid, NULL);
                        TEST_CHECK2(test,
                             "result optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})",
                                (! OPT_IS_ERROR(data.ret)),
                                *filter ? *filter : "",
                                flags[i_flg], i_flg, *opt_head, *desc_head,
                                desc_minlen, desc_align);
                    } /* filter */
                } /* desc_head / opt_head */
            } /* desc_align*/
        } /* desc_min*/
    } /* flags */

    if (line != NULL)
        free(line);
    if (tmpfilein != tmpfileout)
        fclose(tmpfilein);
    fclose(tmpfileout);

    LOG_INFO(log, "%s(): %zu log lines processed of average length %zu, max %zu (limit:%d)",
        __func__, n_lines, n_lines?(n_chars / n_lines):0, chars_max, columns);
    LOG_INFO(log, "%s(all): %zu log lines processed of average length %zu, max %zu (limit:%d)",
        __func__, n_lines_all, n_lines_all?(n_chars_all / n_lines_all):0, chars_max_all, columns);

    return VOIDP(TEST_END(test));
}

static void * test_options(void * vdata) {
    options_test_t *opts = (options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTIONS");
    log_t *         log = test != NULL ? test->log : NULL;

    int             argc = opts->argc;
    const char*const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, VERSION_STRING, opts);
    int             result;

    opt_config_test.log = logpool_getlog(opts->logs, LOG_OPTIONS_PREFIX_DEFAULT, LPG_NODEFAULT);

    /* test opt_parse_options with default flags */
    result = opt_parse_options(&opt_config_test);
    LOG_INFO(log, ">>> opt_parse_options() result: %d", result);
    TEST_CHECK2(test, "opt_parse_options() expected >0, got %d", (result > 0), result);

    /* test opt_parse_options with only main section in usage */
    opt_config_test.flags |= OPT_FLAG_MAINSECTION;
    result = opt_parse_options(&opt_config_test);
    LOG_INFO(log, ">>> opt_parse_options(shortusage) result: %d", result);
    TEST_CHECK2(test, "opt_parse_options(shortusage) expected >0, got %d", result > 0, result);

    opt_config_test.flags &= ~OPT_FLAG_MAINSECTION;
    logpool_release(opts->logs, opt_config_test.log);

    return VOIDP(TEST_END(test));
}

/* *************** TEST OPTUSAGE *************** */

static void * test_optusage_stdout(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTUSAGE_STDOUT");
    log_t *         log = test != NULL ? test->log : NULL;
    int             argc = opts->argc;
    const char *const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                s_opt_desc_test, VERSION_STRING, (void*)opts);
    int             columns, desc_minlen;

    LOG_INFO(log, ">>> OPTUSAGE STDOUT tests");

    opt_config_test.log = logpool_getlog(opts->logs, "options", LPG_NODEFAULT);

    /* test opt_usage with different columns settings */
    char * env_columns = getenv("COLUMNS");
    char num[10];
    //int columns = 70; {
    for (columns = 0; columns < 120; ++columns) {
        snprintf(num, sizeof(num) / sizeof(*num), "%d", columns);
        setenv("COLUMNS", num, 1);
        vterm_free();
        vterm_color(STDOUT_FILENO, VCOLOR_EMPTY); /* run vterm_init() with previous flags */
        for (desc_minlen = 0; desc_minlen < columns + 10; ++desc_minlen) {
            LOG_VERBOSE(log, "optusage_stdout tests: cols:%d descmin:%d", columns, desc_minlen);
            opt_config_test.desc_minlen = desc_minlen;
            TEST_CHECK(test, "opt_usage(NULL filter)",
                    OPT_IS_EXIT_OK(opt_usage(0, &opt_config_test, NULL)));
            TEST_CHECK(test, "opt_usage(all filter)",
                    OPT_IS_EXIT_OK(opt_usage(0, &opt_config_test, "all")));
        }
    }

    /* restore COLUMNS env */
    if (env_columns == NULL) {
        unsetenv("COLUMNS");
    } else {
        setenv("COLUMNS", env_columns, 1);
    }
    vterm_free();

    return VOIDP(TEST_END(test));
}
/* *************** TEST LIST *************** */

static int intcmp(const void * a, const void *b) {
    return (long)a - (long)b;
}

static void * test_list(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "LIST");
    log_t *         log = test != NULL ? test->log : NULL;
    slist_t *       list = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 7, 4, 1 };
    const size_t    intssz = sizeof(ints)/sizeof(*ints);
    long            prev;
    FILE *          out;

    /* insert_sorted */
    for (size_t i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "slist_insert_sorted(%d) not NULL",
            (list = slist_insert_sorted(list, (void*)((long)ints[i]), intcmp)) != NULL, ints[i]);
        if (log->level >= LOG_LVL_INFO) {
            out = log_getfile_locked(log);
            fprintf(out, "%02d> ", ints[i]);
            SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
            fputc('\n', out);
            funlockfile(out);
        }
    }
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz);

    /* prepend, append, remove */
    list = slist_prepend(list, (void*)((long)-2));
    list = slist_prepend(list, (void*)((long)0));
    list = slist_prepend(list, (void*)((long)-1));
    list = slist_append(list, (void*)((long)-4));
    list = slist_append(list, (void*)((long)20));
    list = slist_append(list, (void*)((long)15));
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz + 6);

    if (log->level >= LOG_LVL_INFO) {
        out = log_getfile_locked(log);
        fprintf(out, "after prepend/append:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
        fputc('\n', out);
        funlockfile(out);
    }
    /* we have -1, 0, -2, ..., 20, -4, 15, we will remove -1(head), then -2(in the middle),
     * then 15 (tail), then -4(in the middle), and the list should still be sorted */
    list = slist_remove(list, (void*)((long)-1), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-2), intcmp, NULL);
    list = slist_remove(list, (void*)((long)15), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-4), intcmp, NULL);
    TEST_CHECK(test, "slist_length", slist_length(list) == intssz + 2);

    if (log->level >= LOG_LVL_INFO) {
        out = log_getfile_locked(log);
        fprintf(out, "after remove:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(out, "%ld ", a);
        fputc('\n', out);
        funlockfile(out);
    }

    prev = 0;
    SLIST_FOREACH_DATA(list, a, long) {
        TEST_CHECK2(test, "elt(%ld) >= prev(%ld)", a >= prev, a, prev);
        prev = a;
    }
    slist_free(list, NULL);
    TEST_CHECK2(test, "last elt(%ld) is 20", prev == 20, prev);

    /* check SLIST_FOREACH_DATA MACRO */
    TEST_CHECK(test, "prepends", (list = slist_prepend(slist_prepend(NULL, (void*)((long)2)),
                                                      (void*)((long)1))) != NULL);
    TEST_CHECK(test, "list_length is 2", slist_length(list) == 2);
    prev = 0;
    SLIST_FOREACH_DATA(list, value, long) {
        LOG_INFO(log, "loop #%ld val=%ld", prev, value);
        if (prev == 1)
            break ;
        ++prev;
        if (prev < 5)
            continue ;
        prev *= 10;
    }
    TEST_CHECK(test, "SLIST_FOREACH_DATA break", prev == 1);
    slist_free(list, NULL);

    return VOIDP(TEST_END(test));
}

/* *************** TEST HASH *************** */

static void test_one_hash_insert(
                    hash_t *hash, const char * str,
                    const options_test_t * opts, testgroup_t * test) {
    log_t * log = test != NULL ? test->log : NULL;
    int     ins;
    char *  fnd;
    (void) opts;

    ins = hash_insert(hash, (void *) str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, " hash_insert [%s]: %d, ", str, ins);
    }

    fnd = (char *) hash_find(hash, str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, "find: [%s]\n", fnd);
    }

    TEST_CHECK2(test, "insert [%s] is %d, got %d", ins == HASH_SUCCESS, str, HASH_SUCCESS, ins);

    TEST_CHECK2(test, "hash_find(%s) is %s, got %s", fnd != NULL && strcmp(fnd, str) == 0,
                str, str, STR_CHECKNULL(fnd));
}

static void * test_hash(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *               test = TEST_START(opts->testpool, "HASH");
    log_t *                     log = test != NULL ? test->log : NULL;
    hash_t *                    hash;
    static const char * const   hash_strs[] = {
        VERSION_STRING, "a", "z", "ab", "ac", "cxz", "trz", NULL
    };

    hash = hash_alloc(HASH_DEFAULT_SIZE, 0, hash_ptr, hash_ptrcmp, NULL);
    TEST_CHECK(test, "hash_alloc not NULL", hash != NULL);
    if (hash != NULL) {
        if (log->level >= LOG_LVL_INFO) {
            fprintf(log->out, "hash_ptr: %08x\n", hash_ptr(hash, opts));
            fprintf(log->out, "hash_ptr: %08x\n", hash_ptr(hash, hash));
            fprintf(log->out, "hash_str: %08x\n", hash_str(hash, log->out));
            fprintf(log->out, "hash_str: %08x\n", hash_str(hash, VERSION_STRING));
        }
        hash_free(hash);
    }

    for (unsigned int hash_size = 1; hash_size < 200; hash_size += 100) {
        TEST_CHECK2(test, "hash_alloc(sz=%u) not NULL",
                (hash = hash_alloc(hash_size, 0, hash_str, (hash_cmp_fun_t) strcmp, NULL)) != NULL,
                hash_size);
        if (hash == NULL) {
            continue ;
        }

        for (const char *const* strs = hash_strs; *strs; strs++) {
            test_one_hash_insert(hash, *strs, opts, test);
        }

        if (log->level >= LOG_LVL_INFO) {
            TEST_CHECK(test, "hash_print_stats OK", hash_print_stats(hash, log->out) > 0);
        }
        hash_free(hash);
    }
    return VOIDP(TEST_END(test));
}

/* *************** TEST STACK *************** */
enum {
    SPT_NONE        = 0,
    SPT_PRINT       = 1 << 0,
    SPT_REPUSH      = 1 << 1,
};
static int rbuf_pop_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags,
                         testgroup_t * test) {
    log_t * log = test != NULL ? test->log : NULL;
    size_t i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "rbuf_push(%d)", rbuf_push(rbuf, (void*)((long)ints[i])) == 0, ints[i]);
    }
    TEST_CHECK2(test, "rbuf_size is %zu", (i = rbuf_size(rbuf)) == intssz, intssz);

    i = intssz - 1;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long t = (long) rbuf_top(rbuf);
        long p = (long) rbuf_pop(rbuf);
        int ret;

        ret = (t == p && p == ints[i]);

        if ((flags & SPT_PRINT) != 0
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_top(%ld) = rbuf_pop(%ld) = %d", ret != 0, t, p, ints[i]);

        if (i == 0) {
            if (j == 1) {
                i = intssz / 2 - intssz % 2;
                j = 2;
            }else if ( j == 2 ) {
                ret = (rbuf_size(rbuf) == 0);

                if ((flags & SPT_PRINT) != 0
                && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK(test, "rbuf_size is 0", ret != 0);
            }
        } else {
            --i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", t);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                ret = (rbuf_push(rbuf, (void*)((long)ints[j])) == 0);

                if ((flags & SPT_PRINT) != 0
                &&  (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK2(test, "rbuf_push(%d)", ret != 0, ints[j]);
            }
            i = intssz - 1;
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);

    return 0;
}
static int rbuf_dequeue_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags,
                             testgroup_t * test) {
    log_t *         log = test != NULL ? test->log : NULL;
    size_t          i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        TEST_CHECK2(test, "rbuf_push(%d)",
                    rbuf_push(rbuf, (void*)((long)ints[i])) == 0, ints[i]);
    }
    TEST_CHECK2(test, "rbuf_size = %zu, got %zu",
                (i = rbuf_size(rbuf)) == intssz, intssz, i);
    i = 0;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long b = (long) rbuf_bottom(rbuf);
        long d = (long) rbuf_dequeue(rbuf);
        int ret = (b == d && b == ints[i]);

        if ((ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel)))
        && (flags & SPT_PRINT) != 0) fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_bottom(%ld) = rbuf_dequeue(%ld) = %d",
                    ret != 0, b, d, ints[i]);

        if (i == intssz - 1) {
            ret = (rbuf_size(rbuf) == ((j == 1 ? 1 : 0) * intssz));

            if ((flags & SPT_PRINT) != 0
            && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                fputc('\n', log->out);

            TEST_CHECK(test, "end rbuf_size", ret != 0);
            j = 2;
            i = 0;
        } else {
            ++i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", b);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                ret = (rbuf_push(rbuf, (void*)((long)ints[j])) == 0);

                if ((flags & SPT_PRINT) != 0
                && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
                    fputc('\n', log->out);

                TEST_CHECK2(test, "rbuf_push(%d)", ret != 0, ints[j]);
            }
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);
    TEST_CHECK(test, "all dequeued", i == 0);

    return 0;
}

static void * test_rbuf(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "RBUF");
    log_t *         log = test != NULL ? test->log : NULL;
    rbuf_t *        rbuf;
    const int       ints[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    size_t          i, iter;
    int             ret;

    /* rbuf_pop 31 elts */
    LOG_INFO(log, "* rbuf_pop tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    TEST_CHECK(test, "rbuf_create", rbuf != NULL);

    for (iter = 0; iter < 10; iter++) {
        rbuf_pop_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, test);
    }
    for (iter = 0; iter < 10; iter++) {
        rbuf_pop_test(rbuf, ints, intssz,
                iter == 0 ? SPT_PRINT | SPT_REPUSH : SPT_REPUSH, test);
    }
    rbuf_free(rbuf);

    /* rbuf_dequeue 31 elts */
    LOG_INFO(log, "* rbuf_dequeue tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    TEST_CHECK(test, "rbuf_create(dequeue)", rbuf != NULL);

    for (iter = 0; iter < 10; iter++) {
        rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, test);
    }
    for (iter = 0; iter < 10; iter++) {
        rbuf_dequeue_test(rbuf, ints, intssz,
                       iter == 0 ? SPT_PRINT | SPT_REPUSH : SPT_REPUSH, test);
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));

    /* rbuf_dequeue 4001 elts */
    int tab[4001];
    size_t tabsz = sizeof(tab) / sizeof(tab[0]);
    LOG_INFO(log, "* rbuf_dequeue tests2 tab[%zu]", tabsz);
    for (i = 0; i < tabsz; i++) {
        tab[i] = i;
    }
    for (iter = 0; iter < 1000; iter++) {
        rbuf_dequeue_test(rbuf, tab, tabsz, SPT_NONE, test);
    }
    for (iter = 0; iter < 1000; iter++) {
        rbuf_dequeue_test(rbuf, tab, tabsz, SPT_REPUSH, test);
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf RBF_OVERWRITE, rbuf_get, rbuf_set */
    LOG_INFO(log, "* rbuf OVERWRITE tests");
    rbuf = rbuf_create(5, RBF_DEFAULT | RBF_OVERWRITE);
    TEST_CHECK(test, "rbuf_create(overwrite)", rbuf != NULL);

    for (i = 0; i < 3; i++) {
        TEST_CHECK0(test, "rbuf_push(%zu,overwrite)",
            (rbuf_push(rbuf, (void *)((long)((i % 5) + 1))) == 0), "rbuf_push==0", (i % 5) + 1);
    }
    i = 1;
    while(rbuf_size(rbuf) != 0) {
        size_t b = (size_t) rbuf_bottom(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, 0);
        size_t d = (size_t) rbuf_dequeue(rbuf);
        int ret = (b == i && g == i && d == i);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_get(%zd) = rbuf_top = rbuf_dequeue = %zu",
                ret != 0, i - 1, i);

        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", i);
        i++;
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    TEST_CHECK2(test, "3 elements dequeued, got %zd", i == 4, i - 1);

    for (i = 0; i < 25; i++) {
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "#%zu(%zu) ", i, (i % 5) + 1);

        ret = (rbuf_push(rbuf, (void *)((size_t) ((i % 5) + 1))) == 0);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_push(%zu,overwrite)", ret != 0, (i % 5) + 1);
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);

    TEST_CHECK2(test, "rbuf_size = 5, got %zu", (i = rbuf_size(rbuf)) == 5, i);
    TEST_CHECK(test, "rbuf_set(7,7) ko", rbuf_set(rbuf, 7, (void *) 7L) == -1);

    i = 5;
    while (rbuf_size(rbuf) != 0) {
        size_t t = (size_t) rbuf_top(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, i-1);
        size_t p = (size_t) rbuf_pop(rbuf);

        ret = (t == i && g == i && p == i);

        if (log->level >= LOG_LVL_INFO
        && (ret == 0 || (test && LOG_CAN_LOG(log, test->ok_loglevel))))
            fputc('\n', log->out);

        TEST_CHECK2(test, "rbuf_get(%zd) = rbuf_top = rbuf_pop = %zu", ret != 0, i - 1, i);

        i--;
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", p);
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    TEST_CHECK2(test, "5 elements treated, got %zd", i == 0, 5 - i);

    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf_set with RBF_OVERWRITE OFF */
    LOG_INFO(log, "* rbuf_set tests with increasing buffer");
    rbuf = rbuf_create(1, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    TEST_CHECK(test, "rbuf_create(increasing)", rbuf != NULL);
    TEST_CHECK2(test, "rbuf_size = 0, got %zu", (i = rbuf_size(rbuf)) == 0, i);

    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(1,1)", rbuf_set(rbuf, 1, (void *) 1) == 0);
        TEST_CHECK2(test, "rbuf_size = 2, got %zu", (i = rbuf_size(rbuf)) == 2, i);
    }
    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(3,3)", rbuf_set(rbuf, 3, (void *) 3) == 0);
        TEST_CHECK2(test, "rbuf_size = 4, got %zu", (i = rbuf_size(rbuf)) == 4, i);
    }
    for (iter = 0; iter < 10; iter++) {
        TEST_CHECK(test, "rbuf_set(5000,5000)", rbuf_set(rbuf, 5000, (void*) 5000) == 0);
        TEST_CHECK2(test, "rbuf_size = 5001, got %zu", (i = rbuf_size(rbuf)) == 5001, i);
    }
    TEST_CHECK(test, "rbuf_reset", rbuf_reset(rbuf) == 0);

    for (iter = 1; iter <= 5000; iter++) {
        TEST_CHECK2(test, "rbuf_set(%zu)", rbuf_set(rbuf, iter-1, (void*) iter) == 0, iter);
        TEST_CHECK2(test, "rbuf_size = %zu, got %zu", (i = rbuf_size(rbuf)) == iter, iter, i);
    }
    for (iter = 0; rbuf_size(rbuf) != 0; iter++) {
        size_t g = (size_t) rbuf_get(rbuf, 5000-iter-1);
        size_t t = (size_t) rbuf_top(rbuf);
        size_t p = (size_t) rbuf_pop(rbuf);
        i = rbuf_size(rbuf);
        TEST_CHECK2(test, "top(%zu) = get(#%zd=%zu) = pop(%zu) = %zd "
                          "and rbuf_size(%zu) = %zd",
            (t == p && g == p && p == 5000-iter && i == 5000-iter-1),
            t, 5000-iter-1, g, p, 5000-iter, i, 5000-iter-1);
    }
    TEST_CHECK2(test, "5000 elements treated, got %zu", iter == 5000, iter);

    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    return VOIDP(TEST_END(test));
}

/* *************** TEST AVL TREE *************** */
#define LG(val) ((void *)((long)(val)))

typedef struct {
    rbuf_t *    results;
    log_t *     log;
    FILE *      out;
    void *      min;
    void *      max;
    unsigned long nerrors;
    unsigned long ntests;
    avltree_t * tree;
} avltree_print_data_t;

static void avlprint_larg_manual(avltree_node_t * root, rbuf_t * stack, FILE * out) {
    rbuf_t *    fifo        = rbuf_create(32, RBF_DEFAULT);

    if (root) {
        rbuf_push(fifo, root);
    }

    while (rbuf_size(fifo)) {
        avltree_node_t *    node    = (avltree_node_t *) rbuf_dequeue(fifo);

        if (out) fprintf(out, "%ld(%ld,%ld) ", (long) node->data,
                node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);

        rbuf_push(stack, node);

        if (node->left) {
            rbuf_push(fifo, node->left);
        }
        if (node->right) {
            rbuf_push(fifo, node->right);
        }
    }
    if (out) fputc('\n', out);
    rbuf_free(fifo);
}
static void avlprint_inf_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    if (node->left)
        avlprint_inf_left(node->left, stack, out);
    if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
    if (node->right)
        avlprint_inf_left(node->right, stack, out);
}
static void avlprint_inf_right(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    if (node->right)
        avlprint_inf_right(node->right, stack, out);
    if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
    if (node->left)
        avlprint_inf_right(node->left, stack, out);
}
static void avlprint_pref_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
    if (node->left)
        avlprint_pref_left(node->left, stack, out);
    if (node->right)
        avlprint_pref_left(node->right, stack, out);
}
static void avlprint_suff_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
   if (node->left)
        avlprint_suff_left(node->left, stack, out);
    if (node->right)
        avlprint_suff_left(node->right, stack, out);
    if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
}
static void avlprint_rec_all(avltree_node_t * node, rbuf_t * stack, FILE * out) {
   if (!node) return;
   if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
   rbuf_push(stack, node);
   if (node->left)
        avlprint_rec_all(node->left, stack, out);
   if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
   if (node->right)
        avlprint_rec_all(node->right, stack, out);
    if (out) fprintf(out, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    rbuf_push(stack, node);
}
static unsigned int avlprint_rec_get_height(avltree_node_t * node) {
    if (!node) return 0;
    unsigned hl     = avlprint_rec_get_height(node->left);
    unsigned hr     = avlprint_rec_get_height(node->right);
    if (hr > hl)
        hl = hr;
    return 1 + hl;
}
static size_t avlprint_rec_get_count(avltree_node_t * node) {
    if (!node) return 0;
    return 1 + avlprint_rec_get_count(node->left)
             + avlprint_rec_get_count(node->right);
}
static unsigned int avlprint_rec_check_balance(avltree_node_t * node, log_t * log) {
    if (!node)      return 0;
    long hl     = avlprint_rec_get_height(node->left);
    long hr     = avlprint_rec_get_height(node->right);
    unsigned nerror = 0;
    if (hr - hl != (long)node->balance) {
        LOG_ERROR(log, "error: balance %d of %ld(%ld:h=%ld,%ld:h=%ld) should be %ld",
                  node->balance, (long) node->data,
                  node->left  ? (long) node->left->data : -1L,  hl,
                  node->right ? (long) node->right->data : -1L, hr,
                  hr - hl);
        nerror++;
    } else if (hr - hl > 1L || hr - hl < -1L) {
        LOG_ERROR(log, "error: balance %ld (in_node:%d) of %ld(%ld:h=%ld,%ld:h=%ld) is too big",
                  hr - hl, node->balance, (long) node->data,
                  node->left  ? (long) node->left->data : -1L,  hl,
                  node->right ? (long) node->right->data : -1L, hr);
        nerror++;
    }
    return nerror + avlprint_rec_check_balance(node->left, log)
                  + avlprint_rec_check_balance(node->right, log);
}
static void * avltree_rec_find_min(avltree_node_t * node) {
    if (node == NULL)
        return (void *) LONG_MIN;
    if (node->left != NULL) {
        return avltree_rec_find_min(node->left);
    }
    return node->data;
}
static void * avltree_rec_find_max(avltree_node_t * node) {
    if (node == NULL)
        return (void *) LONG_MAX;
    if (node->right != NULL) {
        return avltree_rec_find_max(node->right);
    }
    return node->data;
}
static avltree_visit_status_t visit_print(
                                avltree_t *                     tree,
                                avltree_node_t *                node,
                                const avltree_visit_context_t * context,
                                void *                          user_data) {

    static const unsigned long visited_mask = (1UL << ((sizeof(node->data))* 8UL - 1UL));
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;
    long value = (long)(((unsigned long) node->data) & ~(visited_mask));
    long previous;

    if ((context->state & AVH_MERGE) != 0) {
        avltree_print_data_t * job_data = (avltree_print_data_t *) context->data;
        if (avltree_count(tree) < 1000) {
            while (rbuf_size(job_data->results)) {
                rbuf_push(stack, rbuf_pop(job_data->results));
            }
            rbuf_free(job_data->results);
            job_data->results = NULL;
        } else {
            data->nerrors += job_data->nerrors;
        }
        return AVS_CONTINUE;
    }

    if ((context->how & AVH_MERGE) != 0 && stack == NULL) {
        if (avltree_count(tree) < 1000) {
            data->results = stack = rbuf_create(32, RBF_DEFAULT);
        } else {
            ++(data->nerrors);
        }
    }

    previous = rbuf_size(stack)
                    ? (long)((unsigned long)(((avltree_node_t *) rbuf_top(stack))->data)
                             & ~(visited_mask))
                    : LONG_MAX;


    if ((context->how & (AVH_PREFIX|AVH_SUFFIX)) == 0) {
        switch (context->state) {
            case AVH_INFIX:
                if ((context->how & AVH_RIGHT) == 0) {
                    if (previous == LONG_MAX) previous = LONG_MIN;
                    if (value < previous) {
                        if (data->out != NULL)
                            LOG_INFO(data->log, NULL);
                        LOG_ERROR(data->log, "error: bad tree order node %ld < prev %ld",
                                  value, previous);
                        return AVS_ERROR;
                    }
                } else if (value > previous) {
                    if (data->out != NULL)
                        LOG_INFO(data->log, NULL);
                    LOG_ERROR(data->log, "error: bad tree order node %ld > prev %ld",
                              value, previous);
                    return AVS_ERROR;
                }
                break ;
            default:
                break ;
        }
    }
    if (data->out) fprintf(data->out, "%ld(%ld,%ld) ", value,
            node->left ?(long)((unsigned long)(node->left->data) & ~(visited_mask)) : -1,
            node->right? (long)((unsigned long)(node->right->data) & ~(visited_mask)) : -1);
    rbuf_push(stack, node);

    if (stack == NULL && (context->how & AVH_MERGE) == 0) {
        if (((unsigned long)node->data & visited_mask) != 0) {
            LOG_WARN(data->log, "node %ld (0x%lx) already visited !", value, (unsigned long) node);
            ++(data->nerrors);
        }
        node->data = (void *) ((unsigned long)(node->data) | visited_mask);
    }

    return AVS_CONTINUE;
}
static avltree_visit_status_t visit_checkprint(
                                avltree_t *                     tree,
                                avltree_node_t *                node,
                                const avltree_visit_context_t * context,
                                void *                          user_data) {
    (void) tree;
    (void) context;
    static const unsigned long visited_mask = (1UL << ((sizeof(node->data))* 8UL - 1UL));
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;

    ++(data->ntests);
    if (((unsigned long)(node->data) & visited_mask) != 0) {
        node->data = (void *) ((unsigned long)(node->data) & ~(visited_mask));
    } else {
        ++(data->nerrors);
        LOG_ERROR(data->log, "error: node %ld (0x%lx) has not been visited !",
                  (long)node->data, (unsigned long) node);
    }

    return AVS_CONTINUE;
}
static AVLTREE_DECLARE_VISITFUN(visit_range, tree, node, context, user_data) {
    (void) tree;
    (void) context;
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;

    if (data->out) fprintf(data->out, "%ld(%ld,%ld) ", (long) node->data,
            node->left ?(long)((unsigned long)(node->left->data) ) : -1,
            node->right? (long)((unsigned long)(node->right->data) ) : -1);

    if (context->how == AVH_INFIX) {
        /* avltree_visit(INFIX) mode */
        if ((long) node->data >= (long) data->min && (long) node->data <= (long) data->max) {
            rbuf_push(stack, node);
            if ((long) node->data > (long) data->max) {
                return AVS_FINISHED;
            }
        }
    } else {
        /* avltree_visit_range(min,max) mode */
        if ((long) node->data < (long) data->min || (long) node->data > (long) data->max) {
            LOG_ERROR(data->log, "error: bad tree order: node %ld not in range [%ld:%ld]",
                      (long)node->data, (long) data->min, (long) data->max);
            return AVS_ERROR;
        }
        rbuf_push(stack, node);
    }
    return AVS_CONTINUE;
}
static avltree_node_t *     avltree_node_insert_rec(
                                avltree_t * tree,
                                avltree_node_t ** node,
                                void * data) {
    avltree_node_t * new;
    int cmp;

    if (*node == NULL) {
        if ((new = avltree_node_create(tree, data, (*node), NULL)) != NULL) {
            new->balance = 0;
            *node = new;
            ++tree->n_elements;
        } else if (errno == 0) {
            errno = ENOMEM;
        }
        return new;
    } else if ((cmp = tree->cmp(data, (*node)->data)) <= 0) {
        new = avltree_node_insert_rec(tree, &((*node)->left), data);
    } else {
        new = avltree_node_insert_rec(tree, &((*node)->right), data);
    }
    return new;
}

static void * avltree_insert_rec(avltree_t * tree, void * data) {
    avltree_node_t * node;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    node = avltree_node_insert_rec(tree, &(tree->root), data);
    if (node != NULL) {
        if (data == NULL) {
            errno = 0;
        }
        return data;
    }
    if (errno == 0) {
        errno = EAGAIN;
    }
    return NULL;
}

enum {
    TAC_NONE        = 0,
    TAC_NO_ORDER    = 1 << 0,
    TAC_REF_NODE    = 1 << 1,
    TAC_RES_NODE    = 1 << 2,
    TAC_RESET_REF   = 1 << 3,
    TAC_RESET_RES   = 1 << 4,
    TAC_DEFAULT     = TAC_RES_NODE | TAC_REF_NODE | TAC_RESET_RES | TAC_RESET_REF,
} test_avltree_check_flags_t;

static unsigned int avltree_check_results(rbuf_t * results, rbuf_t * reference,
                                          log_t * log, int flags) {
    unsigned int nerror = 0;

    if (rbuf_size(results) != rbuf_size(reference)) {
        LOG_ERROR(log, "error: results size:%zu != reference size:%zu",
                  rbuf_size(results), rbuf_size(reference));
        return 1;
    }
    for (size_t i_ref = 0, n = rbuf_size(reference); i_ref < n; ++i_ref) {
        void * refdata, * resdata;
        long ref, res;

        refdata = rbuf_get(reference, i_ref);
        ref = (flags & TAC_REF_NODE) != 0
              ? (long) (((avltree_node_t *)refdata)->data) : (long) refdata;

        if ((flags & TAC_NO_ORDER) == 0) {
            resdata = rbuf_get(results, i_ref);
            res = (flags & TAC_RES_NODE) != 0
                  ? (long)(((avltree_node_t *)resdata)->data) : (long) resdata;

            if (log)
                LOG_SCREAM(log, "check REF #%zu n=%zu pRef=%lx ref=%ld pRes=%lx res=%ld flags=%u\n",
                       i_ref, n, (unsigned long) refdata, ref, (unsigned long) resdata, res, flags);

            if (res != ref) {
                LOG_ERROR(log, "error: result:%ld != reference:%ld", res, ref);
                nerror++;
            }
        } else {
            size_t i_res;
            for (i_res = rbuf_size(results); i_res > 0; --i_res) {
                resdata = rbuf_get(results, i_res - 1);
                if ((flags & TAC_RES_NODE) != 0) {
                    if (resdata == NULL)
                        continue ;
                    res = (long)(((avltree_node_t *)resdata)->data);
                } else {
                    res = (long) resdata;
                }
                if (log)
                    LOG_SCREAM(log, "check REF #%zu n=%zu pRef=%lx ref=%ld "
                                    "pRes=%lx res=%ld flags=%u\n",
                       i_ref, n, (unsigned long) refdata, ref, (unsigned long) resdata, res, flags);

                if (ref == res) {
                    rbuf_set(results, i_res - 1, NULL);
                    break ;
                }
            }
            if (i_res == 0) {
                LOG_ERROR(log, "check_no_order: elt '%ld' not visited !", ref);
                ++nerror;
            }
        }
    }
    if ((flags & TAC_RESET_REF) != 0)
        rbuf_reset(reference);
    if ((flags & TAC_RESET_RES) != 0)
        rbuf_reset(results);

    return nerror;
}

static unsigned int avltree_test_visit(avltree_t * tree, int check_balance,
                                       FILE * out, log_t * log) {
    unsigned int            nerror = 0;
    rbuf_t *                results    = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    rbuf_t *                reference  = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    avltree_print_data_t    data = { .results = results, .out = out, .log = log };
    long                    value, ref_val;
    int                     errno_bak;

    if (log && log->level < LOG_LVL_INFO) {
        data.out = out = NULL;
    }

    if (check_balance) {
        int n = avlprint_rec_check_balance(tree->root, log);
        if (out)
            LOG_INFO(log, "Checking balances: %u error(s).", n);
        nerror += n;
    }

    if (out) {
        fprintf(out, "LARG PRINT\n");
        avltree_print(tree, avltree_print_node_default, out);
    }

    /* BREADTH */
    if (out)
        fprintf(out, "manLARGL ");
    avlprint_larg_manual(tree->root, reference, out);

    if (out)
        fprintf(out, "LARGL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_BREADTH) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* prefix left */
    if (out)
        fprintf(out, "recPREFL ");
    avlprint_pref_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "PREFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* infix left */
    if (out)
        fprintf(out, "recINFL  ");
    avlprint_inf_left(tree->root, reference, out); if (out) fprintf(out, "\n");
    if (out)
        fprintf(out, "INFL     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* infix right */
    if (out)
        fprintf(out, "recINFR  ");
    avlprint_inf_right(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "INFR     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* suffix left */
    if (out)
        fprintf(out, "recSUFFL ");
    avlprint_suff_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "SUFFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_SUFFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* prefix + infix + suffix left */
    if (out)
        fprintf(out, "recALL   ");
    avlprint_rec_all(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "ALL      ");
    if (avltree_visit(tree, visit_print, &data,
                      AVH_PREFIX | AVH_SUFFIX | AVH_INFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    if (out)
        LOG_INFO(log, "current tree stack maxsize = %zu",
                 tree != NULL && tree->shared != NULL
                 ? rbuf_maxsize(tree->shared->stack) : 0);

    /* visit_range(min,max) */
    /*   get infix left reference */
    avlprint_inf_left(tree->root, reference, NULL);
    if (out)
        fprintf(out, "RNG(all) ");
    ref_val = -1L;
    data.min = LG(avltree_find_min(tree));
    data.max = LG(avltree_find_max(tree));
    if (AVS_FINISHED !=
            (avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
       ){  //TODO || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
        LOG_ERROR(log, "error: avltree_visit_range() error, last(%ld) <=? max(%ld) ",
                ref_val, (long)data.max);
        ++nerror;
    }
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* parallel */
    if (out)
        fprintf(out, "PARALLEL ");
    data.results = NULL; /* needed because parallel avltree_visit_print */
    if (avltree_visit(tree, visit_print, &data,
                      AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED)
        nerror++;
    data.results = results;
    if (out) fprintf(out, "\n");
    data.ntests = data.nerrors = 0;
    if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
    || data.ntests != avltree_count(tree)) {
        nerror++;
    }
    nerror += data.nerrors;
    rbuf_reset(results);
    rbuf_reset(reference);

    /* parallel AND merge */
    avlprint_pref_left(tree->root, reference, NULL);
    data.results = NULL; /* needed because allocated by each job */
    if (out)
        fprintf(out, "PARALMRG ");
    if (avltree_visit(tree, visit_print, &data,
                AVH_PARALLEL_DUPDATA(AVH_PREFIX | AVH_MERGE, sizeof(data))) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(data.results, reference, log, TAC_DEFAULT | TAC_NO_ORDER);
    if (data.results != NULL) {
        rbuf_free(data.results);
    }
    data.results = results;

    if (out)
        LOG_INFO(log, "current tree stack maxsize = %zu",
                 tree != NULL && tree->shared != NULL
                 ? rbuf_maxsize(tree->shared->stack) : 0);

    /* count */
    ref_val = avlprint_rec_get_count(tree->root);
    errno = EBUSY; /* setting errno to check it is correctly set by avltree */
    value = avltree_count(tree);
    errno_bak = errno;
    if (out)
        LOG_INFO(log, "COUNT = %ld", value);
    if (value != ref_val || (value == 0 && errno_bak != 0)) {
        LOG_ERROR(log, "error incorrect COUNT %ld, expected %ld (errno:%d)",
                  value, ref_val, errno_bak);
        ++nerror;
    }
    /* memorysize */
    value = avltree_memorysize(tree);
    if (out)
        LOG_INFO(log, "MEMORYSIZE = %ld (%.03fMB)",
                 value, value / 1000.0 / 1000.0);
    /* depth */
    if (check_balance) {
        errno = EBUSY; /* setting errno to check it is correctly set by avltree */
        value = avltree_find_depth(tree);
        errno_bak = errno;
        ref_val = avlprint_rec_get_height(tree->root);
        if (out)
            LOG_INFO(log, "DEPTH = %ld", value);
        if (value != ref_val || (value == 0 && errno_bak != 0)) {
            LOG_ERROR(log, "error incorrect DEPTH %ld, expected %ld (errno:%d)",
                      value, ref_val, errno_bak);
            ++nerror;
        }
    }
    /* min */
    /* setting errno to check it is correctly set by avltree */
    errno =  avltree_count(tree) == 0 ? 0 : EBUSY;
    value = (long) avltree_find_min(tree);
    errno_bak = errno;
    ref_val = (long) avltree_rec_find_min(tree->root);
    if (out)
        LOG_INFO(log, "MINIMUM value = %ld", value);
    if (ref_val == LONG_MIN) {
        if (value != 0 || errno_bak == 0) {
            LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld and non 0 errno (%d)",
                      value, 0L, errno_bak);
            ++nerror;
        }
    } else if (value != ref_val) {
        LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld",
                  value, ref_val);
        ++nerror;
    }
    /* max */
    /* setting errno to check it is correctly set by avltree */
    errno =  avltree_count(tree) == 0 ? 0 : EBUSY;
    value = (long) avltree_find_max(tree);
    errno_bak = errno;
    ref_val = (long) avltree_rec_find_max(tree->root);
    if (out)
        LOG_INFO(log, "MAXIMUM value = %ld", value);
    if (ref_val == LONG_MAX) {
        if (value != 0 || errno_bak == 0) {
            LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld and non 0 errno (%d)",
                      value, 0L, errno_bak);
            ++nerror;
        }
    } else if (value != ref_val) {
        LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld",
                  value, ref_val);
        ++nerror;
    }

    /* avltree_to_{slist,array,rbuf} */
    static const unsigned int hows[] = { AVH_INFIX, AVH_PREFIX, AVH_PREFIX | AVH_PARALLEL };
    static const char *       shows[] = { "infix",  "prefix",   "prefix_parallel" };
    for (size_t i = 0; i < PTR_COUNT(hows); ++i) {
        slist_t *       slist;
        rbuf_t *        rbuf;
        void **         array;
        size_t          n;
        unsigned int    flags = TAC_REF_NODE | TAC_RESET_RES;

        BENCHS_DECL(tm_bench, cpu_bench);

        rbuf_reset(reference);
        rbuf_reset(results);

        if ((hows[i] & AVH_PREFIX) != 0) {
            avlprint_pref_left(tree->root, reference, NULL);
        } else {
            avlprint_inf_left(tree->root, reference, NULL);
        }
        if ((hows[i] & AVH_PARALLEL) != 0) {
            flags |= TAC_NO_ORDER;
        }

        /* avltree_to_slist */
        BENCHS_START(tm_bench, cpu_bench);
        slist = avltree_to_slist(tree, hows[i]);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_slist(%s) ", shows[i]);
        SLIST_FOREACH_DATA(slist, data, void *) {
            rbuf_push(results, data);
        }
        slist_free(slist, NULL);
        nerror += avltree_check_results(results, reference, log, flags);

        /* avltree_to_rbuf */
        BENCHS_START(tm_bench, cpu_bench);
        rbuf = avltree_to_rbuf(tree, hows[i]);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_rbuf(%s) ", shows[i]);
        for (unsigned int i = 0, n = rbuf_size(rbuf); i < n; ++i) {
            rbuf_push(results, rbuf_get(rbuf, i));
        }
        rbuf_free(rbuf);
        nerror += avltree_check_results(results, reference, log, flags);

        /* avltree_to_array */
        BENCHS_START(tm_bench, cpu_bench);
        n = avltree_to_array(tree, hows[i], &array);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_array(%s) ", shows[i]);
        if (array != NULL) {
            for (size_t idx = 0; idx < n; ++idx) {
                rbuf_push(results, array[idx]);
            }
            free(array);
        }
        nerror += avltree_check_results(results, reference, log, flags);

        rbuf_reset(reference);
        rbuf_reset(results);
    }

    if (results)
        rbuf_free(results);
    if (reference)
        rbuf_free(reference);

    return nerror;
}

static void * avltree_test_visit_job(void * vdata) {
    avltree_print_data_t * data = (avltree_print_data_t *) vdata;
    int result, ret = AVS_FINISHED;
    rbuf_t * results_bak = data->results;
    BENCHS_DECL(tm_bench, cpu_bench);

    BENCHS_START(tm_bench, cpu_bench);
    if ((result = avltree_visit(data->tree, visit_print, data, AVH_INFIX)) != AVS_FINISHED)
        ret = AVS_ERROR;
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
                    "INFIX visit (%zd nodes,%p,%d error) | ",
                    avltree_count(data->tree), (void*) data->tree,
                    result == AVS_FINISHED ? 0 : 1);

    data->results = NULL;
    data->nerrors = data->ntests = 0;
    BENCHS_START(tm_bench, cpu_bench);
    if ((result = avltree_visit(data->tree, visit_print, data,
                                AVH_PREFIX | AVH_PARALLEL)) != AVS_FINISHED) {
        ret = AVS_ERROR;
    }
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
                    "PARALLEL_PREFIX visit (%zd nodes,%p,%d error) | ",
                    avltree_count(data->tree), (void*) data->tree,
                    result == AVS_FINISHED ? 0 : 1);

    BENCHS_START(tm_bench, cpu_bench);
    if (avltree_visit(data->tree, visit_checkprint, data, AVH_PREFIX) != AVS_FINISHED
    ||  data->nerrors != 0 || data->ntests != avltree_count(data->tree)) {
        ret = AVS_ERROR;
    }
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
        "check // prefix with prefix (%zd nodes,%p,%lu error%s) | ",
        avltree_count(data->tree), (void*) data->tree,
        data->nerrors, data->nerrors > 1 ? "s" : "");

    data->results = results_bak;

    return (void *) ((long) ret);
}

static void * avltree_test_insert_job(void * vdata) {
    avltree_print_data_t * data = (avltree_print_data_t *) vdata;
    int ret = AVS_FINISHED;

    for (size_t i = 0; i < (size_t) data->max; ++i) {
        if (avltree_insert(data->tree, (void *) i) != (void *) i) {
            ret = AVS_ERROR;
        }
    }
    return (void *) ((long) ret);
}

static void * test_avltree(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "AVLTREE");
    log_t *         log = test != NULL ? test->log : NULL;
    unsigned int    nerrors = 0;
    avltree_t *     tree = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 1, 7, 4, 1 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    int             n;
    long            ref_val;
    unsigned int    progress_max = opts->main == pthread_self()
                                   ? (opts->columns > 100 ? 100 : opts->columns) : 0;

    /* create tree INSERT REC*/
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert_rec)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(log, "* inserting tree(insert_rec)");
    for (size_t i = 0; i < intssz; i++) {
        void * result = avltree_insert_rec(tree, LG(ints[i]));
        if (result != LG(ints[i]) || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%d>, result <%p> : %s",
                      ints[i], result, strerror(errno));
            nerrors++;
        }
    }
    /* visit */
    nerrors += avltree_test_visit(tree, 0, log->out, log);
    /* free */
    LOG_INFO(log, "* freeing tree(insert_rec)");
    avltree_free(tree);

    /* create tree INSERT */
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(log, "* inserting in tree(insert)");
    for (size_t i = 0; i < intssz; i++) {
        LOG_DEBUG(log, "* inserting %d", ints[i]);
        void * result = avltree_insert(tree, LG(ints[i]));
        if (result != LG(ints[i]) || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%d>, result <%p> : %s",
                      ints[i], result, strerror(errno));
            nerrors++;
        }
        n = avlprint_rec_check_balance(tree->root, log);
        LOG_DEBUG(log, "Checking balances: %d error(s).", n);
        nerrors += n;

        if (log->level >= LOG_LVL_SCREAM) {
            avltree_print(tree, avltree_print_node_default, log->out);
            getchar();
        }
    }

    /* visit */
    nerrors += avltree_test_visit(tree, 1, log->out, log);
    /* remove */
    LOG_INFO(log, "* removing in tree(insert)");
    if (avltree_remove(tree, (const void *) 123456789L) != NULL || errno != ENOENT) {
        LOG_ERROR(log, "error removing 123456789, bad result");
        nerrors++;
    }
    for (size_t i = 0; i < intssz; i++) {
        LOG_DEBUG(log, "* removing %d", ints[i]);
        void * elt = avltree_remove(tree, (const void *) LG(ints[i]));
        if (elt != LG(ints[i]) || (elt == NULL && errno != 0)) {
            LOG_ERROR(log, "error removing elt <%d>: %s", ints[i], strerror(errno));
            nerrors++;
        } else if ((n = avltree_count(tree)) != (int)(intssz - i - 1)) {
            LOG_ERROR(log, "error avltree_count() : %d, expected %zd", n, intssz - i - 1);
            nerrors++;
        }
        /* visit */
        nerrors += avltree_test_visit(tree, 1, NULL, log);

        if (log->level >= LOG_LVL_SCREAM) {
            avltree_print(tree, avltree_print_node_default, log->out);
            getchar();
        }
    }
    /* free */
    LOG_INFO(log, "* freeing tree(insert)");
    avltree_free(tree);

    /* check flags AFL_INSERT_* */
    const char *    one_1 = "one";
    const char *    two_1 = "two";
    char *          one_2 = strdup(one_1);
    void *          result, * find;
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** creating tree(insert, AFL_INSERT_*)");
    if ((tree = avltree_create(AFL_DEFAULT, (avltree_cmpfun_t) strcmp, NULL)) == NULL
    || avltree_insert(tree, (void *) one_1) != one_1
    || avltree_insert(tree, (void *) two_1) != two_1) {
        LOG_ERROR(log, "AFL_INSERT_*: error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* check flag AFL_INSERT_NODOUBLE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_NODOUBLE;
    n = avltree_count(tree);
    /* set errno to check avltree_insert sets correctly errno */
    if ((!(errno=EBUSY) || avltree_insert(tree, (void *) one_1) != NULL || errno == 0)
    ||  (!(errno=EBUSY) || avltree_insert(tree, (void *) two_1) != NULL || errno == 0)
    ||  (!(errno=EBUSY) || avltree_insert(tree, (void *) one_2) != NULL || errno == 0)
    ||  (avltree_insert(tree, avltree_find_min(tree)) != NULL || errno == 0)
    ||  (int) avltree_count(tree) != n) {
        LOG_ERROR(log, "error inserting existing element with AFL_INSERT_NODOUBLE: not rejected");
        nerrors++;
    }
    /* check flag AFL_INSERT_IGNDOUBLE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_IGNDOUBLE;
    n = avltree_count(tree);
    /* array: multiple of 3: {insert0, result0, find0, insert1, result1, find1, ... } */
    const void * elts[] = { one_1, one_1, one_1, two_1, two_1, two_1, one_2, one_1, one_1 };
    for (unsigned i = 0; i < sizeof(elts) / sizeof(*elts); i += 3) {
        errno = EBUSY;
        find = NULL;
        if ((result = avltree_insert(tree, (void*)elts[i])) != elts[i+1]
        ||  (result == NULL && errno != 0)
        ||  (find = avltree_find(tree, (void*) elts[i])) != elts[i+2]
        ||  (find == NULL && errno != 0)
        ||  (int) avltree_count(tree) != n || (n == 0 && errno != 0)) {
            LOG_ERROR(log, "AFL_INSERT_IGNDOUBLE error: inserting '%s' <%p>, "
                           "expecting ret:<%p> & find:<%p> & count:%d, "
                           "got <%p> & <%p> & %zu, errno:%d.",
                           (const char *)elts[i], elts[i], elts[i+1], elts[i+2], n,
                           result, find, avltree_count(tree), errno);
            nerrors++;
        }
    }
    /* check flag AFL_INSERT_REPLACE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_REPLACE;
    n = avltree_count(tree);
    /* array: multiple of 3: {insert0, result0, find0, insert1, result1, find1, ... } */
    elts[sizeof(elts)/sizeof(*elts)-1] = one_2;
    for (unsigned i = 0; i < sizeof(elts) / sizeof(*elts); i += 3) {
        errno = EBUSY;
        find = NULL;
        if ((result = avltree_insert(tree, (void*)elts[i])) != elts[i+1]
        ||  (result == NULL && errno != 0)
        ||  (find = avltree_find(tree, (void*) elts[i])) != elts[i+2]
        ||  (find == NULL && errno != 0)
        ||  (int) avltree_count(tree) != n || (n == 0 && errno != 0)) {
            LOG_ERROR(log, "AFL_INSERT_REPLACE error: inserting '%s' <%p>, "
                           "expecting ret:<%p> & find:<%p> & count:%d, "
                           "got <%p> & <%p> & %zu, errno:%d.",
                           (const char *)elts[i], elts[i], elts[i+1], elts[i+2], n,
                           result, find, avltree_count(tree), errno);
            nerrors++;
        }
    }
    /* free */
    LOG_INFO(log, "* freeing tree(insert, AFL_INSERT_*)");
    avltree_free(tree);
    if (one_2 != NULL)
        free(one_2);

    /* test with tree created manually */
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert_manual)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree(manual insert): %s", strerror(errno));
        nerrors++;
    }
    tree->root =
        AVLNODE(LG(10),
                AVLNODE(LG(5),
                    AVLNODE(LG(3),
                        AVLNODE(LG(2), NULL, NULL),
                        AVLNODE(LG(4), NULL, NULL)),
                    AVLNODE(LG(7),
                        AVLNODE(LG(6), NULL, NULL),
                        AVLNODE(LG(8), NULL, NULL))),
                AVLNODE(LG(15),
                    AVLNODE(LG(13),
                        AVLNODE(LG(12), NULL, NULL),
                        AVLNODE(LG(14), NULL, NULL)),
                    AVLNODE(LG(17),
                        AVLNODE(LG(16),NULL, NULL),
                        AVLNODE(LG(18), NULL, NULL)))
            );
    tree->n_elements = avlprint_rec_get_count(tree->root);
    nerrors += avltree_test_visit(tree, 1, log->out, log);

    /* free */
    LOG_INFO(log, "* freeing tree(insert_manual)");
    avltree_free(tree);

    /* create tree INSERT - same values */
    const size_t samevalues_count = 30;
    const long samevalues[] = { 0, 2, LONG_MAX };
    for (const long * samevalue = samevalues; *samevalue != LONG_MAX; samevalue++) {
        LOG_INFO(log, "*************************************************");
        LOG_INFO(log, "*** CREATING TREE (insert same values:%ld)", *samevalue);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }
        /* visit on empty tree */
        LOG_INFO(log, "* visiting empty tree (insert_same_values:%ld)", *samevalue);
        nerrors += avltree_test_visit(tree, 1, NULL, log);
        /* insert */
        LOG_INFO(log, "* inserting in tree(insert_same_values:%ld)", *samevalue);
        for (size_t i = 0, count; i < samevalues_count; i++) {
            LOG_DEBUG(log, "* inserting %ld", *samevalue);
            errno = EBUSY; /* setting errno to check it is correctly set by avltree */
            void * result = avltree_insert(tree, LG(*samevalue));
            if (result != LG(*samevalue) || (result == NULL && errno != 0)) {
                LOG_ERROR(log, "error inserting elt <%ld>, result <%p> : %s",
                          *samevalue, result, strerror(errno));
                nerrors++;
            }
            n = avlprint_rec_check_balance(tree->root, log);
            LOG_DEBUG(log, "Checking balances: %d error(s).", n);
            nerrors += n;

            count = avltree_count(tree);
            LOG_DEBUG(log, "Checking count: %zu, %zu expected.", count, i + 1);
            if (count != i + 1) {
                LOG_ERROR(log, "error count: %zu, %zu expected.", count, i + 1);
                ++nerrors;
            }

            if (log->level >= LOG_LVL_SCREAM) {
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        /* visit */
        LOG_INFO(log, "* visiting tree(insert_same_values:%ld)", *samevalue);
        nerrors += avltree_test_visit(tree, 1, LOG_CAN_LOG(log, LOG_LVL_DEBUG)
                                               ? log->out : NULL, log);
        /* remove */
        LOG_INFO(log, "* removing in tree(insert_same_values:%ld)", *samevalue);
        for (size_t i = 0; i < samevalues_count; i++) {
            LOG_DEBUG(log, "* removing %ld", *samevalue);
            errno = EBUSY; /* setting errno to check it is correctly set by avltree */
            void * elt = avltree_remove(tree, (const void *) LG(*samevalue));
            if (elt != LG(*samevalue) || (elt == NULL && errno != 0)) {
                LOG_ERROR(log, "error removing elt <%ld>: %s", *samevalue, strerror(errno));
                nerrors++;
            } else if ((n = avltree_count(tree)) != (int)(samevalues_count - i - 1)) {
                LOG_ERROR(log, "error avltree_count() : %d, expected %zd",
                          n, samevalues_count - i - 1);
                nerrors++;
            }
            /* visit */
            nerrors += avltree_test_visit(tree, 1, NULL, log);

            if (log->level >= LOG_LVL_SCREAM) {
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        /* free */
        LOG_INFO(log, "* freeing tree(insert_same_values: %ld)", *samevalue);
        avltree_free(tree);
    }

    /* create Big tree INSERT */
    BENCH_DECL(bench);
    BENCH_TM_DECL(tm_bench);
    rbuf_t * two_results        = rbuf_create(2, RBF_DEFAULT | RBF_OVERWRITE);
    rbuf_t * all_results        = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    rbuf_t * all_refs           = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    avltree_print_data_t data   = { .results = two_results, .log = log, .out = NULL };
    const size_t nb_elts[] = { 100 * 1000, 1 * 1000 * 1000, SIZE_MAX, 10 * 1000 * 1000, 0 };
    for (const size_t * nb = nb_elts; *nb != 0; nb++) {
        long    value, min_val = LONG_MAX, max_val = LONG_MIN;
        size_t  n_remove, total_remove = *nb / 10;
        rbuf_t * del_vals;
        slist_t * slist;
        rbuf_t * rbuf;
        void ** array;
        size_t len;

        if (*nb == SIZE_MAX) { /* after size max this is only for TEST_bigtree */
            if ((opts->test_mode & TEST_MASK(TEST_bigtree)) != 0) continue ; else break ;
        }
        if (nb == nb_elts) {
            srand(INT_MAX); /* first loop with predictive random for debugging */
            //log->level = LOG_LVL_DEBUG;
        } else {
            //log->level = LOG_LVL_INFO;
            srand(time(NULL));
        }

        LOG_INFO(log, "*************************************************");
        LOG_INFO(log, "*** CREATING BIG TREE (insert, %zu elements)", *nb);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }
        data.tree = tree;

        /* insert */
        del_vals = rbuf_create(total_remove, RBF_DEFAULT | RBF_OVERWRITE);
        LOG_INFO(log, "* inserting in Big tree(insert, %zu elements)", *nb);
        BENCHS_START(tm_bench, bench);
        for (size_t i = 0, n_remove = 0; i < *nb; i++) {
            LOG_DEBUG(log, "* inserting %zu", i);
            value = rand();
            value = (long)(value % (*nb * 10));
            if (value > max_val)
                max_val = value;
            if (value < min_val)
                min_val = value;
            if (++n_remove <= total_remove)
                rbuf_push(del_vals, LG(value));

            void * result = avltree_insert(tree, (void *) value);

            if (progress_max && (i * (progress_max)) % *nb == 0 && log->level >= LOG_LVL_INFO)
                fputc('.', log->out);

            if (result != (void *) value || (result == NULL && errno != 0)) {
                LOG_ERROR(log, "error inserting elt <%ld>, result <%lx> : %s",
                          value, (unsigned long) result, strerror(errno));
                nerrors++;
            }
            if (log->level >= LOG_LVL_SCREAM) {
                nerrors += avlprint_rec_check_balance(tree->root, log);
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        if (progress_max && log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCHS_STOP_LOG(tm_bench, bench, log, "creation of %zu nodes ", *nb);

        /* visit */
        LOG_INFO(log, "* checking balance, prefix, infix, infix_r, "
                         "parallel, min, max, depth, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check BALANCE (recursive) of %zu nodes | ", *nb);

        /* prefix */
        rbuf_reset(data.results);
        data.results=NULL; /* in order to run visit_checkprint() */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PREFIX visit of %zu nodes | ", *nb);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check prefix visit with prefix (%zu nodes) | ", *nb);
        nerrors += data.nerrors;
        data.results = two_results;

        /* infix */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX visit of %zu nodes | ", *nb);

        /* infix_right */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX_RIGHT visit of %zu nodes | ", *nb);

        /* parallel */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as parallel avltree_visit_print without AVH_PARALLEL_DUPDATA */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX visit of %zu nodes | ", *nb);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        || data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check // visit with prefix (%zu nodes) | ", *nb);
        nerrors += data.nerrors;
        data.results = two_results;

        /* parallel_merge */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as avltree_visit_print will create its own datas */
        data.nerrors = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PARALLEL_DUPDATA(AVH_PREFIX|AVH_MERGE, sizeof(data))) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX_MERGE visit of %zu nodes | ", *nb);
        if (data.nerrors != avltree_count(tree)) {
            LOG_ERROR(log, "parallel_prefix_merge: error expected %zu visits, got %lu",
                      avltree_count(tree), data.nerrors);
            ++nerrors;
        }
        data.nerrors = 0;
        data.results = two_results;

        LOG_INFO(log, "current tree stack maxsize = %zu",
                 tree != NULL && tree->shared != NULL ? rbuf_maxsize(tree->shared->stack) : 0);

        /* min */
        BENCH_START(bench);
        value = (long) avltree_find_min(tree);
        BENCH_STOP_LOG(bench, log, "MINIMUM value = %ld | ", value);
        if (value != min_val) {
            LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld",
                      value, min_val);
            ++nerrors;
        }
        /* max */
        BENCH_START(bench);
        value = (long) avltree_find_max(tree);
        BENCH_STOP_LOG(bench, log, "MAXIMUM value = %ld | ", value);
        if (value != max_val) {
            LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld",
                      value, max_val);
            ++nerrors;
        }
        /* depth */
        BENCH_START(bench);
        n = avlprint_rec_get_height(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive DEPTH (%zu nodes) = %d | ",
                       *nb, n);

        BENCH_START(bench);
        value = avltree_find_depth(tree);
        BENCH_STOP_LOG(bench, log, "DEPTH (%zu nodes) = %ld | ",
                       *nb, value);
        if (value != n) {
            LOG_ERROR(log, "error incorrect DEPTH %ld, expected %d",
                      value, n);
            ++nerrors;
        }
        /* count */
        BENCH_START(bench);
        n = avlprint_rec_get_count(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive COUNT (%zu nodes) = %d | ",
                       *nb, n);

        BENCH_START(bench);
        value = avltree_count(tree);
        BENCH_STOP_LOG(bench, log, "COUNT (%zu nodes) = %ld | ",
                       *nb, value);
        if (value != n || (size_t) value != *nb) {
            LOG_ERROR(log, "error incorrect COUNT %ld, expected %d(rec), %zu",
                      value, n, *nb);
            ++nerrors;
        }
        /* memorysize */
        BENCH_START(bench);
        n = avltree_memorysize(tree);
        BENCH_STOP_LOG(bench, log, "MEMORYSIZE (%zu nodes) = %d (%.03fMB) | ",
                       *nb, n, n / 1000.0 / 1000.0);

        /* visit range */
        rbuf_reset(all_results);
        rbuf_reset(all_refs);
        data.min = LG((((long)avltree_find_max(tree)) / 2) - 500);
        data.max = LG((long)data.min + 2000);
        /* ->avltree_visit_range() */
        data.results = all_results;
        ref_val = -1L;
        BENCHS_START(tm_bench, bench);
        if (AVS_FINISHED !=
                (n = avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
        || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit_range() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "VISIT_RANGE (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->avltree_visit() */
        data.results = all_refs;
        BENCHS_START(tm_bench, bench);
        if (AVS_FINISHED != (n = avltree_visit(tree, visit_range, &data, AVH_INFIX))
        || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "VISIT INFIX (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->compare avltree_visit and avltree_visit_range */
        if ((n = avltree_check_results(all_results, all_refs, NULL, TAC_DEFAULT)) > 0) {
            LOG_ERROR(log, "visit_range/visit_infix --> error: different results");
            nerrors += n;
        }
        data.results = two_results;

        /* avltree_to_slist */
        BENCHS_START(tm_bench, bench);
        slist = avltree_to_slist(tree, AVH_INFIX);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_slist(infix) %s", "");
        if (slist_length(slist) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_slist(infix): wrong length");
            ++nerrors;
        }
        slist_free(slist, NULL);

        BENCHS_START(tm_bench, bench);
        slist = avltree_to_slist(tree, AVH_INFIX | AVH_PARALLEL);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_slist(infix_parallel) %s", "");
        if (slist_length(slist) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_slist(infix_parallel): wrong length");
            ++nerrors;
        }
        slist_free(slist, NULL);

        /* avltree_to_rbuf */
        BENCHS_START(tm_bench, bench);
        rbuf = avltree_to_rbuf(tree, AVH_INFIX);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_rbuf(infix) %s", "");
        if (rbuf_size(rbuf) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_rbuf(infix): wrong length");
            ++nerrors;
        }
        rbuf_free(rbuf);

        BENCHS_START(tm_bench, bench);
        rbuf = avltree_to_rbuf(tree, AVH_INFIX | AVH_PARALLEL);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_rbuf(infix_parallel) %s", "");
        if (rbuf_size(rbuf) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_rbuf(infix_parallel): wrong length");
            ++nerrors;
        }
        rbuf_free(rbuf);

        /* avltree_to_array */
        BENCHS_START(tm_bench, bench);
        len = avltree_to_array(tree, AVH_INFIX, &array);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_array(infix) %s", "");
        if (len != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_array(infix): wrong length");
            ++nerrors;
        }
        if (array) free(array);

        BENCHS_START(tm_bench, bench);
        len = avltree_to_array(tree, AVH_INFIX | AVH_PARALLEL, &array);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_array(infix_parallel) %s", "");
        if (len != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_array(infix_parallel): wrong length");
            ++nerrors;
        }
        if (array) free(array);

        /* remove (total_remove) elements */
        LOG_INFO(log, "* removing in tree (%zu nodes)", total_remove);
        BENCHS_START(tm_bench, bench);
        for (n_remove = 0; rbuf_size(del_vals) != 0; ++n_remove) {
            if (progress_max
            &&  (n_remove * (progress_max)) % total_remove == 0 && log->level >= LOG_LVL_INFO)
                fputc('.', log->out);
            value = (long) rbuf_pop(del_vals);

            LOG_DEBUG(log, "* deleting %ld", value);
            void * elt = avltree_remove(tree, (const void*)(value));
            if (elt != LG(value) || (elt == NULL && errno != 0)) {
                LOG_ERROR(log, "error removing elt <%ld>: %s", value, strerror(errno));
                nerrors++;
            }
            if ((n = avltree_count(tree)) != (int)(*nb - n_remove - 1)) {
                LOG_ERROR(log, "error avltree_count() : %d, expected %zd", n, *nb - n_remove - 1);
                nerrors++;
            }
            /* currently, don't visit or check balance in remove loop because this is too long */
        }
        if (progress_max && log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCHS_STOP_LOG(tm_bench, bench, log, "REMOVED %zu nodes | ", total_remove);
        rbuf_free(del_vals);

        /***** Checking tree after remove */
        LOG_INFO(log, "* checking BALANCE, prefix, infix, infix_r, parallel, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check balance (recursive) of %zd nodes | ", *nb - total_remove);

        /* count */
        BENCH_START(bench);
        n = avlprint_rec_get_count(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive COUNT (%zu nodes) = %d | ",
                       *nb - total_remove, n);

        BENCH_START(bench);
        value = avltree_count(tree);
        BENCH_STOP_LOG(bench, log, "COUNT (%zu nodes) = %ld | ",
                       *nb - total_remove, value);
        if (value != n || (size_t) value != (*nb - total_remove)) {
            LOG_ERROR(log, "error incorrect COUNT %ld, expected %d(rec), %zd",
                      value, n, *nb - total_remove);
            ++nerrors;
        }

        /* prefix */
        rbuf_reset(data.results);
        data.results=NULL; /* in order to run visit_checkprint() */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PREFIX visit of %zu nodes | ", *nb - total_remove);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check prefix visit with prefix (%zu nodes) | ",
                        *nb - total_remove);
        nerrors += data.nerrors;
        data.results = two_results;

        /* infix */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX visit of %zd nodes | ", *nb - total_remove);

        /* infix right */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "INFIX_RIGHT visit of %zd nodes | ", *nb - total_remove);

        /* parallel */
        rbuf_reset(data.results);
        data.results = NULL; /* needed because parallel avltree_visit_print */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX visit of %zu nodes | ",
                        *nb - total_remove);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        nerrors += data.nerrors;
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check // visit with prefix (%zu nodes) | ",
                        *nb - total_remove);
        data.results = two_results;

        /* parallel_merge */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as avltree_visit_print will create its own datas */
        data.nerrors = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PARALLEL_DUPDATA(AVH_PREFIX|AVH_MERGE, sizeof(data))) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX_MERGE visit of %zu nodes | ",
                        *nb - total_remove);
        if (data.nerrors != avltree_count(tree)) {
            LOG_ERROR(log, "parallel_prefix_merge: error expected %zu visits, got %lu",
                      avltree_count(tree), data.nerrors);
            ++nerrors;
        }
        data.nerrors = 0;
        data.results = two_results;

        /* free */
        LOG_INFO(log, "* freeing tree(insert)");
        BENCHS_START(tm_bench, bench);
        avltree_free(tree);
        BENCHS_STOP_LOG(tm_bench, bench, log, "freed %zd nodes | ", *nb - total_remove);
    }

    /* ****************************************
     * Trees with shared resources
     * ***************************************/
    avltree_t * tree2 = NULL;
    const size_t nb = 100000;
    slist_t * jobs = NULL;
    avltree_print_data_t data2 = data;
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TWO TREES with shared resources (insert, %zu elements)", nb);
    if ((tree = avltree_create(AFL_DEFAULT | AFL_SHARED_STACK, intcmp, NULL)) == NULL
    || (tree2 = avltree_create(AFL_DEFAULT & ~AFL_SHARED_STACK, intcmp, NULL)) == NULL
    || (data2.results = rbuf_create(rbuf_maxsize(data.results), RBF_DEFAULT | RBF_OVERWRITE))
                            == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    } else {
        tree2->shared = tree->shared;
        data.tree = tree;
        data2.tree = tree2;
    }

    /* insert */
    LOG_INFO(log, "* inserting in shared trees (insert, %zu elements)", nb);
    BENCHS_START(tm_bench, bench);
    for (size_t i = 0; i < nb; i++) {
        LOG_DEBUG(log, "* inserting %zu", i);
        long value = rand();
        value = (long)(value % (nb * 10));

        void * result = avltree_insert(tree, (void *) value);
        void * result2 = avltree_insert(tree2, (void *) value);

        if (progress_max && (i * (progress_max)) % nb == 0 && log->level >= LOG_LVL_INFO)
            fputc('.', log->out);

        if (result != (void *) value || result != result2 || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%ld>, result <%lx> : %s",
                     value, (unsigned long) result, strerror(errno));
            nerrors++;
        }
    }
    if (progress_max && log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    BENCHS_STOP_LOG(tm_bench, bench, log, "creation of %zu nodes (x2) ", nb);

    /* infix */
    rbuf_reset(data.results);
    rbuf_reset(data2.results);
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "VISITS of %zd nodes (x2) | ", nb);
    slist_free(jobs, NULL);

    data.max = data2.max = (void*) nb;
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(NULL, vjob_run(avltree_test_insert_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_insert_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "INSERTION of %zd more nodes (x2) | ", nb);
    slist_free(jobs, NULL);

    /* visits */
    rbuf_reset(data.results);
    rbuf_reset(data2.results);
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(NULL, vjob_run(avltree_test_visit_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "VISITS of %zd nodes (x2) | ", nb * 2);
    slist_free(jobs, NULL);

    rbuf_free(data2.results);
    avltree_free(tree2);
    avltree_free(tree);

    /* END */
    rbuf_free(two_results);
    rbuf_free(all_results);
    rbuf_free(all_refs);

    if (test != NULL) {
        ++(test->n_tests);
        if (nerrors == 0)
            ++(test->n_ok);
        test->n_errors += nerrors;
        nerrors = 0;
    }
    return VOIDP(TEST_END(test) + nerrors);
}

/* *************** SENSOR VALUE TESTS ************************************ */

#define TEST_SV_CMP_EQ(TYPE, sval, TYPE2, val2, log, _test)                 \
        do {                                                                \
            int eq = (sensor_value_equal(&sval, &val2) != 0)                \
                        == (sval.type == val2.type);                        \
            char ssv[64], sv2[64];                                          \
            /* expect compare==0 (equal) unless sval is buffer and val2 int */ \
            int cmp;                                                        \
            long double dbl = sensor_value_todouble(&sval);                 \
            if (SENSOR_VALUE_IS_FLOATING(sval.type)                         \
            &&  dbl - 0.1L != dbl && floorl(dbl) != dbl                     \
            && !SENSOR_VALUE_IS_FLOATING(val2.type)                         \
            && !SENSOR_VALUE_IS_BUFFER(val2.type)) {                        \
                /* double has decimal: expect compare with an int != 0 */   \
                cmp = sensor_value_compare(&sval, &val2) != 0;              \
            } else                                                          \
                cmp = (sensor_value_compare(&sval, &val2) == 0)             \
                        == (SENSOR_VALUE_IS_BUFFER(sval.type) == 0          \
                            || SENSOR_VALUE_IS_BUFFER(val2.type) != 0);     \
            LOG_VERBOSE(log, "test equal(%s)/compare(%s) 2 types "          \
                    "(" #TYPE " <> " #TYPE2 ")",                            \
                    eq == 0 ? "KO" : "ok", cmp==0 ? "KO" : "ok");           \
            sensor_value_tostring(&val2, sv2, sizeof(sv2) / sizeof(*sv2));  \
            sensor_value_tostring(&sval, ssv, sizeof(ssv) / sizeof(*ssv));  \
            TEST_CHECK2(_test, "equal(%s)/compare(%s) 2 types "             \
                   "(" #TYPE " & " #TYPE2 ") sval:'%s' val2:'%s'",          \
                   (eq != 0 && cmp != 0),                                   \
                   eq == 0 ? "KO" : "ok", cmp==0 ? "KO" : "ok", ssv, sv2);  \
        } while (0)

#define TEST_SENSOR2STRING(TYPE, sval, value, fmt, log, _test)              \
    do {                                                                    \
        char            ref[64];                                            \
        char            conv[64];                                           \
        sensor_value_t  new;                                                \
        sensor_value_t  dblval, intval, strval, bufval;                     \
        char            sbuf[64], bbuf[64];                                 \
        sval.type = TYPE;                                                   \
        /* Prepare String storing the reference value */                    \
        if (TYPE == SENSOR_VALUE_BYTES || TYPE == SENSOR_VALUE_STRING) {    \
            str0cpy(ref, (const char*)((ptrdiff_t)((size_t)value)), sizeof(ref)); \
        } else {                                                            \
            SENSOR_VALUE_GET(sval, TYPE) = (value);                         \
            snprintf(ref, sizeof(ref), fmt, SENSOR_VALUE_GET(sval, TYPE));  \
        }                                                                   \
        LOG_INFO(log, "sensor_value eq/cmp/conv: " #TYPE " (%s)", ref);     \
        /* create string, bytes, ldouble and int values from sval */        \
        SENSOR_VALUE_INIT_BUF(strval, SENSOR_VALUE_STRING, sbuf, 0);        \
        strval.data.b.maxsize = sizeof(sbuf) / sizeof(*sbuf);               \
        memset(strval.data.b.buf, 0, strval.data.b.maxsize);                \
        SENSOR_VALUE_INIT_BUF(bufval, SENSOR_VALUE_BYTES, bbuf, 0);         \
        bufval.data.b.maxsize = sizeof(bbuf) / sizeof(*bbuf);               \
        memset(bufval.data.b.buf, 0, bufval.data.b.maxsize);                \
        if (SENSOR_VALUE_IS_BUFFER(sval.type)) {                            \
            if (sval.type == SENSOR_VALUE_STRING) {                         \
                sensor_value_fromraw(sval.data.b.buf, &strval);             \
            } else {                                                        \
                strval.data.b.size = sval.data.b.size;                      \
                memcpy(strval.data.b.buf, sval.data.b.buf, sval.data.b.size);\
            }                                                               \
            bufval.data.b.size = sval.data.b.size;                          \
            sensor_value_fromraw(sval.data.b.buf, &bufval);                 \
        } else {                                                            \
            strval.data.b.size = sensor_value_tostring(&sval,               \
                                  strval.data.b.buf,strval.data.b.maxsize); \
            bufval.data.b.size = sensor_value_tostring(&sval,               \
                                  bufval.data.b.buf,bufval.data.b.maxsize); \
        }                                                                   \
        intval.type = SENSOR_VALUE_INT64;                                   \
        errno = 0; intval.data.i64 = sensor_value_toint(&sval);             \
        if (errno == EOVERFLOW) {                                           \
            /* int overflow, use uint64 instead */                          \
            intval.type = SENSOR_VALUE_UINT64;                              \
            intval.data.u64 += INTMAX_MAX;                                  \
        }                                                                   \
        dblval.type = SENSOR_VALUE_LDOUBLE;                                 \
        errno = 0; dblval.data.ld = sensor_value_todouble(&sval);           \
        if (errno == EOVERFLOW) {                                           \
            \
        }                                                                   \
        /* Test equal/compare between different types */                    \
        TEST_SV_CMP_EQ(TYPE, sval, SENSOR_VALUE_STRING, strval, log, _test);    \
        TEST_SV_CMP_EQ(TYPE, sval, SENSOR_VALUE_BYTES, bufval, log, _test);     \
        TEST_SV_CMP_EQ(TYPE, sval, SENSOR_VALUE_LDOUBLE, dblval, log, _test);   \
        TEST_SV_CMP_EQ(TYPE, sval, SENSOR_VALUE_INT64, intval, log, _test);     \
        /* Convert the value to string with libvsensors */                  \
        sensor_value_tostring(&sval, conv, sizeof(conv)/sizeof(*conv));     \
        /* Compare converted and reference value */                         \
        TEST_CHECK2(_test, "sensor_value_tostring(" #TYPE ") "              \
                          "res:%s expected:%s",                             \
                    (strcmp(ref, conv) == 0), conv, ref);                   \
        /* check sensor_value_fromraw() by copying sval to new */           \
        memset(&new, 0, sizeof(new));                                       \
        new.type = sval.type;                                               \
        if (TYPE == SENSOR_VALUE_BYTES || TYPE == SENSOR_VALUE_STRING) {    \
            new.data.b.buf = conv;                                          \
            new.data.b.maxsize = sizeof(conv) / sizeof(*conv);              \
            if (TYPE == SENSOR_VALUE_BYTES) new.data.b.size = sval.data.b.size; \
            sensor_value_fromraw((void*)((ptrdiff_t)((size_t)SENSOR_VALUE_GET(sval, TYPE))), &new);\
        } else {                                                            \
            sensor_value_fromraw(&SENSOR_VALUE_GET(sval, TYPE), &new);      \
        }                                                                   \
        /* Check sensor_value_equal() with sval == new */                   \
        TEST_CHECK2(_test, "sensor_value_equal(" #TYPE ") val:%s",          \
                    sensor_value_equal(&sval, &new) != 0, ref);             \
        /* Check sensor_value_compare() with sval == new */                 \
        TEST_CHECK2(_test, "sensor_value_compare(" #TYPE ") ref:%s",        \
                    (0 == sensor_value_compare(&sval, &new)), ref);         \
        /* For numbers, check sensor_value_compare with sval>new, sval<new */ \
        if (TYPE != SENSOR_VALUE_BYTES && TYPE != SENSOR_VALUE_STRING) {    \
            SENSOR_VALUE_GET(sval, TYPE)++;                                 \
            TEST_CHECK(_test, "sensor_value_compare(" #TYPE ") expected: >0", \
                        sensor_value_compare(&sval, &new) > 0);             \
            SENSOR_VALUE_GET(sval, TYPE)-=2LL;                              \
            TEST_CHECK(_test, "sensor_value_compare(" #TYPE ") expected: <0", \
                       sensor_value_compare(&sval, &new) < 0);              \
        }                                                                   \
    } while (0)

/*
            if (SENSOR_VALUE_IS_FLOATING(sval.type))                        \
                SENSOR_VALUE_GET(sval, TYPE) += 0.001;                      \
                if (sensor_value_compare(&sval, &new) <= 0) {               \
                    ++nerrors;                                              \
                    LOG_ERROR(log, "ERROR sensor_value_compare(" #TYPE ") expected: >0"); \
                }                                                           \
                SENSOR_VALUE_GET(sval, TYPE) -= 0.002;                      \
                if (sensor_value_compare(&sval, &new) >= 0) {               \
                    ++nerrors;                                              \
                    LOG_ERROR(log, "ERROR sensor_value_compare(" #TYPE ") expected: <0"); \
                }                                                           \
            }                                                               \
*/
static void * test_sensor_value(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "SENSOR_VALUE");
    log_t *         log = test != NULL ? test->log : NULL;
    unsigned long   r;
    unsigned long   i;
    unsigned long   nb_op = 10000000;
    int             cmp, vcmp;
    BENCH_DECL(t);

    sensor_value_t v1 = { .type = SENSOR_VALUE_INT, .data.i = 1000000 };
    sensor_value_t v2 = { .type = SENSOR_VALUE_INT, .data.i = v1.data.i+2*nb_op };
    sensor_sample_t s1 = { .value = v1 };
    sensor_sample_t s2 = { .value = v2 };

    #ifdef _DEBUG
    if (vlib_thread_valgrind(0, NULL)) {
        nb_op = 1000000;
    }
    #endif

    BENCH_START(t);

    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        cmp = (sensor_value_toint(&s1.value) < sensor_value_toint(&s2.value));
        TEST_CHECK2(test, "sensor_value_toint compare v1:%d v2:%d cmp=%d", cmp != 0,
                    s1.value.data.i, s2.value.data.i, cmp);
        r += cmp;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_toint", r, nb_op);

    TEST_CHECK2(test, "sensor_value_toint %lu - expected %lu", r == 2 * nb_op, r, 2 * nb_op);

    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        cmp = sensor_value_todouble(&s1.value) < sensor_value_todouble(&s2.value);
        TEST_CHECK2(test, "sensor_value_todouble compare v1:%d v2:%d cmp=%d", cmp != 0,
                    s1.value.data.i, s2.value.data.i, cmp);
        r += cmp;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_todouble", r, nb_op);

    TEST_CHECK2(test, "sensor_value_todouble %lu - expected %lu",
                r == 2 * nb_op, r, 2 * nb_op);

    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        vcmp = sensor_value_equal(&s1.value, &s2.value);
        TEST_CHECK2(test, "sensor_value_equal int v1:%d v2:%d vcmp=%d",
                    vcmp == 0, s1.value.data.i, s2.value.data.i, vcmp);
        r += vcmp;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_equal int", r, nb_op);

    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        vcmp = sensor_value_compare(&s1.value, &s2.value);
        TEST_CHECK2(test, "sensor_value_compare int v1:%d v2:%d vcmp=%d",
             vcmp < 0, s1.value.data.i, s2.value.data.i, vcmp);
        if (vcmp != 0)
            r += vcmp > 0 ? 1 : -1;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_compare int", r, nb_op);

    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        pthread_mutex_lock(&lock);
        s1.value.data.i = r++;
        vcmp = sensor_value_compare(&s2.value, &s1.value);
        pthread_mutex_unlock(&lock);
        TEST_CHECK2(test, "sensor_value_toint(mutex) compare v1:%d v2:%d vcmp=%d",
                    vcmp > 0, s1.value.data.i, s2.value.data.i, vcmp);
        if (vcmp != 0)
            r += vcmp > 0 ? 1 : -1;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_compare int mutex", r, nb_op);
    pthread_mutex_destroy(&lock);

    s1.value.type = SENSOR_VALUE_DOUBLE; s1.value.data.d = s1.value.data.i;
    s2.value.type = SENSOR_VALUE_DOUBLE; s2.value.data.d = s2.value.data.i;
    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        vcmp = sensor_value_equal(&s1.value, &s2.value);
        TEST_CHECK2(test, "sensor_value_equal double v1:%d v2:%d vcmp=%d",
                    vcmp == 0, s1.value.data.i, s2.value.data.i, vcmp);
        r += vcmp;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_equal double", r, nb_op);

    BENCH_START(t);
    for (i=0, r=0; i < nb_op; i++) {
        s1.value.data.i = r++;
        vcmp = sensor_value_compare(&s1.value, &s2.value);
        TEST_CHECK2(test, "sensor_value_compare double v1:%d v2:%d vcmp=%d",
                    vcmp < 0, s1.value.data.i, s2.value.data.i, vcmp);
        if (vcmp != 0)
            r += vcmp > 0 ? 1 : -1;
    }
    BENCH_STOP_LOG(t, log, "%-30s r=%-10lu ops=%-10lu", "sensor_value_compare double", r, nb_op);

    TEST_SENSOR2STRING(SENSOR_VALUE_UCHAR, s1.value, UCHAR_MAX-1, "%u", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_CHAR, s1.value, CHAR_MIN+1, "%d", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_UINT, s1.value, UINT_MAX-1U, "%u", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_INT, s1.value, INT_MIN+1, "%d", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_UINT16, s1.value, UINT16_MAX-1U, "%u", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_INT16, s1.value, INT16_MIN+1, "%d", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_UINT32, s1.value, UINT32_MAX-1U, "%u", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_INT32, s1.value, INT32_MIN+1, "%d", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_ULONG, s1.value, ULONG_MAX-1UL, "%lu", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_LONG, s1.value, LONG_MIN+1L, "%ld", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_FLOAT, s1.value, 252, "%f", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_DOUBLE, s1.value, 259, "%lf", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_LDOUBLE,s1.value,(long double)(ULONG_MAX-1)+0.1L,
                       "%Lf", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_LDOUBLE,s1.value,(long double)(UINT_MAX-1),
                       "%Lf", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_LDOUBLE,s1.value,(long double)(UINT_MAX-1)+0.1L,
                       "%Lf", log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_UINT64, s1.value, UINT64_MAX-1ULL, "%" PRIu64, log, test);
    TEST_SENSOR2STRING(SENSOR_VALUE_INT64, s1.value, INT64_MIN+1LL, "%" PRId64, log, test);

    char bytes[20]; s1.value.data.b.buf = bytes;
    s1.value.data.b.maxsize = sizeof(bytes) / sizeof(*bytes);
    memset(bytes, 0, s1.value.data.b.maxsize);
    s1.value.data.b.size
        = str0cpy(s1.value.data.b.buf, "abcdEf09381", s1.value.data.b.maxsize);
    TEST_SENSOR2STRING(SENSOR_VALUE_STRING, s1.value, "abcdEf09381", "%s", log, test);
    memcpy(bytes, "\xe3\x00\x45\xff\x6e", 5); s1.value.data.b.size = 5;
    TEST_SENSOR2STRING(SENSOR_VALUE_BYTES, s1.value, "e3 00 45 ff 6e", "%s", log, test);

    s1.value.type=SENSOR_VALUE_ULONG;
    s1.value.data.ul = ULONG_MAX;
    double d = sensor_value_todouble(&s1.value);
    long double ld = s1.value.data.ul;
    errno = 0;
    long double ldv = sensor_value_todouble(&s1.value);
    LOG_INFO(log, "sensor_value_todouble(%lu) = %lf ld:%Lf ul2ld:%Lf %s",
             s1.value.data.ul, d, ldv, ld, errno != 0 ? strerror(errno) : "");

    return VOIDP(TEST_END(test));
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
    ctx->log->out = NULL;
    return (void *) ((long)nerrors);
}

static const int                s_stop_signal   = SIGHUP;
static volatile sig_atomic_t    s_pipe_stop     = 0;

static void pipe_sighdl(int sig) {
    if (sig == s_stop_signal) { // && (si->si_pid == 0 || si->si_pid == pid))
        s_pipe_stop = 1;
    }
}

static void * pipe_log_thread(void * data) {
    char                buf[4096];
    struct sigaction    sa          = { .sa_handler = pipe_sighdl, .sa_flags = 0 };
    sigset_t            sigmask;
    FILE **             fpipe       = (FILE **) data;
    ssize_t             n;
    unsigned long       nerrors     = 0;
    int                 fd_pipein   = fileno(fpipe[PIPE_IN]);
    int                 fd_pipeout  = fileno(fpipe[PIPE_OUT]);
    int                 o_nonblock  = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, s_stop_signal);
    sigaction(s_stop_signal, &sa, NULL);
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
    while (1) {
        errno = 0;
        /* On signal reception, set read_NON_BLOCK so that we exit as soon as
         * no more data is available. */
        if (s_pipe_stop && !o_nonblock) {
            o_nonblock = 1;
            while ((n = fcntl(fd_pipein, F_SETFL, O_NONBLOCK)) < 0) {
                if (errno != EINTR) {
                    fpipe[PIPE_IN] = NULL;
                    return (void *) (nerrors + 1);
                }
            }
        }
        /* Read data, exit if none.
         * On EINTR, reloop and sighandler set pipestop to unblock read fd.*/
        if ((n = read(fd_pipein, buf, sizeof(buf))) < 0 && errno == EINTR) {
            continue ;
        } else if (n <= 0)
            break ;
        /* Write data. On EINTR, retry. sighandler will set pipestop to unblock read fd. */
        while (write(fd_pipeout, buf, n) < 0) {
            if (errno != EINTR) {
                nerrors++;
                break ;
            } else {
                /* EINTR: conitinue */
            }
        }
    }
    fpipe[PIPE_IN] = NULL;
    return (void *) nerrors;
}

static void * test_log_thread(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *       test = TEST_START(opts->testpool, "LOG");
    log_t *             log = test != NULL ? test->log : NULL;
    unsigned int        n_test_threads = N_TEST_THREADS;
    const char * const  files[] = { "stdout", "one_file", NULL };
    slist_t             *filepaths = NULL;
    const size_t        cmdsz = 4 * PATH_MAX;
    char *              cmd;
    int                 i_file;
    int                 ret;

    char                path[PATH_MAX];
    FILE *              file;
    FILE *              fpipeout = NULL;
    FILE *              fpipein = NULL;
    test_log_thread_t   * threads;
    log_t               * logs;
    log_t               all_logs[(sizeof(files)/sizeof(*files)) * n_test_threads];
    test_log_thread_t   all_threads[(sizeof(files)/sizeof(*files)) * n_test_threads];
    log_t               thlog = {
                            .level = LOG_LVL_SCREAM,
                            .flags = LOG_FLAG_DEFAULT | LOG_FLAG_FILE | LOG_FLAG_FUNC
                                | LOG_FLAG_LINE | LOG_FLAG_LOC_TAIL };
    char                prefix[20];
    pthread_t           pipe_tid;
    int                 p[2];
    int                 fd_pipein;
    int                 fd_backup;
    void *              thread_ret;
    FILE *              fpipe[2];

    if (vlib_thread_valgrind(0, NULL)) { /* dirty workaround */
        n_test_threads = 10;
    }

    fflush(NULL); /* First of all ensure all previous messages are flushed */
    i_file = 0;
    for (const char * const * filename = files; *filename; filename++, ++i_file) {
        BENCH_TM_DECL(tm0);
        BENCH_DECL(t0);

        threads = &(all_threads[i_file * n_test_threads]);
        logs = &(all_logs[i_file * n_test_threads]);

        file = fpipeout = fpipein = NULL;
        fd_pipein = fd_backup = -1;
        p[0] = p[1] = -1;

        /* generate unique tst file name for the current loop */
        snprintf(path, sizeof(path), "/tmp/test_thread_%s_%d_%u_%lx.log",
                 *filename, i_file, (unsigned int) getpid(), (unsigned long) time(NULL));
        filepaths = slist_prepend(filepaths, strdup(path));
        LOG_INFO(log, ">>> logthread: test %s (%s)", *filename, path);

        /* Create pipe to redirect stdout & stderr to pipe log file (fpipeout) */
        if ((!strcmp("stderr", *filename) && (file = stderr))
        ||  (!strcmp(*filename, "stdout") && (file = stdout))) {
            TEST_CHECK2(test, "create file '%s'",
                        (fpipeout = fopen(path, "w")) != NULL, path);
            if (fpipeout == NULL) {
                continue ;
            }
            TEST_CHECK(test, "pipe()", (ret = pipe(p)) == 0);
            if (ret != 0) {
                fclose(fpipeout);
                continue ;
            }
            TEST_CHECK(test, "pipe fd, dup(), dup2()",
                (ret = (p[PIPE_OUT] < 0 || (fd_backup = dup(fileno(file))) < 0
                        || dup2(p[PIPE_OUT], fileno(file)) < 0)) == 0);
            if (ret != 0) {
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                close(fd_backup);
                continue ;
            }
            TEST_CHECK(test, "fdopen(p[PIPE_IN])",
                       (fpipein = fdopen(p[PIPE_IN], "r")) != NULL);
            if (fpipein == NULL) {
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                continue ;
            }
            /* create thread reading data from pipe, and writing to pipe log file */
            fd_pipein = fileno(fpipein);
            fpipe[PIPE_IN] = fpipein;
            fpipe[PIPE_OUT] = fpipeout;
            TEST_CHECK(test, "pthread_create(pipe)",
                       (ret = pthread_create(&pipe_tid, NULL, pipe_log_thread, fpipe)) == 0);
            if (ret == 0 && vlib_thread_valgrind(0, NULL)) {
                pthread_detach(pipe_tid);
            }
        } else {
            TEST_CHECK2(test, "create file '%s'", (file = fopen(path, "w")) != NULL, path);
            if (file == NULL)
                continue ;
        }

        /* Create log threads and launch them */
        thlog.out = file;
        BENCH_TM_START(tm0);
        BENCH_START(t0);
        for (unsigned int i = 0; i < n_test_threads; ++i) {
            memcpy(&(logs[i]), &thlog, sizeof(log_t));
            snprintf(prefix, sizeof(prefix), "THREAD%05d", i);
            logs[i].prefix = strdup(prefix);
            threads[i].log = &logs[i];
            TEST_CHECK2(test, "pthread_create(#%u)",
                        (ret = pthread_create(&(threads[i].tid), NULL,
                                              log_thread, &(threads[i]))) == 0, i);
            if (ret != 0) {
                threads[i].log = NULL;
            } else if (vlib_thread_valgrind(0, NULL)) {
                pthread_detach(threads[i].tid);
            }
        }

        /* wait log threads to terminate */
        LOG_INFO(log, "Waiting logging threads...");
        for (unsigned int i = 0; i < n_test_threads; ++i) {
            thread_ret = NULL;
            if (vlib_thread_valgrind(0, NULL)) {
                while(threads[i].log->out != NULL) {
                    sleep(1);
                }
            } else {
                pthread_join(threads[i].tid, &thread_ret);
            }
            TEST_CHECK2(test, "log_thread #%u errors=0", (unsigned long) thread_ret == 0, i);
            if (threads[i].log->prefix) {
                char * to_free = threads[i].log->prefix;
                threads[i].log->prefix = "none";
                free(to_free);
            }
        }
        LOG_INFO(log, "logging threads finished.");
        fflush(file);
        BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);

        /* terminate log_pipe thread */
        if (fd_pipein >= 0) {
            fflush(NULL);
            LOG_INFO(log, "Waiting pipe thread...");
            thread_ret = NULL;
            if (vlib_thread_valgrind(0, NULL)) {
                s_pipe_stop = 1;
                sleep(2);
                while (fpipe[PIPE_IN] != NULL) {
                    sleep(1);
                    close(fd_pipein);
                }
                sleep(2);
                LOG_INFO(log, "[valgrind] pipe thread finished...");
                sleep(1);
            } else {
                pthread_kill(pipe_tid, s_stop_signal);
                pthread_join(pipe_tid, &thread_ret);
            }
            LOG_INFO(log, "pipe thread finished.");
            TEST_CHECK(test, "pipe_thread nerrors=0", (unsigned long) thread_ret == 0);
            /* restore redirected file */
            TEST_CHECK(test, "dup2() restore", dup2(fd_backup, fileno(file)) >= 0);
            /* cleaning */
            fclose(fpipein); // fclose will close p[PIPE_IN]
            close(p[PIPE_OUT]);
            close(fd_backup);
            fclose(fpipeout);
            /* write something in <file> and it must not be redirected anymore */
            ret = fprintf(file, "*** checking %s is not anymore redirected ***\n\n", *filename);
            TEST_CHECK(test, "fprintf stdout", ret > 0);
        } else {
            fclose(file);
        }
        LOG_INFO(log, "duration : %ld.%03lds (cpus %ld.%03lds)",
                 BENCH_TM_GET(tm0) / 1000, BENCH_TM_GET(tm0) % 1000,
                 BENCH_GET(t0) / 1000, BENCH_GET(t0) % 1000);
    }

    /* compare logs */
    LOG_INFO(log, "checking logs...");
    TEST_CHECK(test, "malloc shell command", (cmd = malloc(cmdsz)) != NULL);
    if (cmd != NULL) {
        size_t n = 0;
        /* construct a shell script iterating on each file, filter them to remove timestamps
         * and diff them */
        n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n, "ret=true; prev=; for f in ");
        SLIST_FOREACH_DATA(filepaths, spath, char *) {
            n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n, "%s ", spath);
        }
        n += VLIB_SNPRINTF(ret, cmd + n, cmdsz - n,
              "; do sed -e 's/^[.:0-9[:space:]]*//' -e s/'Thread #[0-9]*/Thread #X/' "
              "       -e 's/tid:[0-9]*/tid:X/'"
              "       \"$f\" | sort > \"${f%%.log}_filtered.log\"; "
              "     if test -n \"$prev\"; then "
              "         diff -u \"${prev%%.log}_filtered.log\" \"${f%%.log}_filtered.log\" "
              "           || { echo \"diff error (filtered $prev <> $f)\"; ret=false; };"
              "         grep -E '\\*\\*\\* checking .* "
              "                  is not anymore redirected \\*\\*\\*' \"${f}\""
              "           && { echo \"error: <${f}> pipe has not been restored\"; ret=false; };"
              "         rm \"$prev\" \"${prev%%.log}_filtered.log\";"
              "     fi; prev=$f; "
              " done; test -e \"$prev\" && rm \"$prev\" \"${prev%%.log}_filtered.log\"; $ret");
        TEST_CHECK(test, "log comparison", system(cmd) == 0);
        free(cmd);
    }
    slist_free(filepaths, free);

    return VOIDP(TEST_END(test));
}

/* *************** TEST BENCH *************** */
static volatile sig_atomic_t s_bench_stop;
static void bench_sighdl(int sig) {
    (void)sig;
    s_bench_stop = 1;
}
static void * test_bench(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *       test = TEST_START(opts->testpool, "BENCH");
    log_t *             log = test != NULL ? test->log : NULL;
    BENCH_DECL(t0);
    BENCH_TM_DECL(tm0);
    struct sigaction sa_bak, sa = { .sa_handler = bench_sighdl, .sa_flags = SA_RESTART };
    sigset_t sigset, sigset_bak;
    struct itimerval timer1_bak;
    const int step_ms = 300;
    const unsigned      margin_lvl[] = { LOG_LVL_ERROR, LOG_LVL_WARN };
    const char *        margin_str[] = { "error: ", "warning: " };
    const unsigned char margin_tm[]  = { 75, 15 };
    const unsigned char margin_cpu[] = { 100, 30 };
    unsigned int nerrors = 0;
    int (*sigmaskfun)(int, const sigset_t *, sigset_t *);

    sigmaskfun = (pthread_self() == opts->main) ? sigprocmask : pthread_sigmask;

    sigemptyset(&sa.sa_mask);

    if (log->level < LOG_LVL_INFO) {
        LOG_WARN(log, "%s(): BENCH_STOP_PRINTF not tested (log level too low).", __func__);
    } else {
        BENCH_START(t0);
        BENCH_STOP_PRINTF(t0, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                              12UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    }

    BENCH_START(t0);
    BENCH_STOP_LOG(t0, log, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                   40UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    BENCH_STOP_PRINT(t0, LOG_INFO, log, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                     98UL, "STRING", 'Z', (void*)&nerrors, nerrors);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, log, "// fake-fmt-check LOG%s //", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, log, "// fake-fmt-check LOG %d //", 54);

    if (log->level < LOG_LVL_INFO) {
        LOG_WARN(log, "%s(): BENCH_STOP_PRINTF not tested (log level too low).", __func__);
    } else {
        BENCH_TM_START(tm0);
        BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF%s || ", "");
        BENCH_TM_START(tm0);
        BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF %d || ", 65);
    }

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_INFO, log, "__/ fake-fmt-check1 PRINT=LOG_INFO %s\\__ ", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_INFO, log, "__/ fake-fmt-check2 PRINT=LOG_INFO %s\\__ ", "");
    LOG_INFO(log, NULL);

    /* block SIGALRM (sigsuspend will unlock it) */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    TEST_CHECK(test, "sigmaskfun()", sigmaskfun(SIG_SETMASK, &sigset, &sigset_bak) == 0);

    sigfillset(&sa.sa_mask);
    TEST_CHECK(test, "sigaction()", sigaction(SIGALRM, &sa, &sa_bak) == 0);
    TEST_CHECK(test, "getitimer()", getitimer(ITIMER_REAL, &timer1_bak) == 0);

    for (int i = 0; i < 5; i++) {
        struct itimerval timer123 = {
            .it_value =    { .tv_sec = 0, .tv_usec = 123678 },
            .it_interval = { .tv_sec = 0, .tv_usec = 0 },
        };
        BENCH_TM_START(tm0); BENCH_START(t0); BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);
        LOG_INFO(log, "BENCH measured with BENCH_TM DURATION=%ldns cpus=%ldns",
                 BENCH_TM_GET_NS(tm0), BENCH_GET_NS(t0));

        BENCH_START(t0); BENCH_TM_START(tm0); BENCH_TM_STOP(tm0);
        BENCH_STOP(t0);
        LOG_INFO(log, "BENCH_TM measured with BENCH cpu=%ldns DURATION=%ldns",
                 BENCH_GET_NS(t0), BENCH_TM_GET_NS(tm0));

        TEST_CHECK(test, "setitimer()", setitimer(ITIMER_REAL, &timer123, NULL) == 0);

        sigfillset(&sigset);
        sigdelset(&sigset, SIGALRM);

        BENCH_TM_START(tm0);
        sigsuspend(&sigset);
        BENCH_TM_STOP(tm0);
        LOG_INFO(log, "BENCH_TM timer(123678) DURATION=%ldns", BENCH_TM_GET_NS(tm0));

        for (unsigned wi = 0; wi < 2; wi++) {
            if ((BENCH_TM_GET_US(tm0) < ((123678 * (100-margin_tm[wi])) / 100)
            ||  BENCH_TM_GET_US(tm0) > ((123678 * (100+margin_tm[wi])) / 100))) {
                vlog(margin_lvl[wi], NULL, __FILE__, __func__, __LINE__,
                     "%s: BAD TM_bench %lu, expected %d with margin %u%%",
                     margin_str[wi],
                     (unsigned long)(BENCH_TM_GET(tm0)), 123678, margin_tm[wi]);

                TEST_CHECK2(test, "TM_bench %lu below error margin(%d+/-%u%%)",
                        margin_lvl[wi] != LOG_LVL_ERROR,
                        (unsigned long)(BENCH_TM_GET(tm0)), 123678, margin_tm[wi]);
                break ;
            }
        }
    }
    LOG_INFO(log, NULL);

    /* unblock SIGALRM as we poll on s_bench_stop instead of using sigsuspend */
    sigemptyset(&sigset);
    TEST_CHECK(test, "sigmaskfun()", sigmaskfun(SIG_SETMASK, &sigset, NULL) == 0);

    for (int i=0; i< 2000 / step_ms; i++) {
        struct itimerval timer_bak, timer = {
            .it_value     = { .tv_sec = step_ms / 1000, .tv_usec = (step_ms % 1000) * 1000 },
            .it_interval  = { 0, 0 }
        };

        s_bench_stop = 0;
        TEST_CHECK(test, "setitimer", setitimer(ITIMER_REAL, &timer, &timer_bak) == 0);

        BENCH_START(t0);
        BENCH_TM_START(tm0);

        while (s_bench_stop == 0)
            ;
        BENCH_TM_STOP(tm0);
        BENCH_STOP(t0);

        for (unsigned wi = 0; wi < 2; wi++) {
            if (i > 0 && (BENCH_GET(t0) < ((step_ms * (100-margin_cpu[wi])) / 100)
                          || BENCH_GET(t0) > ((step_ms * (100+margin_cpu[wi])) / 100))) {
                vlog(margin_lvl[wi], NULL, __FILE__, __func__, __LINE__,
                     "%s: BAD cpu bench %lu, expected %d with margin %u%%",
                     margin_str[wi],
                     (unsigned long)(BENCH_GET(t0)), step_ms, margin_cpu[wi]);

                TEST_CHECK2(test, "CPU_bench %lu below error margin(%d+/-%u%%)",
                        margin_lvl[wi] != LOG_LVL_ERROR,
                        (unsigned long)(BENCH_GET(t0)), step_ms, margin_cpu[wi]);
                break ;
            }
        }
        for (unsigned wi = 0; wi < 2; wi++) {
            if (i > 0 && (BENCH_TM_GET(tm0) < ((step_ms * (100-margin_tm[wi])) / 100)
                          || BENCH_TM_GET(tm0) > ((step_ms * (100+margin_tm[wi])) / 100))) {
                vlog(margin_lvl[wi], NULL, __FILE__, __func__, __LINE__,
                     "%s: BAD TM_bench %lu, expected %d with margin %u%%",
                     margin_str[wi],
                     (unsigned long)(BENCH_TM_GET(tm0)), step_ms, margin_tm[wi]);
                TEST_CHECK2(test, "TM_bench %lu below error margin(%d+/-%u%%)",
                        margin_lvl[wi] != LOG_LVL_ERROR,
                        (unsigned long)(BENCH_TM_GET(tm0)), step_ms, margin_tm[wi]);
                break ;
            }
        }
    }
    /* no need to restore timer as it_interval is 0. */
    if (setitimer(ITIMER_REAL, &timer1_bak, NULL) < 0) {
        ++nerrors;
        LOG_ERROR(log, "restore setitimer(): %s\n", strerror(errno));
    }
    TEST_CHECK(test, "restore sigaction()",
               sigaction(SIGALRM, &sa_bak, NULL) == 0);
    TEST_CHECK(test, "restore sigmaskfun()",
               sigmaskfun(SIG_SETMASK, &sigset_bak, NULL) == 0);

    return VOIDP(TEST_END(test));
}

/* *************** TEST ACCOUNT *************** */
#define TEST_ACC_USER "nobody"
#define TEST_ACC_GROUP "nogroup"
static void * test_account(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "ACCOUNT");
    log_t *         log = test != NULL ? test->log : NULL;
    struct passwd   pw, * ppw;
    struct group    gr, * pgr;
    char *          user = TEST_ACC_USER;
    char *          group = TEST_ACC_GROUP;
    char *          buffer = NULL;
    char *          bufbak;
    size_t          bufsz = 0;
    uid_t           uid, refuid = 0;
    gid_t           gid, refgid = 500;
    int             ret;

    if (test == NULL) {
        return VOIDP(1);
    }

    while (refuid <= 500 && (ppw = getpwuid(refuid)) == NULL) refuid++;
    if (ppw != NULL)
        user = strdup(ppw->pw_name);
    while (refgid + 1 > 0 && refgid <= 500 && (pgr = getgrgid(refgid)) == NULL) refgid--;
    if (pgr != NULL)
        group = strdup(pgr->gr_name);

    LOG_INFO(log, "accounts: testing with user `%s`(%ld) and group `%s`(%ld)",
             user, (long) refuid, group, (long) refgid);

    /* pwfindid_r/grfindid_r with NULL buffer */
    TEST_CHECK2(test, "pwfindid_r(\"%s\", &uid, NULL, NULL) returns %d, uid:%ld",
        (ret = pwfindid_r(user, &uid, NULL, NULL)) == 0 && uid == refuid,
        user, ret, (long) uid);

    TEST_CHECK2(test, "grfindid_r(\"%s\", &gid, NULL, NULL) returns %d, gid:%ld",
        (ret = grfindid_r(group, &gid, NULL, NULL)) == 0 && gid == refgid,
        group, ret, (long) gid);

    TEST_CHECK(test, "pwfindid_r(\"__**UserNoFOOUUnd**!!\", &uid, NULL, NULL) "
                     "expecting error",
                (ret = pwfindid_r("__**UserNoFOOUUnd**!!", &uid, NULL, NULL)) != 0);

    TEST_CHECK(test, "grfindid_r(\"__**GroupNoFOOUUnd**!!\", &gid, NULL, NULL) "
                     "expecting error",
        (ret = grfindid_r("__**GroupNoFOOUUnd**!!", &gid, NULL, NULL)) != 0);

    TEST_CHECK2(test, "pwfindid_r(\"%s\", &uid, &buffer, NULL) expecting error",
        (ret = pwfindid_r(user, &uid, &buffer, NULL)) != 0, user);

    TEST_CHECK2(test, "grfindid_r(\"%s\", &gid, &buffer, NULL) expecting error",
        (ret = grfindid_r(group, &gid, &buffer, NULL)) != 0, group);

    /* pwfindid_r/grfindid_r with shared buffer */
    TEST_CHECK2(test, "pwfindid_r(\"%s\", &uid, &buffer, &bufsz) "
                      "returns %d, uid:%ld, buffer:0x%lx bufsz:%zu",
        (ret = pwfindid_r(user, &uid, &buffer, &bufsz)) == 0
            && buffer != NULL && uid == refuid,
        user, ret, (long) uid, (unsigned long) buffer, bufsz);

    bufbak = buffer;
    TEST_CHECK2(test, "grfindid_r(\"%s\", &gid, &buffer, &bufsz) "
                      "returns %d, gid:%ld, buffer:0x%lx bufsz:%zu ",
        (ret = grfindid_r(group, &gid, &buffer, &bufsz)) == 0
            && buffer == bufbak && gid == refgid,
        group, ret, (long) gid, (unsigned long) buffer, bufsz);

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with shared buffer */
    TEST_CHECK2(test, "pwfind_r(\"%s\", &pw, &buffer, &bufsz) "
                      "returns %d, uid:%ld, buffer:0x%lx bufsz:%zu",
        (ret = pwfind_r(user, &pw, &buffer, &bufsz)) == 0
            && buffer == bufbak && uid == refuid,
        user, ret, (long) pw.pw_uid, (unsigned long) buffer, bufsz);

    TEST_CHECK2(test, "grfind_r(\"%s\", &gr, &buffer, &bufsz) "
                      "returns %d, gid:%ld, buffer:0x%lx bufsz:%zu",
        (ret = grfind_r(group, &gr, &buffer, &bufsz)) == 0
            && buffer == bufbak && gid == refgid,
        group, ret, (long) gr.gr_gid, (unsigned long) buffer, bufsz);

    TEST_CHECK2(test, "pwfindbyid_r(%ld, &pw, &buffer, &bufsz) "
                      "returns %d, user:\"%s\", buffer:0x%lx bufsz:%zu",
        (ret = pwfindbyid_r(refuid, &pw, &buffer, &bufsz)) == 0
            && buffer == bufbak && strcmp(user, pw.pw_name) == 0,
        (long) refuid, ret, pw.pw_name, (unsigned long) buffer, bufsz);

    TEST_CHECK2(test, "grfindbyid_r(%ld, &gr, &buffer, &bufsz) "
                      "returns %d, group:\"%s\", buffer:0x%lx bufsz:%zu",
        (ret = grfindbyid_r(refgid, &gr, &buffer, &bufsz)) == 0
            && buffer == bufbak && strcmp(group, gr.gr_name) == 0,
        (long) refgid, ret, gr.gr_name, (unsigned long) buffer, bufsz);

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with NULL buffer */
    TEST_CHECK2(test, "pwfind_r(\"%s\", &pw, NULL, &bufsz) expecting error",
        (ret = pwfind_r(user, &pw, NULL, &bufsz)) != 0, user);

    TEST_CHECK2(test, "grfind_r(\"%s\", &gr, NULL, &bufsz) expecting error",
        (ret = grfind_r(group, &gr, NULL, &bufsz)) != 0, group);

    TEST_CHECK2(test, "pwfindbyid_r(%ld, &pw, NULL, &bufsz) expecting error",
        (ret = pwfindbyid_r(refuid, &pw, NULL, &bufsz)) != 0, (long) refuid);

    TEST_CHECK2(test, "grfindbyid_r(%ld, &gr, &buffer, NULL) "
                      "expecting error and ! buffer changed",
        (ret = grfindbyid_r(refgid, &gr, &buffer, NULL)) != 0 && buffer == bufbak,
        (long) refgid);

    if (buffer != NULL) {
        free(buffer);
    }
    if (user != NULL && ppw != NULL) {
        free(user);
    }
    if (group != NULL && pgr != NULL) {
        free(group);
    }

    return VOIDP(TEST_END(test));
}

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
    vlib_thread_t * target;
    int             pipe_fdout;
    ssize_t         offset;
} pipethread_ctx_t;

static int  piperead_callback(
                vlib_thread_t *         vthread,
                vlib_thread_event_t     event,
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
                vlib_thread_t *         vthread,
                vlib_thread_event_t     event,
                void *                  event_data,
                void *                  user_data) {
    pipethread_ctx_t * data = (pipethread_ctx_t *) user_data;
    (void) vthread;
    (void) event_data;
    (void) event;

    LOG_DEBUG(data->log, "%s(): enter", __func__);

    ++(data->nb_try);
    if (vlib_thread_pipe_write(data->target, data->pipe_fdout, PIPETHREAD_STR,
                sizeof(PIPETHREAD_STR) - 1) != sizeof(PIPETHREAD_STR) - 1) {
        LOG_ERROR(data->log, "error vlib_thread_pipe_write(pipestr): %s", strerror(errno));
        ++(data->nb_error);
    }

    ++(data->nb_bigtry);
    if (vlib_thread_pipe_write(data->target, data->pipe_fdout, data->bigpipebuf,
            PIPETHREAD_BIGSZ) != PIPETHREAD_BIGSZ) {
        LOG_ERROR(data->log, "error vlib_thread_pipe_write(bigpipebuf): %s", strerror(errno));
        ++(data->nb_error);
    }

    return 0;
}

static void * test_thread(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test            = TEST_START(opts->testpool, "THREAD");
    log_t *         log             = test != NULL ? test->log : NULL;
    vlib_thread_t * vthread;
    void *          thread_result   = (void *) 1UL;
    const long      bench_margin_ms = 400;
    long            bench;

    BENCH_TM_DECL(t);

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 0, kill before start");
    TEST_CHECK(test, "vlib_thread_create(t=0,kill+0)",
               (vthread = vlib_thread_create(0, log)) != NULL);

    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vlib_thread_stop(t=0,kill+0)",
               vlib_thread_stop(vthread) == thread_result);

    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 500, start and kill after 1s");
    TEST_CHECK(test, "vlib_thread_create(t=500,kill+1)",
               (vthread = vlib_thread_create(500, log)) != NULL);
    TEST_CHECK(test, "vlib_thread_start(t=500,kill+1)",
               vlib_thread_start(vthread) == 0);

    sched_yield();
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vlib_thread_stop(t=500,kill+1)",
               vlib_thread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, start and kill after 1s");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vlib_thread_create(t=0,kill+1)",
               (vthread = vlib_thread_create(0, log)) != NULL);
    TEST_CHECK(test, "vlib_thread_start(t=0,kill+1)",
               vlib_thread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vlib_thread_stop(t=0,kill+1)",
               vlib_thread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0 exit_sig SIGALRM, start and kill after 1s");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vlib_thread_create(t=0,sig=alrm,kill+1)",
               (vthread = vlib_thread_create(0, log)) != NULL);
    TEST_CHECK(test, "vlib_thread_exit_signal(ALRM)",
               vlib_thread_set_exit_signal(vthread, SIGALRM) == 0);
    TEST_CHECK(test, "vlib_thread_start(t=0,sig=alrm,kill+1)",
               vlib_thread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vlib_thread_stop(t=0,sig=alrm,kill+1)",
               vlib_thread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, exit_sig SIGALRM after 500ms, "
                   "start and kill after 500 more ms");
    BENCH_TM_START(t);
    TEST_CHECK(test, "vlib_thread_create(t=0,sig=alrm+.5,kill+1)",
               (vthread = vlib_thread_create(0, log)) != NULL);
    TEST_CHECK(test, "vlib_thread_start(t=0,sig=alrm+.5,kill+1)",
               vlib_thread_start(vthread) == 0);
    LOG_INFO(log, "sleeping");
    usleep(500000);
    TEST_CHECK(test, "vlib_thread_exit_signal(ALRM)",
               vlib_thread_set_exit_signal(vthread, SIGALRM) == 0);
    usleep(500000);
    LOG_INFO(log, "killing");
    TEST_CHECK(test, "vlib_thread_stop(t=0,sig=alrm+.5,kill+1)",
               vlib_thread_stop(vthread) == thread_result);
    BENCH_TM_STOP(t);
    bench = BENCH_TM_GET(t);
    TEST_CHECK2(test, "test duration: %ld ms (margin:%ld)",
                bench <= 1000 + bench_margin_ms, bench, bench_margin_ms);

    /* **** */
    LOG_INFO(log, "creating multiple threads");
    const unsigned      nthreads = 50;
    int                 pipefd = -1;
    vlib_thread_t *     vthreads[nthreads];
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
        TEST_CHECK2(test, "vlib_thread_create(#%zu)",
                ((vthreads[i] = vlib_thread_create(i % 5 == 0 ? 10 : 0, &logs[i])) != NULL), i);
        if (vthreads[i] != NULL) {
            all_pipectx[i] = pipectx;
            all_pipectx[i].log = &logs[i];
            if (i == 0) {
                TEST_CHECK2(test, "vlib_thread_pipe_create(#%zu)",
                    ((pipefd = vlib_thread_pipe_create(vthreads[i], piperead_callback,
                                                      &(all_pipectx[0]))) >= 0), i);
                all_pipectx[0].pipe_fdout = pipefd;
                all_pipectx[0].target = vthreads[0];
                pipectx.pipe_fdout = pipefd;
                pipectx.target = vthreads[0];
            }
            if (i && i % 5 == 0) {
                TEST_CHECK2(test, "vlib_register_event(#%zu,pipe_write,proc_start)",
                    vlib_thread_register_event(vthreads[i], VTE_PROCESS_START, NULL,
                            thread_loop_pipe_write, &(all_pipectx[i])) == 0, i);
            }
            TEST_CHECK2(test, "vlib_thread_start(#%zu)",
                       vlib_thread_start(vthreads[i]) == 0, i);
            ++(all_pipectx[0].nb_try);
            TEST_CHECK2(test, "vlib_thread_pipe_write(#%zu,small)",
                vlib_thread_pipe_write(vthreads[0], pipefd, PIPETHREAD_STR,
                                       sizeof(PIPETHREAD_STR) - 1) == sizeof(PIPETHREAD_STR) - 1, i);

            ++(all_pipectx[0].nb_bigtry);
            TEST_CHECK2(test, "vlib_thread_pipe_write(#%zu,big)",
                vlib_thread_pipe_write(vthreads[0], pipefd, pipectx.bigpipebuf, PIPETHREAD_BIGSZ)
                        == PIPETHREAD_BIGSZ, i);
        }
    }
    sleep(2);
    for (int i = sizeof(vthreads) / sizeof(*vthreads) - 1; i >= 0; i--) {
        if (vthreads[i] != NULL) {
            TEST_CHECK2(test, "vlib_thread_stop(#%d)",
                        vlib_thread_stop(vthreads[i]) == thread_result, i);
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

/* *************** TEST VDECODE_BUFFER *************** */

static const unsigned char zlibbuf[] = {
    31,139,8,0,239,31,168,90,0,3,51,228,2,0,83,252,81,103,2,0,0,0
};
static const char * const strtabbuf[] = {
    VDECODEBUF_STRTAB_MAGIC,
    "String1", "String2",
    "Long String 3 ___________________________________________________________"
        "________________________________________________________________"
        "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++_",
    NULL
};
static char rawbuf[] = { 0x0c,0x0a,0x0f,0x0e,
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
};
static char rawbuf2[] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
};

int test_one_bufdecode(const char * inbuf, size_t inbufsz, char * outbuf, size_t outbufsz,
                       const char * refbuffer, size_t refbufsz, const char * desc,
                       testgroup_t * test) {
    char decoded_buffer[32768];
    void * ctx = NULL;
    unsigned int total = 0;
    int n;

    /* decode and assemble in allbuffer */
    while ((n = vdecode_buffer(NULL, outbuf, outbufsz, &ctx, inbuf, inbufsz)) > 0) {
        memcpy(decoded_buffer + total, outbuf, n);
        total += n;
    }
    /* check wheter ctx is set to NULL after call */
    TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) ctx NULL", ctx == NULL, desc, outbufsz);

    if (outbufsz == 0 || inbuf == NULL
#  if ! CONFIG_ZLIB
    || (inbufsz >= 3 && inbuf[0] == 31 && (unsigned char)(inbuf[1]) == 139 && inbuf[2] == 8)
#  endif
    ) {
        /* (outbufsz=0 or inbuf == NULL) -> n must be -1, total must be 0 */
        TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) returns -1",
                    n == -1, desc, outbufsz);

        TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) outbufsz 0 n_decoded=0, got %u",
                    total == 0, desc, outbufsz, total);
    } else {
        /* outbufsz > 0: n must be 0, check whether decoded total is refbufsz */
        TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) returns 0, got %d", n == 0,
                    desc, outbufsz, n);

        TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) total(%u) = ref(%zu)",
                    total == refbufsz, desc, outbufsz, total, refbufsz);

        if (total == refbufsz && total != 0) {
            /* compare refbuffer and decoded_buffer */
            TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) decoded = reference",
                0 == memcmp(refbuffer, decoded_buffer, total), desc, outbufsz);
        }
    }
    return 0;
}

static void * test_bufdecode(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "VDECODE_BUF");
    unsigned        n;
    char            buffer[16384];
    char            refbuffer[16384];
    unsigned        bufszs[] = { 0, 1, 2, 3, 8, 16, 3000 };
#ifdef _DEBUG
    log_t *         log = test != NULL ? test->log : NULL;
#endif

#  if ! CONFIG_ZLIB
    LOG_INFO(log, "warning: no zlib on this system");
#  endif

    for (unsigned n_bufsz = 0; n_bufsz < sizeof(bufszs) / sizeof(*bufszs); n_bufsz++) {
        unsigned bufsz = bufszs[n_bufsz];
        unsigned int refbufsz;

        /* NULL or 0 size inbuffer */
        LOG_DEBUG(log, "* NULL inbuf (outbufsz:%u)", bufsz);
        TEST_CHECK2(test, "test_onedecode NULL buffer (outbufsz:%u)",
                    0 == test_one_bufdecode(NULL, 198, buffer, bufsz, NULL, 0,
                                           "inbuf=NULL", test), bufsz);

        LOG_DEBUG(log, "* inbufsz=0 (outbufsz:%u)", bufsz);

        TEST_CHECK2(test, "raw buf inbufsz=0 (outbufsz:%u)",
                    0 == test_one_bufdecode((const char *) rawbuf, 0,
                                            buffer, bufsz, NULL, 0, "inbufsz=0", test), bufsz);

        /* ZLIB */
        LOG_DEBUG(log, "* ZLIB (outbufsz:%u)", bufsz);
        str0cpy(refbuffer, "1\n", sizeof(refbuffer));
        refbufsz = 2;
        TEST_CHECK2(test, "ZLIB (outbufsz:%u)",
            0 == test_one_bufdecode((const char *) zlibbuf, sizeof(zlibbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "zlib", test), bufsz);

        /* RAW with MAGIC */
        LOG_DEBUG(log, "* RAW (outbufsz:%u)", bufsz);
        for (n = 4; n < sizeof(rawbuf) / sizeof(char); n++) {
            refbuffer[n - 4] = rawbuf[n];
        }
        refbufsz = n - 4;
        TEST_CHECK2(test, "RAW magic (outbufsz:%u)",
            0 == test_one_bufdecode(rawbuf, sizeof(rawbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "raw", test), bufsz);

        /* RAW WITHOUT MAGIC */
        LOG_DEBUG(log, "* RAW without magic (outbufsz:%u)", bufsz);
        for (n = 0; n < sizeof(rawbuf2) / sizeof(char); n++) {
            refbuffer[n] = rawbuf2[n];
        }
        refbufsz = n;
        TEST_CHECK2(test, "RAW without magic (outbufsz:%u)",
            0 == test_one_bufdecode(rawbuf2, sizeof(rawbuf2) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "raw2", test), bufsz);

        /* STRTAB */
        LOG_DEBUG(log, "* STRTAB (outbufsz:%u)", bufsz);
        refbufsz = 0;
        for (const char *const* pstr = strtabbuf+1; *pstr; pstr++) {
            size_t n = str0cpy(refbuffer + refbufsz, *pstr, sizeof(refbuffer) - refbufsz);
            refbufsz += n;
        }
        TEST_CHECK2(test, "STRTAB (outbufsz:%u)",
            0 == test_one_bufdecode((const char *) strtabbuf, sizeof(strtabbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "strtab", test), bufsz);
    }

    return VOIDP(TEST_END(test));
}

/* *************** TEST SOURCE FILTER *************** */

void opt_set_source_filter_bufsz(size_t bufsz);
int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx);

#define FILE_PATTERN        "/* #@@# FILE #@@# "
#define FILE_PATTERN_END    " */\n"

static void * test_srcfilter(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *       test = TEST_START(opts->testpool, "SRC_FILTER");
    log_t *             log = test != NULL ? test->log : NULL;

    if (test == NULL) {
        return VOIDP(1);
    }
#  ifndef APP_INCLUDE_SOURCE
    LOG_INFO(log, ">>> SOURCE_FILTER tests: APP_INCLUDE_SOURCE undefined, skipping tests");
#  else
    char                tmpfile[PATH_MAX];
    char                cmd[PATH_MAX*4];
    int                 vlibsrc, sensorsrc, vsensorsdemosrc, testsrc;

    const char * const  files[] = {
        /*----------------------------------*/
        "@vsensorsdemo", "",
        "LICENSE", "README.md", "src/main.c", "version.h", "build.h",
        "src/screenloop.c", "src/logloop.c",
        "ext/libvsensors/include/libvsensors/sensor.h",
        "ext/vlib/include/vlib/account.h",
        "ext/vlib/include/vlib/avltree.h",
        "ext/vlib/include/vlib/hash.h",
        "ext/vlib/include/vlib/log.h",
        "ext/vlib/include/vlib/logpool.h",
        "ext/vlib/include/vlib/options.h",
        "ext/vlib/include/vlib/rbuf.h",
        "ext/vlib/include/vlib/slist.h",
        "ext/vlib/include/vlib/thread.h",
        "ext/vlib/include/vlib/time.h",
        "ext/vlib/include/vlib/util.h",
        "ext/vlib/include/vlib/vlib.h",
        "ext/vlib/include/vlib/term.h",
        "ext/vlib/include/vlib/job.h",
        "ext/vlib/include/vlib/test.h",
        "Makefile", "config.make",
        /*----------------------------------*/
        "@" BUILD_APPNAME, "test/",
        "LICENSE", "README.md", "version.h", "build.h",
        "test.c", "test-sensorplugin.c", "test-math.c",
        /*----------------------------------*/
        "@vlib", "ext/vlib/",
        "src/term.c",
        "src/avltree.c",
        "src/log.c",
        "src/logpool.c",
        "src/options.c",
        /*----------------------------------*/
        "@libvsensors", "ext/libvsensors/",
        "src/sensor.c",
        NULL
    };

    void * ctx = NULL;
    vsensorsdemosrc = (vsensorsdemo_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    vsensorsdemo_get_source(NULL, NULL, 0, &ctx);
    testsrc = (test_vsensorsdemo_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    test_vsensorsdemo_get_source(NULL, NULL, 0, &ctx);
    vlibsrc = (vlib_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    vlib_get_source(NULL, NULL, 0, &ctx);
    sensorsrc = (libvsensors_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    libvsensors_get_source(NULL, NULL, 0, &ctx);

    if (vsensorsdemosrc == 0) {
        LOG_WARN(log, "warning: vsensorsdemo is built without APP_INCLUDE_SOURCE");
    }
    if (testsrc == 0) {
        LOG_WARN(log, "warning: " BUILD_APPNAME  " is built without APP_INCLUDE_SOURCE");
    }
    if (vlibsrc == 0) {
        LOG_WARN(log, "warning: vlib is built without APP_INCLUDE_SOURCE");
    }
    if (sensorsrc == 0) {
        LOG_WARN(log, "warning: libvsensors is built without APP_INCLUDE_SOURCE");
    }

    size_t max_filesz = 0;

    for (const char * const * file = files; *file; file++) {
        size_t n = strlen(*file);
        if (n > max_filesz) {
            max_filesz = n;
        }
    }
    static const char * const projects[] = { BUILD_APPNAME, "vsensorsdemo", "vlib", "libvsensors", NULL };
    unsigned int maxprjsz = 0;
    for (const char * const * prj = projects; *prj != NULL; ++prj) {
        if (strlen(*prj) > maxprjsz)    maxprjsz = strlen(*prj);
    }
    max_filesz += maxprjsz + 1;

    const size_t min_buffersz = sizeof(FILE_PATTERN) - 1 + max_filesz + sizeof(FILE_PATTERN_END);
    const size_t sizes_minmax[] = {
        min_buffersz - 1,
        min_buffersz + 10,
        sizeof(FILE_PATTERN) - 1 + PATH_MAX + sizeof(FILE_PATTERN_END),
        sizeof(FILE_PATTERN) - 1 + PATH_MAX + sizeof(FILE_PATTERN_END) + 10,
        PATH_MAX * 2,
        PATH_MAX * 2,
        SIZE_MAX
    };

    str0cpy(tmpfile, "tmp_srcfilter.XXXXXXXX", sizeof(tmpfile));
    int fd = mkstemp(tmpfile);
    FILE * out;

    TEST_CHECK(test, "open logfile", (out = fdopen(fd, "w")) != NULL);
    if (out != NULL) {
        for (const size_t * sizemin = sizes_minmax; *sizemin != SIZE_MAX; sizemin++) {
            for (size_t size = *(sizemin++); size <= *sizemin; size++) {
                size_t tmp_errors = 0;
                const char * prj = "vsensorsdemo"; //BUILD_APPNAME;
                const char * prjpath = "";
                for (const char * const * file = files; *file; file++) {
                    char pattern[PATH_MAX];
                    int ret;

                    if (**file == '@') {
                        prj = *file + 1;
                        ++file;
                        prjpath = *file;
                        continue ;
                    }
                    if (strcmp(prj, BUILD_APPNAME) == 0 && testsrc == 0)
                        continue ;
                    if (strcmp(prj, "vsensorsdemo") == 0 && vsensorsdemosrc == 0)
                        continue ;
                    if (strcmp(prj, "vlib") == 0 && vlibsrc == 0)
                        continue ;
                    if (strcmp(prj, "libvsensors") == 0 && sensorsrc == 0)
                        continue ;

                    snprintf(pattern, sizeof(pattern), "%s/%s", prj, *file);

                    ftruncate(fd, 0);
                    rewind(out);

                    /* run filter_source function */
                    opt_set_source_filter_bufsz(size);
                    opt_filter_source(out, pattern,
                            vsensorsdemo_get_source,
                            test_vsensorsdemo_get_source,
                            vlib_get_source,
                            libvsensors_get_source, NULL);
                    fflush(out);

                    /* build diff command */
                    ret = VLIB_SNPRINTF(ret, cmd, sizeof(cmd),
                            "{ printf '" FILE_PATTERN "%s" FILE_PATTERN_END "'; "
                            "cat '%s%s'; echo; } | ", pattern, prjpath, *file);

                    if (size >= min_buffersz && log->level >= LOG_LVL_VERBOSE) {
                        snprintf(cmd + ret, sizeof(cmd) - ret, "diff -ru \"%s\" - 1>&2",
                                 tmpfile);//FIXME log->out
                    } else {
                        snprintf(cmd + ret, sizeof(cmd) - ret,
                                "diff -qru \"%s\" - >/dev/null 2>&1", tmpfile);
                    }
                    if (size >= min_buffersz && log->level >= LOG_LVL_DEBUG)
                        fprintf(log->out, "**** FILE %s", pattern);

                    /* run diff command */
                    ret = system(cmd);

                    if (size < min_buffersz) {
                        if (ret != 0)
                            ++tmp_errors;
                        if (ret != 0 && log->level >= LOG_LVL_DEBUG) {
                            fprintf(log->out, "**** FILE %s", pattern);
                            fprintf(log->out, " [ret:%d, bufsz:%zu]\n", ret, size);
                        }
                    } else {
                        if (log->level >= LOG_LVL_DEBUG || ret != 0) {
                            if (log->level < LOG_LVL_DEBUG)
                                fprintf(log->out, "**** FILE %s", pattern);
                            fprintf(log->out, " [ret:%d, bufsz:%zu]\n", ret, size);
                        }
                        TEST_CHECK2(test, "compare logs [ret:%d bufsz:%zu file:%s]",
                                ret == 0, ret, size, pattern);
                    }
                }
                if (size < min_buffersz && tmp_errors == 0) {
                    LOG_WARN(log, "warning: no error with bufsz(%zu) < min_bufsz(%zu)",
                            size, min_buffersz);
                    //FIXME ++nerrors;
                }
            }
        }
        fclose(out);
    }
    unlink(tmpfile);
    opt_set_source_filter_bufsz(0);

#  endif /* ifndef APP_INCLUDE_SOURCE */

    return VOIDP(TEST_END(test));
}

/* *************** TEST LOG POOL *************** */
#define POOL_LOGGER_PREF        "pool-logger"
#define POOL_LOGGER_ALL_PREF    "pool-logger-all"

typedef struct {
    logpool_t *             logpool;
    char                    fileprefix[PATH_MAX];
    volatile sig_atomic_t   running;
    unsigned int            nb_iter;
} logpool_test_updater_data_t;

static void * logpool_test_logger(void * vdata) {
    logpool_test_updater_data_t * data = (logpool_test_updater_data_t *) vdata;
    logpool_t *     logpool = data->logpool;
    log_t *         log = logpool_getlog(logpool, POOL_LOGGER_PREF, LPG_TRUEPREFIX);
    log_t *         alllog = logpool_getlog(logpool, POOL_LOGGER_ALL_PREF, LPG_TRUEPREFIX);
    unsigned long   count = 0;

    while (data->running) {
        LOG_INFO(log, "loop #%lu", count);
        LOG_INFO(alllog, "loop #%lu", count);
        ++count;
    }
    return NULL;
}

static void * logpool_test_updater(void * vdata) {
    logpool_test_updater_data_t * data = (logpool_test_updater_data_t *) vdata;
    logpool_t *     logpool = data->logpool;
    log_t *         log = logpool_getlog(logpool, POOL_LOGGER_PREF, LPG_TRUEPREFIX);
    unsigned long   count = 0;
    log_t           newlog;
    char            logpath[PATH_MAX*2];

    newlog.flags = log->flags;
    newlog.level = LOG_LVL_INFO;
    newlog.out = NULL;
    newlog.prefix = strdup(log->prefix);
    while (count < data->nb_iter) {
        for (int i = 0; i < 10; ++i) {
            snprintf(logpath, sizeof(logpath), "%s-%03lu.log", data->fileprefix, count % 20);
            newlog.flags &= ~(LOG_FLAG_COLOR | LOG_FLAG_PID);
            if (count % 10 == 0)
                newlog.flags |= (LOG_FLAG_COLOR | LOG_FLAG_PID);
            logpool_add(logpool, &newlog, logpath);
        }
        usleep(20);
        ++count;
    }
    data->running = 0;
    free(newlog.prefix);
    return NULL;

}

static void * test_logpool(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test        = TEST_START(opts->testpool, "LOGPOOL");
    log_t *         log         = test != NULL ? test->log : NULL;
    log_t           log_tpl     = { log->level,
                                    LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE | LOG_FLAG_PID,
                                    log->out, NULL };
    logpool_t *     logpool     = NULL;
    log_t *         testlog;
    int             ret;

    if (test == NULL) {
        return VOIDP(1);
    }
    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(opts->logs));

    TEST_CHECK(test, "logpool_create()", (logpool = logpool_create()) != NULL);

    log_tpl.prefix = NULL; /* add a default log instance */
    TEST_CHECK(test, "logpool_add(NULL)", logpool_add(logpool, &log_tpl, NULL) != NULL);
    log_tpl.prefix = "TOTO*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_PID | LOG_FLAG_ABS_TIME;
    TEST_CHECK(test, "logpool_add(*)", logpool_add(logpool, &log_tpl, NULL) != NULL);
    log_tpl.prefix = "test/*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_TID | LOG_FLAG_ABS_TIME;
    TEST_CHECK(test, "logpool_add(tests/*)", logpool_add(logpool, &log_tpl, NULL) != NULL);

    const char * const prefixes[] = {
        "vsensorsdemo", "test", "tests", "tests/avltree", NULL
    };

    int flags[] = { LPG_NODEFAULT, LPG_NONE, LPG_TRUEPREFIX, LPG_NODEFAULT, INT_MAX };
    for (int * flag = flags; *flag != INT_MAX; flag++) {
        for (const char * const * prefix = prefixes; *prefix; prefix++) {

            /* logpool_getlog() */
            testlog = logpool_getlog(logpool, *prefix, *flag);

            /* checking logpool_getlog() result */
            if ((*flag & LPG_NODEFAULT) != 0 && flag == flags) {
                TEST_CHECK2(test, "logpool_getlog NULL with LPG_NODEFAULT prefix <%s>",
                            testlog == NULL, STR_CHECKNULL(*prefix));

            } else {
                TEST_CHECK2(test, "logpool_getlog ! NULL with flag:%d prefix <%s>",
                            testlog != NULL, *flag, STR_CHECKNULL(*prefix));
            }
            if (testlog != NULL || log->level >= LOG_LVL_INFO) {
                LOG_INFO(testlog, "CHECK LOG <%-20s> getflg:%d ptr:%p pref:%s",
                        *prefix, *flag, (void *) testlog,
                        testlog ? STR_CHECKNULL(testlog->prefix) : STR_NULL);
            }
            ret = ! ((*flag & LPG_NODEFAULT) != 0 && testlog != NULL
            &&  (   ((*prefix == NULL || testlog->prefix == NULL) && testlog->prefix != *prefix)
                 || (*prefix != NULL && testlog->prefix != NULL
                        && strcmp(testlog->prefix, *prefix) != 0) ));

            TEST_CHECK2(test, "logpool_getlog() returns required prefix with LPG_NODEFAULT"
                        " prefix <%s>", ret != 0, STR_CHECKNULL(*prefix));
        }
    }
    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(logpool));
    logpool_print(logpool, log);

    /* ****************************************************************** */
    /* test logpool on-the-fly log update */
    /* ****************************************************************** */
    logpool_test_updater_data_t data;
    pthread_t tid_l1, tid_l2, tid_l3, tid_u;
    char firstfileprefix[PATH_MAX*2];
    char check_cmd[PATH_MAX*6];
    size_t len, lensuff;

    LOG_INFO(log, "LOGPOOL: checking multiple threads logging while being updated...");
    /* init thread data */
    data.logpool = logpool;
    data.running = 1;
    if ((opts->test_mode & TEST_MASK(TEST_logpool_big)) != 0) {
        data.nb_iter = 8000;
    } else {
        data.nb_iter = 1000;
    }
    len = str0cpy(firstfileprefix, "logpool-test-XXXXXX", sizeof(firstfileprefix));
    lensuff = str0cpy(firstfileprefix + len, "-INIT.log", sizeof(firstfileprefix) - len);
    if (mkstemps(firstfileprefix, lensuff) < 0) {
        LOG_WARN(log, "mkstemps error: %s", strerror(errno));
        str0cpy(firstfileprefix, "logpool-test-INIT.log", sizeof(firstfileprefix));
    }
    strn0cpy(data.fileprefix, firstfileprefix, len, sizeof(data.fileprefix));
    /* get default log instance and update it, so that loggers first log in '...INIT' file */
    TEST_CHECK(test, "logpool_getlog(NULL)",
               (testlog = logpool_getlog(logpool, NULL, LPG_NONE)) != NULL);
    testlog->level = LOG_LVL_INFO;
    testlog->flags &= ~LOG_FLAG_PID;
    testlog->flags |= LOG_FLAG_TID;
    testlog->prefix = NULL;
    testlog->out = NULL;
    TEST_CHECK(test, "logpool_add(NULL)", logpool_add(logpool, testlog, firstfileprefix) != NULL);
    logpool_print(logpool, log);
    /* init global logpool-logger log instance */
    log_tpl.level = LOG_LVL_INFO;
    log_tpl.prefix = POOL_LOGGER_ALL_PREF;
    log_tpl.out = NULL;
    log_tpl.flags = testlog->flags;
    TEST_CHECK(test, "logpool_add(" POOL_LOGGER_PREF ")",
               logpool_add(logpool, &log_tpl, data.fileprefix) != NULL);
    /* launch threads */
    TEST_CHECK(test, "thread1", pthread_create(&tid_l1, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "thread2", pthread_create(&tid_l2, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "thread3", pthread_create(&tid_l3, NULL, logpool_test_logger, &data) == 0);
    TEST_CHECK(test, "threadU", pthread_create(&tid_u, NULL, logpool_test_updater, &data) == 0);
    if (vlib_thread_valgrind(0, NULL)) {
        while (data.running) {
            sleep(1);
        }
        sleep(2);
    } else {
        pthread_join(tid_l1, NULL);
        pthread_join(tid_l2, NULL);
        pthread_join(tid_l3, NULL);
        pthread_join(tid_u, NULL);
    }
    fflush(NULL);

    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(logpool));
    logpool_print(logpool, log);

    if ((opts->test_mode & TEST_MASK(TEST_logpool_big)) != 0) {
        LOG_INFO(log, "LOGPOOL: checking logs...");
        /* check logs versus global logpool-logger-all global log */
        snprintf(check_cmd, sizeof(check_cmd),
                 "ret=false; "
                 "sed -e 's/^[^]]*]//' '%s-'*.log | sort > '%s_concat_filtered.log'; "
                 "rm -f '%s-'*.log; "
                 "sed -e 's/^[^]]*]//' '%s' | sort "
                 "  | diff -ruq '%s_concat_filtered.log' - "
                 "    && ret=true; ",
                 data.fileprefix, data.fileprefix, data.fileprefix, data.fileprefix,
                 data.fileprefix);
        TEST_CHECK(test, "logpool thread logs comparison",
            *data.fileprefix != 0 && system(check_cmd) == 0);
    }
    snprintf(check_cmd, sizeof(check_cmd), "rm -f '%s' '%s-'*.log '%s'_*_filtered.log",
             data.fileprefix, data.fileprefix, data.fileprefix);
    system(check_cmd);

    /* free logpool and exit */
    LOG_INFO(log, "LOGPOOL: freeing...");
    logpool_free(logpool);

    return VOIDP(TEST_END(test));
}

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
        if (i != data->pr_job_counter) {
            LOG_ERROR(log, "%s(): error bad s_pr_job_counter %u, expected %u",
                    __func__, data->pr_job_counter, i);
            break ;
        }
        LOG_INFO(log, "%s(): #%d", __func__, i);
        vsleep_ms(s_PR_JOB_LOOP_SLEEPMS);
    }
    LOG_INFO(log, "%s(): finished.", __func__);
    return (void *)((unsigned long) i);
}

static void * test_job(void * vdata) {
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

#if 0 /* freeing job and let it run is NOT supported */
    LOG_INFO(log, "* run, free and wait...");
    data.pr_job_counter = 0;
    if ((job = vjob_run(pr_job, &data)) == NULL) {
        ++nerrors;
        LOG_ERROR(log, "vjob_run(): error: %s", strerror(errno));
    } else {
        vjob_free(job);
        vsleep_ms((s_PR_JOB_LOOP_NB + 1) * s_PR_JOB_LOOP_SLEEPMS);
        if (data.pr_job_counter != s_PR_JOB_LOOP_NB) {
            ++nerrors;
            LOG_ERROR(log, "error: expected job_counter = %u", s_PR_JOB_LOOP_NB);
        }
    }

    LOG_INFO(log, "* runandfree, and wait...");
    data.pr_job_counter = 0;
    if (vjob_runandfree(pr_job, &data) != 0) {
        ++nerrors;
        LOG_ERROR(log, "vjob_runandfree(): error: %s", strerror(errno));
    }
    vsleep_ms((s_PR_JOB_LOOP_NB + 1) * s_PR_JOB_LOOP_SLEEPMS);
    if (data.pr_job_counter != s_PR_JOB_LOOP_NB) {
        ++nerrors;
        LOG_ERROR(log, "error: expected job_counter = %u", s_PR_JOB_LOOP_NB);
    }

    LOG_INFO(log, "* run, free and kill (should not be possible as job freed)...");
    data.pr_job_counter = 0;
    if ((job = vjob_run(pr_job, &data)) == NULL) {
        ++nerrors;
        LOG_ERROR(log, "vjob_run(): error: %s", strerror(errno));
    } else {
        vjob_free(job);
        vsleep_ms(s_PR_JOB_LOOP_SLEEPMS);
        pthread_cancel(s_pr_job_tid);
        vsleep_ms(s_PR_JOB_LOOP_NB * s_PR_JOB_LOOP_SLEEPMS);
        if (data.pr_job_counter >= s_PR_JOB_LOOP_NB) {
            ++nerrors;
            LOG_ERROR(log, "error: expected job_counter < %u", s_PR_JOB_LOOP_NB);
        }
    }

    LOG_INFO(log, "* run, free and kill without delay (should not be possible as job freed)...");
    s_pr_job_counter = 0;
    if ((job = vjob_run(pr_job, &data)) == NULL) {
        ++nerrors;
        LOG_ERROR(log, "vjob_run(): error: %s", strerror(errno));
    } else {
        vjob_free(job);
        sched_yield();
        pthread_cancel(s_pr_job_tid);
        vsleep_ms((s_PR_JOB_LOOP_NB + 1) * s_PR_JOB_LOOP_SLEEPMS);
        if (data.pr_job_counter >= s_PR_JOB_LOOP_NB) {
            ++nerrors;
            LOG_ERROR(log, "error: expected job_counter < %u", s_PR_JOB_LOOP_NB);
        }
    }
#endif

    return VOIDP(TEST_END(test));
}

/* ************************************************************************ */
/* TEST TESTS */
/* ************************************************************************ */
static void * test_tests(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    /* except this 'fake' global test, tests here are done manually (no bootstrap) */
    testgroup_t *   globaltest  = TEST_START(opts->testpool, "TEST");
    log_t *         log         = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned long   nerrors     = 0;
    unsigned long   n_tests     = 0;
    unsigned long   n_ok        = 0;
    unsigned long   testend_ret;

    /* Hi, this is the only test where checks must be done manually
     * as it is checking the vlib testpool framework.
     * In all other tests, the vlib testpool framework should be used. */

    LOG_INFO(log, ">>> TEST tests (manual)");

    /* create test testpool */
    testpool_t *    tests = tests_create(opts->logs, TPF_DEFAULT);
    testgroup_t *   test;
    unsigned int    old_flags = INT_MAX;
    log_level_t     old_level = INT_MAX;
    unsigned int    expected_err;

    /*** TESTS01 ***/
    test = TEST_START(tests, "TEST01");
    expected_err = 1;

    if (test == NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR: TEST_START(TEST01) returned NULL");
    } else {
        ++n_ok;
        old_flags = test->log->flags;
        old_level = test->log->level;
        test->log->flags &= ~LOG_FLAG_COLOR;
        if (log->level < LOG_LVL_INFO) test->log->level = 0;
    }
    ++n_tests;

    TEST_CHECK(test, "CHECK01", 1 == 1);
    TEST_CHECK(test, "CHECK02", (errno = EAGAIN) && 1 == 0);

    testend_ret = TEST_END2(test, "%s", test && test->n_errors == expected_err
                                         ? " AS EXPECTED" : "");
    if (test != NULL) { test->log->flags = old_flags; test->log->level = old_level; }

    if (test != NULL
    && (test->n_errors != expected_err || test->n_tests != 2 || test->n_errors != testend_ret
        || test->n_ok != (test->n_tests - test->n_errors)
        || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST01 test_end/n_tests/n_errors/n_ok counters mismatch");
    } else ++n_ok;
    ++n_tests;

    if (test != NULL) {
        n_ok += test->n_ok + test->n_ok; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    /*** TESTS02 ***/
    test = TEST_START(tests, "TEST02");
    expected_err = 2;

    if (test == NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR: TEST_START(TEST02) returned NULL");
    } else {
        ++n_ok;
        old_flags = test->log->flags;
        old_level = test->log->level;
        test->log->flags &= ~LOG_FLAG_COLOR;
        test->flags |= TPF_STORE_RESULTS | TPF_BENCH_RESULTS;
        if (log->level < LOG_LVL_INFO) test->log->level = 0;
    }
    ++n_tests;

    TEST_CHECK(test, "CHECK01", 1 == 1);
    TEST_CHECK(test, "CHECK02", 1 == 0);
    if (test != NULL) test->ok_loglevel = LOG_LVL_INFO;
    TEST_CHECK2(test, "CHECK03 checking %s", 1 == 0, "something");
    TEST_CHECK2(test, "CHECK04 checking %s", (errno = ENOMEM) && 1 == 1 + 2*7 - 14,
                      "something else");
    TEST_CHECK(test, "CHECK05 usleep", usleep(12345) == 0);
    TEST_CHECK(test, "CHECK06", 2 == 2);
    testend_ret = TEST_END2(test, "%s", test && test->n_errors == expected_err
                                         ? " AS EXPECTED" : "");
    if (test != NULL) { test->log->flags = old_flags; test->log->level = old_level; }

    if (test != NULL
    &&  (test->n_errors != expected_err || test->n_tests != 6 || test->n_errors != testend_ret
         || test->n_ok != (test->n_tests - test->n_errors)
         || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST02 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;
    ++n_tests;
    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    expected_err = 0;
    test = TEST_START(tests, "TEST03");

    ++n_tests;
    if (test != NULL
    &&  (test->n_errors != 0 || test->n_tests != 0
         || test->n_ok != (test->n_tests - test->n_errors)
         || (test->flags & TPF_FINISHED) != 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST03 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;

    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
        LOG_INFO(test->log, NULL);
        /* release logpool log to have clean counters (because TEST_END() not called) */
        ++n_tests;
        if (logpool_release(opts->logs, test->log) < 0
        && (errno != EACCES || (test->flags & TPF_LOGTRUEPREFIX) != 0)) {
            ++nerrors;
            LOG_ERROR(log, "error: logpool_release(%s): <0: %s flg:%x",
                      test->name, strerror(errno),test->flags);
        } else ++n_ok;
    }

    expected_err = 0;
    test = TEST_START(tests, "TEST04");
    testend_ret = TEST_END(test);

    if (test != NULL
    &&  (test->n_errors != 0 || test->n_tests != 0
         || test->n_ok != (test->n_tests - test->n_errors) || testend_ret != test->n_errors
         || (test->flags & TPF_FINISHED) == 0)) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST04 test_end/n_tests/n_errors/n_ok/finished counters mismatch");
    } else ++n_ok;
    ++n_tests;
    if (test != NULL) {
        n_ok += test->n_ok + test->n_errors; /* ERRORS recorded in test are expected */
        n_tests += test->n_tests;
    }

    ++n_tests;
    log_t newlog;
    log_t * oldlog = log_set_vlib_instance(&newlog);
    if (oldlog != NULL) {
        newlog = *oldlog;
        newlog.flags &= ~(LOG_FLAG_COLOR);
        if (log->level < LOG_LVL_INFO) newlog.level = 0;
        else newlog.level = LOG_LVL_INFO;
    }
    ++n_tests;
    if (TEST_END(test) != testend_ret) {
        ++nerrors;
    } else ++n_ok;

    test = NULL;
    if (TEST_START((testpool_t*)(NULL), "TEST05") != NULL
    ||  TEST_START((testpool_t*)(test), "TEST05") != NULL) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_START(NULL) did not return NULL");
    } else ++n_ok;
    ++n_tests;
    if (TEST_END(test) == 0) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_END(NULL) returned 0");
    } else ++n_ok;
    TEST_CHECK(test, "CHECK01: TEST_CHECK with NULL test", (expected_err = 98) && (1 == 1));
    ++n_tests;
    if (expected_err != 98) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK(test, "CHECK02: TEST_CHECK with NULL test", (++expected_err) && 1 == 2);
    ++n_tests;
    if (expected_err != 99) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK2(test, "CHECK03: TEST_CHECK2 with NULL %s", (++expected_err) && 1 == 1, "test");
    ++n_tests;
    if (expected_err != 100) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    TEST_CHECK2(test, "CHECK04: TEST_CHECK2 with NULL T%s", (++expected_err) && 1 == 2, "est");
    ++n_tests;
    if (expected_err != 101) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, TEST_CHECK with NULL test not executed");
    } else ++n_ok;
    LOG_INFO(log, NULL);
    log_set_vlib_instance(oldlog);

    tests_print(tests, TPR_DEFAULT);

    if (tests_free(tests) != 0) {
        ++nerrors;
        LOG_ERROR(log, "ERROR, test_free() error");
    } else ++n_ok;
    ++n_tests;

    LOG_INFO(log, "<- %s(): ending with %lu error%s (manual).\n",
             __func__, nerrors, nerrors > 1 ? "s" : "");

    if (globaltest != NULL) {
        globaltest->n_errors += nerrors;
        globaltest->n_tests += n_tests;
        globaltest->n_ok += n_ok;
    }
    logpool_release(opts->logs, log); /* necessary because logpool_getlog() called here */
    return VOIDP(TEST_END(globaltest) + (globaltest == NULL ? nerrors : 0));
}


#ifdef TEST_EXPERIMENTAL_PARALLEL
/** for parallel tests */
typedef struct {
    vjob_t *        job;
    unsigned int    testidx;
} testjob_t;

unsigned long check_test_jobs(options_test_t * opts, log_t * log, shlist_t * jobs,
                              unsigned int * nb_jobs, unsigned long * current_tests) {
    unsigned long nerrors = 0;
    testjob_t * tjob;
    unsigned int nb_jobs_orig = *nb_jobs;
    (void) opts;

    if (jobs->head == NULL) {
        LOG_VERBOSE(log, "%s(): No job running", __func__);
        return 0;
    }

    tjob = (testjob_t *) jobs->head->data;

    LOG_VERBOSE(log, "WAITING for 1 termination (%u running)", *nb_jobs);

    while (nb_jobs_orig == *nb_jobs) {
        for (slist_t * list = jobs->head, * prev = NULL; list != NULL; /*no_incr*/) {
            tjob = (testjob_t *) (list->data);
            vjob_t * vjob = tjob->job;
            if (vjob_done(vjob)) {
                slist_t *   to_free = list;
                void *      result = vjob_free(vjob);

                if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT) {
                    LOG_ERROR(log, "error: cannot get job result for '%s'",
                            s_testconfig[tjob->testidx].name);
                    ++nerrors;
                } else {
                    LOG_VERBOSE(log, "TEST JOB '%s' finished with result %ld",
                                s_testconfig[tjob->testidx].name, (long) result);
                    nerrors += (long) result;
                }
                --(*nb_jobs);
                *current_tests &= ~TEST_MASK(tjob->testidx);
                if (list == jobs->tail) {
                    jobs->tail = prev;
                }
                list = list->next;
                if (prev == NULL) {
                    jobs->head = list;
                } else {
                    prev->next = list;
                }
                slist_free_1(to_free, free);
            } else {
                prev = list;
                list = list->next;
            }
        }
        sleep(1);
    }
    return nerrors;
}
#endif

/* *************** TEST MAIN FUNC *************** */

int test(int argc, const char *const* argv, unsigned int test_mode, logpool_t ** logpool) {
    options_test_t  options_test    = { .flags = 0, .test_mode = test_mode, .main=pthread_self(),
                                        .testpool = NULL, .logs = logpool ? *logpool : NULL};

    log_t *         log             = logpool_getlog(options_test.logs,
                                                     TESTPOOL_LOG_PREFIX, LPG_TRUEPREFIX);
    unsigned int    errors = 0;
    char const **   test_argv = NULL;
    shlist_t        jobs = SHLIST_INITIALIZER();
    sigset_t        sigset_bak;
#ifdef TEST_EXPERIMENTAL_PARALLEL
    unsigned int    nb_jobs = 0;
    unsigned long   current_tests = 0;
#endif
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, VERSION_STRING,
                                                      &options_test);

    LOG_INFO(log, NULL);
    LOG_INFO(log, ">>> TEST MODE: 0x%x, %u CPUs\n", test_mode, vjob_cpu_nb());

    if ((test_argv = malloc(sizeof(*test_argv) * argc)) != NULL) {
        test_argv[0] = BUILD_APPNAME;
        for (int i = 1; i < argc; ++i) {
            test_argv[i] = argv[i];
        }
        argv = test_argv;
    }
    options_test.argc = argc;
    options_test.argv = argv;
    opt_config_test.flags |= OPT_FLAG_SILENT;
    opt_parse_options(&opt_config_test);
    if (options_test.test_mode == 0) {
        logpool_release(options_test.logs, log);
        if (logpool)
            *logpool = options_test.logs;
        if (test_argv != NULL)
            free(test_argv);
        return OPT_ERROR(OPT_EOPTARG);
    }
    test_mode |= options_test.test_mode;
    options_test.out = log->out;

    /* get term columns */
    if ((options_test.columns
                = vterm_get_columns(log && log->out ? fileno(log->out) : STDERR_FILENO)) <= 0) {
        options_test.columns = 80;
    }

    /* create testpool */
    options_test.testpool = tests_create(options_test.logs,
                                         TPF_STORE_ERRORS | TPF_TESTOK_SCREAM | TPF_LOGTRUEPREFIX);

    /* Block SIGALRM by default to not disturb at least test_bench() */
    if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0) {
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGALRM);
        if (sigprocmask(SIG_SETMASK, &sigset, &sigset_bak) != 0) {
            ++errors;
            LOG_ERROR(log, "cannot block SIGALRM: %s", strerror(errno));
        }
    }

    /* Run all requested tests */
    for (unsigned int testidx = 0; testidx < PTR_COUNT(s_testconfig); ++testidx ) {
        if (s_testconfig[testidx].name == NULL || s_testconfig[testidx].fun == NULL) {
            continue ;
        }
        if ((test_mode & TEST_MASK(testidx)) != 0) {
#ifdef TEST_EXPERIMENTAL_PARALLEL // TODO
            if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0) {
                /* ********************************************************
                 * MULTI THREADED TESTS
                 *********************************************************/
                testjob_t * tjob;

                do {
                    int waitneeded = 0;
                    SLIST_FOREACH_DATA(jobs.head, it_tjob, testjob_t *) {
                        if ((s_testconfig[it_tjob->testidx].mask & TEST_MASK(testidx)) != 0) {
                            waitneeded = 1;
                            break ;
                        }
                    }
                    if (waitneeded == 0 && (current_tests & s_testconfig[testidx].mask) == 0) {
                        break ;
                    }
                    errors += check_test_jobs(&options_test, log, &jobs, &nb_jobs, &current_tests);
                } while (jobs.head != NULL);

                LOG_VERBOSE(log, "RUNNING test job '%s'...", s_testconfig[testidx].name);
                if ((tjob = malloc(sizeof(*tjob))) == NULL
                || (tjob->job = vjob_run(s_testconfig[testidx].fun, &options_test)) == NULL) {
                    if (tjob != NULL)
                        free(tjob);
                    LOG_ERROR(log, "error: cannot run job for test '%s'", s_testconfig[testidx].name);
                } else {
                    tjob->testidx = testidx;
                    current_tests |= TEST_MASK(testidx);
                    ++nb_jobs;
                    if ((jobs.head = slist_appendto(jobs.head, tjob, &jobs.tail)) != NULL
                    &&  nb_jobs > vjob_cpu_nb()) {
                        errors += check_test_jobs(&options_test, log, &jobs, &nb_jobs, &current_tests);
                    }
                }
            } else
#endif
            {
                /* ********************************************************
                 * SINGLE THREADED TESTS
                 *********************************************************/
                LOG_VERBOSE(log, "running test '%s'...", s_testconfig[testidx].name);
                errors += (long) (s_testconfig[testidx].fun(&options_test));
            }
        }
    }
    SLIST_FOREACH_DATA(jobs.head, tjob, testjob_t *) {
        void * result = vjob_waitandfree(tjob->job);
        if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT) {
            LOG_ERROR(log, "error: cannot get test job result");
            ++errors;
        } else {
            errors += (long) result;
        }
    }
    slist_free(jobs.head, free);

    if ((test_mode & TEST_MASK(TEST_PARALLEL)) != 0
    &&  sigprocmask(SIG_SETMASK, &sigset_bak, NULL) != 0) {
        ++errors;
        LOG_ERROR(log, "cannot block SIGALRM: %s", strerror(errno));
    }

    /* test Tree */
    /*if ((test_mode & (TEST_MASK(TEST_tree) | TEST_MASK(TEST_bigtree) | TEST_MASK(TEST_tree_MT))) != 0) {
        vjob_t * job = NULL;
        if ((test_mode & TEST_MASK(TEST_tree_MT)) != 0) {
            job = vjob_run(test_avltree, &options_test);
        }
        errors += (long) test_avltree(&options_test);

        if (job != NULL) {
            void * result = vjob_waitandfree(job);
            if (result == VJOB_ERR_RESULT || result == VJOB_NO_RESULT)
                ++errors;
            else
                errors += (long) result;
        }
    }*/

    /* print tests results and free testpool */
    tests_print(options_test.testpool, TPR_PRINT_ERRORS | TPR_PRINT_OK);
    tests_print(options_test.testpool, TPR_PRINT_GROUPS);
    tests_free(options_test.testpool);

    /* ***************************************************************** */
    LOG_INFO(log, "<<< END of Tests : %u error(s).\n", errors);

    logpool_release(options_test.logs, log);

    if (test_argv != NULL) {
        free(test_argv);
    }

    /* just update logpool, don't free it: it will be done by caller */
    if (logpool) {
        *logpool = options_test.logs;
    }

    return -errors;
}

#ifndef APP_INCLUDE_SOURCE
static const char * s_app_no_source_string
    = "\n/* #@@# FILE #@@# " BUILD_APPNAME "/* */\n" \
             BUILD_APPNAME " source not included in this build.\n";

int test_vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {
        return vdecode_buffer(out, buffer, buffer_size, ctx,
                              s_app_no_source_string, strlen(s_app_no_source_string));
}
#endif

#endif /* ! ifdef _TEST */

