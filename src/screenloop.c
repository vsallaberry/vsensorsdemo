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
 * Test program for libvsensors and vlib / terminal sensors display loop.
 */
#include <unistd.h>
#include <sys/select.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <fnmatch.h>
#include <pthread.h>
#include <signal.h>

#include "vlib/term.h"
#include "vlib/log.h"
#include "vlib/util.h"
#include "vlib/account.h"
#include "vlib/job.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "vsensors.h"

/* ************************************************************************ */
/** display context for vsensors_display */
typedef struct {
    /* contexts / handles */
    sensor_ctx_t *  sctx;
    options_t *     opts;
    log_t *         log;
    FILE *          out;
    int             outfd;
    slist_t *       logpool_backup;
    vjob_t *        update_job;
    pthread_mutex_t update_mutex;
    pthread_cond_t  update_cond;
    struct timeval  update_job_now;
    /* timers */
    unsigned int    timer_ms;
    unsigned int    sensors_timer_ms;
    struct timeval  wincheck_interval;
    struct timeval  sensors_interval;
    struct timeval  next_wincheck_time;
    struct timeval  next_sensorscheck_time;
    struct timeval  timeout_time;
    /* display data */
    int             nrefresh;
    int             nupdates;
    unsigned int    page;
    sensor_sample_t *wselected;
    unsigned int    nbpages;
    unsigned int    sensors_nbpages;
    unsigned int    sensors_page;
    unsigned int    columns;
    unsigned int    rows;
    unsigned int    start_row;
    unsigned int    end_row;
    unsigned int    start_col;
    unsigned int    end_col;
    unsigned int    space_col;
    unsigned int    col_size;
    unsigned int    label_size;
    unsigned int    nbcol_per_page;
    unsigned int    sb_row;
    unsigned int    sb_start_col;
    unsigned int    sb_end_col;
    /* display colors */
    int             term_darkmode; /* 0: force light, -1: auto, others: dark */
    vterm_color_t   color_label;
    vterm_color_t   color_value;
    const char *    scolor_reset;
    const char *    scolor_info_label;
    const char *    scolor_info_desc;
    char *          scolor_header;
    char *          scolor_wselected;
} vsensors_display_data_t;

/* ************************************************************************ */
#define STR2(value)             #value
#define STR(macro)              STR2(macro)

#define VSENSOR_DISPLAY_PAD_FAM 0
#define VSENSOR_DISPLAY_PAD_NAM 23
#define VSENSOR_DISPLAY_PAD_VAL 13

#define VSENSOR_DISPLAY_FMT     "%-" STR(VSENSOR_DISPLAY_PAD_NAM) "s" \
                                "%s:%s"

#define VSENSOR_DISPLAY_COLS_MIN (  VSENSOR_DISPLAY_PAD_FAM \
                                  + (VSENSOR_DISPLAY_PAD_FAM ? 1 : 0) /* separator: '/' */ \
                                  + VSENSOR_DISPLAY_PAD_NAM + 1       /* separator: ':' */ \
                                  + VSENSOR_DISPLAY_PAD_VAL)

#define VSENSORS_SCREEN_COLS_MIN (VSENSOR_DISPLAY_COLS_MIN + 1)
#define VSENSORS_SCREEN_ROWS_MIN (5)

#define VSENSOR_DRAW            (0x80000000)
#define VSENSOR_COMPUTE         (0x40000000)
#define VSENSOR_CHECK_UPDATES   (0x20000000)
#define VSENSOR_DRAW_SPECIAL    (0x10000000)
#define VSENSOR_SPEC_PAGE_MASK  (0x00ff0000)
#define VSENSOR_HELP_PAGES      (0x00010000)
#define VSENSOR_LIST_PAGES      (0x00020000)
#define VSENSOR_STATUSBAR_PAGE  (0x00030000)
#define VSENSOR_ALL_PAGE_MASK   (0x0000ffff)
#define VSENSOR_STRICT_PAGE_MASK (VSENSOR_ALL_PAGE_MASK | VSENSOR_SPEC_PAGE_MASK)

#define VSENSORS_WINCHECK_MS    (2000)
#define VSENSORS_STATUSBAR_MS   (2000)

/* ************************************************************************ */
/** display data for each sensor value (sensor_sample_t.user_data) */
typedef struct vsensors_watch_display_s {
    unsigned int        idx;
    unsigned int        val_size;
    unsigned int        row;
    unsigned int        col;
    unsigned int        page;
    char *              label;
    char *              footer;
    char *              info;
    sensor_sample_t *   next;
    sensor_sample_t *   prev;
    struct vsensors_watch_display_s * next_display;
} vsensors_watch_display_t;

/* special sensors displayed in the status bar */
#define VSENSORS_COLOR_STATUSBAR VCOLOR_BUILD(VCOLOR_WHITE, VCOLOR_BG_BLUE, VCOLOR_BOLD)
static struct vsensors_status_bar_s {
    const char *        sensorpattern;
    const char *        header;
    const char *        footer;
    unsigned int        val_size;
    int                 row;
    int                 col;
    vterm_colorset_t    colors;
} s_vsensors_statusbar[] = {
#define VSENSORS_SB_AUTO INT_MAX
#if 1 /* compute automatically items positions and separators */
    { "cpu/cpus total %",   "", "%",  3, -1, VSENSORS_SB_AUTO, VSENSORS_COLOR_STATUSBAR },
    { "smc/*{TC0D}*",       "", "'C", 4, -1, VSENSORS_SB_AUTO, VSENSORS_COLOR_STATUSBAR },
    { "smc/*{F0Ac}*",       "", " rpm",4,-1,VSENSORS_SB_AUTO, VSENSORS_COLOR_STATUSBAR },
    { "memory/used memory %","M","%", 3, -1,VSENSORS_SB_AUTO, VSENSORS_COLOR_STATUSBAR },
#else /* manual items positions */
    { "cpu/cpus total %",       "|", "%]", 3, -1,  -6,  VSENSORS_COLOR_STATUSBAR },
    { "smc/*{TC0D}*",           "|", "˚C", 4, -1,  -13, VSENSORS_COLOR_STATUSBAR },
    { "memory/used memory %",   "[M:", "%",3, -1,  -20, VSENSORS_COLOR_STATUSBAR },
#endif
    { NULL, NULL, NULL, 0, 0, 0, 0}
};

/* ************************************************************************ */
/** get sensor name */
static const char * s_null_name = STR_NULL;
static const char * vsensors_fam_name(const sensor_sample_t * sensor) {
    if (sensor != NULL && sensor->desc != NULL && sensor->desc->family != NULL
        && sensor->desc->family->info != NULL && sensor->desc->family->info->name != NULL) {
        return sensor->desc->family->info->name;
    }
    return s_null_name;
}
static const char * vsensors_label(const sensor_sample_t * sensor) {
    if (sensor != NULL && sensor->desc != NULL && sensor->desc->label != NULL) {
        return sensor->desc->label;
    }
    return s_null_name;
}

/* ************************************************************************ */
/** free private display data of one sensor */
static void vsensors_watch_priv_free(void * vdata) {
    sensor_sample_t * watch = (sensor_sample_t *) vdata;
    vsensors_watch_display_t * wdata = (vsensors_watch_display_t *) (watch->user_data);
    watch->user_data = NULL;
    while (wdata != NULL) {
        void * to_free = wdata;
        if (wdata->label != NULL) {
            free(wdata->label);
        }
        if (wdata->footer != NULL) {
            free(wdata->footer);
        }
        if (wdata->info != NULL) {
            free(wdata->info);
        }
        wdata = wdata->next_display;
        free(to_free);
    }
}

