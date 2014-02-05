#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#include "diag.h"

/*
 * bytes_t *, lkit_expr_t *
 */
static lkit_expr_t root;

static lkit_type_t *type_of_expr(lkit_expr_t *);
static void lexpr_init_ctx(lkit_expr_t *);
static void lexpr_fini_ctx(lkit_expr_t *);

static int
lexpr_dump(bytes_t *key, lkit_expr_t *value)
{
    bytestream_t bs;
    bytestream_init(&bs);

    lkit_type_str(type_of_expr(value), &bs);
    bytestream_cat(&bs, 4, " <- ");
    lkit_type_str(value->type, &bs);
    bytestream_cat(&bs, 1, "\0");
    TRACE("EXPR: %s: %s", (key != NULL) ? (char *)(key->data) : "<null>", SDATA(&bs, 0));
    if (value->isref) {
        lexpr_dump(NULL, value->value.ref);
    } else {
        if (value->value.literal == NULL) {
            TRACE("LITERAL: <null>");
        } else {
            TRACE("LITERAL:");
            fparser_datum_dump(&value->value.literal, NULL);
        }
    }

    bytestream_fini(&bs);
    return 0;
}

static uint64_t
lkit_expr_hash(bytes_t *key)
{
    if (key == NULL) {
        return 0;
    }
    return bytes_hash(key);
}

static int
lkit_expr_cmp(bytes_t *a, bytes_t *b)
{
    int diff;
    uint64_t ha, hb;

    ha = lkit_expr_hash(a);
    hb = lkit_expr_hash(b);
    diff = (int)(ha - hb);
    if (diff == 0) {
        ssize_t szdiff = a->sz - b->sz;

        if (szdiff == 0) {
            return memcmp(a->data, b->data, a->sz);
        }

        return (int) szdiff;
    }
    return diff;
}

static int
lexpr_destroy(lkit_expr_t **pexpr)
{
    if (*pexpr != NULL) {
        array_fini(&((*pexpr)->subs));
        lexpr_fini_ctx(*pexpr);
        free(*pexpr);
        *pexpr = NULL;
    }
    return 0;
}

static int
lkit_expr_fini_dict(UNUSED bytes_t *key, lkit_expr_t *value)
{
    lexpr_destroy(&value);
    return 0;
}

static lkit_type_t *
type_of_expr(lkit_expr_t *expr)
{
    if (expr->type->tag == LKIT_FUNC) {
        lkit_func_t *tf;
        lkit_type_t **pty;

        tf = (lkit_func_t *)(expr->type);

        if ((pty = array_get(&tf->fields, 0)) == NULL) {
            FAIL("array_get");
        }
        return *pty;
    }
    return expr->type;
}

lkit_expr_t *
lkit_expr_find(lkit_expr_t *ctx, bytes_t *name)
{
    lkit_expr_t *expr = NULL;
    lkit_expr_t *cctx;

    for (cctx = ctx; cctx!= NULL; cctx = cctx->parent) {
        if ((expr = dict_get_item(&cctx->ctx, name)) != NULL) {
            return expr;
        }
    }
    return NULL;
}

