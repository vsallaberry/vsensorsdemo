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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#include "vlib/util.h"
#include "vlib/account.h"
#include "vlib/logpool.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST ACCOUNT *************** */
#define TEST_ACC_USER "nobody"
#define TEST_ACC_GROUP "nogroup"
void * test_account(void * vdata) {
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

#endif /* ! ifdef _TEST */