/* ************************************************************************ */
/** compute placement of each sensor */
static int vsensors_display_compute(
                vsensors_display_data_t *   data) {
    FILE *                      out = data->out;
    char                        name[1024];
    vsensors_watch_display_t    watch_data;
    char *                      last_statusbar_sepptr;
    unsigned int                nb_per_col;
    int                         ret;
    int                         outfd = fileno(out);
    ssize_t                     len;
    unsigned int                i;
    size_t                      maxlen, watchs_nb;
    int                         statusbar_col, statusbar_row, statusbar_step;
    slist_t *                   lsb;
    unsigned int                i_st;
    sensor_sample_t *           prev;
    const slist_t *             watchs = NULL;
    unsigned long               timer_pgcd;
    double                      timer_precision = 10000.0L;
    const double                timer_min_precision = 50.0L;

    LOG_DEBUG(data->log, "%s(): COMPUTING WATCHS DISPLAY DATA", __func__);

    /* special colors for terminal light mode */
    if (data->term_darkmode == 0
    ||  (data->term_darkmode < 0 && VCOLOR_GET_BACK(vterm_termfgbg(outfd)) != VCOLOR_BG_BLACK)) {
        data->term_darkmode = 0;
        data->color_label = VCOLOR_BLUE;
        data->color_value = VCOLOR_RED;
        data->scolor_info_label = vterm_color(outfd, VCOLOR_GREEN);
        data->scolor_info_desc = vterm_color(outfd, VCOLOR_RESET);
    } else {
        data->term_darkmode = 1;
        data->color_label = VCOLOR_YELLOW;
        data->color_value = VCOLOR_GREEN;
        data->scolor_info_label = vterm_color(outfd, VCOLOR_BLUE);
        data->scolor_info_desc = vterm_color(outfd, VCOLOR_CYAN);
    }

    /* watch status bar sensors if not already watched */
    if ((data->opts->flags & FLAG_SB_ONLYWATCHED) == 0) {
        LOG_DEBUG(data->log, "%s(): Looking for suitable watchs for status-bar", __func__);
        for (i_st = 0, lsb = data->opts->sb_watchs.head; /*no_check*/; ++i_st) {
            struct vsensors_status_bar_s * sb, ssb;
            char label[512];
            if (data->opts->sb_watchs.head != NULL) {
                char * pattern;
                if (lsb == NULL)
                    break ;
                pattern = STR_CHECKNULL((char *) (lsb->data));
                lsb = lsb->next;
                if ((pattern = strchr(pattern, ':')) == NULL)
                    continue ;
                snprintf(label, PTR_COUNT(label), "%s", ++pattern);
                sb = &ssb;
                sb->sensorpattern = label;
            } else {
                if (i_st >= PTR_COUNT(s_vsensors_statusbar))
                    break ;
                sb = &(s_vsensors_statusbar[i_st]);
            }
            if (sb->sensorpattern == NULL)
                continue ;
            if (sensor_watch_find(data->sctx, sb->sensorpattern, SSF_DEFAULT, NULL) == NULL) {
                sensor_watch_t watch = SENSOR_WATCH_INITIALIZER(VSENSORS_STATUSBAR_MS, NULL);
                sensor_watch_add(data->sctx, sb->sensorpattern, SSF_DEFAULT, &watch);
            }
        }
    }

    /* get PlusGrandCommunDiviseur of all sensor watchs refresh intervals */
    timer_pgcd = sensor_watch_pgcd(data->sctx, &timer_precision, timer_min_precision);
    data->sensors_interval = (struct timeval) {
        .tv_sec = timer_pgcd / 1000, .tv_usec = (timer_pgcd % 1000) * 1000 };
    memset(&(data->next_sensorscheck_time), 0, sizeof(data->next_sensorscheck_time));

    /* compute global timer taking care of wincheck_interval and senesor_watch_pgcd */
    timer_pgcd = pgcd_rounded(timer_pgcd, data->wincheck_interval.tv_sec * 1000
                                          + data->wincheck_interval.tv_usec / 1000,
                              &timer_precision, timer_min_precision);

    /* compute global timer taking care of previous ones and loop timeout */
    timer_pgcd = pgcd_rounded(timer_pgcd, data->opts->timeout,
                              &timer_precision, timer_min_precision);

    data->timer_ms = timer_pgcd;
    LOG_DEBUG(data->log, "timer_ms= %u (precision=%lf)", data->timer_ms, timer_precision);

    data->sensors_nbpages = 1;
    data->sensors_page = 1;
    data->nbpages = 1;
    data->start_row = 2;
    data->start_col = 1;
    data->space_col = 4;
    data->end_row = data->rows - 3;
    data->end_col = data->columns - 1;
    data->col_size = VSENSOR_DISPLAY_COLS_MIN;

    nb_per_col = data->end_row - data->start_row + 1;

    /* get number of watchs and maximum size of a watch label */
    sensor_lock(data->sctx, SENSOR_LOCK_READ);
    watchs = sensor_watch_list_get(data->sctx);
    maxlen = 0;
    watchs_nb = 0;

    SLISTC_FOREACH_DATA(watchs, watch, sensor_sample_t *) {
        unsigned int sz = strlen(vsensors_fam_name(watch)) + 1 + strlen(vsensors_label(watch));
        if (sz > maxlen)
            maxlen = sz;
        ++watchs_nb;
    }

    /* compute nb_col_per_page and col_size */
    if (data->nbcol_per_page == 0) {
        unsigned int nb_per_page, nb_pages;

        data->nbcol_per_page = (data->end_col - data->start_col + 1 - data->space_col)
                               / data->col_size;
        if (data->nbcol_per_page == 0)
            data->nbcol_per_page = 1;

        nb_per_page = data->nbcol_per_page * nb_per_col;
        nb_pages    = (watchs_nb / nb_per_page) + (watchs_nb % nb_per_page == 0 ? 0 : 1);

        if (watchs_nb > 0 && (data->nbcol_per_page - 1) * nb_per_col * nb_pages >= watchs_nb) {
            nb_per_page = (watchs_nb / nb_pages) + (watchs_nb % nb_pages == 0 ? 0 : 1);
            data->nbcol_per_page = (nb_per_page / nb_per_col)
                                 + (nb_per_page % nb_per_col == 0 ? 0 : 1);
        }

    }
    /*nb_per_page = data->nbcol_per_page * nb_per_col;*/

    data->col_size = (data->end_col - data->start_col
                      - (data->nbcol_per_page - 1) * data->space_col
                     ) / (data->nbcol_per_page);

    /* shrink maxlen or expand col_size according to screen size */
    if (maxlen + VSENSOR_DISPLAY_PAD_VAL + 1 < data->col_size)
        data->col_size = maxlen + VSENSOR_DISPLAY_PAD_VAL + 1;
    else
        maxlen = data->col_size - 1 - VSENSOR_DISPLAY_PAD_VAL;
    data->label_size = maxlen;

    /* compute labels and positions for each sensor */
    ret = 0;
    watch_data.idx = 0;
    watch_data.row = data->start_row;
    watch_data.col = data->start_col;
    watch_data.page = 1;
    prev = NULL;
    data->sb_row = data->rows - 1;
    statusbar_row = data->sb_row;
    data->sb_start_col = 0;
    statusbar_col = data->columns;
    statusbar_step = -1;
    last_statusbar_sepptr = NULL;

    SLISTC_FOREACH_ELT(watchs, list) {
        sensor_sample_t * watch = (sensor_sample_t *) list->data;

        /** allocate sensor user private data, or free previous label */
        if (watch->user_data != NULL) {
            vsensors_watch_priv_free(watch);
        }
        if ((watch->user_data = malloc(sizeof(vsensors_watch_display_t))) != NULL) {
            watch->userfreefun = vsensors_watch_priv_free;
        } else {
            ret = -1;
            break ;
        }
        /* keep prev/next */
        watch_data.next = list->next != NULL ? (sensor_sample_t *) list->next->data : NULL;
        watch_data.prev = prev;
        prev = watch;
        /* other watch_data init */
        watch_data.next_display = NULL;
        watch_data.footer = NULL;
        watch_data.val_size = 0;
        /* compute display position of sensor */
        if (watch_data.row > data->end_row) {
            /* row exceeded, must change columns or page */
            if (watch_data.col + 2 * data->col_size + data->space_col > data->end_col) {
                /* must change page */
                watch_data.col = data->start_col;
                watch_data.row = data->start_row;
                ++(watch_data.page);
                ++(data->sensors_nbpages);
            } else {
                /* must change column */
                watch_data.row = data->start_row;
                watch_data.col += data->col_size + data->space_col;
            }
        }
        /* Prepare label of sensor */
        len = VLIB_SNPRINTF(len, name, sizeof(name)/sizeof(*name), "%s/%s%s",
                    vsensors_fam_name(watch),
                    vterm_color(outfd, data->color_label),
                    vsensors_label(watch));
        for (i = len;   i + 1 < sizeof(name) / sizeof(*name)
                     && i - vterm_color_size(outfd, data->color_label) < maxlen;i++) {
            name[i] = ' ';
        }
        name[i]=0;
        watch_data.label = NULL;
        len = asprintf(&watch_data.label,
                 "%s%s:%s", //VSENSOR_DISPLAY_FMT,
                 name,
                 data->scolor_reset,
                 vterm_color(outfd, data->color_value));
        if (len >= 0 && watch_data.label != NULL) {
            ssize_t real_len = len;
            len -= vterm_color_size(outfd, data->color_label)
                   + vterm_color_size(outfd, VCOLOR_RESET)
                   + vterm_color_size(outfd, data->color_value);

            if (len > (ssize_t)data->col_size - VSENSOR_DISPLAY_PAD_VAL) {
                char * trunc_from = watch_data.label
                        + data->col_size - VSENSOR_DISPLAY_PAD_VAL -2/*'..'*/ -1/*':'*/
                        + vterm_color_size(outfd, data->color_label);
                snprintf(trunc_from, real_len + 1 - (trunc_from - watch_data.label),
                        "..%s:%s", data->scolor_reset,
                        vterm_color(outfd, data->color_value));
            }
        }
        /* prepare info of sensor */
        if ((watch_data.info = malloc(512 * sizeof(*(watch_data.info)))) != NULL) {
            unsigned int len = 0;
            int snpret;

            /* sensor_value_type */
            len += VLIB_SNPRINTF(snpret, watch_data.info + len, 512 - len, " type:%s",
                                 sensor_value_type_name(watch->value.type));

            /* sensor_property_t */
            for (sensor_property_t * prop = watch->desc->properties; prop != NULL
                    && (prop->name != NULL || prop->value.type != SENSOR_VALUE_NULL); ++prop) {
                len += VLIB_SNPRINTF(snpret, watch_data.info + len, 512 - len, " %s:",
                                     STR_CHECKNULL(prop->name));
                len += sensor_value_tostring(&(prop->value), watch_data.info + len, 512 - len);
            }
        }

        /* special sensors (cpu total %, ...) are displayed in status bar */
        for (i_st = 0, lsb = data->opts->sb_watchs.head; /*no_check*/; ++i_st) {
            struct vsensors_status_bar_s * sb, ssb;
            char label[512], sb_label[512], sb_header[32];
            if (data->opts->sb_watchs.head != NULL) {
                char * pattern, * desc;
                if (lsb == NULL)
                    break ;
                desc = STR_CHECKNULL((char *) (lsb->data));
                lsb = lsb->next;
                if ((pattern = strchr(desc, ':')) == NULL)
                    continue ;
                snprintf(sb_label, PTR_COUNT(sb_label), "%s", pattern + 1);
                sb = &ssb;
                if (watch->value.type == SENSOR_VALUE_CHAR
                || watch->value.type == SENSOR_VALUE_UCHAR) { //TODO bad val_size handling
                    sb->val_size = 3;
                } else if (SENSOR_VALUE_IS_FLOATING(watch->value.type)) {
                    sb->val_size = 6;
                } else {
                    sb->val_size = 12;
                }
                sb->header = sb_header;
                strn0cpy(sb_header, desc, (pattern - desc), PTR_COUNT(sb_header));
                sb->footer = "";
                sb->colors = VSENSORS_COLOR_STATUSBAR;
                sb->col = VSENSORS_SB_AUTO;
                sb->row = VSENSORS_SB_AUTO;
                sb->sensorpattern = sb_label;
            } else {
                if (i_st >= PTR_COUNT(s_vsensors_statusbar))
                    break ;
                sb = &(s_vsensors_statusbar[i_st]);
            }
            snprintf(label, PTR_COUNT(label), "%s/%s", watch->desc->family->info->name,
                     STR_CHECKNULL(watch->desc->label));
            if (sb->sensorpattern != NULL
            &&  fnmatch(sb->sensorpattern, label, 0) == 0
            &&  (watch_data.next_display = malloc(sizeof(watch_data))) != NULL) {
                vsensors_watch_display_t * wdata_sb = watch_data.next_display;
                char sep[2] = { 0, 0 };
                char end[2] = { 0, 0 };
                int sbcol_bak = statusbar_col;

                /* size / coords */
                wdata_sb->val_size = sb->val_size;
                wdata_sb->page = VSENSOR_STATUSBAR_PAGE;

                if (sb->row == VSENSORS_SB_AUTO || sb->col == VSENSORS_SB_AUTO) {
                    /* auto-mode */
                    if (statusbar_col == (int) data->columns) {
                        statusbar_col += statusbar_step; /* add ] at the end */
                        *end = ']';
                    }
                    statusbar_col += (sb->val_size
                                      + (sb->header != NULL ? strlen(sb->header) : 0)
                                      + (sb->footer != NULL ? strlen(sb->footer) : 0)
                                     ) * statusbar_step;
                    statusbar_col += statusbar_step; /* add | or [ at the beginning */
                    if (statusbar_col < 1 || (unsigned int) statusbar_col > data->columns) {
                        LOG_DEBUG(data->log, "STOP statusbar col=%d bak=%d",
                                  statusbar_col, sbcol_bak);
                        statusbar_col = sbcol_bak;
                        free(wdata_sb);
                        watch_data.next_display = NULL;
                        break ;
                    }
                    *sep = '|';
                    wdata_sb->row = statusbar_row;
                    wdata_sb->col = statusbar_col;
                } else {
                    wdata_sb->row = (sb->row < 0
                                   ? data->rows : 0) + sb->row;

                    wdata_sb->col = (sb->col < 0
                                   ? data->columns : 0) + sb->col;
                }

                /* header / footer */
                wdata_sb->label = NULL;
                len = asprintf(&(wdata_sb->label), "%s%s%s%s%s",
                               vterm_color(outfd, VCOLOR_GET_FORE(sb->colors)),
                               vterm_color(outfd, VCOLOR_GET_BACK(sb->colors)),
                               vterm_color(outfd, VCOLOR_GET_STYLE(sb->colors)),
                               sep, sb->header);

                if (*sep != 0 && wdata_sb->label != NULL) {
                    last_statusbar_sepptr = wdata_sb->label
                                          + vterm_color_size(outfd, VCOLOR_GET_FORE(sb->colors))
                                          + vterm_color_size(outfd, VCOLOR_GET_BACK(sb->colors))
                                          + vterm_color_size(outfd, VCOLOR_GET_STYLE(sb->colors));
                }

                wdata_sb->footer = NULL;
                asprintf(&(wdata_sb->footer), "%s%s%s", sb->footer, end, data->scolor_reset);

                /* end init */
                wdata_sb->next = watch_data.next;
                wdata_sb->prev = watch_data.prev;
                wdata_sb->info = NULL;
                wdata_sb->next_display = NULL;

                break ;
            }
        }

        /* copy computed data to sensor user private data and go to next one */
        memcpy(watch->user_data, &watch_data, sizeof(watch_data));

        ++(watch_data.row);
        ++(watch_data.idx);
    }
    data->sb_end_col = statusbar_col - 1;
    if (last_statusbar_sepptr != NULL) {
        *last_statusbar_sepptr = '[';
    }
    if ((data->page & VSENSOR_SPEC_PAGE_MASK) == 0) {
        data->nbpages = data->sensors_nbpages;
    }
    sensor_unlock(data->sctx);
    return ret;
}

