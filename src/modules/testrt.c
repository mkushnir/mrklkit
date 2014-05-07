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
#include <mrklkit/lparse.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/builtin.h>
#include <mrklkit/util.h>

#include <mrklkit/modules/testrt.h>
#include <mrklkit/modules/dparser.h>
#include <mrklkit/modules/dsource.h>

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
static dict_t targets;
rt_struct_t *testrt_source = NULL;
/* unit context */
static lkit_expr_t root;
static array_t testrts;


int
remove_undef(lkit_expr_t *expr)
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

            if (builtin_remove_undef(*arg) != 0) {
                TRRET(TESTRT_PARSE + 500);
            }

            expr->type = (*arg)->type;

        } else if (strcmp(name, "HANGOVER") == 0) {
        } else {
            return builtin_remove_undef(expr);
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
_parse_take(lkit_expr_t *ctx,
            testrt_t *trt,
            array_t *form,
            array_iter_t *it,
            int seterror)
{
    fparser_datum_t **node;
    lkit_struct_t *ts;

    if (trt->takeexpr != NULL) {
        TRRET(TESTRT_PARSE + 300);
    }

    trt->takeexpr = lkit_expr_new(&root);
    ts = (lkit_struct_t *)lkit_type_get(LKIT_STRUCT);

    for (node = array_next(form, it);
         node != NULL;
         node = array_next(form, it)) {

        lkit_expr_t **expr;
        lkit_type_t **ty;

        //if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
        //    TRRET(TESTRT_PARSE + 400);
        //}

        if ((expr = array_incr(&trt->takeexpr->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*expr = lkit_expr_parse(ctx, *node, seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 401);
        }

        if (remove_undef(*expr) != 0) {
            TRRET(TESTRT_PARSE + 209);
        }

        if ((ty = array_incr(&ts->fields)) == NULL) {
            FAIL("array_incr");
        }
        *ty = (*expr)->type;
    }

    trt->takeexpr->type = lkit_type_finalize((lkit_type_t *)ts);

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
    lkit_struct_t *ts;

    if (trt->doexpr != NULL) {
        TRRET(TESTRT_PARSE + 300);
    }

    trt->doexpr = lkit_expr_new(&root);

    ts = (lkit_struct_t *)lkit_type_get(LKIT_STRUCT);

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

        if ((*expr = lkit_expr_parse(ctx, *node, seterror)) == NULL) {
            TRRET(TESTRT_PARSE + 302);
        }

        if (remove_undef(*expr) != 0) {
            TRRET(TESTRT_PARSE + 209);
        }

        if ((ty = array_incr(&ts->fields)) == NULL) {
            FAIL("array_incr");
        }
        *ty = (*expr)->type;
    }

    trt->doexpr->type = lkit_type_finalize((lkit_type_t *)ts);

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
        ifexpr->isref = 1;

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
        andexpr->isref = 1;
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
        texpr->isref = 1;

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
        fexpr->isref = 1;

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
    //lkit_expr_dump(ifexpr);
    if (remove_undef(ifexpr) != 0) {
        TRRET(TESTRT_PARSE + 209);
    }
    //lkit_expr_dump(ifexpr);

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

            //TRACE("clause name %s", cname->data);

            if (strcmp((char *)cname->data, "SEE") == 0) {
                //TRACE("parse see");
                if (form->elnum > 1) {
                    if (_parse_see(ctx, trt, form, &it, seterror) != 0) {
                        TRRET(TESTRT_PARSE + 101);
                    }
                }
            } else if (strcmp((char *)cname->data, "DO") == 0) {
                //TRACE("parse do");
                if (_parse_do(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else if (strcmp((char *)cname->data, "TAKE") == 0) {
                //TRACE("parse take");
                if (_parse_take(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 103);
                }
            } else {
                lkit_expr_t **expr, **oexpr;

                //TRACE("parse lkit expr");
                if ((expr = array_incr(&ctx->subs)) == NULL) {
                    FAIL("array_incr");
                }
                if ((*expr = lkit_expr_parse(ctx, dat, 1)) == NULL) {
                    dat->error = 1;
                    TRRET(TESTRT_PARSE + 104);
                }

                if (remove_undef(*expr) != 0) {
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

    if ((trt = array_incr(&testrts)) == NULL) {
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

        if (_parse_clause(&root, trt, *node, 1) != 0) {
            TRRET(TESTRT_PARSE + 4);
        }
    }

    return 0;
}

static int
_compile_trt(testrt_t *trt, void *udata)
{
    int res = 0;
    lkit_struct_t *ts;
    lkit_expr_t **expr;
    array_iter_t it;
    LLVMModuleRef module = (LLVMModuleRef)udata;
    LLVMBuilderRef builder;
    LLVMValueRef fn, fnmain, av, arg;
    LLVMBasicBlockRef bb;
    bytes_t *name;
    char buf[1024];

    builder = LLVMCreateBuilder();

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
            res = TESTRT_COMPILE + 200;
            goto end;
        }
        bytes_decref(&name);
    }

    /* run */
    snprintf(buf, sizeof(buf), "%s.run", trt->name->data);
    fn = LLVMAddFunction(module,
                        buf,
                        LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    bb = LLVMAppendBasicBlock(fn, NEWVAR(".BB"));
    LLVMPositionBuilderAtEnd(builder, bb);

    /* take key */
    arg = LLVMConstIntToPtr(
        LLVMConstInt(LLVMInt64Type(), (uintptr_t)trt, 0),
        LLVMPointerType(LLVMVoidType(), 0));

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

        val = builtin_compile_expr(module, builder, *expr);
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
                    res = TESTRT_COMPILE + 201;
                    goto end;
                }

                switch ((*arg)->type->tag) {
                case LKIT_INT:
                    val = LLVMBuildAdd(builder,
                                       val,
                                       builtin_compile_expr(module,
                                                            builder,
                                                            *arg),
                                       NEWVAR("MERGE"));
                    break;

                case LKIT_FLOAT:
                    val = LLVMBuildFAdd(builder,
                                        val,
                                        builtin_compile_expr(module,
                                                             builder,
                                                             *arg),
                                        NEWVAR("MERGE"));
                    break;

                default:
                    res = TESTRT_COMPILE + 202;
                    goto end;
                }
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

    LLVMBuildRetVoid(builder);
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

        tblock = LLVMAppendBasicBlock(fnmain, NEWVAR("L.if.true"));
        endblock = LLVMAppendBasicBlock(fnmain, NEWVAR("L.if.end"));
        res = builtin_compile_expr(module, builder, *seecond);
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
        if (builtin_compile_expr(module, builder, *expr) == NULL) {
            res = TESTRT_COMPILE + 203;
            goto end;
        }
    }


end:
    LLVMDisposeBuilder(builder);
    return res;
}


static int
_compile(LLVMModuleRef module)
{
    LLVMBuilderRef builder;
    LLVMValueRef fn;
    LLVMBasicBlockRef bb;

    builder = LLVMCreateBuilder();

    if (dsource_compile(module) != 0) {
        TRRET(TESTRT_COMPILE + 100);
    }

    fn = LLVMAddFunction(module,
                        TESTRT_START,
                        LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    bb = LLVMAppendBasicBlock(fn, NEWVAR(".BB"));
    LLVMPositionBuilderAtEnd(builder, bb);

    /* initialize all non-lazy variables */
    builtin_call_eager_initializers(module, builder);

    /* compile all */
    if (array_traverse(&testrts,
                       (array_traverser_t)_compile_trt,
                       module) != 0) {
        TRRET(TESTRT_COMPILE + 101);
    }

    /* return */
    bb = LLVMGetLastBasicBlock(fn);
    LLVMPositionBuilderAtEnd(builder, bb);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(fn, LLVMExternalLinkage);

    return 0;
}


static int
_link(LLVMExecutionEngineRef ee, LLVMModuleRef module)
{
    testrt_t *trt;
    array_iter_t it;

    if (dsource_link(ee, module) != 0) {
        TRRET(TESTRT_LINK + 1);
    }

    /* testrt link */
    for (trt = array_first(&testrts, &it);
         trt != NULL;
         trt = array_next(&testrts, &it)) {

        char buf[1024];
        bytes_t *name;

        snprintf(buf, sizeof(buf), "%s.take", trt->name->data);
        name = bytes_new_from_str(buf);
        if (ltype_link_methods((lkit_type_t *)trt->takeexpr->type, ee, module, name)) {
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

            //TRACE("tag=%s rtsz=%ld iter=%u", LKIT_TAG_STR((*ty)->tag), (*ty)->rtsz, it.iter);

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
    testrt_target_t *d0;

    if ((d0 = dict_get_item(&targets, &trt->key)) == NULL) {
        testrt_target_t *take;

        take = testrt_target_new(trt->id,
                                 (lkit_struct_t *)trt->takeexpr->type);
        d0 = testrt_target_new(trt->id, (lkit_struct_t *)trt->doexpr->type);
        mrklkit_rt_struct_shallow_copy(take->value, trt->key.value);
        dict_set_item(&targets, take, d0);
    }
    testrt_target_fini(&trt->key);
    return d0->value->fields;
}

void
testrt_get_do_empty(testrt_t *trt)
{
    testrt_target_t *d0;

    if ((d0 = dict_get_item(&targets, &trt->key)) == NULL) {
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
testrt_run(bytestream_t *bs, dsource_t *ds)
{
    int res = 0;
    char enddelim = '\0';

    testrt_source = mrklkit_rt_struct_new(ds->_struct);

    /*
     * This should become a call into an llvm-generated parser with only
     * bs and test_source params.
     */
    if ((res = dparse_struct(bs,
                             ds->fdelim,
                             0x7fffffffffffffff,
                             testrt_source,
                             &enddelim,
                             ds->parse_flags)) == DPARSE_NEEDMORE) {
        goto end;

    } else if (res == DPARSE_ERRORVALUE) {
        TRACE("error, delim='%02hhx'", enddelim);
        D32(SPDATA(bs), SEOD(bs) - SPOS(bs));
        goto end;

    } else {
        if (enddelim != ds->rdelim[0] ||
            enddelim == ds->rdelim[1]) {

            /* truncated input? */
            TRACE("TRUNC, delim=%02hhx", enddelim);
            res = DPARSE_ERRORVALUE;
            goto end;
        }

        if (enddelim == ds->rdelim[0]) {
            TRACE("EOL");
        }

        TRACE("ok, delim=%02hhx:", enddelim);
        //testrt_dump_source(testrt_source);

        //if (mrklkit_call_void(TESTRT_START) != 0) {
        //    FAIL("mrklkit_call_void");
        //}

        if (ds->parse_flags & DPARSE_MERGEDELIM) {
        } else {
            SINCR(bs);
        }
    }

end:
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
    //array_traverse(&testrts, (array_traverser_t)dump_trt, NULL);
    dict_traverse(&targets, (dict_traverser_t)dump_tgt, NULL);
}


void
testrt_dump_source(rt_struct_t *source)
{
    mrklkit_rt_struct_dump(source);
    TRACEC("\n");
}

static void
_init(void)
{
    size_t i;

    for (i = 0; i < countof(const_bytes); ++i) {
        *const_bytes[i].var = bytes_new_from_str(const_bytes[i].val);
        BYTES_INCREF(*const_bytes[i].var);
    }

    null_struct = (lkit_struct_t *)lkit_type_finalize(
            lkit_type_get(LKIT_STRUCT));

    dsource_init_module();

    lkit_expr_init(&root, builtin_get_root_ctx());

    array_init(&testrts, sizeof(testrt_t), 0,
               (array_initializer_t)testrt_init,
               (array_finalizer_t)testrt_fini);

    dict_init(&targets, 101,
              (dict_hashfn_t)testrt_target_hash,
              (dict_item_comparator_t)testrt_target_cmp,
              (dict_item_finalizer_t)testrt_target_fini_dict_item);
}


static void
_fini(void)
{
    size_t i;
    array_fini(&testrts);
    lkit_expr_fini(&root);
    dsource_fini_module();

    for (i = 0; i < countof(const_bytes); ++i) {
        bytes_decref(const_bytes[i].var);
    }
}

static mrklkit_parser_info_t _parsers[] = {
    {"dsource", dsource_parse},
    {"testrt", _parse_trt},
    {NULL, NULL},
};


mrklkit_module_t testrt_module = {
    _init,
    _fini,
    _parsers,
    NULL,
    _compile,
    _link,
};
