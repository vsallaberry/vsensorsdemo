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
 * tests for vsensorsdemo, libvsensors, vlib.
 * The testing part was previously in main.c. To see previous history of
 * vsensorsdemo tests, look at main.c history (git log -r eb571ec4a src/main.c).
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
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <pwd.h>
#include <grp.h>

#include "vlib/hash.h"
#include "vlib/util.h"
#include "vlib/options.h"
#include "vlib/time.h"
#include "vlib/account.h"
#include "vlib/thread.h"
#include "vlib/rbuf.h"
#include "vlib/avltree.h"

#include "libvsensors/sensor.h"

#include "version.h"

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS("TEST-" BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2018")

/** strings corresponding of unit tests ids , for command line, in same order as g_testmode_str */
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
    TEST_vthread,
    TEST_list,
    TEST_tree,
    TEST_rbuf,
};
const char * const g_testmode_str[] = {
    "all", "sizeof", "options", "ascii", "bench", "hash", "sensorvalue", "log", "account",
    "vthread", "list", "tree", "rbuf", NULL
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
    { 'h', "help",      "[filter1[,...]]",     "show usage. " },
    { 'h', "show-help", NULL,           NULL },
    { 'l', "log-level", "level",        "set log level [module1=]level1[@file1][,...]\n"
                                        "(1..6 for ERR,WRN,INF,VER,DBG,SCR)." },
    { 'T', "test",      "[test_mode]",  "test mode. Default: 1." },
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
    { 'T', NULL,        NULL,           NULL },
    { 'U', NULL,        NULL,           NULL },
    { 'V', NULL,        NULL,           NULL },
    { 'W', NULL,        NULL,           NULL },
    { 'X', NULL,        NULL,           NULL },
    { 'Y', NULL,        NULL,           NULL },
    { 'Z', NULL,        NULL,           NULL },
    { OPT_ID_SECTION+5, NULL, "arguments", "\nArguments:" },
    { OPT_ID_ARG+1, NULL, "[program [arguments]]", "program and its arguments" },
    { OPT_ID_ARG+2, NULL, "[second_program [arguments]]", "program and its arguments - " },
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
    (void) options;
    (void) i_argv;
    if ((opt & OPT_DESCRIBE_OPTION) != 0) {
        switch (opt & OPT_OPTION_FLAG_MASK) {
            case 'h':
                return opt_describe_filter(opt, arg, i_argv, opt_config);
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
    case 'h':
        return opt_usage(OPT_EXIT_OK(0), opt_config, arg);
    case 'l':
        if (arg != NULL && *arg != '-') {
            /* nothing */
        }
        break ;
    case 'T':
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

unsigned int test_getmode(const char *arg) {
    char * endptr = NULL;
    const unsigned int test_mode_all = 0xffffffffU;
    unsigned int test_mode = test_mode_all;
    if (arg != NULL) {
        errno = 0;
        test_mode = strtol(arg, &endptr, 0);
        if (errno != 0 || endptr == NULL || *endptr != 0) {
            const char * token, * next = arg;
            size_t len, i;
            while ((len = strtok_ro_r(&token, ",", &next, NULL, 0)) > 0 || *next) {
                for (i = 0; g_testmode_str[i]; i++) {
                    if (!strncasecmp(g_testmode_str[i], token, len) && g_testmode_str[i][len] == 0) {
                        test_mode |= (i == TEST_all ? test_mode_all : (1U << i));
                        break ;
                    }
                }
                if (g_testmode_str[i] == NULL) {
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
    PSIZEOF(struct timeval);
    PSIZEOF(struct timespec);
    PSIZEOF(log_t);
    PSIZEOF(avltree_t);
    PSIZEOF(avltree_node_t);
    PSIZEOF(sensor_watch_t);
    PSIZEOF(sensor_value_t);
    PSIZEOF(sensor_sample_t);
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
    opt_config_t    opt_config_test = { argc, argv, parse_option_test, s_opt_desc_test,
                                        OPT_FLAG_DEFAULT, VERSION_STRING, opts, NULL };
    int             result;
    int             nerrors = 0;

    result = opt_parse_options(&opt_config_test);
    LOG_INFO(NULL, ">>> opt_parse_options() result: %d", result);
    if (result <= 0) {
        LOG_ERROR(NULL, "ERROR opt_parse_options() expected >0, got %d", result);
        ++nerrors;
    }

    opt_config_test.flags |= OPT_FLAG_MAINSECTION;
    result = opt_parse_options(&opt_config_test);
    LOG_INFO(NULL, ">>> opt_parse_options(shortusage) result: %d", result);
    if (result <= 0) {
        LOG_ERROR(NULL, "ERROR opt_parse_options(shortusage) expected >0, got %d", result);
        ++nerrors;
    }

    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** TEST LIST *************** */

static int intcmp(const void * a, const void *b) {
    return (long)a - (long)b;
}

static int test_list(const options_test_t * opts) {
    int             nerrors = 0;
    (void)          opts;
    slist_t *       list = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 7, 4, 1 };
    const size_t    intssz = sizeof(ints)/sizeof(*ints);
    long            prev;

    LOG_INFO(NULL, ">>> LIST tests");

    /* insert_sorted */
    for (size_t i = 0; i < intssz; i++) {
        if ((list = slist_insert_sorted(list, (void*)((long)ints[i]), intcmp)) == NULL) {
            LOG_ERROR(NULL, "slist_insert_sorted(%d) returned NULL", ints[i]);
            nerrors++;
        }
        fprintf(stderr, "%02d> ", ints[i]);
        SLIST_FOREACH_DATA(list, a, long) fprintf(stderr, "%ld ", a);
        fputc('\n', stderr);
    }
    /* prepend, append, remove */
    list = slist_prepend(list, (void*)((long)-2));
    list = slist_prepend(list, (void*)((long)0));
    list = slist_prepend(list, (void*)((long)-1));
    list = slist_append(list, (void*)((long)-4));
    list = slist_append(list, (void*)((long)20));
    list = slist_append(list, (void*)((long)15));
    fprintf(stderr, "after prepend/append:");
    SLIST_FOREACH_DATA(list, a, long) fprintf(stderr, "%ld ", a);
    fputc('\n', stderr);
    /* we have -1, 0, -2, ..., 20, -4, 15, we will remove -1(head), then -2(in the middle),
     * then 15 (tail), then -4(in the middle), and the list should still be sorted */
    list = slist_remove(list, (void*)((long)-1), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-2), intcmp, NULL);
    list = slist_remove(list, (void*)((long)15), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-4), intcmp, NULL);
    fprintf(stderr, "after remove:");
    SLIST_FOREACH_DATA(list, a, long) fprintf(stderr, "%ld ", a);
    fputc('\n', stderr);

    prev = 0;
    SLIST_FOREACH_DATA(list, a, long) {
        if (a < prev) {
            LOG_ERROR(NULL, "list elt <%ld> has wrong position regarding <%ld>", a, prev);
            nerrors++;
        }
        prev = a;
    }
    slist_free(list, NULL);

    if (prev != 20) {
        LOG_ERROR(NULL, "list elt <%ld> should be last insteand of <%d>", 20, prev);
        nerrors++;
    }

    LOG_INFO(NULL, NULL);
    return nerrors;
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

/* *************** TEST STACK *************** */
enum {
    SPT_NONE        = 0,
    SPT_PRINT       = 1 << 0,
    SPT_REPUSH      = 1 << 1,
};
static int rbuf_pop_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags) {
    unsigned int nerrors = 0;
    size_t i, j;

    for (i = 0; i < intssz; i++) {
        if (rbuf_push(rbuf, (void*)((long)ints[i])) != 0) {
            LOG_ERROR(NULL, "error rbuf_push(%d)", ints[i]);
            nerrors++;
        }
    }
    if ((i = rbuf_size(rbuf)) != intssz) {
        LOG_ERROR(NULL, "error rbuf_size %lu should be %lu", i, intssz);
        nerrors++;
    }
    i = intssz - 1;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long t = (long) rbuf_top(rbuf);
        long p = (long) rbuf_pop(rbuf);

        if (t != p || p != ints[i]) {
            if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
            LOG_ERROR(NULL, "error rbuf_top(%ld) != rbuf_pop(%ld) != %d", t, p, ints[i]);
            nerrors++;
        }
        if (i == 0) {
            if (j == 1) {
                i = intssz / 2 - intssz % 2;
                j = 2;
            }else if ( j == 2 ) {
                if (rbuf_size(rbuf) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
                    LOG_ERROR(NULL, "rbuf is too large");
                    nerrors++;
                }
            }
        } else {
            --i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(stderr, "%ld ", t);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                if (rbuf_push(rbuf, (void*)((long)ints[j])) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
                    LOG_ERROR(NULL, "error rbuf_push(%d)", ints[j]);
                    nerrors++;
                }
            }
            i = intssz - 1;
            j = 1;
        }

    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', stderr);

    return nerrors;
}
static int rbuf_dequeue_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags) {
    unsigned int nerrors = 0;
    size_t i, j;

    for (i = 0; i < intssz; i++) {
        if (rbuf_push(rbuf, (void*)((long)ints[i])) != 0) {
            LOG_ERROR(NULL, "error rbuf_push(%d)", ints[i]);
            nerrors++;
        }
    }
    if ((i = rbuf_size(rbuf)) != intssz) {
        LOG_ERROR(NULL, "error rbuf_size %lu should be %lu", i, intssz);
        nerrors++;
    }
    i = 0;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long b = (long) rbuf_bottom(rbuf);
        long d = (long) rbuf_dequeue(rbuf);
        if (b != d || b != ints[i]) {
            if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
            LOG_ERROR(NULL, "error rbuf_bottom(%ld) != rbuf_dequeue(%ld) != %d", b, d, ints[i]);
            nerrors++;
        }
        if (i == intssz - 1) {
            if (rbuf_size(rbuf) != ((j == 1?1:0) * intssz)) {
                if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
                LOG_ERROR(NULL, "rbuf is too large");
                nerrors++;
            }
            j = 2;
            i = 0;
        } else {
            ++i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(stderr, "%ld ", b);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                if (rbuf_push(rbuf, (void*)((long)ints[j])) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', stderr);
                    LOG_ERROR(NULL, "error rbuf_push(%d)", ints[j]);
                    nerrors++;
                }
            }
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', stderr);
    if (i != 0) {
        LOG_ERROR(NULL, "error: missing values in dequeue");
        nerrors++;
    }
    return nerrors;
}
static int test_rbuf(options_test_t * opts) {
    rbuf_t *  rbuf;
    unsigned int    nerrors = 0;
    const int       ints[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    size_t          i, iter;
    (void) opts;
    //log_t * vlogsave = log_set_vlib_instance(NULL);

    LOG_INFO(NULL, ">>> STACK TESTS");

    /* rbuf_pop 31 elts */
    LOG_INFO(NULL, "* rbuf_pop tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    if (rbuf == NULL) {
        LOG_ERROR(NULL, "ERROR rbuf_create null");
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_pop_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE);
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_pop_test(rbuf, ints, intssz, iter == 0 ?
                                                     SPT_PRINT | SPT_REPUSH : SPT_REPUSH);
    }
    rbuf_free(rbuf);

    /* rbuf_dequeue 31 elts */
    LOG_INFO(NULL, "* rbuf_dequeue tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    if (rbuf == NULL) {
        LOG_ERROR(NULL, "ERROR rbuf_create null");
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE);
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ?
                                                         SPT_PRINT | SPT_REPUSH : SPT_REPUSH);
    }

    /* rbuf_dequeue 4001 elts */
    int tab[4001];
    size_t tabsz = sizeof(tab) / sizeof(tab[0]);
    LOG_INFO(NULL, "* rbuf_dequeue tests2 tab[%lu]", tabsz);
    for (i = 0; i < tabsz; i++) {
        tab[i] = i;
    }
    for (iter = 0; iter < 1000; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, tab, tabsz, SPT_NONE);
    }
    for (iter = 0; iter < 1000; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, tab, tabsz, SPT_REPUSH);
    }
    rbuf_free(rbuf);

    /* rbuf RBF_OVERWRITE, rbuf_get, rbuf_set */
    LOG_INFO(NULL, "* rbuf OVERWRITE tests");
    rbuf = rbuf_create(5, RBF_DEFAULT | RBF_OVERWRITE);
    if (rbuf == NULL) {
        LOG_ERROR(NULL, "ERROR rbuf_create Overwrite null");
        nerrors++;
    }
    for (i = 0; i < 3; i++) {
        if (rbuf_push(rbuf, (void *) ((i % 5) + 1)) != 0) {
            LOG_ERROR(NULL, "error rbuf_push(%lu) overwrite mode", (i % 5) + 1);
            nerrors++;
        }
    }
    i = 1;
    while(rbuf_size(rbuf) != 0) {
        size_t b = (size_t) rbuf_bottom(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, 0);
        size_t d = (size_t) rbuf_dequeue(rbuf);
        if (b != i || g != i || d != i) {
            LOG_ERROR(NULL, "error rbuf_get(%ld) != rbuf_top != rbuf_dequeue != %lu", i - 1, i);
            nerrors++;
        }
        fprintf(stderr, "%lu ", i);
        i++;
    }
    fputc('\n', stderr);
    if (i != 4) {
        LOG_ERROR(NULL, "error rbuf_size 0 but not 3 elts (%lu)", i-1);
        nerrors++;
    }

    for (i = 0; i < 25; i++) {
        fprintf(stderr, "#%lu(%lu) ", i, (i % 5) + 1);
        if (rbuf_push(rbuf, (void *)((size_t) ((i % 5) + 1))) != 0) {
            fputc('\n', stderr);
            LOG_ERROR(NULL, "error rbuf_push(%lu) overwrite mode", (i % 5) + 1);
            nerrors++;
        }
    }
    fputc('\n', stderr);
    if ((i = rbuf_size(rbuf)) != 5) {
        LOG_ERROR(NULL, "error rbuf_size (%lu) should be 5", i);
        nerrors++;
    }
    if (rbuf_set(rbuf, 7, (void*)7L) != -1) {
        LOG_ERROR(NULL, "error rbuf_set(7) should be rejected");
        nerrors++;
    }
    i = 5;
    while (rbuf_size(rbuf) != 0) {
        size_t t = (size_t) rbuf_top(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, i-1);
        size_t p = (size_t) rbuf_pop(rbuf);
        if (t != i || g != i || p != i) {
            fputc('\n', stderr);
            LOG_ERROR(NULL, "error rbuf_get(%lu) != rbuf_top != rbuf_pop != %lu", i - 1, i);
            nerrors++;
        }
        i--;
        fprintf(stderr, "%lu ", p);
    }
    fputc('\n', stderr);
    if (i != 0) {
        LOG_ERROR(NULL, "error rbuf_size 0 but not 5 elts (%lu)", 5-i);
        nerrors++;
    }
    rbuf_free(rbuf);

    /* rbuf_set with RBF_OVERWRITE OFF */
    LOG_INFO(NULL, "* rbuf_set tests with increasing buffer");
    rbuf = rbuf_create(1, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    if (rbuf == NULL) {
        LOG_ERROR(NULL, "ERROR rbuf_create Overwrite null");
        nerrors++;
    }
    if ((i = rbuf_size(rbuf)) != 0) {
        LOG_ERROR(NULL, "ERROR rbuf_size(%lu) should be 0", i);
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 1, (void*) 1) != 0) {
            LOG_ERROR(NULL, "ERROR rbuf_set(1)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 2) {
            LOG_ERROR(NULL, "ERROR rbuf_size(%lu) should be 2", i);
            nerrors++;
        }
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 3, (void*) 3) != 0) {
            LOG_ERROR(NULL, "ERROR rbuf_set(3)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 4) {
            LOG_ERROR(NULL, "ERROR rbuf_size(%lu) should be 4", i);
            nerrors++;
        }
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 5000, (void*) 5000) != 0) {
            LOG_ERROR(NULL, "ERROR rbuf_set(5000)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 5001) {
            LOG_ERROR(NULL, "ERROR rbuf_size(%lu) should be 5001", i);
            nerrors++;
        }
    }
    if (rbuf_reset(rbuf) != 0) {
        LOG_ERROR(NULL, "error: rbuf_reset");
        nerrors++;
    }
    for (iter = 1; iter <= 5000; iter++) {
        if (rbuf_set(rbuf, iter-1, (void*) iter) != 0) {
            LOG_ERROR(NULL, "ERROR rbuf_set(%lu)", iter);
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != iter) {
            LOG_ERROR(NULL, "ERROR rbuf_size(%lu) should be %lu", i, iter);
            nerrors++;
        }
    }
    for (iter = 0; rbuf_size(rbuf) != 0; iter++) {
        size_t g = (size_t) rbuf_get(rbuf, 5000-iter-1);
        size_t t = (size_t) rbuf_top(rbuf);
        size_t p = (size_t) rbuf_pop(rbuf);
        i = rbuf_size(rbuf);
        if (t != p | g != p || p != 5000-iter || i != 5000-iter-1) {
            LOG_ERROR(NULL, "error top(%lu) != get(#%lu=%lu) != pop(%lu) != %lu "
                            "or rbuf_size(%lu) != %lu",
                      t, 5000-iter-1, g, p, 5000-iter, i, 5000-iter-1);
            nerrors++;
        }
    }
    if (iter != 5000) {
        LOG_ERROR(NULL, "error not 5000 elements (%lu)", iter);
        nerrors++;
    }
    rbuf_free(rbuf);

    //log_set_vlib_instance(vlogsave);
    LOG_INFO(NULL, NULL);
    return nerrors;
}