/* ************************************************************************ */
static void vsensors_display_sensor_info(vsensors_display_data_t * data,
                                         sensor_sample_t * sensor) {
    char            label_buf[64];
    char            info_buf[128];
    unsigned int    info_desc_columns = (data->sb_end_col - data->sb_start_col + 1);
    FILE *          out = data->out;
    /*int             outfd = data->outfd;*/
    vsensors_watch_display_t * wdata;
    int             ret;
    unsigned int    label_len, info_len, xtra_info_len;
    struct          timeval * tv;

    if (sensor == NULL || sensor->user_data == NULL || info_desc_columns < 5) {
        flockfile(out);
        vterm_goto(out, data->sb_row, data->sb_start_col);
        for (unsigned int i = 0; i < info_desc_columns; ++i) {
            fputc(' ', out);
        }
        funlockfile(out);
        return ;
    }

    wdata = (vsensors_watch_display_t *) sensor->user_data;
    tv = &(sensor->next_update_time);
    flockfile(out);

    /* display sensor info
     * we could pre-compute some info as it is done for sensors list, but this
     * info is only printed for selected sensor, then we keep it dynamic. */
    label_len = VLIB_SNPRINTF(ret, label_buf, sizeof(label_buf)/sizeof(*label_buf),
                    "%s/%s", vsensors_fam_name(sensor),vsensors_label(sensor));

    info_len = VLIB_SNPRINTF(ret, info_buf, sizeof(info_buf)/sizeof(*info_buf),
                "timer=%lu next=%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64,
                sensor_watch_timerms(sensor),
                (tv->tv_sec / INT64_C(3600)) % INT64_C(24),
                (tv->tv_sec / INT64_C(60)) % INT64_C(60),
                tv->tv_sec % INT64_C(60), tv->tv_usec / INT64_C(1000));

    xtra_info_len = wdata->info != NULL ? strlen(wdata->info) : 0;

    /* trunc label / info / xtra_info if necessary */
    if (label_len + 2 /* ': ' */ + info_len + xtra_info_len > info_desc_columns) {
        int newlabel_len = info_desc_columns - info_len - xtra_info_len - 2;
        if (newlabel_len > 3) {
            label_len = newlabel_len;
            strncpy(label_buf + label_len - 2, "..\0", 3);
        } else {
            if (label_len > 4) {
                label_len = 4;
                strncpy(label_buf + label_len - 2, "..\0", 3);
            }
            if (label_len + 2 /* ': ' */ + info_len > info_desc_columns) {
                info_len = info_desc_columns - label_len - 2 /* ': ' */;
                xtra_info_len = 0;
            } else if (label_len + 2 /* ': ' */ + info_len + xtra_info_len > info_desc_columns) {
                xtra_info_len = info_desc_columns - label_len - info_len - 2 /* ': ' */;
            }
        }
    }

    vterm_goto(out, data->sb_row, data->sb_start_col);
    fprintf(out, "%s%s%s: %s", data->scolor_info_label, label_buf,
            data->scolor_reset, data->scolor_info_desc);
    fwrite(info_buf, 1, info_len, out);
    fwrite(wdata->info, 1, xtra_info_len, out);
    fputs(data->scolor_reset, out);
    funlockfile(out);
}

