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
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
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
#include "vlib/logpool.h"
#include "vlib/term.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "vsensors.h"

#define VERSION_STRING OPT_VERSION_STRING_GPL3PLUS("TEST-" BUILD_APPNAME, APP_VERSION, \
                            "git:" BUILD_GITREV, "Vincent Sallaberry", "2017-2020")

/** strings corresponding of unit tests ids , for command line, in same order as s_testmode_str */
enum testmode_t {
    TEST_all = 0,
    TEST_sizeof,
    TEST_options,
    TEST_optusage,
    TEST_ascii,
    TEST_color,
    TEST_bench,
    TEST_hash,
    TEST_sensorvalue,
    TEST_log,
    TEST_account,
    TEST_vthread,
    TEST_list,
    TEST_tree,
    TEST_rbuf,
    TEST_bufdecode,
    TEST_srcfilter,
    TEST_logpool,
    /* starting from here, tests are not included in 'all' by default */
    TEST_excluded_from_all,
    TEST_bigtree = TEST_excluded_from_all,
    TEST_optusage_big,
    TEST_optusage_stdout,
    TEST_NB /* Must be LAST ! */
};
static const char * const s_testmode_str[] = {
    "all", "sizeof", "options", "optusage", "ascii", "color", "bench", "hash",
    "sensorvalue", "log", "account", "vthread", "list", "tree", "rbuf",
    "bufdecode", "srcfilter", "logpool", "bigtree", "optusage_big", "optusage_stdout",
    NULL
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
    { 'T', NULL,        NULL,           NULL },
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

typedef struct {
    unsigned int    flags;
    unsigned int    test_mode;
    logpool_t *     logs;
    FILE *          out;
} options_test_t;

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

int test_describe_filter(int short_opt, const char * arg, int * i_argv,
                         const opt_config_t * opt_config) {
    int     n = 0, ret;
    char    sep[2]= { 0, 0 };
    (void) short_opt;
    (void) opt_config;

    n += VLIB_SNPRINTF(ret, (char *) arg + n, *i_argv - n,
                         "- test modes (default '%s'): '",
                         s_testmode_str[TEST_all]);

    for (const char *const* mode = s_testmode_str; *mode; mode++, *sep = ',')
        n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s", sep, *mode);

    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "'. Excluded from '%s':'",
                         s_testmode_str[TEST_all]);
    *sep = 0;
    for (unsigned i = TEST_excluded_from_all; i < TEST_NB; i++, *sep = ',') {
        if (i < sizeof(s_testmode_str) / sizeof(char *)) {
            n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "%s%s",
                    sep, s_testmode_str[i]);
        }
    }
    n += VLIB_SNPRINTF(ret, ((char *)arg) + n, *i_argv - n, "'");

    *i_argv = n;
    return OPT_CONTINUE(1);
}

