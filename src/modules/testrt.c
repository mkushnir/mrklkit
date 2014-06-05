#include <stdio.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/bytestream.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>

#include <mrklkit/module.h>
#include <mrklkit/mrklkit.h>
#include <mrklkit/fparser.h>
#include <mrklkit/dparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/builtin.h>
#include <mrklkit/util.h>

#include <mrklkit/modules/testrt.h>

#include "diag.h"


/* "fake" constants */
static bytes_t *_if, *_and, *_comma, *_call;
struct {
    bytes_t **var;
    char *val;
} const_bytes[] = {
    {&_if, "if"},
    {&_and, "and"},
    {&_comma, ","},
    {&_call, "call"},
};
static lkit_struct_t *null_struct;

/* shared context */
static array_t dsources;
static dict_t targets;
rt_struct_t *testrt_source = NULL;

static int
_parse_typedef(testrt_ctx_t *tctx, array_t *form, array_iter_t *it)
{
    lkit_type_t *ty = NULL;
    bytes_t *typename = NULL;

    if (lparse_next_word_bytes(form, it, &typename, 1) != 0) {
        TRRET(TESTRT_PARSE + 800);
    }

    if (strcmp((char *)typename->data, "url") == 0) {
        if ((ty = malloc(sizeof(lkit_type_t))) == NULL) {
            FAIL("malloc");
        }
        ty->tag = LKIT_USER + 100;
        ty->name = "url";
        ty->rtsz = 0;
        ty->dtor = NULL;
        ty->error = 0;
        ty = lkit_type_finalize(&tctx->mctx, ty);
        lkit_register_typedef(&tctx->mctx, ty, typename);
    } else {
        /* get back */
        --it->iter;
        return lkit_parse_typedef(&tctx->mctx, form, it);
    }


    return 0;
}

static int
_cb0(UNUSED bytes_t *key, lkit_type_t *value, LLVMContextRef lctx)
{
    if (value->tag == (int)(LKIT_USER + 100)) {
        value->backend = LLVMPointerType(LLVMStructTypeInContext(lctx,
                                                              NULL,
                                                              0,
                                                              0), 0);
    }
    return 0;
}


static int
_cb1(UNUSED bytes_t *key, lkit_type_t *value, LLVMContextRef lctx)
{
    int res;

    res = ltype_compile(value, lctx);
    if (res != 0) {
        //TRACE("Could not compile this type:");
        //lkit_type_dump(value);
    }
    return 0;
}


static int
_compile_type(testrt_ctx_t *tctx, LLVMContextRef lctx)
{
    (void)dict_traverse(&tctx->mctx.typedefs, (dict_traverser_t)_cb0, lctx);
    return dict_traverse(&tctx->mctx.types, (dict_traverser_t)_cb1, lctx);
}

static int
_parse_builtin(testrt_ctx_t *tctx, array_t *form, array_iter_t *it)
{
    return builtin_parse_exprdef(&tctx->mctx, &tctx->builtin, form, it);
}

/**
 * data source example
 *
 */

static void
dsource_init(dsource_t *dsource)
{
    dsource->timestamp_index = -1;
    dsource->date_index = -1;
    dsource->time_index = -1;
    dsource->duration_index = -1;
    dsource->error = 0;
    /* weak ref */
    dsource->kind = NULL;
    dsource->_struct = NULL;
}

static void
dsource_fini(dsource_t *dsource)
{
    dsource->timestamp_index = -1;
    dsource->date_index = -1;
    dsource->time_index = -1;
    dsource->duration_index = -1;
    dsource->error = 0;
    /* weak ref */
    dsource->kind = NULL;
    /* weak ref */
    dsource->_struct = NULL;
}

static dsource_t *
dsource_new(void)
{
    dsource_t **dsource;

    if ((dsource = array_incr(&dsources)) == NULL) {
        FAIL("array_incr");
    }
    if ((*dsource = malloc(sizeof(dsource_t))) == NULL) {
        FAIL("malloc");
    }
    dsource_init(*dsource);
    return *dsource;
}

static void
dsource_destroy(dsource_t **dsource)
{
    if (*dsource != NULL) {
        dsource_fini(*dsource);
        free(*dsource);
        *dsource = NULL;
    }
}


dsource_t *
dsource_get(const char *name)
{
    dsource_t **v;
    array_iter_t it;

    for (v = array_first(&dsources, &it);
         v != NULL;
         v = array_next(&dsources, &it)) {

        if (strcmp(name, (const char *)(*v)->kind->data) == 0) {
            return *v;
        }
    }

    return NULL;
}


