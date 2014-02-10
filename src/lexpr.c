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

static void lexpr_init_ctx(lkit_expr_t *);
static void lexpr_fini_ctx(lkit_expr_t *);

void
lkit_expr_dump(lkit_expr_t *expr)
{
    bytestream_t bs;
    lkit_expr_t **subexpr;
    array_iter_t it;

    bytestream_init(&bs);

    lkit_type_str(lkit_type_of_expr(expr), &bs);
    bytestream_cat(&bs, 4, " <- ");
    lkit_type_str(expr->type, &bs);
    bytestream_cat(&bs, 1, "\0");
    if (expr->isref) {
        if (expr->name != NULL) {
            TRACE("<REF>:%s", expr->name->data);
        } else {
            TRACE("<REF>:");
        }
        lkit_expr_dump(expr->value.ref);
    } else {
        if (expr->name != NULL) {
            if (expr->value.literal == NULL) {
                TRACE("%s: <null>", expr->name->data);
            } else {
                TRACE("%s:", expr->name->data);
                fparser_datum_dump(&expr->value.literal, NULL);
            }
        } else {
            if (expr->value.literal == NULL) {
                TRACE("<LITERAL>: <null>");
            } else {
                TRACE("<LITERAL>:");
                fparser_datum_dump(&expr->value.literal, NULL);
            }
        }
    }
    if (expr->subs.elnum > 0) {
        TRACE("SUBS:");
        for (subexpr = array_first(&expr->subs, &it);
             subexpr != NULL;
             subexpr = array_next(&expr->subs, &it)) {

            lkit_expr_dump(*subexpr);
        }
    }

    bytestream_fini(&bs);
}

UNUSED static int
lexpr_dump(bytes_t *key, lkit_expr_t *value)
{
    bytestream_t bs;
    lkit_expr_t **subexpr;
    array_iter_t it;

    bytestream_init(&bs);

    lkit_type_str(lkit_type_of_expr(value), &bs);
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
    if (value->subs.elnum > 0) {
        TRACE("SUBS:");
        for (subexpr = array_first(&value->subs, &it);
             subexpr != NULL;
             subexpr = array_next(&value->subs, &it)) {

            lexpr_dump(NULL, *subexpr);
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

lkit_type_t *
lkit_type_of_expr(lkit_expr_t *expr)
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

    //TRACE("ctx=%p name=%s", ctx, name->data);

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
    lkit_type_t *type = NULL;

    if ((expr = malloc(sizeof(lkit_expr_t))) == NULL) {
        FAIL("malloc");
    }

    if (array_init(&expr->subs, sizeof(lkit_expr_t *), 0,
                   NULL,
                   (array_finalizer_t)lexpr_destroy) != 0) {
        FAIL("array_init");
    }
    lexpr_init_ctx(expr);
    expr->name = NULL;
    expr->isref = 0;
    expr->error = 0;
    expr->parent = ctx;

    /* first probe for type/reference */
    if ((expr->type = lkit_type_parse(dat, 0)) == NULL) {
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
            expr->name = (bytes_t *)(dat->body);
            if ((expr->value.ref = lkit_expr_find(ctx, expr->name)) == NULL) {
                TR(LKIT_EXPR_PARSE + 5);
                goto err;
            }

            //TRACE("ISREF");
            expr->isref = 1;
            expr->type = expr->value.ref->type;
            break;

        case FPARSER_SEQ:
            {
                lkit_type_t **paramtype = NULL;
                array_t *form;
                array_iter_t it;
                fparser_datum_t **node;
                lkit_func_t *tf = NULL;
                int isvararg = 0;

                form = (array_t *)(dat->body);
                if (lparse_first_word_bytes(form, &it, &expr->name, 1) != 0) {
                    TR(LKIT_EXPR_PARSE + 6);
                    goto err;
                }

                if ((expr->value.ref = lkit_expr_find(ctx, expr->name)) == NULL) {
                    //TRACE("failed probe '%s'", expr->name->data);
                    //dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
                    TR(LKIT_EXPR_PARSE + 7);
                    goto err;
                }

                /* functions only ATM */
                if (expr->value.ref->type->tag != LKIT_FUNC) {
                    TR(LKIT_EXPR_PARSE + 8);
                    goto err;
                }

                //TRACE("ISREF");
                /*
                 * Please note difference expr->value.ref->type vs
                 * expr->type.
                 *
                 */
                expr->isref = 1;
                expr->type = lkit_type_of_expr(expr->value.ref);

                tf = (lkit_func_t *)(expr->value.ref->type);

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

                        if ((argtype = lkit_type_of_expr(*arg)) == NULL) {
                            TR(LKIT_EXPR_PARSE + 12);
                            goto err;
                        }

                        if ((*paramtype)->tag == LKIT_VARARG) {
                            isvararg = 1;
                        } else {
                            if ((*paramtype)->tag != LKIT_UNDEF &&
                                argtype->tag != LKIT_UNDEF) {

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
    lkit_type_destroy(&type);
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
    lkit_gitem_t *gitem;
    fparser_datum_t **node = NULL;

    /* name */
    if (lparse_next_word_bytes(form, it, &name, 1) != 0) {
        TR(LKIT_PARSE_EXPRDEF + 1);
        goto err;
    }

    /* must be unique */
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
    if ((gitem = array_incr(&ctx->glist)) == NULL) {
        FAIL("array_incr");
    }
    gitem->name = name;
    gitem->expr = expr;

end:
    return res;

err:
    lexpr_destroy(&expr);
    res = 1;
    goto end;
}

static int
_acb(void *item, void *udata)
{
    lkit_gitem_t *gitem = item;
    struct {
        dict_traverser_t cb;
        void *udata;
    } *params = udata;
    return params->cb(gitem->name, gitem->expr, params->udata);
}

int
lexpr_transform(dict_traverser_t cb, void *udata)
{
    struct {
        dict_traverser_t cb;
        void *udata;
    } params = {cb, udata};
    return array_traverse(&root.glist, _acb, &params);
    //return dict_traverse(&root.ctx, cb, udata);
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
    array_init(&ctx->glist, sizeof(lkit_gitem_t), 0, NULL, NULL);
}

static void
lexpr_fini_ctx(lkit_expr_t *ctx)
{
    //dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
    dict_fini(&ctx->ctx);
    array_fini(&ctx->glist);
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

