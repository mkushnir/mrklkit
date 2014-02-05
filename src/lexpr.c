#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#include "diag.h"

/*
 * lkit_expr_t *, lkit_expr_t *
 */
static dict_t exprs;

static int
lexpr_dump(bytes_t *key, lkit_expr_t *value)
{
    bytestream_t bs;
    bytestream_init(&bs);

    lkit_type_str(value->type, &bs);
    bytestream_cat(&bs, 1, "\0");
    TRACE("EXPR: %s:%s", (key != NULL) ? (char *)(key->data) : "<null>", SDATA(&bs, 0));
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
lkit_expr_parse(fparser_datum_t *dat, int seterror)
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
    expr->isref = 0;

    /* first probe for type/reference */
    if ((expr->type = lkit_type_parse(dat, 0)) == NULL) {
        /* expr found ? */
        //TRACE("value:");
        //fparser_datum_dump(&dat, NULL);

        switch (FPARSER_DATUM_TAG(dat)) {
            lkit_type_t *type;
            bytes_t *name;
            array_t *form;
            array_iter_t it;
            fparser_datum_t **node;
            lkit_func_t *tf;

        case FPARSER_INT:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_INT);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_STR:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_STR);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_FLOAT:
            expr->value.literal = dat;
            type = lkit_type_new(LKIT_FLOAT);
            if ((expr->type = lkit_type_find(type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_WORD:
            /*
             * must be exprref
             */
            name = (bytes_t *)(dat->body);
            if ((expr->value.ref = dict_get_item(&exprs, name)) == NULL) {
                goto err;
            }

            //TRACE("ISREF");
            expr->isref = 1;
            expr->type = expr->value.ref->type;
            break;

        case FPARSER_SEQ:
            form = (array_t *)(dat->body);
            if (lparse_first_word_bytes(form, &it, &name, 1) != 0) {
                goto err;
            }

            if ((expr->value.ref = dict_get_item(&exprs, name)) == NULL) {
                //TRACE("failed probe '%s'", name->data);
                //dict_traverse(&exprs, (dict_traverser_t)lexpr_dump, NULL);
                goto err;
            }

            //TRACE("ISREF");
            expr->isref = 1;
            expr->type = expr->value.ref->type;

            /* functions only ATM */
            if (expr->type->tag != LKIT_FUNC) {
                goto err;
            }

            tf = (lkit_func_t *)(expr->type);

            for (node = array_next(form, &it);
                 node != NULL;
                 node = array_next(form, &it)) {

                lkit_expr_t **arg;
                lkit_type_t **paramtype, *argtype;

                if ((arg = array_incr(&expr->subs)) == NULL) {
                    FAIL("array_incr");
                }
                if ((*arg = lkit_expr_parse(*node, 1)) == NULL) {
                    goto err;
                }

                TRACE("arg:");
                lexpr_dump(NULL, *arg);

                if ((paramtype = array_get(&tf->fields, it.iter)) == NULL) {
                    goto err;
                }

                if (*paramtype == NULL) {
                    goto err;
                }

                if ((argtype = type_of_expr(*arg)) == NULL) {
                    goto err;
                }
                if (lkit_type_cmp(*paramtype, argtype) != 0) {
                    goto err;
                }
            }

            if ((tf->fields.elnum - expr->subs.elnum) != 1) {
                goto err;
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
lkit_parse_exprdef(array_t *form, array_iter_t *it)
{
    int res = 0;
    bytes_t *name;
    lkit_expr_t *expr;
    fparser_datum_t **node;

    /* name */
    if (lparse_next_word_bytes(form, it, &name, 1) != 0) {
        goto err;
    }

    if (dict_get_item(&exprs, name) != NULL) {
        goto err;
    }

    /* type and value */
    if ((node = array_next(form, it)) == NULL) {
        goto err;
    }

    if ((expr = lkit_expr_parse(*node, 1)) == NULL) {
        (*node)->error = 1;
        goto err;
    }

    dict_set_item(&exprs, name, expr);

end:
    return res;

err:
    lexpr_destroy(&expr);
    res = 1;
    goto end;
}

void
lexpr_init(void)
{
    dict_init(&exprs, 101,
              (dict_hashfn_t)lkit_expr_hash,
              (dict_item_comparator_t)lkit_expr_cmp,
              (dict_item_finalizer_t)lkit_expr_fini_dict);
}

void lexpr_fini(void)
{
    dict_traverse(&exprs, (dict_traverser_t)lexpr_dump, NULL);
    dict_fini(&exprs);
}

