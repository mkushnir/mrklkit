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

static lkit_type_t *type_of_expr(lkit_expr_t *);
static void lexpr_dump(bytestream_t *, lkit_expr_t *);

static void
lexpr_dump(bytestream_t *bs, lkit_expr_t *expr)
{
    lkit_type_str(type_of_expr(expr), bs);
    bytestream_cat(bs, 1, " ");
    //lkit_type_str(expr->type, bs);
    //bytestream_cat(bs, 2, "] ");
    if (expr->isref) {
        if (expr->name != NULL) {
            bytestream_nprintf(bs,
                strlen((char *)expr->name->data) + 8,
                "<REF %s>\n", expr->name->data);
        } else {
            bytestream_cat(bs, 13, "<REF <NULL>>\n");
        }
        //lexpr_dump(bs, expr->value.ref);
    } else {
        if (expr->name != NULL) {
            if (expr->value.literal == NULL) {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 10,
                    "%s: <NULL>\n", expr->name->data);
            } else {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 3,
                    "%s: ", expr->name->data);
                fparser_datum_dump_bytestream(expr->value.literal, bs);
                bytestream_cat(bs, 1, "\n");
            }
        } else {
            if (expr->value.literal == NULL) {
                bytestream_cat(bs, 17, "<LITERAL <null>>\n");
            } else {
                bytestream_cat(bs, 11, "<LITERAL>: ");
                fparser_datum_dump_bytestream(expr->value.literal, bs);
                bytestream_cat(bs, 1, "\n");
            }
        }
    }
    if (expr->subs.elnum > 0) {
        lkit_expr_t **subexpr;
        array_iter_t it;

        bytestream_cat(bs, 6, "SUBS:\n");
        for (subexpr = array_first(&expr->subs, &it);
             subexpr != NULL;
             subexpr = array_next(&expr->subs, &it)) {

            lexpr_dump(bs, *subexpr);
        }
    }
}

