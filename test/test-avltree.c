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
#include <stdlib.h>

#define VLIB_AVLTREE_NODE_TESTS 1 // for avltree_node_set() test functions
#include "vlib/avltree.h"
#include "vlib/test.h"

#include "version.h"
#include "test_private.h"

/* *************** TEST AVL TREE *************** */
#define LG(val) ((void *)((long)(val)))

static inline int avlprint_node_lr(FILE * out, avltree_node_t * node, avltree_node_t * left, avltree_node_t * right) {
    if (out)
        return fprintf(out, "%ld(%ld,%ld) ", (long) avltree_node_data(node),
                    left?(long)avltree_node_data(left):-1,right?(long)avltree_node_data(right):-1);
    return 0;
}

static inline int avlprint_node(FILE * out, avltree_node_t * node) {
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    return avlprint_node_lr(out, node, left, right);
}

typedef struct {
    rbuf_t *    results;
    log_t *     log;
    FILE *      out;
    void *      min;
    void *      max;
    unsigned long nerrors;
    unsigned long ntests;
    avltree_t * tree;
} avltree_print_data_t;

static void avlprint_larg_manual(avltree_node_t * root, rbuf_t * stack, FILE * out) {
    rbuf_t *    fifo        = rbuf_create(32, RBF_DEFAULT);

    if (root) {
        rbuf_push(fifo, root);
    }

    while (rbuf_size(fifo)) {
        avltree_node_t *    node    = (avltree_node_t *) rbuf_dequeue(fifo);
        avltree_node_t *    left    = avltree_node_left(node), * right = avltree_node_right(node);

        avlprint_node_lr(out, node, left, right);

        rbuf_push(stack, node);

        if (left) {
            rbuf_push(fifo, left);
        }
        if (right) {
            rbuf_push(fifo, right);
        }
    }
    if (out) fputc('\n', out);
    rbuf_free(fifo);
}
static void avlprint_inf_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    if (left)
        avlprint_inf_left(left, stack, out);

    avlprint_node_lr(out, node, left, right);

    rbuf_push(stack, node);
    if (right)
        avlprint_inf_left(right, stack, out);
}
static void avlprint_inf_right(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    if (right)
        avlprint_inf_right(right, stack, out);

    avlprint_node_lr(out, node, left, right);

    rbuf_push(stack, node);
    if (left)
        avlprint_inf_right(left, stack, out);
}
static void avlprint_pref_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    avlprint_node_lr(out, node, left, right);
    rbuf_push(stack, node);
    if (left)
        avlprint_pref_left(left, stack, out);
    if (right)
        avlprint_pref_left(right, stack, out);
}
static void avlprint_suff_left(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    if (left)
        avlprint_suff_left(left, stack, out);
    if (right)
        avlprint_suff_left(right, stack, out);
    avlprint_node_lr(out, node, left, right);
    rbuf_push(stack, node);
}
static void avlprint_rec_all(avltree_node_t * node, rbuf_t * stack, FILE * out) {
    if (!node) return;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    avlprint_node_lr(out, node, left, right);
    rbuf_push(stack, node);
    if (left)
        avlprint_rec_all(left, stack, out);
    avlprint_node_lr(out, node, left, right);
    rbuf_push(stack, node);
    if (right)
        avlprint_rec_all(right, stack, out);
    avlprint_node_lr(out, node, left, right);
    rbuf_push(stack, node);
}
static unsigned int avlprint_rec_get_height(avltree_node_t * node) {
    if (!node) return 0;
    unsigned hl     = avlprint_rec_get_height(avltree_node_left(node));
    unsigned hr     = avlprint_rec_get_height(avltree_node_right(node));
    if (hr > hl)
        hl = hr;
    return 1 + hl;
}
static size_t avlprint_rec_get_count(avltree_node_t * node) {
    if (!node) return 0;
    return 1 + avlprint_rec_get_count(avltree_node_left(node))
             + avlprint_rec_get_count(avltree_node_right(node));
}
static unsigned int avlprint_rec_check_balance(avltree_node_t * node, log_t * log) {
    if (!node)      return 0;
    avltree_node_t * left = avltree_node_left(node), * right = avltree_node_right(node);
    long hl     = avlprint_rec_get_height(left);
    long hr     = avlprint_rec_get_height(right);
    unsigned nerror = 0;
    if (hr - hl != (long)avltree_node_balance(node)) {
        LOG_ERROR(log, "error: balance %d of %ld(%ld:h=%ld,%ld:h=%ld) should be %ld",
                  avltree_node_balance(node), (long) avltree_node_data(node),
                  left  ? (long) avltree_node_data(left)  : -1L,  hl,
                  right ? (long) avltree_node_data(right) : -1L, hr,
                  hr - hl);
        nerror++;
    } else if (hr - hl > 1L || hr - hl < -1L) {
        LOG_ERROR(log, "error: balance %ld (in_node:%d) of %ld(%ld:h=%ld,%ld:h=%ld) is too big",
                  hr - hl, avltree_node_balance(node), (long) avltree_node_data(node),
                  left  ? (long) avltree_node_data(left)  : -1L,  hl,
                  right ? (long) avltree_node_data(right) : -1L, hr);
        nerror++;
    }
    return nerror + avlprint_rec_check_balance(left, log)
                  + avlprint_rec_check_balance(right, log);
}
static void * avltree_rec_find_min(avltree_node_t * node) {
    if (node == NULL)
        return (void *) LONG_MIN;
    avltree_node_t * left = avltree_node_left(node);
    if (left != NULL) {
        return avltree_rec_find_min(left);
    }
    return avltree_node_data(node);
}
static void * avltree_rec_find_max(avltree_node_t * node) {
    if (node == NULL)
        return (void *) LONG_MAX;
    avltree_node_t * right = avltree_node_right(node);
    if (right != NULL) {
        return avltree_rec_find_max(right);
    }
    return avltree_node_data(node);
}
static AVLTREE_DECLARE_VISITFUN(visit_print, node_data, context, user_data) {

    static const unsigned long visited_mask = (1UL << ((sizeof(node_data))* 8UL - 1UL));

    avltree_node_t * node = context->node, * left = avltree_node_left(node), * right = avltree_node_right(node);
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;
    long value = (long)(((unsigned long) node_data) & ~(visited_mask));
    long previous;

    if ((context->state & AVH_MERGE) != 0) {
        avltree_print_data_t * job_data = (avltree_print_data_t *) context->data;
        if (avltree_count(context->tree) < 1000) {
            while (rbuf_size(job_data->results)) {
                rbuf_push(stack, rbuf_pop(job_data->results));
            }
            rbuf_free(job_data->results);
            job_data->results = NULL;
        } else {
            data->nerrors += job_data->nerrors;
        }
        return AVS_CONTINUE;
    }

    if ((context->how & AVH_MERGE) != 0 && stack == NULL) {
        if (avltree_count(context->tree) < 1000) {
            data->results = stack = rbuf_create(32, RBF_DEFAULT);
        } else {
            ++(data->nerrors);
        }
    }

    previous = rbuf_size(stack)
                    ? (long)((unsigned long)(avltree_node_data((avltree_node_t *) rbuf_top(stack)))
                             & ~(visited_mask))
                    : LONG_MAX;

    if ((context->how & (AVH_PREFIX|AVH_SUFFIX)) == 0) {
        switch (context->state) {
            case AVH_INFIX:
                if ((context->how & AVH_RIGHT) == 0) {
                    if (previous == LONG_MAX) previous = LONG_MIN;
                    if (value < previous) {
                        if (data->out != NULL)
                            LOG_INFO(data->log, NULL);
                        LOG_ERROR(data->log, "error: bad tree order node %ld < prev %ld",
                                  value, previous);
                        return AVS_ERROR;
                    }
                } else if (value > previous) {
                    if (data->out != NULL)
                        LOG_INFO(data->log, NULL);
                    LOG_ERROR(data->log, "error: bad tree order node %ld > prev %ld",
                              value, previous);
                    return AVS_ERROR;
                }
                break ;
            default:
                break ;
        }
    }
    if (data->out) fprintf(data->out, "%ld(%ld,%ld) ", value,
            left  ? (long)((unsigned long)(avltree_node_data(left))  & ~(visited_mask)) : -1,
            right ? (long)((unsigned long)(avltree_node_data(right)) & ~(visited_mask)) : -1);
    rbuf_push(stack, node);

    if (stack == NULL && (context->how & AVH_MERGE) == 0) {
        if (((unsigned long)node_data & visited_mask) != 0) {
            LOG_WARN(data->log, "node %ld (0x%lx) already visited !", value, (unsigned long) node);
            ++(data->nerrors);
        }
        avltree_node_set_data(node, (void *) ((unsigned long)(node_data) | visited_mask));
    }

    return AVS_CONTINUE;
}
static AVLTREE_DECLARE_VISITFUN(visit_checkprint, node_data, context, user_data) {
    static const unsigned long visited_mask = (1UL << ((sizeof(node_data))* 8UL - 1UL));

    avltree_node_t *        node = context->node;
    avltree_print_data_t *  data = (avltree_print_data_t *) user_data;

    ++(data->ntests);
    if (((unsigned long)(node_data) & visited_mask) != 0) {
        avltree_node_set_data(node, (void *) ((unsigned long)(node_data) & ~(visited_mask)));
    } else {
        ++(data->nerrors);
        LOG_ERROR(data->log, "error: node %ld (0x%lx) has not been visited !",
                  (long) node_data, (unsigned long) node);
    }

    return AVS_CONTINUE;
}
static AVLTREE_DECLARE_VISITFUN(visit_range, node_data, context, user_data) {
    avltree_node_t * node = context->node;
    avltree_print_data_t * data = (avltree_print_data_t *) user_data;
    rbuf_t * stack = data->results;

    avlprint_node(data->out, node);

    if (context->how == AVH_INFIX) {
        /* avltree_visit(INFIX) mode */
        if ((long) node_data >= (long) data->min && (long) node_data <= (long) data->max) {
            rbuf_push(stack, node);
            if ((long) node_data > (long) data->max) {
                return AVS_FINISHED;
            }
        }
    } else {
        /* avltree_visit_range(min,max) mode */
        if ((long) node_data < (long) data->min || (long) node_data > (long) data->max) {
            LOG_ERROR(data->log, "error: bad tree order: node %ld not in range [%ld:%ld]",
                      (long) node_data, (long) data->min, (long) data->max);
            return AVS_ERROR;
        }
        rbuf_push(stack, node);
    }
    return AVS_CONTINUE;
}
static avltree_node_t *     avltree_node_insert_rec(
                                avltree_t * tree,
                                avltree_node_t ** nodeptr,
                                void * data) {
    avltree_node_t * node = avltree_node_get(nodeptr), * new;
    int cmp;

    if (node == NULL) {
        if ((new = avltree_node_create(tree, data, node, NULL)) != NULL) {
            avltree_node_set(nodeptr, new);
            ++tree->n_elements;
        } else if (errno == 0) {
            errno = ENOMEM;
        }
        return new;
    } else if ((cmp = tree->cmp(data, avltree_node_data(node))) <= 0) {
        new = avltree_node_insert_rec(tree, avltree_node_left_ptr(node), data);
    } else {
        new = avltree_node_insert_rec(tree, avltree_node_right_ptr(node), data);
    }
    return new;
}

