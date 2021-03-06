#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/hash.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(lexpr);
#endif

static void lexpr_dump(mnbytestream_t *, lkit_expr_t *, int);


mnbytes_t *
lkit_cexpr_qual_name(lkit_cexpr_t *ectx, mnbytes_t *name)
{
    mnbytes_t *res;

    if (ectx->base.name != NULL) {
        res = bytes_printf("%s::%s", ectx->base.name->data, name->data);
    } else {
        res = bytes_new_from_str((char *)name->data);
    }

    return res;
}


int
lkit_expr_is_constant(lkit_expr_t *expr)
{
    while (expr->isref) {
        expr = expr->value.ref;
    }
    assert(!expr->isref);
    return expr->value.literal != NULL;
}


static void
lexpr_dump_flags(mnbytestream_t *bs, lkit_expr_t *expr)
{
    if (expr->isctx) {
        bytestream_cat(bs, 1, "c");
    }
    if (expr->isref) {
        bytestream_cat(bs, 1, "r");
    }
    if (expr->isbuiltin) {
        bytestream_cat(bs, 1, "b");
    }
    if (expr->ismacro) {
        bytestream_cat(bs, 1, "m");
    }
    if (expr->lazy_init) {
        bytestream_cat(bs, 1, "l");
    }
    if (expr->undef_removed) {
        bytestream_cat(bs, 1, "U");
    }
    if (expr->fparam) {
        bytestream_cat(bs, 1, "f");
    }
    if (expr->zref) {
        bytestream_cat(bs, 1, "z");
    }
    switch (expr->mpolicy) {
    case LKIT_MPMALLOC:
        bytestream_cat(bs, 1, "a");
        break;
    case LKIT_MPMPOOL:
        bytestream_cat(bs, 1, "p");
        break;
    default:
        break;
    }
}


static void
lexpr_dump(mnbytestream_t *bs, lkit_expr_t *expr, int level)
{
    char fmt[32];

    snprintf(fmt, sizeof(fmt), "%%%dc", level);
    bytestream_nprintf(bs, 32, fmt, ' ');

    lkit_type_str(lkit_expr_type_of(expr), bs);
    bytestream_cat(bs, 1, " ");
    if (expr->isref) {
        if (expr->name != NULL) {
            if (expr->error) {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 16 +
                    SIZEOFCOLOR(ERRCOLOR),
                    ERRCOLOR("<REF %s :"), expr->name->data);
            } else {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 16,
                    "<REF %s :", expr->name->data);
            }
        } else {
            if (expr->error) {
                bytestream_cat(bs, 13 + SIZEOFCOLOR(ERRCOLOR),
                               ERRCOLOR("<REF <NULL> :"));
            } else {
                bytestream_cat(bs, 13, "<REF <NULL> :");
            }
        }
    } else {
        if (expr->name != NULL) {
            if (expr->value.literal == NULL) {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 16,
                    "%s: <NULL> :", expr->name->data);
            } else {
                bytestream_nprintf(bs,
                    strlen((char *)expr->name->data) + 4,
                    "%s: ", expr->name->data);
                fparser_datum_dump_bytestream(expr->value.literal, bs);
                bytestream_cat(bs, 3, " :f");
            }
        } else {
            if (expr->value.literal == NULL) {
                if (expr->error) {
                    bytestream_cat(bs, 17 + SIZEOFCOLOR(ERRCOLOR),
                                   ERRCOLOR("<LITERAL <null> :"));
                } else {
                    bytestream_cat(bs, 17, "<LITERAL <null> :");
                }
            } else {
                if (expr->error) {
                    bytestream_cat(bs, 11 + SIZEOFCOLOR(ERRCOLOR),
                                   ERRCOLOR("<LITERAL>: "));
                } else {
                    bytestream_cat(bs, 11, "<LITERAL>: ");
                }
                fparser_datum_dump_bytestream(expr->value.literal, bs);
                bytestream_cat(bs, 2, " :");
            }
        }
    }

    lexpr_dump_flags(bs, expr);
    bytestream_nprintf(bs, 4, ">\n");

    if (expr->subs.elnum > 0) {
        lkit_expr_t **subexpr;
        mnarray_iter_t it;

        bytestream_nprintf(bs, 32, fmt, ' ');

        if (expr->error) {
            bytestream_cat(bs, 6 + SIZEOFCOLOR(ERRCOLOR), ERRCOLOR("SUBS:\n"));
        } else {
            bytestream_cat(bs, 6, "SUBS:\n");
        }
        for (subexpr = array_first(&expr->subs, &it);
             subexpr != NULL;
             subexpr = array_next(&expr->subs, &it)) {

            lexpr_dump(bs, *subexpr, level + 2);
        }
    }
}