int
lkit_expr_dump(lkit_expr_t *expr)
{
    bytestream_t bs;

    bytestream_init(&bs);
    lexpr_dump(&bs, expr);
    bytestream_cat(&bs, 1, "\0");
    TRACE("%s", SDATA(&bs, 0));
    //D64(SDATA(&bs, 0), SEOD(&bs));
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


void
lkit_expr_fini(lkit_expr_t *expr)
{
    array_fini(&(expr->subs));
    lexpr_fini_ctx(expr);
}

static int
lexpr_destroy(lkit_expr_t **pexpr)
{
    if (*pexpr != NULL) {
        lkit_expr_fini(*pexpr);
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
    if (expr->type != NULL) {
        if (expr->type->tag == LKIT_FUNC) {
            lkit_func_t *tf;
            lkit_type_t **pty;

            tf = (lkit_func_t *)(expr->type);

            if ((pty = array_get(&tf->fields, 0)) == NULL) {
                FAIL("array_get");
            }
            return *pty;
        } else {
            return expr->type;
        }
    }
    return NULL;
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

static int
parse_expr_quals(array_t *form,
                 array_iter_t *it,
                 unsigned char *qual,
                 lkit_expr_t *expr)
{
    char *s = (char *)qual;

#define EXPR_SET_INT(m) \
    int64_t value; \
    if (lparse_next_int(form, it, &value, 1) != 0) { \
        expr->error = 1; \
        return 1; \
    } \
    expr->m = (int)value;

    if (strcmp(s, ":lazy") == 0) {
        EXPR_SET_INT(lazy_init);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

void
lkit_expr_init(lkit_expr_t *expr, lkit_expr_t *ctx)
{
    expr->name = NULL;
    if (array_init(&expr->subs, sizeof(lkit_expr_t *), 0,
                   NULL,
                   (array_finalizer_t)lexpr_destroy) != 0) {
        FAIL("array_init");
    }
    lexpr_init_ctx(expr);
    expr->parent = ctx;
    expr->isref = 0;
    expr->error = 0;
    expr->lazy_init = 0;
    expr->type = NULL;
}


lkit_expr_t *
lkit_expr_new(lkit_expr_t *ctx)
{
    lkit_expr_t *expr;

    if ((expr = malloc(sizeof(lkit_expr_t))) == NULL) {
        FAIL("malloc");
    }
    lkit_expr_init(expr, ctx);

    return expr;
}


lkit_expr_t *
lkit_expr_parse(lkit_expr_t *ctx, fparser_datum_t *dat, int seterror)
{
    lkit_expr_t *expr;

    expr = lkit_expr_new(ctx);

    /* first probe for type/reference */
    if ((expr->type = lkit_type_parse(dat, 0)) == NULL) {
        /* expr found ? */
        //TRACE("value:");
        //fparser_datum_dump(&dat, NULL);

        switch (FPARSER_DATUM_TAG(dat)) {
        case FPARSER_INT:
            expr->value.literal = dat;
            expr->type = lkit_type_get(LKIT_INT);
            break;

        case FPARSER_STR:
            expr->value.literal = dat;
            expr->type = lkit_type_get(LKIT_STR);
            break;

        case FPARSER_FLOAT:
            expr->value.literal = dat;
            expr->type = lkit_type_get(LKIT_FLOAT);
            break;

        case FPARSER_BOOL:
            expr->value.literal = dat;
            expr->type = lkit_type_get(LKIT_BOOL);
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

            //TRACE("ISREF by %s", expr->name->data);
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
                int isvararg = 0, narg;

                form = (array_t *)(dat->body);
                if (lparse_first_word_bytes(form, &it, &expr->name, 1) != 0) {
                    TR(LKIT_EXPR_PARSE + 7);
                    goto err;
                }

                if ((expr->value.ref = lkit_expr_find(ctx,
                                                      expr->name)) == NULL) {
                    TRACE("failed probe '%s'", expr->name->data);
                    //dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
                    TR(LKIT_EXPR_PARSE + 8);
                    goto err;
                }

                //TRACE("ISREF by %s", expr->name->data);
                expr->isref = 1;

                /* functions only ATM */
                if (expr->value.ref->type->tag != LKIT_FUNC) {
                    TR(LKIT_EXPR_PARSE + 9);
                    goto err;
                }

                /* quals */
                lparse_quals(form,
                             &it,
                             (quals_parser_t)parse_expr_quals,
                             expr);

                /*
                 * Please note difference expr->value.ref->type vs
                 * expr->type.
                 *
                 */
                if ((expr->type = type_of_expr(expr->value.ref)) == NULL) {
                    TR(LKIT_EXPR_PARSE + 10);
                    goto err;
                }

                assert(expr->type != NULL);

                tf = (lkit_func_t *)(expr->value.ref->type);

                narg = 1;
                for (node = array_next(form, &it);
                     node != NULL;
                     node = array_next(form, &it), ++narg) {

                    lkit_expr_t **arg;
                    lkit_type_t *argtype;

                    if ((arg = array_incr(&expr->subs)) == NULL) {
                        FAIL("array_incr");
                    }

                    if ((*arg = lkit_expr_parse(ctx, *node, 1)) == NULL) {
                        TR(LKIT_EXPR_PARSE + 11);
                        goto err;
                    }

                    //TRACE("just parsed arg:");
                    //lkit_expr_dump(*arg);

                    paramtype = array_get(&tf->fields, narg);

                    if (!isvararg) {
                        if (paramtype == NULL) {
                            TR(LKIT_EXPR_PARSE + 12);
                            goto err;
                        }

                        if (*paramtype == NULL) {
                            TR(LKIT_EXPR_PARSE + 13);
                            goto err;
                        }

                        if ((argtype = type_of_expr(*arg)) == NULL) {
                            TR(LKIT_EXPR_PARSE + 14);
                            goto err;
                        }

                        if ((*paramtype)->tag == LKIT_VARARG) {
                            isvararg = 1;
                        } else {
                            if ((*paramtype)->tag != LKIT_UNDEF &&
                                argtype->tag != LKIT_UNDEF) {

                                if (lkit_type_cmp(*paramtype, argtype) != 0) {
                                    lkit_type_dump(*paramtype);
                                    lkit_type_dump(argtype);
                                    lkit_expr_dump(expr);
                                    TR(LKIT_EXPR_PARSE + 15);
                                    goto err;
                                }
                            }
                        }

                    } else {
                        if (paramtype != NULL && *paramtype != NULL) {
                            if ((*paramtype)->tag != LKIT_VARARG) {
                                TR(LKIT_EXPR_PARSE + 16);
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
                        TR(LKIT_EXPR_PARSE + 17);
                        goto err;
                    }
                }

                if (!isvararg) {
                    if ((tf->fields.elnum - expr->subs.elnum) != 1) {
                        TR(LKIT_EXPR_PARSE + 17);
                        goto err;
                    }
                }

                //TRACE("resulting expr: %ld subs", expr->subs.elnum);
                //lkit_expr_dump(expr);

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


/*
 * (keyword name value)
 */
int
lkit_parse_exprdef(lkit_expr_t *ctx, array_t *form, array_iter_t *it)
{
    int res = 0;
    bytes_t *name = NULL;
    lkit_expr_t *expr = NULL;
    lkit_gitem_t **gitem;
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
    (*gitem)->name = name;
    (*gitem)->expr = expr;

end:
    return res;

err:
    lexpr_destroy(&expr);
    res = 1;
    goto end;
}

static int
gitem_init(lkit_gitem_t **gitem)
{
    if ((*gitem = malloc(sizeof(lkit_gitem_t))) == NULL) {
        FAIL("malloc");
    }
    (*gitem)->name = NULL;
    (*gitem)->expr = NULL;
    return 0;
}

static int
gitem_fini(lkit_gitem_t **gitem)
{
    if (*gitem != NULL) {
        free(*gitem);
        *gitem = NULL;
    }
    return 0;
}

void
lexpr_init_ctx(lkit_expr_t *ctx)
{
    dict_init(&ctx->ctx, 101,
              (dict_hashfn_t)lkit_expr_hash,
              (dict_item_comparator_t)lkit_expr_cmp,
              (dict_item_finalizer_t)lkit_expr_fini_dict);
    array_init(&ctx->glist, sizeof(lkit_gitem_t *), 0,
               (array_initializer_t)gitem_init,
               (array_finalizer_t)gitem_fini);
}

void
lexpr_fini_ctx(lkit_expr_t *ctx)
{
    //dict_traverse(&ctx->ctx, (dict_traverser_t)lexpr_dump, NULL);
    dict_fini(&ctx->ctx);
    array_fini(&ctx->glist);
}

void
lexpr_init(void)
{
}

void lexpr_fini(void)
{
}