static void * avltree_insert_rec(avltree_t * tree, void * data) {
    avltree_node_t * node;

    if (tree == NULL) {
        errno = EINVAL;
        return NULL;
    }
    node = avltree_node_insert_rec(tree, &(tree->root), data);
    if (node != NULL) {
        if (data == NULL) {
            errno = 0;
        }
        return data;
    }
    if (errno == 0) {
        errno = EAGAIN;
    }
    return NULL;
}

enum {
    TAC_NONE        = 0,
    TAC_NO_ORDER    = 1 << 0,
    TAC_REF_NODE    = 1 << 1,
    TAC_RES_NODE    = 1 << 2,
    TAC_RESET_REF   = 1 << 3,
    TAC_RESET_RES   = 1 << 4,
    TAC_DEFAULT     = TAC_RES_NODE | TAC_REF_NODE | TAC_RESET_RES | TAC_RESET_REF,
} test_avltree_check_flags_t;

static unsigned int avltree_check_results(rbuf_t * results, rbuf_t * reference,
                                          log_t * log, int flags) {
    unsigned int nerror = 0;

    if (rbuf_size(results) != rbuf_size(reference)) {
        LOG_ERROR(log, "error: results size:%zu != reference size:%zu",
                  rbuf_size(results), rbuf_size(reference));
        return 1;
    }
    for (size_t i_ref = 0, n = rbuf_size(reference); i_ref < n; ++i_ref) {
        void * refdata, * resdata;
        long ref, res;

        refdata = rbuf_get(reference, i_ref);
        ref = (flags & TAC_REF_NODE) != 0
              ? (long) (avltree_node_data((avltree_node_t *)refdata)) : (long) refdata;

        if ((flags & TAC_NO_ORDER) == 0) {
            resdata = rbuf_get(results, i_ref);
            res = (flags & TAC_RES_NODE) != 0
                  ? (long)(avltree_node_data((avltree_node_t *)resdata)) : (long) resdata;

            if (log)
                LOG_SCREAM(log, "check REF #%zu n=%zu pRef=%lx ref=%ld pRes=%lx res=%ld flags=%u\n",
                       i_ref, n, (unsigned long) refdata, ref, (unsigned long) resdata, res, flags);

            if (res != ref) {
                LOG_ERROR(log, "error: result:%ld != reference:%ld", res, ref);
                nerror++;
            }
        } else {
            size_t i_res;
            for (i_res = rbuf_size(results); i_res > 0; --i_res) {
                resdata = rbuf_get(results, i_res - 1);
                if ((flags & TAC_RES_NODE) != 0) {
                    if (resdata == NULL)
                        continue ;
                    res = (long)(avltree_node_data((avltree_node_t *)resdata));
                } else {
                    res = (long) resdata;
                }
                if (log)
                    LOG_SCREAM(log, "check REF #%zu n=%zu pRef=%lx ref=%ld "
                                    "pRes=%lx res=%ld flags=%u\n",
                       i_ref, n, (unsigned long) refdata, ref, (unsigned long) resdata, res, flags);

                if (ref == res) {
                    rbuf_set(results, i_res - 1, NULL);
                    break ;
                }
            }
            if (i_res == 0) {
                LOG_ERROR(log, "check_no_order: elt '%ld' not visited !", ref);
                ++nerror;
            }
        }
    }
    if ((flags & TAC_RESET_REF) != 0)
        rbuf_reset(reference);
    if ((flags & TAC_RESET_RES) != 0)
        rbuf_reset(results);

    return nerror;
}