/* *************** TEST AVL TREE *************** */
#include <math.h>

static void avlprint_larg(avltree_t * tree, FILE * out) {
    rbuf_t *    fifo        = rbuf_create(32 * 3, RBF_DEFAULT);
    int         width       = 200;
    int         node_nb     = 1;
    int         node_sz     = width / 3;
    int         display     = (node_nb - 1) * node_sz;
    int         indent      = (width - display) / 2;
    ssize_t     n;
    ssize_t     old_idx     = -1;

    if (tree && tree->root) {
        rbuf_push(fifo, 0);
        rbuf_push(fifo, 0);
        rbuf_push(fifo, tree->root);
        for (n = 0; n < indent; n++)
            fputc(' ', out);
    }

    while (rbuf_size(fifo)) {
        size_t              level   = (size_t) rbuf_dequeue(fifo);
        ssize_t             idx     = (ssize_t) rbuf_dequeue(fifo);
        avltree_node_t *    node    = (avltree_node_t *) rbuf_dequeue(fifo);

        /* padding for inexistant previous brothers */
        if (old_idx + 1 < idx) {
            for(n = 0; n < node_sz * (idx - old_idx - 1); n++)
                fputc(' ', out);
        }

        /* print node & padding */
        n = fprintf(out, "%ld(%d)", (long)node->data, node->balance);
        if (rbuf_size(fifo) != 0 && (size_t) rbuf_bottom(fifo) == level) {
            for( ; n < node_sz; n++)
                fputc(' ', out);
        }
        /* detect if this is the last element of the level */
        if (rbuf_size(fifo) == 0 || level != (size_t) rbuf_bottom(fifo)) {
            /* next element is on new level */
            old_idx = -1;
            fputc('\n', out);
            node_nb = pow(2, level + 1);
            node_sz = width / (node_nb  );
            display = (node_nb-1) * node_sz;
            indent = (width - display) / 2;
            for (n = 0; n < indent; n++)
                    fputc(' ', out);
            for (int i = 0; i < pow(2, level); i++) {
                for (n = 0; n < node_sz; n++)
                    fputc(n == node_sz / 2 ? '+' : '_', out);
                if (i + 1!= pow(2, level))
                for (n = 0; n < node_sz; n++)
                    fputc(' ', out);
            }
            fputc('\n', out);
            for (n = 0; n < indent; n++)
                fputc(' ', out);
        } else {
            old_idx = idx;
        }

        if (node->left) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2)));
            rbuf_push(fifo, node->left);
        }
        if (node->right) {
            rbuf_push(fifo, (void*)((long)(level + 1)));
            rbuf_push(fifo, (void*)((ssize_t)(idx * 2 + 1)));
            rbuf_push(fifo, node->right);
        }
    }
    fputc('\n', out);
    rbuf_free(fifo);
}
static void avlprint_inf_left(avltree_node_t * node) {
    if (!node) return;
    if (node->left)
        avlprint_inf_left(node->left);
    fprintf(stderr, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    if (node->right)
        avlprint_inf_left(node->right);
}
static void avlprint_inf_right(avltree_node_t * node) {
    if (!node) return;
    if (node->right)
        avlprint_inf_right(node->right);
    fprintf(stderr, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    if (node->left)
        avlprint_inf_right(node->left);
}
static void avlprint_pref_left(avltree_node_t * node) {
    if (!node) return;
    fprintf(stderr, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
    if (node->left)
        avlprint_pref_left(node->left);
    if (node->right)
        avlprint_pref_left(node->right);
}
static void avlprint_suff_left(avltree_node_t * node) {
    if (!node) return;
   if (node->left)
        avlprint_suff_left(node->left);
    if (node->right)
        avlprint_suff_left(node->right);
    fprintf(stderr, "%ld(%ld,%ld) ", (long)node->data,
            node->left?(long)node->left->data:-1,node->right?(long)node->right->data:-1);
}
static avltree_visit_status_t visit_print(
                                avltree_t *                 tree,
                                avltree_node_t *            node,
                                avltree_visit_context_t *   context,
                                void *                      user_data) {
    long previous = *((long *) user_data);
    (void) context;
    (void) user_data;
    switch ((int)context->how) {
        case AVH_INFIX:
            if ((long) node->data < previous) {
                LOG_ERROR(NULL, "error: bad tree order node %ld < prev %ld",
                          (long)node->data, previous);
                return AVS_ERROR;
            }
            break ;
        case AVH_INFIX | AVH_RIGHT:
            if ((long) node->data > previous) {
                LOG_ERROR(NULL, "error: bad tree order node %ld > prev %ld",
                          (long)node->data, previous);
                return AVS_ERROR;
            }
            break ;
        default:
            break ;
    }
    fprintf(stderr, "%ld(%ld,%ld) ", (long) node->data,
            node->left ?(long)node->left->data : -1, node->right? (long)node->right->data : -1);
    return AVS_CONTINUE;
}

static avltree_node_t *     avltree_node_insert_rec(
                                avltree_t * tree,
                                avltree_node_t ** node,
                                void * data) {
    int cmp;

    if (*node == NULL) {
        avltree_node_t * new = avltree_node_create(tree, data, (*node), NULL);
        new->balance = (*node) ? 1 + ((*node)->balance != 0 ? 1 : 0): 0;
        *node = new;
        return new;
    } else if ((cmp = tree->cmp(data, (*node)->data)) <= 0) {
        return avltree_node_insert_rec(tree, &((*node)->left), data);
    } else {
        return avltree_node_insert_rec(tree, &((*node)->right), data);
    }
}

static avltree_node_t * avltree_insert_rec(avltree_t * tree, void * data) {
    if (tree == NULL) {
        return NULL;
    }
    return avltree_node_insert_rec(tree, &(tree->root), data);
}

static unsigned int avltree_test_visit(avltree_t * tree) {
    unsigned int nerror = 0;
    long prev;

    fprintf(stderr, "LARG PRINT\n");
    avlprint_larg(tree, stderr);
    fprintf(stderr, "recPREFL ");
    avlprint_pref_left(tree->root); fprintf(stderr, "\n");
    fprintf(stderr, "recINFFL ");
    avlprint_inf_left(tree->root); fprintf(stderr, "\n");
    fprintf(stderr, "recINFFR ");
    avlprint_inf_right(tree->root); fprintf(stderr, "\n");
    fprintf(stderr, "recSUFFL ");
    avlprint_suff_left(tree->root); fprintf(stderr, "\n");

    fprintf(stderr, "LARGL ");
    if (avltree_visit(tree, visit_print, &prev, AVH_BREADTH) != AVS_FINISHED)
        nerror++;
    fprintf(stderr, "\n");

    fprintf(stderr, "PREFL ");
    if (avltree_visit(tree, visit_print, &prev, AVH_PREFIX) != AVS_FINISHED)
        nerror++;
    fprintf(stderr, "\n");

    fprintf(stderr, "INFFL ");
    prev = LONG_MIN;
    if (avltree_visit(tree, visit_print, &prev, AVH_INFIX) != AVS_FINISHED)
        nerror++;
    fprintf(stderr, "\n");

    fprintf(stderr, "INFFR ");
    prev = LONG_MAX;
    if (avltree_visit(tree, visit_print, &prev, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED)
        nerror++;
    fprintf(stderr, "\n");

    fprintf(stderr, "SUFFL ");
    if (avltree_visit(tree, visit_print, &prev, AVH_SUFFIX) != AVS_FINISHED)
        nerror++;
    fprintf(stderr, "\n");

    return nerror;
}

#define LG(val) ((void *)((long)(val)))

static int test_avltree(const options_test_t * opts) {
    int             nerrors = 0;
    (void)          opts;
    avltree_t *     tree = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 1, 7, 4, 1 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    log_t           *logsave, log = { LOG_LVL_VERBOSE, stderr, LOG_FLAG_DEFAULT, "vlib" };

    logsave = log_set_vlib_instance(&log);

    //log.level = LOG_LVL_DEBUG;

    LOG_INFO(NULL, ">>> AVL-TREE tests");

    /* create tree INSERT REC*/
    LOG_INFO(NULL, "* creating tree(insert_rec)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(NULL, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(NULL, "* interting tree(insert_rec)");
    for (size_t i = 0; i < intssz; i++) {
        avltree_node_t * node = avltree_insert_rec(tree, (void*)((long)ints[i]));
        if (node == NULL) {
            LOG_ERROR(NULL, "error inserting elt <%ld>: %s", ints[i], strerror(errno));
            nerrors++;
        }
    }
    /* visit */
    nerrors += avltree_test_visit(tree);
    /* free */
    LOG_INFO(NULL, "* freeing tree(insert_rec)");
    avltree_free(tree);

    /* create tree INSERT */
    LOG_INFO(NULL, "* creating tree(insert)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(NULL, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(NULL, "* inserting in tree(insert)");
    for (size_t i = 0; i < intssz; i++) {
        LOG_DEBUG(&log, "* inserting %d", ints[i]);
        avltree_node_t * node = avltree_insert(tree, (void*)((long)ints[i]));
        if (node == NULL) {
            LOG_ERROR(NULL, "error inserting elt <%ld>: %s", ints[i], strerror(errno));
            nerrors++;
        }
        if (log.level >= LOG_LVL_DEBUG) {
            avlprint_larg(tree, stderr);
            getchar();
        }
    }
    /* visit */
    nerrors += avltree_test_visit(tree);
    /* free */
    LOG_INFO(NULL, "* freeing tree(insert)");
    avltree_free(tree);

    /* test with tree created manually */
    LOG_INFO(NULL, "* creating tree (insert_manual)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(NULL, "error creating tree(manual insert): %s", strerror(errno));
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
    avltree_test_visit(tree);
    /* free */
    LOG_INFO(NULL, "* freeing tree(insert_manual)");
    avltree_free(tree);

    log_set_vlib_instance(logsave);
    LOG_INFO(NULL, NULL);
    return nerrors;
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
    const int step_ms = 500;
    const unsigned char margin_tm = 30;
    const unsigned char margin_cpu = 30;
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
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, NULL, "__/ fake-fmt-check1 PRINT=LOG_WARN %s\\__ ", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, NULL, "__/ fake-fmt-check2 PRINT=LOG_WARN %s\\__ ", "");
    LOG_INFO(NULL, NULL);

    for (int i = 0; i < 5; i++) {
        BENCH_TM_START(tm0); BENCH_START(t0); BENCH_STOP(t0);
        BENCH_TM_STOP(tm0); LOG_WARN(NULL, "BENCH measured with BENCH_TM DURATION=%lldns cpu=%lldns", BENCH_TM_GET_NS(tm0), BENCH_GET_NS(t0));

        BENCH_START(t0); BENCH_TM_START(tm0); BENCH_TM_STOP(tm0);
        BENCH_STOP(t0); LOG_WARN(NULL, "BENCH_TM measured with BENCH cpu=%lldns DURATION=%lldns", BENCH_GET_NS(t0), BENCH_TM_GET_NS(tm0));

        BENCH_TM_START(tm0); usleep(123678);
        BENCH_TM_STOP(tm0); LOG_WARN(NULL, "BENCH_TM USLEEP(123678) DURATION=%lldns", BENCH_TM_GET_NS(tm0));
        if ((BENCH_TM_GET_US(tm0) < ((123678 * (100-margin_tm)) / 100)
        ||  BENCH_TM_GET_US(tm0) > ((123678 * (100+margin_tm)) / 100))) {
            fprintf(opts->out, "Error: BAD TM_bench %lu, expected %d with margin %u%%\n",
                    (unsigned long)(BENCH_TM_GET(tm0)), 123678, margin_tm);
            nerrors++;
        }
    }
    LOG_INFO(NULL, NULL);

    for (int i=0; i< 2000 / step_ms; i++) {
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

        BENCH_START(t0);
        BENCH_TM_START(tm0);
        while (s_bench_stop == 0)
            ;
        BENCH_TM_STOP(tm0);
        BENCH_STOP(t0);

        if (sigaction(SIGALRM, &sa_bak, NULL) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "restore sigaction(): %s\n", strerror(errno));
        }
        if (setitimer(ITIMER_REAL, &timer_bak, NULL) < 0) {
            ++nerrors;
            LOG_ERROR(NULL, "restore setitimer(): %s\n", strerror(errno));
        }

        if (i > 0 && (BENCH_GET(t0) < ((step_ms * (100-margin_cpu)) / 100)
                      || BENCH_GET(t0) > ((step_ms * (100+margin_cpu)) / 100))) {
            fprintf(opts->out, "Error: BAD bench %lu, expected %d with margin %u%%\n",
                    (unsigned long)(BENCH_GET(t0)), step_ms, margin_cpu);
            nerrors++;
        }
        if (i > 0 && (BENCH_TM_GET(tm0) < ((step_ms * (100-margin_tm)) / 100)
                      || BENCH_TM_GET(tm0) > ((step_ms * (100+margin_tm)) / 100))) {
            fprintf(opts->out, "Error: BAD TM_bench %lu, expected %d with margin %u%%\n",
                    (unsigned long)(BENCH_TM_GET(tm0)), step_ms, margin_tm);
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
    struct passwd   pw, * ppw;
    struct group    gr, * pgr;
    int             nerrors = 0;
    char *          user = TEST_ACC_USER;
    char *          group = TEST_ACC_GROUP;
    char *          buffer = NULL;
    char *          bufbak;
    size_t          bufsz = 0;
    uid_t           uid, refuid = 0;
    gid_t           gid, refgid = 500;
    int             ret;
    (void) opts;

    LOG_INFO(NULL, ">>> ACCOUNT TESTS...");

    while (refuid <= 500 && (ppw = getpwuid(refuid)) == NULL) refuid++;
    if (ppw)
        user = strdup(ppw->pw_name);
    while ((long) refgid >= 0 && refgid <= 500 && (pgr = getgrgid(refgid)) == NULL) refgid--;
    if (pgr)
        group = strdup(pgr->gr_name);

    LOG_INFO(NULL, "accounts: testing with user `%s`(%ld) and group `%s`(%ld)", user, (long) refuid, group, (long) refgid);

    /* pwfindid_r/grfindid_r with NULL buffer */
    if ((ret = pwfindid_r(user, &uid, NULL, NULL)) != 0 || uid != refuid) {
        LOG_ERROR(NULL, "error pwfindid_r(\"%s\", &uid, NULL, NULL) returns %d, uid:%ld", user, ret, (long) uid);
        nerrors++;
    }
    if ((ret = grfindid_r(group, &gid, NULL, NULL)) != 0 || gid != refgid) {
        LOG_ERROR(NULL, "error grfindid_r(\"%s\", &gid, NULL, NULL) returns %d, gid:%ld", group, ret, (long) gid);
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
    if ((ret = pwfindid_r(user, &uid, &buffer, NULL)) == 0) {
        LOG_ERROR(NULL, "error pwfindid_r(\"%s\", &uid, &buffer, NULL) returns OK, expected error", user);
        nerrors++;
    }
    if ((ret = grfindid_r(group, &gid, &buffer, NULL)) == 0) {
        LOG_ERROR(NULL, "error grfindid_r(\"%s\", &gid, &buffer, NULL) returns OK, expected error", group);
        nerrors++;
    }

    /* pwfindid_r/grfindid_r with shared buffer */
    if ((ret = pwfindid_r(user, &uid, &buffer, &bufsz)) != 0 ||  buffer == NULL || uid != refuid) {
        LOG_ERROR(NULL, "error pwfindid_r(\"%s\", &uid, &buffer, &bufsz) "
                        "returns %d, uid:%ld, buffer:0x%lx bufsz:%lu",
                  user, ret, (long) uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    bufbak = buffer;
    if ((ret = grfindid_r(group, &gid, &buffer, &bufsz)) != 0 || buffer != bufbak || gid != refgid) {
        LOG_ERROR(NULL, "error grfindid_r(\"%s\", &gid, &buffer, &bufsz) "
                        "returns %d, gid:%ld, buffer:0x%lx bufsz:%lu",
                  group, ret, (long) gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with shared buffer */
    if ((ret = pwfind_r(user, &pw, &buffer, &bufsz)) != 0 || buffer != bufbak || uid != refuid) {
        LOG_ERROR(NULL, "error pwfind_r(\"%s\", &pw, &buffer, &bufsz) "
                        "returns %d, uid:%ld, buffer:0x%lx bufsz:%lu",
                  user, ret, (long) pw.pw_uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = grfind_r(group, &gr, &buffer, &bufsz)) != 0 || buffer != bufbak || gid != refgid) {
        LOG_ERROR(NULL, "error grfind_r(\"%s\", &gr, &buffer, &bufsz) "
                        "returns %d, gid:%ld, buffer:0x%lx bufsz:%lu",
                  group, ret, (long) gr.gr_gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = pwfindbyid_r(refuid, &pw, &buffer, &bufsz)) != 0 || buffer != bufbak || strcmp(user, pw.pw_name)) {
        LOG_ERROR(NULL, "error pwfindbyid_r(%ld, &pw, &buffer, &bufsz) "
                        "returns %d, user:\"%s\", buffer:0x%lx bufsz:%lu",
                  (long) refuid, ret, pw.pw_name, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = grfindbyid_r(refgid, &gr, &buffer, &bufsz)) != 0 || buffer != bufbak || strcmp(group, gr.gr_name)) {
        LOG_ERROR(NULL, "error grfindbyid_r(%ld, &gr, &buffer, &bufsz) "
                        "returns %d, group:\"%s\", buffer:0x%lx bufsz:%lu",
                  (long) refgid, ret, gr.gr_name, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with NULL buffer */
    if ((ret = pwfind_r(user, &pw, NULL, &bufsz)) == 0) {
        LOG_ERROR(NULL, "error pwfind_r(\"%s\", &pw, NULL, &bufsz) returns OK, expected error", user);
        nerrors++;
    }
    if ((ret = grfind_r(group, &gr, NULL, &bufsz)) == 0) {
        LOG_ERROR(NULL, "error grfind_r(\"%s\", &gr, NULL, &bufsz) returns OK, expected error", group);
        nerrors++;
    }
    if ((ret = pwfindbyid_r(refuid, &pw, NULL, &bufsz)) == 0) {
        LOG_ERROR(NULL, "error pwfindbyid_r(%ld, &pw, NULL, &bufsz) returns OK, expected error", refuid);
        nerrors++;
    }
    if ((ret = grfindbyid_r(refgid, &gr, &buffer, NULL)) == 0 || buffer != bufbak) {
        LOG_ERROR(NULL, "error grfindbyid_r(%ld, &gr, &buffer, NULL) returns OK or changed buffer, expected error", refgid);
        nerrors++;
    }

    if (buffer != NULL) {
        free(buffer);
    }
    if (user && ppw) {
        free(user);
    }
    if (group && pgr) {
        free(group);
    }
    if (nerrors == 0) {
        LOG_INFO(NULL, "-> %s() OK.", __func__);
    }
    LOG_INFO(NULL, NULL);
    return nerrors;
}

static int test_thread(const options_test_t * opts) {
    int     nerrors = 0;
    (void)  opts;
    vlib_thread_t * vthread;

    LOG_INFO(NULL, ">>> THREAD tests");

    LOG_INFO(NULL, "creating thread timeout 0, kill before start");
    if ((vthread = vlib_thread_create(0, NULL)) == NULL)
        nerrors++;
    LOG_INFO(NULL, "killing");
    if (vlib_thread_stop(vthread) != 0)
        nerrors++;

    LOG_INFO(NULL, "creating thread timeout 500, start and kill after 1s");
    if ((vthread = vlib_thread_create(500, NULL)) == NULL)
        nerrors++;

    if (vlib_thread_start(vthread) != 0)
        nerrors++;

    LOG_INFO(NULL, "sleeping");
    sleep(1);
    LOG_INFO(NULL, "killing");

    if (vlib_thread_stop(vthread) != NULL)
        nerrors++;

    LOG_INFO(NULL, "creating thread timeout 0, start and kill after 1s");
    if ((vthread = vlib_thread_create(0, NULL)) == NULL)
        nerrors++;

    if (vlib_thread_start(vthread) != 0)
        nerrors++;

    LOG_INFO(NULL, "sleeping");
    sleep(1);
    LOG_INFO(NULL, "killing");

    if (vlib_thread_stop(vthread) != 0)
        nerrors++;

    LOG_INFO(NULL, "creating multiple threads");
    const int nthreads = 50;
    vlib_thread_t *vthreads[nthreads];
    log_t logs[nthreads];
    for(size_t i = 0; i < sizeof(vthreads) / sizeof(*vthreads); i++) {
        logs[i].prefix = strdup("thread000");
        snprintf(logs[i].prefix + 6, 4, "%03lu", i);
        logs[i].level = i % (nthreads/5) == 0 ? LOG_LVL_DEBUG : LOG_LVL_INFO;
        logs[i].out = stderr;
        logs[i].flags = LOG_FLAG_DEFAULT;
        if ((vthreads[i] = vlib_thread_create(i % 5 == 0 ? 100 : 0, &logs[i])) == NULL)
            nerrors++;
        else if (vlib_thread_start(vthreads[i]) != 0)
            nerrors++;
    }
    for (int i = sizeof(vthreads) / sizeof(*vthreads) - 1; i >= 0; i--) {
        if (vthreads[i] && vlib_thread_stop(vthreads[i]) != NULL)
            nerrors++;
        free(logs[i].prefix);
    }
    sleep(2);
    LOG_INFO(NULL, NULL);
    return nerrors;
}


/* *************** TEST MAIN FUNC *************** */

int test(int argc, const char *const* argv, unsigned int test_mode) {
    options_test_t  options_test    = { .flags = 0, .test_mode = test_mode, .logs = NULL, .out = stderr };
    int             errors = 0;

    LOG_INFO(NULL, ">>> TEST MODE: 0x%x\n", test_mode);

    /* Manage test program options */
    if ((test_mode & (1 << TEST_options)) != 0)
        errors += !OPT_IS_EXIT_OK(test_parse_options(argc, argv, &options_test));

    /* sizeof */
    if ((test_mode & (1 << TEST_sizeof)) != 0)
        errors += test_sizeof(&options_test);

    /* ascii */
    if ((test_mode & (1 << TEST_ascii)) != 0)
        errors += test_ascii(&options_test);

    /* test Bench */
    if ((test_mode & (1 << TEST_bench)) != 0)
        errors += test_bench(&options_test);

    /* test List */
    if ((test_mode & (1 << TEST_list)) != 0)
        errors += test_list(&options_test);

    /* test Hash */
    if ((test_mode & (1 << TEST_hash)) != 0)
        errors += test_hash(&options_test);

    /* test rbuf */
    if ((test_mode & (1 << TEST_rbuf)) != 0)
        errors += test_rbuf(&options_test);

    /* test Tree */
    if ((test_mode & (1 << TEST_tree)) != 0)
        errors += test_avltree(&options_test);

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
    if ((test_mode & (1 << TEST_sensorvalue)) != 0)
        errors += test_sensor_value(&options_test);

    /* Test vlib account functions */
    if ((test_mode & (1 << TEST_account)) != 0)
        errors += test_account(&options_test);

    /* Test vlib thread functions */
    if ((test_mode & (1 << TEST_vthread)) != 0)
        errors += test_thread(&options_test);

    /* Test Log in multiple threads */
    if ((test_mode & (1 << TEST_log)) != 0)
        errors += test_log_thread(&options_test);

    // *****************************************************************
    LOG_INFO(NULL, "<<< END of Tests : %d error(s).\n", errors);

    return -errors;
}
#endif /* ! ifdef _TEST */

