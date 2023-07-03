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
#include "vlib/test.h"
#include "libvsensors/sensor.h"

#include "version.h"
#include "test_private.h"

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

void * test_sensor_value(void * vdata) {
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

#endif /* ! ifdef _TEST */

