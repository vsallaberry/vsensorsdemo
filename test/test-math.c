/*
 * Copyright (C) 2020 Vincent Sallaberry
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
/* ** TESTS for vlib math functions *****************************************/
#ifndef _TEST
extern int ___nothing___; /* empty */
#else
#include "test_private.h"

#include "version.h"

/* ************************************************************************ */
/* MATH TESTS */
/* ************************************************************************ */
void * test_math(void * vdata) {
    const options_test_t *  opts    = (const options_test_t *) vdata;
    testgroup_t *           test    = TEST_START(opts->testpool, "MATH");
    /*log_t *               log     = test == NULL ? NULL : test->log;*/
    unsigned long           ret;

    static const long num[] = {
        /**/ 0, 0, /**/ 35, 75, /**/ -35, 75, /**/ 35, -75,
        /**/ -35, -75, /**/ -3, 0, /**/ 0, -3,
        /**/ 29318307, 13,
    };
    static const unsigned long res[] = { 0, 5, 5, 5, 5, 3, 3, 1 };

    test->ok_loglevel = LOG_LVL_INFO;
    for (unsigned int i = 0; i < sizeof(num) / sizeof(*num); i+=2) {
        TEST_CHECK2(test, "pgcd(%ld,%ld) = %lu, got %lu",
                    (ret = pgcd(num[i], num[i+1])) == res[i/2],
                    num[i], num[i+1], res[i/2], ret);
    }

    return VOIDP(TEST_END(test));
}
#endif /* ! ifdef _TEST */