static unsigned int avltree_test_visit(avltree_t * tree, int check_balance,
                                       FILE * out, log_t * log) {
    unsigned int            nerror = 0;
    rbuf_t *                results    = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    rbuf_t *                reference  = rbuf_create(VLIB_RBUF_SZ, RBF_DEFAULT);
    avltree_iterator_t *    iterator;
    rbuf_t *                stack;
    avltree_visit_context_t*context;
    avltree_print_data_t    data = { .results = results, .out = out, .log = log };
    long                    value, ref_val;
    int                     errno_bak;
    BENCHS_DECL(tm_bench, cpu_bench);

    if (log && log->level < LOG_LVL_INFO) {
        data.out = out = NULL;
    }

        /* create iterator and check context */
    if ((iterator = avltree_iterator_create(tree, AVH_PREFIX)) == NULL
    ||  (context = avltree_iterator_context(iterator)) == NULL) {
        LOG_WARN(log, "cannot create or get tree iterator context");
        ++nerror;
        stack = NULL;
    } else {
        stack = context->stack;
    }
    avltree_iterator_abort(iterator);

    if (check_balance) {
        int n = avlprint_rec_check_balance(tree->root, log);
        if (out)
            LOG_INFO(log, "Checking balances: %u error(s).", n);
        nerror += n;
    }

    if (out) {
        fprintf(out, "LARG PRINT\n");
        avltree_print(tree, avltree_print_node_default, out);
    }

    /* BREADTH */
    if (out)
        fprintf(out, "manLARGL ");
    avlprint_larg_manual(tree->root, reference, out);

    if (out)
        fprintf(out, "LARGL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_BREADTH) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* prefix left */
    if (out)
        fprintf(out, "recPREFL ");
    avlprint_pref_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "PREFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* infix left */
    if (out)
        fprintf(out, "recINFL  ");
    avlprint_inf_left(tree->root, reference, out); if (out) fprintf(out, "\n");
    if (out)
        fprintf(out, "INFL     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* infix right */
    if (out)
        fprintf(out, "recINFR  ");
    avlprint_inf_right(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "INFR     ");
    if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* suffix left */
    if (out)
        fprintf(out, "recSUFFL ");
    avlprint_suff_left(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "SUFFL    ");
    if (avltree_visit(tree, visit_print, &data, AVH_SUFFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* prefix + infix + suffix left */
    if (out)
        fprintf(out, "recALL   ");
    avlprint_rec_all(tree->root, reference, out); if (out) fprintf(out, "\n");

    if (out)
        fprintf(out, "ALL      ");
    if (avltree_visit(tree, visit_print, &data,
                      AVH_PREFIX | AVH_SUFFIX | AVH_INFIX) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    if (out)
        LOG_INFO(log, "current tree stack maxsize = %zu", rbuf_maxsize(stack));

    /* visit_range(min,max) */
    /*   get infix left reference */
    avlprint_inf_left(tree->root, reference, NULL);
    if (out)
        fprintf(out, "RNG(all) ");
    ref_val = -1L;
    data.min = LG(avltree_find_min(tree));
    data.max = LG(avltree_find_max(tree));
    if (AVS_FINISHED !=
            (avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
       ){  //TODO || (ref_val = (long)((avltree_node_t*)rbuf_top(data.results))->data) > (long) data.max) {
        LOG_ERROR(log, "error: avltree_visit_range() error, last(%ld) <=? max(%ld) ",
                ref_val, (long)data.max);
        ++nerror;
    }
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_DEFAULT);

    /* parallel */
    if (out)
        fprintf(out, "PARALLEL ");
    data.results = NULL; /* needed because parallel avltree_visit_print */
    if (avltree_visit(tree, visit_print, &data,
                      AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED)
        nerror++;
    data.results = results;
    if (out) fprintf(out, "\n");
    data.ntests = data.nerrors = 0;
    if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
    || data.ntests != avltree_count(tree)) {
        nerror++;
    }
    nerror += data.nerrors;
    rbuf_reset(results);
    rbuf_reset(reference);

    /* parallel AND merge */
    avlprint_pref_left(tree->root, reference, NULL);
    data.results = NULL; /* needed because allocated by each job */
    if (out)
        fprintf(out, "PARALMRG ");
    if (avltree_visit(tree, visit_print, &data,
                AVH_PARALLEL_DUPDATA(AVH_PREFIX | AVH_MERGE, sizeof(data))) != AVS_FINISHED)
        nerror++;
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(data.results, reference, log, TAC_DEFAULT | TAC_NO_ORDER);
    if (data.results != NULL) {
        rbuf_free(data.results);
    }
    data.results = results;

    if (out)
        LOG_INFO(log, "current tree stack maxsize = %zu", rbuf_maxsize(stack));

    /* count */
    ref_val = avlprint_rec_get_count(tree->root);
    errno = EBUSY; /* setting errno to check it is correctly set by avltree */
    value = avltree_count(tree);
    errno_bak = errno;
    if (out)
        LOG_INFO(log, "COUNT = %ld", value);
    if (value != ref_val || (value == 0 && errno_bak != 0)) {
        LOG_ERROR(log, "error incorrect COUNT %ld, expected %ld (errno:%d)",
                  value, ref_val, errno_bak);
        ++nerror;
    }
    /* memorysize */
    value = avltree_memorysize(tree);
    if (out)
        LOG_INFO(log, "MEMORYSIZE = %ld (%.03fMB)",
                 value, value / 1000.0 / 1000.0);
    /* depth */
    if (check_balance) {
        errno = EBUSY; /* setting errno to check it is correctly set by avltree */
        value = avltree_find_depth(tree);
        errno_bak = errno;
        ref_val = avlprint_rec_get_height(tree->root);
        if (out)
            LOG_INFO(log, "DEPTH = %ld", value);
        if (value != ref_val || (value == 0 && errno_bak != 0)) {
            LOG_ERROR(log, "error incorrect DEPTH %ld, expected %ld (errno:%d)",
                      value, ref_val, errno_bak);
            ++nerror;
        }
    }
    /* min */
    /* setting errno to check it is correctly set by avltree */
    errno =  avltree_count(tree) == 0 ? 0 : EBUSY;
    value = (long) avltree_find_min(tree);
    errno_bak = errno;
    ref_val = (long) avltree_rec_find_min(tree->root);
    if (out)
        LOG_INFO(log, "MINIMUM value = %ld", value);
    if (ref_val == LONG_MIN) {
        if (value != 0 || errno_bak == 0) {
            LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld and non 0 errno (%d)",
                      value, 0L, errno_bak);
            ++nerror;
        }
    } else if (value != ref_val) {
        LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld",
                  value, ref_val);
        ++nerror;
    }
    /* max */
    /* setting errno to check it is correctly set by avltree */
    errno =  avltree_count(tree) == 0 ? 0 : EBUSY;
    value = (long) avltree_find_max(tree);
    errno_bak = errno;
    ref_val = (long) avltree_rec_find_max(tree->root);
    if (out)
        LOG_INFO(log, "MAXIMUM value = %ld", value);
    if (ref_val == LONG_MAX) {
        if (value != 0 || errno_bak == 0) {
            LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld and non 0 errno (%d)",
                      value, 0L, errno_bak);
            ++nerror;
        }
    } else if (value != ref_val) {
        LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld",
                  value, ref_val);
        ++nerror;
    }

    /* avltree_to_{slist,array,rbuf} */
    static const unsigned int hows[] = { AVH_INFIX, AVH_PREFIX, AVH_PREFIX | AVH_PARALLEL };
    static const char *       shows[] = { "infix",  "prefix",   "prefix_parallel" };
    for (size_t i = 0; i < PTR_COUNT(hows); ++i) {
        slist_t *       slist;
        rbuf_t *        rbuf;
        void **         array;
        size_t          n;
        unsigned int    flags = TAC_REF_NODE | TAC_RESET_RES;

        rbuf_reset(reference);
        rbuf_reset(results);

        if ((hows[i] & AVH_PREFIX) != 0) {
            avlprint_pref_left(tree->root, reference, NULL);
        } else {
            avlprint_inf_left(tree->root, reference, NULL);
        }
        if ((hows[i] & AVH_PARALLEL) != 0) {
            flags |= TAC_NO_ORDER;
        }

        /* avltree_to_slist */
        BENCHS_START(tm_bench, cpu_bench);
        slist = avltree_to_slist(tree, hows[i]);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_slist(%s) ", shows[i]);
        SLIST_FOREACH_DATA(slist, data, void *) {
            rbuf_push(results, data);
        }
        slist_free(slist, NULL);
        nerror += avltree_check_results(results, reference, log, flags);

        /* avltree_to_rbuf */
        BENCHS_START(tm_bench, cpu_bench);
        rbuf = avltree_to_rbuf(tree, hows[i]);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_rbuf(%s) ", shows[i]);
        for (unsigned int i = 0, n = rbuf_size(rbuf); i < n; ++i) {
            rbuf_push(results, rbuf_get(rbuf, i));
        }
        rbuf_free(rbuf);
        nerror += avltree_check_results(results, reference, log, flags);

        /* avltree_to_array */
        BENCHS_START(tm_bench, cpu_bench);
        n = avltree_to_array(tree, hows[i], &array);
        if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_to_array(%s) ", shows[i]);
        if (array != NULL) {
            for (size_t idx = 0; idx < n; ++idx) {
                rbuf_push(results, array[idx]);
            }
            free(array);
        }
        nerror += avltree_check_results(results, reference, log, flags);

        rbuf_reset(reference);
        rbuf_reset(results);
    }

    // AVLTREE_FOREACH_DATA: testing avltree_iterator_{create,next,abort}().
    // iterator: build reference stack
    avlprint_inf_left(tree->root, reference, NULL);
    BENCHS_START(tm_bench, cpu_bench);

    // iterator: AVLTREE_FOREACH_DATA(INFIX)
    AVLTREE_FOREACH_DATA(tree, it_long, long, AVH_INFIX) {
        if (out) LOG_DEBUG(log, "Iterator(inf): %ld", it_long);
        rbuf_push(results, (void*) it_long);
    }
    // iterator: check results againt reference
    if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_iterator(INFIX) %s", "");
    nerror += avltree_check_results(results, reference, log, TAC_RESET_RES | TAC_REF_NODE);

    // iterator: AVLTREE_FOREACH_DATA(INFIX), with a break after value 2.
    AVLTREE_FOREACH_DATA(tree, it_long, long, AVH_INFIX) {
        if (it_long > 2)
            break ;
        rbuf_push(results, (void*) it_long);
        LOG_DEBUG(log, "Iterator(inf): %ld", it_long);
    }
    // iterator: remove values greater than 2 in reference
    while (rbuf_size(reference) && (long) avltree_node_data(rbuf_top(reference)) > 2) {
        rbuf_pop(reference);
    }
    // iterator: check results againt reference
    nerror += avltree_check_results(results, reference, log, TAC_RESET_RES | TAC_REF_NODE);

    // iterator: build reference stack (infix right)
    rbuf_reset(reference);
    avlprint_inf_right(tree->root, reference, NULL);
    BENCHS_START(tm_bench, cpu_bench);

    // iterator: AVLTREE_FOREACH_DATA(INFIX_RIGHT)
    AVLTREE_FOREACH_DATA(tree, it_long, long, AVH_INFIX | AVH_RIGHT) {
        if (out) LOG_DEBUG(log, "Iterator(infR): %ld", it_long);
        rbuf_push(results, (void*) it_long);
    }
    // iterator: check results againt reference
    if (out) BENCHS_STOP_LOG(tm_bench, cpu_bench, log, "avltree_iterator(INFIX_RIGHT) %s", "");
    nerror += avltree_check_results(results, reference, log, TAC_RESET_RES | TAC_REF_NODE);

    // iterator: avltree_iterator_create_inrange()
    avlprint_inf_left(tree->root, reference, NULL);
    ref_val = -1L;
    data.min = LG((long)avltree_find_min(tree) * 3);
    data.max = LG((long)avltree_find_max(tree) * 0.7);
    data.results = reference;
    rbuf_reset(data.results);
    if (out)
        fprintf(out, "%8s(%ld,%ld) ", "RNG", (long) data.min, (long) data.max);
    if (AVS_FINISHED !=
            (avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
       ){
        LOG_ERROR(log, "error: avltree_visit_range() error, last(%ld) <=? max(%ld) ",
                ref_val, (long)data.max);
        ++nerror;
    }
    data.results = results;
    if ((iterator = avltree_iterator_create_inrange(tree, data.min, data.max, AVH_INFIX)) == NULL) {
        LOG_ERROR(log, "error: avltree_create_inrange() error");
        ++nerror;
    } else {
        if (out) fprintf(out, "\n%8s(%ld,%ld) ", "IT_RNG", (long) data.min, (long) data.max);
        void * data;
        while ((data = avltree_iterator_next(iterator)) != NULL || errno == 0) {
            avlprint_node(out, avltree_iterator_context(iterator)->node);
            rbuf_push(results, data);
        }
    }
    if (out) fprintf(out, "\n");
    nerror += avltree_check_results(results, reference, log, TAC_RESET_RES | TAC_REF_NODE);

    if (results)
        rbuf_free(results);
    if (reference)
        rbuf_free(reference);

    return nerror;
}

static void * avltree_test_visit_job(void * vdata) {
    avltree_print_data_t * data = (avltree_print_data_t *) vdata;
    long value;
    size_t len;
    int result, ret = AVS_FINISHED;
    rbuf_t * results_bak = data->results;
    BENCHS_DECL(tm_bench, cpu_bench);

    BENCHS_START(tm_bench, cpu_bench);
    if ((result = avltree_visit(data->tree, visit_print, data, AVH_INFIX)) != AVS_FINISHED)
        ret = AVS_ERROR;
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
                    "INFIX visit (%zd nodes,%p,%d error) | ",
                    avltree_count(data->tree), (void*) data->tree,
                    result == AVS_FINISHED ? 0 : 1);

    data->results = NULL;
    data->nerrors = data->ntests = 0;
    BENCHS_START(tm_bench, cpu_bench);
    if ((result = avltree_visit(data->tree, visit_print, data,
                                AVH_PREFIX | AVH_PARALLEL)) != AVS_FINISHED) {
        ret = AVS_ERROR;
    }
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
                    "PARALLEL_PREFIX visit (%zd nodes,%p,%d error) | ",
                    avltree_count(data->tree), (void*) data->tree,
                    result == AVS_FINISHED ? 0 : 1);

    BENCHS_START(tm_bench, cpu_bench);
    if (avltree_visit(data->tree, visit_checkprint, data, AVH_PREFIX) != AVS_FINISHED
    ||  data->nerrors != 0 || data->ntests != avltree_count(data->tree)) {
        ret = AVS_ERROR;
    }
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log,
        "check // prefix with prefix (%zd nodes,%p,%lu error%s) | ",
        avltree_count(data->tree), (void*) data->tree,
        data->nerrors, data->nerrors > 1 ? "s" : "");

    /* AVLTREE_FOREACH_DATA(INFIX) */
    value = LONG_MIN;
    len = 0;
    BENCHS_START(tm_bench, cpu_bench);
    AVLTREE_FOREACH_DATA(data->tree, it_long, long, AVH_INFIX) {
        if (it_long < value) {
            LOG_ERROR(data->log, "bad iterator infix order : %ld(cur) < %ld(prev)", it_long, value);
            ++data->nerrors;
            ret = AVS_ERROR;
        } else {
            value = it_long;
        }
        ++len;
    }
    BENCHS_STOP_LOG(tm_bench, cpu_bench, data->log, "AVLTREE_FOREACH_DATA(infix) %s", "");
    if (len != avltree_count(data->tree)) {
        LOG_ERROR(data->log, "bad number if iterated elements: %zu(n) != %zu(expected)", len, avltree_count(data->tree));
        ++data->nerrors;
        ret = AVS_ERROR;
    }
        
    data->results = results_bak;

    return (void *) ((long) ret);
}

