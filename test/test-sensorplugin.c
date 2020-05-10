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
/* ** TESTS for libvsensors plugins ****************************************/
#ifndef _TEST
extern int ___nothing___; /* empty */
#else
#include "libvsensors/sensor.h"

#include "test_private.h"

/* *************** TEST SENSOR_PLUGIN *************** */
#define TEST_SENSOR_PLUGIN_NB_DESC 3
static sensor_status_t testsensor_plugin_init(sensor_family_t * family) {
    (void)family;
    return SENSOR_SUCCESS;
}
static sensor_status_t testsensor_plugin_free(sensor_family_t * family) {
    if (family->priv != NULL) {
        SLIST_FOREACH_DATA(family->priv, desc, sensor_desc_t *)
            if (desc->label != NULL) free((void*)(desc->label));
        slist_free(family->priv, free);
    }
    return SENSOR_SUCCESS;
}
static slist_t * testsensor_plugin_list(sensor_family_t * family) {
    slist_t * list = NULL;
    for (int i = 0; i < TEST_SENSOR_PLUGIN_NB_DESC; ++i) {
        char label[20];
        sensor_desc_t * desc;
        if ((desc = calloc(1, sizeof(sensor_desc_t))) == NULL) {
            break ;
        }
        snprintf(label, sizeof(label), "sensor%02d",
                 i);//i % (TEST_SENSOR_PLUGIN_NB_DESC-1));
        //if (i >= TEST_SENSOR_PLUGIN_NB_DESC -1)
        //    *label = 'S'; // FIXME
        desc->key = (void*)((unsigned long)i);
        desc->label = strdup(label);
        desc->type = SENSOR_VALUE_ULONG;
        desc->family = family;
        family->priv = slist_prepend((slist_t *)family->priv, desc);
        list = slist_prepend((slist_t *)list, desc);
    }
    return list;
}
static sensor_status_t testsensor_plugin_update(sensor_sample_t * sensor,
                                                const struct timeval * now) {
    sensor->value.data.ul = (unsigned long)((now->tv_sec * 1000UL + now->tv_usec / 1000UL) << 8)
                            + (unsigned long)(sensor->desc->key);
    return SENSOR_SUCCESS;
}
static const sensor_family_info_t plugin1_info = {
    .name = "plugin1",
    .init = NULL,
    .free = testsensor_plugin_free,
    .list = testsensor_plugin_list,
    .update = testsensor_plugin_update,
    .notify = NULL,
    .write = NULL
};
static const sensor_family_info_t plugin2_info = {
    .name = "plugin2",
    .init = testsensor_plugin_init,
    .free = testsensor_plugin_free,
    .list = testsensor_plugin_list,
    .update = testsensor_plugin_update,
    .notify = NULL,
    .write = NULL
};
static const sensor_family_info_t plugin3_info = {
    .name = "plugin2",
    .init = testsensor_plugin_init,
    .free = testsensor_plugin_free,
    .list = testsensor_plugin_list,
    .update = testsensor_plugin_update,
    .notify = NULL,
    .write = NULL
};
static const sensor_family_info_t plugin4_info = {
    .name = "plug",
    .init = testsensor_plugin_init,
    .free = testsensor_plugin_free,
    .list = testsensor_plugin_list,
    .update = testsensor_plugin_update,
    .notify = NULL,
    .write = NULL
};
typedef struct {
    testgroup_t *           test;
    log_t *                 log;
    const options_test_t *  opts;
    sensor_ctx_t *          sctx;
    const slist_t *         sensors;
    const slist_t *         watchs;
    unsigned int            list_len;
    unsigned int            watch_len;
    unsigned int            timer_ms;
} testsensor_data_t;
static int testsensors_update(testsensor_data_t * d) {
    testgroup_t *   test = d->test;
    log_t *         log = d->log;
    struct timeval  now = { .tv_sec = 0, .tv_usec = 0 };
    struct timeval  timer = { .tv_sec = d->timer_ms / 1000,
                              .tv_usec = (d->timer_ms % 1000) * 1000 };
    slist_t *       updates;
    unsigned int    i;

    /* first update_get */
    updates = sensor_update_get(d->sctx, &now);
    TEST_CHECK2(test, "check updates(first): expected %u, got %u",
                (i = slist_length(updates)) == d->watch_len, d->watch_len, i);
    sensor_update_free(updates);

    /* update_get at the same now */
    updates = sensor_update_get(d->sctx, &now);
    TEST_CHECK2(test, "check updates(same_now): expected %u, got %u",
                (i = slist_length(updates)) == 0, 0U, i);
    sensor_update_free(updates);

    /* update_get after d->timer_ms */
    timeradd(&now, &timer, &now);
    updates = sensor_update_get(d->sctx, &now);
    TEST_CHECK2(test, "check updates(now+=timer): expected %u, got %u",
                (i = slist_length(updates)) == d->watch_len, d->watch_len, i);
    sensor_update_free(updates);

    /* manually check updates after d->timer_ms */
    timeradd(&now, &timer, &now);
    sensor_lock(d->sctx, SENSOR_LOCK_READ);
    i = 0;
    SLISTC_FOREACH_DATA(d->watchs, sensor, sensor_sample_t *) {
        if (sensor_update_check(sensor, &now) == SENSOR_UPDATED)
            ++i;
        else
            LOG_INFO(log, "%s/%s NOT updated",
                     sensor->desc->family->info->name, sensor->desc->label);
    }
    sensor_unlock(d->sctx);
    TEST_CHECK2(test, "check manual updates(now+=timer): expected %u, got %u",
                i == d->watch_len, d->watch_len, i);

    return 0;
}
static sensor_status_t test_sensor_desc_visit(const sensor_desc_t * desc, void * vdata) {
    slist_t ** plist = (slist_t **) vdata;
    *plist = slist_prepend(*plist, (void *) desc);
    return SENSOR_SUCCESS;
}
static sensor_status_t test_sensor_sample_visit(sensor_sample_t * sample, void * vdata) {
    slist_t ** plist = (slist_t **) vdata;
    *plist = slist_prepend(*plist, sample);
    return SENSOR_SUCCESS;
}

