#include <stdio.h>

#include <llvm-c/Core.h>

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


static lkit_expr_t root;
static array_t testrts;


rt_struct_t *testrt_source = NULL;
UNUSED static rt_struct_t *testrt_target = NULL;
static dict_t targets;

static void
testrt_target_init(testrt_target_t *tg, uint64_t id, lkit_struct_t *kty, lkit_struct_t *vty)
{
    tg->id = id;
    tg->kty = kty;
    tg->vty = vty;
    tg->hash = 0;
}


UNUSED static testrt_target_t *
testrt_target_new(uint64_t id, lkit_struct_t *kty, lkit_struct_t *vty)
{
    testrt_target_t *tg;

    if ((tg = malloc(sizeof(testrt_target_t) +
                     kty->base.rtsz +
                     (vty != NULL ? vty->base.rtsz : 0))) == NULL) {
        FAIL("malloc");
    }
    testrt_target_init(tg, id, kty, vty);
    /* the data follow */
    tg->value = (void **)tg->data;
    return tg;
}


static uint64_t
testrt_target_hash(testrt_target_t *tg)
{
    if (tg->hash == 0) {
        lkit_type_t **ty;
        array_iter_t it;

        lkit_type_dump((lkit_type_t *)tg->kty);

        for (ty = array_first(&tg->kty->fields, &it);
             ty != NULL;
             ty = array_next(&tg->kty->fields, &it)) {

            TRACE("tag=%s rtsz=%ld iter=%u", LKIT_TAG_STR((*ty)->tag), (*ty)->rtsz, it.iter);

            switch ((*ty)->tag) {
            case LKIT_INT:
            case LKIT_FLOAT:
            case LKIT_BOOL:
                {
                    unsigned char *v;

                    v = (unsigned char *)(tg->value + it.iter);
                    TRACE("%p/%p", tg->value, tg->value + it.iter);
                    D8(v, (*ty)->rtsz);
                    tg->hash = fasthash(tg->hash, v, (*ty)->rtsz);
                }
                break;

            case LKIT_STR:
                {
                    bytes_t *bytes;

                    bytes = (bytes_t *)(tg->value + it.iter);
                    TRACE("%p/%p", tg->value, tg->value + it.iter);
                    D8(bytes, sizeof(*bytes));
                    tg->hash = fasthash(tg->hash, bytes->data, bytes->sz);
                }
                break;

            default:
                FAIL("testrt_target_hash");
            }
        }
    }

    return tg->hash;
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

        for (ty = array_first(&a->kty->fields, &it);
             ty != NULL;
             ty = array_next(&a->kty->fields, &it)) {

            switch ((*ty)->tag) {
            case LKIT_INT:
            case LKIT_FLOAT:
            case LKIT_BOOL:
                {
                    int64_t ai, bi;

                    ai = (int64_t)(a->value + it.iter);
                    bi = (int64_t)(b->value + it.iter);
                    diff = ai - bi;
                }
                break;

            case LKIT_STR:
                {
                    bytes_t *ab, *bb;

                    ab = (bytes_t *)*(a->value + it.iter);
                    bb = (bytes_t *)*(b->value + it.iter);
                    diff = bytes_hash(ab) - bytes_hash(bb);
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
testrt_target_fini(testrt_target_t *k, UNUSED testrt_target_t *v)
{
    if (k != NULL) {
        free(k);
    }
}


void *
testrt_get_target(testrt_t *trt, void *key)
{
    testrt_target_t tg, *probe;

    testrt_target_init(&tg, trt->id, (lkit_struct_t *)trt->takeexpr->type, NULL);
    tg.value = key;
    TRACE("key=%p", key);
    if ((probe = dict_get_item(&targets, &tg)) == NULL) {
        probe = testrt_target_new(trt->id,
                                  (lkit_struct_t *)trt->takeexpr->type,
                                  (lkit_struct_t *)trt->doexpr->type);
    }
    return probe;
    //return testrt_target->fields->data;
}


int
testrt_run(bytestream_t *bs, dsource_t *ds)
{
    int res = 0;
    char enddelim = '\0';

    // XXX init, fini!
    testrt_source = mrklkit_rt_struct_new(ds->_struct, NULL, NULL);

    if ((res = dparse_struct(bs,
                             ds->fdelim,
                             ds->rdelim,
                             ds->_struct,
                             testrt_source,
                             &enddelim,
                             ds->parse_flags)) == DPARSE_NEEDMORE) {
        goto end;

    } else if (res == DPARSE_ERRORVALUE) {
        TRACE("error, delim='%c'", enddelim);
        array_traverse(&testrt_source->fields,
                       (array_traverser_t)tobj_dump,
                       NULL);
        goto end;

    } else {
        if (enddelim != '\n') {
            /* truncated input? */
            res = 1;
            goto end;
        }

        if (enddelim == '\n') {
            TRACE("EOL");
        }

        TRACE("ok, delim='%c':", enddelim);
        array_traverse(&testrt_source->fields,
                       (array_traverser_t)tobj_dump,
                       NULL);

        if (mrklkit_call_void(TESTRT_START) != 0) {
            FAIL("mrklkit_call_void");
        }
    }

end:
    mrklkit_rt_struct_destroy(&testrt_source);
    return res;
}


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
        if (lparse_next_int(form, it, (int64_t *)&trt->id, 1) != 0) {
            return 1;
        }
        trt->name = bytes_new(64);
        snprintf((char *)trt->name->data, trt->name->sz, "testrt.%016lx", trt->id);
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
    trt->type = (lkit_struct_t *)lkit_type_get(LKIT_STRUCT);
    return 0;
}


static int
testrt_fini(testrt_t *trt)
{
    (void)lkit_expr_destroy(&trt->seeexpr);
    (void)lkit_expr_destroy(&trt->doexpr);
    (void)lkit_expr_destroy(&trt->takeexpr);
    (void)lkit_type_destroy((lkit_type_t **)(&trt->type));
    bytes_destroy(&trt->name);
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

        if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
            TRRET(TESTRT_PARSE + 400);
        }

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
                if (_parse_see(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 101);
                }
            } else if (strcmp((char *)cname->data, "DO") == 0) {
                //TRACE("parse do");
                if (_parse_do(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else if (strcmp((char *)cname->data, "TAKE") == 0) {
                //TRACE("parse take");
                if (_parse_take(ctx, trt, form, &it, seterror) != 0) {
                    TRRET(TESTRT_PARSE + 102);
                }
            } else {
                lkit_expr_t **expr;

                //TRACE("parse lkit expr");
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
_precompile_trt(testrt_t *trt)
{
    lkit_struct_t *ts;
    array_iter_t it;
    lkit_type_t **ty;

    /* finalize trt->type */

    ts = (lkit_struct_t *)trt->takeexpr->type;
    for (ty = array_first(&ts->fields, &it);
         ty != NULL;
         ty = array_next(&ts->fields, &it)) {

        lkit_type_t **fty;

        if ((fty = array_incr(&trt->type->fields)) == NULL) {
            FAIL("array_incr");
        }
        *fty = *ty;
    }

    ts = (lkit_struct_t *)trt->doexpr->type;
    for (ty = array_first(&ts->fields, &it);
         ty != NULL;
         ty = array_next(&ts->fields, &it)) {

        lkit_type_t **fty;

        if ((fty = array_incr(&trt->type->fields)) == NULL) {
            FAIL("array_incr");
        }
        *fty = *ty;
    }

    return 0;
}

static int
_precompile(void)
{
    array_iter_t it;
    testrt_t *trt;

    for (trt = array_first(&testrts, &it);
         trt != NULL;
         trt = array_next(&testrts, &it)) {
        if (_precompile_trt(trt) != 0) {
            return 1;
        }
    }
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
    return builtin_compile(gitem, udata);
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
    LLVMValueRef fn, fnmain, av, args[2], target, ctarget;
    LLVMBasicBlockRef bb;

    char buf[1024];

    builder = LLVMCreateBuilder();

    /* TAKE and DO */
    snprintf(buf, sizeof(buf), "%s.run", trt->name->data);
    fn = LLVMAddFunction(module,
                        buf,
                        LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    bb = LLVMAppendBasicBlock(fn, NEWVAR(".BB"));
    LLVMPositionBuilderAtEnd(builder, bb);

    /* ... */
    if (ltype_compile((lkit_type_t *)trt->type, NULL) != 0) {
        res = TESTRT_COMPILE + 200;
        goto end;
    }


    /* key */
    ts = (lkit_struct_t *)trt->takeexpr->type;
    av = LLVMBuildAlloca(builder,
                         ts->deref_backend,
                         NEWVAR("alloca"));
    for (expr = array_first(&trt->takeexpr->subs, &it);
         expr != NULL;
         expr = array_next(&trt->takeexpr->subs, &it)) {
        LLVMValueRef res, gep;

        res = builtin_compile_expr(module, builder, *expr);
        gep = LLVMBuildStructGEP(builder, av, it.iter, NEWVAR("gep"));
        LLVMBuildStore(builder, res, gep);
    }

    /* backend *ctarget = (backend *)testrt_get_target(trt, key) */
    args[0] = LLVMConstIntToPtr(
        LLVMConstInt(LLVMInt64Type(),
            (uintptr_t)trt, 0),
        LLVMPointerType(LLVMVoidType(), 0));
    args[1] = av;

    target = LLVMBuildCall(builder,
                           LLVMGetNamedFunction(module, "testrt_get_target"),
                           args, 2, NEWVAR("call"));

    ctarget = LLVMBuildPointerCast(builder,
                                   target,
                                   trt->type->base.backend,
                                   NEWVAR("cast"));
    //LLVMBuildLoad(builder, ctarget, NEWVAR("tmp"));
    //LLVMBuildStore(builder, ctarget, NEWVAR("tmp"));

    LLVMBuildRetVoid(builder);
    LLVMSetLinkage(fn, LLVMExternalLinkage);

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

    builtin_call_eager_initializers(module, builder);

    if (array_traverse(&testrts,
                       (array_traverser_t)_compile_trt,
                       module) != 0) {
        TRRET(TESTRT_COMPILE + 101);
    }

    bb = LLVMGetLastBasicBlock(fn);
    LLVMPositionBuilderAtEnd(builder, bb);
    LLVMBuildRetVoid(builder);

    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(fn, LLVMExternalLinkage);

    return 0;
}


static void
_init(void)
{
    size_t i;

    for (i = 0; i < countof(const_bytes); ++i) {
        *const_bytes[i].var = bytes_new(strlen(const_bytes[i].val) + 1);
        memcpy((*const_bytes[i].var)->data, const_bytes[i].val, (*const_bytes[i].var)->sz);
    }

    dsource_init_module();

    lkit_expr_init(&root, builtin_get_root_ctx());

    array_init(&testrts, sizeof(testrt_t), 0,
               (array_initializer_t)testrt_init,
               (array_finalizer_t)testrt_fini);

    dict_init(&targets, 101,
              (dict_hashfn_t)testrt_target_hash,
              (dict_item_comparator_t)testrt_target_cmp,
              (dict_item_finalizer_t)testrt_target_fini);
}


static void
_fini(void)
{
    size_t i;
    array_fini(&testrts);
    lkit_expr_fini(&root);
    dsource_fini_module();

    for (i = 0; i < countof(const_bytes); ++i) {
        bytes_destroy(const_bytes[i].var);
    }
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
    _precompile,
    _compile,
};
