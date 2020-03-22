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

#include "vlib/term.h"
#include "vlib/log.h"
#include "vlib/util.h"

#include "libvsensors/sensor.h"

#include "version.h"
#include "vsensors.h"

/** display context for vsensors_display */
typedef struct {
    sensor_ctx_t *  sctx;
    slist_t *       watchs;
    options_t *     opts;
    int             nrefresh;
    int             nupdates;
    unsigned int    page;
    unsigned int    nbpages;
    unsigned int    columns;
    unsigned int    rows;
    unsigned int    start_row;
    unsigned int    end_row;
    unsigned int    start_col;
    unsigned int    end_col;
    unsigned int    space_col;
    unsigned int    col_size;
    unsigned int    nbcol_per_page;
    struct timeval  start_time;
} vsensors_display_data_t;

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

#define VSENSOR_DRAW            (0x80000000)
#define VSENSOR_PAGE_MASK       (~VSENSOR_DRAW)

/** display data for each sensor value (sensor_sample_t.priv) */
typedef struct {
    unsigned int idx;
    unsigned int row;
    unsigned int col;
    unsigned int page;
    char *       label;
} vsensors_watch_display_t;

/** get sensor name */
static const char * s_null_name = "(null)";
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

/** compute placement of each sensor */
static int vsensors_display_compute(
                vsensors_display_data_t *   data,
                FILE *                      out) {

    char                        name[1024];
    vsensors_watch_display_t    watch_data;
    unsigned int                nb_per_page, nb_per_col;
    int                         ret;
    int                         outfd = fileno(out);
    ssize_t                     len;
    unsigned int                i;
    size_t                      maxlen;

    data->nbpages = 1;
    data->start_row = 2;
    data->start_col = 1;
    data->space_col = 4;
    data->end_row = data->rows - 3;
    data->end_col = data->columns - 1;
    data->col_size = VSENSOR_DISPLAY_COLS_MIN;

    nb_per_col = data->end_row - data->start_row;
    data->end_row = data->start_row + nb_per_col;

    if (data->nbcol_per_page == 0) {
        data->nbcol_per_page = (data->end_col - data->start_col + 1 - data->space_col)
                               / data->col_size;
        if (data->nbcol_per_page == 0)
            data->nbcol_per_page = 1;
    }
    nb_per_page = data->nbcol_per_page * nb_per_col;

    data->col_size = (data->end_col - data->start_col + 1
                      - (data->nbcol_per_page - 1) * data->space_col
                     ) / (data->nbcol_per_page);

    ret = 0;
    watch_data.idx = 0;
    watch_data.row = data->start_row;
    watch_data.col = data->start_col;
    watch_data.page = 1;

    maxlen = 0;
    SLIST_FOREACH_DATA(data->watchs, watch, sensor_sample_t *) {
        unsigned int sz = strlen(vsensors_fam_name(watch)) + 1 + strlen(vsensors_label(watch));
        if (sz > maxlen)
            maxlen = sz;
    }
    if (maxlen > data->col_size - 1 - VSENSOR_DISPLAY_PAD_VAL)
        maxlen = data->col_size - 1 - VSENSOR_DISPLAY_PAD_VAL;

    SLIST_FOREACH_DATA(data->watchs, watch, sensor_sample_t *) {
        if (watch == NULL)
            continue ;
        /** allocate sensor user private data */
        if (watch->priv == NULL
        &&  (watch->priv = malloc(sizeof(vsensors_watch_display_t))) == NULL) {
            ret = -1;
            break ;
        }
        /* compute display position of sensor */
        if (watch_data.row > data->end_row) {
            /* row exceeded, must change columns or page */
            if (watch_data.col + 2 * data->col_size + data->space_col > data->end_col) {
                /* must change page */
                watch_data.col = data->start_col;
                watch_data.row = data->start_row;
                ++(watch_data.page);
                ++(data->nbpages);
            } else {
                /* must change column */
                watch_data.row = data->start_row;
                watch_data.col += data->col_size + data->space_col;
            }
        }
        /* Prepare label of sensor */
        len = VLIB_SNPRINTF(len, name, sizeof(name)/sizeof(*name), "%s/%s%s",
                    vsensors_fam_name(watch),
                    vterm_color(outfd, VCOLOR_YELLOW),
                    vsensors_label(watch));
        for (i = len;   i + 1 < sizeof(name) / sizeof(*name)
                     && i - vterm_color_size(outfd, VCOLOR_YELLOW) < maxlen;i++) {
            name[i] = ' ';
        }
        name[i]=0;
        watch_data.label = NULL;
        len = asprintf(&watch_data.label,
                 "%s%s:%s", //VSENSOR_DISPLAY_FMT,
                 name,
                 vterm_color(outfd, VCOLOR_RESET),
                 vterm_color(outfd, VCOLOR_GREEN));
        if (len >= 0 && watch_data.label != NULL) {
            len -= vterm_color_size(outfd, VCOLOR_YELLOW)
                   + vterm_color_size(outfd, VCOLOR_RESET) + vterm_color_size(outfd, VCOLOR_GREEN);

            if (len > data->col_size - VSENSOR_DISPLAY_PAD_VAL) {
                sprintf(watch_data.label
                        + data->col_size - VSENSOR_DISPLAY_PAD_VAL -2/*'..'*/ -1/*':'*/
                        + vterm_color_size(outfd, VCOLOR_YELLOW),
                        "..%s:%s", vterm_color(outfd, VCOLOR_RESET),
                        vterm_color(outfd, VCOLOR_GREEN));
            }
        }
        /* copy computed data to sensor user private data and go to next one */
        memcpy(watch->priv, &watch_data, sizeof(watch_data));

        ++(watch_data.row);
        ++(watch_data.idx);
    }

    return ret;
}

