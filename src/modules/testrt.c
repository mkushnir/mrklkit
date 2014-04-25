#include <stdio.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/module.h>
#include <mrklkit/lparse.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/builtin.h>
#include <mrklkit/util.h>
#include <mrklkit/modules/testrt.h>
#include <mrklkit/modules/dparser.h>
#include <mrklkit/modules/dsource.h>

#include "builtin_private.h"
#include "diag.h"

static lkit_expr_t root;
static bytes_t *_if, *_and, *_comma;

rt_struct_t *testrt_source = NULL;


int
testrt_run(bytestream_t *bs, dsource_t *ds)
{
    int res = 0;
    char enddelim = '\0';

    testrt_source = mrklkit_rt_struct_new(ds->_struct);

    if ((res = dparse_struct(bs,
                             ds->fdelim,
                             ds->rdelim,
                             ds->_struct,
                             testrt_source,
                             &enddelim,
                             ds->parse_flags)) == DPARSE_NEEDMORE) {
        goto end;

    } else if (res == DPARSE_ERRORVALUE) {
        goto end;

    } else {
        if (enddelim != '\n') {
            /* truncated input? */
            res = 1;
            goto end;
        }
    }

end:
    mrklkit_rt_struct_dtor(&testrt_source);
    return res;
}


static int
parse_quals(array_t *form,
                 array_iter_t *it,
                 unsigned char *qual,
                 testrt_t *trt)
{
    char *s = (char *)qual;

    if (strcmp(s, ":dsource") == 0) {
        bytes_t *value;
        if (lparse_next_word_bytes(form, it, &value, 1) != 0) {
            return 1;
        }
        trt->dsource = value;
    } else if (strcmp(s, ":id") == 0) {
        int64_t val;
        if (lparse_next_int(form, it, &val, 1) != 0) {
            return 1;
        }
        trt->id = bytes_new(64);
        snprintf((char *)trt->id->data, trt->id->sz, "testrt.%016lx", val);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}


static testrt_t *
testrt_new(void)
{
    testrt_t *res;

    if ((res = malloc(sizeof(testrt_t))) == NULL) {
        FAIL("malloc");
    }

    res->dsource = NULL;
    res->id = NULL;
    res->seeexpr = NULL;
    res->doexpr = lkit_expr_new(&root);
    res->takeexpr = lkit_expr_new(&root);

    return res;
}


UNUSED static void
testrt_destroy(testrt_t **trt)
{
    if (*trt != NULL) {
        (void)lkit_expr_destroy(&(*trt)->seeexpr);
        (void)lkit_expr_destroy(&(*trt)->doexpr);
        (void)lkit_expr_destroy(&(*trt)->takeexpr);
        bytes_destroy(&(*trt)->id);
        free(*trt);
        *trt = NULL;
    }
}


static int
_parse_take(lkit_expr_t *ctx,
            testrt_t *trt,
            array_t *form,
            array_iter_t *it,
            int seterror)
{
    fparser_datum_t **node;

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **expr;

        if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
            TRRET(TESTRT_PARSE + 400);
        }

        if ((expr = array_incr(&trt->takeexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*expr = lkit_expr_parse(ctx, *node, seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 401);
        }
    }

    return 0;
}


static int
_parse_do(lkit_expr_t *ctx,
          testrt_t *trt,
          array_t *form,
          array_iter_t *it,
          int seterror)
{
    fparser_datum_t **node;

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **expr;

        if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
            TRRET(TESTRT_PARSE + 300);
        }

        if ((expr = array_incr(&trt->doexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*expr = lkit_expr_parse(ctx, *node, seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 301);
        }
    }

    return 0;
}


static int
_parse_see(lkit_expr_t *ctx,
           testrt_t *trt,
           array_t *form,
           array_iter_t *it,
           UNUSED int seterror)
{
    fparser_datum_t **node;
    lkit_expr_t *ifexpr, **subexpr, *andexpr;

    /* create an if expression if not exists */
    if (trt->seeexpr == NULL) {
        lkit_expr_t *texpr, *fexpr;

        trt->seeexpr = lkit_expr_new(&root);
        ifexpr = trt->seeexpr;
        ifexpr->name = _if;

        if ((ifexpr->value.ref = lkit_expr_find(ctx,
                                              ifexpr->name)) == NULL) {
            TRACE("failed probe '%s'", ifexpr->name->data);
            TRRET(TESTRT_PARSE + 200);
        }

        if ((ifexpr->type = lkit_expr_type_of(ifexpr->value.ref)) == NULL) {
            TRRET(TESTRT_PARSE + 201);
        }

        andexpr = lkit_expr_new(ctx);
        andexpr->name = _and;
        if ((andexpr->value.ref = lkit_expr_find(ctx,
                                                 andexpr->name)) == NULL) {
            TRACE("failed probe '%s'", andexpr->name->data);
            TRRET(TESTRT_PARSE + 202);
        }
        if ((andexpr->type = lkit_expr_type_of(andexpr->value.ref)) == NULL) {
            TRRET(TESTRT_PARSE + 203);
        }

        /* condition */
        if ((subexpr = array_incr(&ifexpr->subs)) == NULL) {
            FAIL("array_incr");
        }
        *subexpr = andexpr;

        /* texpr */
        texpr = lkit_expr_new(ctx);
        texpr->name = _comma;
        if ((texpr->value.ref = lkit_expr_find(ctx,
                                              texpr->name)) == NULL) {
            TRACE("failed probe '%s'", texpr->name->data);
            TRRET(TESTRT_PARSE + 204);
        }

        /* hack ... */
        if ((texpr->type = lkit_type_get(LKIT_VOID)) == NULL) {
            TRRET(TESTRT_PARSE + 205);
        }

        if ((subexpr = array_incr(&ifexpr->subs)) == NULL) {
            FAIL("array_incr");
        }
        *subexpr = texpr;


        /* fexpr */
        fexpr = lkit_expr_new(ctx);
        fexpr->name = _comma;
        if ((fexpr->value.ref = lkit_expr_find(ctx,
                                              fexpr->name)) == NULL) {
            TRACE("failed probe '%s'", fexpr->name->data);
            TRRET(TESTRT_PARSE + 206);
        }

        /* hack ... */
        if ((fexpr->type = lkit_type_get(LKIT_VOID)) == NULL) {
            TRRET(TESTRT_PARSE + 207);
        }

        if ((subexpr = array_incr(&ifexpr->subs)) == NULL) {
            FAIL("array_incr");
        }
        *subexpr = fexpr;

    } else {
        ifexpr = trt->seeexpr;
        if ((subexpr = array_get(&ifexpr->subs, 0)) == NULL) {
            FAIL("array_incr");
        }
        andexpr = *subexpr;
    }

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **arg;

        if ((arg = array_incr(&andexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*arg = lkit_expr_parse(ctx, *node, 1)) == NULL) {
            TRRET(TESTRT_PARSE + 208);
        }
    }

    /* remove undef and validate*/
    lkit_expr_dump(ifexpr);
    if (builtin_remove_undef(ifexpr) != 0) {
        TRRET(TESTRT_PARSE + 209);
    }
    lkit_expr_dump(ifexpr);

    return 0;
}

static int
_parse_clause(lkit_expr_t *ctx,
              testrt_t *trt,
              fparser_datum_t *dat,
              int seterror)
{

    switch (FPARSER_DATUM_TAG(dat)) {
    case FPARSER_SEQ:
        {
            array_t *form;
            array_iter_t it;
            bytes_t *cname = NULL;

            form = (array_t *)(dat->body);

            if (lparse_first_word_bytes(form, &it, &cname, seterror) != 0) {
                TRRET(TESTRT_PARSE + 100);
            }

            TRACE("clause name %s", cname->data);

            if (strcmp((char *)cname->data, "SEE") == 0) {
                TRACE("parse see");
                if (_parse_see(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 101);
                }
            } else if (strcmp((char *)cname->data, "DO") == 0) {
                TRACE("parse do");
                if (_parse_do(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else if (strcmp((char *)cname->data, "TAKE") == 0) {
                TRACE("parse take");
                if (_parse_take(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else {
                lkit_expr_t **expr;

                TRACE("parse lkit expr");
                if ((expr = array_incr(&ctx->subs)) == NULL) {
                    FAIL("array_incr");
                }
                if ((*expr = lkit_expr_parse(ctx, dat, 1)) == NULL) {
                    dat->error = 1;
                    TRRET(TESTRT_PARSE + 103);
                }
            }
        }
        break;

    default:
        TRRET(TESTRT_PARSE + 104);
    }

    return 0;
}


static int
_parse_trt(array_t *form, array_iter_t *it)
{
    fparser_datum_t **node = NULL;
    UNUSED lkit_gitem_t **gitem;
    testrt_t *trt;

    if ((trt = testrt_new()) == NULL) {
        TRRET(TESTRT_PARSE + 1);
    }

    /* (testrt quals clause ...) */

    /* quals */
    if (lparse_quals(form,
                 it,
                 (quals_parser_t)parse_quals,
                 trt) != 0) {
        TRRET(TESTRT_PARSE + 2);
    }

    if (trt->id == NULL) {
        TRRET(TESTRT_PARSE + 3);
    }

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        if (_parse_clause(&root, trt, *node, 1) != 0) {
            TRRET(TESTRT_PARSE + 4);
        }
    }

    testrt_destroy(&trt);

    return 0;
}


UNUSED static int
_acb(lkit_gitem_t **gitem, void *udata)
{
    int (*cb)(lkit_expr_t *) = udata;
    return cb((*gitem)->expr);
}


UNUSED static int
__compile(lkit_gitem_t **gitem, UNUSED void *udata)
{
    UNUSED bytes_t *name = (*gitem)->name;
    UNUSED lkit_expr_t *expr = (*gitem)->expr;
    UNUSED LLVMModuleRef module = (LLVMModuleRef)udata;

    TRACE("compiling %s", name->data);
    lkit_expr_dump(expr);
    return builtingen_sym_compile(gitem, udata);
    return 0;
}


static int
_compile(UNUSED LLVMModuleRef module)
{
    //array_traverse(&root.glist, (array_traverser_t)_acb, lkit_expr_dump);

    if (array_traverse(&root.glist,
                       (array_traverser_t)__compile,
                       module) != 0) {
        return 1;
    }

    //if (array_traverse(&root.glist,
    //                   (array_traverser_t)builtingen_sym_compile,
    //                   module) != 0) {
    //    return 1;
    //}
    return 0;
}


static void
_init(void)
{
    //size_t i;
    //struct {
    //    bytes_t **var;
    //    char *val
    //} const_bytes[] = {
    //    &_if, "if",
    //    &_and, "and",
    //};

    //for (i = 0; i < countof(const_bytes); ++i) {
    //    *const_bytes[i].var = bytes_new(strlen(const_bytes[i].val) + 1);
    //    memcpy((*const_bytes[i].var)->data, const_bytes[i].val, (*const_bytes[i].var)->sz);
    //}

    _if = bytes_new(3);
    memcpy(_if->data, "if", 3);

    _and = bytes_new(4);
    memcpy(_and->data, "and", 4);

    _comma = bytes_new(2);
    memcpy(_comma->data, ",", 2);

    lkit_expr_init(&root, builtin_get_root_ctx());
    dsource_init_module();
}


static void
_fini(void)
{
    dsource_fini_module();
    lkit_expr_fini(&root);
    bytes_destroy(&_if);
    bytes_destroy(&_and);
    bytes_destroy(&_comma);
}

static mrklkit_parser_info_t _parsers[] = {
    {"testrt", _parse_trt},
    {"dsource", dsource_parse},
    {NULL, NULL},
};


mrklkit_module_t testrt_module = {
    _init,
    _fini,
    _parsers,
    _compile,
};