/* ************************************************************************ */
typedef enum {
    VSH_NONE        = 0,
    VSH_RELATIVE,
    VSH_ABSOLUTE,
} vsensors_select_how_t;
static void vsensors_select_sensor(
                vsensors_display_data_t *   data,
                int                         offset,
                int                         how) {
    FILE *  out = data->out;
    /*int     outfd = data->outfd;*/
    vsensors_watch_display_t * wdata;

    sensor_lock(data->sctx, SENSOR_LOCK_READ);
    flockfile(out);
    if (data->wselected != NULL && data->wselected->user_data != NULL) {
        int step;
        sensor_sample_t * watch;

        wdata = (vsensors_watch_display_t *) data->wselected->user_data;

        /* clear selector */
        vterm_goto(out, wdata->row, wdata->col - 1);
        fputc(' ', out);
        /* clear sensor info */
        vsensors_display_sensor_info(data, NULL);

        if ((data->page & VSENSOR_SPEC_PAGE_MASK) != 0) {
            funlockfile(out);
            sensor_unlock(data->sctx);
            return ;
        }

        if (how == VSH_ABSOLUTE) {
            if (offset < 0) {
                funlockfile(out);
                sensor_unlock(data->sctx);
                return ;
            }
            offset = offset - wdata->idx;
        }

        step = offset > 0 ? 1 : -1;
        for (watch = data->wselected; watch != NULL; /*no_incr*/) {
            sensor_sample_t *           next;
            vsensors_watch_display_t *  wcur = (vsensors_watch_display_t *) watch->user_data;

            if (wcur == NULL)
                continue ;

            if (watch == data->wselected && offset == 0
            && wcur->page == (data->page & VSENSOR_STRICT_PAGE_MASK)) {
                break ;
            }

            next = step > 0 ? wcur->next : wcur->prev;

            if (next == NULL || ((vsensors_watch_display_t *)next->user_data)->page
                                != (data->page & VSENSOR_STRICT_PAGE_MASK)) {
                break ;
            }
            watch = next;
            offset -= step;
            if (offset == 0)
                break ;
        }
        data->wselected = watch;

        LOG_SCREAM(data->log, "%s(): NEW SELECTED: %s (page=%x)", __func__,
                   watch != NULL ? watch->desc->label : "'NONE'", data->page);
    }
    if (data->wselected != NULL && data->wselected->user_data != NULL) {
        wdata = (vsensors_watch_display_t *) data->wselected->user_data;
        /* display selector mark */
        vterm_goto(out, wdata->row, wdata->col - 1);
        fprintf(out, "%s*%s", data->scolor_wselected, data->scolor_reset);
        /* display sensor info */
        vsensors_display_sensor_info(data, data->wselected);
    }
    funlockfile(out);
    sensor_unlock(data->sctx);
}

/* ************************************************************************ */
/** display one sensor at its position
 * called unlocked */
static void vsensors_display_one_sensor(
                sensor_sample_t *           sensor,
                vsensors_display_data_t *   data) {

    FILE *  out     = data->out;
    int     outfd   = data->outfd;
    unsigned int                len;
    vsensors_watch_display_t *  wdata = (vsensors_watch_display_t *) sensor->user_data;
    char buf[VSENSOR_DISPLAY_PAD_VAL];

    flockfile(out);

    /* convert sensor value to string */
    len = sensor_value_tostring(&sensor->value, buf, sizeof(buf) / sizeof(*buf));

    /* optionally display additional items (such as statusbar) */
    for (vsensors_watch_display_t * wdata2 = wdata->next_display;
                        wdata2 != NULL; wdata2 = wdata2->next_display) {
        /* print custom display */
        unsigned int val_len = len > wdata2->val_size ? wdata2->val_size : len;

        vterm_goto(out, wdata2->row, wdata2->col);
        fputs(wdata2->label != NULL ? wdata2->label : "", out);
        for (unsigned int i = val_len; i < wdata2->val_size; ++i)
           fputc(' ', out);
        fwrite(buf, 1, val_len, out);
        fputs(wdata2->footer != NULL ? wdata2->footer : "", out);
    }

    /* Display the sensor */
    if (wdata->page == (data->page & VSENSOR_STRICT_PAGE_MASK)) {
        /* update current selected sensor */
        if (sensor == data->wselected) {
            vsensors_display_sensor_info(data, sensor);
        }

        if ((data->page & VSENSOR_DRAW) != 0) {
            /* print label and value on page change */
            vterm_goto(out, wdata->row, wdata->col);
            fprintf(out, "%s%" STR(VSENSOR_DISPLAY_PAD_VAL) "s%s",
                    STR_CHECKNULL(wdata->label), buf, data->scolor_reset);
        } else {
            /* print only value when page did not change */
            vterm_goto(out, wdata->row, wdata->col + data->label_size + 1);
            fprintf(out, "%s%" STR(VSENSOR_DISPLAY_PAD_VAL) "s%s",
                    vterm_color(outfd, data->color_value), buf,
                    data->scolor_reset);
        }
    }
    funlockfile(out);
}

/* ************************************************************************ */
static int vsensors_print_header(vsensors_display_data_t * data, const char * header,
                                 unsigned int header_len, unsigned int header_col) {
    FILE *  out     = data->out;
    /*int     outfd   = data->outfd;*/

    flockfile(out);

    if (data->columns < header_col + header_len)
        vterm_goto(out, 1, 0);
    else
        vterm_goto(out, 0, header_col);

    fprintf(out, "%s%s%s", data->scolor_header,
            header, data->scolor_reset);

    funlockfile(out);
    return 0;
}

/* ************************************************************************ */
static int vsensors_replacelogs(vsensors_display_data_t * data) {
    char * buffer = NULL, * name = NULL, logpath[PATH_MAX];
    size_t bufsz = 0;
    struct passwd pw;

    if (pwfindbyid_r(getuid(), &pw, &buffer, &bufsz) == 0) {
        name = pw.pw_name;
    }

    snprintf(logpath, sizeof(logpath)/sizeof(*logpath),
             "/tmp/vsensorsdemo_screen-%s.log", name == NULL ? "nobody" : name);

    if (buffer != NULL)
        free(buffer);

    LOG_INFO(data->log, "opening screen log file '%s'", logpath);
    if (logpool_replacefile(data->opts->logs, NULL, logpath,
                &(data->logpool_backup)) != 0) {
        LOG_WARN(data->log, "cannot open log file '%s', disabling logging...", logpath);
        logpool_enable(data->opts->logs, NULL, 0, NULL);
    }
    LOG_VERBOSE(data->log, "New Screen Session");
    return 0;
}