void * test_sensor_plugin(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *       test = TEST_START(opts->testpool, "SENSOR_PLUGIN");
    log_t *             log = test != NULL ? test->log : NULL;
    testsensor_data_t   d = { .test = test, .log = log, .opts = opts, .timer_ms = 100 };
    sensor_watch_t      watch = SENSOR_WATCH_INITIALIZER(d.timer_ms, NULL);
    sensor_watch_t      watch2 = SENSOR_WATCH_INITIALIZER(d.timer_ms+253, NULL);
    unsigned int        i;
    slist_t *           matchs, * familylist;
    avltree_t *         families = avltree_create(AFL_DEFAULT | AFL_INSERT_IGNDOUBLE,
                                                  hash_ptrcmp, NULL);
    BENCHS_DECL         (tm_bench, cpu_bench);

    /* ************************************************************ */
    /* INIT and FREE just after */
    LOG_INFO(log, "* 1. sensor_init() followed by sensor_free()");
    d.watch_len = TEST_SENSOR_PLUGIN_NB_DESC;
    TEST_CHECK(test, "sensor_init(1)", (d.sctx = sensor_init(opts->logs)) != NULL);
    TEST_CHECK(test, "sensor_family_register(1:after-init)",
               sensor_family_register(d.sctx, &plugin1_info) == SENSOR_SUCCESS);
    TEST_CHECK(test, "sensor_free(1)", sensor_free(d.sctx) == SENSOR_SUCCESS);

    /* ************************************************************ */
    /* register after list_get */
    LOG_INFO(log, "* 2. sensor_family_register() after sensor_list_get()");
    d.watch_len = TEST_SENSOR_PLUGIN_NB_DESC;
    TEST_CHECK(test, "(2) sensor_init()", (d.sctx = sensor_init(opts->logs)) != NULL);
    TEST_CHECK(test, "(2) sensor_list_get()", (d.sensors = sensor_list_get(d.sctx)) != NULL);
    d.list_len = slist_length(d.sensors);
    TEST_CHECK(test, "(2) sensor_family_register(after-list_get)",
               sensor_family_register(d.sctx, &plugin1_info) == SENSOR_SUCCESS);

    TEST_CHECK2(test, "(2) %d sensors added to list()",
            d.list_len + TEST_SENSOR_PLUGIN_NB_DESC == slist_length(d.sensors),
            TEST_SENSOR_PLUGIN_NB_DESC);
    TEST_CHECK(test, "(2) sensor_watch_add()",
               (sensor_watch_add(d.sctx, "plugin[1-9]/*",
                                 SSF_CASEFOLD | SSF_DEFAULT, &watch)) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(2) sensor_watch_list_get()",
               (d.watchs = sensor_watch_list_get(d.sctx)) != NULL);
    TEST_CHECK2(test, "(2) %u sensors watched(), got %u",
                d.watch_len == (i = slist_length(d.watchs)), d.watch_len, i);
    TEST_CHECK(test, "(2) sensor_update()", testsensors_update(&d) == 0);
    usleep(20000); /* some delay then stop background jobs */
    TEST_CHECK(test, "(2) sensor_free()", sensor_free(d.sctx) == SENSOR_SUCCESS);

    /* ************************************************************ */
    /* register before list_get */
    LOG_INFO(log, "* 3. sensor_family_register() before sensor_list_get()");
    TEST_CHECK(test, "(3) sensor_init()", (d.sctx = sensor_init(opts->logs)) != NULL);
    TEST_CHECK(test, "(3) sensor_list_get()", (d.sensors = sensor_list_get(d.sctx)) != NULL);
    TEST_CHECK(test, "(3) sensor_family_register(before-list_get)",
               sensor_family_register(d.sctx, &plugin1_info) == SENSOR_SUCCESS);
    TEST_CHECK2(test, "(3) %d sensors added to list",
                d.list_len + TEST_SENSOR_PLUGIN_NB_DESC == slist_length(d.sensors),
                TEST_SENSOR_PLUGIN_NB_DESC);
    TEST_CHECK(test, "(3) sensor_watch_add()",
               (sensor_watch_add(d.sctx, "plugin[1-9]/*",
                                 SSF_DEFAULT | SSF_CASEFOLD, &watch)) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(3) sensor_watch_list_get()",
               (d.watchs = sensor_watch_list_get(d.sctx)) != NULL);
    TEST_CHECK2(test, "(3) %u sensors watched(), got %u",
                d.watch_len == (i = slist_length(d.watchs)), d.watch_len, i);
    TEST_CHECK(test, "(3) sensor_update()", testsensors_update(&d) == 0);
    TEST_CHECK(test, "(3) sensor_free()", sensor_free(d.sctx) == SENSOR_SUCCESS);

    /* ************************************************************ */
    /* register 2 plugins 1 before list_get, 1 after */
    LOG_INFO(log, "* 4. register 1 plugin before sensor_list_get(), 1 after");
    d.watch_len = TEST_SENSOR_PLUGIN_NB_DESC * 2;
    TEST_CHECK(test, "(4) sensor_init()", (d.sctx = sensor_init(opts->logs)) != NULL);
    TEST_CHECK(test, "(4) sensor_family_register(before-list_get)",
               sensor_family_register(d.sctx, &plugin1_info) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(4) sensor_list_get()", (d.sensors = sensor_list_get(d.sctx)) != NULL);
    TEST_CHECK(test, "(4) sensor_family_register(before-list_get)",
               sensor_family_register(d.sctx, &plugin2_info) == SENSOR_SUCCESS);
    TEST_CHECK2(test, "(4) %d sensors added to list",
                d.list_len + TEST_SENSOR_PLUGIN_NB_DESC * 2 == slist_length(d.sensors),
                TEST_SENSOR_PLUGIN_NB_DESC * 2);
    d.list_len = slist_length(d.sensors);
    TEST_CHECK(test, "(4) sensor_watch_add()",
               (sensor_watch_add(d.sctx, "plugin[1-9]/*",
                                 SSF_DEFAULT | SSF_CASEFOLD, &watch)) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(4) sensor_watch_list_get()",
               (d.watchs = sensor_watch_list_get(d.sctx)) != NULL);
    TEST_CHECK2(test, "(4) %u sensors watched(), got %u",
                d.watch_len == (i = slist_length(d.watchs)), d.watch_len, i);

    TEST_CHECK(test, "(4) sensor_update()", testsensors_update(&d) == 0);

    /* ************************************************************ */
    /* register plugins 3 and 4 */
    LOG_INFO(log, "* 5. register 2 more plugins with name conflists");
    TEST_CHECK(test, "(5) sensor_family_register(before-list_get)",
               sensor_family_register(d.sctx, &plugin3_info) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(5) sensor_family_register(before-list_get)",
               sensor_family_register(d.sctx, &plugin4_info) == SENSOR_SUCCESS);
    TEST_CHECK(test, "(5) sensor_list_get()", (d.sensors = sensor_list_get(d.sctx)) != NULL);
    i = slist_length(d.sensors);
    TEST_CHECK2(test, "(5) %d sensors added to list, got %u",
                i == d.list_len + TEST_SENSOR_PLUGIN_NB_DESC*2,
                TEST_SENSOR_PLUGIN_NB_DESC*2, i - d.list_len);
    d.list_len = i;
    TEST_CHECK(test, "(5) remove all watches", sensor_watch_del(d.sctx, "*", SSF_DEFAULT)
                                               == SENSOR_SUCCESS);

    /* ************************************************************ */
    /* error cases */
    LOG_INFO(log, "* 6. sensor*_add/del/find error cases");
    TEST_CHECK(test, "(6) sensor_watch_add NULL ctx",
               sensor_watch_add(NULL, "*", SSF_DEFAULT, &watch) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_add NULL pattern",
               sensor_watch_add(d.sctx, NULL, SSF_DEFAULT, &watch) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_add empty pattern",
               sensor_watch_add(d.sctx, "", SSF_DEFAULT, &watch) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_add NULL watch",
               sensor_watch_add(d.sctx, "*", SSF_DEFAULT, NULL) == SENSOR_ERROR);

    TEST_CHECK(test, "(6) sensor_find NULL ctx",
               sensor_find(NULL, "*", SSF_DEFAULT, NULL) == NULL);
    TEST_CHECK(test, "(6) sensor_find NULL pattern",
               sensor_find(d.sctx, NULL, SSF_DEFAULT, NULL) == NULL);
    TEST_CHECK(test, "(6) sensor_find empty pattern",
               sensor_find(d.sctx, "", SSF_DEFAULT, NULL) == NULL);

    TEST_CHECK(test, "(6) sensor_visit NULL ctx",
               sensor_visit(NULL, "*", SSF_DEFAULT,
                            test_sensor_desc_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_visit NULL pattern",
               sensor_visit(d.sctx, NULL, SSF_DEFAULT,
                            test_sensor_desc_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_visit empty pattern",
               sensor_visit(d.sctx, "", SSF_DEFAULT,
                            test_sensor_desc_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_visit NULL visit",
               sensor_visit(d.sctx, "*", SSF_DEFAULT, NULL, NULL) == SENSOR_ERROR);

    TEST_CHECK(test, "(6) sensor_watch_find NULL ctx",
               sensor_watch_find(NULL, "*", SSF_DEFAULT, NULL) == NULL);
    TEST_CHECK(test, "(6) sensor_watch_find NULL pattern",
               sensor_watch_find(d.sctx, NULL, SSF_DEFAULT, NULL) == NULL);
    TEST_CHECK(test, "(6) sensor_watch_find empty pattern",
               sensor_watch_find(d.sctx, "", SSF_DEFAULT, NULL) == NULL);

    TEST_CHECK(test, "(6) sensor_watch_visit NULL ctx",
               sensor_watch_visit(NULL, "*", SSF_DEFAULT,
                                 test_sensor_sample_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_visit NULL pattern",
               sensor_watch_visit(d.sctx, NULL, SSF_DEFAULT,
                                 test_sensor_sample_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_visit empty pattern",
               sensor_watch_visit(d.sctx, "", SSF_DEFAULT,
                                 test_sensor_sample_visit, NULL) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_visit NULL visit",
               sensor_watch_visit(d.sctx, "*", SSF_DEFAULT, NULL, NULL) == SENSOR_ERROR);

    TEST_CHECK(test, "(6) sensor_watch_del NULL ctx",
               sensor_watch_del(NULL, "*", SSF_DEFAULT) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_del NULL pattern",
               sensor_watch_del(d.sctx, NULL, SSF_DEFAULT) == SENSOR_ERROR);
    TEST_CHECK(test, "(6) sensor_watch_add empty pattern",
               sensor_watch_del(d.sctx, "", SSF_DEFAULT) == SENSOR_ERROR);

    d.watch_len = slist_length((d.watchs = sensor_watch_list_get(d.sctx)));
    TEST_CHECK2(test, "(6) 0 sensors in watch list, got %u", d.watch_len == 0, d.watch_len);

    /* ************************************************************ */
    /* find/visit tests */
    LOG_INFO(log, "* 7. sensor{,_watch}_{find,visit}()");

    TEST_CHECK(test, "add plug/sensor00", sensor_watch_add(d.sctx, "plug/sensor00",
                                                           SSF_DEFAULT, &watch)
                                            == SENSOR_SUCCESS);
    d.watch_len = slist_length((d.watchs = sensor_watch_list_get(d.sctx)));
    TEST_CHECK2(test, "1 sensors in watch list, got %u", d.watch_len == 1, d.watch_len);
    TEST_CHECK(test, "add plug/*", sensor_watch_add(d.sctx, "plug/*", SSF_DEFAULT, &watch)
                                   == SENSOR_SUCCESS);
    d.watch_len = slist_length((d.watchs = sensor_watch_list_get(d.sctx)));
    TEST_CHECK2(test, "%d sensors in watch list, got %u",
                d.watch_len == TEST_SENSOR_PLUGIN_NB_DESC,
                TEST_SENSOR_PLUGIN_NB_DESC, d.watch_len);

    TEST_CHECK(test, "sensor_find NO PATTERN",
               sensor_find(d.sctx, "*", SSF_NOPATTERN, NULL) == NULL);
    TEST_CHECK(test, "sensor_find NO PATTERN CASE_INSENSITIVE",
               sensor_find(d.sctx, "*", SSF_NOPATTERN | SSF_CASEFOLD, NULL) == NULL);
    TEST_CHECK(test, "sensor_find CASE-SENSITIVE",
               sensor_find(d.sctx, "Plug/sensor00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_find CASE-INSENSITIVE",
               sensor_find(d.sctx, "Plug/sensor00", SSF_CASEFOLD, NULL) != NULL);
    TEST_CHECK(test, "sensor_find PAT CASE-SENSITIVE",
               sensor_find(d.sctx, "Plug/*00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_find PAT CASE-INSENSITIVE",
               sensor_find(d.sctx, "Plug/*00", SSF_CASEFOLD, NULL) != NULL);
    TEST_CHECK(test, "sensor_find PAT2 CASE-SENSITIVE",
               sensor_find(d.sctx, "Plug*00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_find PAT2 CASE-INSENSITIVE",
               sensor_find(d.sctx, "Plug*00", SSF_CASEFOLD, NULL) != NULL);

    TEST_CHECK(test, "sensor_watch_find NO PATTERN",
               sensor_watch_find(d.sctx, "*", SSF_NOPATTERN, NULL) == NULL);
    TEST_CHECK(test, "sensor_watch_find NO PATTERN",
               sensor_watch_find(d.sctx, "*", SSF_NOPATTERN | SSF_CASEFOLD, NULL) == NULL);
    TEST_CHECK(test, "sensor_watch_find CASE-SENSITIVE",
               sensor_watch_find(d.sctx, "Plug/sensor00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_watch_find CASE-INSENSITIVE",
               sensor_watch_find(d.sctx, "Plug/sensor00", SSF_CASEFOLD, NULL) != NULL);
    TEST_CHECK(test, "sensor_watch_find PAT CASE-SENSITIVE",
               sensor_watch_find(d.sctx, "Plug/*00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_watch_find PAT CASE-INSENSITIVE",
               sensor_watch_find(d.sctx, "Plug/*00", SSF_CASEFOLD, NULL) != NULL);
    TEST_CHECK(test, "sensor_watch_find PAT2 CASE-SENSITIVE",
               sensor_watch_find(d.sctx, "Plug*00", SSF_NONE, NULL) == NULL);
    TEST_CHECK(test, "sensor_watch_find PAT2 CASE-INSENSITIVE",
               sensor_watch_find(d.sctx, "Plug*00", SSF_CASEFOLD, NULL) != NULL);

    /* get families */
    SLISTC_FOREACH_DATA(d.sensors, desc, sensor_desc_t *) {
        avltree_insert(families, desc->family);
    }
    familylist = avltree_to_slist(families, AVH_PREFIX);
    LOG_INFO(log, "%zu families (%u in list)",
             avltree_count(families), slist_length(familylist));

    LOG_INFO(log, "add all watches");

    TEST_CHECK(test, "wait loading FIXME", SENSOR_SUCCESS
                   == sensor_init_wait(d.sctx));

    TEST_CHECK(test, "add all watchs", SENSOR_SUCCESS
                   == sensor_watch_add_desc(d.sctx, NULL, SSF_DEFAULT, &watch));
    d.watchs = sensor_watch_list_get(d.sctx);
    d.sensors = sensor_list_get(d.sctx);
    d.watch_len = slist_length(d.watchs);

    /* check timers */
    LOG_INFO(log, "check timers");
    SLISTC_FOREACH_DATA(d.watchs, sample, sensor_sample_t *) {
        TEST_CHECK2(test, "%s/%s must have %d.%03ds timer, got %d.%03ds",
                    timercmp(&sample->watch->update_interval, &watch.update_interval, ==),
                    STR_CHECKNULL(sample->desc->family->info->name),
                    STR_CHECKNULL(sample->desc->label),
                    (int)watch.update_interval.tv_sec, (int)watch.update_interval.tv_usec/1000,
                    (int)sample->watch->update_interval.tv_sec,
                    (int)sample->watch->update_interval.tv_usec/1000);
    }

    LOG_INFO(log, "add all watches again and re-check timers");
    TEST_CHECK(test, "add all watchs one second time", SENSOR_SUCCESS
                   == sensor_watch_add_desc(d.sctx, NULL, SSF_DEFAULT, &watch));
    TEST_CHECK(test, "list unchanged",
               slist_length(sensor_watch_list_get(d.sctx)) == d.watch_len);

    TEST_CHECK(test, "add all watchs one more time", SENSOR_SUCCESS
                   == sensor_watch_add(d.sctx, "*", SSF_DEFAULT, &watch));
    TEST_CHECK(test, "list unchanged",
               slist_length(sensor_watch_list_get(d.sctx)) == d.watch_len);

    /* check timers */
    SLISTC_FOREACH_DATA(d.watchs, sample, sensor_sample_t *) {
        TEST_CHECK2(test, "%s/%s must have %d.%03ds timer, got %d.%03ds",
                    timercmp(&sample->watch->update_interval, &watch.update_interval, ==),
                    STR_CHECKNULL(sample->desc->family->info->name),
                    STR_CHECKNULL(sample->desc->label),
                    (int)watch.update_interval.tv_sec, (int)watch.update_interval.tv_usec/1000,
                    (int)sample->watch->update_interval.tv_sec,
                    (int)sample->watch->update_interval.tv_usec/1000);
    }

    /* update some timers */
    LOG_INFO(log, "update some timers and check");

    TEST_CHECK(test, "change plugin2 timers", SENSOR_SUCCESS
                   == sensor_watch_add(d.sctx, "plugin2*", SSF_DEFAULT, &watch2));
    d.watchs = sensor_watch_list_get(d.sctx);
    d.watch_len = slist_length(d.watchs);

    SLISTC_FOREACH_DATA(d.watchs, sample, sensor_sample_t *) {
        struct timeval * tv = fnmatch("plugin2*", sample->desc->family->info->name, FNM_CASEFOLD) == 0
                              ? &watch2.update_interval : &watch.update_interval;
        TEST_CHECK2(test, "%s/%s must have %d.%03ds timer, got %d.%03ds",
                    timercmp(&sample->watch->update_interval, tv, ==),
                    STR_CHECKNULL(sample->desc->family->info->name),
                    STR_CHECKNULL(sample->desc->label),
                    (int)tv->tv_sec, (int)tv->tv_usec/1000,
                    (int)sample->watch->update_interval.tv_sec,
                    (int)sample->watch->update_interval.tv_usec/1000);
    }

    static const char * findtype[] = { "find", "visit", NULL };
    for (unsigned int i = 0; i < 2; ++i) {
        long res, exp;

        /* *****************************************
         * check sensor_find()/sensor_visit()
         * *****************************************/

        /* one match */
        LOG_INFO(log, "* %u. a) sensor_%s() on each sensor", 7 + i * 2, findtype[i]);
        BENCHS_START(tm_bench, cpu_bench);
        SLISTC_FOREACH_DATA(d.sensors, desc, sensor_desc_t *) {
            char label[1024];
            int is_double;

            snprintf(label, sizeof(label) / sizeof(*label), "%s/%s",
                     desc->family->info->name, desc->label);

            /* plugin2 family name is double */
            is_double = (fnmatch("plugin2/sensor0[0-2]", label, 0) == 0);

            if (i == 0) {
                if (is_double == 0) {
                    TEST_CHECK2(test, "sensor_find(%s)",
                                sensor_find(d.sctx, label,
                                   SSF_DEFAULT & ~SSF_CASEFOLD, NULL) == desc, label);
                } else {
                    TEST_CHECK2(test, "sensor_find(%s)",
                                sensor_find(d.sctx, label,
                                   SSF_DEFAULT & ~SSF_CASEFOLD, NULL) != NULL, label);
                }
                matchs = NULL;
                TEST_CHECK2(test, "sensor_find(%s, list)",
                            sensor_find(d.sctx, label,
                                   SSF_DEFAULT & ~SSF_CASEFOLD, &matchs) != NULL, label);
                TEST_CHECK2(test, "sensor_find list is %s",
                    matchs != NULL
                    && ((is_double == 0 && matchs->next == NULL && matchs->data == desc)
                        || (is_double && matchs->next != NULL && matchs->next->next == NULL
                            && (matchs->data == desc || matchs->next->data == desc))), label);
                slist_free(matchs, NULL);
                matchs = NULL;
            } else {
                matchs = NULL;
                TEST_CHECK2(test, "sensor_visit(%s)",
                            sensor_visit(d.sctx, label,
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_desc_visit, &matchs) == SENSOR_SUCCESS, label);
                TEST_CHECK2(test, "sensor_visit list is %s",
                    matchs != NULL
                    && ((is_double == 0 && matchs->next == NULL && matchs->data == desc)
                        || (is_double && matchs->next != NULL && matchs->next->next == NULL
                            && (matchs->data == desc || matchs->next->data == desc))), label);
                slist_free(matchs, NULL);
                matchs = NULL;
            }

        }
        BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "        all sensor_%s() ", findtype[i]);

        /* all matches */
        LOG_INFO(log, "* %u. b) sensor_%s(* and */*)", 7 + i * 2, findtype[i]);
        matchs = NULL;
        if (i == 0) {
            TEST_CHECK(test, "sensor_find(*) not null",
                       sensor_find(d.sctx, "*", SSF_DEFAULT, &matchs) != NULL);
        } else {
            TEST_CHECK(test, "sensor_visit(*)",
                            sensor_visit(d.sctx, "*",
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_desc_visit, &matchs) == SENSOR_SUCCESS);
        }
        TEST_CHECK2(test, "sensor_%s(*) must give all sensors, got %ld, expected %ld",
                   (exp = slist_length(d.sensors)) == (res = slist_length(matchs)),
                   findtype[i], res, exp);
        SLISTC_FOREACH_DATA(d.sensors, desc, sensor_desc_t *) {
            TEST_CHECK2(test, "'%s/%s' is in results",
                        slist_find(matchs, desc, hash_ptrcmp) != NULL,
                        desc->family->info->name, STR_CHECKNULL(desc->label));
        }
        SLISTC_FOREACH_DATA(matchs, desc, sensor_desc_t *) {
            TEST_CHECK2(test, "'%s/%s' is in sensors",
                        slist_find(d.sensors, desc, hash_ptrcmp) != NULL,
                        desc->family->info->name, STR_CHECKNULL(desc->label));
        }
        slist_free(matchs, NULL);
        matchs = NULL;

        if (i == 0) {
            TEST_CHECK(test, "sensor_find(*/*) not null",
                       sensor_find(d.sctx, "*/*", SSF_DEFAULT, &matchs) != NULL);
        } else {
            TEST_CHECK(test, "sensor_visit(*/*)",
                       sensor_visit(d.sctx, "*/*",
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_desc_visit, &matchs) == SENSOR_SUCCESS);
        }
        TEST_CHECK2(test, "sensor_%s(*/*) must give all sensors, got %ld, expected %ld",
                   (exp = slist_length(d.sensors)) == (res = slist_length(matchs)),
                   findtype[i], res, exp);
        SLISTC_FOREACH_DATA(d.sensors, desc, sensor_desc_t *) {
            TEST_CHECK2(test, "'%s/%s' is in results",
                        slist_find(matchs, desc, hash_ptrcmp) != NULL,
                        desc->family->info->name, STR_CHECKNULL(desc->label));
        }
        slist_free(matchs, NULL);
        matchs = NULL;

        /* family matches */
        LOG_INFO(log, "* %u. c) sensor_%s(per family)", 7 + i * 2, findtype[i]);

        /* *****************************************
         * check sensor_watch_find()/sensor_watch_visit()
         * *****************************************/

        /* one match */
        LOG_INFO(log, "* %u. a) sensor_watch_%s() on each watch", 8 + i * 2, findtype[i]);
        BENCHS_START(tm_bench, cpu_bench);
        SLISTC_FOREACH_DATA(d.watchs, sample, sensor_sample_t *) {
            char label[1024];
            int is_double;

            snprintf(label, sizeof(label) / sizeof(*label), "%s/%s",
                     sample->desc->family->info->name, sample->desc->label);

            /* plugin2 family name is double */
            is_double = (fnmatch("plugin2/sensor0[0-2]", label, 0) == 0);

            if (i == 0) {
                if (is_double == 0) {
                    TEST_CHECK2(test, "sensor_watch_find(%s)",
                            sensor_watch_find(d.sctx, label,
                                              SSF_DEFAULT & ~SSF_CASEFOLD, NULL) == sample, label);
                } else {
                    TEST_CHECK2(test, "sensor_watch_find(%s)",
                        sensor_watch_find(d.sctx, label,
                                          SSF_DEFAULT & ~SSF_CASEFOLD, NULL) != NULL, label);
                }
                matchs = NULL;
                TEST_CHECK2(test, "sensor_watch_find(%s, list)",
                            sensor_watch_find(d.sctx, label,
                                          SSF_DEFAULT & ~SSF_CASEFOLD, &matchs) != NULL, label);
                TEST_CHECK2(test, "sensor_watch_find list is %s",
                    matchs != NULL
                    && ((is_double == 0 && matchs->next == NULL && matchs->data == sample)
                        || (is_double && matchs->next != NULL && matchs->next->next == NULL
                            && (matchs->data == sample || matchs->next->data == sample))), label);
                slist_free(matchs, NULL);
                matchs = NULL;
            } else {
                matchs = NULL;
                TEST_CHECK2(test, "sensor_watch_visit(%s)",
                            sensor_watch_visit(d.sctx, label,
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_sample_visit, &matchs) == SENSOR_SUCCESS, label);
                TEST_CHECK2(test, "sensor_watch_visit list is %s",
                    matchs != NULL
                    && ((is_double == 0 && matchs->next == NULL && matchs->data == sample)
                        || (is_double && matchs->next != NULL && matchs->next->next == NULL
                            && (matchs->data == sample || matchs->next->data == sample))), label);
                slist_free(matchs, NULL);
                matchs = NULL;
            }
        }
        BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "        all sensor_watch_%s() ", findtype[i]);

        /* all matches */
        LOG_INFO(log, "* %u. b) sensor_watch_%s(* and */*)", 8 + i * 2, findtype[i]);
        matchs = NULL;
        if (i == 0) {
            TEST_CHECK(test, "sensor_watch_find(*) not null",
                       sensor_watch_find(d.sctx, "*", SSF_DEFAULT, &matchs) != NULL);
        } else {
            TEST_CHECK(test, "sensor_watch_visit(*)",
                        sensor_watch_visit(d.sctx, "*",
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_sample_visit, &matchs) == SENSOR_SUCCESS);
        }
        TEST_CHECK2(test, "sensor_watch_%s(*) must give all watchs, got %ld, expected %ld",
                    (exp = slist_length(d.watchs)) == (res = slist_length(matchs)),
                    findtype[i], res, exp);
        SLISTC_FOREACH_DATA(d.watchs, watch, sensor_sample_t *) {
            TEST_CHECK2(test, "'%s/%s' is in results",
                        slist_find(matchs, watch, hash_ptrcmp) != NULL,
                        watch->desc->family->info->name, STR_CHECKNULL(watch->desc->label));
        }
        slist_free(matchs, NULL);
        matchs = NULL;
        if (i == 0) {
            TEST_CHECK(test, "sensor_watch_find(*/*) not null",
                       sensor_watch_find(d.sctx, "*/*", SSF_DEFAULT, &matchs) != NULL);
        } else {
            TEST_CHECK(test, "sensor_watch_visit(*/*()",
                       sensor_watch_visit(d.sctx, "*/*",
                                   SSF_DEFAULT & ~SSF_CASEFOLD,
                                   test_sensor_sample_visit, &matchs) == SENSOR_SUCCESS);
        }
        TEST_CHECK2(test, "sensor_watch_%s(*/*) must give all watchs got %ld, expected %ld",
                   (exp = slist_length(d.watchs)) == (res = slist_length(matchs)),
                   findtype[i], res, exp);
        SLISTC_FOREACH_DATA(d.watchs, watch, sensor_sample_t *) {
            TEST_CHECK2(test, "'%s/%s' is in results",
                        slist_find(matchs, watch, hash_ptrcmp) != NULL,
                        watch->desc->family->info->name, STR_CHECKNULL(watch->desc->label));
        }
        slist_free(matchs, NULL);
        matchs = NULL;

        /* family matches */
        LOG_INFO(log, "* %u. c) sensor_watch_%s(per family)", 8 + i * 2, findtype[i]);

    }

    TEST_CHECK(test, "sensor_free(3)", sensor_free(d.sctx) == SENSOR_SUCCESS);
    avltree_free(families);
    slist_free(familylist, NULL);

    return VOIDP(TEST_END(test));
}

#endif /* ! ifdef _TEST */

