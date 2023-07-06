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

#include "vlib/util.h"
#include "vlib/options.h"
#include "vlib/logpool.h"
#include "vlib/term.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

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

/* *************** TEST OPTIONS *************** */

int test_options_init(int argc, const char *const* argv, options_test_t * opts) {
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, test_version_string(),
                                                      opts);

    opt_config_test.flags |= OPT_FLAG_SILENT;
    return opt_parse_options(&opt_config_test);
}

#define OPTUSAGE_PIPE_END_LINE  "#$$^& TEST_OPT_USAGE_END &^$$#" /* no EOL here! */

typedef struct {
    opt_config_t *  opt_config;
    const char *    filter;
    int             ret;
} optusage_data_t;

static void * optusage_run(void * vdata) {
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
void * test_optusage(void * vdata) {
    options_test_t *opts = (options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTUSAGE");
    log_t *         log = test != NULL ? test->log : NULL;

    int             argc = opts->argc;
    const char*const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                            s_opt_desc_test, test_version_string(), opts);
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

void * test_options(void * vdata) {
    options_test_t *opts = (options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTIONS");
    log_t *         log = test != NULL ? test->log : NULL;

    int             argc = opts->argc;
    const char*const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                      s_opt_desc_test, test_version_string(), opts);
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

void * test_optusage_stdout(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "OPTUSAGE_STDOUT");
    log_t *         log = test != NULL ? test->log : NULL;
    int             argc = opts->argc;
    const char *const* argv = opts->argv;
    opt_config_t    opt_config_test = OPT_INITIALIZER(argc, argv, parse_option_test,
                                                s_opt_desc_test, test_version_string(), (void*)opts);
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

#endif /* ! ifdef _TEST */