/* ************************************************************************ */
static int vsensors_please_wait(vsensors_display_data_t * data) {
    FILE *  out     = data->out;
    int     outfd   = data->outfd;

    vsensors_select_sensor(data, -1, VSH_ABSOLUTE);

    flockfile(out);
    vterm_goto(out, data->rows - 1, 0);
    fprintf(out, "%s%sPlease wait... %s", vterm_color(outfd, VCOLOR_WHITE),
            vterm_color(outfd, VCOLOR_BG_RED), data->scolor_reset);
    fflush(out);
    funlockfile(out);

    return 0;
}

/* ************************************************************************ */
static inline int vsensors_is_watched(vsensors_display_data_t * data,
                                      sensor_sample_t * sensor) {
    return sensor->user_data != NULL
           &&  ( (((vsensors_watch_display_t *)sensor->user_data)->page
                        == (data->page & VSENSOR_STRICT_PAGE_MASK)) );
}

/* ************************************************************************ */
static inline int vsensors_is_displayed(vsensors_display_data_t * data,
                                        sensor_sample_t * sensor) {
    return sensor->user_data != NULL
           &&  ( (((vsensors_watch_display_t *)sensor->user_data)->page
                        == (data->page & VSENSOR_STRICT_PAGE_MASK))
                 || ((vsensors_watch_display_t *)sensor->user_data)->next_display != NULL);
}

/* ************************************************************************ */
static void vsensors_draw_specials(vsensors_display_data_t * data) {
    sensor_lock(data->sctx, SENSOR_LOCK_READ);
    SLISTC_FOREACH_DATA(sensor_watch_list_get(data->sctx), sensor, sensor_sample_t *) {
        if (sensor->user_data != NULL) {
            vsensors_watch_display_t * wdata = (vsensors_watch_display_t *) (sensor->user_data);
            if (wdata->next_display != NULL || sensor == data->wselected) {
                vsensors_display_one_sensor(sensor, data);
            }
        }
    }
    sensor_unlock(data->sctx);
}

/* ************************************************************************ */
static int vsensors_lock_update(vsensors_display_data_t * data) {
    int ret = pthread_mutex_lock(&(data->update_mutex));
    sensor_lock(data->sctx, SENSOR_LOCK_WRITE);
    sensor_unlock(data->sctx);
    return ret;
}
/* ************************************************************************ */
static int vsensors_update_wakeup(vsensors_display_data_t * data, struct timeval * now) {
    int ret;
    pthread_mutex_lock(&(data->update_mutex));
    data->update_job_now = *now;
    ret = pthread_cond_signal(&(data->update_cond));
    pthread_mutex_unlock(&(data->update_mutex));
    return ret;
}
/* ************************************************************************ */
static int vsensors_unlock_update(vsensors_display_data_t * data) {
    return pthread_mutex_unlock(&(data->update_mutex));
}
/* ************************************************************************ */
static int vsensors_update_stop(vsensors_display_data_t * data) {
    pthread_mutex_lock(&(data->update_mutex));
    vjob_killnowait(data->update_job);
    pthread_cond_signal(&(data->update_cond));
    pthread_mutex_unlock(&(data->update_mutex));
    if (vjob_waitandfree(data->update_job) != VJOB_ERR_RESULT)
        data->update_job = NULL;
    return 0;
}
/* ************************************************************************ */
static void vsensors_update_job_cleanup(void * data) {
    pthread_mutex_unlock(&(((vsensors_display_data_t *)data)->update_mutex));
}

/* ************************************************************************ */
static void * vsensors_update_job(void * vdata) {
    vsensors_display_data_t *   data = (vsensors_display_data_t *) vdata;
    sensor_status_t             update_ret;
    FILE *                      out = data->out;
    int                         outfd = data->outfd;
    int                         ret;
    struct timeval              now = { .tv_sec = 0, .tv_usec = 0};

    /* kill mode only under vjob_testkill() */
    vjob_killmode(0, 0, NULL, NULL);
    pthread_cleanup_push(vsensors_update_job_cleanup, data);
    /* notify parent we are ready */
    pthread_mutex_lock(&(data->update_mutex));
    pthread_cond_signal(&(data->update_cond));

    while (1) {
        /* check termination and wait to be woken up */
        vjob_testkill();
        pthread_cond_wait(&(data->update_cond), &(data->update_mutex));
        now = data->update_job_now;
        pthread_mutex_unlock(&(data->update_mutex));

        /* do updates */
        ret = 0;
        sensor_lock(data->sctx, SENSOR_LOCK_READ);
        SLISTC_FOREACH_DATA(sensor_watch_list_get(data->sctx), sensor, sensor_sample_t *) {
            if (vsensors_is_displayed(data, sensor)) {
                if ((update_ret = sensor_update_check(sensor, &now)) == SENSOR_UPDATED) {
                    ++ret;
                    /* Display sensors update */
                    vsensors_display_one_sensor(sensor, data);
                } else if (update_ret == SENSOR_UNCHANGED && sensor == data->wselected) {
                    /* update selected sensor info (next_update_time) */
                    vsensors_display_sensor_info(data, sensor);
                } else if (update_ret == SENSOR_RELOAD_FAMILY) {
                    data->page |= VSENSOR_COMPUTE;
                    data->wselected = NULL;
                    break ;
                }
            }
        }

        if (ret > 0) {
            ++(data->nupdates); /* number of time we got one or more sensors updates */
        }

        flockfile(out);
        vterm_goto(out, 0, 13);
        fprintf(out, "%sUPDATES%s: %s%s%3d%s",
                vterm_color(outfd, VCOLOR_GREEN), data->scolor_reset,
                vterm_color(outfd, VCOLOR_BOLD), vterm_color(outfd, VCOLOR_RED),
                ret, data->scolor_reset);
        vterm_goto(out, 0, data->columns - 1);
        fflush(out);
        funlockfile(out);

        sensor_unlock(data->sctx);
        pthread_mutex_lock(&(data->update_mutex));
    }
    pthread_cleanup_pop(0);
    return NULL;
}