/** display one sensor at its position */
static int vsensors_display_one_sensor(
                sensor_sample_t *           sensor,
                vsensors_display_data_t *   data,
                FILE *                      out,
                int                         outfd) {

    vsensors_watch_display_t * wdata;

    if (sensor == NULL || sensor->priv == NULL)
        return -1;

    wdata = (vsensors_watch_display_t *) sensor->priv;

    if (wdata->page == (data->page & VSENSOR_PAGE_MASK)) {
        char buf[VSENSOR_DISPLAY_PAD_VAL];

        sensor_value_tostring(&sensor->value, buf, sizeof(buf) / sizeof(*buf));

        vterm_goto(out, wdata->row, wdata->col);
        fprintf(out, "%s%" STR(VSENSOR_DISPLAY_PAD_VAL) "s%s",
                wdata->label != NULL ? wdata->label : "null", buf,
                vterm_color(outfd, VCOLOR_RESET));
    }

    return 0;
}

static int vsensors_display(vterm_screen_event_t event, FILE * out,
                            struct timeval * now, fd_set *infdset, void * vdata) {
    static const char * const   header = "[q:quit n:next p:prev x:expand]";
    static const unsigned int   header_col = 26;
    static unsigned int         header_len;
    vsensors_display_data_t *   data    = (vsensors_display_data_t *) vdata;
    int                         outfd   = fileno(out);
    int                         ret;
    struct timeval              elapsed;

    switch (event) {
        case VTERM_SCREEN_START:
            /* set start time */
            data->start_time = *now;
            /* disable LOGGING */
            logpool_enable(data->opts->logs, NULL, 0, NULL);
            /* print header */
            header_len = strlen(header);
            if (data->columns < header_col + strlen(header))
                vterm_goto(out, 1, 0);
            else
                vterm_goto(out, 0, header_col);
            fprintf(out, "%s%s%s%s", vterm_color(outfd, VCOLOR_WHITE),
                    vterm_color(outfd, VCOLOR_BG_BLACK),
                    header, vterm_color(outfd, VCOLOR_RESET));
            break ;

        case VTERM_SCREEN_LOOP: {
            /* check if loop timeout has expired */
            if (data->opts->timeout > 0) {
                timersub(now, &data->start_time, &elapsed);
                if (elapsed.tv_sec * 1000UL + elapsed.tv_usec / 1000UL > data->opts->timeout) {
                    return VTERM_SCREEN_END;
                }
            }
            ++(data->nrefresh); /* number of times the display loop ran */

            /* redisplay on columns/lines change : does not work*/
            #if 0
            if (vterm_get_columns(outfd) != (int)data->columns
            ||  vterm_get_lines(outfd) != (int)data->rows) {
                data->rows = vterm_get_lines(outfd);
                data->columns = vterm_get_columns(outfd);
                data->nbcol_per_page = 0;
                vsensors_display_compute(data, out);
                data->page |= VSENSOR_DRAW;
            }
            #endif
            /* redisplays sensors on page change */
            if ((data->page & VSENSOR_DRAW) != 0) {
                data->page = data->page & ~VSENSOR_DRAW;
                for (unsigned int r = data->start_row; r <= data->end_row; r++) {
                    vterm_goto(out, r, data->start_col);
                    for (unsigned int c = data->start_col; c <= data->end_col; c++)
                        fputc(' ', out);
                }
                fflush(out);
                SLIST_FOREACH_DATA(data->watchs, sensor, sensor_sample_t *) {
                    vsensors_display_one_sensor(sensor, data, out, outfd);
                }
            }

            /* Show current page and total pages */
            if (data->columns <= header_col + header_len + 9)
                vterm_goto(out, 1, data->columns - 9);
            else
                vterm_goto(out, 0, header_col + header_len + 1);
            fprintf(out, "%s%s[%3u/%-3u]%s", vterm_color(outfd, VCOLOR_WHITE),
                    vterm_color(outfd, VCOLOR_BG_BLUE),
                    data->page, data->nbpages,
                    vterm_color(outfd, VCOLOR_RESET));

            #if _DEBUG
            /* number of times the display loop ran */
            vterm_goto(out, data->rows - 1, 14);
            ret = fprintf(out, "nrefresh:%s%s%010d%s",
                          vterm_color(outfd, VCOLOR_WHITE), vterm_color(outfd, VCOLOR_BG_BLUE),
                          data->nrefresh, vterm_color(outfd, VCOLOR_BG_BLACK));

            if (data->columns > 14 + 19 + 1 + 19) {
                /* number of time we got one or more sensors updates */
                vterm_goto(out, data->rows - 1, 14 + 19 + 1);
                fprintf(out, "nupdates:%s%s%010d%s",
                        vterm_color(outfd, VCOLOR_WHITE), vterm_color(outfd, VCOLOR_BG_GREEN),
                        data->nupdates, vterm_color(outfd, VCOLOR_RESET));
                if (data->columns >= 75) {
                    /* return value of printf */
                    vterm_goto(out, data->rows - 1, 59);
                    fprintf(out, "%d", ret);
                }
            }
            #endif

            /* Move cursor and flush display */
            vterm_goto(out, 0, data->columns - 1);
            fflush(out);

            break ;
        }
        case VTERM_SCREEN_TIMER: {
            slist_t *       updates;

            /* display current time */
            vterm_goto(out, 0,0);
            fprintf(out, "%s%s%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64 "%s",
                    vterm_color(outfd, VCOLOR_BLACK), vterm_color(outfd, VCOLOR_BG_YELLOW),
                    (now->tv_sec / INT64_C(3600)) % INT64_C(24), (now->tv_sec / INT64_C(60)) % INT64_C(60),
                    now->tv_sec % INT64_C(60), now->tv_usec / INT64_C(1000), vterm_color(outfd, VCOLOR_RESET));

            /* get sensors updates */
            ret = 0;
            updates = sensor_update_get(data->sctx, now);

            if (updates != NULL) {
                ++(data->nupdates); /* number of time we got one or more sensors updates */
                /* Display sensors updates */
                SLIST_FOREACH_DATA(updates, sensor, sensor_sample_t *) {
                    vsensors_display_one_sensor(sensor, data, out, outfd);
                    ++ret;
                }
                /* free sensor updates */
                sensor_update_free(updates);
            }

            vterm_goto(out, 0, 13);
            fprintf(out, "%sUPDATES%s: %s%s%3d%s",
                    vterm_color(1, VCOLOR_GREEN), vterm_color(outfd, VCOLOR_RESET),
                    vterm_color(1, VCOLOR_BOLD), vterm_color(outfd, VCOLOR_RED),
                    ret, vterm_color(outfd, VCOLOR_RESET));
            break ;
        }

        case VTERM_SCREEN_INPUT: {
            int     nread;
            char    buf[128];
            if (FD_ISSET(STDIN_FILENO, infdset)
            && (nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                #if _DEBUG
                /* display pressed key */
                vterm_goto(out, vterm_get_lines(outfd) - 1, 0);
                if (nread == 1 && isprint(*buf)) {
                    fprintf(out, "%skey '%c' 0x%02x%s ", vterm_color(outfd, VCOLOR_RED),
                        *buf, (unsigned char)(*buf), vterm_color(outfd, VCOLOR_RESET));
                } else if (nread < 10) {
                    unsigned long n;
                    int i;
                    for (n = 0, i = 0; i < 4 && i < nread; i++)
                        n = (n << 8UL) + ((unsigned char *)buf)[i];
                    fprintf(out, "%skey%08lx#%u%s", vterm_color(outfd, VCOLOR_RED),
                            n, nread, vterm_color(outfd, VCOLOR_RESET));
                } else {
                    fprintf(out, "%skey>#%d%s       ", vterm_color(outfd, VCOLOR_RED), nread,
                        vterm_color(outfd, VCOLOR_RESET));
                }
                #endif

                if (nread == 1) {
                    switch(*buf) {
                        case 'q': case 'Q': case 27:
                            return VTERM_SCREEN_END;
                        case 'p': case 'P':
                            if (data->page <= 1) data->page = data->nbpages; else --(data->page);
                            data->page |= VSENSOR_DRAW;
                            break ;
                        case 'n': case 'N':
                            if (data->page >= data->nbpages) data->page = 1; else ++(data->page);
                            data->page |= VSENSOR_DRAW;
                            break ;
                        case 'x': case 'X': case 9:
                            if (data->nbcol_per_page == 1)
                                data->nbcol_per_page = 0;
                            else
                                data->nbcol_per_page = 1;
                            vsensors_display_compute(data, out);
                            data->page = 1 | VSENSOR_DRAW;
                            break ;
                    }
                }
            }
            break ;
        }
        case VTERM_SCREEN_END:
            /* re-enable logging */
            logpool_enable(data->opts->logs, NULL, 1, NULL);
            break ;
    }
    return event;
}

int vsensors_screen_loop(
                options_t *     opts,
                sensor_ctx_t *  sctx,
                slist_t *       watchs,
                log_t *         log,
                FILE *          out) {
    vsensors_display_data_t data;
    unsigned int            timer_ms;
    int                     ret, fd;

    /* fails if the screen is too little */
    if (out == NULL || vterm_get_columns((fd = fileno(out))) < VSENSOR_DISPLAY_COLS_MIN + 1
        || vterm_get_lines(fd) < 5) {
        LOG_WARN(log, "watch loop: screen is not terminal or is too small(cols<%d or row<%d",
                 VSENSOR_DISPLAY_COLS_MIN+1, 5);
        return -1;
    }

    /* get PlusGrandCommunDiviseur of all sensor watchs refresh intervals */
    timer_ms = sensor_watch_pgcd(sctx);
    data = (vsensors_display_data_t) {
        .sctx = sctx, .watchs = watchs, .nrefresh = 0, .nupdates = 0,
        .page = 1 | VSENSOR_DRAW, .opts = opts, .nbcol_per_page = 0 };
    data.columns = vterm_get_columns(fileno(out));
    data.rows = vterm_get_lines(fileno(out));

    /* precompute the position of each sensor to speed up display loop
     * TODO: this has to be done again on Terminal resize */
    if ((ret = vsensors_display_compute(&data, out)) == 0) {
        LOG_VERBOSE(log, "display_compute> rows:%u cols:%u row0:%u rowN:%u "
                         "col0:%u colN:%u cSpc:%u colS:%u",
                  data.rows, data.columns, data.start_row, data.end_row,
                  data.start_col, data.end_col, data.space_col, data.col_size);

        /* run display loop */
        ret = vterm_screen_loop(out, timer_ms, NULL, vsensors_display, &data);
    }

    /* free allocated watchs private data */
    SLIST_FOREACH_DATA(watchs, watch, sensor_sample_t *) {
        if (watch->priv != NULL) {
            char * label;
            if ((label = ((vsensors_watch_display_t *)watch->priv)->label) != NULL)
                free(label);
            free(watch->priv);
        }
    }

    return ret == VTERM_OK ? 0 : -1;
}