static void * avltree_test_insert_job(void * vdata) {
    avltree_print_data_t * data = (avltree_print_data_t *) vdata;
    int ret = AVS_FINISHED;

    for (size_t i = 0; i < (size_t) data->max; ++i) {
        if (avltree_insert(data->tree, (void *) i) != (void *) i) {
            ret = AVS_ERROR;
        }
    }
    return (void *) ((long) ret);
}

void * test_avltree(void * vdata) {
    const options_test_t * opts = (const options_test_t *) vdata;
    testgroup_t *   test = TEST_START(opts->testpool, "AVLTREE");
    log_t *         log = test != NULL ? test->log : NULL;
    unsigned int    nerrors = 0;
    avltree_t *     tree = NULL;
    const int       ints[] = { 2, 9, 4, 5, 8, 3, 6, 1, 7, 4, 1 };
    const size_t    intssz = sizeof(ints) / sizeof(*ints);
    int             n;
    long            ref_val;
    unsigned int    progress_max = opts->main == pthread_self()
                                   ? (opts->columns > 100 ? 100 : opts->columns) : 0;

    /* create tree INSERT REC*/
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert_rec)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(log, "* inserting tree(insert_rec)");
    for (size_t i = 0; i < intssz; i++) {
        void * result = avltree_insert_rec(tree, LG(ints[i]));
        if (result != LG(ints[i]) || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%d>, result <%p> : %s",
                      ints[i], result, strerror(errno));
            nerrors++;
        }
    }

    /* visit */
    nerrors += avltree_test_visit(tree, 0, log->out, log);
    /* free */
    LOG_INFO(log, "* freeing tree(insert_rec)");
    avltree_free(tree);

    /* create tree INSERT */
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* insert */
    LOG_INFO(log, "* inserting in tree(insert)");
    for (size_t i = 0; i < intssz; i++) {
        LOG_DEBUG(log, "* inserting %d", ints[i]);
        void * result = avltree_insert(tree, LG(ints[i]));
        if (result != LG(ints[i]) || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%d>, result <%p> : %s",
                      ints[i], result, strerror(errno));
            nerrors++;
        }
        n = avlprint_rec_check_balance(tree->root, log);
        LOG_DEBUG(log, "Checking balances: %d error(s).", n);
        nerrors += n;

        if (log->level >= LOG_LVL_SCREAM) {
            avltree_print(tree, avltree_print_node_default, log->out);
            getchar();
        }
    }

    /* visit */
    nerrors += avltree_test_visit(tree, 1, log->out, log);
    /* remove */
    LOG_INFO(log, "* removing in tree(insert)");
    if (avltree_remove(tree, (const void *) 123456789L) != NULL || errno != ENOENT) {
        LOG_ERROR(log, "error removing 123456789, bad result");
        nerrors++;
    }
    for (size_t i = 0; i < intssz; i++) {
        LOG_DEBUG(log, "* removing %d", ints[i]);
        void * elt = avltree_remove(tree, (const void *) LG(ints[i]));
        if (elt != LG(ints[i]) || (elt == NULL && errno != 0)) {
            LOG_ERROR(log, "error removing elt <%d>: %s", ints[i], strerror(errno));
            nerrors++;
        } else if ((n = avltree_count(tree)) != (int)(intssz - i - 1)) {
            LOG_ERROR(log, "error avltree_count() : %d, expected %zd", n, intssz - i - 1);
            nerrors++;
        }
        /* visit */
        nerrors += avltree_test_visit(tree, 1, NULL, log);

        if (log->level >= LOG_LVL_SCREAM) {
            avltree_print(tree, avltree_print_node_default, log->out);
            getchar();
        }
    }
    /* free */
    LOG_INFO(log, "* freeing tree(insert)");
    avltree_free(tree);

    /* check flags AFL_INSERT_* */
    const char *    one_1 = "one";
    const char *    two_1 = "two";
    char *          one_2 = strdup(one_1);
    void *          result, * find;
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** creating tree(insert, AFL_INSERT_*)");
    if ((tree = avltree_create(AFL_DEFAULT, (avltree_cmpfun_t) strcmp, NULL)) == NULL
    || avltree_insert(tree, (void *) one_1) != one_1
    || avltree_insert(tree, (void *) two_1) != two_1) {
        LOG_ERROR(log, "AFL_INSERT_*: error creating tree: %s", strerror(errno));
        nerrors++;
    }
    /* check flag AFL_INSERT_NODOUBLE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_NODOUBLE;
    n = avltree_count(tree);
    /* set errno to check avltree_insert sets correctly errno */
    if ((!(errno=EBUSY) || avltree_insert(tree, (void *) one_1) != NULL || errno == 0)
    ||  (!(errno=EBUSY) || avltree_insert(tree, (void *) two_1) != NULL || errno == 0)
    ||  (!(errno=EBUSY) || avltree_insert(tree, (void *) one_2) != NULL || errno == 0)
    ||  (avltree_insert(tree, avltree_find_min(tree)) != NULL || errno == 0)
    ||  (int) avltree_count(tree) != n) {
        LOG_ERROR(log, "error inserting existing element with AFL_INSERT_NODOUBLE: not rejected");
        nerrors++;
    }
    /* check flag AFL_INSERT_IGNDOUBLE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_IGNDOUBLE;
    n = avltree_count(tree);
    /* array: multiple of 3: {insert0, result0, find0, insert1, result1, find1, ... } */
    const void * elts[] = { one_1, one_1, one_1, two_1, two_1, two_1, one_2, one_1, one_1 };
    for (unsigned i = 0; i < sizeof(elts) / sizeof(*elts); i += 3) {
        errno = EBUSY;
        find = NULL;
        if ((result = avltree_insert(tree, (void*)elts[i])) != elts[i+1]
        ||  (result == NULL && errno != 0)
        ||  (find = avltree_find(tree, (void*) elts[i])) != elts[i+2]
        ||  (find == NULL && errno != 0)
        ||  (int) avltree_count(tree) != n || (n == 0 && errno != 0)) {
            LOG_ERROR(log, "AFL_INSERT_IGNDOUBLE error: inserting '%s' <%p>, "
                           "expecting ret:<%p> & find:<%p> & count:%d, "
                           "got <%p> & <%p> & %zu, errno:%d.",
                           (const char *)elts[i], elts[i], elts[i+1], elts[i+2], n,
                           result, find, avltree_count(tree), errno);
            nerrors++;
        }
    }
    /* check flag AFL_INSERT_REPLACE */
    tree->flags &= ~(AFL_INSERT_MASK);
    tree->flags |= AFL_INSERT_REPLACE;
    n = avltree_count(tree);
    /* array: multiple of 3: {insert0, result0, find0, insert1, result1, find1, ... } */
    elts[sizeof(elts)/sizeof(*elts)-1] = one_2;
    for (unsigned i = 0; i < sizeof(elts) / sizeof(*elts); i += 3) {
        errno = EBUSY;
        find = NULL;
        if ((result = avltree_insert(tree, (void*)elts[i])) != elts[i+1]
        ||  (result == NULL && errno != 0)
        ||  (find = avltree_find(tree, (void*) elts[i])) != elts[i+2]
        ||  (find == NULL && errno != 0)
        ||  (int) avltree_count(tree) != n || (n == 0 && errno != 0)) {
            LOG_ERROR(log, "AFL_INSERT_REPLACE error: inserting '%s' <%p>, "
                           "expecting ret:<%p> & find:<%p> & count:%d, "
                           "got <%p> & <%p> & %zu, errno:%d.",
                           (const char *)elts[i], elts[i], elts[i+1], elts[i+2], n,
                           result, find, avltree_count(tree), errno);
            nerrors++;
        }
    }
    /* free */
    LOG_INFO(log, "* freeing tree(insert, AFL_INSERT_*)");
    avltree_free(tree);
    if (one_2 != NULL)
        free(one_2);

    /* test with tree created manually */
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TREE (insert_manual)");
    if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
        LOG_ERROR(log, "error creating tree(manual insert): %s", strerror(errno));
        nerrors++;
    }
    tree->root =
        AVLNODE(LG(10),
                AVLNODE(LG(5),
                    AVLNODE(LG(3),
                        AVLNODE(LG(2), NULL, NULL),
                        AVLNODE(LG(4), NULL, NULL)),
                    AVLNODE(LG(7),
                        AVLNODE(LG(6), NULL, NULL),
                        AVLNODE(LG(8), NULL, NULL))),
                AVLNODE(LG(15),
                    AVLNODE(LG(13),
                        AVLNODE(LG(12), NULL, NULL),
                        AVLNODE(LG(14), NULL, NULL)),
                    AVLNODE(LG(17),
                        AVLNODE(LG(16),NULL, NULL),
                        AVLNODE(LG(18), NULL, NULL)))
            );
    tree->n_elements = avlprint_rec_get_count(tree->root);
    nerrors += avltree_test_visit(tree, 1, log->out, log);

    /* free */
    LOG_INFO(log, "* freeing tree(insert_manual)");
    avltree_free(tree);

    /* create tree INSERT - same values */
    const size_t samevalues_count = 30;
    const long samevalues[] = { 0, 2, LONG_MAX };
    for (const long * samevalue = samevalues; *samevalue != LONG_MAX; samevalue++) {
        LOG_INFO(log, "*************************************************");
        LOG_INFO(log, "*** CREATING TREE (insert same values:%ld)", *samevalue);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }
        /* visit on empty tree */
        LOG_INFO(log, "* visiting empty tree (insert_same_values:%ld)", *samevalue);
        nerrors += avltree_test_visit(tree, 1, NULL, log);
        /* insert */
        LOG_INFO(log, "* inserting in tree(insert_same_values:%ld)", *samevalue);
        for (size_t i = 0, count; i < samevalues_count; i++) {
            LOG_DEBUG(log, "* inserting %ld", *samevalue);
            errno = EBUSY; /* setting errno to check it is correctly set by avltree */
            void * result = avltree_insert(tree, LG(*samevalue));
            if (result != LG(*samevalue) || (result == NULL && errno != 0)) {
                LOG_ERROR(log, "error inserting elt <%ld>, result <%p> : %s",
                          *samevalue, result, strerror(errno));
                nerrors++;
            }
            n = avlprint_rec_check_balance(tree->root, log);
            LOG_DEBUG(log, "Checking balances: %d error(s).", n);
            nerrors += n;

            count = avltree_count(tree);
            LOG_DEBUG(log, "Checking count: %zu, %zu expected.", count, i + 1);
            if (count != i + 1) {
                LOG_ERROR(log, "error count: %zu, %zu expected.", count, i + 1);
                ++nerrors;
            }

            if (log->level >= LOG_LVL_SCREAM) {
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        /* visit */
        LOG_INFO(log, "* visiting tree(insert_same_values:%ld)", *samevalue);
        nerrors += avltree_test_visit(tree, 1, LOG_CAN_LOG(log, LOG_LVL_DEBUG)
                                               ? log->out : NULL, log);
        /* remove */
        LOG_INFO(log, "* removing in tree(insert_same_values:%ld)", *samevalue);
        for (size_t i = 0; i < samevalues_count; i++) {
            LOG_DEBUG(log, "* removing %ld", *samevalue);
            errno = EBUSY; /* setting errno to check it is correctly set by avltree */
            void * elt = avltree_remove(tree, (const void *) LG(*samevalue));
            if (elt != LG(*samevalue) || (elt == NULL && errno != 0)) {
                LOG_ERROR(log, "error removing elt <%ld>: %s", *samevalue, strerror(errno));
                nerrors++;
            } else if ((n = avltree_count(tree)) != (int)(samevalues_count - i - 1)) {
                LOG_ERROR(log, "error avltree_count() : %d, expected %zd",
                          n, samevalues_count - i - 1);
                nerrors++;
            }
            /* visit */
            nerrors += avltree_test_visit(tree, 1, NULL, log);

            if (log->level >= LOG_LVL_SCREAM) {
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        /* free */
        LOG_INFO(log, "* freeing tree(insert_same_values: %ld)", *samevalue);
        avltree_free(tree);
    }

    /* create Big tree INSERT */
    BENCH_DECL(bench);
    BENCH_TM_DECL(tm_bench);
    rbuf_t * two_results        = rbuf_create(2, RBF_DEFAULT | RBF_OVERWRITE);
    rbuf_t * all_results        = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    rbuf_t * all_refs           = rbuf_create(1000, RBF_DEFAULT | RBF_SHRINK_ON_RESET);
    avltree_print_data_t data   = { .results = two_results, .log = log, .out = NULL };
    const size_t nb_elts[] = { 100 * 1000, 1 * 1000 * 1000, SIZE_MAX, 10 * 1000 * 1000, 0 };
    for (const size_t * nb = nb_elts; *nb != 0; nb++) {
        long    value, min_val = LONG_MAX, max_val = LONG_MIN;
        size_t  n_remove, total_remove = *nb / 10;
        rbuf_t * del_vals;
        slist_t * slist;
        rbuf_t * rbuf;
        rbuf_t * stack;
        void ** array;
        size_t len;
        avltree_iterator_t * iterator;
        avltree_visit_context_t * context;
        
        if (*nb == SIZE_MAX) { /* after size max this is only for TEST_bigtree */
            if ((opts->test_mode & TEST_MASK(TEST_bigtree)) != 0) continue ; else break ;
        }
        if (nb == nb_elts) {
            srand(INT_MAX); /* first loop with predictive random for debugging */
            //log->level = LOG_LVL_DEBUG;
        } else {
            //log->level = LOG_LVL_INFO;
            srand(time(NULL));
        }

        LOG_INFO(log, "*************************************************");
        LOG_INFO(log, "*** CREATING BIG TREE (insert, %zu elements)", *nb);
        if ((tree = avltree_create(AFL_DEFAULT, intcmp, NULL)) == NULL) {
            LOG_ERROR(log, "error creating tree: %s", strerror(errno));
            nerrors++;
        }
        data.tree = tree;

        /* create iterator and check context */
        if ((iterator = avltree_iterator_create(tree, AVH_PREFIX)) == NULL
        ||  (context = avltree_iterator_context(iterator)) == NULL) {
            LOG_WARN(log, "cannot create or get tree iterator context");
            ++nerrors;
            stack = NULL;
        } else {
            stack = context->stack;
        }
        avltree_iterator_abort(iterator);
        
        /* insert */
        del_vals = rbuf_create(total_remove, RBF_DEFAULT | RBF_OVERWRITE);
        LOG_INFO(log, "* inserting in Big tree(insert, %zu elements)", *nb);
        BENCHS_START(tm_bench, bench);
        for (size_t i = 0, n_remove = 0; i < *nb; i++) {
            LOG_DEBUG(log, "* inserting %zu", i);
            value = rand();
            value = (long)(value % (*nb * 10));
            if (value > max_val)
                max_val = value;
            if (value < min_val)
                min_val = value;
            if (++n_remove <= total_remove)
                rbuf_push(del_vals, LG(value));

            void * result = avltree_insert(tree, (void *) value);

            if (progress_max && (i * (progress_max)) % *nb == 0 && log->level >= LOG_LVL_INFO)
                fputc('.', log->out);

            if (result != (void *) value || (result == NULL && errno != 0)) {
                LOG_ERROR(log, "error inserting elt <%ld>, result <%lx> : %s",
                          value, (unsigned long) result, strerror(errno));
                nerrors++;
            }
            if (log->level >= LOG_LVL_SCREAM) {
                nerrors += avlprint_rec_check_balance(tree->root, log);
                avltree_print(tree, avltree_print_node_default, log->out);
                getchar();
            }
        }
        if (progress_max && log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCHS_STOP_LOG(tm_bench, bench, log, "creation of %zu nodes ", *nb);

        /* visit */
        LOG_INFO(log, "* checking balance, prefix, infix, infix_r, "
                         "parallel, min, max, depth, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check BALANCE (recursive) of %zu nodes | ", *nb);

        /* prefix */
        rbuf_reset(data.results);
        data.results=NULL; /* in order to run visit_checkprint() */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PREFIX visit of %zu nodes | ", *nb);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check prefix visit with prefix (%zu nodes) | ", *nb);
        nerrors += data.nerrors;
        data.results = two_results;

        /* infix */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX visit of %zu nodes | ", *nb);

        /* infix_right */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX_RIGHT visit of %zu nodes | ", *nb);

        /* parallel */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as parallel avltree_visit_print without AVH_PARALLEL_DUPDATA */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX visit of %zu nodes | ", *nb);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        || data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check // visit with prefix (%zu nodes) | ", *nb);
        nerrors += data.nerrors;
        data.results = two_results;

        /* parallel_merge */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as avltree_visit_print will create its own datas */
        data.nerrors = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PARALLEL_DUPDATA(AVH_PREFIX|AVH_MERGE, sizeof(data))) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX_MERGE visit of %zu nodes | ", *nb);
        if (data.nerrors != avltree_count(tree)) {
            LOG_ERROR(log, "parallel_prefix_merge: error expected %zu visits, got %lu",
                      avltree_count(tree), data.nerrors);
            ++nerrors;
        }
        data.nerrors = 0;
        data.results = two_results;

        LOG_INFO(log, "current tree stack maxsize = %zu", rbuf_maxsize(stack));

        /* min */
        BENCH_START(bench);
        value = (long) avltree_find_min(tree);
        BENCH_STOP_LOG(bench, log, "MINIMUM value = %ld | ", value);
        if (value != min_val) {
            LOG_ERROR(log, "error incorrect minimum value %ld, expected %ld",
                      value, min_val);
            ++nerrors;
        }
        /* max */
        BENCH_START(bench);
        value = (long) avltree_find_max(tree);
        BENCH_STOP_LOG(bench, log, "MAXIMUM value = %ld | ", value);
        if (value != max_val) {
            LOG_ERROR(log, "error incorrect maximum value %ld, expected %ld",
                      value, max_val);
            ++nerrors;
        }
        /* depth */
        BENCH_START(bench);
        n = avlprint_rec_get_height(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive DEPTH (%zu nodes) = %d | ",
                       *nb, n);

        BENCH_START(bench);
        value = avltree_find_depth(tree);
        BENCH_STOP_LOG(bench, log, "DEPTH (%zu nodes) = %ld | ",
                       *nb, value);
        if (value != n) {
            LOG_ERROR(log, "error incorrect DEPTH %ld, expected %d",
                      value, n);
            ++nerrors;
        }
        /* count */
        BENCH_START(bench);
        n = avlprint_rec_get_count(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive COUNT (%zu nodes) = %d | ",
                       *nb, n);

        BENCH_START(bench);
        value = avltree_count(tree);
        BENCH_STOP_LOG(bench, log, "COUNT (%zu nodes) = %ld | ",
                       *nb, value);
        if (value != n || (size_t) value != *nb) {
            LOG_ERROR(log, "error incorrect COUNT %ld, expected %d(rec), %zu",
                      value, n, *nb);
            ++nerrors;
        }
        /* memorysize */
        BENCH_START(bench);
        n = avltree_memorysize(tree);
        BENCH_STOP_LOG(bench, log, "MEMORYSIZE (%zu nodes) = %d (%.03fMB) | ",
                       *nb, n, n / 1000.0 / 1000.0);

        /* visit range */
        rbuf_reset(all_results);
        rbuf_reset(all_refs);
        data.min = LG((((long)avltree_find_max(tree)) / 2) - 500);
        data.max = LG((long)data.min + 2000);
        /* ->avltree_visit_range() */
        data.results = all_results;
        ref_val = -1L;
        BENCHS_START(tm_bench, bench);
        if (AVS_FINISHED !=
                (n = avltree_visit_range(tree, data.min, data.max, visit_range, &data, 0))
        || (ref_val = (long)avltree_node_data((avltree_node_t*)rbuf_top(data.results))) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit_range() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "VISIT_RANGE (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->avltree_visit() */
        data.results = all_refs;
        BENCHS_START(tm_bench, bench);
        if (AVS_FINISHED != (n = avltree_visit(tree, visit_range, &data, AVH_INFIX))
        || (ref_val = (long)avltree_node_data((avltree_node_t*)rbuf_top(data.results))) > (long) data.max) {
            LOG_ERROR(log, "error: avltree_visit() return %d, last(%ld) <=? max(%ld) ",
                      n, ref_val, (long)data.max);
            ++nerrors;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "VISIT INFIX (%zu / %zu nodes) | ", rbuf_size(data.results), *nb);
        /* ->compare avltree_visit and avltree_visit_range */
        if ((n = avltree_check_results(all_results, all_refs, NULL, TAC_DEFAULT)) > 0) {
            LOG_ERROR(log, "visit_range/visit_infix --> error: different results");
            nerrors += n;
        }
        data.results = two_results;

        /* avltree_to_slist */
        BENCHS_START(tm_bench, bench);
        slist = avltree_to_slist(tree, AVH_INFIX);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_slist(infix) %s", "");
        if (slist_length(slist) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_slist(infix): wrong length");
            ++nerrors;
        }
        slist_free(slist, NULL);

        BENCHS_START(tm_bench, bench);
        slist = avltree_to_slist(tree, AVH_INFIX | AVH_PARALLEL);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_slist(infix_parallel) %s", "");
        if (slist_length(slist) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_slist(infix_parallel): wrong length");
            ++nerrors;
        }
        slist_free(slist, NULL);

        /* avltree_to_rbuf */
        BENCHS_START(tm_bench, bench);
        rbuf = avltree_to_rbuf(tree, AVH_INFIX);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_rbuf(infix) %s", "");
        if (rbuf_size(rbuf) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_rbuf(infix): wrong length");
            ++nerrors;
        }
        rbuf_free(rbuf);

        BENCHS_START(tm_bench, bench);
        rbuf = avltree_to_rbuf(tree, AVH_INFIX | AVH_PARALLEL);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_rbuf(infix_parallel) %s", "");
        if (rbuf_size(rbuf) != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_rbuf(infix_parallel): wrong length");
            ++nerrors;
        }
        rbuf_free(rbuf);

        /* avltree_to_array */
        BENCHS_START(tm_bench, bench);
        len = avltree_to_array(tree, AVH_INFIX, &array);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_array(infix) %s", "");
        if (len != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_array(infix): wrong length");
            ++nerrors;
        }
        if (array) free(array);

        BENCHS_START(tm_bench, bench);
        len = avltree_to_array(tree, AVH_INFIX | AVH_PARALLEL, &array);
        BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_to_array(infix_parallel) %s", "");
        if (len != tree->n_elements) {
            LOG_ERROR(log, "avltree_to_array(infix_parallel): wrong length");
            ++nerrors;
        }
        if (array) free(array);

        /* avltree_iterator_{create,next,abort}() */
        iterator = avltree_iterator_create(tree, AVH_INFIX);
        if (!iterator) {
            LOG_WARN(log, "cannot create tree iterator");
            ++nerrors;
        } else {
            void * data;
            long prev = LONG_MIN;
            len = 0;
            BENCHS_START(tm_bench, bench);
            while ((data = avltree_iterator_next(iterator)) != NULL || errno == 0) {
                value = (long)data;
                if (value < prev) {
                    LOG_ERROR(log, "bad iterator infix order : %ld(cur) < %ld(prev)", value, prev);
                    ++nerrors;
                } else {
                    prev = value;
                }
                ++len;
            }
            BENCHS_STOP_LOG(tm_bench, bench, log, "avltree_iterator(infix) %s", "");
            if (len != avltree_count(tree)) {
                LOG_ERROR(log, "bad number if iterated elements: %zu(n) != %zu(expected)", len, avltree_count(tree));
                ++nerrors;
            }
        }

        /* remove (total_remove) elements */
        LOG_INFO(log, "* removing in tree (%zu nodes)", total_remove);
        BENCHS_START(tm_bench, bench);
        for (n_remove = 0; rbuf_size(del_vals) != 0; ++n_remove) {
            if (progress_max
            &&  (n_remove * (progress_max)) % total_remove == 0 && log->level >= LOG_LVL_INFO)
                fputc('.', log->out);
            value = (long) rbuf_pop(del_vals);

            LOG_DEBUG(log, "* deleting %ld", value);
            void * elt = avltree_remove(tree, (const void*)(value));
            if (elt != LG(value) || (elt == NULL && errno != 0)) {
                LOG_ERROR(log, "error removing elt <%ld>: %s", value, strerror(errno));
                nerrors++;
            }
            if ((n = avltree_count(tree)) != (int)(*nb - n_remove - 1)) {
                LOG_ERROR(log, "error avltree_count() : %d, expected %zd", n, *nb - n_remove - 1);
                nerrors++;
            }
            /* currently, don't visit or check balance in remove loop because this is too long */
        }
        if (progress_max && log->level >= LOG_LVL_INFO)
            fputc('\n', log->out);
        BENCHS_STOP_LOG(tm_bench, bench, log, "REMOVED %zu nodes | ", total_remove);
        rbuf_free(del_vals);

        /***** Checking tree after remove */
        LOG_INFO(log, "* checking BALANCE, prefix, infix, infix_r, parallel, count");

        BENCH_START(bench);
        nerrors += avlprint_rec_check_balance(tree->root, log);
        BENCH_STOP_LOG(bench, log, "check balance (recursive) of %zd nodes | ", *nb - total_remove);

        /* count */
        BENCH_START(bench);
        n = avlprint_rec_get_count(tree->root);
        BENCH_STOP_LOG(bench, log, "Recursive COUNT (%zu nodes) = %d | ",
                       *nb - total_remove, n);

        BENCH_START(bench);
        value = avltree_count(tree);
        BENCH_STOP_LOG(bench, log, "COUNT (%zu nodes) = %ld | ",
                       *nb - total_remove, value);
        if (value != n || (size_t) value != (*nb - total_remove)) {
            LOG_ERROR(log, "error incorrect COUNT %ld, expected %d(rec), %zd",
                      value, n, *nb - total_remove);
            ++nerrors;
        }

        /* prefix */
        rbuf_reset(data.results);
        data.results=NULL; /* in order to run visit_checkprint() */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PREFIX visit of %zu nodes | ", *nb - total_remove);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check prefix visit with prefix (%zu nodes) | ",
                        *nb - total_remove);
        nerrors += data.nerrors;
        data.results = two_results;

        /* infix */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "INFIX visit of %zd nodes | ", *nb - total_remove);

        /* infix right */
        rbuf_reset(data.results);
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_INFIX | AVH_RIGHT) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "INFIX_RIGHT visit of %zd nodes | ", *nb - total_remove);

        /* parallel */
        rbuf_reset(data.results);
        data.results = NULL; /* needed because parallel avltree_visit_print */
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data, AVH_PREFIX | AVH_PARALLEL) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX visit of %zu nodes | ",
                        *nb - total_remove);
        data.nerrors = data.ntests = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_checkprint, &data, AVH_PREFIX) != AVS_FINISHED
        ||  data.ntests != avltree_count(tree)) {
            nerrors++;
        }
        nerrors += data.nerrors;
        BENCHS_STOP_LOG(tm_bench, bench, log,
                        "check // visit with prefix (%zu nodes) | ",
                        *nb - total_remove);
        data.results = two_results;

        /* parallel_merge */
        rbuf_reset(data.results);
        data.results=NULL; /* needed as avltree_visit_print will create its own datas */
        data.nerrors = 0;
        BENCHS_START(tm_bench, bench);
        if (avltree_visit(tree, visit_print, &data,
                    AVH_PARALLEL_DUPDATA(AVH_PREFIX|AVH_MERGE, sizeof(data))) != AVS_FINISHED) {
            nerrors++;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "PARALLEL_PREFIX_MERGE visit of %zu nodes | ",
                        *nb - total_remove);
        if (data.nerrors != avltree_count(tree)) {
            LOG_ERROR(log, "parallel_prefix_merge: error expected %zu visits, got %lu",
                      avltree_count(tree), data.nerrors);
            ++nerrors;
        }
        data.nerrors = 0;
        data.results = two_results;

        /* AVLTREE_FOREACH_DATA(INFIX) */
        value = LONG_MIN;
        len = 0;
        BENCHS_START(tm_bench, bench);
        AVLTREE_FOREACH_DATA(tree, it_long, long, AVH_INFIX) {
            if (it_long < value) {
                LOG_ERROR(log, "bad iterator infix order : %ld(cur) < %ld(prev)", it_long, value);
                ++nerrors;
            } else {
                value = it_long;
            }
            ++len;
        }
        BENCHS_STOP_LOG(tm_bench, bench, log, "AVLTREE_FOREACH_DATA(infix) %s", "");
        if (len != avltree_count(tree)) {
            LOG_ERROR(log, "bad number if iterated elements: %zu(n) != %zu(expected)", len, avltree_count(tree));
            ++nerrors;
        }

        /* free */
        LOG_INFO(log, "* freeing tree(insert)");
        BENCHS_START(tm_bench, bench);
        avltree_free(tree);
        BENCHS_STOP_LOG(tm_bench, bench, log, "freed %zd nodes | ", *nb - total_remove);
    }

    /* ****************************************
     * Trees with shared resources
     * ***************************************/
    avltree_t * tree2 = NULL;
    const size_t nb = 100000;
    slist_t * jobs = NULL;
    avltree_print_data_t data2 = data;
    LOG_INFO(log, "*************************************************");
    LOG_INFO(log, "*** CREATING TWO TREES with shared resources (insert, %zu elements)", nb);
    if ((tree = avltree_create(AFL_DEFAULT | AFL_SHARED_STACK, intcmp, NULL)) == NULL
    || (tree2 = avltree_create(AFL_DEFAULT & ~AFL_SHARED_STACK, intcmp, NULL)) == NULL
    || (data2.results = rbuf_create(rbuf_maxsize(data.results), RBF_DEFAULT | RBF_OVERWRITE))
                            == NULL) {
        LOG_ERROR(log, "error creating tree: %s", strerror(errno));
        nerrors++;
    } else {
        tree2->shared = tree->shared;
        data.tree = tree;
        data2.tree = tree2;
    }

    /* insert */
    LOG_INFO(log, "* inserting in shared trees (insert, %zu elements)", nb);
    BENCHS_START(tm_bench, bench);
    for (size_t i = 0; i < nb; i++) {
        LOG_DEBUG(log, "* inserting %zu", i);
        long value = rand();
        value = (long)(value % (nb * 10));

        void * result = avltree_insert(tree, (void *) value);
        void * result2 = avltree_insert(tree2, (void *) value);

        if (progress_max && (i * (progress_max)) % nb == 0 && log->level >= LOG_LVL_INFO)
            fputc('.', log->out);

        if (result != (void *) value || result != result2 || (result == NULL && errno != 0)) {
            LOG_ERROR(log, "error inserting elt <%ld>, result <%lx> : %s",
                     value, (unsigned long) result, strerror(errno));
            nerrors++;
        }
    }
    if (progress_max && log->level >= LOG_LVL_INFO)
        fputc('\n', log->out);
    BENCHS_STOP_LOG(tm_bench, bench, log, "creation of %zu nodes (x2) ", nb);

    /* infix */
    rbuf_reset(data.results);
    rbuf_reset(data2.results);
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "VISITS of %zd nodes (x2) | ", nb);
    slist_free(jobs, NULL);

    data.max = data2.max = (void*) nb;
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(NULL, vjob_run(avltree_test_insert_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_insert_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "INSERTION of %zd more nodes (x2) | ", nb);
    slist_free(jobs, NULL);

    /* visits */
    rbuf_reset(data.results);
    rbuf_reset(data2.results);
    BENCHS_START(tm_bench, bench);
    jobs = slist_prepend(NULL, vjob_run(avltree_test_visit_job, &data));
    jobs = slist_prepend(jobs, vjob_run(avltree_test_visit_job, &data2));
    SLIST_FOREACH_DATA(jobs, job, vjob_t *) {
        void * result = vjob_waitandfree(job);
        if ((long) result != AVS_FINISHED) {
            nerrors++;
        }
    }
    BENCHS_STOP_LOG(tm_bench, bench, log, "VISITS of %zd nodes (x2) | ", nb * 2);
    slist_free(jobs, NULL);

    rbuf_free(data2.results);
    avltree_free(tree2);
    avltree_free(tree);

    /* END */
    rbuf_free(two_results);
    rbuf_free(all_results);
    rbuf_free(all_refs);

    if (test != NULL) {
        ++(test->n_tests);
        if (nerrors == 0)
            ++(test->n_ok);
        test->n_errors += nerrors;
        nerrors = 0;
    }
    return VOIDP(TEST_END(test) + nerrors);
}

#endif /* ! ifdef _TEST */