/* ************************************************************************ */
static unsigned int vsensors_display(
                        vterm_screen_event_t    event,
                        FILE *                  out_unused,
                        struct timeval *        now,
                        vterm_screen_ev_data_t *evdata,
                        void *                  vdata) {
    (void) out_unused;
    static const char * const   header = "[q:quit n:next p:prev x:expand ?:help]";
    static const unsigned int   header_col = 26;
    static const unsigned int   page_len = 9;
    static unsigned int         header_len;
    unsigned int                callback_ret = VTERM_SCREEN_CB_OK;
    char                        buf[128];
    char                        key_buf[32];
    vsensors_display_data_t *   data    = (vsensors_display_data_t *) vdata;
    FILE *                      out     = data->out;
    int                         outfd   = data->outfd;
    int                         ret;
    #if _DEBUG
    const unsigned int dbg_colsz = 48, dbg_loops_sz = 16,/*dbg_refresh_sz=15,*/ dbg_key_sz = 15;
    #endif

    switch (event) {
        case VTERM_SCREEN_INIT:
            /* set start time and refresh time */
            timeradd(now, &(data->wincheck_interval), &(data->next_wincheck_time));
            timeradd(now, &(data->sensors_interval), &(data->next_sensorscheck_time));
            data->timeout_time.tv_sec = now->tv_sec + data->opts->timeout / 1000;
            data->timeout_time.tv_usec = now->tv_usec + data->opts->timeout * 1000;
            /* replace stdout/stderr LOGGING by a file */
            vsensors_replacelogs(data);
            /* print header */
            header_len = strlen(header);
            break ;

        case VTERM_SCREEN_START:
            if (data->update_job == NULL) {
                pthread_mutex_lock(&(data->update_mutex));
                if ((data->update_job = vjob_run(vsensors_update_job, data)) == NULL) {
                    pthread_mutex_unlock(&(data->update_mutex));
                    callback_ret = VTERM_SCREEN_CB_EXIT;
                    break ;
                }
                pthread_cond_wait(&(data->update_cond), &(data->update_mutex));
                pthread_mutex_unlock(&(data->update_mutex));
            }
            vsensors_print_header(data, header, header_len, header_col);
            data->page |= VSENSOR_DRAW;
            vsensors_update_wakeup(data, now);
            break ;

        case VTERM_SCREEN_LOOP:
            ++(data->nrefresh); /* number of times the display loop ran */

            /* recompute sensors labels and positions if needed */
            if ((data->page & VSENSOR_COMPUTE) != 0) {
                unsigned int old_timer = data->timer_ms;
                LOG_SCREAM(data->log, "%s(): COMPUTE REQUESTED", __func__);
                vsensors_lock_update(data);
                vsensors_please_wait(data);
                data->nbcol_per_page = 0;
                data->page = (data->page & VSENSOR_SPEC_PAGE_MASK) | 1 | VSENSOR_DRAW;
                data->wselected = NULL;
                vsensors_display_compute(data);
                vterm_clear(out);
                vsensors_print_header(data, header, header_len, header_col);
                vsensors_unlock_update(data);
                if (data->timer_ms != old_timer) {
                    evdata->newtimer_ms = data->timer_ms;
                    callback_ret = VTERM_SCREEN_CB_NEWTIMER;
                    break ;
                }
            }

            /* redisplays sensors on page change */
            if ((data->page & VSENSOR_DRAW) != 0) {
                LOG_SCREAM(data->log, "%s(): DRAW REQUESTED", __func__);
                vterm_clear_rect(out, data->start_row, data->start_col, data->end_row, data->end_col);
                if ((data->page & VSENSOR_SPEC_PAGE_MASK) == VSENSOR_HELP_PAGES) {
                    /* HELP PAGES */
                    const char * const helps[] = {
                        data->opts->version_string, vlib_get_version(),
                        libvsensors_get_version(), " ",
                        "q: quit", "n: next page", "p: previous page",
                        "x: expand sensor labels", "?: toggle help page",
                        "a: add sensors (pattern:'family/label', eg: 'cpu/*', '*')",
                        "d: delete sensors (pattern:'family/label', eg: 'mem*/*[%]', '*')",
                        "A: replace selected sensor (or add new one(s)), case-sensitive",
                        "D: delete selected sensor (or other(s)), case-sensitive",
                        "t: toggle term dark mode (dark/light, default:auto)",
                        "O: toggle only-watched option (current:%d)",
                         NULL };
                    const char * const * help = helps;
                    for (unsigned int row = data->start_row, page = 1; *help != NULL; ++help) {
                        char            line[1024];
                        const char *    token, * next = *help;
                        size_t          len, trunclen;
                        if (row > data->end_row) {
                            row = data->start_row;
                            if (++page > data->nbpages)
                                data->nbpages = page;
                        }
                        while (row <= data->end_row
                        && ((len = strtok_ro_r(&token, "\n", &next, NULL, 0)) > 0 || *next)) {
                            if (len == 0 || page != (data->page & VSENSOR_ALL_PAGE_MASK)) {
                                ++row;
                                continue ;
                            }
                            if (*token == 'O') {
                                char tmp[1024];
                                len = strn0cpy(tmp, token, len, PTR_COUNT(tmp));
                                len = VLIB_SNPRINTF(ret, line, PTR_COUNT(line), tmp,
                                                    (data->opts->flags & FLAG_SB_ONLYWATCHED) != 0);
                                token = line;
                            }
                            trunclen = len;
                            vterm_strlen(outfd, token, &trunclen, data->end_col - data->start_col);
                            flockfile(out);
                            vterm_goto(out, row++, data->start_col);
                            if (*token != 0 && token[1] == ':') {
                                fprintf(out, "%s%c%s", vterm_color(outfd, VCOLOR_BOLD),
                                        *token, vterm_color(outfd, VCOLOR_RESET));
                                ++token;
                                --len;
                                --trunclen;
                            }
                            fwrite(token, 1, trunclen, out);

                            funlockfile(out);
                        }
                    }
                    vsensors_select_sensor(data, -1, VSH_ABSOLUTE);
                } else if ((data->page & VSENSOR_SPEC_PAGE_MASK) == VSENSOR_LIST_PAGES) {
                    /* SENSORS LIST PAGES */
                    vsensors_select_sensor(data, -1, VSH_ABSOLUTE);
                } else {
                    /* MAIN SENSORS PAGES */
                    sensor_lock(data->sctx, SENSOR_LOCK_READ);
                    if (data->wselected != NULL
                        && (data->wselected->user_data == NULL
                            || ((vsensors_watch_display_t*)data->wselected->user_data)->page
                                                   != (data->page & VSENSOR_STRICT_PAGE_MASK))) {
                        vsensors_select_sensor(data, -1, VSH_ABSOLUTE);
                        data->wselected = NULL;
                    }
                    SLISTC_FOREACH_DATA(sensor_watch_list_get(data->sctx),
                                        sensor, sensor_sample_t *) {
                        if (vsensors_is_displayed(data, sensor)) {
                            if (data->wselected == NULL && vsensors_is_watched(data, sensor)) {
                                data->wselected = sensor;
                            }
                            vsensors_display_one_sensor(sensor, data);
                        }
                    }
                    vsensors_select_sensor(data, 0, VSH_RELATIVE);
                    sensor_unlock(data->sctx);
                }

                /* Show current page and total pages */
                flockfile(out);
                if (data->columns <= header_col + header_len)
                    vterm_goto(out, 0, data->columns - page_len);
                else if (data->columns <= header_col + header_len + page_len)
                    vterm_goto(out, 1, data->columns - page_len);
                else
                    vterm_goto(out, 0, header_col + header_len + 1);
                fprintf(out, "%s%s[%3u/%-3u]%s",
                        vterm_color(outfd, (data->page & VSENSOR_SPEC_PAGE_MASK) == 0
                                           ? VCOLOR_WHITE : VCOLOR_BLACK),
                        vterm_color(outfd, (data->page & VSENSOR_SPEC_PAGE_MASK) == 0
                                           ? VCOLOR_BG_BLUE : VCOLOR_BG_GREEN),
                        data->page & VSENSOR_ALL_PAGE_MASK, data->nbpages,
                        data->scolor_reset);
                funlockfile(out);
            }
            if ((data->page & VSENSOR_DRAW_SPECIAL) != 0
            ||  ((data->page & VSENSOR_DRAW) != 0 && (data->page & VSENSOR_SPEC_PAGE_MASK) != 0)) {
                vsensors_draw_specials(data);
            }

            #if _DEBUG
            /* number of times the display loop ran */
            if (header_col + header_len + 1 + page_len + 1 + dbg_colsz < data->columns) {
                vterm_printxy(out, 0, data->columns - 1 - dbg_colsz,
                        "loops:%s%s%010d%s",
                        vterm_color(outfd, VCOLOR_WHITE), vterm_color(outfd, VCOLOR_BG_BLUE),
                        data->nrefresh, vterm_color(outfd, VCOLOR_BG_BLACK));
                /* number of time we got one or more sensors updates */
                vterm_printxy(out, 0, data->columns - 1 - dbg_colsz + dbg_loops_sz + 1,
                        "UPDs:%s%s%010d%s", vterm_color(outfd, VCOLOR_BLACK),
                        vterm_color(outfd, VCOLOR_BG_GREEN), data->nupdates, data->scolor_reset);
            }
            #endif

            /* Move cursor and flush display */
            flockfile(out);
            vterm_goto(out, 0, data->columns - 1);
            fflush(out);
            funlockfile(out);

            ret = data->page;
            data->page = data->page & (VSENSOR_STRICT_PAGE_MASK | VSENSOR_COMPUTE);

            /* check sensors updates */
            if ((ret & (VSENSOR_CHECK_UPDATES | VSENSOR_DRAW | VSENSOR_COMPUTE)) != 0) {
                vsensors_update_wakeup(data, now);
            }

            break ;

        case VTERM_SCREEN_TIMER:
            /* display current time */
            vterm_printxy(out, 0, 0,
                    "%s%s%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64 "%s",
                    vterm_color(outfd, VCOLOR_BLACK), vterm_color(outfd, VCOLOR_BG_YELLOW),
                    (now->tv_sec / INT64_C(3600)) % INT64_C(24),
                    (now->tv_sec / INT64_C(60)) % INT64_C(60),
                    now->tv_sec % INT64_C(60), now->tv_usec / INT64_C(1000),
                    data->scolor_reset);

            /* check if loop timeout has expired */
            if (data->opts->timeout > 0 && timercmp(now, &(data->timeout_time), >=)) {
                callback_ret = VTERM_SCREEN_CB_EXIT;
                break ;
            }

            /* redisplay on columns/lines change */
            if (timercmp(now, &(data->next_wincheck_time), >=)) {
                unsigned int new_cols, new_rows;
                timeradd(now, &(data->wincheck_interval), &(data->next_wincheck_time));
                if (vterm_get_winsize(outfd, &new_rows, &new_cols) == VTERM_OK) {
                    if ((new_rows != data->rows || new_cols != data->columns)
                    && new_rows >= VSENSORS_SCREEN_ROWS_MIN
                    && new_cols >= VSENSORS_SCREEN_COLS_MIN) {
                        data->rows = new_rows;
                        data->columns = new_cols;
                        data->page |= VSENSOR_COMPUTE;
                    }
                }
            }

            /* check if sensors should be updated */
            if (timercmp(now, &(data->next_sensorscheck_time), >=)) {
                timeradd(&(data->sensors_interval), now, &(data->next_sensorscheck_time));
                data->page |= VSENSOR_CHECK_UPDATES;
            }

            break ;

        case VTERM_SCREEN_INPUT: {
            const char *    key = evdata->input.key_buffer;
            unsigned int    uread = evdata->input.key_size;

            /* check input */
            if (evdata->input.key == VTERM_KEY_EMPTY) {
                if (FD_ISSET(STDIN_FILENO, &(evdata->input.fdset_in))) {
                    ret     = read(STDIN_FILENO, key_buf, sizeof(key_buf));
                    uread   = ret > 0 ? ret : 0;
                    key     = key_buf;
                } else {
                    uread = 0;
                }
            }
            if (uread <= 0) {
                break ;
            }
            #if _DEBUG
            if (header_col + header_len + 1 + page_len + 1 + dbg_colsz < data->columns) {
                /* display pressed key */
                flockfile(out);
                vterm_goto(out, 0, data->columns - dbg_key_sz - 1);
                if (uread == 1 && isprint(*key)) {
                    fprintf(out, "%skey '%c' 0x%02x%s   ", vterm_color(outfd, VCOLOR_RED),
                            *key, (unsigned char)(*key), data->scolor_reset);
                } else if (uread < 10) {
                    uint64_t n = 0;
                    for (unsigned int i = 0; i < 5 && i < uread; i++)
                        n = (n << UINT64_C(8)) + ((unsigned char *)key)[i];
                    fprintf(out, "%skey%10" PRIx64 "#%u%s", vterm_color(outfd, VCOLOR_RED),
                            n, uread, data->scolor_reset);
                } else {
                    fprintf(out, "%skey>#%d%s        ", vterm_color(outfd, VCOLOR_RED), uread,
                            data->scolor_reset);
                }
                funlockfile(out);
            }
            #endif
            if (vterm_cap_check(outfd, VTERM_KEY_UP, key, uread)) {
                vsensors_select_sensor(data, -1, VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_DOWN, key, uread)) {
                vsensors_select_sensor(data, +1, VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_LEFT, key, uread)) {
                vsensors_select_sensor(data, -(data->end_row - data->start_row + 1),
                        VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_RIGHT, key, uread)) {
                vsensors_select_sensor(data, +(data->end_row - data->start_row + 1),
                        VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_SH_LEFT, key, uread)) {
                vsensors_select_sensor(data, -10, VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_SH_RIGHT, key, uread)) {
                vsensors_select_sensor(data, +10, VSH_RELATIVE);
            } else if (vterm_cap_check(outfd, VTERM_KEY_BACKSPACE, key, uread)) {
                uread = 1;
                key = key_buf;
                *key_buf = 0x08;
            } else if (vterm_cap_check(outfd, VTERM_KEY_TAB, key, uread)) {
                uread = 1;
                key = key_buf;
                *key_buf = 0x09;
            } else if (vterm_cap_check(outfd, VTERM_KEY_ESC, key, uread)) {
                uread = 1;
                key = key_buf;
                *key_buf = 27;
            } else if (vterm_cap_check(outfd, VTERM_KEY_PAGEUP, key, uread)) {
                uread = 1;
                key = key_buf;
                *key_buf = 'p';
            } else if (vterm_cap_check(outfd, VTERM_KEY_PAGEDOWN, key, uread)) {
                uread = 1;
                key = key_buf;
                *key_buf = 'n';
            }
            if (uread != 1) {
                break ;
            }
            switch(*key) {
                case 'q': case 'Q': case 27:
                    vsensors_please_wait(data);
                    callback_ret = VTERM_SCREEN_CB_EXIT;
                    break ;
                case 'p': case 'P':
                    if ((data->page & VSENSOR_ALL_PAGE_MASK) <= 1)
                        data->page = (data->page & ~VSENSOR_ALL_PAGE_MASK) | data->nbpages;
                    else
                        --(data->page);
                    data->page |= VSENSOR_DRAW;
                    break ;
                case 'n': case 'N':
                    if ((data->page & VSENSOR_ALL_PAGE_MASK) >= data->nbpages)
                        data->page = (data->page & ~VSENSOR_ALL_PAGE_MASK) | 1;
                    else
                        ++(data->page);
                    data->page |= VSENSOR_DRAW;
                    break ;
                case 'x': case 'X': case 9:
                    vsensors_please_wait(data);
                    vsensors_lock_update(data);
                    if (data->nbcol_per_page == 1)
                        data->nbcol_per_page = 0;
                    else
                        data->nbcol_per_page = 1;
                    data->page = 1 | VSENSOR_DRAW;
                    vsensors_display_compute(data);
                    data->wselected = NULL;
                    vsensors_unlock_update(data);
                    break ;
                case 'a': case 'A': {
                    static char         prompt[64];
                    static char         prompt_tm[64];
                    static const char * prompt_str = "sensors to add: ";
                    static const char * prompt_tm_str = "sensors timer (ms): ";
                    static unsigned int prompt_len = UINT_MAX;
                    static unsigned int prompt_tm_len = UINT_MAX;
                    char            buf_tm[15];
                    unsigned int    maxlen = sizeof(buf) / sizeof(*buf);
                    unsigned int    prompt_flags = VTERM_PROMPT_ERASE;
                    unsigned long   timervalue;
                    sensor_watch_t  watch;

                    /* lock update loop */
                    vsensors_lock_update(data);

                    if (prompt_len == UINT_MAX) {
                        snprintf(prompt, sizeof(prompt) / sizeof(*prompt),
                                "%s" "%s" "%s%s",
                                vterm_color(outfd, VCOLOR_RED), prompt_str,
                                vterm_color(outfd, VCOLOR_WHITE),
                                vterm_color(outfd, VCOLOR_BG_BLUE));
                        prompt_len = strlen(prompt_str);

                        snprintf(prompt_tm, sizeof(prompt_tm) / sizeof(*prompt_tm),
                                "%s" "%s" "%s%s",
                                vterm_color(outfd, VCOLOR_RED), prompt_tm_str,
                                vterm_color(outfd, VCOLOR_WHITE),
                                vterm_color(outfd, VCOLOR_BG_BLUE));
                        prompt_tm_len = strlen(prompt_tm_str);
                    }

                    if (*key == 'A' && data->wselected != NULL) {
                        snprintf(buf, maxlen, "%s/%s", vsensors_fam_name(data->wselected),
                                vsensors_label(data->wselected));
                        prompt_flags |= VTERM_PROMPT_WITH_DEFAULT;
                    }

                    data->page |= VSENSOR_DRAW_SPECIAL; /* status bar erased */
                    vterm_goto(out, data->rows - 1, 0);
                    if ((ret = vterm_prompt(prompt, stdin, out, buf,
                                    (0 + prompt_len + maxlen - 1 > data->columns
                                     ? data->columns - prompt_len /*- 1*/ : maxlen),
                                    prompt_flags, NULL)) <= 0) {
                        vsensors_unlock_update(data);
                        break ;
                    }

                    /* prompt sensor timer */
                    maxlen = sizeof(buf_tm);

                    if (*key == 'A' && data->wselected != NULL) {
                        snprintf(buf_tm, maxlen, "%lu", sensor_watch_timerms(data->wselected));
                    } else {
                        str0cpy(buf_tm, "1000", maxlen);
                    }

                    vterm_goto(out, data->rows - 1, 0);
                    if ((ret = vterm_prompt(prompt_tm, stdin, out, buf_tm,
                                    (0 + prompt_tm_len + maxlen - 1 > data->columns
                                     ? data->columns - prompt_tm_len /*- 1*/ : maxlen),
                                    VTERM_PROMPT_ERASE | VTERM_PROMPT_WITH_DEFAULT,
                                    NULL)) > 0
                    &&  vstrtoul(buf_tm, NULL, 0, &timervalue) == 0) {
                        watch = SENSOR_WATCH_INITIALIZER(timervalue, NULL);

                        vsensors_please_wait(data);
                        sensor_watch_add(data->sctx, buf,
                                *key == 'A' ? SSF_DEFAULT & ~SSF_CASEFOLD
                                            : SSF_DEFAULT | SSF_CASEFOLD, &watch);

                        data->page |= VSENSOR_COMPUTE;
                    }
                    vsensors_unlock_update(data);
                    break ;
                }
                case 'd': case 'D': {
                    static char prompt[64];
                    static const char * prompt_str = "sensors to delete: ";
                    static unsigned int prompt_len = UINT_MAX;
                    unsigned int prompt_flags = VTERM_PROMPT_ERASE;
                    unsigned int maxlen = sizeof(buf) / sizeof(*buf);

                    /* finish update loop */
                    vsensors_lock_update(data);

                    if (prompt_len == UINT_MAX) {
                        snprintf(prompt, sizeof(prompt) / sizeof(*prompt),
                                "%s" "%s" "%s%s",
                                vterm_color(outfd, VCOLOR_RED), prompt_str,
                                vterm_color(outfd, VCOLOR_WHITE),
                                vterm_color(outfd, VCOLOR_BG_BLUE));
                        prompt_len = strlen(prompt_str);
                    }

                    if (*key == 'D' && data->wselected != NULL) {
                        snprintf(buf, maxlen, "%s/%s", vsensors_fam_name(data->wselected),
                                vsensors_label(data->wselected));
                        prompt_flags |= VTERM_PROMPT_WITH_DEFAULT;
                    }

                    data->page |= VSENSOR_DRAW_SPECIAL; /* status bar erased */
                    vterm_goto(out, data->rows - 1, 0);
                    ret = vterm_prompt(prompt, stdin, out, buf,
                            (0 + prompt_len + maxlen - 1 > data->columns
                             ? data->columns - prompt_len /*- 1*/ : maxlen),
                            prompt_flags, NULL);

                    if (ret > 0) {
                        vsensors_please_wait(data);
                        data->wselected = NULL;
                        sensor_watch_del(data->sctx, buf,
                                *key == 'D' ? SSF_DEFAULT & ~SSF_CASEFOLD
                                            : SSF_DEFAULT | SSF_CASEFOLD);

                        data->page |= VSENSOR_COMPUTE;
                    }
                    vsensors_unlock_update(data);
                    break ;
                }
                case 't': case 'T':
                    data->term_darkmode = (data->term_darkmode + 1) % 2;
                    vsensors_please_wait(data);
                    data->page = 1 | VSENSOR_COMPUTE;
                    break ;
                case 'O':
                    if ((data->opts->flags & FLAG_SB_ONLYWATCHED) == 0)
                        data->opts->flags |= FLAG_SB_ONLYWATCHED;
                    else
                        data->opts->flags &= ~FLAG_SB_ONLYWATCHED;
                    if ((data->page & VSENSOR_SPEC_PAGE_MASK) == VSENSOR_HELP_PAGES)
                        data->page |= VSENSOR_DRAW;
                    break ;
                case '?':
                    if ((data->page & VSENSOR_SPEC_PAGE_MASK) == VSENSOR_HELP_PAGES) {
                        data->page &= ~VSENSOR_HELP_PAGES;
                        data->nbpages = data->sensors_nbpages;
                        data->page = (data->page & ~VSENSOR_ALL_PAGE_MASK) | data->sensors_page;
                    } else if ((data->page & VSENSOR_SPEC_PAGE_MASK) == 0) {
                        data->sensors_page = (data->page & VSENSOR_ALL_PAGE_MASK);
                        data->page = (data->page & ~VSENSOR_ALL_PAGE_MASK) | 1| VSENSOR_HELP_PAGES;
                        data->nbpages = 1;
                    }
                    data->page |= VSENSOR_DRAW;
                    break ;
                case 0x1a:
                    kill(0, SIGTSTP);
                    break ;
                default:
                    if (*key >= '1' && *key <= '9'
                    &&  (unsigned char) (*key - '0') <= data->nbpages) {
                        data->page = (*key - '0') | VSENSOR_DRAW;
                    }
                    break ;
            }
            break ;
        }
        case VTERM_SCREEN_END:
            /* kill update job */
            vsensors_update_stop(data);
            break ;

        case VTERM_SCREEN_EXIT:
            /* re-enable logging */
            LOG_VERBOSE(data->log, "closing screen log file...");
            logpool_enable(data->opts->logs, NULL, 1, NULL);
            logpool_replacefile(data->opts->logs, data->logpool_backup, NULL, NULL);
            LOG_INFO(data->log, "screen log file closed, logs restored.");
            logpool_logpath_free(data->opts->logs, data->logpool_backup);
            data->logpool_backup = NULL;
            break ;
    }
    return callback_ret;
}

