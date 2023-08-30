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
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <fnmatch.h>
#include <limits.h>

#include "vlib/util.h"
#include "vlib/logpool.h"
#include "vlib/test.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST SOURCE FILTER *************** */

void opt_set_source_filter_bufsz(size_t bufsz);
int vsensorsdemo_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx);

#define FILE_PATTERN        "/* #@@# FILE #@@# "
#define FILE_PATTERN_END    " */\n"

void * test_srcfilter(void * vdata) {
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

    snprintf(tmpfile, sizeof(tmpfile), "%s/tmp_srcfilter.XXXXXXXX", test_tmpdir());
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

                    if (ftruncate(fd, 0) < 0) {
                        LOG_WARN(log, "cannot truncate tmpfile [bufsz:%zu file:%s]",
                                 size, pattern);
                    }
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

#endif /* ! ifdef _TEST */

