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

#include "version.h"
#include "test_private.h"

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
    size_t in_rem = inbufsz;
    unsigned int total = 0;
    int n;

    /* decode and assemble in allbuffer */
    while ((n = vdecode_buffer(NULL, outbuf, outbufsz, &ctx, inbuf, in_rem)) >= 0) {
        memcpy(decoded_buffer + total, outbuf, n);
        total += n;
        in_rem = 0;
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
        /* outbufsz > 0: n must be -1, check whether decoded total is refbufsz */
        TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) returns -1, got %d", n == -1,
                    desc, outbufsz, n);

        if (refbuffer != NULL) {
            TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) total(%u) = ref(%zu)",
                        total == refbufsz, desc, outbufsz, total, refbufsz);

            if (total == refbufsz && total != 0) {
                /* compare refbuffer and decoded_buffer */
                TEST_CHECK2(test, "vdecode_buffer(%s,outbufsz:%zu) decoded = reference",
                    0 == memcmp(refbuffer, decoded_buffer, total), desc, outbufsz);
            }
        }
    }
    if (outbuf)
        memcpy(outbuf, decoded_buffer, total);
    return total;
}

void * test_bufdecode(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "VDECODE_BUF");
    unsigned        n;
    char            buffer[16384], buffer2[16384];
    char            refbuffer[16384];
    unsigned        bufszs[] = { 0, 1, 2, 3, 8, 16, 128, 3000 };
    log_t *         log = test != NULL ? test->log : NULL;
    int             ret;

#  if ! CONFIG_ZLIB
    LOG_INFO(log, "warning: no zlib on this system");
#  endif

    for (unsigned n_bufsz = 0; n_bufsz < sizeof(bufszs) / sizeof(*bufszs); n_bufsz++) {
        const unsigned bufsz = bufszs[n_bufsz];
        unsigned int refbufsz;

        /* NULL or 0 size inbuffer */
        LOG_VERBOSE(log, "* NULL inbuf (outbufsz:%u)", bufsz);
        TEST_CHECK2(test, "test_onedecode NULL buffer (outbufsz:%u)",
                    0 == test_one_bufdecode(NULL, 198, buffer, bufsz, refbuffer, 0,
                                           "inbuf=NULL", test), bufsz);

        LOG_VERBOSE(log, "* inbufsz=0 (outbufsz:%u)", bufsz);

        TEST_CHECK2(test, "raw buf inbufsz=0 (outbufsz:%u)",
                    0 == test_one_bufdecode((const char *) rawbuf, 0,
                                            buffer, bufsz, refbuffer, 0, "inbufsz=0", test), bufsz);

        /* ZLIB */
        LOG_VERBOSE(log, "* ZLIB (outbufsz:%u)", bufsz);
        str0cpy(refbuffer, "1\n", sizeof(refbuffer));
        refbufsz = 2;
        TEST_CHECK2(test, "ZLIB (outbufsz:%u)",
            0 <= test_one_bufdecode((const char *) zlibbuf, sizeof(zlibbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "zlib", test), bufsz);

        if (bufsz>3) { // TODO NOT WORKING with inbufsz <=3
            LOG_VERBOSE(log, "* ZLIB encode (outbufsz:%u)", bufsz);
            refbufsz = VLIB_SNPRINTF(ret, refbuffer, sizeof(refbuffer), "%s1234567ABCDED\n000\nAAA", VDECODEBUF_ZLIBENC_MAGIC);

            TEST_CHECK2(test, "ZLIB encode (outbufsz:%u, ret:%d)",
                    0 <= (ret = test_one_bufdecode(refbuffer, refbufsz, buffer, bufsz,
                                            NULL, 0, "zlib_encode", test)), bufsz, ret);

            TEST_CHECK2(test, "ZLIB decode (outbufsz:%u)",
                0 <= test_one_bufdecode(buffer, ret < 0 ? 0 : (size_t) ret, buffer2, bufsz,
                            refbuffer + PTR_COUNT(VDECODEBUF_ZLIBENC_MAGIC) - 1,
                            refbufsz - (PTR_COUNT(VDECODEBUF_ZLIBENC_MAGIC) - 1), "zlib_decode", test), bufsz);
        }

        /* RAW with MAGIC */
        LOG_VERBOSE(log, "* RAW (outbufsz:%u)", bufsz);
        for (n = 4; n < sizeof(rawbuf) / sizeof(char); n++) {
            refbuffer[n - 4] = rawbuf[n];
        }
        refbufsz = n - 4;
        TEST_CHECK2(test, "RAW magic (outbufsz:%u)",
            0 <= test_one_bufdecode(rawbuf, sizeof(rawbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "raw", test), bufsz);

        /* RAW WITHOUT MAGIC */
        LOG_VERBOSE(log, "* RAW without magic (outbufsz:%u)", bufsz);
        for (n = 0; n < sizeof(rawbuf2) / sizeof(char); n++) {
            refbuffer[n] = rawbuf2[n];
        }
        refbufsz = n;
        TEST_CHECK2(test, "RAW without magic (outbufsz:%u)",
            0 <= test_one_bufdecode(rawbuf2, sizeof(rawbuf2) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "raw2", test), bufsz);

        /* STRTAB */
        LOG_VERBOSE(log, "* STRTAB (outbufsz:%u)", bufsz);
        refbufsz = 0;
        for (const char *const* pstr = strtabbuf+1; *pstr; pstr++) {
            size_t n = str0cpy(refbuffer + refbufsz, *pstr, sizeof(refbuffer) - refbufsz);
            refbufsz += n;
        }
        TEST_CHECK2(test, "STRTAB (outbufsz:%u)",
            0 <= test_one_bufdecode((const char *) strtabbuf, sizeof(strtabbuf) / sizeof(char),
                                    buffer, bufsz, refbuffer, refbufsz, "strtab", test), bufsz);
    }

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