/* ************************************************************************ */
int vsensors_screen_loop(
                options_t *     opts,
                sensor_ctx_t *  sctx,
                log_t *         log,
                FILE *          out) {
    vsensors_display_data_t data;
    int                     ret;

    if (out == NULL) {
        return -1;
    }

    data = (vsensors_display_data_t) {
        .opts = opts, .log = log, .sctx = sctx, .logpool_backup = NULL,
        .out = out, .outfd=fileno(out),
        .wincheck_interval = { .tv_sec = VSENSORS_WINCHECK_MS / 1000,
                               .tv_usec = (VSENSORS_WINCHECK_MS % 1000) * 1000 },
        .page = 1 | VSENSOR_DRAW, .wselected = NULL, .nbcol_per_page = 0,
        .nrefresh = 0, .nupdates = 0, .term_darkmode = -1 };

    /* fails if the screen is too little */
    if (out == NULL || vterm_get_winsize(data.outfd, &data.rows, &data.columns) != VTERM_OK
    ||  data.columns < VSENSORS_SCREEN_COLS_MIN || data.rows < VSENSORS_SCREEN_ROWS_MIN) {
        LOG_WARN(log, "watch loop: screen is not terminal or is too small(cols<%d or row<%d",
                 VSENSORS_SCREEN_COLS_MIN, VSENSORS_SCREEN_ROWS_MIN);
        return -1;
    }

    /* colors */
    data.scolor_reset = vterm_color(data.outfd, VCOLOR_RESET);
    if ((data.scolor_header = vterm_buildcolor(data.outfd,
            VCOLOR_BUILD(VCOLOR_WHITE, VCOLOR_BG_BLACK, VCOLOR_EMPTY), NULL, NULL)) == NULL
    ||  (data.scolor_wselected = vterm_buildcolor(data.outfd,
            VCOLOR_BUILD(VCOLOR_WHITE, VCOLOR_BG_RED, VCOLOR_BOLD), NULL, NULL)) == NULL) {
        if (data.scolor_header != NULL)
            free(data.scolor_header);
        LOG_WARN(log, "cannot init colors: %s", strerror(errno));
        return -1;
    }

    if (pthread_mutex_init(&(data.update_mutex), NULL) != 0
    ||  (pthread_cond_init(&(data.update_cond), NULL) != 0
         && ( pthread_mutex_destroy(&(data.update_mutex)) || 1))) {
        LOG_ERROR(log, "cannot init update_job locks");
        free(data.scolor_header);
        free(data.scolor_wselected);
        return -1;
    }

    /* precompute the position of each sensor to speed up display loop */
    if ((ret = vsensors_display_compute(&data)) == 0) {
        LOG_VERBOSE(log, "display_compute> rows:%u cols:%u row0:%u rowN:%u "
                         "col0:%u colN:%u cSpc:%u colS:%u timerms:%u",
                  data.rows, data.columns, data.start_row, data.end_row,
                  data.start_col, data.end_col, data.space_col, data.col_size,
                  (unsigned int) data.timer_ms);

        /* run display loop */
        ret = vterm_screen_loop(out, data.timer_ms, NULL, vsensors_display, &data);
    }

    /* finish update job */
    pthread_cond_destroy(&(data.update_cond));
    pthread_mutex_destroy(&(data.update_mutex));

    /* free allocated watchs private data */
    sensor_lock(data.sctx, SENSOR_LOCK_READ);
    SLISTC_FOREACH_DATA(sensor_watch_list_get(data.sctx), watch, sensor_sample_t *) {
        watch->userfreefun = NULL;
        vsensors_watch_priv_free(watch);
    }
    sensor_unlock(data.sctx);

    if (data.scolor_header != NULL)
        free(data.scolor_header);
    if (data.scolor_wselected != NULL)
        free(data.scolor_wselected);

    return ret == VTERM_OK ? 0 : -1;
}

