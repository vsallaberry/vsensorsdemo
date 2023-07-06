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
#include <sys/stat.h>
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
void *          test_tests(void * vdata);
static void *   test_optusage(void * vdata);
void *          test_sizeof(void * vdata);
static void *   test_ascii(void * vdata);
static void *   test_colors(void * vdata);
static void *   test_bench(void * vdata);
void *          test_math(void * vdata);
void *          test_list(void * vdata);
void *          test_hash(void * vdata);
void *          test_rbuf(void * vdata);
void *          test_avltree(void * vdata);
void *          test_sensor_value(void * vdata);
void *          test_sensor_plugin(void * vdata);
static void *   test_account(void * vdata);
static void *   test_bufdecode(void * vdata);
static void *   test_srcfilter(void * vdata);
void *          test_logpool(void * vdata);
void *          test_job(void * vdata);
void *          test_thread(void * vdata);
void *          test_log_thread(void * vdata);
static void *   test_optusage_stdout(void * vdata);

static const struct {
    const char *    name;
    void *          (*fun)(void*);
    unsigned int    mask; /* set of tests preventing this one to run */
} s_testconfig[] = { // same order as enum test_private.h/test_mode_t
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

/* create a temp directory if not done already, and return its path */
static char                 s_tests_tmpdir[PATH_MAX * 2] = { 0, };
static const char * const   s_tests_tmpdir_base = "/tmp";

const char * test_tmpdir() {
    if (*s_tests_tmpdir == 0) {
        int ret;
        const char * base = getenv("VLIB_TEST_TMPDIR_BASE");
        if (base == NULL)
            base = s_tests_tmpdir_base;
        snprintf(s_tests_tmpdir, sizeof(s_tests_tmpdir), "%s/%s-tests.XXXXXX", base, BUILD_APPNAME);
        ret = mkdir(base, S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret == 0)
            chmod(base, S_IRWXU | S_IRWXG | S_IRWXO);
        if ((ret != 0 && errno != EEXIST)
        ||  mkdtemp(s_tests_tmpdir) == NULL) {
            LOG_WARN(NULL, "cannot create tmpdir %s: %s. Trying in current dir...", s_tests_tmpdir, strerror(errno));
            snprintf(s_tests_tmpdir, sizeof(s_tests_tmpdir), "tmp-%s-tests", BUILD_APPNAME);
            if (mkdir(s_tests_tmpdir, S_IRWXU) != 0 && errno != EEXIST)
                return NULL;
        }
    }
    return s_tests_tmpdir;
}

int test_clean_tmpdir() {
    if (*s_tests_tmpdir != 0) {
        if (rmdir(s_tests_tmpdir) == 0) {
            *s_tests_tmpdir = 0;
            /*int errno_save = errno;
            rmdir(s_tests_tmpdir_base);
            errno = errno_save;*/
        } else {
            return -1;
        }
    }
    return 0;
}

/* used by slist, avltree and other tests */
int intcmp(const void * a, const void *b) {
    return (long)a - (long)b;
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
    const char *    tmpdir;
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
    if ((tmpdir = test_tmpdir()) == NULL) {
        LOG_WARN(log, "cannot create tmpdir: %s", strerror(errno));
    } else {
        LOG_INFO(log, "created tmpdir '%s'.", tmpdir);
    }

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

    if (test_clean_tmpdir() != 0) {
        LOG_WARN(log, "cannot clean tmpdir %s: %s", tmpdir ? tmpdir : "(null)", strerror(errno));
    }

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