int
lkit_expr_dump(lkit_expr_t *expr)
{
    mnbytestream_t bs;

    bytestream_init(&bs, 4096);
    lexpr_dump(&bs, expr, 0);
    bytestream_cat(&bs, 1, "\0");
    TRACEC("%s", SDATA(&bs, 0));
    //D64(SDATA(&bs, 0), SEOD(&bs));
    bytestream_fini(&bs);
    return 0;
}

static uint64_t
lkit_expr_hash(mnbytes_t *key)
{
    /* this check is not in bytes_hash() */
    if (key == NULL) {
        return 0;
    }
    return bytes_hash(key);
}

static int
lkit_expr_cmp(mnbytes_t *a, mnbytes_t *b)
{
    int64_t diff;
    uint64_t ha, hb;

    ha = lkit_expr_hash(a);
    hb = lkit_expr_hash(b);
    diff = (int64_t)(ha - hb);
    if (diff == 0) {
        ssize_t szdiff = a->sz - b->sz;

        if (szdiff == 0) {
            return memcmp(a->data, b->data, a->sz);
        }

        return szdiff > 0 ? 1 : -1;
    }
    return diff > 0 ? 1 : -1;
}


lkit_type_t *
lkit_expr_type_of(lkit_expr_t *expr)
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
lkit_expr_find(lkit_cexpr_t *ectx, mnbytes_t *name)
{
    mnhash_item_t *dit;
    lkit_cexpr_t *cctx;

    for (cctx = ectx; cctx!= NULL; cctx = cctx->parent) {
        if ((dit = hash_get_item(&cctx->ctx, name)) != NULL) {
            lkit_expr_t *expr;

            expr = dit->value;
            return expr;
        }
    }
    return NULL;
}


void
lkit_expr_set_referenced(lkit_expr_t *expr)
{
    lkit_expr_t **pexpr;
    mnarray_iter_t it;

    for (pexpr = array_first(&expr->subs, &it);
         pexpr != NULL;
         pexpr = array_next(&expr->subs, &it)) {
        if ((*pexpr)->isref || (*pexpr)->ismacro) {
            ++(*pexpr)->referenced;
            lkit_expr_set_referenced(*pexpr);
            ++(*pexpr)->value.ref->referenced;
            lkit_expr_set_referenced((*pexpr)->value.ref);
        }
    }
    if (expr->isref || expr->ismacro) {
        ++expr->referenced;
        if (expr->isref) {
            lkit_expr_set_referenced(expr->value.ref);
        }
    }
}


/*
 * XXX TODO
 */
static int
_lkit_expr_promote_policy(mrklkit_ctx_t *mctx,
                          lkit_expr_t *expr,
                          lkit_mpolicy_t mpolicy)
{
    if (expr->isref) {
        assert(expr->value.ref != NULL);
        if (_lkit_expr_promote_policy(mctx, expr->value.ref, mpolicy) == 0) {
            if (expr->mpolicy <= mpolicy) {
                expr->mpolicy = mpolicy;
                return 0;
            }
        }
    } else {
        return 0;
    }
    return 1;
}

int
lkit_expr_promote_policy(mrklkit_ctx_t *mctx,
                         lkit_expr_t *expr,
                         lkit_mpolicy_t mpolicy)
{
    if (expr->mpolicy == mpolicy) {
        return 0;
    }
    if (expr->mpolicy > mpolicy) {
        return 1;
    }
    return _lkit_expr_promote_policy(mctx, expr, mpolicy);
}


void
lkit_cexpr_fini(lkit_cexpr_t *ectx)
{
    array_fini(&(ectx->base.subs));
    hash_fini(&ectx->ctx);
    array_fini(&ectx->glist);
}


void
lkit_expr_fini(lkit_expr_t *expr)
{
    array_fini(&(expr->subs));
    if (expr->isctx) {
        lkit_cexpr_t *cexpr;

        cexpr = (lkit_cexpr_t *)expr;
        hash_fini(&cexpr->ctx);
        array_fini(&cexpr->glist);
    }
}