unsigned int test_getmode(const char *arg) {
    char * endptr = NULL;
    const unsigned int test_mode_all = (1 << (TEST_excluded_from_all)) - 1; //0xffffffffU;
    unsigned int test_mode = test_mode_all;
    if (arg != NULL) {
        errno = 0;
        test_mode = strtol(arg, &endptr, 0);
        if (errno != 0 || endptr == NULL || *endptr != 0) {
            const char * token, * next = arg;
            size_t len, i;
            while ((len = strtok_ro_r(&token, ",", &next, NULL, 0)) > 0 || *next) {
                for (i = 0; s_testmode_str[i]; i++) {
                    if (!strncasecmp(s_testmode_str[i], token, len)
                    &&  s_testmode_str[i][len] == 0) {
                        test_mode |= (i == TEST_all ? test_mode_all : (1U << i));
                        break ;
                    }
                }
                if (s_testmode_str[i] == NULL) {
                    fprintf(stderr, "unreconized test id '");
                    fwrite(token, 1, len, stderr); fputs("'\n", stderr);
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
#define PSIZEOF(type, log)              \
            LOG_INFO(log, "%32s: %zu", #type, sizeof(type))
#define POFFSETOF(type, member, log)    \
            LOG_INFO(log, "%32s: %zu", "off(" #type "." #member ")", offsetof(type, member))

static int test_sizeof(const options_test_t * opts) {
    log_t *         log     = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int    nerrors = 0;

    LOG_INFO(log, ">>> SIZE_OF tests");
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
    PSIZEOF(size_t, log);
    PSIZEOF(time_t, log);
    PSIZEOF(float, log);
    PSIZEOF(double, log);
    PSIZEOF(long double, log);
    PSIZEOF(char *, log);
    PSIZEOF(unsigned char *, log);
    PSIZEOF(void *, log);
    PSIZEOF(struct timeval, log);
    PSIZEOF(struct timespec, log);
    PSIZEOF(log_t, log);
    PSIZEOF(avltree_t, log);
    PSIZEOF(avltree_node_t, log);
    PSIZEOF(sensor_watch_t, log);
    PSIZEOF(sensor_value_t, log);
    PSIZEOF(sensor_sample_t, log);
    POFFSETOF(avltree_node_t, left, log);
    POFFSETOF(avltree_node_t, right, log);
    POFFSETOF(avltree_node_t, data, log);
    POFFSETOF(avltree_node_t, balance, log);

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
    for (unsigned i = 0; i < 1000; i++) {
        if (nodes[i]) free(nodes[i]);
    }

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** ASCII and TEST LOG BUFFER *************** */

static int test_ascii(options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int    nerrors = 0;
    int             result;
    char            ascii[256];
    ssize_t         n;

    LOG_INFO(log, ">>> ASCII/LOG_BUFFER tests");

    for (result = -128; result <= 127; result++) {
        ascii[result + 128] = result;
    }

    n = LOG_BUFFER(0, log, ascii, 256, "ascii> ");
    if (n <= 0) {
        LOG_ERROR(log, "%s ERROR : log_buffer returns %zd, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, log, NULL, 256, "null_01> ");
    if (n <= 0) {
        LOG_ERROR(log, "%s ERROR : log_buffer returns %zd, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, log, NULL, 0, "null_02> ");
    if (n <= 0) {
        LOG_ERROR(log, "%s ERROR : log_buffer returns %zd, expected >0", __func__, n);
        ++nerrors;
    }

    n = LOG_BUFFER(0, log, ascii, 0, "ascii_sz=0> ");
    if (n <= 0) {
        LOG_ERROR(log, "%s ERROR : log_buffer returns %zd, expected >0", __func__, n);
        ++nerrors;
    }
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST COLORS *****************/
static int test_colors(options_test_t * opts) {
    log_t *         log     = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    FILE *          out     = log && log->out ? log->out : stderr;
    unsigned int    nerrors = 0;

    for (int f=VCOLOR_FG; f < VCOLOR_BG; f++) {
        for (int s=VCOLOR_STYLE; s < VCOLOR_RESERVED; s++) {
            flockfile(out);
            log_header(LOG_LVL_INFO, log, __FILE__, __func__, __LINE__);
            fprintf(out, "hello %s%sWORLD",
                     vterm_color(1, s), vterm_color(1, f));
            for (int b=VCOLOR_BG; b < VCOLOR_STYLE; b++) {
                fprintf(out, "%s %s%s%sWORLD", vterm_color(1, VCOLOR_RESET),
                        vterm_color(1, f), vterm_color(1, s), vterm_color(1, b));
            }
            fprintf(out, "%s!\n", vterm_color(1, VCOLOR_RESET));
            funlockfile(out);
        }
    }
    LOG_INFO(log, "%shello%s %sworld%s !", vterm_color(fileno(out), VCOLOR_GREEN),
             vterm_color(fileno(out), VCOLOR_RESET), vterm_color(fileno(out), VCOLOR_RED),
             vterm_color(fileno(out), VCOLOR_RESET));

    LOG_VERBOSE(log, "res:\x1{green}");
    LOG_VERBOSE(log, "res:\x1{green}");

    LOG_BUFFER(LOG_LVL_INFO, log, vterm_color(1, VCOLOR_RESET), 6, "vcolor_reset ");

    //TODO log to file and check it does not contain colors

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST OPTIONS *************** */

#define OPTUSAGE_PIPE_END_LINE  "#$$^& TEST_OPT_USAGE_END &^$$#" /* no EOL here! */

typedef struct {
    opt_config_t *  opt_config;
    const char *    filter;
    int             ret;
} optusage_data_t;

void * optusage_run(void * vdata) {
    optusage_data_t * data = (optusage_data_t *) vdata ;

    data->ret = opt_usage(0, data->opt_config, data->filter);

    flockfile(data->opt_config->log->out);
    fprintf(data->opt_config->log->out, "\n%s\n", OPTUSAGE_PIPE_END_LINE);
    fflush(data->opt_config->log->out);
    funlockfile(data->opt_config->log->out);

    return NULL;
}

/* test with options logging to file */
static int test_optusage(int argc, const char *const* argv, options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                            s_opt_desc_test, VERSION_STRING, opts);
    unsigned int    nerrors = 0;
    int             columns, desc_minlen, desc_align;

    log_t   optlog = { .level = LOG_LVL_INFO, .out = NULL, .prefix = "opttests",
                       .flags = LOG_FLAG_LEVEL | LOG_FLAG_MODULE };

    char    tmpname[PATH_MAX];
    int     tmpfdout, tmpfdin;
    FILE *  tmpfilein, * tmpfileout;
    char *  line;
    size_t  line_allocsz;
    ssize_t len;
    size_t  n_lines = 0, n_chars = 0, chars_max = 0;
    size_t  n_lines_all = 0, n_chars_all = 0, chars_max_all = 0;
    int     pipefd[2];
    int		ret;

    LOG_INFO(log, ">>> OPTUSAGE log tests");

    if (pipe(pipefd) < 0 || (tmpfileout = fdopen((tmpfdout = pipefd[1]), "w")) == NULL
    ||  (tmpfilein = fdopen((tmpfdin = pipefd[0]), "r")) == NULL)  {
        LOG_ERROR(log, "%s(): exiting: cannot create/open pipe: %s",
            __func__, strerror(errno));
        return ++nerrors;
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
            if ((opts->test_mode & (1 << TEST_optusage_big)) == 0
                    && (desc_minlen > 5 && desc_minlen < columns - 5)
                    && desc_minlen != 15 && desc_minlen != 80
                    && desc_minlen != 20 && desc_minlen != 30 && desc_minlen != 40
                    && desc_minlen != OPT_USAGE_DESC_MINLEN)
                continue ;

            for (desc_align = 0; desc_align < columns + 5; ++desc_align) {
                /* skip some tests if not in big test mode */
                if ((opts->test_mode & (1 << TEST_optusage_big)) == 0
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
                        if (pthread_create(&tid, NULL, optusage_run, &data) != 0) {
                            ++nerrors;
                            LOG_ERROR(log, "%s(): cannot create optusage thread: %s",
                                    __func__, strerror(errno));
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
                                        ++nerrors;
                                        LOG_ERROR(log,
                                         "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})"
                                            "error: line has bad character 0x%02x : '%s'",
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
                                    ++nerrors;
                                    LOG_ERROR(log,
                                     "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})"
                                        ": error: line len (%zd) > max (%d)  : '%s'",
                                        *filter ? *filter : "",
                                        flags[i_flg], i_flg, *opt_head, *desc_head,
                                        desc_minlen, desc_align, len, columns, line);
                                }
                            } /* getline */
                        } /* select */

                        pthread_join(tid, NULL);
                        if (OPT_IS_ERROR(data.ret)) {
                            LOG_ERROR(log,
                             "optusage%s(flg:%d#%d,optH:%s,desc{H:%s,min:%d,align:%d})"
                                ": opt_usage() error", *filter ? *filter : "",
                                flags[i_flg], i_flg, *opt_head, *desc_head,
                                desc_minlen, desc_align);
                            ++nerrors;
                        }
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

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);

    return nerrors;
}

static int test_options(int argc, const char *const* argv, options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, VERSION_STRING, opts);
    int             result;
    unsigned int    nerrors = 0;

    LOG_INFO(log, ">>> OPTIONS tests");

    opt_config_test.log = logpool_getlog(opts->logs, "options", LPG_NODEFAULT);

    /* test opt_parse_options with default flags */
    result = opt_parse_options(&opt_config_test);
    LOG_INFO(log, ">>> opt_parse_options() result: %d", result);
    if (result <= 0) {
        LOG_ERROR(log, "ERROR opt_parse_options() expected >0, got %d", result);
        ++nerrors;
    }

    /* test opt_parse_options with only main section in usage */
    opt_config_test.flags |= OPT_FLAG_MAINSECTION;
    result = opt_parse_options(&opt_config_test);
    LOG_INFO(log, ">>> opt_parse_options(shortusage) result: %d", result);
    if (result <= 0) {
        LOG_ERROR(log, "ERROR opt_parse_options(shortusage) expected >0, got %d", result);
        ++nerrors;
    }
    opt_config_test.flags &= ~OPT_FLAG_MAINSECTION;

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST OPTUSAGE *************** */

static int test_optusage_stdout(int argc, const char *const* argv, options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, VERSION_STRING, opts);
    unsigned int    nerrors = 0;
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
            opt_usage(0, &opt_config_test, NULL);
            opt_usage(0, &opt_config_test, "all");
        }
    }

    /* restore COLUMNS env */
    if (env_columns == NULL) {
        unsetenv("COLUMNS");
    } else {
        setenv("COLUMNS", env_columns, 1);
    }
    vterm_free();

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}
/* *************** TEST LIST *************** */

static int intcmp(const void * a, const void *b) {
    return (long)a - (long)b;
}

static int test_list(const options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int    nerrors = 0;
    slist_t *       list = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 7, 4, 1 };
    const size_t    intssz = sizeof(ints)/sizeof(*ints);
    long            prev;

    LOG_INFO(log, ">>> LIST tests");

    /* insert_sorted */
    for (size_t i = 0; i < intssz; i++) {
        if ((list = slist_insert_sorted(list, (void*)((long)ints[i]), intcmp)) == NULL) {
            LOG_ERROR(log, "slist_insert_sorted(%d) returned NULL", ints[i]);
            nerrors++;
        }
        if (log->level >= LOG_LVL_INFO) {
            fprintf(log->out, "%02d> ", ints[i]);
            SLIST_FOREACH_DATA(list, a, long) fprintf(log->out, "%ld ", a);
            fputc('\n', log->out);
        }
    }
    /* prepend, append, remove */
    list = slist_prepend(list, (void*)((long)-2));
    list = slist_prepend(list, (void*)((long)0));
    list = slist_prepend(list, (void*)((long)-1));
    list = slist_append(list, (void*)((long)-4));
    list = slist_append(list, (void*)((long)20));
    list = slist_append(list, (void*)((long)15));
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, "after prepend/append:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(log->out, "%ld ", a);
        fputc('\n', log->out);
    }
    /* we have -1, 0, -2, ..., 20, -4, 15, we will remove -1(head), then -2(in the middle),
     * then 15 (tail), then -4(in the middle), and the list should still be sorted */
    list = slist_remove(list, (void*)((long)-1), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-2), intcmp, NULL);
    list = slist_remove(list, (void*)((long)15), intcmp, NULL);
    list = slist_remove(list, (void*)((long)-4), intcmp, NULL);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(log->out, "after remove:");
        SLIST_FOREACH_DATA(list, a, long) fprintf(log->out, "%ld ", a);
        fputc('\n', log->out);
    }

    prev = 0;
    SLIST_FOREACH_DATA(list, a, long) {
        if (a < prev) {
            LOG_ERROR(log, "list elt <%ld> has wrong position regarding <%ld>", a, prev);
            nerrors++;
        }
        prev = a;
    }
    slist_free(list, NULL);

    if (prev != 20) {
        LOG_ERROR(log, "list elt <%d> should be last insteand of <%ld>", 20, prev);
        nerrors++;
    }

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST HASH *************** */

static void test_one_hash_insert(
                    hash_t *hash, const char * str, options_test_t * opts,
                    unsigned int * errors, log_t * log) {
    FILE *  out = log->out;
    int     ins;
    char *  fnd;
    (void)  opts;

    ins = hash_insert(hash, (void *) str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(out, " hash_insert [%s]: %d, ", str, ins);
    }

    fnd = (char *) hash_find(hash, str);
    if (log->level >= LOG_LVL_INFO) {
        fprintf(out, "find: [%s]\n", fnd);
    }

    if (ins != HASH_SUCCESS) {
        LOG_ERROR(log, "ERROR hash_insert [%s]: got %d, expected HASH_SUCCESS(%d)",
                  str, ins, HASH_SUCCESS);
        ++(*errors);
    }
    if (fnd == NULL || strcmp(fnd, str)) {
        LOG_ERROR(log, "ERROR hash_find [%s]: got %s", str, fnd ? fnd : "NULL");
        ++(*errors);
    }
}

static int test_hash(options_test_t * opts) {
    log_t *                     log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    hash_t *                    hash;
    FILE *                      out = log->out;
    unsigned int                errors = 0;
    static const char * const   hash_strs[] = {
        VERSION_STRING, "a", "z", "ab", "ac", "cxz", "trz", NULL
    };

    LOG_INFO(log, ">>> HASH TESTS");

    hash = hash_alloc(HASH_DEFAULT_SIZE, 0, hash_ptr, hash_ptrcmp, NULL);
    if (hash == NULL) {
        LOG_ERROR(log, "hash_alloc(): ERROR Hash null");
        errors++;
    } else {
        if (log->level >= LOG_LVL_INFO) {
            fprintf(out, "hash_ptr: %08x\n", hash_ptr(hash, opts));
            fprintf(out, "hash_ptr: %08x\n", hash_ptr(hash, hash));
            fprintf(out, "hash_str: %08x\n", hash_str(hash, out));
            fprintf(out, "hash_str: %08x\n", hash_str(hash, VERSION_STRING));
        }
        hash_free(hash);
    }

    for (unsigned int hash_size = 1; hash_size < 200; hash_size += 100) {
        if ((hash = hash_alloc(hash_size, 0, hash_str, (hash_cmp_fun_t) strcmp, NULL)) == NULL) {
            LOG_ERROR(log, "ERROR hash_alloc() sz:%d null", hash_size);
            errors++;
            continue ;
        }

        for (const char *const* strs = hash_strs; *strs; strs++) {
            test_one_hash_insert(hash, *strs, opts, &errors, log);
        }

        if (log->level >= LOG_LVL_INFO && hash_print_stats(hash, out) <= 0) {
            LOG_ERROR(log, "ERROR hash_print_stat sz:%d returns <= 0, expected >0", hash_size);
            errors++;
        }
        hash_free(hash);
    }
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, errors);
    return errors;
}

/* *************** TEST STACK *************** */
enum {
    SPT_NONE        = 0,
    SPT_PRINT       = 1 << 0,
    SPT_REPUSH      = 1 << 1,
};
static int rbuf_pop_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags, log_t * log) {
    unsigned int nerrors = 0;
    size_t i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        if (rbuf_push(rbuf, (void*)((long)ints[i])) != 0) {
            LOG_ERROR(log, "error rbuf_push(%d)", ints[i]);
            nerrors++;
        }
    }
    if ((i = rbuf_size(rbuf)) != intssz) {
        LOG_ERROR(log, "error rbuf_size %zu should be %zu", i, intssz);
        nerrors++;
    }
    i = intssz - 1;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long t = (long) rbuf_top(rbuf);
        long p = (long) rbuf_pop(rbuf);

        if (t != p || p != ints[i]) {
            if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
            LOG_ERROR(log, "error rbuf_top(%ld) != rbuf_pop(%ld) != %d", t, p, ints[i]);
            nerrors++;
        }
        if (i == 0) {
            if (j == 1) {
                i = intssz / 2 - intssz % 2;
                j = 2;
            }else if ( j == 2 ) {
                if (rbuf_size(rbuf) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
                    LOG_ERROR(log, "rbuf is too large");
                    nerrors++;
                }
            }
        } else {
            --i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", t);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                if (rbuf_push(rbuf, (void*)((long)ints[j])) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
                    LOG_ERROR(log, "error rbuf_push(%d)", ints[j]);
                    nerrors++;
                }
            }
            i = intssz - 1;
            j = 1;
        }

    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);

    return nerrors;
}
static int rbuf_dequeue_test(rbuf_t * rbuf, const int * ints, size_t intssz, int flags, log_t*log) {
    unsigned int nerrors = 0;
    size_t i, j;

    if (log->level < LOG_LVL_INFO)
        flags &= ~SPT_PRINT;

    for (i = 0; i < intssz; i++) {
        if (rbuf_push(rbuf, (void*)((long)ints[i])) != 0) {
            LOG_ERROR(log, "error rbuf_push(%d)", ints[i]);
            nerrors++;
        }
    }
    if ((i = rbuf_size(rbuf)) != intssz) {
        LOG_ERROR(log, "error rbuf_size %zu should be %zu", i, intssz);
        nerrors++;
    }
    i = 0;
    j = 0;
    while (rbuf_size(rbuf) != 0) {
        long b = (long) rbuf_bottom(rbuf);
        long d = (long) rbuf_dequeue(rbuf);
        if (b != d || b != ints[i]) {
            if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
            LOG_ERROR(log, "error rbuf_bottom(%ld) != rbuf_dequeue(%ld) != %d", b, d, ints[i]);
            nerrors++;
        }
        if (i == intssz - 1) {
            if (rbuf_size(rbuf) != ((j == 1?1:0) * intssz)) {
                if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
                LOG_ERROR(log, "rbuf is too large");
                nerrors++;
            }
            j = 2;
            i = 0;
        } else {
            ++i;
        }
        if ((flags & SPT_PRINT) != 0)
            fprintf(log->out, "%ld ", b);

        if (j == 0 && (flags & SPT_REPUSH) != 0 && rbuf_size(rbuf) == intssz / 2) {
            for (j = 0; j < intssz; j++) {
                if (rbuf_push(rbuf, (void*)((long)ints[j])) != 0) {
                    if ((flags & SPT_PRINT) != 0) fputc('\n', log->out);
                    LOG_ERROR(log, "error rbuf_push(%d)", ints[j]);
                    nerrors++;
                }
            }
            j = 1;
        }
    }
    if ((flags & SPT_PRINT) != 0)
        fputc('\n', log->out);
    if (i != 0) {
        LOG_ERROR(log, "error: missing values in dequeue");
        nerrors++;
    }
    return nerrors;
}
static int test_rbuf(options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    rbuf_t *        rbuf;
    unsigned int    nerrors = 0;
    const int       ints[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    size_t          i, iter;

    LOG_INFO(log, ">>> STACK TESTS");

    /* rbuf_pop 31 elts */
    LOG_INFO(log, "* rbuf_pop tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    if (rbuf == NULL) {
        LOG_ERROR(log, "ERROR rbuf_create null");
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_pop_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, log);
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_pop_test(rbuf, ints, intssz, iter == 0 ?
                                                     SPT_PRINT | SPT_REPUSH : SPT_REPUSH, log);
    }
    rbuf_free(rbuf);

    /* rbuf_dequeue 31 elts */
    LOG_INFO(log, "* rbuf_dequeue tests");
    rbuf = rbuf_create(1, RBF_DEFAULT);
    if (rbuf == NULL) {
        LOG_ERROR(log, "ERROR rbuf_create null");
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ? SPT_PRINT : SPT_NONE, log);
    }
    for (iter = 0; iter < 10; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, ints, intssz, iter == 0 ?
                                                         SPT_PRINT | SPT_REPUSH : SPT_REPUSH, log);
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
        nerrors += rbuf_dequeue_test(rbuf, tab, tabsz, SPT_NONE, log);
    }
    for (iter = 0; iter < 1000; iter++) {
        nerrors += rbuf_dequeue_test(rbuf, tab, tabsz, SPT_REPUSH, log);
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf RBF_OVERWRITE, rbuf_get, rbuf_set */
    LOG_INFO(log, "* rbuf OVERWRITE tests");
    rbuf = rbuf_create(5, RBF_DEFAULT | RBF_OVERWRITE);
    if (rbuf == NULL) {
        LOG_ERROR(log, "ERROR rbuf_create Overwrite null");
        nerrors++;
    }
    for (i = 0; i < 3; i++) {
        if (rbuf_push(rbuf, (void *) ((i % 5) + 1)) != 0) {
            LOG_ERROR(log, "error rbuf_push(%zu) overwrite mode", (i % 5) + 1);
            nerrors++;
        }
    }
    i = 1;
    while(rbuf_size(rbuf) != 0) {
        size_t b = (size_t) rbuf_bottom(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, 0);
        size_t d = (size_t) rbuf_dequeue(rbuf);
        if (b != i || g != i || d != i) {
            LOG_ERROR(log, "error rbuf_get(%zd) != rbuf_top != rbuf_dequeue != %zu", i - 1, i);
            nerrors++;
        }
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", i);
        i++;
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    if (i != 4) {
        LOG_ERROR(log, "error rbuf_size 0 but not 3 elts (%zd)", i-1);
        nerrors++;
    }

    for (i = 0; i < 25; i++) {
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "#%zu(%zu) ", i, (i % 5) + 1);
        if (rbuf_push(rbuf, (void *)((size_t) ((i % 5) + 1))) != 0) {
            if (log->level >= LOG_LVL_INFO)
                fputc('\n', log->out);
            LOG_ERROR(log, "error rbuf_push(%zu) overwrite mode", (i % 5) + 1);
            nerrors++;
        }
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    if ((i = rbuf_size(rbuf)) != 5) {
        LOG_ERROR(log, "error rbuf_size (%zu) should be 5", i);
        nerrors++;
    }
    if (rbuf_set(rbuf, 7, (void *) 7L) != -1) {
        LOG_ERROR(log, "error rbuf_set(7) should be rejected");
        nerrors++;
    }
    i = 5;
    while (rbuf_size(rbuf) != 0) {
        size_t t = (size_t) rbuf_top(rbuf);
        size_t g = (size_t) rbuf_get(rbuf, i-1);
        size_t p = (size_t) rbuf_pop(rbuf);
        if (t != i || g != i || p != i) {
            if (log->level >= LOG_LVL_INFO)
                fputc('\n', log->out);
            LOG_ERROR(log, "error rbuf_get(%zd) != rbuf_top != rbuf_pop != %zu", i - 1, i);
            nerrors++;
        }
        i--;
        if (log->level >= LOG_LVL_INFO)
            fprintf(log->out, "%zu ", p);
    }
    if (log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    if (i != 0) {
        LOG_ERROR(log, "error rbuf_size 0 but not 5 elts (%zd)", 5-i);
        nerrors++;
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    /* rbuf_set with RBF_OVERWRITE OFF */
    LOG_INFO(log, "* rbuf_set tests with increasing buffer");
    rbuf = rbuf_create(1, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    if (rbuf == NULL) {
        LOG_ERROR(log, "ERROR rbuf_create Overwrite null");
        nerrors++;
    }
    if ((i = rbuf_size(rbuf)) != 0) {
        LOG_ERROR(log, "ERROR rbuf_size(%zu) should be 0", i);
        nerrors++;
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 1, (void *) 1) != 0) {
            LOG_ERROR(log, "ERROR rbuf_set(1)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 2) {
            LOG_ERROR(log, "ERROR rbuf_size(%zu) should be 2", i);
            nerrors++;
        }
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 3, (void *) 3) != 0) {
            LOG_ERROR(log, "ERROR rbuf_set(3)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 4) {
            LOG_ERROR(log, "ERROR rbuf_size(%zu) should be 4", i);
            nerrors++;
        }
    }
    for (iter = 0; iter < 10; iter++) {
        if (rbuf_set(rbuf, 5000, (void*) 5000) != 0) {
            LOG_ERROR(log, "ERROR rbuf_set(5000)");
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != 5001) {
            LOG_ERROR(log, "ERROR rbuf_size(%zu) should be 5001", i);
            nerrors++;
        }
    }
    if (rbuf_reset(rbuf) != 0) {
        LOG_ERROR(log, "error: rbuf_reset");
        nerrors++;
    }
    for (iter = 1; iter <= 5000; iter++) {
        if (rbuf_set(rbuf, iter-1, (void*) iter) != 0) {
            LOG_ERROR(log, "ERROR rbuf_set(%zu)", iter);
            nerrors++;
        }
        if ((i = rbuf_size(rbuf)) != iter) {
            LOG_ERROR(log, "ERROR rbuf_size(%zu) should be %zu", i, iter);
            nerrors++;
        }
    }
    for (iter = 0; rbuf_size(rbuf) != 0; iter++) {
        size_t g = (size_t) rbuf_get(rbuf, 5000-iter-1);
        size_t t = (size_t) rbuf_top(rbuf);
        size_t p = (size_t) rbuf_pop(rbuf);
        i = rbuf_size(rbuf);
        if (t != p || g != p || p != 5000-iter || i != 5000-iter-1) {
            LOG_ERROR(log, "error top(%zu) != get(#%zd=%zu) != pop(%zu) != %zd "
                            "or rbuf_size(%zu) != %zd",
                      t, 5000-iter-1, g, p, 5000-iter, i, 5000-iter-1);
            nerrors++;
        }
    }
    if (iter != 5000) {
        LOG_ERROR(log, "error not 5000 elements (%zu)", iter);
        nerrors++;
    }
    LOG_INFO(log, "rbuf MEMORYSIZE = %zu", rbuf_memorysize(rbuf));
    rbuf_free(rbuf);

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST AVL TREE *************** */
typedef struct {
    rbuf_t *    results;
    log_t *     log;
    FILE *      out;
    void *      min;
    void *      max;
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
    (void) tree;
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;
    long previous
           = rbuf_size(stack) ? (long)(((avltree_node_t *) rbuf_top(stack))->data)
                              : LONG_MAX;

    if ((context->how & (AVH_PREFIX|AVH_SUFFIX)) == 0) {
        switch (context->state) {
            case AVH_INFIX:
                if ((context->how & AVH_RIGHT) == 0) {
                    if (previous == LONG_MAX) previous = LONG_MIN;
                    if ((long) node->data < previous) {
                        LOG_ERROR(data->log, "error: bad tree order node %ld < prev %ld",
                                  (long)node->data, previous);
                        return AVS_ERROR;
                    }
                } else if ((long) node->data > previous) {
                    LOG_ERROR(data->log, "error: bad tree order node %ld > prev %ld",
                              (long)node->data, previous);
                    return AVS_ERROR;
                }
                break ;
            default:
                break ;
        }
    }
    if (data->out) fprintf(data->out, "%ld(%ld,%ld) ", (long) node->data,
            node->left ?(long)node->left->data : -1, node->right? (long)node->right->data : -1);
    rbuf_push(stack, node);
    return AVS_CONTINUE;
}
static avltree_visit_status_t visit_range(
                                avltree_t *                     tree,
                                avltree_node_t *                node,
                                const avltree_visit_context_t * context,
                                void *                          user_data) {
    (void) tree;
    (void) context;
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;

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

static unsigned int avltree_check_results(rbuf_t * results, rbuf_t * reference, log_t * log) {
    unsigned int nerror = 0;

    if (rbuf_size(results) != rbuf_size(reference)) {
        LOG_ERROR(log, "error: results size:%zu != reference size:%zu",
                  rbuf_size(results), rbuf_size(reference));
        return 1;
    }
    while (rbuf_size(reference)) {
        long ref = (long)(((avltree_node_t *)rbuf_dequeue(reference))->data);
        long res = (long)(((avltree_node_t *)rbuf_dequeue(results  ))->data);
        if (res != ref) {
            LOG_ERROR(log, "error: result:%ld != reference:%ld", res, ref);
            nerror++;
        }
    }
    rbuf_reset(reference);
    rbuf_reset(results);
    return nerror;
}

static unsigned int avltree_test_visit(avltree_t * tree, int check_balance,
                                       FILE * out, log_t * log) {
    unsigned int            nerror = 0;
    rbuf_t *                results    = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    rbuf_t *                reference  = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    avltree_print_data_t    data = { .results = results, .out = out};
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
    nerror += avltree_check_results(results, reference, log);

    /* prefix left */
    if (out)
        fprintf(out, "recPREFL ");
    avlprint_pref_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "PREFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log);

    /* infix left */
    if (out)
        fprintf(out, "recINFL  ");
    avlprint_inf_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "INFL     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log);

    /* infix right */
    if (out)
        fprintf(out, "recINFR  ");
    avlprint_inf_right(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "INFR     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log);

    /* suffix left */
    if (out)
        fprintf(out, "recSUFFL ");
    avlprint_suff_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "SUFFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_SUFFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log);

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
    nerror += avltree_check_results(results, reference, log);

    if (out)
        LOG_INFO(log, "current tree stack maxsize = %zu", rbuf_maxsize(tree->stack));
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

    if (results)
        rbuf_free(results);
    if (reference)
        rbuf_free(reference);

    return nerror;
}

#define LG(val) ((void *)((long)(val)))

static int test_avltree(const options_test_t * opts) {
    unsigned int    nerrors = 0;
    avltree_t *     tree = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 1, 7, 4, 1 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    int             n;
    long            ref_val;
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);

    LOG_INFO(log, ">>> AVL-TREE tests");

    /* create tree INSERT REC*/
    LOG_INFO(log, "* CREATING TREE (insert_rec)");
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
    LOG_INFO(log, "* CREATING TREE (insert)");
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

        if (log->level >= LOG_LVL_DEBUG) {
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

        if (log->level >= LOG_LVL_DEBUG) {
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
    LOG_INFO(log, "* creating tree(insert, AFL_INSERT_*)");
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
    LOG_INFO(log, "* CREATING TREE (insert_manual)");
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
        LOG_INFO(log, "* CREATING TREE (insert same values:%ld)", *samevalue);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }
        /* visit on empty tree */
        LOG_INFO(log, "* visiting empty tree (insert_same_values:%ld)", *samevalue);
        nerrors += avltree_test_visit(tree, 1, NULL, log);
        /* insert */
        LOG_INFO(log, "* inserting in tree(insert_same_values:%ld)", *samevalue);
        for (size_t i = 0; i < samevalues_count; i++) {
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

            if (log->level >= LOG_LVL_DEBUG) {
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        /* visit */
        nerrors += avltree_test_visit(tree, 1, NULL, log);
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

            if (log->level >= LOG_LVL_DEBUG) {
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
    rbuf_t * two_results        = rbuf_create(2, RBF_DEFAULT | RBF_OVERWRITE);
    rbuf_t * all_results        = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    rbuf_t * all_refs           = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    avltree_print_data_t data   = { .results = two_results, .log = log, .out = NULL };
    const size_t nb_elts[] = { 100 * 1000, 1 * 1000 * 1000, SIZE_MAX, 10 * 1000 * 1000, 0 };
    for (const size_t * nb = nb_elts; *nb != 0; nb++) {
        long    value, min_val = LONG_MAX, max_val = LONG_MIN;
        size_t  n_remove, total_remove = *nb / 10;
        rbuf_t * del_vals;

        if (*nb == SIZE_MAX) { /* after size max this is only for TEST_bigtree */
            if ((opts->test_mode & (1 << TEST_bigtree)) != 0) continue ; else break ;
        }
        if (nb == nb_elts) {
            srand(INT_MAX); /* first loop with predictive random for debugging */
            //log->level = LOG_LVL_DEBUG;
        } else {
            //log->level = LOG_LVL_INFO;
            srand(time(NULL));
        }

        LOG_INFO(log, "* CREATING BIG TREE (insert, %zu elements)", *nb);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }

        /* insert */
        del_vals = rbuf_create(total_remove, RBF_DEFAULT | RBF_OVERWRITE);
        LOG_INFO(log, "* inserting in Big tree(insert, %zu elements)", *nb);
        BENCH_START(bench);
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

            if ((i * 100) % *nb == 0 && log->level >= LOG_LVL_INFO)
                fputc('.', log->out);

            if (result != (void *) value || (result == NULL && errno != 0)) {
                LOG_ERROR(log, "error inserting elt <%ld>, result <%lx> : %s",
                          value, (unsigned long) result, strerror(errno));
                nerrors++;
            }
            if (log->level >= LOG_LVL_DEBUG) {
                nerrors += avlprint_rec_check_balance(tree->root, log);
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        if (log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCH_STOP_LOG(bench, log, "creation of %zu nodes ", *nb);

        /* visit */
        LOG_INFO(log, "* checking balance, infix, infix_r, min, max, depth, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check balance (recursive) of %zu nodes | ", *nb);

        rbuf_reset(data.results);
        BENCH_START(bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCH_STOP_LOG(bench, log, "infix visit of %zu nodes | ", *nb);

        rbuf_reset(data.results);
        BENCH_START(bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCH_STOP_LOG(bench, log, "infix_right visit of %zu nodes | ", *nb);

        LOG_INFO(log, "current tree stack maxsize = %zu", rbuf_maxsize(tree->stack));
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
        BENCH_START(bench);
        if (AVS_FINISHED !=
                (n = avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
        || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit_range() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCH_STOP_LOG(bench, log, "VISIT_RANGE (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->avltree_visit() */
        data.results = all_refs;
        BENCH_START(bench);
        if (AVS_FINISHED != (n = avltree_visit(tree, visit_range, &data, AVH_INFIX))
        || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCH_STOP_LOG(bench, log, "VISIT INFIX (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->compare avltree_visit and avltree_visit_range */
        if ((n = avltree_check_results(all_results, all_refs, NULL)) > 0) {
            LOG_ERROR(log, "visit_range/visit_infix --> error: different results");
            nerrors += n;
        }
        data.results = two_results;

        /* remove (total_remove) elements */
        LOG_INFO(log, "* removing in tree (%zu nodes)", total_remove);
        BENCH_START(bench);
        for (n_remove = 0; rbuf_size(del_vals) != 0; ++n_remove) {
            if ((n_remove * 100) % total_remove == 0 && log->level >= LOG_LVL_INFO)
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
        if (log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCH_STOP_LOG(bench, log, "REMOVED %zu nodes | ", total_remove);
        rbuf_free(del_vals);

        /***** Checking tree after remove */
        LOG_INFO(log, "* checking balance, infix, infix_r, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check balance (recursive) of %zd nodes | ", *nb - total_remove);

        /* count */
        BENCH_START(bench);
        n = avlprint_rec_get_count(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive COUNT (%zu nodes) = %d | ",
                       *nb, n);

        BENCH_START(bench);
        value = avltree_count(tree);
        BENCH_STOP_LOG(bench, log, "COUNT (%zu nodes) = %ld | ",
                       *nb, value);
        if (value != n || (size_t) value != (*nb - total_remove)) {
            LOG_ERROR(log, "error incorrect COUNT %ld, expected %d(rec), %zd",
                      value, n, *nb - total_remove);
            ++nerrors;
        }
        /* infix */
        rbuf_reset(data.results);
        BENCH_START(bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCH_STOP_LOG(bench, log, "infix visit of %zd nodes | ", *nb - total_remove);
        /* infix right */
        rbuf_reset(data.results);
        BENCH_START(bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCH_STOP_LOG(bench, log, "infix_right visit of %zd nodes | ", *nb - total_remove);

        /* free */
        LOG_INFO(log, "* freeing tree(insert)");
        BENCH_START(bench);
        avltree_free(tree);
        BENCH_STOP_LOG(bench, log, "freed %zd nodes | ", *nb - total_remove);
    }
    rbuf_free(two_results);
    rbuf_free(all_results);
    rbuf_free(all_refs);
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}


/* *************** BENCH SENSOR VALUES CAST *************** */

static int test_sensor_value(options_test_t * opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned long   r;
    unsigned long   i;
    unsigned long   nb_op = 50000000;
    unsigned int    nerrors = 0;
    BENCH_DECL(t);

    LOG_INFO(log, ">>> SENSOR VALUE TESTS");

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
        BENCH_STOP_LOG(t, log, "sensor_value_toint    r=%08lx ", r);

        BENCH_START(t);
        for (i=0, r=0; i < nb_op; i++) {
            s1.value.data.i = r++;
            r += sensor_value_todouble(&s1.value) < sensor_value_todouble(&s2.value);
        }
        BENCH_STOP_LOG(t, log, "sensor_value_todouble %-10s ", "");
    }
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
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
            while ((n = fcntl(fd_pipein, F_SETFL, O_NONBLOCK)) < 0)
                if (errno != EINTR) return (void *) (nerrors + 1);
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
            }
        }
    }
    return (void *) nerrors;
}

static int test_log_thread(options_test_t * opts) {
    log_t *             log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    const char * const  files[] = { "stdout", "one_file", NULL };
    slist_t             *filepaths = NULL;
    const size_t        cmdsz = 4 * PATH_MAX;
    char *              cmd;
    unsigned int        nerrors = 0;
    int                 i;
    int                 ret;

    LOG_INFO(log, ">>> LOG THREAD TESTS");
    fflush(NULL); /* First of all ensure all previous messages are flushed */
    i = 0;
    for (const char * const * filename = files; *filename; filename++, i++) {
        char                path[PATH_MAX];
        FILE *              file;
        FILE *              fpipeout = NULL;
        FILE *              fpipein = NULL;
        test_log_thread_t   threads[N_TEST_THREADS];
        log_t               logs[N_TEST_THREADS];
        log_t               thlog = {
                .level = LOG_LVL_SCREAM,
                .flags = LOG_FLAG_DEFAULT | LOG_FLAG_FILE | LOG_FLAG_FUNC
                         | LOG_FLAG_LINE | LOG_FLAG_LOC_TAIL
        };
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
        LOG_INFO(log, ">>> logthread: test %s (%s)", *filename, path);

        /* Create pipe to redirect stdout & stderr to pipe log file (fpipeout) */
        if ((!strcmp("stderr", *filename) && (file = stderr))
        ||  (!strcmp(*filename, "stdout") && (file = stdout))) {
            fpipeout = fopen(path, "w");
            if (fpipeout == NULL) {
                LOG_ERROR(log, "error: create cannot create '%s': %s", path, strerror(errno));
                ++nerrors;
                continue ;
            } else if (pipe(p) < 0) {
                LOG_ERROR(log, "ERROR pipe: %s", strerror(errno));
                ++nerrors;
                fclose(fpipeout);
                continue ;
            } else if (p[PIPE_OUT] < 0 || (fd_backup = dup(fileno(file))) < 0
            ||         dup2(p[PIPE_OUT], fileno(file)) < 0) {
                LOG_ERROR(log, "ERROR dup2: %s", strerror(errno));
                ++nerrors;
                fclose(fpipeout);
                close(p[PIPE_IN]);
                close(p[PIPE_OUT]);
                close(fd_backup);
                continue ;
            } else if ((fpipein = fdopen(p[PIPE_IN], "r")) == NULL) {
                LOG_ERROR(log, "ERROR, cannot fdopen p[PIPE_IN]: %s", strerror(errno));
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
            LOG_ERROR(log, "Error: create cannot create '%s': %s", path, strerror(errno));
            ++nerrors;
            continue ;
        }

        /* Create log threads and launch them */
        thlog.out = file;
        BENCH_TM_START(tm0);
        BENCH_START(t0);
        for (unsigned int i = 0; i < N_TEST_THREADS; i++) {
            logs[i] = thlog;
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
                LOG_ERROR(log, "ERROR dup2 restore: %s", strerror(errno));
            }
            /* cleaning */
            fclose(fpipein); // fclose will close p[PIPE_IN]
            close(p[PIPE_OUT]);
            close(fd_backup);
            fclose(fpipeout);
            /* write something in <file> and it must not be redirected anymore */
            if (fprintf(file, "*** checking %s is not anymore redirected ***\n\n", *filename) <= 0) {
                LOG_ERROR(log, "ERROR fprintf(checking %s): %s", *filename, strerror(errno));
                ++nerrors;
            }
        } else {
            fclose(file);
        }
        LOG_INFO(log, "duration : %ld.%03lds (clock %ld.%03lds)",
                 BENCH_TM_GET(tm0) / 1000, BENCH_TM_GET(tm0) % 1000,
                 BENCH_GET(t0) / 1000, BENCH_GET(t0) % 1000);
    }
    /* compare logs */
    LOG_INFO(log, "checking logs...");
    if ((cmd = malloc(cmdsz)) == NULL) {
        LOG_ERROR(log, "Error, cannot allocate memory for shell command: %s", strerror(errno));
        ++nerrors;
    } else {
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
        if (system(cmd) != 0) {
            LOG_ERROR(log, "Error during logs comparison");
            ++nerrors;
        }
        free(cmd);
    }
    slist_free(filepaths, free);
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST BENCH *************** */
static volatile sig_atomic_t s_bench_stop;
static void bench_sighdl(int sig) {
    (void)sig;
    s_bench_stop = 1;
}
static int test_bench(options_test_t *opts) {
    log_t *             log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    BENCH_DECL(t0);
    BENCH_TM_DECL(tm0);
    struct sigaction sa_bak, sa = { .sa_handler = bench_sighdl, .sa_flags = SA_RESTART };
    sigset_t sigset, sigset_bak;
    const int step_ms = 300;
    const unsigned      margin_lvl[] = { LOG_LVL_ERROR, LOG_LVL_WARN };
    const char *        margin_str[] = { "error: ", "warning: " };
    const unsigned char margin_tm[]  = { 75, 15 };
    const unsigned char margin_cpu[] = { 100, 30 };
    unsigned int nerrors = 0;

    LOG_INFO(log, ">>> BENCH TESTS...");
    sigemptyset(&sa.sa_mask);
    BENCH_START(t0);
    BENCH_STOP_PRINTF(t0, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                      12UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    BENCH_START(t0);
    BENCH_STOP_LOG(t0, log, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                   40UL, "STRING", 'Z', (void*)&nerrors, nerrors);
    BENCH_STOP_PRINT(t0, LOG_WARN, log, "fake-bench-for-fmt-check r=%lu s=%s c=%c p=%p e=%d ",
                     98UL, "STRING", 'Z', (void*)&nerrors, nerrors);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, log, "// fake-fmt-check LOG%s //", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_LOG(tm0, log, "// fake-fmt-check LOG %d //", 54);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF%s || ", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINTF(tm0, ">> fake-fmt-check PRINTF %d || ", 65);

    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, log, "__/ fake-fmt-check1 PRINT=LOG_WARN %s\\__ ", "");
    BENCH_TM_START(tm0);
    BENCH_TM_STOP_PRINT(tm0, LOG_WARN, log, "__/ fake-fmt-check2 PRINT=LOG_WARN %s\\__ ", "");
    LOG_INFO(log, NULL);

    sigfillset(&sigset);
    sigdelset(&sigset, SIGALRM);
    if (sigprocmask(SIG_SETMASK, &sigset, &sigset_bak) != 0) {
        LOG_ERROR(log, "sigprocmask : %s", strerror(errno));
        ++nerrors;
    }
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, &sa_bak) < 0) {
        ++nerrors;
        LOG_ERROR(log, "sigaction(): %s\n", strerror(errno));
    }

    for (int i = 0; i < 5; i++) {
        struct itimerval timer123 = {
            .it_value =    { .tv_sec = 0, .tv_usec = 123678 },
            .it_interval = { .tv_sec = 0, .tv_usec = 0 },
        };
        BENCH_TM_START(tm0); BENCH_START(t0); BENCH_STOP(t0);
        BENCH_TM_STOP(tm0);
        LOG_INFO(log, "BENCH measured with BENCH_TM DURATION=%ldns cpu=%ldns",
                 BENCH_TM_GET_NS(tm0), BENCH_GET_NS(t0));

        BENCH_START(t0); BENCH_TM_START(tm0); BENCH_TM_STOP(tm0);
        BENCH_STOP(t0);
        LOG_INFO(log, "BENCH_TM measured with BENCH cpu=%ldns DURATION=%ldns",
                 BENCH_GET_NS(t0), BENCH_TM_GET_NS(tm0));

        if (setitimer(ITIMER_REAL, &timer123, NULL) < 0) {
            LOG_ERROR(log, "setitimer(): %s", strerror(errno));
            ++nerrors;
        }

        BENCH_TM_START(tm0);
        pause();
        BENCH_TM_STOP(tm0);
        LOG_INFO(log, "BENCH_TM timer(123678) DURATION=%ldns", BENCH_TM_GET_NS(tm0));

        for (unsigned wi = 0; wi < 2; wi++) {
            if ((BENCH_TM_GET_US(tm0) < ((123678 * (100-margin_tm[wi])) / 100)
            ||  BENCH_TM_GET_US(tm0) > ((123678 * (100+margin_tm[wi])) / 100))) {
                vlog(margin_lvl[wi], NULL, __FILE__, __func__, __LINE__,
                     "%s: BAD TM_bench %lu, expected %d with margin %u%%",
                     margin_str[wi],
                     (unsigned long)(BENCH_TM_GET(tm0)), 123678, margin_tm[wi]);
                if (margin_lvl[wi] == LOG_LVL_ERROR)
                    ++nerrors;
                break ;
            }
        }
    }
    LOG_INFO(log, NULL);

    for (int i=0; i< 2000 / step_ms; i++) {
        struct itimerval timer_bak, timer = {
            .it_value     = { .tv_sec = step_ms / 1000, .tv_usec = (step_ms % 1000) * 1000 },
            .it_interval  = { 0, 0 }
        };

        s_bench_stop = 0;
        if (setitimer(ITIMER_REAL, &timer, &timer_bak) < 0) {
            ++nerrors;
            LOG_ERROR(log, "setitimer(): %s\n", strerror(errno));
        }
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
                if (margin_lvl[wi] == LOG_LVL_ERROR)
                    ++nerrors;
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
                if (margin_lvl[wi] == LOG_LVL_ERROR)
                    ++nerrors;
                break ;
            }
        }
    }
    /* no need to restore timer as it_interval is 0.
    if (setitimer(ITIMER_REAL, &timer_bak, NULL) < 0) {
        ++nerrors;
        LOG_ERROR(log, "restore setitimer(): %s\n", strerror(errno));
    }*/
    if (sigaction(SIGALRM, &sa_bak, NULL) < 0) {
        ++nerrors;
        LOG_ERROR(log, "restore sigaction(): %s\n", strerror(errno));
    }
    if (sigprocmask(SIG_SETMASK, &sigset_bak, NULL) != 0) {
        LOG_ERROR(log, "sigprocmask(restore) : %s", strerror(errno));
        ++nerrors;
    }
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST ACCOUNT *************** */
#define TEST_ACC_USER "nobody"
#define TEST_ACC_GROUP "nogroup"
static int test_account(options_test_t *opts) {
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    struct passwd   pw, * ppw;
    struct group    gr, * pgr;
    unsigned int    nerrors = 0;
    char *          user = TEST_ACC_USER;
    char *          group = TEST_ACC_GROUP;
    char *          buffer = NULL;
    char *          bufbak;
    size_t          bufsz = 0;
    uid_t           uid, refuid = 0;
    gid_t           gid, refgid = 500;
    int             ret;

    LOG_INFO(log, ">>> ACCOUNT TESTS...");

    while (refuid <= 500 && (ppw = getpwuid(refuid)) == NULL) refuid++;
    if (ppw)
        user = strdup(ppw->pw_name);
    while (refgid + 1 > 0 && refgid <= 500 && (pgr = getgrgid(refgid)) == NULL) refgid--;
    if (pgr)
        group = strdup(pgr->gr_name);

    LOG_INFO(log, "accounts: testing with user `%s`(%ld) and group `%s`(%ld)",
             user, (long) refuid, group, (long) refgid);

    /* pwfindid_r/grfindid_r with NULL buffer */
    if ((ret = pwfindid_r(user, &uid, NULL, NULL)) != 0 || uid != refuid) {
        LOG_ERROR(log, "error pwfindid_r(\"%s\", &uid, NULL, NULL) "
                        "returns %d, uid:%ld", user, ret, (long) uid);
        nerrors++;
    }
    if ((ret = grfindid_r(group, &gid, NULL, NULL)) != 0 || gid != refgid) {
        LOG_ERROR(log, "error grfindid_r(\"%s\", &gid, NULL, NULL) "
                        "returns %d, gid:%ld", group, ret, (long) gid);
        nerrors++;
    }
    if ((ret = pwfindid_r("__**UserNoFOOUUnd**!!", &uid, NULL, NULL)) == 0) {
        LOG_ERROR(log, "error pwfindid_r(\"__**UserNoFOOUUnd**!!\", &uid, NULL, NULL) "
                        "returns OK, expected error");
        nerrors++;
    }
    if ((ret = grfindid_r("__**GroupNoFOOUUnd**!!", &gid, NULL, NULL)) == 0) {
        LOG_ERROR(log, "error grfindid_r(\"__**GroupNoFOOUUnd**!!\", &gid, NULL, NULL) "
                        "returns OK, expected error");
        nerrors++;
    }
    if ((ret = pwfindid_r(user, &uid, &buffer, NULL)) == 0) {
        LOG_ERROR(log, "error pwfindid_r(\"%s\", &uid, &buffer, NULL) "
                        "returns OK, expected error", user);
        nerrors++;
    }
    if ((ret = grfindid_r(group, &gid, &buffer, NULL)) == 0) {
        LOG_ERROR(log, "error grfindid_r(\"%s\", &gid, &buffer, NULL) "
                        "returns OK, expected error", group);
        nerrors++;
    }

    /* pwfindid_r/grfindid_r with shared buffer */
    if ((ret = pwfindid_r(user, &uid, &buffer, &bufsz)) != 0
    ||  buffer == NULL || uid != refuid) {
        LOG_ERROR(log, "error pwfindid_r(\"%s\", &uid, &buffer, &bufsz) "
                        "returns %d, uid:%ld, buffer:0x%lx bufsz:%zu",
                  user, ret, (long) uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    bufbak = buffer;
    if ((ret = grfindid_r(group, &gid, &buffer, &bufsz)) != 0
    ||  buffer != bufbak || gid != refgid) {
        LOG_ERROR(log, "error grfindid_r(\"%s\", &gid, &buffer, &bufsz) "
                        "returns %d, gid:%ld, buffer:0x%lx bufsz:%zu",
                  group, ret, (long) gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with shared buffer */
    if ((ret = pwfind_r(user, &pw, &buffer, &bufsz)) != 0
    ||  buffer != bufbak || uid != refuid) {
        LOG_ERROR(log, "error pwfind_r(\"%s\", &pw, &buffer, &bufsz) "
                        "returns %d, uid:%ld, buffer:0x%lx bufsz:%zu",
                  user, ret, (long) pw.pw_uid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = grfind_r(group, &gr, &buffer, &bufsz)) != 0
    ||  buffer != bufbak || gid != refgid) {
        LOG_ERROR(log, "error grfind_r(\"%s\", &gr, &buffer, &bufsz) "
                        "returns %d, gid:%ld, buffer:0x%lx bufsz:%zu",
                  group, ret, (long) gr.gr_gid, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = pwfindbyid_r(refuid, &pw, &buffer, &bufsz)) != 0
    ||  buffer != bufbak || strcmp(user, pw.pw_name)) {
        LOG_ERROR(log, "error pwfindbyid_r(%ld, &pw, &buffer, &bufsz) "
                        "returns %d, user:\"%s\", buffer:0x%lx bufsz:%zu",
                  (long) refuid, ret, pw.pw_name, (unsigned long) buffer, bufsz);
        nerrors++;
    }
    if ((ret = grfindbyid_r(refgid, &gr, &buffer, &bufsz)) != 0
    ||  buffer != bufbak || strcmp(group, gr.gr_name)) {
        LOG_ERROR(log, "error grfindbyid_r(%ld, &gr, &buffer, &bufsz) "
                        "returns %d, group:\"%s\", buffer:0x%lx bufsz:%zu",
                  (long) refgid, ret, gr.gr_name, (unsigned long) buffer, bufsz);
        nerrors++;
    }

    /* pwfind_r/grfind_r/pwfindbyid_r/grfindbyid_r with NULL buffer */
    if ((ret = pwfind_r(user, &pw, NULL, &bufsz)) == 0) {
        LOG_ERROR(log, "error pwfind_r(\"%s\", &pw, NULL, &bufsz) "
                        "returns OK, expected error", user);
        nerrors++;
    }
    if ((ret = grfind_r(group, &gr, NULL, &bufsz)) == 0) {
        LOG_ERROR(log, "error grfind_r(\"%s\", &gr, NULL, &bufsz) "
                        "returns OK, expected error", group);
        nerrors++;
    }
    if ((ret = pwfindbyid_r(refuid, &pw, NULL, &bufsz)) == 0) {
        LOG_ERROR(log, "error pwfindbyid_r(%ld, &pw, NULL, &bufsz) "
                        "returns OK, expected error", (long) refuid);
        nerrors++;
    }
    if ((ret = grfindbyid_r(refgid, &gr, &buffer, NULL)) == 0 || buffer != bufbak) {
        LOG_ERROR(log, "error grfindbyid_r(%ld, &gr, &buffer, NULL) "
                        "returns OK or changed buffer, expected error", (long) refgid);
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
    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

#define PIPETHREAD_STR      "Test Start Loop\n"
#define PIPETHREAD_BIGSZ    (PIPE_BUF * 4)
typedef struct {
    log_t *         log;
    unsigned int    nb_ok;
    unsigned int    nb_bigok;
    unsigned int    nb_error;
    char *          bigpipebuf;
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
    const unsigned int      header_sz = 1;
    (void) vthread;
    (void) event;
    (void) event_data;

    while ((ret = read(fd, buffer, header_sz)) > 0) {
        if (*buffer == 0) {
            /* bigpipebuf */
            n = header_sz;
            while (n < PIPETHREAD_BIGSZ) {
                size_t toread = n + PIPE_BUF > PIPETHREAD_BIGSZ ? PIPETHREAD_BIGSZ - n : PIPE_BUF;
                ret = read(fd, buffer, toread);
                if (ret < 0) {
                    LOG_ERROR(pipectx->log, "error: bad big pipe message size");
                    ++pipectx->nb_error;
                    break ;
                }
                for (unsigned int i = 0; i < ret; i++) {
                    if (buffer[i] != (char)((n + i) % 254 + 1)) {
                        LOG_ERROR(pipectx->log, "error: bad big pipe message (sz %zd)", ret);
                        ++pipectx->nb_error;
                    }
                }
                n += ret;
            }
            if (n != PIPETHREAD_BIGSZ) {
                LOG_ERROR(pipectx->log, "error: bad pipe message");
                ++pipectx->nb_error;
            } else {
                ++pipectx->nb_bigok;
            }
        } else {
            if ((ret = read(fd, buffer + header_sz, sizeof(PIPETHREAD_STR) - 1 - header_sz)) <= 0) {
                LOG_ERROR(pipectx->log, "error: bad pipe message size");
                ++pipectx->nb_error;
                break ;
            }
            ret += header_sz;
            LOG_BUFFER(LOG_LVL_VERBOSE, pipectx->log, buffer, ret, "%s(): ", __func__);
            if (ret != sizeof(PIPETHREAD_STR) - 1 || memcmp(buffer, PIPETHREAD_STR, ret) != 0) {
                LOG_ERROR(pipectx->log, "error: bad pipe message");
                ++pipectx->nb_error;
            } else {
                ++pipectx->nb_ok;
            }
            n += ret;
        }
    }

    return 0;
}

static int test_thread(const options_test_t * opts) {
    log_t *         log             = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int    nerrors         = 0;
    vlib_thread_t * vthread;
    void *          thread_result   = (void *) 1UL;
    const long      bench_margin_ms = 400;

    BENCH_TM_DECL(t);

    LOG_INFO(log, ">>> THREAD tests");

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 0, kill before start");
    if ((vthread = vlib_thread_create(0, log)) == NULL) {
        LOG_ERROR(log, "vlib_thread_create() error");
        nerrors++;
    }
    LOG_INFO(log, "killing");
    if (vlib_thread_stop(vthread) != thread_result) {
        LOG_ERROR(log, "vlib_thread_stop() error");
        nerrors++;
    }
    BENCH_TM_STOP(t);
    if (BENCH_TM_GET(t) > bench_margin_ms) {
        ++nerrors;
        LOG_ERROR(log, "error: test duration: %ld ms", BENCH_TM_GET(t));
    }

    /* **** */
    BENCH_TM_START(t);
    LOG_INFO(log, "creating thread timeout 500, start and kill after 1s");
    if ((vthread = vlib_thread_create(500, log)) == NULL) {
        LOG_ERROR(log, "vlib_thread_create() error");
        nerrors++;
    }
    if (vlib_thread_start(vthread) != 0) {
        LOG_ERROR(log, "vlib_thread_start() error");
        nerrors++;
    }
    sched_yield();
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    if (vlib_thread_stop(vthread) != thread_result) {
        LOG_ERROR(log, "vlib_thread_stop() error");
        nerrors++;
    }
    BENCH_TM_STOP(t);
    if (BENCH_TM_GET(t) > 1000 + bench_margin_ms) {
        ++nerrors;
        LOG_ERROR(log, "error: test duration: %ld ms", BENCH_TM_GET(t));
    }

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, start and kill after 1s");
    BENCH_TM_START(t);
    if ((vthread = vlib_thread_create(0, log)) == NULL) {
        LOG_ERROR(log, "vlib_thread_create() error");
        nerrors++;
    }
    if (vlib_thread_start(vthread) != 0) {
        LOG_ERROR(log, "vlib_thread_start() error");
        nerrors++;
    }
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    if (vlib_thread_stop(vthread) != thread_result) {
        LOG_ERROR(log, "vlib_thread_stop() error");
        nerrors++;
    }
    BENCH_TM_STOP(t);
    if (BENCH_TM_GET(t) > 1000 + bench_margin_ms) {
        ++nerrors;
        LOG_ERROR(log, "error: test duration: %ld ms", BENCH_TM_GET(t));
    }

    /* **** */
    LOG_INFO(log, "creating thread timeout 0 exit_sig SIGALRM, start and kill after 1s");
    BENCH_TM_START(t);
    if ((vthread = vlib_thread_create(0, log)) == NULL) {
        LOG_ERROR(log, "vlib_thread_create() error");
        nerrors++;
    }
    if (vlib_thread_set_exit_signal(vthread, SIGALRM) != 0) {
        LOG_ERROR(log, "vlib_thread_exit_signal() error");
        nerrors++;
    }
    if (vlib_thread_start(vthread) != 0) {
        LOG_ERROR(log, "vlib_thread_start() error");
        nerrors++;
    }
    LOG_INFO(log, "sleeping");
    sleep(1);
    LOG_INFO(log, "killing");
    if (vlib_thread_stop(vthread) != thread_result) {
        LOG_ERROR(log, "vlib_thread_stop() error");
        nerrors++;
    }
    BENCH_TM_STOP(t);
    if (BENCH_TM_GET(t) > 1000 + bench_margin_ms) {
        ++nerrors;
        LOG_ERROR(log, "error: test duration: %ld ms", BENCH_TM_GET(t));
    }

    /* **** */
    LOG_INFO(log, "creating thread timeout 0, exit_sig SIGALRM after 500ms, "
                   "start and kill after 500 more ms");
    BENCH_TM_START(t);
    if ((vthread = vlib_thread_create(0, log)) == NULL) {
        LOG_ERROR(log, "vlib_thread_create() error");
        nerrors++;
    }
    if (vlib_thread_start(vthread) != 0) {
        LOG_ERROR(log, "vlib_thread_start() error");
        nerrors++;
    }
    LOG_INFO(log, "sleeping");
    usleep(500000);
    if (vlib_thread_set_exit_signal(vthread, SIGALRM) != 0) {
        LOG_ERROR(log, "vlib_thread_exit_signal() error");
        nerrors++;
    }
    usleep(500000);
    LOG_INFO(log, "killing");
    if (vlib_thread_stop(vthread) != thread_result) {
        LOG_ERROR(log, "vlib_thread_stop() error");
        nerrors++;
    }
    BENCH_TM_STOP(t);
    if (BENCH_TM_GET(t) > 1000 + bench_margin_ms) {
        ++nerrors;
        LOG_ERROR(log, "error: test duration: %ld ms", BENCH_TM_GET(t));
    }

    /* **** */
    LOG_INFO(log, "creating multiple threads");
    const unsigned      nthreads = 50;
    int                 pipefd = -1;
    vlib_thread_t *     vthreads[nthreads];
    log_t               logs[nthreads];
    pipethread_ctx_t    pipectx = { log, 0, 0, 0, NULL };

    /* init big pipe buf */
    if ((pipectx.bigpipebuf = malloc(PIPETHREAD_BIGSZ)) != NULL) {
        pipectx.bigpipebuf[0] = 0;
        for (unsigned i = 1; i < PIPETHREAD_BIGSZ; ++i) {
            pipectx.bigpipebuf[i] = (char) (i%254+1);
        }
    } else {
        LOG_ERROR(log, "thread: malloc bigpipebuf error: %s", strerror(errno));
        ++nerrors;
    }
    for(size_t i = 0; i < sizeof(vthreads) / sizeof(*vthreads); i++) {
        logs[i].prefix = strdup("thread000");
        snprintf(logs[i].prefix + 6, 4, "%03zu", i);
        logs[i].level = log->level;
        logs[i].out = log->out;
        logs[i].flags = LOG_FLAG_DEFAULT;
        if ((vthreads[i] = vlib_thread_create(i % 5 == 0 ? 100 : 0, &logs[i])) == NULL) {
            LOG_ERROR(log, "vlib_thread_create() error");
            nerrors++;
        } else {
            if (i == 0) {
                if ((pipefd = vlib_thread_pipe_create(vthreads[i], piperead_callback,
                                                      &pipectx)) < 0) {
                    LOG_ERROR(log, "error vlib_thread_pipe_create()");
                    ++nerrors;
                }
            }
            if (vlib_thread_start(vthreads[i]) != 0) {
                LOG_ERROR(log, "vlib_thread_start() error");
                nerrors++;
            }
            if (vlib_thread_pipe_write(vthreads[0], pipefd, PIPETHREAD_STR,
                                       sizeof(PIPETHREAD_STR) - 1) != sizeof(PIPETHREAD_STR) - 1) {
                LOG_ERROR(log, "error vlib_thread_pipe_write(pipestr): %s", strerror(errno));
                nerrors++ ;
            }
            if (vlib_thread_pipe_write(vthreads[0], pipefd, pipectx.bigpipebuf, PIPETHREAD_BIGSZ)
                        != PIPETHREAD_BIGSZ) {
                LOG_ERROR(log, "error vlib_thread_pipe_write(bigpipebuf): %s", strerror(errno));
                nerrors++ ;
            }
        }
    }

    for (int i = sizeof(vthreads) / sizeof(*vthreads) - 1; i >= 0; i--) {
        if (vthreads[i] && vlib_thread_stop(vthreads[i]) != thread_result) {
            LOG_ERROR(log, "vlib_thread_stop() error");
            ++nerrors;
        }
        free(logs[i].prefix);
    }

    if (pipectx.bigpipebuf != NULL)
        free(pipectx.bigpipebuf);

    if (pipectx.nb_ok != nthreads || pipectx.nb_bigok != nthreads || pipectx.nb_error != 0) {
        LOG_ERROR(log, "error: pipethread: %u/%u msgs, %u/%u bigmsgs, %u errors",
                  pipectx.nb_ok, nthreads, pipectx.nb_bigok, nthreads, pipectx.nb_error);
        ++nerrors;
    }
    sleep(2);

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
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
                       log_t * log) {
    char decoded_buffer[32768];
    void * ctx = NULL;
    unsigned int nerrors = 0;
    unsigned int total = 0;
    int n;

    /* decode and assemble in allbuffer */
    while ((n = vdecode_buffer(NULL, outbuf, outbufsz, &ctx, inbuf, inbufsz)) > 0) {
        memcpy(decoded_buffer + total, outbuf, n);
        total += n;
    }
    /* check wheter ctx is set to NULL after call */
    if (ctx != NULL) {
        LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) finished but ctx not NULL",
                  desc, outbufsz);
        ++nerrors;
    }
    if (outbufsz == 0 || inbuf == NULL
#  if ! CONFIG_ZLIB
    || (inbufsz >= 3 && inbuf[0] == 31 && (unsigned char)(inbuf[1]) == 139 && inbuf[2] == 8)
#  endif
    ) {
        /* (outbufsz=0 or inbuf == NULL) -> n must be -1, total must be 0 */
        if (n != -1) {
            LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) has not returned -1",
                      desc, outbufsz);
            ++nerrors;
        }
        if (total != 0) {
            LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) outbufsz 0 but n_decoded=%u",
                      desc, outbufsz, total);
            ++nerrors;
        }
    } else {
        if (n != 0) {
            /* outbufsz > 0: n must be 0, check whether decoded total is refbufsz */
            LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) has not returned 0 (%d)",
                      desc, outbufsz, n);
            ++nerrors;
        }
        if (total != refbufsz) {
            LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) total(%u) != ref(%zu)",
                      desc, outbufsz, total, refbufsz);
            ++nerrors;
        } else if (total != 0) {
            /* compare refbuffer and decoded_buffer */
            if (memcmp(refbuffer, decoded_buffer, total) != 0) {
                LOG_ERROR(log, "error: vdecode_buffer(%s,outbufsz:%zu) decoded != reference",
                          desc, outbufsz);
                ++nerrors;
            }
        }
    }
    return nerrors;
}

int test_bufdecode(options_test_t * opts) {
    unsigned int    nerrors = 0;
    log_t *         log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned        n;
    char            buffer[16384];
    char            refbuffer[16384];
    unsigned        bufszs[] = { 0, 1, 2, 3, 8, 16, 3000 };

    LOG_INFO(log, ">>> VDECODE_BUFFER tests");

#  if ! CONFIG_ZLIB
    LOG_INFO(log, "warning: no zlib on this system");
#  endif

    for (unsigned n_bufsz = 0; n_bufsz < sizeof(bufszs) / sizeof(*bufszs); n_bufsz++) {
        unsigned bufsz = bufszs[n_bufsz];
        unsigned int refbufsz;

        /* NULL or 0 size inbuffer */
        LOG_DEBUG(log, "* NULL inbuf (outbufsz:%u)", bufsz);
        nerrors += test_one_bufdecode(NULL, 198,
                                      buffer, bufsz, NULL, 0, "inbuf=NULL", log);
        LOG_DEBUG(log, "* inbufsz=0 (outbufsz:%u)", bufsz);
        nerrors += test_one_bufdecode((const char *) rawbuf, 0,
                                      buffer, bufsz, NULL, 0, "inbufsz=0", log);

        /* ZLIB */
        LOG_DEBUG(log, "* ZLIB (outbufsz:%u)", bufsz);
        str0cpy(refbuffer, "1\n", sizeof(refbuffer));
        refbufsz = 2;
        nerrors += test_one_bufdecode((const char *) zlibbuf, sizeof(zlibbuf) / sizeof(char),
                                      buffer, bufsz, refbuffer, refbufsz, "zlib", log);

        /* RAW with MAGIC */
        LOG_DEBUG(log, "* RAW (outbufsz:%u)", bufsz);
        for (n = 4; n < sizeof(rawbuf) / sizeof(char); n++) {
            refbuffer[n - 4] = rawbuf[n];
        }
        refbufsz = n - 4;
        nerrors += test_one_bufdecode(rawbuf, sizeof(rawbuf) / sizeof(char),
                                      buffer, bufsz, refbuffer, refbufsz, "raw", log);

        /* RAW WITHOUT MAGIC */
        LOG_DEBUG(log, "* RAW without magic (outbufsz:%u)", bufsz);
        for (n = 0; n < sizeof(rawbuf2) / sizeof(char); n++) {
            refbuffer[n] = rawbuf2[n];
        }
        refbufsz = n;
        nerrors += test_one_bufdecode(rawbuf2, sizeof(rawbuf2) / sizeof(char),
                                      buffer, bufsz, refbuffer, refbufsz, "raw2", log);

        /* STRTAB */
        LOG_DEBUG(log, "* STRTAB (outbufsz:%u)", bufsz);
        refbufsz = 0;
        for (const char *const* pstr = strtabbuf+1; *pstr; pstr++) {
            size_t n = str0cpy(refbuffer + refbufsz, *pstr, sizeof(refbuffer) - refbufsz);
            refbufsz += n;
        }
        nerrors += test_one_bufdecode((const char *) strtabbuf, sizeof(strtabbuf) / sizeof(char),
                                      buffer, bufsz, refbuffer, refbufsz, "strtab", log);
    }

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST SOURCE FILTER *************** */

void opt_set_source_filter_bufsz(size_t bufsz);

#define FILE_PATTERN        "/* #@@# FILE #@@# "
#define FILE_PATTERN_END    " */\n"

static int test_srcfilter(options_test_t * opts) {
    log_t *             log = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int        nerrors = 0;

    LOG_INFO(log, ">>> SOURCE_FILTER tests");

#  ifndef APP_INCLUDE_SOURCE
    LOG_INFO(log, ">>> SOURCE_FILTER tests: APP_INCLUDE_SOURCE undefined, skipping tests");
#  else
    char                tmpfile[PATH_MAX];
    char                cmd[PATH_MAX];
    int                 vlibsrc, sensorsrc;

    const char * const  files[] = {
        "@" BUILD_APPNAME, "",
        "LICENSE", "README.md", "src/main.c", "src/test.c", "version.h", "build.h",
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
        "Makefile", "config.make",
        "@vlib", "ext/vlib/",
        "src/term.c",
        "src/avltree.c",
        "src/log.c",
        "src/logpool.c",
        "src/options.c",
        NULL
    };

    void * ctx = NULL;
    vlibsrc = (vlib_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    vlib_get_source(NULL, NULL, 0, &ctx);
    sensorsrc = (libvsensors_get_source(NULL, cmd, sizeof(cmd), &ctx) == sizeof(cmd));
    libvsensors_get_source(NULL, NULL, 0, &ctx);

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
    static const char * const projects[] = { BUILD_APPNAME, "vlib", "libvsensors", NULL };
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
    FILE * out = fdopen(fd, "w");

    if (out == NULL) {
        LOG_ERROR(log, "cannot create tmpfile '%s'", tmpfile);
        ++nerrors;
    } else

    for (const size_t * sizemin = sizes_minmax; *sizemin != SIZE_MAX; sizemin++) {
      for (size_t size = *(sizemin++); size <= *sizemin; size++) {
        size_t tmp_errors = 0;
        const char * prj = BUILD_APPNAME;
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
                    vlib_get_source,
                    libvsensors_get_source, NULL);
            fflush(out);

            /* build diff command */
            ret = VLIB_SNPRINTF(ret, cmd, sizeof(cmd),
                    "{ printf '" FILE_PATTERN "%s" FILE_PATTERN_END "'; "
                    "cat '%s%s'; echo; } | ", pattern, prjpath, *file);

            if (size >= min_buffersz && log->level >= LOG_LVL_VERBOSE) {
                snprintf(cmd + ret, sizeof(cmd) - ret, "diff -ru \"%s\" - 1>&2", tmpfile);//FIXME log->out
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
                if (ret != 0) {
                    ++nerrors;
                }
            }
        }
        if (size < min_buffersz && tmp_errors == 0) {
            LOG_WARN(log, "error: no error with bufsz(%zu) < min_bufsz(%zu)",
                     size, min_buffersz);
            //FIXME ++nerrors;
        }
      }
    }
    fclose(out);
    unlink(tmpfile);
    opt_set_source_filter_bufsz(0);

#  endif /* ifndef APP_INCLUDE_SOURCE */

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST LOG POOL *************** */
static int test_logpool(options_test_t * opts) {
    log_t *         log         = logpool_getlog(opts->logs, "tests", LPG_TRUEPREFIX);
    unsigned int    nerrors     = 0;
    log_t           log_tpl     = { log->level,
                                    LOG_FLAG_DEFAULT | LOGPOOL_FLAG_TEMPLATE | LOG_FLAG_PID,
                                    log->out, NULL };
    logpool_t *     logpool     = NULL;
    log_t *         testlog;

    LOG_INFO(log, ">>> LOG POOL tests");

    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(opts->logs));

    if ((logpool = logpool_create()) == NULL) {
        ++nerrors;
        LOG_ERROR(log, "error: logpool_create() = NULL");
    }
    logpool_add(logpool, &log_tpl, NULL);
    log_tpl.prefix = "*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_PID | LOG_FLAG_ABS_TIME;
    logpool_add(logpool, &log_tpl, NULL);
    log_tpl.prefix = "tests/*";
    log_tpl.flags = LOG_FLAG_LEVEL | LOG_FLAG_TID | LOG_FLAG_ABS_TIME;
    logpool_add(logpool, &log_tpl, NULL);

    const char * const prefixes[] = {
        "vsensorsdemo", "test", "tests", "tests/avltree", NULL
    };

    int flags[] = { LPG_NONE, LPG_NODEFAULT, LPG_TRUEPREFIX, LPG_NODEFAULT, INT_MAX };
    for (int * flag = flags; *flag != INT_MAX; flag++) {
        for (const char * const * prefix = prefixes; *prefix; prefix++) {
            testlog = logpool_getlog(logpool, *prefix, *flag);
            LOG_INFO(testlog, "CHECK LOG <%-20s> getflg:%d ptr:%p pref:%s",
                     *prefix, *flag, (void *) testlog, testlog ? testlog->prefix : "<>");
        }
    }
    LOG_INFO(log, "LOGPOOL MEMORY SIZE = %zu", logpool_memorysize(logpool));
    logpool_free(logpool);

    LOG_INFO(log, "<- %s(): ending with %u error(s).\n", __func__, nerrors);
    return nerrors;
}

/* *************** TEST MAIN FUNC *************** */

int test(int argc, const char *const* argv, unsigned int test_mode, logpool_t ** logpool) {
    options_test_t  options_test    = { .flags = 0, .test_mode = test_mode,
                                        .logs = logpool ? *logpool : NULL};
    log_t *         log             = logpool_getlog(options_test.logs, "tests", LPG_TRUEPREFIX);
    unsigned int    errors = 0;
    char const **   test_argv = NULL;

    LOG_INFO(log, ">>> TEST MODE: 0x%x\n", test_mode);

    if ((test_argv = malloc(sizeof(*test_argv) * argc)) != NULL) {
        test_argv[0] = BUILD_APPNAME;
        for (int i = 1; i < argc; ++i) {
            test_argv[i] = argv[i];
        }
        argv = test_argv;
    }

    /* Manage test program options and test parse options*/
    options_test.out = log->out;
    if ((test_mode & (1 << TEST_options)) != 0)
        errors += !OPT_IS_EXIT_OK(test_options(argc, argv, &options_test));

    /* opt_usage */
    if ((test_mode & ((1 << TEST_optusage) | (1 << TEST_optusage_big))) != 0)
        errors += test_optusage(argc, argv, &options_test);

    /* opt_usage stdout */
    if ((test_mode & (1 << TEST_optusage_stdout)) != 0)
        errors += test_optusage_stdout(argc, argv, &options_test);

    /* sizeof */
    if ((test_mode & (1 << TEST_sizeof)) != 0)
        errors += test_sizeof(&options_test);

    /* ascii */
    if ((test_mode & (1 << TEST_ascii)) != 0)
        errors += test_ascii(&options_test);

    /* colors */
    if ((test_mode & (1 << TEST_color)) != 0)
        errors += test_colors(&options_test);

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
    if ((test_mode & ((1 << TEST_tree) | (1 << TEST_bigtree))) != 0)
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

    /* Test vlib vdecode_buffer */
    if ((test_mode & (1 << TEST_bufdecode)) != 0)
        errors += test_bufdecode(&options_test);

    /* Test vlib source filter */
    if ((test_mode & (1 << TEST_srcfilter)) != 0)
        errors += test_srcfilter(&options_test);

    /* Test vlib log pool */
    if ((test_mode & (1 << TEST_logpool)) != 0)
        errors += test_logpool(&options_test);

    /* Test vlib thread functions */
    if ((test_mode & (1 << TEST_vthread)) != 0)
        errors += test_thread(&options_test);

    /* Test Log in multiple threads */
    if ((test_mode & (1 << TEST_log)) != 0)
        errors += test_log_thread(&options_test);

    /* ***************************************************************** */
    LOG_INFO(log, "<<< END of Tests : %u error(s).\n", errors);

    if (test_argv != NULL) {
        free(test_argv);
    }

    /* just update logpool, don't free it: it will be done by caller */
    if (logpool) {
        *logpool = options_test.logs;
    }

    return -errors;
}
#endif /* ! ifdef _TEST */

