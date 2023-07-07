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

#include "vlib/util.h"
#include "vlib/logpool.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** ASCII and TEST LOG BUFFER *************** */

void * test_ascii(void * vdata) {
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

#endif /* ! ifdef _TEST */