int
lkit_expr_destroy(lkit_expr_t **pexpr)
{
    if (*pexpr != NULL) {
        lkit_expr_fini(*pexpr);
        free(*pexpr);
        *pexpr = NULL;
    }
    return 0;
}


lkit_expr_t *
lkit_expr_new(void)
{
    lkit_expr_t *expr;

    if ((expr = malloc(sizeof(lkit_expr_t))) == NULL) {
        FAIL("malloc");
    }
    lkit_expr_init(expr);

    return expr;
}


static int
gitem_fini(lkit_gitem_t **gitem)
{
    if (*gitem != NULL) {
        BYTES_DECREF(&(*gitem)->name);
        free(*gitem);
        *gitem = NULL;
    }
    return 0;
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


void
lkit_cexpr_init(lkit_cexpr_t *ectx, lkit_cexpr_t *pectx)
{
    lkit_expr_init(&ectx->base);
    ectx->base.isctx = -1;
    ectx->parent = pectx;
    array_init(&ectx->glist, sizeof(lkit_gitem_t *), 0,
               (array_initializer_t)gitem_init,
               (array_finalizer_t)gitem_fini);
    hash_init(&ectx->ctx, 101,
              (hash_hashfn_t)lkit_expr_hash,
              (hash_item_comparator_t)lkit_expr_cmp,
              NULL);
}


lkit_cexpr_t *
lkit_cexpr_new(lkit_cexpr_t *pectx)
{
    lkit_cexpr_t *ectx;

    if ((ectx = malloc(sizeof(lkit_cexpr_t))) == NULL) {
        FAIL("malloc");
    }
    lkit_cexpr_init(ectx, pectx);

    return ectx;
}


lkit_expr_t *
lkit_expr_build_ref(lkit_cexpr_t *ectx, mnbytes_t *name)
{
    lkit_expr_t *expr;

    expr = lkit_expr_new();
    expr->name = name;
    expr->isref = -1;
    expr->value.ref = lkit_expr_find(ectx, name);
    expr->mpolicy = expr->value.ref->mpolicy;
    expr->type = lkit_expr_type_of(expr->value.ref);
    return expr;
}


lkit_expr_t *
lkit_expr_build_literal(mrklkit_ctx_t *mctx,
                        fparser_datum_t *value)
{
    lkit_expr_t *expr;

    expr = lkit_expr_new();
    expr->value.literal = value;
    switch (value->tag) {
    case FPARSER_STR:
        expr->type = lkit_type_get(mctx, LKIT_STR);
        break;

    case FPARSER_INT:
        expr->type = lkit_type_get(mctx, LKIT_INT);
        break;

    case FPARSER_FLOAT:
        expr->type = lkit_type_get(mctx, LKIT_FLOAT);
        break;

    case FPARSER_BOOL:
        expr->type = lkit_type_get(mctx, LKIT_BOOL);
        break;

    case FPARSER_VOID:
        expr->type = lkit_type_get(mctx, LKIT_VOID);
        break;

    case FPARSER_NULL:
        expr->type = lkit_type_get(mctx, LKIT_NULL);
        break;

    default:
        FAIL("lkit_expr_build_literal");
    }

    return expr;
}


lkit_expr_t *
lkit_expr_add_sub(lkit_expr_t *expr, lkit_expr_t *sub)
{
    lkit_expr_t **pexpr;
    if ((pexpr = array_incr(&expr->subs)) == NULL) {
        FAIL("array_incr");
    }
    *pexpr = sub;
    return sub;
}

void
lkit_expr_init(lkit_expr_t *expr)
{
    expr->name = NULL;
    expr->title = NULL;
    if (array_init(&expr->subs, sizeof(lkit_expr_t *), 0,
                   NULL,
                   (array_finalizer_t)lkit_expr_destroy) != 0) {
        FAIL("array_init");
    }
    expr->mpolicy = MRKLKIT_DEFAULT_MPOLICY;
    expr->isctx = 0;
    expr->isref = 0;
    expr->isbuiltin = 0;
    expr->value.literal = NULL;
    expr->error = 0;
    expr->ismacro = 0;
    expr->lazy_init = 0;
    expr->referenced = 0;
    expr->undef_removed = 0;
    expr->fparam = 0;
    expr->fparam_idx = -1;
    expr->zref = 0;
    expr->type = NULL;
}


void
lexpr_add_to_ctx(lkit_cexpr_t *ectx, mnbytes_t *name, lkit_expr_t *expr)
{
    lkit_gitem_t **gitem;

    hash_set_item(&ectx->ctx, name, expr);
    if ((gitem = array_incr(&ectx->glist)) == NULL) {
        FAIL("array_incr");
    }
    (*gitem)->name = lkit_cexpr_qual_name(ectx, name);
    (*gitem)->expr = expr;
}


static int
parse_expr_quals(mnarray_t *form,
                 mnarray_iter_t *it,
                 unsigned char *qual,
                 lkit_expr_t *expr)
{
    char *s = (char *)qual;

    if (strcmp(s, ":lazy") == 0) {
        int64_t value;

        if (lparse_next_int(form, it, &value, 1) != 0) {
            expr->error = -1;
            return 1;
        }
        expr->lazy_init = (int)value;

    } else if (strcmp(s, ":force") == 0) {
        int64_t value;

        if (lparse_next_int(form, it, &value, 1) != 0) {
            expr->error = -1;
            return 1;
        }
        expr->referenced = (int)value;

    } else if (strcmp(s, ":title") == 0) {
        if (lparse_next_str_bytes(form, it, &expr->title, 1) != 0) {
            expr->error = -1;
            return 1;
        }
    } else {
        fparser_datum_t **node;

        //TRACE("unknown qual: %s", s);
        /* expect a single argument, and ignore it */
        if ((node = array_next(form, it)) == NULL) {
            return 1;
        }
    }
    return 0;
}

#define ETR(m, dat, ty)                \
do {                                   \
    TRACE(m);                          \
    fparser_datum_dump_formatted(dat); \
    TRACEC(" !~ ");                    \
    lkit_type_dump((lkit_type_t *)ty); \
    TRACEC("\n");                      \
} while (0)

static lkit_expr_t *
_lkit_expr_parse(mrklkit_ctx_t *mctx,
                lkit_cexpr_t *ectx,
                fparser_datum_t *dat,
                int seterror,
                lkit_expr_parse_fixture_t fixture,
                void *udata)
{
    lkit_type_t *ty;
    lkit_expr_t *expr;

    expr = NULL;

    /* first probe for type/reference */
    if ((ty = lkit_type_parse(mctx, dat, 0)) == NULL) {
        /* expr found ? */
        //TRACE("value:");
        //fparser_datum_dump(&dat, NULL);

        switch (FPARSER_DATUM_TAG(dat)) {
        case FPARSER_INT:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_INT);
            break;

        case FPARSER_STR:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_STR);
            break;

        case FPARSER_FLOAT:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_FLOAT);
            break;

        case FPARSER_BOOL:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_BOOL);
            break;

        case FPARSER_VOID:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_VOID);
            break;

        case FPARSER_NULL:
            expr = lkit_expr_new();
            expr->value.literal = dat;
            expr->type = lkit_type_get(mctx, LKIT_NULL);
            break;

        case FPARSER_WORD:
            /*
             * must be exprref
             */
            expr = lkit_expr_new();
            expr->name = (mnbytes_t *)(dat->body);
            if ((expr->value.ref = lkit_expr_find(
                            ectx, expr->name)) == NULL) {
                int idx;

                idx = 1;
                if (fixture != NULL) {
                    int res;

                    /*
                     * a simple forward-looking logic: dispatch parsing to
                     * a fixture, and optionally retry lkit_expr_find().
                     */
                    if ((res = fixture(mctx,
                                       ectx,
                                       dat,
                                       udata,
                                       seterror)) != 0) {
                        if (res == LKIT_EXPR_PARSE_FIXTURE_TRYAGAIN) {
                            if ((expr->value.ref =
                                    lkit_expr_find(ectx,
                                                   expr->name)) == NULL) {
                                idx = 100;
                                goto err1;
                            } else {
                                /* ok */
                            }
                        } else {
                            idx = 101;
                            goto err1;
                        }
                    } else {
                        idx = 102;
                        goto err1;
                    }
                } else {
                    idx = 103;
                    goto err1;
                }

                goto done1;

err1:
                if (seterror) {
                    TRACEN("cannot find reference target: ");
                    TRACEC(ERRCOLOR("'%s'\n"), expr->name->data);
                }
                TR(LKIT_EXPR_PARSE + idx);
                goto err;

done1:
                ;
            }

            //TRACE("ISREF by %s", expr->name->data);
            expr->isref = -1;
            expr->mpolicy = expr->value.ref->mpolicy;
            expr->type = expr->value.ref->type;

            break;

        case FPARSER_SEQ:
            {
                lkit_type_t **paramtype = NULL;
                mnarray_t *form;
                mnarray_iter_t it;
                fparser_datum_t **node;
                fparser_tag_t tag;

                expr = lkit_expr_new();
                form = (mnarray_t *)(dat->body);
                if ((node = array_first(form, &it)) == NULL) {
                    TR(LKIT_EXPR_PARSE + 2);
                    goto err;
                }
                tag = FPARSER_DATUM_TAG(*node);
                if (tag == FPARSER_WORD) {
                    /*
                     * XXX (xxx) (xxx :q q :w w :e e)
                     */
                    if (lparse_first_word_bytes(form,
                                                &it,
                                                &expr->name,
                                                seterror) != 0) {
                        TR(LKIT_EXPR_PARSE + 3);
                        goto err;
                    }

                    if ((expr->value.ref = lkit_expr_find(
                                    ectx, expr->name)) == NULL) {
                        int idx;
                        idx = 4;
                        if (fixture != NULL) {
                            int res;

                            /*
                             * a simple forward-looking logic: dispatch
                             * parsing to a fixture, and optionally retry
                             * lkit_expr_find().
                             */
                            if ((res = fixture(mctx,
                                               ectx,
                                               dat,
                                               udata,
                                               seterror)) != 0) {
                                if (res == LKIT_EXPR_PARSE_FIXTURE_TRYAGAIN) {
                                    if ((expr->value.ref =
                                        lkit_expr_find(ectx,
                                                       expr->name)) == NULL) {
                                        idx = 40;
                                        goto err0;
                                    } else {
                                        /* ok */
                                    }
                                } else {
                                    idx = 41;
                                    goto err0;
                                }
                            } else {
                                idx = 42;
                                goto err0;
                            }
                        } else {
                            idx = 43;
                            goto err0;
                        }

                        goto done0;

err0:
                        if (seterror) {
                            TRACEN("cannot find definition: ");
                            TRACEC(ERRCOLOR("'%s'\n"), expr->name->data);
                        }
                        TR(LKIT_EXPR_PARSE + idx);
                        goto err;

done0:
                        ;
                    }

                    expr->isref = -1;
                    expr->mpolicy = expr->value.ref->mpolicy;
                    expr->type = lkit_expr_type_of(expr->value.ref);
                    assert(expr->type != NULL);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                    if (expr->value.ref->type->tag == LKIT_FUNC) {
                        lkit_func_t *tf = NULL;
                        int isvararg = 0, narg;

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

                            if ((*arg = _lkit_expr_parse(mctx,
                                                         ectx,
                                                         *node,
                                                         seterror,
                                                         fixture,
                                                         udata)) == NULL) {
                                TR(LKIT_EXPR_PARSE + 6);
                                goto err;
                            }

                            //TRACE("just parsed arg:");
                            //lkit_expr_dump(*arg);

                            paramtype = array_get(&tf->fields, narg);

                            if (!isvararg) {
                                if (paramtype == NULL) {
                                    ETR("too much arguments:",
                                        dat, expr->value.ref->type);
                                    TR(LKIT_EXPR_PARSE + 7);
                                    goto err;
                                }

                                if (*paramtype == NULL) {
                                    ETR("too much arguments:",
                                        dat, expr->value.ref->type);
                                    TR(LKIT_EXPR_PARSE + 8);
                                    goto err;
                                }

                                if ((argtype =
                                        lkit_expr_type_of(*arg)) == NULL) {
                                    TR(LKIT_EXPR_PARSE + 9);
                                    goto err;
                                }

                                if ((*paramtype)->tag == LKIT_VARARG) {
                                    isvararg = 1;
                                } else {
                                    if (((*paramtype)->tag != LKIT_UNDEF &&
                                         (*paramtype)->tag != LKIT_TY &&
                                         (*paramtype)->tag != LKIT_ANY) &&
                                        (argtype->tag != LKIT_UNDEF &&
                                         argtype->tag != LKIT_TY)) {

                                        if (lkit_type_cmp(*paramtype,
                                                          argtype) != 0) {
                                            ETR("argument type does not match "
                                                "formal parameter type:",
                                                dat, expr->value.ref->type);
                                            TR(LKIT_EXPR_PARSE + 10);
                                            lkit_type_dump(*paramtype);
                                            lkit_type_dump(argtype);
                                            goto err;
                                        }
                                    }
                                }

                            } else {
                                if (paramtype != NULL && *paramtype != NULL) {
                                    if ((*paramtype)->tag != LKIT_VARARG) {
                                        TR(LKIT_EXPR_PARSE + 11);
                                        goto err;
                                    }
                                }
                            }
                        }

                        if ((paramtype = array_last(&tf->fields,
                                                    &it)) == NULL) {
                            FAIL("array_get");
                        }

                        if ((*paramtype)->tag == LKIT_VARARG) {
                            isvararg = 1;

                        } else {
                            if (isvararg) {
                                TR(LKIT_EXPR_PARSE + 12);
                                goto err;
                            }
                        }

                        if (!isvararg) {
                            if ((tf->fields.elnum - expr->subs.elnum) != 1) {
                                ETR("number of arguments does not match "
                                    "type definition", dat,
                                    expr->value.ref->type);
                                TR(LKIT_EXPR_PARSE + 13);
                                goto err;
                            }
                        }
                    } else if (expr->value.ref->type->tag == LKIT_IR) {
                        /*
                         * re-work it
                         */
                        if (lparse_next_str_datum(form,
                                                  &it,
                                                  &expr->value.literal,
                                                  1) != 0) {
                            TRACEN("cannot find body: ");
                            TRACEC(ERRCOLOR("'%s'\n"), expr->name->data);
                            TR(LKIT_EXPR_PARSE + 14);
                            goto err;
                        }

                        expr->type = ty;
                        expr->isref = 0;

                    } else {
                        /*
                         * (xxx) not a function
                         */
                    }

                } else if (tag == FPARSER_INT) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_INT);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else if (tag == FPARSER_FLOAT) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_FLOAT);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else if (tag == FPARSER_STR) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_STR);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else if (tag == FPARSER_BOOL) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_BOOL);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else if (tag == FPARSER_VOID) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_VOID);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else if (tag == FPARSER_NULL) {
                    expr->value.literal = *node;
                    expr->type = lkit_type_get(mctx, LKIT_NULL);

                    /* quals */
                    lparse_quals(form,
                                 &it,
                                 (quals_parser_t)parse_expr_quals,
                                 expr);

                } else {
                    TR(LKIT_EXPR_PARSE + 14);
                    goto err;
                }
                //TRACE("resulting expr: %ld subs", expr->subs.elnum);
                //lkit_expr_dump(expr);

            }
            break;

        default:
            FAIL("_lkit_expr_parse");
        }

    } else {
        /* typed declaration */
        if (ty->tag == LKIT_FUNC) {
            expr = (lkit_expr_t *)lkit_cexpr_new(ectx);
        } else {
            expr = lkit_expr_new();
        }
        expr->type = ty;
        expr->value.ref = NULL;
        //TRACE("type:");
        //lkit_type_dump(expr->type);
    }

    /*
     * mark all references for eager initializers
     */
    if (mctx->mark_referenced) {
        if (!expr->lazy_init) {
            if (expr->isref || expr->ismacro) {
                lkit_expr_set_referenced(expr->value.ref);
            }
        }
    }

end:
    return expr;

err:
    dat->error = seterror;
    (void)lkit_expr_destroy(&expr);
    goto end;
}


lkit_expr_t *
lkit_expr_parse(mrklkit_ctx_t *mctx,
                lkit_cexpr_t *ectx,
                fparser_datum_t *dat,
                int seterror)
{
    return _lkit_expr_parse(mctx, ectx, dat, seterror, NULL, NULL);
}


int
lkit_expr_parse2(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 fparser_datum_t *dat,
                 int seterror,
                 lkit_expr_t **pexpr,
                 lkit_expr_parse_fixture_t fixture,
                 void *udata)
{
    if ((*pexpr = _lkit_expr_parse(mctx,
                                   ectx,
                                   dat,
                                   seterror,
                                   fixture,
                                   udata)) == NULL) {
        TRRET(LKIT_PARSE_EXPR2 + 1);
    }
    return 0;
}

void
lexpr_init(void)
{
}

void lexpr_fini(void)
{
}

