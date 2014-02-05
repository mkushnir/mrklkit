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
lexpr_dump(lkit_expr_t *key, UNUSED lkit_expr_t *value)
{
    bytestream_t bs;
    bytestream_init(&bs);

    lkit_type_str(key->type, &bs);
    bytestream_cat(&bs, 1, "\0");
    TRACE("%s:%s", key->name->data, SDATA(&bs, 0));

    bytestream_fini(&bs);
    return 0;
}

static uint64_t
lkit_expr_hash(lkit_expr_t *expr)
{
    if (expr == NULL) {
        return 0;
    }
    //TRACE("expr=%s:%ld", expr->name->data, bytes_hash(expr->name));
    return bytes_hash(expr->name);
}

static int
lkit_expr_cmp(UNUSED lkit_expr_t *a, UNUSED lkit_expr_t *b)
{
    int diff;
    uint64_t ha, hb;

    ha = lkit_expr_hash(a);
    hb = lkit_expr_hash(b);
    diff = (int)(ha - hb);
    if (diff == 0) {
        ssize_t szdiff = a->name->sz - b->name->sz;

        if (szdiff == 0) {
            return memcmp(a->name->data, b->name->data, a->name->sz);
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
lkit_expr_fini_dict(lkit_expr_t *key, UNUSED lkit_expr_t *value)
{
    lexpr_destroy(&key);
    return 0;
}

int
lkit_expr_parse(array_t *form, array_iter_t *it)
{
    int res = 0;
    lkit_expr_t *expr = NULL, *qwe;
    fparser_datum_t **node;

    if ((expr = malloc(sizeof(lkit_expr_t))) == NULL) {
        FAIL("malloc");
    }

    if (array_init(&expr->subs, sizeof(lkit_expr_t *), 0,
                   NULL,
                   (array_finalizer_t)lexpr_destroy) != 0) {
        FAIL("array_init");
    }
    expr->isref = 0;

    /* name */
    if (lparse_next_word_bytes(form, it, &expr->name, 1) != 0) {
        goto err;
    }

    //TRACE("checking expr name '%s'", expr->name->data);
    if ((qwe = dict_get_item(&exprs, expr)) != NULL) {
        //TRACE("found expr name '%s'", qwe->name->data);
        goto err;
    }

    /* type and value */
    if ((node = array_next(form, it)) == NULL) {
        goto err;
    }

    /* first probe for type/reference */
    if ((expr->type = lkit_type_parse(*node, 0)) == NULL) {
        /* expr found ? */
        TRACE("value:");
        fparser_datum_dump(node, NULL);

        switch (FPARSER_DATUM_TAG(*node)) {
            lkit_expr_t probe;
            array_t *nform;
            array_iter_t nit;

        case FPARSER_INT:
            expr->value.literal = *node;
            probe.type = lkit_type_new(LKIT_INT);
            if ((expr->type = lkit_type_find(probe.type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_STR:
            expr->value.literal = *node;
            probe.type = lkit_type_new(LKIT_STR);
            if ((expr->type = lkit_type_find(probe.type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_FLOAT:
            expr->value.literal = *node;
            probe.type = lkit_type_new(LKIT_FLOAT);
            if ((expr->type = lkit_type_find(probe.type)) == NULL) {
                goto err;
            }
            break;

        case FPARSER_WORD:
            /*
             * must be exprref
             */
            probe.name = (bytes_t *)((*node)->body);
            if ((expr->value.ref = dict_get_item(&exprs, &probe)) == NULL) {
                goto err;

            } else {
                TRACE("ISREF");
                expr->isref = 1;
                expr->type = expr->value.ref->type;
            }
            break;

        case FPARSER_SEQ:
            nform = (array_t *)((*node)->body);
            if (lparse_first_word_bytes(nform, &nit, &probe.name, 1) != 0) {
                goto err;
            }
            if ((expr->value.ref = dict_get_item(&exprs, &probe)) == NULL) {
                TRACE("failed probe '%s'", probe.name->data);
                dict_traverse(&exprs, (dict_traverser_t)lexpr_dump, NULL);
                goto err;
            } else {
                TRACE("ISREF");
                expr->isref = 1;
                expr->type = expr->value.ref->type;
            }
            break;

        default:
            FAIL("lkit_expr_parse");
        }

    } else {
        /* typed declaration */
        TRACE("type:");
        lkit_type_dump(expr->type);
        expr->value.ref = NULL;
    }

    dict_set_item(&exprs, expr, expr);

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