static int
dsource_compile(LLVMModuleRef module)
{
    array_iter_t it;
    dsource_t **ds;

    for (ds = array_first(&dsources, &it);
         ds != NULL;
         ds = array_next(&dsources, &it)) {
        if (ltype_compile_methods((lkit_type_t *)(*ds)->_struct,
                                  module,
                                  (*ds)->kind)) {
            TRRET(DSOURCE + 100);
        }
    }
    return 0;
}


static int
dsource_link(LLVMExecutionEngineRef ee, LLVMModuleRef module)
{
    array_iter_t it;
    dsource_t **ds;


    for (ds = array_first(&dsources, &it);
         ds != NULL;
         ds = array_next(&dsources, &it)) {
        if (ltype_link_methods((lkit_type_t *)(*ds)->_struct,
                               ee,
                               module,
                               (*ds)->kind)) {
            TRRET(DSOURCE + 200);
        }
    }
    return 0;
}


/**
 * dsource ::= (dsource quals? kind _struct?)
 *
 */
static int
parse_dsource_quals(array_t *form,
                   array_iter_t *it,
                   unsigned char *qual,
                   dsource_t *dsource)
{
    char *s = (char *)qual;

#define DSOURCE_SET_INT(m) \
    int64_t value; \
    if (lparse_next_int(form, it, &value, 1) != 0) { \
        dsource->error = 1; \
        return 1; \
    } \
    dsource->m = (int)value;

    if (strcmp(s, ":tsidx") == 0) {
        DSOURCE_SET_INT(timestamp_index);
    } else if (strcmp(s, ":dtidx") == 0) {
        DSOURCE_SET_INT(date_index);
    } else if (strcmp(s, ":tmidx") == 0) {
        DSOURCE_SET_INT(time_index);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

static int
_parse_dsource(testrt_ctx_t *tctx, array_t *form, array_iter_t *it)
{
    dsource_t *dsource;
    fparser_datum_t **node;

    dsource = dsource_new();

    /* kind */
    if (lparse_next_word_bytes(form, it, &dsource->kind, 1) != 0) {
        dsource->error = 1;
        return 1;
    }
    /* quals */
    lparse_quals(form, it, (quals_parser_t)parse_dsource_quals, dsource);

    if ((node = array_next(form, it)) == NULL) {
        dsource->error = 1;
        return 1;
    }

    if ((dsource->_struct = (lkit_struct_t *)lkit_type_parse(&tctx->mctx,
                                                             *node,
                                                             1)) == NULL) {
        dsource->error = 1;
        return 1;
    }
    if (dsource->_struct->base.tag != LKIT_STRUCT) {
        dsource->error = 1;
        return 1;
    }

    dsource->fdelim = dsource->_struct->delim[0];

    return 0;
}


static int
remove_undef(testrt_ctx_t *tctx, lkit_expr_t *expr)
{
    char *name = (expr->name != NULL) ? (char *)expr->name->data : ")(";

    if (expr->type->tag == LKIT_UNDEF) {
        if (strcmp(name, "MERGE") == 0 ||
            strcmp(name, "SPLIT") == 0) {
            lkit_expr_t **arg;

            if ((arg = array_get(&expr->subs, 0)) == NULL) {
                TRRET(TESTRT_PARSE + 500);
            }

            //lkit_expr_dump(*arg);

            if (builtin_remove_undef(&tctx->mctx, &tctx->builtin, *arg) != 0) {
                TRRET(TESTRT_PARSE + 500);
            }

            expr->type = (*arg)->type;

        } else if (strcmp(name, "HANGOVER") == 0) {
        } else {
            return builtin_remove_undef(&tctx->mctx, &tctx->builtin, expr);
        }
    }
    return 0;
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
        char buf[1024];

        if (lparse_next_int(form, it, (int64_t *)&trt->id, 1) != 0) {
            return 1;
        }
        snprintf(buf, sizeof(buf), "testrt.%016lx", trt->id);
        trt->name = bytes_new_from_str(buf);
        BYTES_INCREF(trt->name);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

void
url_destroy(bytes_t **o)
{
    BYTES_DECREF(o);
}

static int
testrt_init(testrt_t *trt)
{
    trt->dsource = NULL;
    trt->id = 0;
    trt->name = NULL;
    trt->seeexpr = NULL;
    trt->doexpr = NULL;
    trt->takeexpr = NULL;
    array_init(&trt->otherexpr, sizeof(lkit_expr_t *), 0, NULL, NULL);
    return 0;
}


static int
testrt_fini(testrt_t *trt)
{
    (void)lkit_expr_destroy(&trt->seeexpr);
    (void)lkit_expr_destroy(&trt->doexpr);
    (void)lkit_expr_destroy(&trt->takeexpr);
    array_fini(&trt->otherexpr);
    bytes_decref(&trt->name);
    return 0;
}

static int
_parse_take(testrt_ctx_t *tctx,
            lkit_expr_t *ectx,
            testrt_t *trt,
            array_t *form,
            array_iter_t *it,
            int seterror)
{
    fparser_datum_t **node;
    lkit_struct_t *ts;

    if (trt->takeexpr != NULL) {
        TRRET(TESTRT_PARSE + 600);
    }

    trt->takeexpr = lkit_expr_new(&tctx->root);
    ts = (lkit_struct_t *)lkit_type_get(&tctx->mctx, LKIT_STRUCT);

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **expr;
        lkit_type_t **ty;

        if ((expr = array_incr(&trt->takeexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*expr = lkit_expr_parse(&tctx->mctx,
                                     ectx,
                                     *node,
                                     seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 601);
        }

        if (remove_undef(tctx, *expr) != 0) {
            TRRET(TESTRT_PARSE + 602);
        }

        if ((ty = array_incr(&ts->fields)) == NULL) {
            FAIL("array_incr");
        }
        *ty = (*expr)->type;
    }

    trt->takeexpr->type = lkit_type_finalize(&tctx->mctx, (lkit_type_t *)ts);

    return 0;
}


static int
_parse_do(testrt_ctx_t *tctx,
          lkit_expr_t *ectx,
          testrt_t *trt,
          array_t *form,
          array_iter_t *it,
          int seterror)
{
    fparser_datum_t **node;
    lkit_struct_t *ts;

    if (trt->doexpr != NULL) {
        TRRET(TESTRT_PARSE + 300);
    }

    trt->doexpr = lkit_expr_new(&tctx->root);

    ts = (lkit_struct_t *)lkit_type_get(&tctx->mctx, LKIT_STRUCT);

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **expr;
        lkit_type_t **ty;

        if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
            TRRET(TESTRT_PARSE + 301);
        }

        if ((expr = array_incr(&trt->doexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*expr = lkit_expr_parse(&tctx->mctx,
                                     ectx,
                                     *node,
                                     seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 302);
        }

        if (remove_undef(tctx, *expr) != 0) {
            TRRET(TESTRT_PARSE + 303);
        }

        if ((ty = array_incr(&ts->fields)) == NULL) {
            FAIL("array_incr");
        }
        *ty = (*expr)->type;
    }

    trt->doexpr->type = lkit_type_finalize(&tctx->mctx, (lkit_type_t *)ts);

    return 0;
}


static int
_parse_see(testrt_ctx_t *tctx,
           lkit_expr_t *ectx,
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

        trt->seeexpr = lkit_expr_new(&tctx->root);
        ifexpr = trt->seeexpr;
        ifexpr->name = _if;
        ifexpr->isref = 1;

        if ((ifexpr->value.ref = lkit_expr_find(ectx,
                                              ifexpr->name)) == NULL) {
            TRACE("failed probe '%s'", ifexpr->name->data);
            TRRET(TESTRT_PARSE + 200);
        }

        if ((ifexpr->type = lkit_expr_type_of(ifexpr->value.ref)) == NULL) {
            TRRET(TESTRT_PARSE + 201);
        }

        andexpr = lkit_expr_new(ectx);
        andexpr->name = _and;
        andexpr->isref = 1;
        if ((andexpr->value.ref = lkit_expr_find(ectx,
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
        texpr = lkit_expr_new(ectx);
        texpr->name = _comma;
        if ((texpr->value.ref = lkit_expr_find(ectx,
                                              texpr->name)) == NULL) {
            TRACE("failed probe '%s'", texpr->name->data);
            TRRET(TESTRT_PARSE + 204);
        }
        texpr->isref = 1;

        /* hack ... */
        if ((texpr->type = lkit_type_get(&tctx->mctx, LKIT_VOID)) == NULL) {
            TRRET(TESTRT_PARSE + 205);
        }

        if ((subexpr = array_incr(&ifexpr->subs)) == NULL) {
            FAIL("array_incr");
        }
        *subexpr = texpr;


        /* fexpr */
        fexpr = lkit_expr_new(ectx);
        fexpr->name = _comma;
        if ((fexpr->value.ref = lkit_expr_find(ectx,
                                              fexpr->name)) == NULL) {
            TRACE("failed probe '%s'", fexpr->name->data);
            TRRET(TESTRT_PARSE + 206);
        }
        fexpr->isref = 1;

        /* hack ... */
        if ((fexpr->type = lkit_type_get(&tctx->mctx, LKIT_VOID)) == NULL) {
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

        if ((*arg = lkit_expr_parse(&tctx->mctx,
                                    ectx,
                                    *node,
                                    1)) == NULL) {
            TRRET(TESTRT_PARSE + 208);
        }
    }

    /* remove undef and validate*/
    //lkit_expr_dump(ifexpr);
    if (remove_undef(tctx, ifexpr) != 0) {
        TRRET(TESTRT_PARSE + 209);
    }
    //lkit_expr_dump(ifexpr);

    return 0;
}

static int
_parse_clause(testrt_ctx_t *tctx,
              lkit_expr_t *ectx,
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

            //TRACE("clause name %s", cname->data);

            if (strcmp((char *)cname->data, "SEE") == 0) {
                //TRACE("parse see");
                if (form->elnum > 1) {
                    if (_parse_see(tctx, ectx, trt, form, &it, seterror) != 0) {
                        TRRET(TESTRT_PARSE + 101);
                    }
                }
            } else if (strcmp((char *)cname->data, "DO") == 0) {
                //TRACE("parse do");
                if (_parse_do(tctx, ectx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else if (strcmp((char *)cname->data, "TAKE") == 0) {
                //TRACE("parse take");
                if (_parse_take(tctx, ectx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 103);
                }
            } else {
                lkit_expr_t **expr, **oexpr;

                //TRACE("parse lkit expr");
                if ((expr = array_incr(&ectx->subs)) == NULL) {
                    FAIL("array_incr");
                }
                if ((*expr = lkit_expr_parse(&tctx->mctx,
                                             ectx,
                                             dat,
                                             1)) == NULL) {
                    dat->error = 1;
                    TRRET(TESTRT_PARSE + 104);
                }

                if (remove_undef(tctx, *expr) != 0) {
                    TRRET(TESTRT_PARSE + 105);
                }

                if ((oexpr = array_incr(&trt->otherexpr)) == NULL) {
                    TRRET(TESTRT_PARSE + 106);
                }
                *oexpr = *expr;
            }
        }
        break;

    default:
        TRRET(TESTRT_PARSE + 107);
    }

    return 0;
}


static int
_parse_trt(testrt_ctx_t *tctx, array_t *form, array_iter_t *it)
{
    fparser_datum_t **node = NULL;
    UNUSED lkit_gitem_t **gitem;
    testrt_t *trt;

    if ((trt = array_incr(&tctx->testrts)) == NULL) {
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

    if (trt->name == NULL) {
        TRRET(TESTRT_PARSE + 3);
    }

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        if (_parse_clause(tctx, &tctx->root, trt, *node, 1) != 0) {
            TRRET(TESTRT_PARSE + 4);
        }
    }

    return 0;
}


static int
_parse_expr(testrt_ctx_t *tctx,
            const char *name,
            array_t *form,
            array_iter_t *it)
{
    if (strcmp(name, "type") == 0) {
        return _parse_typedef(tctx, form, it);
    } else if (strcmp(name, "sym") == 0) {
        return _parse_builtin(tctx, form, it);
    } else if (strcmp(name, "dsource") == 0) {
        return _parse_dsource(tctx, form, it);
    } else if (strcmp(name, "testrt") == 0) {
        return _parse_trt(tctx, form, it);
    }
    return TESTRT_PARSE + 700;
}


static int
_compile_trt(testrt_t *trt, void *udata)
{
    int res = 0;
    testrt_ctx_t *tctx;
    lkit_struct_t *ts;
    lkit_expr_t **expr;
    array_iter_t it;
    LLVMModuleRef module;
    LLVMContextRef lctx;
    LLVMBuilderRef builder;
    LLVMValueRef fn, fnmain, av, arg;
    LLVMBasicBlockRef bb;
    bytes_t *name;
    char buf[1024];

    tctx = udata;
    module = tctx->mctx.module;
    lctx = LLVMGetModuleContext(module);
    builder = LLVMCreateBuilderInContext(lctx);

    /* TAKE and DO */

    /* methods */
    snprintf(buf, sizeof(buf), "%s.take", trt->name->data);
    name = bytes_new_from_str(buf);
    if (ltype_compile_methods((lkit_type_t *)trt->takeexpr->type,
                              module,
                              name) != 0) {
        res = TESTRT_COMPILE + 200;
        goto end;
    }
    bytes_decref(&name);

    if (trt->doexpr != NULL) {
        snprintf(buf, sizeof(buf), "%s.do", trt->name->data);
        name = bytes_new_from_str(buf);
        if (ltype_compile_methods((lkit_type_t *)trt->doexpr->type,
                                  module,
                                  name) != 0) {
            res = TESTRT_COMPILE + 201;
            goto end;
        }
        bytes_decref(&name);
    }

    /* run */
    snprintf(buf, sizeof(buf), "%s.run", trt->name->data);
    fn = LLVMAddFunction(module,
                        buf,
                        LLVMFunctionType(LLVMInt64TypeInContext(lctx), NULL, 0, 0));
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    bb = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR(".BB"));
    LLVMPositionBuilderAtEnd(builder, bb);

    /* take key */
    arg = LLVMConstIntToPtr(
        LLVMConstInt(LLVMInt64TypeInContext(lctx), (uintptr_t)trt, 0),
        LLVMPointerType(LLVMInt8TypeInContext(lctx), 0));

    ts = (lkit_struct_t *)trt->takeexpr->type;
    av = LLVMBuildCall(builder,
                       LLVMGetNamedFunction(module,
                                            "testrt_acquire_take_key"),
                       &arg,
                       1,
                       NEWVAR("call"));
    av = LLVMBuildPointerCast(builder, av, ts->base.backend, NEWVAR("cast"));

    for (expr = array_first(&trt->takeexpr->subs, &it);
         expr != NULL;
         expr = array_next(&trt->takeexpr->subs, &it)) {

        LLVMValueRef val, gep;

        val = builtin_compile_expr(&tctx->builtin,
                                   module,
                                   builder,
                                   *expr);
        gep = LLVMBuildStructGEP(builder, av, it.iter, NEWVAR("gep"));
        LLVMBuildStore(builder, val, gep);

        if ((*expr)->type->tag == LKIT_STR) {
            LLVMBuildCall(builder,
                          LLVMGetNamedFunction(module,
                                               "mrklkit_bytes_incref"),
                          &val,
                          1,
                          NEWVAR("call"));
        }
    }

    /* do */
    if (trt->doexpr != NULL) {
        av = LLVMBuildCall(builder,
                           LLVMGetNamedFunction(module, "testrt_get_do"),
                           &arg,
                           1,
                           NEWVAR("call"));

        ts = (lkit_struct_t *)trt->doexpr->type;
        av = LLVMBuildPointerCast(builder,
                                  av,
                                  ts->base.backend,
                                  NEWVAR("cast"));

        for (expr = array_first(&trt->doexpr->subs, &it);
             expr != NULL;
             expr = array_next(&trt->doexpr->subs, &it)) {

            LLVMValueRef val, gep;

            //lkit_expr_dump(*expr);

            //TRACE("expr %s", (*expr)->name->data);

            gep = LLVMBuildStructGEP(builder, av, it.iter, NEWVAR("gep"));
            val = LLVMBuildLoad(builder, gep, NEWVAR("val"));

            if (strcmp((char *)(*expr)->name->data, "MERGE") == 0) {
                lkit_expr_t **arg;

                if ((arg = array_get(&(*expr)->subs, 0)) == NULL) {
                    res = TESTRT_COMPILE + 202;
                    goto end;
                }

                switch ((*arg)->type->tag) {
                case LKIT_INT:
                    val = LLVMBuildAdd(builder,
                                       val,
                                       builtin_compile_expr(&tctx->builtin,
                                                            module,
                                                            builder,
                                                            *arg),
                                       NEWVAR("MERGE"));
                    break;

                case LKIT_FLOAT:
                    val = LLVMBuildFAdd(builder,
                                        val,
                                        builtin_compile_expr(&tctx->builtin,
                                                             module,
                                                             builder,
                                                             *arg),
                                        NEWVAR("MERGE"));
                    break;

                default:
                    res = TESTRT_COMPILE + 203;
                    goto end;
                }
            } else if (strcmp((char *)(*expr)->name->data, "SPLIT") == 0) {
            } else {
                res = TESTRT_COMPILE + 204;
                goto end;
            }

            LLVMBuildStore(builder, val, gep);

            if ((*expr)->type->tag == LKIT_STR) {
                LLVMBuildCall(builder,
                              LLVMGetNamedFunction(module,
                                                   "mrklkit_bytes_incref"),
                              &val,
                              1,
                              NEWVAR("call"));
            }
        }

    } else {
        av = LLVMBuildCall(builder,
                           LLVMGetNamedFunction(module, "testrt_get_do_empty"),
                           &arg,
                           1,
                           NEWVAR("call"));
    }

    LLVMBuildRet(builder, LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0));
    //LLVMSetLinkage(fn, LLVMExternalLinkage);
    LLVMSetLinkage(fn, LLVMPrivateLinkage);

    /* see if we call from within start */
    fnmain = LLVMGetNamedFunction(module, TESTRT_START);
    assert(fnmain != NULL);
    bb = LLVMGetLastBasicBlock(fnmain);
    LLVMPositionBuilderAtEnd(builder, bb);


    /* SEE */
    if (trt->seeexpr != NULL) {
        lkit_expr_t **seecond;
        LLVMBasicBlockRef endblock, tblock;
        LLVMValueRef res;

        if ((seecond = array_get(&trt->seeexpr->subs, 0)) == NULL) {
            FAIL("array_get");
        }
        //lkit_expr_dump(*seecond);

        tblock = LLVMAppendBasicBlockInContext(lctx, fnmain, NEWVAR("L.if.true"));
        endblock = LLVMAppendBasicBlockInContext(lctx, fnmain, NEWVAR("L.if.end"));
        res = builtin_compile_expr(&tctx->builtin, module, builder, *seecond);
        assert(res != NULL);
        LLVMBuildCondBr(builder, res, tblock, endblock);
        LLVMPositionBuilderAtEnd(builder, tblock);
        LLVMBuildCall(builder, fn, NULL, 0, NEWVAR("call"));
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);

    } else {
        LLVMBuildCall(builder, fn, NULL, 0, NEWVAR("call"));
    }

    bb = LLVMGetLastBasicBlock(fnmain);
    LLVMPositionBuilderAtEnd(builder, bb);
    for (expr = array_first(&trt->otherexpr, &it);
         expr != NULL;
         expr = array_next(&trt->otherexpr, &it)) {
        if (builtin_compile_expr(&tctx->builtin, module, builder, *expr) == NULL) {
            res = TESTRT_COMPILE + 205;
            goto end;
        }
    }


end:
    LLVMDisposeBuilder(builder);
    return res;
}


static int
_compile(testrt_ctx_t *tctx, LLVMModuleRef module)
{
    LLVMContextRef lctx;
    LLVMBuilderRef builder;
    LLVMValueRef fn;
    LLVMBasicBlockRef bb;

    /* builtin */
    if (builtin_sym_compile(&tctx->mctx, &tctx->builtin, module) != 0) {
        TRRET(TESTRT_COMPILE + 100);
    }

    lctx = LLVMGetModuleContext(module);
    builder = LLVMCreateBuilderInContext(lctx);

    if (dsource_compile(module) != 0) {
        TRRET(TESTRT_COMPILE + 101);
    }

    fn = LLVMAddFunction(module,
                        TESTRT_START,
                        LLVMFunctionType(LLVMInt64TypeInContext(lctx), NULL, 0, 0));
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    bb = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR(".BB"));
    LLVMPositionBuilderAtEnd(builder, bb);

    /* initialize all non-lazy variables */
    builtin_call_eager_initializers(&tctx->builtin, module, builder);

    /* compile all */
    if (array_traverse(&tctx->testrts,
                       (array_traverser_t)_compile_trt,
                       tctx) != 0) {
        TRRET(TESTRT_COMPILE + 102);
    }

    bb = LLVMGetLastBasicBlock(fn);
    LLVMPositionBuilderAtEnd(builder, bb);

    /* builtin symbol postactions */
    if (builtin_sym_compile_post(&tctx->builtin, module, builder) != 0) {
        TRRET(TESTRT_COMPILE + 103);
    }

    /* return */
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0));

    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(fn, LLVMExternalLinkage);

    return 0;
}


static int
_link(testrt_ctx_t *tctx, LLVMExecutionEngineRef ee, LLVMModuleRef module)
{
    testrt_t *trt;
    array_iter_t it;

    if (dsource_link(ee, module) != 0) {
        TRRET(TESTRT_LINK + 1);
    }

    /* testrt link */
    for (trt = array_first(&tctx->testrts, &it);
         trt != NULL;
         trt = array_next(&tctx->testrts, &it)) {

        char buf[1024];
        bytes_t *name;

        snprintf(buf, sizeof(buf), "%s.take", trt->name->data);
        name = bytes_new_from_str(buf);
        if (ltype_link_methods((lkit_type_t *)trt->takeexpr->type,
                               ee,
                               module,
                               name)) {
            TRRET(TESTRT_LINK + 2);
        }
        bytes_decref(&name);

        if (trt->doexpr != NULL) {
            snprintf(buf, sizeof(buf), "%s.do", trt->name->data);
            name = bytes_new_from_str(buf);
            if (ltype_link_methods((lkit_type_t *)trt->doexpr->type,
                                   ee,
                                   module,
                                   name)) {
                TRRET(TESTRT_LINK + 3);
            }
            bytes_decref(&name);
        }
    }

    return 0;
}


/*
 *
 */
static void
testrt_target_init(testrt_target_t *tgt, uint64_t id, lkit_struct_t *type)
{
    tgt->id = id;
    tgt->type = type;
    tgt->hash = 0;
    tgt->value = mrklkit_rt_struct_new(tgt->type);
}


static testrt_target_t *
testrt_target_new(uint64_t id, lkit_struct_t *type)
{
    testrt_target_t *tgt;

    if ((tgt = malloc(sizeof(testrt_target_t))) == NULL) {
        FAIL("malloc");
    }
    testrt_target_init(tgt, id, type);
    return tgt;
}


static uint64_t
testrt_target_hash(testrt_target_t *tgt)
{
    if (tgt->hash == 0) {
        lkit_type_t **ty;
        array_iter_t it;

        for (ty = array_first(&tgt->type->fields, &it);
             ty != NULL;
             ty = array_next(&tgt->type->fields, &it)) {

            //TRACE("tag=%s rtsz=%ld iter=%u",
            //      LKIT_TAG_STR((*ty)->tag), (*ty)->rtsz, it.iter);

            switch ((*ty)->tag) {
            case LKIT_INT:
                {
                    union {
                        int64_t i;
                        unsigned char c;
                    } v;

                    v.i = mrklkit_rt_get_struct_item_int(tgt->value, it.iter);
                    tgt->hash = fasthash(tgt->hash, &v.c, (*ty)->rtsz);
                }
                break;

            case LKIT_FLOAT:
                {
                    union {
                        double d;
                        unsigned char c;
                    } v;

                    v.d = mrklkit_rt_get_struct_item_float(tgt->value, it.iter);
                    tgt->hash = fasthash(tgt->hash, &v.c, (*ty)->rtsz);
                }
                break;

            case LKIT_BOOL:
                {
                    union {
                        uint64_t i;
                        unsigned char c;
                    } v;

                    v.i = mrklkit_rt_get_struct_item_bool(tgt->value, it.iter);
                    tgt->hash = fasthash(tgt->hash, &v.c, (*ty)->rtsz);
                }
                break;

            case LKIT_STR:
                {
                    bytes_t *v;

                    v = mrklkit_rt_get_struct_item_str(tgt->value, it.iter);
                    if (v != NULL) {
                        //TRACE("v=%p", v);
                        //D8(v, sizeof(*v));
                        tgt->hash = fasthash(tgt->hash, v->data, v->sz);
                    }
                }
                break;

            default:
                FAIL("testrt_target_hash");
            }
        }
    }

    return tgt->hash;
}


static int
testrt_target_cmp(testrt_target_t *a, testrt_target_t *b)
{
    uint64_t diff;

    if (a == b) {
        return 0;
    }

    diff = a->id - b->id;
    if (diff != 0) {
        return diff;
    }

    diff = testrt_target_hash(a) - testrt_target_hash(b);

    if (diff == 0) {
        lkit_type_t **ty;
        array_iter_t it;

        for (ty = array_first(&a->type->fields, &it);
             ty != NULL;
             ty = array_next(&a->type->fields, &it)) {

            switch ((*ty)->tag) {
            case LKIT_INT:
                {
                    int64_t ai, bi;

                    ai = mrklkit_rt_get_struct_item_int(a->value, it.iter);
                    bi = mrklkit_rt_get_struct_item_int(b->value, it.iter);
                    diff = ai - bi;
                }
                break;

            case LKIT_BOOL:
                {
                    int64_t ab, bb;

                    ab = mrklkit_rt_get_struct_item_bool(a->value, it.iter);
                    bb = mrklkit_rt_get_struct_item_bool(b->value, it.iter);
                    diff = ab - bb;
                }
                break;

            case LKIT_FLOAT:
                {
                    double af, bf;

                    af = mrklkit_rt_get_struct_item_float(a->value, it.iter);
                    bf = mrklkit_rt_get_struct_item_float(b->value, it.iter);
                    diff = af > bf ? 1 : af < bf ? -1 : 0;
                }
                break;

            case LKIT_STR:
                {
                    bytes_t *as, *bs;

                    as = mrklkit_rt_get_struct_item_str(a->value, it.iter);
                    bs = mrklkit_rt_get_struct_item_str(b->value, it.iter);
                    diff = bytes_cmp(as, bs);
                }
                break;

            default:
                FAIL("testrt_target_hash");
            }

            if (diff != 0) {
                break;
            }
        }
    }

    return diff;
}


static void
testrt_target_fini(testrt_target_t *tgt)
{
    mrklkit_rt_struct_destroy(&tgt->value);
    tgt->hash = 0;
}


static void
testrt_target_fini_dict_item(testrt_target_t *k, testrt_target_t *v)
{
    if (k != NULL) {
        testrt_target_fini(k);
        free(k);
    }
    if (v != NULL) {
        testrt_target_fini(v);
        free(v);
    }
}


void *
testrt_acquire_take_key(testrt_t *trt)
{
    testrt_target_init(&trt->key,
                       trt->id,
                       (lkit_struct_t *)trt->takeexpr->type);
    return trt->key.value->fields;
}

void *
testrt_get_do(testrt_t *trt)
{
    dict_item_t *it;
    testrt_target_t *d0;

    if ((it = dict_get_item(&targets, &trt->key)) == NULL) {
        testrt_target_t *take;

        take = testrt_target_new(trt->id,
                                 (lkit_struct_t *)trt->takeexpr->type);
        d0 = testrt_target_new(trt->id, (lkit_struct_t *)trt->doexpr->type);
        mrklkit_rt_struct_shallow_copy(take->value, trt->key.value);
        dict_set_item(&targets, take, d0);
    } else {
        d0 = it->value;
    }
    testrt_target_fini(&trt->key);
    return d0->value->fields;
}

void
testrt_get_do_empty(testrt_t *trt)
{
    dict_item_t *it;
    testrt_target_t *d0;

    if ((it = dict_get_item(&targets, &trt->key)) == NULL) {
        testrt_target_t *take;

        take = testrt_target_new(trt->id,
                                 (lkit_struct_t *)trt->takeexpr->type);
        d0 = testrt_target_new(trt->id, null_struct);
        mrklkit_rt_struct_shallow_copy(take->value, trt->key.value);
        dict_set_item(&targets, take, d0);
    }
    testrt_target_fini(&trt->key);
}


int
testrt_run_once(bytestream_t *bs,
                const byterange_t *br,
                testrt_ctx_t *tctx)
{
    int res = 0;

    testrt_source = mrklkit_rt_struct_new(tctx->ds->_struct);

    dparse_struct_setup(bs, br, testrt_source);

    if (res == DPARSE_NEEDMORE) {
        goto end;

    } else if (res == DPARSE_ERRORVALUE) {
        //D32(SDATA(bs, br->start), br->end - br->start);
        goto end;

    } else {
        if (mrklkit_call_void(&tctx->mctx, TESTRT_START) != 0) {
            FAIL("mrklkit_call_void");
        }
    }

end:
    //testrt_dump_source(testrt_source);
    mrklkit_rt_struct_destroy(&testrt_source);
    return res;
}


static int
dump_tgt(testrt_target_t *k, testrt_target_t *v, UNUSED void *udata)
{
    //TRACE("%p:%p", k, v);
    //D8(k, sizeof(*k));
    //D8(v, sizeof(*v));
    //lkit_type_dump((lkit_type_t *)k->type);
    //lkit_type_dump((lkit_type_t *)v->type);
    mrklkit_rt_struct_dump(k->value);
    mrklkit_rt_struct_dump(v->value);
    TRACEC("\n");
    return 0;
}


UNUSED static int
dump_trt(testrt_t *trt, UNUSED void *udata)
{
    TRACE("%s:", trt->name->data);
    return 0;
}


void
testrt_dump_targets(void)
{
    //array_traverse(&tctx->testrts, (array_traverser_t)dump_trt, NULL);
    dict_traverse(&targets, (dict_traverser_t)dump_tgt, NULL);
}


void
testrt_dump_source(rt_struct_t *source)
{
    dparse_rt_struct_dump(source);
    TRACEC("\n");
}

static void
_init(testrt_ctx_t *tctx)
{
    size_t i;

    for (i = 0; i < countof(const_bytes); ++i) {
        *const_bytes[i].var = bytes_new_from_str(const_bytes[i].val);
        BYTES_INCREF(*const_bytes[i].var);
    }

    null_struct = (lkit_struct_t *)lkit_type_finalize(&tctx->mctx,
            lkit_type_get(&tctx->mctx, LKIT_STRUCT));

    if (array_init(&dsources, sizeof(dsource_t *), 0,
                   NULL,
                   (array_finalizer_t)dsource_destroy) != 0) {
        FAIL("array_init");
    }

    lkit_expr_init(&tctx->builtin, NULL);
    lkit_expr_init(&tctx->root, &tctx->builtin);
    array_init(&tctx->testrts, sizeof(testrt_t), 0,
               (array_initializer_t)testrt_init,
               (array_finalizer_t)testrt_fini);
    dict_init(&targets, 101,
              (dict_hashfn_t)testrt_target_hash,
              (dict_item_comparator_t)testrt_target_cmp,
              (dict_item_finalizer_t)testrt_target_fini_dict_item);
}


static void
_fini(testrt_ctx_t *tctx)
{
    size_t i;
    array_fini(&tctx->testrts);
    lkit_expr_fini(&tctx->root);
    lkit_expr_fini(&tctx->builtin);
    array_fini(&dsources);

    for (i = 0; i < countof(const_bytes); ++i) {
        bytes_decref(const_bytes[i].var);
    }
}

mrklkit_module_t testrt_module = {
    (mrklkit_module_initializer_t)_init,
    (mrklkit_module_finalizer_t)_fini,
    (mrklkit_expr_parser_t)_parse_expr,
    NULL,
    (mrklkit_type_compiler_t)_compile_type,
    (mrklkit_module_compiler_t)_compile,
    (mrklkit_module_linker_t)_link,
};