lkit_expr_t *
lkit_expr_parse(lkit_expr_t *ctx, fparser_datum_t *dat, int seterror)
{
    lkit_expr_t *expr;

    if ((expr = malloc(sizeof(lkit_expr_t))) == NULL) {
        FAIL("malloc");
    }

    if (array_init(&expr->subs, sizeof(lkit_expr_t *), 0,
                   NULL,
                   (array_finalizer_t)lexpr_destroy) != 0) {
        FAIL("array_init");
    }
    lexpr_init_ctx(expr);
    expr->isref = 0;
    expr->parent = ctx;

    /* first probe for type/reference */
    if ((expr->type = lkit_type_parse(dat, 0)) == NULL) {
        lkit_type_t *type, **paramtype = NULL;
        bytes_t *name = NULL;
        array_t *form;
        array_iter_t it;
        fparser_datum_t **node;
        lkit_func_t *tf = NULL;
        int isvararg = 0;
        /* expr found ? */
        //TRACE("value:");
        //fparser_datum_dump(&dat, NULL);

        switch (FPARSER_DATUM_TAG(dat)) {
        case FPARSER_INT:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_INT);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                TR(LKIT_EXPR_PARSE + 1);
                goto err;
            }
            break;

        case FPARSER_STR:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_STR);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                TR(LKIT_EXPR_PARSE + 2);
                goto err;
            }
            break;

        case FPARSER_FLOAT:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_FLOAT);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                TR(LKIT_EXPR_PARSE + 3);
                goto err;
            }
            break;

        case FPARSER_BOOL:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_BOOL);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                TR(LKIT_EXPR_PARSE + 4);
                goto err;
            }
            break;

        case FPARSER_WORD:
            /*
             * must be exprref
             */
            name = (bytes_t *)(dat->body);
            if ((expr->value.ref = lkit_expr_find(ctx, name)) == NULL) {
                TR(LKIT_EXPR_PARSE + 5);
                goto err;
            }

            //TRACE("ISREF");
            expr->isref = 1;
            expr->type = expr->value.ref->type;
            break;

        case FPARSER_SEQ:
            form = (array_t *)(dat->body);
            if (lparse_first_word_bytes(form, &it, &name, 1) != 0) {
                TR(LKIT_EXPR_PARSE + 6);
                goto err;
            }

            if ((expr->value.ref = lkit_expr_find(ctx, name)) == NULL) {
                //TRACE("failed probe '%s'", name->data);
                //dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
                TR(LKIT_EXPR_PARSE + 7);
                goto err;
            }

            //TRACE("ISREF");
            expr->isref = 1;
            expr->type = expr->value.ref->type;

            /* functions only ATM */
            if (expr->type->tag != LKIT_FUNC) {
                TR(LKIT_EXPR_PARSE + 8);
                goto err;
            }

            tf = (lkit_func_t *)(expr->type);

            for (node = array_next(form, &it);
                 node != NULL;
                 node = array_next(form, &it)) {

                lkit_expr_t **arg;
                lkit_type_t *argtype;

                if ((arg = array_incr(&expr->subs)) == NULL) {
                    FAIL("array_incr");
                }
                if ((*arg = lkit_expr_parse(ctx, *node, 1)) == NULL) {
                    TR(LKIT_EXPR_PARSE + 9);
                    goto err;
                }

                //TRACE("arg:");
                //lexpr_dump(NULL, *arg);

                paramtype = array_get(&tf->fields, it.iter);

                if (!isvararg) {
                    if (paramtype == NULL) {
                        TR(LKIT_EXPR_PARSE + 10);
                        goto err;
                    }

                    if (*paramtype == NULL) {
                        TR(LKIT_EXPR_PARSE + 11);
                        goto err;
                    }

                    if ((argtype = type_of_expr(*arg)) == NULL) {
                        TR(LKIT_EXPR_PARSE + 12);
                        goto err;
                    }

                    if ((*paramtype)->tag == LKIT_VARARG) {
                        isvararg = 1;
                    } else {
                        if ((*paramtype)->tag != LKIT_UNDEF) {
                            if (lkit_type_cmp(*paramtype, argtype) != 0) {
                                TR(LKIT_EXPR_PARSE + 13);
                                goto err;
                            }
                        }
                    }

                } else {
                    if (paramtype != NULL && *paramtype != NULL) {
                        if ((*paramtype)->tag != LKIT_VARARG) {
                            TR(LKIT_EXPR_PARSE + 14);
                            goto err;
                        }
                    }
                }
            }

            if ((paramtype = array_last(&tf->fields, &it)) == NULL) {
                FAIL("array_get");
            }

            if ((*paramtype)->tag == LKIT_VARARG) {
                isvararg = 1;

            } else {
                if (isvararg) {
                    TR(LKIT_EXPR_PARSE + 15);
                    goto err;
                }
            }

            if (!isvararg) {
                if ((tf->fields.elnum - expr->subs.elnum) != 1) {
                    TR(LKIT_EXPR_PARSE + 16);
                    goto err;
                }
            }

            break;

        default:
            FAIL("lkit_expr_parse");
        }

    } else {
        /* typed declaration */
        expr->value.ref = NULL;
        //TRACE("type:");
        //lkit_type_dump(expr->type);
    }

end:
    return expr;

err:
    dat->error = seterror;
    lexpr_destroy(&expr);
    goto end;
}

int
lkit_parse_exprdef(lkit_expr_t *ctx, array_t *form, array_iter_t *it)
{
    int res = 0;
    bytes_t *name = NULL;
    lkit_expr_t *expr = NULL;
    fparser_datum_t **node = NULL;

    /* name */
    if (lparse_next_word_bytes(form, it, &name, 1) != 0) {
        TR(LKIT_PARSE_EXPRDEF + 1);
        goto err;
    }

    if (lkit_expr_find(ctx, name) != NULL) {
        TR(LKIT_PARSE_EXPRDEF + 2);
        goto err;
    }

    /* type and value */
    if ((node = array_next(form, it)) == NULL) {
        TR(LKIT_PARSE_EXPRDEF + 3);
        goto err;
    }

    if ((expr = lkit_expr_parse(ctx, *node, 1)) == NULL) {
        (*node)->error = 1;
        TR(LKIT_PARSE_EXPRDEF + 4);
        goto err;
    }

    dict_set_item(&ctx->ctx, name, expr);

end:
    return res;

err:
    lexpr_destroy(&expr);
    res = 1;
    goto end;
}

int
lexpr_parse(array_t *form, array_iter_t *it)
{
    return lkit_parse_exprdef(&root, form, it);
}

static void
lexpr_init_ctx(lkit_expr_t *ctx)
{
    dict_init(&ctx->ctx, 101,
              (dict_hashfn_t)lkit_expr_hash,
              (dict_item_comparator_t)lkit_expr_cmp,
              (dict_item_finalizer_t)lkit_expr_fini_dict);
}

static void
lexpr_fini_ctx(lkit_expr_t *ctx)
{
    dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
    dict_fini(&ctx->ctx);
}

void
lexpr_init(void)
{
    lexpr_init_ctx(&root);
}

void lexpr_fini(void)
{
    lexpr_fini_ctx(&root);
}

