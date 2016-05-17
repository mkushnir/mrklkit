#include <assert.h>
#include <stdint.h>

#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/dpexpr.h>
#include <mrklkit/builtin.h>
#include <mrklkit/module.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(builtingen);
#endif


static LLVMValueRef
find_named_global(lkit_cexpr_t *ectx,
                  LLVMModuleRef module,
                  bytes_t *name, bytes_t **qual_name)
{
    LLVMValueRef ref = NULL;

    *qual_name = NULL;
    for (; ectx != NULL; ectx = ectx->parent) {
        *qual_name = lkit_cexpr_qual_name(ectx, name);
        //TRACE("trying %s", (*qual_name)->data);
        if ((ref = LLVMGetNamedGlobal(module,
                (char *)(*qual_name)->data)) != NULL) {
            return ref;
        }
        BYTES_DECREF(qual_name);
    }
    return NULL;
}


static LLVMValueRef
find_named_function(lkit_cexpr_t *ectx,
                    LLVMModuleRef module,
                    bytes_t *name, bytes_t **qual_name)
{
    LLVMValueRef ref = NULL;

    *qual_name = NULL;
    for (; ectx != NULL; ectx = ectx->parent) {
        *qual_name = lkit_cexpr_qual_name(ectx, name);
        //TRACE("trying %s", (*qual_name)->data);
        if ((ref = LLVMGetNamedFunction(module,
                (char *)(*qual_name)->data)) != NULL) {
            return ref;
        }
        BYTES_DECREF(qual_name);
    }
    return NULL;
}



/**
 * LLVM IR emitter.
 */

void
compile_incref(LLVMModuleRef module,
               LLVMBuilderRef builder,
               lkit_expr_t *expr,
               LLVMValueRef v)
{
    UNUSED LLVMContextRef lctx;
    char *torprefix;

    if (expr->mpolicy == LKIT_MPMPOOL) {
        return;
    }

    lctx = LLVMGetModuleContext(module);

    switch (expr->type->tag) {
    case LKIT_STR:
        torprefix = "mrklkit_rt_bytes_";
        break;
    case LKIT_ARRAY:
        torprefix = "mrklkit_rt_array_";
        break;
    case LKIT_DICT:
        torprefix = "mrklkit_rt_dict_";
        break;
    case LKIT_STRUCT:
        torprefix = "mrklkit_rt_struct_";
        break;
    default:
        torprefix = NULL;
        break;
    }

    if (torprefix != NULL && expr->type->tag != LKIT_VOID) {
        char buf[1024];
        LLVMValueRef torfn;

        (void)snprintf(buf, sizeof(buf), "%sincref", torprefix);

        if ((torfn = LLVMGetNamedFunction(module, buf)) == NULL) {
            TRACE("no such function: %s", buf);
            FAIL("compile_incref");
        }

        LLVMBuildCall(builder, torfn, &v, 1, "");
    }
}


void
compile_decref(LLVMModuleRef module,
               LLVMBuilderRef builder,
               lkit_expr_t *expr,
               LLVMValueRef v)
{
    UNUSED LLVMContextRef lctx;
    char *torprefix;

    if (expr->mpolicy == LKIT_MPMPOOL) {
        return;
    }

    lctx = LLVMGetModuleContext(module);

    switch (expr->type->tag) {
    case LKIT_STR:
        torprefix = "mrklkit_rt_bytes_";
        break;
    case LKIT_ARRAY:
        torprefix = "mrklkit_rt_array_";
        break;
    case LKIT_DICT:
        torprefix = "mrklkit_rt_dict_";
        break;
    case LKIT_STRUCT:
        torprefix = "mrklkit_rt_struct_";
        break;
    default:
        torprefix = NULL;
        break;
    }

    if (torprefix != NULL && expr->type->tag != LKIT_VOID) {
        char buf[1024];
        LLVMValueRef torfn;

        (void)snprintf(buf, sizeof(buf), "%sdecref", torprefix);

        if ((torfn = LLVMGetNamedFunction(module, buf)) == NULL) {
            TRACE("no such function: %s", buf);
            FAIL("compile_decref");
        }

        v = LLVMBuildPointerCast(builder,
                                 v,
                                 LLVMTypeOf(LLVMGetParam(torfn, 0)),
                                 NEWVAR("cast"));
        LLVMBuildCall(builder, torfn, &v, 1, "");
    }
}


static LLVMValueRef
compile_if(mrklkit_ctx_t *mctx,
           lkit_cexpr_t *ectx,
           LLVMModuleRef module,
           LLVMBuilderRef builder,
           lkit_expr_t *cexpr,
           lkit_expr_t *texpr,
           lkit_expr_t *fexpr,
           lkit_type_t *restype,
           void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL, cond, texp = NULL, fexp = NULL, iexp[2];
    UNUSED LLVMValueRef res;
    LLVMBasicBlockRef currblock, nextblock, endblock, tblock, fblock, iblock[2];

    lctx = LLVMGetModuleContext(module);

    currblock = LLVMGetInsertBlock(builder);
    nextblock = LLVMGetNextBasicBlock(currblock);
    if (nextblock == NULL) {
        LLVMValueRef fn;
        fn = LLVMGetBasicBlockParent(currblock);
        endblock = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR("C.end"));
    } else {
        endblock = LLVMInsertBasicBlockInContext(lctx,
                                                 nextblock,
                                                 NEWVAR("C.end"));
    }

    tblock = LLVMInsertBasicBlockInContext(lctx, endblock, NEWVAR("C.true"));
    fblock = LLVMInsertBasicBlockInContext(lctx, endblock, NEWVAR("C.false"));

    /**/
    cond = lkit_compile_expr(mctx, ectx, module, builder, cexpr, udata);
    assert(cond != NULL);
    (void)LLVMBuildCondBr(builder, cond, tblock, fblock);

    /**/
    LLVMPositionBuilderAtEnd(builder, tblock);

    if (texpr != NULL) {
        texp = lkit_compile_expr(mctx, ectx, module, builder, texpr, udata);
        //assert(texp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    /**/
    LLVMPositionBuilderAtEnd(builder, fblock);

    if (fexpr != NULL) {
        fexp = lkit_compile_expr(mctx, ectx, module, builder, fexpr, udata);
        //assert(fexp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    LLVMPositionBuilderAtEnd(builder, endblock);
    if (restype != NULL) {
        if (restype->tag != LKIT_VOID) {
            v = LLVMBuildPhi(builder,
                             mrklkit_ctx_get_type_backend(mctx, restype),
                             NEWVAR("result"));
            iexp[0] = texp;
            iblock[0] = LLVMIsConstant(texp) ? tblock :
                        LLVMGetInstructionParent(texp);
            iexp[1] = fexp;
            iblock[1] = LLVMIsConstant(fexp) ? fblock :
                        LLVMGetInstructionParent(fexp);
            LLVMAddIncoming(v, iexp, iblock, 2);
        } else {
            nextblock = LLVMGetNextBasicBlock(endblock);
            if (nextblock == NULL) {
                LLVMValueRef fn;
                fn = LLVMGetBasicBlockParent(endblock);
                endblock = LLVMAppendBasicBlockInContext(lctx,
                                                         fn,
                                                         NEWVAR("C.end.void"));
            } else {
                endblock = LLVMInsertBasicBlockInContext(lctx,
                                                         nextblock,
                                                         NEWVAR("C.end.void"));
            }
            v = LLVMBuildBr(builder, endblock);
            LLVMPositionBuilderAtEnd(builder, endblock);
        }
    }

    return v;
}


static LLVMValueRef
_compile_cmp(mrklkit_ctx_t *mctx,
             lkit_cexpr_t *ectx,
             LLVMModuleRef module,
             LLVMBuilderRef builder,
             lkit_expr_t *a,
             lkit_expr_t *b,
             LLVMIntPredicate ip,
             LLVMRealPredicate rp,
             void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    LLVMValueRef va, vb;

    lctx = LLVMGetModuleContext(module);

    if ((va = lkit_compile_expr(mctx,
                                ectx,
                                module,
                                builder,
                                a,
                                udata)) == NULL) {
        TR(COMPILE_CMP + 1);
        goto end;
    }

    if ((vb = lkit_compile_expr(mctx,
                                ectx,
                                module,
                                builder,
                                b,
                                udata)) == NULL) {
        TR(COMPILE_CMP + 2);
        goto end;
    }

    if (a->type->tag == LKIT_INT ||
        a->type->tag == LKIT_INT_MIN ||
        a->type->tag == LKIT_INT_MAX) {
        v = LLVMBuildICmp(builder, ip, va, vb, NEWVAR("cmp"));

    } else if (a->type->tag == LKIT_BOOL) {
        LLVMTypeRef ty;

        ty = LLVMInt64TypeInContext(lctx);

        v = LLVMBuildICmp(builder,
                          ip,
                          LLVMBuildCast(builder,
                                        LLVMZExt,
                                        va,
                                        ty,
                                        NEWVAR("cast")),
                          LLVMBuildCast(builder,
                                        LLVMZExt,
                                        vb,
                                        ty,
                                        NEWVAR("cast")),
                          NEWVAR("cmp"));

    } else if (a->type->tag == LKIT_FLOAT ||
               a->type->tag == LKIT_FLOAT_MIN ||
               a->type->tag == LKIT_FLOAT_MAX) {

        v = LLVMBuildFCmp(builder, rp, va, vb, NEWVAR("cmp"));

    } else if (a->type->tag == LKIT_STR) {
        LLVMValueRef fn, args[2], rv;

        if ((fn = LLVMGetNamedFunction(module, "bytes_cmpv")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        COMPILE_INCREF_ZREF(module, builder, a, va);
        COMPILE_INCREF_ZREF(module, builder, b, vb);

        args[0] = va;
        args[1] = vb;
        LLVMTypeRef ty;

        ty = LLVMInt8TypeInContext(lctx);
        rv = LLVMBuildCall(builder,
                          fn,
                          args,
                          2,
                          NEWVAR("call"));
        rv = LLVMBuildCast(builder, LLVMTrunc, rv, ty, NEWVAR("cast"));
        v = LLVMBuildICmp(builder,
                          ip,
                          rv,
                          LLVMConstInt(ty, 0, 0),
                          NEWVAR("cmp"));

        COMPILE_DECREF_ZREF(module, builder, a, va);
        COMPILE_DECREF_ZREF(module, builder, b, vb);

    } else {
        TRACE("a:");
        lkit_expr_dump(a);
        TRACE("b:");
        lkit_expr_dump(b);
        TR(COMPILE_CMP + 4);
        goto end;
    }

end:
    return v;
}


static LLVMValueRef
compile_cmp(mrklkit_ctx_t *mctx,
            lkit_cexpr_t *ectx,
            LLVMModuleRef module,
            LLVMBuilderRef builder,
            lkit_expr_t *expr,
            LLVMIntPredicate ip,
            LLVMRealPredicate rp,
            void *udata)
{
    lkit_expr_t **a, **b;
    array_iter_t it;

    a = array_first(&expr->subs, &it);
    b = array_next(&expr->subs, &it);
    return _compile_cmp(mctx, ectx, module, builder, *a, *b, ip, rp, udata);
}


LLVMValueRef
lkit_compile_get(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    lkit_type_t *ty, *cty;
    lkit_expr_t **key, **cont, **dflt;
    LLVMValueRef fn, args[3];
    char *name = (char *)expr->name->data;

    lctx = LLVMGetModuleContext(module);

    ty = expr->type;

    /* container */
    if ((cont = array_get(&expr->subs, 0)) == NULL) {
        FAIL("array_get");
    }

    if ((args[0] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *cont,
                                     udata)) == NULL) {
        TR(COMPILE_GET + 1);
        goto err;
    }

    //compile_incref(module, builder, *cont, args[0]);

    if ((dflt = array_get(&expr->subs, 2)) == NULL) {
        args[2] = LLVMConstPointerNull(
                LLVMPointerType(LLVMInt8TypeInContext(lctx), 0));
    } else {
        assert(lkit_type_cmp(ty, (*dflt)->type) == 0);

        if (LKIT_TAG_POINTER((*dflt)->type->tag)) {
            args[2] = LLVMConstNull(mrklkit_ctx_get_type_backend(
                        mctx, (*dflt)->type));
        } else {
            args[2] = lkit_compile_expr(mctx,
                                        ectx,
                                        module,
                                        builder,
                                        *dflt,
                                        udata);
        }
    }

    cty = (*cont)->type;

    if (cty->tag == LKIT_PARSER) {
        cty = LKIT_PARSER_GET_TYPE(cty);
    }

    switch (cty->tag) {
    case LKIT_ARRAY:
        /* idx */
        {
            char buf[64];
            lkit_type_t *fty;

            if ((key = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *key,
                                             udata)) == NULL) {
                TR(COMPILE_GET + 100);
                goto err;
            }

            if ((fty = lkit_array_get_element_type(
                        (lkit_array_t *)cty)) == NULL) {
                TR(COMPILE_GET + 102);
                goto err;
            }

            if (fty->tag == LKIT_PARSER) {
                fty = LKIT_PARSER_GET_TYPE(fty);
            }

            assert(lkit_type_cmp(ty, fty) == 0);

            (void)snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_array_get_item_%s",
                     fty->name);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("cannot find %s", buf);
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));

            args[2] = LLVMBuildPointerCast(builder,
                                           args[2],
                                           LLVMTypeOf(LLVMGetParam(fn, 2)),
                                           NEWVAR("cast"));

            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR(name));

            if (LKIT_TAG_POINTER(fty->tag)) {
                v = LLVMBuildPointerCast(builder,
                                         v,
                                         mrklkit_ctx_get_type_backend(mctx,
                                                                      fty),
                                         NEWVAR("cast"));
            }
        }
        break;

    case LKIT_DICT:
        /* key */
        {
            char buf[64];
            lkit_type_t *fty;

            if ((key = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }

            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *key,
                                             udata)) == NULL) {
                TR(COMPILE_GET + 200);
                goto err;
            }

            if ((fty = lkit_dict_get_element_type(
                        (lkit_dict_t *)cty)) == NULL) {
                TR(COMPILE_GET + 201);
                goto err;
            }

            if (fty->tag == LKIT_PARSER) {
                fty = LKIT_PARSER_GET_TYPE(fty);
            }

            assert(lkit_type_cmp(ty, fty) == 0);

            (void)snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_dict_get_item_%s",
                     fty->name);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("cannot find %s", buf);
                FAIL("LLVMGetNamedFunction");
            }

            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));

            args[2] = LLVMBuildPointerCast(builder,
                                           args[2],
                                           LLVMTypeOf(LLVMGetParam(fn, 2)),
                                           NEWVAR("cast"));

            COMPILE_INCREF_ZREF(module, builder, *key, args[1]);

            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR(name));

            COMPILE_DECREF_ZREF(module, builder, *key, args[1]);

            if (LKIT_TAG_POINTER(fty->tag)) {
                v = LLVMBuildPointerCast(builder,
                                         v,
                                         mrklkit_ctx_get_type_backend(mctx,
                                                                      fty),
                                         NEWVAR("cast"));
            }
        }
        break;

    case LKIT_STRUCT:
        /* idx */
        {
            int idx;
            lkit_type_t *fty;
            char buf[64];

            if ((key = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            assert((*key)->type->tag == LKIT_STR && LKIT_EXPR_CONSTANT(*key));

            if ((idx = lkit_struct_get_field_index(
                        (lkit_struct_t *)cty,
                        (bytes_t *)(*key)->value.literal->body)) == -1) {
                TR(COMPILE_GET + 300);
                goto err;
            }

            if ((fty = lkit_struct_get_field_type(
                        (lkit_struct_t *)cty,
                        (bytes_t *)(*key)->value.literal->body)) == NULL) {
                TR(COMPILE_GET + 301);
                goto err;
            }

            if (fty->tag == LKIT_PARSER) {
                fty = LKIT_PARSER_GET_TYPE(fty);
            }

            assert(lkit_type_cmp(ty, fty) == 0);

            (void)snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_struct_get_item_%s",
                     fty->name);

            args[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx), idx, 1);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("cannot find %s", buf);
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));

            args[2] = LLVMBuildPointerCast(builder,
                                           args[2],
                                           LLVMTypeOf(LLVMGetParam(fn, 2)),
                                           NEWVAR("cast"));

            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR(name));

            if (LKIT_TAG_POINTER(fty->tag)) {
                v = LLVMBuildPointerCast(builder,
                                         v,
                                         mrklkit_ctx_get_type_backend(mctx,
                                                                      fty),
                                         NEWVAR("cast"));
            }
        }
        break;

    default:
        FAIL("compile_get");
    }

    //compile_decref(module, builder, *cont, args[0]);

    /*
     *
     */
    if (dflt != NULL && LKIT_TAG_POINTER(ty->tag)) {
        if (cty->tag != LKIT_ARRAY) {
            char buf[1024];
            LLVMValueRef parent, test, setfn, phi;
            LLVMBasicBlockRef currblock, trueblock, falseblock, endblock;

            /*
             * (if (isnull v) ((set cont idx) dflt) #v)
             */
            currblock = LLVMGetInsertBlock(builder);
            parent = LLVMGetBasicBlockParent(currblock);
            endblock = LLVMAppendBasicBlockInContext(lctx,
                                                     parent,
                                                     NEWVAR("DF.end"));
            LLVMMoveBasicBlockAfter(endblock, currblock);
            falseblock = LLVMAppendBasicBlockInContext(lctx,
                                                       parent,
                                                       NEWVAR("DF.false"));
            LLVMMoveBasicBlockAfter(falseblock, currblock);
            trueblock = LLVMAppendBasicBlockInContext(lctx,
                                                      parent,
                                                      NEWVAR("DF.true"));
            LLVMMoveBasicBlockAfter(trueblock, currblock);

            test = LLVMBuildIsNull(builder, v, NEWVAR("test"));
            (void)LLVMBuildCondBr(builder, test, trueblock, falseblock);

            LLVMPositionBuilderAtEnd(builder, trueblock);
            if ((args[2] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *dflt,
                                             udata)) == NULL) {
                TR(COMPILE_GET + 400);
                goto err;
            }

            if (expr->mpolicy == LKIT_MPMPOOL) {
                (void)snprintf(buf,
                               sizeof(buf),
                               "mrklkit_rt_%s_set_item_%s_mpool",
                               cty->name,
                               (*dflt)->type->name);
            } else {
                (void)snprintf(buf,
                               sizeof(buf),
                               "mrklkit_rt_%s_set_item_%s",
                               cty->name,
                               (*dflt)->type->name);

            }
            if ((setfn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("no such setter: %s", buf);
                lkit_expr_dump(expr);
                FAIL("LLVMGetNamedFunction");
            } else {
                args[0] = LLVMBuildPointerCast(
                        builder,
                        args[0],
                        LLVMTypeOf(LLVMGetParam(setfn, 0)),
                        NEWVAR("cast"));
                args[2] = LLVMBuildPointerCast(
                        builder,
                        args[2],
                        LLVMTypeOf(LLVMGetParam(setfn, 2)),
                        NEWVAR("cast"));
                (void)LLVMBuildCall(builder, setfn, args, 3, "");
                args[2] = LLVMBuildPointerCast(builder,
                                               args[2],
                                               LLVMTypeOf(v),
                                               NEWVAR("cast"));
            }

            LLVMBuildBr(builder, endblock);

            LLVMPositionBuilderAtEnd(builder, falseblock);
            LLVMBuildBr(builder, endblock);

            LLVMPositionBuilderAtEnd(builder, endblock);
            phi = LLVMBuildPhi(builder, LLVMTypeOf(v), NEWVAR("phi"));
            LLVMAddIncoming(phi, &args[2], &trueblock, 1);
            LLVMAddIncoming(phi, &v, &falseblock, 1);
            v = phi;

        } else {
            LLVMValueRef parent, test, dfltv, phi;
            LLVMBasicBlockRef currblock, trueblock, falseblock, endblock;

            /*
             * (if (isnull v) ((set cont idx) dflt) #v)
             */
            currblock = LLVMGetInsertBlock(builder);
            parent = LLVMGetBasicBlockParent(currblock);
            endblock = LLVMAppendBasicBlockInContext(lctx,
                                                     parent,
                                                     NEWVAR("DF.end"));
            LLVMMoveBasicBlockAfter(endblock, currblock);
            falseblock = LLVMAppendBasicBlockInContext(lctx,
                                                       parent,
                                                       NEWVAR("DF.false"));
            LLVMMoveBasicBlockAfter(falseblock, currblock);
            trueblock = LLVMAppendBasicBlockInContext(lctx,
                                                      parent,
                                                      NEWVAR("DF.true"));
            LLVMMoveBasicBlockAfter(trueblock, currblock);

            test = LLVMBuildIsNull(builder, v, NEWVAR("test"));
            (void)LLVMBuildCondBr(builder, test, trueblock, falseblock);

            LLVMPositionBuilderAtEnd(builder, trueblock);
            if ((dfltv = lkit_compile_expr(mctx,
                                           ectx,
                                           module,
                                           builder,
                                           *dflt,
                                           udata)) == NULL) {
                TR(COMPILE_GET + 401);
                goto err;
            }

            LLVMBuildBr(builder, endblock);

            LLVMPositionBuilderAtEnd(builder, falseblock);
            LLVMBuildBr(builder, endblock);

            LLVMPositionBuilderAtEnd(builder, endblock);
            phi = LLVMBuildPhi(builder, LLVMTypeOf(v), NEWVAR("phi"));
            LLVMAddIncoming(phi, &dfltv, &trueblock, 1);
            LLVMAddIncoming(phi, &v, &falseblock, 1);
            v = phi;
        }
    }
end:
    return v;

err:
    //lkit_expr_dump(expr);
    v = NULL;
    goto end;
}


static LLVMValueRef
lkit_compile_set(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    lkit_expr_t **arg, **cont;
    LLVMValueRef fn, args[3];
    char *mpsuffix;

    lctx = LLVMGetModuleContext(module);

    /* container */
    if ((cont = array_get(&expr->subs, 0)) == NULL) {
        FAIL("array_get");
    }

    if ((args[0] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *cont,
                                     udata)) == NULL) {
        TR(COMPILE_SET + 1);
        goto err;
    }

    if ((arg = array_get(&expr->subs, 2)) == NULL) {
        FAIL("array_get");
    }

    if ((args[2] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *arg,
                                     udata)) == NULL) {
        TR(COMPILE_SET + 3);
        goto err;
    }

    mpsuffix = expr->mpolicy == LKIT_MPMPOOL ? "_mpool" : "";

    switch ((*cont)->type->tag) {
    case LKIT_STRUCT:
        {
            int idx;
            lkit_type_t *fty;
            char buf[64];

            if ((arg = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            assert((*arg)->type->tag == LKIT_STR && LKIT_EXPR_CONSTANT(*arg));

            if ((idx = lkit_struct_get_field_index(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == -1) {
                TR(COMPILE_SET + 100);
                goto err;
            }

            if ((fty = lkit_struct_get_field_type(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == NULL) {
                TR(COMPILE_SET + 101);
                goto err;
            }

            if (fty->tag == LKIT_PARSER) {
                fty = LKIT_PARSER_GET_TYPE(fty);
            }

            snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_struct_set_item_%s%s",
                     fty->name,
                     mpsuffix);

            args[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx), idx, 1);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("fn=%s", buf);
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));
            if (LKIT_TAG_POINTER(fty->tag)) {
                args[2] = LLVMBuildPointerCast(builder,
                                               args[2],
                                               LLVMTypeOf(LLVMGetParam(fn, 2)),
                                               NEWVAR("cat"));
            }
            v = LLVMBuildCall(builder, fn, args, countof(args), "");
        }

        break;

    case LKIT_DICT:
        {
            lkit_type_t *fty;
            char buf[64];

            if ((fty = lkit_dict_get_element_type(
                            (lkit_dict_t *)(*cont)->type)) == NULL) {
                TR(COMPILE_SET + 200);
                goto err;
            }

            if (fty->tag == LKIT_PARSER) {
                fty = LKIT_PARSER_GET_TYPE(fty);
            }

            snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_dict_set_item_%s%s",
                     fty->name,
                     mpsuffix);

            if ((arg = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            assert((*arg)->type->tag == LKIT_STR);

            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *arg,
                                             udata)) == NULL) {
                TR(COMPILE_SET + 201);
                goto err;
            }

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("fn=%s", buf);
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));
            if (LKIT_TAG_POINTER(fty->tag)) {
                args[2] = LLVMBuildPointerCast(builder,
                                               args[2],
                                               LLVMTypeOf(LLVMGetParam(fn, 2)),
                                               NEWVAR("cat"));
            }

            COMPILE_INCREF_ZREF(module, builder, *arg, args[1]);

            v = LLVMBuildCall(builder, fn, args, countof(args), "");

            COMPILE_DECREF_ZREF(module, builder, *arg, args[1]);
        }
        break;

    default:
        TRACE("*cont=%p", *cont);
        lkit_expr_dump(expr);
        FAIL("compile_set");
    }

end:
    return v;

err:
    //lkit_expr_dump(expr);
    v = NULL;
    goto end;
}


static LLVMValueRef
lkit_compile_del(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    lkit_expr_t **arg, **cont;
    LLVMValueRef fn, args[2];
    char *mpsuffix;

    lctx = LLVMGetModuleContext(module);

    /* container */
    if ((cont = array_get(&expr->subs, 0)) == NULL) {
        FAIL("array_get");
    }

    mpsuffix = (*cont)->mpolicy == LKIT_MPMPOOL ? "_mpool" : "";

    if ((args[0] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *cont,
                                     udata)) == NULL) {
        TR(COMPILE_DEL + 1);
        goto err;
    }

    if ((arg = array_get(&expr->subs, 1)) == NULL) {
        FAIL("array_get");
    }

    switch ((*cont)->type->tag) {
    case LKIT_STRUCT:
        {
            int idx;
            lkit_type_t *fty;
            char buf[64];

            assert((*arg)->type->tag == LKIT_STR && LKIT_EXPR_CONSTANT(*arg));

            if ((idx = lkit_struct_get_field_index(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == -1) {
                TR(COMPILE_DEL + 100);
                goto err;
            }

            if ((fty = lkit_struct_get_field_type(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == NULL) {
                TR(COMPILE_DEL + 101);
                goto err;
            }

            snprintf(buf,
                     sizeof(buf),
                     "mrklkit_rt_struct_del_item_%s%s",
                     fty->name,
                     mpsuffix);

            args[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx), idx, 1);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("fn=%s", buf);
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));
            v = LLVMBuildCall(builder, fn, args, countof(args), "");
        }

        break;

    case LKIT_DICT:
        {
            assert((*arg)->type->tag == LKIT_STR);

            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *arg,
                                             udata)) == NULL) {
                TR(COMPILE_DEL + 201);
                goto err;
            }

            if ((*cont)->mpolicy == LKIT_MPMPOOL) {
                if ((fn = LLVMGetNamedFunction(
                            module,
                            "mrklkit_rt_dict_del_item_mpool")) == NULL) {
                    TRACE("fn=%s", "mrklkit_rt_dict_del_item_mpool");
                    FAIL("LLVMGetNamedFunction");
                }
            } else {
                if ((fn = LLVMGetNamedFunction(
                            module,
                            "mrklkit_rt_dict_del_item")) == NULL) {
                    TRACE("fn=%s", "mrklkit_rt_dict_del_item");
                    FAIL("LLVMGetNamedFunction");
                }
            }

            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));

            COMPILE_INCREF_ZREF(module, builder, *arg, args[1]);

            v = LLVMBuildCall(builder, fn, args, countof(args), "");

            COMPILE_DECREF_ZREF(module, builder, *arg, args[1]);
        }
        break;

    default:
        TRACE("*cont=%p", *cont);
        lkit_expr_dump(expr);
        FAIL("compile_del");
    }

end:
    return v;

err:
    //lkit_expr_dump(expr);
    v = NULL;
    goto end;
}


static LLVMValueRef
lkit_compile_parse(mrklkit_ctx_t *mctx,
                   lkit_cexpr_t *ectx,
                   LLVMModuleRef module,
                   LLVMBuilderRef builder,
                   lkit_expr_t *expr,
                   void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    UNUSED lkit_type_t *ty;
    lkit_struct_t *ts;
    lkit_expr_t **cont, **key;
    LLVMValueRef fn, args[2];
    char *name = (char *)expr->name->data;
    int idx;
    lkit_type_t *fty;
    char buf[64];

    lctx = LLVMGetModuleContext(module);

    ty = expr->type;

    /* container */
    if ((cont = array_get(&expr->subs, 0)) == NULL) {
        FAIL("array_get");
    }

    if ((*cont)->type->tag != LKIT_PARSER) {
        TR(COMPILE_PARSE + 1);
        goto err;
    }

    if ((args[0] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *cont,
                                     udata)) == NULL) {
        TR(COMPILE_PARSE + 2);
        goto err;
    }

    if ((key = array_get(&expr->subs, 1)) == NULL) {
        FAIL("array_get");
    }
    assert((*key)->type->tag == LKIT_STR && LKIT_EXPR_CONSTANT(*key));

    ts = (lkit_struct_t *)LKIT_PARSER_GET_TYPE((*cont)->type);
    if ((idx = lkit_struct_get_field_index(ts,
                (bytes_t *)(*key)->value.literal->body)) == -1) {
        TR(COMPILE_PARSE + 3);
        goto err;
    }

    if ((fty = lkit_struct_get_field_type(ts,
                (bytes_t *)(*key)->value.literal->body)) == NULL) {
        TR(COMPILE_PARSE + 4);
        goto err;
    }

    assert(lkit_type_cmp(ty, fty) == 0);

    if (expr->mpolicy == LKIT_MPMPOOL) {
        (void)snprintf(buf,
                 sizeof(buf),
                 "dparse_struct_item_ra_%s_mpool",
                 fty->name);
    } else {
        (void)snprintf(buf,
                 sizeof(buf),
                 "dparse_struct_item_ra_%s",
                 fty->name);
    }

    args[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx), idx, 1);

    if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
        TRACE("cannot find %s", buf);
        FAIL("LLVMGetNamedFunction");
    }
    args[0] = LLVMBuildPointerCast(builder,
                                   args[0],
                                   LLVMTypeOf(LLVMGetParam(fn, 0)),
                                   NEWVAR("cast"));

    v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR(name));

    if (LKIT_TAG_POINTER(fty->tag)) {
        v = LLVMBuildPointerCast(builder,
                                 v,
                                 mrklkit_ctx_get_type_backend(mctx,
                                                              fty),
                                 NEWVAR("cast"));
    }

end:
    return v;

err:
    //lkit_expr_dump(expr);
    v = NULL;
    goto end;
}


static LLVMValueRef
compile_str_join(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 LLVMContextRef lctx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 void *udata)
{
    size_t i;
    lkit_expr_t **arg;
    array_iter_t it;
    LLVMValueRef v = NULL;
    UNUSED LLVMValueRef sz;
    LLVMValueRef tmp, accum, bnfn, bcfn;
    UNUSED LLVMValueRef bifn, bdfn;
    LLVMValueRef *av, *asz;
    LLVMValueRef const0, const1;

    const0 = LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0);
    const1 = LLVMConstInt(LLVMInt64TypeInContext(lctx), 1, 0);
    accum = const0;
    if ((av = malloc(sizeof(LLVMValueRef) * expr->subs.elnum)) == NULL) {
        FAIL("malloc");
    }
    if ((asz = malloc(sizeof(LLVMValueRef) * expr->subs.elnum)) == NULL) {
        FAIL("malloc");
    }
    for (arg = array_first(&expr->subs, &it);
         arg != NULL;
         arg = array_next(&expr->subs, &it)) {
        av[it.iter] = lkit_compile_expr(mctx,
                                        ectx,
                                        module,
                                        builder,
                                        *arg,
                                        udata);
        assert(av[it.iter] != NULL);
        COMPILE_INCREF_ZREF(module, builder, *arg, av[it.iter]);
        asz[it.iter] = LLVMBuildStructGEP(builder,
                                          av[it.iter],
                                          BYTES_SZ_IDX,
                                          NEWVAR("gep"));
        asz[it.iter] = LLVMBuildLoad(builder, asz[it.iter], NEWVAR("load"));
        accum = LLVMBuildAdd(builder, accum, asz[it.iter], NEWVAR("plus"));
        // minus intermediate zero term
        accum = LLVMBuildSub(builder, accum, const1, NEWVAR("dec"));
    }
    // plus final zero term
    accum = LLVMBuildAdd(builder, accum, const1, NEWVAR("plus"));

    if (expr->mpolicy == LKIT_MPMPOOL) {
        if ((bnfn = LLVMGetNamedFunction(
                        module, "mrklkit_rt_bytes_new_mpool")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }
    } else {
        if ((bnfn = LLVMGetNamedFunction(
                        module, "mrklkit_rt_bytes_new")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }
    }
    if ((bcfn = LLVMGetNamedFunction(module,
                                     "bytes_copyz")) == NULL) {
        FAIL("LLVMGetNamedFunction");
    }
    v = LLVMBuildCall(builder, bnfn, &accum, 1, NEWVAR("call"));
    tmp = const0;
    for (i = 0; i < expr->subs.elnum; ++i) {
        LLVMValueRef args[3];

        args[0] = v;
        args[1] = av[i];
        args[2] = tmp;
        (void)LLVMBuildCall(builder, bcfn, args, countof(args), NEWVAR("call"));
        tmp = LLVMBuildAdd(builder, tmp, asz[i], NEWVAR("plus"));
        tmp = LLVMBuildSub(builder, tmp, const1, NEWVAR("dec")); /* zero term */
    }
    for (arg = array_first(&expr->subs, &it);
         arg != NULL;
         arg = array_next(&expr->subs, &it)) {
        COMPILE_DECREF_ZREF(module, builder, *arg, av[it.iter]);
    }
    free(av);
    free(asz);

    return v;
}


static LLVMValueRef
compile_function(mrklkit_ctx_t *mctx,
                 lkit_cexpr_t *ectx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    char *name = (char *)expr->name->data;

    lctx = LLVMGetModuleContext(module);

    //lkit_expr_dump(expr);
    //TRACE("trying %s", expr->name->data);

    if (strcmp(name, "if") == 0) {
        lkit_expr_t **cexpr, **texpr, **fexpr;

        //(sym if (func undef bool undef undef))
        if (expr->subs.elnum < 3) {
            TR(COMPILE_FUNCTION + 100);
            goto err;
        }

        cexpr = array_get(&expr->subs, 0);
        assert(cexpr != NULL);
        texpr = array_get(&expr->subs, 1);
        assert(texpr != NULL);
        fexpr = array_get(&expr->subs, 2);
        assert(fexpr != NULL);
        v = compile_if(mctx,
                       ectx,
                       module,
                       builder,
                       *cexpr,
                       *texpr,
                       *fexpr,
                       expr->type,
                       udata);

    } else if (strcmp(name, ",") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym , (func undef undef ...))
        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            if ((v = lkit_compile_expr(mctx,
                                       ectx,
                                       module,
                                       builder,
                                       *arg,
                                       udata)) == NULL) {
                TR(COMPILE_FUNCTION + 200);
                goto err;
            }
            /* XXX all but last */
            //BUILD_INCREF_ZREF(module, builder, *arg, v);
            //BUILD_DECREF_ZREF(module, builder, *arg, v);
        }

    } else if (strcmp(name, "print") == 0) {
        LLVMValueRef fn, args[2];

        // (sym print (func undef undef ...))
        if ((fn = LLVMGetNamedFunction(module, "printf")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        } else {
            lkit_expr_t **arg;
            array_iter_t it;

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                if ((v = lkit_compile_expr(mctx,
                                           ectx,
                                           module,
                                           builder,
                                           *arg,
                                           udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 301);
                    goto err;
                }

                switch ((*arg)->type->tag) {
                case LKIT_INT:
                case LKIT_INT_MIN:
                case LKIT_INT_MAX:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%ld ",
                                                       NEWVAR("printf.fmt"));
                    args[1] = v;
                    break;

                case LKIT_FLOAT:
                case LKIT_FLOAT_MIN:
                case LKIT_FLOAT_MAX:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%lf ",
                                                       NEWVAR("printf.fmt"));
                    args[1] = v;
                    break;

                case LKIT_STR:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%s ",
                                                       NEWVAR("printf.fmt"));
                    /*
                     * bytes_t *
                     */
                    {
                        char *n;

                        n = ((*arg)->name != NULL) ?
                                (char *)(*arg)->name->data :
                                "str";
                        args[1] = LLVMBuildStructGEP(builder,
                                                     v,
                                                     BYTES_DATA_IDX,
                                                     NEWVAR(n));
                    }
                    break;

                case LKIT_BOOL:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%hhd ",
                                                       NEWVAR("printf.fmt"));
                    args[1] = v;
                    break;

                default:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%p ",
                                                       NEWVAR("printf.fmt"));
                    args[1] = v;
                    break;

                }

                (void)LLVMBuildCall(builder, fn, args, 2, NEWVAR(name));
            }

            args[0] = LLVMBuildGlobalStringPtr(builder,
                                               "\n",
                                               NEWVAR("printf.fmt"));
            (void)LLVMBuildCall(builder, fn, args, 1, NEWVAR("printf.nl"));
        }

    } else if (strcmp(name , "+") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym + (func undef undef ...))
        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            v = LLVMConstInt(mrklkit_ctx_get_type_backend(mctx, expr->type),
                             0,
                             1);

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 400);
                    goto err;
                }

                v = LLVMBuildAdd(builder, v, rand, NEWVAR("plus"));
            }

        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            v = LLVMConstReal(mrklkit_ctx_get_type_backend(mctx, expr->type),
                              0.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 401);
                    goto err;
                }

                v = LLVMBuildFAdd(builder, v, rand, NEWVAR("plus"));
            }
        } else if (expr->type->tag == LKIT_STR) {
            v = compile_str_join(mctx,
                                 ectx,
                                 lctx,
                                 module,
                                 builder,
                                 expr,
                                 udata);
        } else {
            TR(COMPILE_FUNCTION + 402);
            goto err;
        }

    } else if (strcmp(name , "-") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym - (func undef undef ...))
        /*
         * (- 1 2 3 4) -> (+ 1 (- 0 2 3 4)) -> -8
         *   not:
         * (- 1 2 3 4) -> (- 0 1 2 3 4) -> -10
         */
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 500);
            goto err;
        }
        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 501);
                    goto err;
                }

                v = LLVMBuildSub(builder, v, rand, NEWVAR("minus"));
            }
        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 502);
                    goto err;
                }

                v = LLVMBuildFSub(builder, v, rand, NEWVAR("minus"));
            }
        } else {
            TR(COMPILE_FUNCTION + 503);
            goto err;
        }

    } else if (strcmp(name , "/") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym / (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 600);
            goto err;
        }

        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, tmp;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 601);
                    goto err;
                }

                /*
                 * the following hack: v / (rand ? rand : INT64_MAX)
                 */
                tmp = LLVMBuildICmp(builder,
                                    LLVMIntEQ,
                                    rand,
                                    LLVMConstInt(
                                        LLVMInt64TypeInContext(lctx),
                                        0,
                                        0),
                                    NEWVAR("test"));
                rand = LLVMBuildSelect(builder,
                                       tmp,
                                       LLVMConstInt(
                                           LLVMInt64TypeInContext(lctx),
                                           INT64_MAX,
                                           0),
                                       rand,
                                       NEWVAR("select"));
                v = LLVMBuildSDiv(builder, v, rand, NEWVAR("div"));
            }
        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 602);
                    goto err;
                }

                v = LLVMBuildFDiv(builder, v, rand, NEWVAR("div"));
            }
        } else {
            TR(COMPILE_FUNCTION + 603);
            goto err;
        }

    } else if (strcmp(name , "*") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym * (func undef undef ...))
        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            v = LLVMConstInt(mrklkit_ctx_get_type_backend(mctx, expr->type),
                             1,
                             1);

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 700);
                    goto err;
                }

                v = LLVMBuildMul(builder, v, rand, NEWVAR("mul"));
            }

        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            v = LLVMConstReal(mrklkit_ctx_get_type_backend(mctx, expr->type),
                              1.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 701);
                    goto err;
                }

                v = LLVMBuildFMul(builder, v, rand, NEWVAR("mul"));
            }
        } else {
            TR(COMPILE_FUNCTION + 702);
            goto err;
        }

    } else if (strcmp(name , "%") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym % (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 800);
            goto err;
        }

        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, tmp;


                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 801);
                    goto err;
                }

                /*
                 * the following hack: v / (rand ? rand : INT64_MAX)
                 */
                tmp = LLVMBuildICmp(builder,
                                    LLVMIntEQ,
                                    rand,
                                    LLVMConstInt(
                                        LLVMInt64TypeInContext(lctx),
                                        0,
                                        0),
                                    NEWVAR("test"));
                rand = LLVMBuildSelect(builder,
                                       tmp,
                                       LLVMConstInt(
                                           LLVMInt64TypeInContext(lctx),
                                           INT64_MAX,
                                           0),
                                       rand,
                                       NEWVAR("select"));
                v = LLVMBuildSRem(builder, v, rand, NEWVAR("rem"));
            }
        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 802);
                    goto err;
                }

                v = LLVMBuildFRem(builder, v, rand, NEWVAR("rem"));
            }
        } else {
            TR(COMPILE_FUNCTION + 803);
            goto err;
        }

    } else if (strcmp(name , "min") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym min (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 900);
            goto err;
        }

#if 0
        LLVMValueRef mem;
        mem = LLVMBuildAlloca(builder, mrklkit_ctx_get_type_backend(mctx, expr->type), NEWVAR("mem"));
        LLVMBuildStore(builder, v, mem);
        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = lkit_compile_expr(mctx, ectx, module, builder, *arg, udata)) == NULL) {
                TR(COMPILE_FUNCTION + 901);
                goto err;
            }

            LLVMBuildAtomicRMW(builder,
                               LLVMAtomicRMWBinOpMin,
                               mem,
                               rand,
                               LLVMAtomicOrderingNotAtomic,
                               1);
        }

        v = LLVMBuildLoad(builder, mem, NEWVAR("min"));
#endif

        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 902);
                    goto err;
                }

                cond = LLVMBuildICmp(builder,
                                     LLVMIntSLT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("min"));
            }

        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 903);
                    goto err;
                }

                cond = LLVMBuildFCmp(builder,
                                     LLVMRealULT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("min"));
            }

        } else {
            TR(COMPILE_FUNCTION + 904);
            goto err;
        }

    } else if (strcmp(name , "max") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym max (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1000);
            goto err;
        }

        if (expr->type->tag == LKIT_INT ||
            expr->type->tag == LKIT_INT_MIN ||
            expr->type->tag == LKIT_INT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 1001);
                    goto err;
                }

                cond = LLVMBuildICmp(builder,
                                     LLVMIntSGT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("max"));
            }

        } else if (expr->type->tag == LKIT_FLOAT ||
                   expr->type->tag == LKIT_FLOAT_MIN ||
                   expr->type->tag == LKIT_FLOAT_MAX) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = lkit_compile_expr(mctx,
                                              ectx,
                                              module,
                                              builder,
                                              *arg,
                                              udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 1002);
                    goto err;
                }

                cond = LLVMBuildFCmp(builder,
                                     LLVMRealUGT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("max"));
            }

        } else {
            TR(COMPILE_FUNCTION + 1003);
            goto err;
        }

#define MRKLKIT_BUILTINGEN_AND_OR_BODY(endvar,                                 \
                                       nextvar,                                \
                                       trcode,                                 \
                                       testopcode,                             \
                                       buildop)                                \
        lkit_expr_t **arg;                                                     \
        array_iter_t it;                                                       \
        LLVMValueRef parent, zero, phi;                                        \
        LLVMBasicBlockRef currblock, nextblock, endblock;                      \
        zero = LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0);                \
        currblock = LLVMGetInsertBlock(builder);                               \
        parent = LLVMGetBasicBlockParent(currblock);                           \
        endblock = LLVMAppendBasicBlockInContext(lctx,                         \
                                                 parent,                       \
                                                 NEWVAR(endvar));              \
        LLVMMoveBasicBlockAfter(endblock, currblock);                          \
        LLVMPositionBuilderAtEnd(builder, endblock);                           \
        phi = LLVMBuildPhi(builder, LLVMTypeOf(zero), NEWVAR("phi"));          \
        LLVMPositionBuilderAtEnd(builder, currblock);                          \
        arg = array_first(&expr->subs, &it);                                   \
        if ((v = lkit_compile_expr(mctx,                                       \
                                   ectx,                                       \
                                   module,                                     \
                                   builder,                                    \
                                   *arg,                                       \
                                   udata)) == NULL) {                          \
            TR(COMPILE_FUNCTION + trcode);                                     \
            goto err;                                                          \
        }                                                                      \
        currblock = LLVMGetInsertBlock(builder);                               \
        LLVMAddIncoming(phi, &v, &currblock, 1);                               \
        for (arg = array_next(&expr->subs, &it);                               \
             arg != NULL;                                                      \
             arg = array_next(&expr->subs, &it)) {                             \
            LLVMValueRef rand, test;                                           \
            /*                                                                 \
             * short circuit                                                   \
             */                                                                \
            nextblock = LLVMAppendBasicBlockInContext(lctx,                    \
                                                      parent,                  \
                                                      NEWVAR(nextvar));        \
            LLVMMoveBasicBlockAfter(nextblock, currblock);                     \
            test = LLVMBuildICmp(builder, testopcode, v, zero, NEWVAR("test"));\
            (void)LLVMBuildCondBr(builder, test, endblock, nextblock);         \
            currblock = nextblock;                                             \
            LLVMPositionBuilderAtEnd(builder, currblock);                      \
            if ((rand = lkit_compile_expr(mctx,                                \
                                          ectx,                                \
                                          module,                              \
                                          builder,                             \
                                          *arg,                                \
                                          udata)) == NULL) {                   \
                TR(COMPILE_FUNCTION + trcode + 1);                             \
                goto err;                                                      \
            }                                                                  \
            v = buildop(builder, v, rand, NEWVAR("and"));                      \
            currblock = LLVMGetInsertBlock(builder);                           \
            LLVMAddIncoming(phi, &v, &currblock, 1);                           \
        }                                                                      \
        LLVMBuildBr(builder, endblock);                                        \
        LLVMPositionBuilderAtEnd(builder, endblock);                           \
        v = phi


    } else if (strcmp(name , "and") == 0) {
        MRKLKIT_BUILTINGEN_AND_OR_BODY("A.end",
                                       "A.next",
                                       1100,
                                       LLVMIntEQ,
                                       LLVMBuildAnd);

    } else if (strcmp(name , "or") == 0) {
        MRKLKIT_BUILTINGEN_AND_OR_BODY("O.end",
                                       "O.next",
                                       1200,
                                       LLVMIntNE,
                                       LLVMBuildOr);

    } else if (strcmp(name , "not") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym not (func bool bool))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1300);
            goto err;
        }
        v = LLVMBuildNot(builder, v, NEWVAR("not"));

    } else if (strcmp(name , "==") == 0 ||
               strcmp(name , "=" /* compat */
              ) == 0) {
        //(sym == (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntEQ,
                        LLVMRealUEQ,
                        udata);

    } else if (strcmp(name , "!=") == 0) {
        //(sym != (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntNE,
                        LLVMRealUNE,
                        udata);

    } else if (strcmp(name , "<") == 0) {
        //(sym < (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntSLT,
                        LLVMRealULT,
                        udata);

    } else if (strcmp(name , "<=") == 0) {
        //(sym <= (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntSLE,
                        LLVMRealULE,
                        udata);

    } else if (strcmp(name , ">") == 0) {
        //(sym > (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntSGT,
                        LLVMRealUGT,
                        udata);

    } else if (strcmp(name , ">=") == 0) {
        //(sym >= (func bool undef undef)) done
        v = compile_cmp(mctx,
                        ectx,
                        module,
                        builder,
                        expr,
                        LLVMIntSGE,
                        LLVMRealUGE,
                        udata);

#define MRKLKIT_BUILTINGEN_INTRINSIC_BODY1(n, t, m)                            \
        lkit_expr_t **arg;                                                     \
        array_iter_t it;                                                       \
        LLVMValueRef fn, args[1];                                              \
        if ((fn = LLVMGetNamedFunction(module, "llvm." n "." t)) == NULL) {    \
            FAIL("LLVMGetNamedFunction");                                      \
        }                                                                      \
        arg = array_first(&expr->subs, &it);                                   \
        if ((args[0] = lkit_compile_expr(mctx,                                 \
                                   ectx,                                       \
                                   module,                                     \
                                   builder,                                    \
                                   *arg,                                       \
                                   udata)) == NULL) {                          \
            TR(COMPILE_FUNCTION + m);                                          \
            goto err;                                                          \
        }                                                                      \
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));   \


#define MRKLKIT_BUILTINGEN_INTRINSIC_BODY2(n, t, m)                            \
        lkit_expr_t **arg;                                                     \
        array_iter_t it;                                                       \
        LLVMValueRef fn, args[2];                                              \
        if ((fn = LLVMGetNamedFunction(module, "llvm." n "." t)) == NULL) {    \
            FAIL("LLVMGetNamedFunction");                                      \
        }                                                                      \
        arg = array_first(&expr->subs, &it);                                   \
        if ((args[0] = lkit_compile_expr(mctx,                                 \
                                   ectx,                                       \
                                   module,                                     \
                                   builder,                                    \
                                   *arg,                                       \
                                   udata)) == NULL) {                          \
            TR(COMPILE_FUNCTION + m);                                          \
            goto err;                                                          \
        }                                                                      \
        args[1] = LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0);             \
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));   \


    } else if (strcmp(name , "bswap") == 0) {
        //(sym bswap (func int int )) done
        MRKLKIT_BUILTINGEN_INTRINSIC_BODY1("bswap", "i64", 1351)

    } else if (strcmp(name , "bcnt") == 0) {
        //(sym bswap (func int int )) done
        MRKLKIT_BUILTINGEN_INTRINSIC_BODY1("ctpop", "i64", 1352)

    } else if (strcmp(name , "ffs") == 0) {
        //(sym bswap (func int int )) done
        MRKLKIT_BUILTINGEN_INTRINSIC_BODY2("cttz", "i64", 1353)

    } else if (strcmp(name , "fls") == 0) {
        //(sym bswap (func int int )) done
        MRKLKIT_BUILTINGEN_INTRINSIC_BODY2("ctlz", "i64", 1354)

    } else if (strcmp(name, "get") == 0 ||
               strcmp(name, "dp-get") == 0 || /* compat */
               strcmp(name, "get-index") == 0 || /* compat */
               strcmp(name, "get-key") == 0 /* compat */
              ) {
        //(sym get (func undef undef undef under)) done
        v = lkit_compile_get(mctx,
                             ectx,
                             module,
                             builder,
                             expr,
                             udata);

    } else if (strcmp(name, "parse") == 0) {
        //(sym parse (func undef any str)) done
        /*
         * pass through to modules
         */
        v = lkit_compile_parse(mctx,
                               ectx,
                               module,
                               builder,
                               expr,
                               udata);

    } else if (strcmp(name, "itof") == 0 ||
               strcmp(name, "float") == 0 /* compat */
              ) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym itof (func float int))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1400);
            goto err;
        }

        v = LLVMBuildSIToFP(builder,
                            v,
                            LLVMDoubleTypeInContext(lctx),
                            NEWVAR("itof"));

    } else if (strcmp(name, "ftoi") == 0 ||
               strcmp(name, "int") == 0 /* compat */
              ) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym ftoi (func int float))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1500);
            goto err;
        }
        v = LLVMBuildFPToSI(builder,
                            v,
                            LLVMInt64TypeInContext(lctx),
                            NEWVAR("ftoi"));

    } else if (strcmp(name, "atoi") == 0) {
        LLVMValueRef fn, args[2];
        lkit_expr_t **arg0, **arg1;
        // (builtin atoi (func int str int))


        if ((fn = LLVMGetNamedFunction(module, "mrklkit_strtoi64")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        if ((arg0 = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg0,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1601);
            goto err;
        }

        if ((arg1 = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1602);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg0, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg0, args[0]);

    } else if (strcmp(name, "atoi-loose") == 0) {
        LLVMValueRef fn, args[1];
        lkit_expr_t **arg;
        // (builtin atoi (func int str))


        if ((fn = LLVMGetNamedFunction(module,
                                       "mrklkit_strtoi64_loose")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1610);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);

    } else if (strcmp(name, "atof") == 0) {
        LLVMValueRef fn, args[2];
        lkit_expr_t **arg0, **arg1;
        // (builtin atof (func float str float))


        if ((fn = LLVMGetNamedFunction(module, "mrklkit_strtod")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        if ((arg0 = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg0,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1701);
            goto err;
        }

        if ((arg1 = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1702);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg0, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg0, args[0]);

    } else if (strcmp(name, "atof-loose") == 0) {
        LLVMValueRef fn, args[1];
        lkit_expr_t **arg;
        // (builtin atof-loose (func float str))


        if ((fn = LLVMGetNamedFunction(module,
                                       "mrklkit_strtod_loose")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1710);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);

    } else if (strcmp(name, "itob") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym itob (func bool int))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1800);
            goto err;
        }

        v = LLVMBuildICmp(builder,
                          LLVMIntNE,
                          v,
                          LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0),
                          NEWVAR("test"));

    } else if (strcmp(name, "btoi") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym btoi (func int bool))
        arg = array_first(&expr->subs, &it);
        if ((v = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1900);
            goto err;
        }

        v = LLVMBuildZExt(builder,
                          v,
                          LLVMInt64TypeInContext(lctx),
                          NEWVAR("zext"));

    } else if (strcmp(name , "startswith") == 0) {
        lkit_expr_t **arg0, **arg1;
        LLVMValueRef fn, args[2];

        if ((fn = LLVMGetNamedFunction(module, "bytes_startswith")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        arg0 = array_get(&expr->subs, 0);
        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg0,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1950);
            goto err;
        }
        arg1 = array_get(&expr->subs, 1);
        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1951);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg0, args[0]);
        COMPILE_INCREF_ZREF(module, builder, *arg1, args[1]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg0, args[0]);
        COMPILE_DECREF_ZREF(module, builder, *arg1, args[1]);

    } else if (strcmp(name , "endswith") == 0) {
        lkit_expr_t **arg0, **arg1;
        LLVMValueRef fn, args[2];

        if ((fn = LLVMGetNamedFunction(module, "bytes_endswith")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        arg0 = array_get(&expr->subs, 0);
        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg0,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1960);
            goto err;
        }
        arg1 = array_get(&expr->subs, 1);
        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1961);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg0, args[0]);
        COMPILE_INCREF_ZREF(module, builder, *arg1, args[1]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        compile_decref(module, builder, *arg0, args[0]);
        compile_decref(module, builder, *arg1, args[1]);

    } else if (strcmp(name , "tostr") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;
        LLVMValueRef fn, fnarg;
        char *mpsuffix;
        char buf[1024];

        //(sym tostr (func str undef))
        arg = array_first(&expr->subs, &it);
        if ((fnarg = lkit_compile_expr(mctx,
                                       ectx,
                                       module,
                                       builder,
                                       *arg,
                                       udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2000);
            goto err;
        }

        mpsuffix = (*arg)->mpolicy == LKIT_MPMPOOL ? "_mpool" : "";

        switch ((*arg)->type->tag) {
        case LKIT_INT:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_bytes_new_from_int%s",
                           mpsuffix);
            break;

        case LKIT_FLOAT:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_bytes_new_from_float%s",
                           mpsuffix);
            break;

        case LKIT_BOOL:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_bytes_new_from_bool%s",
                           mpsuffix);
            break;

        case LKIT_STR:
            v = fnarg;
            goto tostr_done;
            break;

        default:
            lkit_expr_dump(expr);
            FAIL("compile_function, tostr argument type is not supported");
        }

        if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        v = LLVMBuildCall(builder, fn, &fnarg, 1, NEWVAR("call"));

tostr_done:
        ;


    } else if (strcmp(name, "in") == 0) {
        /*
         * no zref, no incref/decref
         */
        if (expr->subs.elnum < 2) {
            v = LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0);
        } else {
            lkit_expr_t **subj, **obj;
            array_iter_t it;
            LLVMBasicBlockRef currblock, nextblock, endblock;
            LLVMValueRef tmp;

            if (expr->type->tag != LKIT_BOOL) {
                FAIL("compile_function");
            }

            if ((subj = array_first(&expr->subs, &it)) == NULL) {
                FAIL("array_first");
            }

            currblock = LLVMGetInsertBlock(builder);
            nextblock = LLVMGetNextBasicBlock(currblock);
            if (nextblock == NULL) {
                LLVMValueRef fn;
                fn = LLVMGetBasicBlockParent(currblock);
                endblock = LLVMAppendBasicBlockInContext(lctx,
                                                         fn,
                                                         NEWVAR("IN.end"));
            } else {
                endblock = LLVMInsertBasicBlockInContext(lctx,
                                                         nextblock,
                                                         NEWVAR("IN.end"));
            }

            LLVMPositionBuilderAtEnd(builder, endblock);
            v = LLVMBuildPhi(builder,
                             LLVMInt1TypeInContext(lctx),
                             NEWVAR("phi"));

            for (obj = array_next(&expr->subs, &it);
                 obj != NULL;
                 obj = array_next(&expr->subs, &it)) {

                LLVMBasicBlockRef testblock;

                LLVMPositionBuilderAtEnd(builder, currblock);
                if ((tmp = _compile_cmp(mctx,
                                        ectx,
                                        module,
                                        builder,
                                        *subj, *obj,
                                        LLVMIntEQ,
                                        LLVMRealUEQ,
                                        udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 2100);
                    goto err;
                }
                currblock = LLVMGetInsertBlock(builder);
                testblock = LLVMInsertBasicBlockInContext(lctx,
                                                          endblock,
                                                          NEWVAR("IN.test"));
                (void)LLVMBuildCondBr(builder, tmp, endblock, testblock);
                LLVMAddIncoming(v, &tmp, &currblock, 1);
                currblock = testblock;
            }

            LLVMPositionBuilderAtEnd(builder, currblock);
            tmp = LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0);
            LLVMAddIncoming(v, &tmp, &currblock, 1);
            LLVMBuildBr(builder, endblock);
            LLVMPositionBuilderAtEnd(builder, endblock);
        }

    } else if (strcmp(name, "substr") == 0) {
        lkit_expr_t **arg0, **arg1, **arg2;
        LLVMValueRef fn, args[3];
        //(sym substr (func str str int int))

        if ((arg0 = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((*arg0)->mpolicy == LKIT_MPMPOOL) {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_slice_mpool")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        } else {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_slice")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg0,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2201);
            goto err;
        }

        if ((arg1 = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2202);
            goto err;
        }

        if ((arg2 = array_get(&expr->subs, 2)) == NULL) {
            FAIL("array_get");
        }

        if ((args[2] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg2,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2203);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg0, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg0, args[0]);

    } else if (strcmp(name, "strfind") == 0) {
        FAIL("compile_function, not supported: strfind");

    } else if (strcmp(name, "split") == 0) {
        lkit_expr_t **arg1, **arg2;
        lkit_array_t *ty;
        LLVMValueRef fn, args[3];

        /*
         * very slow b/c array_incr_mpool(), use parse
         */

        //(sym split (func (array str) str str))
        ty = lkit_type_get_array(mctx, LKIT_STR);

        if (expr->mpolicy == LKIT_MPMPOOL) {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_array_split_mpool")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        } else {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_array_split")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        }

        args[0] = LLVMConstIntToPtr(
                LLVMConstInt(LLVMInt64TypeInContext(lctx), (uintptr_t)ty, 0),
                LLVMTypeOf(LLVMGetParam(fn, 0)));

        arg1 = array_get(&expr->subs, 0);
        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg1,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2301);
            goto err;
        }

        arg2 = array_get(&expr->subs, 1);
        if ((args[2] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg2,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2302);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg1, args[1]);
        COMPILE_INCREF_ZREF(module, builder, *arg2, args[2]);

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg1, args[1]);
        COMPILE_DECREF_ZREF(module, builder, *arg2, args[2]);

    } else if (strcmp(name, "brushdown") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef fn, args[1];

        //(sym brushdown (func str str))
        if (expr->mpolicy == LKIT_MPMPOOL) {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_brushdown_mpool")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        } else {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_brushdown")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        }

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2401);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);

    } else if (strcmp(name, "urldecode") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef fn, args[1];

        //(sym urldecode (func str str))
        if (expr->mpolicy == LKIT_MPMPOOL) {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_urldecode_mpool")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        } else {
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_urldecode")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
        }

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2501);
            goto err;
        }

        COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
        COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);

    } else if (strcmp(name, "len") == 0) {
        lkit_expr_t **arg;

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((*arg)->type->tag == LKIT_ARRAY) {
            char *fnname;
            LLVMValueRef fn, args[1];

            fnname = "mrklkit_rt_array_len";
            if ((fn = LLVMGetNamedFunction(module, fnname)) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }

            if ((args[0] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *arg,
                                             udata)) == NULL) {
                TR(COMPILE_FUNCTION + 2601);
                goto err;
            }

            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMPointerType(
                                               LLVMInt8TypeInContext(lctx), 0),
                                           NEWVAR("cast"));

            COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
            COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);

        } else if ((*arg)->type->tag == LKIT_STR) {
            LLVMValueRef tmp, const1;

            if ((tmp = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
                TR(COMPILE_FUNCTION + 2602);
                goto err;
            }
            COMPILE_INCREF_ZREF(module, builder, *arg, tmp);
            v = LLVMBuildStructGEP(builder, tmp, BYTES_SZ_IDX, NEWVAR("gep"));
            v = LLVMBuildLoad(builder, v, NEWVAR("load"));
            const1 = LLVMConstInt(LLVMInt64TypeInContext(lctx), 1, 0);
            v = LLVMBuildSub(builder, v, const1, NEWVAR("dec")); /* zero term */
            COMPILE_DECREF_ZREF(module, builder, *arg, tmp);
        } else {
            FAIL("compile_function");
        }

    } else if (strcmp(name, "has") == 0 ||
               strcmp(name, "has-key") == 0) {
        lkit_expr_t **cont, **name;
        LLVMValueRef fn, args[2];

        /* container */
        if ((cont = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        /* name */
        if ((name = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        assert(!(*name)->isref);

        switch ((*cont)->type->tag) {
        case LKIT_DICT:
            if ((fn = LLVMGetNamedFunction(module,
                    "mrklkit_rt_dict_has_item")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            if ((args[0] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *cont,
                                             udata)) == NULL) {
                TR(COMPILE_FUNCTION + 2701);
                goto err;
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMPointerType(
                                               LLVMInt8TypeInContext(lctx), 0),
                                           NEWVAR("cast"));
            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *name,
                                             udata)) == NULL) {
                TR(COMPILE_FUNCTION + 2702);
                goto err;
            }
            COMPILE_INCREF_ZREF(module, builder, *name, args[1]);
            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
            COMPILE_DECREF_ZREF(module, builder, *name, args[1]);
            break;

        default:
            FAIL("compile_function");
        }

    } else if (strcmp(name, "dp-info") == 0) {
        lkit_expr_t **cont, **opt;
        bytes_t *optname;
        LLVMValueRef fn, args[1];

        //(sym dp-info (func undef undef str))

        /* container */
        if ((cont = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *cont,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 4900);
            goto err;
        }

        args[0] = LLVMBuildPointerCast(builder,
                                       args[0],
                                       LLVMPointerType(
                                           LLVMInt8TypeInContext(lctx), 0),
                                       NEWVAR("cast"));


        /* option */
        if ((opt = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        /* constant str */
        assert(!(*opt)->isref);
        optname = (bytes_t *)(*opt)->value.literal->body;

        if (strcmp((char *)optname->data, "pos") == 0) {
            if ((fn = LLVMGetNamedFunction(
                            module, "rt_parser_info_pos")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }

        } else if (strcmp((char *)optname->data, "data") == 0) {
            if (expr->mpolicy == LKIT_MPMPOOL) {
                if ((fn = LLVMGetNamedFunction(
                                module, "rt_parser_info_data_mpool")) == NULL) {
                    FAIL("LLVMGetNamedFunction");
                }
            } else {
                if ((fn = LLVMGetNamedFunction(
                                module, "rt_parser_info_data")) == NULL) {
                    FAIL("LLVMGetNamedFunction");
                }
            }

        } else {
            TR(COMPILE_FUNCTION + 4903);
            goto err;
        }

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));


    } else if (strcmp(name, "atoty") == 0) {
        lkit_expr_t **arg;
        lkit_type_t *ty;
        LLVMValueRef fn, args[2];
        bytes_t *typename;
        lkit_dpexpr_t *pa;
        char buf[1024];
        char *mpsuffix;

        arg = array_get(&expr->subs, 0);
        ty = (*arg)->type;

        if (ty->tag != LKIT_PARSER) {
            TR(COMPILE_FUNCTION + 5001);
            goto err;
        }

        arg = array_get(&expr->subs, 1);
        assert(arg != NULL);

        if ((*arg)->type->tag != LKIT_STR) {
            TR(COMPILE_FUNCTION + 5002);
            goto err;
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5003);
            goto err;
        }

        if ((typename = lkit_typename_get(mctx, ty)) == NULL) {
            TR(COMPILE_FUNCTION + 5004);
            goto err;
        }

        if ((pa = lkit_dpexpr_find(mctx, typename, udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5005);
            goto err;
        }

        if (lkit_type_cmp((lkit_type_t *)pa->ty, ty) != 0) {
            TR(COMPILE_FUNCTION + 5006);
            goto err;
        }

        ty = LKIT_PARSER_GET_TYPE(ty);

        mpsuffix = expr->mpolicy == LKIT_MPMPOOL ? "_mpool" : "";

        switch (ty->tag) {
        case LKIT_DICT:
            (void)snprintf(buf,
                           sizeof(buf),
                           "dparse_dict_from_bytes%s",
                           mpsuffix);
            break;

        case LKIT_ARRAY:
            (void)snprintf(buf,
                           sizeof(buf),
                           "dparse_array_from_bytes%s",
                           mpsuffix);
            break;

        default:
            TR(COMPILE_FUNCTION + 5007);
            goto err;
        }

        if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = LLVMConstIntToPtr(
                LLVMConstInt(LLVMInt64TypeInContext(lctx),
                             (uintptr_t)pa,
                             0),
                LLVMTypeOf(LLVMGetParam(fn, 0)));

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));

        v = LLVMBuildPointerCast(builder,
                                 v,
                                 mrklkit_ctx_get_type_backend(mctx, ty),
                                 NEWVAR("cast"));

    } else if (strcmp(name, "new") == 0) {
        LLVMValueRef fn, args[1];
        char buf[1024];
        char *mpsuffix;

        mpsuffix = expr->mpolicy == LKIT_MPMPOOL ? "_mpool" : "";

        switch (expr->type->tag) {
        case LKIT_ARRAY:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_array_new%s",
                           mpsuffix);
            break;

        case LKIT_DICT:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_dict_new%s",
                           mpsuffix);
            break;

        case LKIT_STRUCT:
            (void)snprintf(buf,
                           sizeof(buf),
                           "mrklkit_rt_struct_new%s",
                           mpsuffix);
            break;

        default:
            TR(COMPILE_FUNCTION + 5101);
            goto err;
        }

        if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
            TRACE("cannot find %s", buf);
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = LLVMConstIntToPtr(
                LLVMConstInt(LLVMInt64TypeInContext(lctx),
                             (uintptr_t)expr->type,
                             0),
                LLVMTypeOf(LLVMGetParam(fn, 0)));

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));

        v = LLVMBuildPointerCast(builder,
                                 v,
                                 mrklkit_ctx_get_type_backend(mctx, expr->type),
                                 NEWVAR("cast"));

    } else if (strcmp(name, "set") == 0) {
        //(sym set (func void undef undef undef)) done
        v = lkit_compile_set(mctx,
                             ectx,
                             module,
                             builder,
                             expr,
                             udata);

    } else if (strcmp(name, "del") == 0) {
        //(sym del (func void undef undef)) done
        v = lkit_compile_del(mctx,
                             ectx,
                             module,
                             builder,
                             expr,
                             udata);

    } else if (strcmp(name, "nowi") == 0) {
        LLVMValueRef fn, args[1];

        if ((fn = LLVMGetNamedFunction(module, "time")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = LLVMConstPointerNull(LLVMTypeOf(LLVMGetFirstParam(fn)));

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));

    } else if (strcmp(name, "nowf") == 0) {
        LLVMValueRef fn;

        if ((fn = LLVMGetNamedFunction(module, "mrklkit_timef")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        v = LLVMBuildCall(builder,
                          fn,
                          NULL,
                          0,
                          NEWVAR("call"));

    } else if (strcmp(name, "isnull") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef a;


        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

        if ((a = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5501);
            goto err;
        }
        COMPILE_INCREF_ZREF(module, builder, *arg, a);
        v = LLVMBuildIsNull(builder, a, NEWVAR("isnull"));
        COMPILE_DECREF_ZREF(module, builder, *arg, a);

    } else if (strcmp(name, "traverse") == 0) {
        lkit_expr_t **cont, **cb;
        LLVMValueRef fn, args[2];
        char *fname;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        cb = array_get(&expr->subs, 1);
        assert(cb != NULL);

        if ((args[0] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *cont,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5601);
            goto err;
        }

        if ((*cont)->type->tag == LKIT_ARRAY) {
            fname = "mrklkit_rt_array_traverse";
        } else {
            assert((*cont)->type->tag == LKIT_DICT);
            fname = "mrklkit_rt_dict_traverse";
        }

        if ((fn = LLVMGetNamedFunction(module, fname)) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = LLVMBuildPointerCast(builder,
                                       args[0],
                                       LLVMTypeOf(LLVMGetParam(fn, 0)),
                                       NEWVAR("cast"));

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *cb,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5602);
            goto err;
        }

        args[1] = LLVMBuildPointerCast(builder,
                                       args[1],
                                       LLVMTypeOf(LLVMGetParam(fn, 1)),
                                       NEWVAR("cast"));

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          "");

    } else if (strcmp(name, "addrof") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef ref;
        bytes_t *qual_name;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

        if ((*arg)->type->tag == LKIT_FUNC) {
            ref = find_named_function(ectx, module, (*arg)->name, &qual_name);
        } else {
            ref = find_named_global(ectx, module, (*arg)->name, &qual_name);
        }

        BYTES_DECREF(&qual_name);

        v = LLVMBuildPointerCast(builder,
                                 ref,
                                 mrklkit_ctx_get_type_backend(mctx,
                                     expr->type),
                                 NEWVAR("cast"));

    } else if (strcmp(name, "copy") == 0) {
        if (expr->mpolicy == LKIT_MPMPOOL) {
            lkit_expr_t **arg;

            arg = array_get(&expr->subs, 0);
            assert(arg != NULL);

            if ((v = lkit_compile_expr(mctx,
                                       ectx,
                                       module,
                                       builder,
                                       *arg,
                                       udata)) == NULL) {
                TR(COMPILE_FUNCTION + 5801);
                goto err;
            }

        } else {
            lkit_expr_t **arg;

            arg = array_get(&expr->subs, 0);
            assert(arg != NULL);

            if (!LKIT_TAG_POINTER((*arg)->type->tag)) {
                v = lkit_compile_expr(mctx,
                                      ectx,
                                      module,
                                      builder,
                                      *arg,
                                      udata);
            } else {
                char *fname;
                LLVMValueRef fn, args[1];

                switch ((*arg)->type->tag) {
                case LKIT_STR:
                    fname = "bytes_new_from_bytes";
                    break;

                default:
                    TRACE("copy is not supported for");
                    lkit_expr_dump(*arg);
                    TR(COMPILE_FUNCTION + 5802);
                    goto err;
                }

                if ((args[0] = lkit_compile_expr(mctx,
                                                 ectx,
                                                 module,
                                                 builder,
                                                 *arg,
                                                 udata)) == NULL) {
                    TR(COMPILE_FUNCTION + 5803);
                    goto err;
                }

                if ((fn = LLVMGetNamedFunction(module, fname)) == NULL) {
                    FAIL("LLVMGetNamedFunction");
                }

                args[0] = LLVMBuildPointerCast(builder,
                                               args[0],
                                               LLVMTypeOf(LLVMGetParam(fn, 0)),
                                               NEWVAR("cast"));

                COMPILE_INCREF_ZREF(module, builder, *arg, args[0]);
                v = LLVMBuildCall(builder,
                                  fn,
                                  args,
                                  countof(args),
                                  NEWVAR("call"));
                COMPILE_DECREF_ZREF(module, builder, *arg, args[0]);
            }
        }

    } else {
        mrklkit_module_t **mod;
        array_iter_t it;

        for (mod = array_first(&mctx->modules, &it);
             mod != NULL;
             mod = array_next(&mctx->modules, &it)) {

            if ((*mod)->compile_expr != NULL) {
                v = (*mod)->compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         expr,
                                         udata);
                break;
            }
        }
    }

end:
    return v;

err:
    v = NULL;
    goto end;
}


LLVMValueRef
lkit_compile_expr(mrklkit_ctx_t *mctx,
                  lkit_cexpr_t *ectx,
                  LLVMModuleRef module,
                  LLVMBuilderRef builder,
                  lkit_expr_t *expr,
                  void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v;

    v = NULL;

    lctx = LLVMGetModuleContext(module);

    if (expr->isref) {
        if (expr->value.ref->ismacro) {
            v = lkit_compile_expr(mctx,
                                  ectx,
                                  module,
                                  builder,
                                  expr->value.ref,
                                  udata);
        }


        if (v == NULL) {
            switch (expr->value.ref->type->tag) {
            case LKIT_FUNC:
                assert(builder != NULL);

                /* first try pre-defined functions */
                if ((v = compile_function(mctx,
                                          ectx,
                                          module,
                                          builder,
                                          expr,
                                          udata)) == NULL) {
                    LLVMValueRef fn;

                    /* then user-defined */
                    fn = LLVMGetNamedFunction(module, (char *)expr->name->data);
                    if (fn == NULL) {
                        TRACE("failed to find builtin (normally must be "
                              "mrklkit_rt_* or a standard C library, found %s)",
                              (char *)expr->name->data);
                        TR(LKIT_COMPILE_EXPR + 1);
                        //LLVMDumpModule(module);

                    } else {
                        lkit_expr_t **rand;
                        array_iter_t it;
                        LLVMValueRef *args = NULL;
                        lkit_func_t *tf = (lkit_func_t *)expr->value.ref->type;

                        /* XXX check args */

                        if (tf->fields.elnum != (expr->subs.elnum + 1)) {
                            lkit_type_dump(expr->value.ref->type);
                            lkit_expr_dump(expr);
                            TR(LKIT_COMPILE_EXPR + 2);
                            break;
                        }

                        if ((args = malloc(sizeof(LLVMValueRef) *
                                           expr->subs.elnum)) == NULL) {
                            FAIL("malloc");
                        }

                        for (rand = array_first(&expr->subs, &it);
                             rand != NULL;
                             rand = array_next(&expr->subs, &it)) {
                            args[it.iter] =
                                lkit_compile_expr(mctx,
                                                  ectx,
                                                  module,
                                                  builder,
                                                  *rand,
                                                  udata);

                            assert(args[it.iter] != NULL);

                            COMPILE_INCREF_ZREF(module,
                                                builder,
                                                *rand,
                                                args[it.iter]);

                            /*
                             * "generic" cast
                             */
                            args[it.iter] = LLVMBuildPointerCast(
                                builder,
                                args[it.iter],
                                LLVMTypeOf(LLVMGetParam(fn, it.iter)),
                                NEWVAR("cast"));
                        }

                        if (expr->type->tag == LKIT_VOID) {
                            v = LLVMBuildCall(builder,
                                              fn,
                                              args,
                                              expr->subs.elnum, "");
                        } else {
                            v = LLVMBuildCall(builder,
                                              fn,
                                              args,
                                              expr->subs.elnum,
                                              NEWVAR((char *)expr->name->data));
                        }
                        //LLVMSetTailCall(v, 1);
                        for (rand = array_first(&expr->subs, &it);
                             rand != NULL;
                             rand = array_next(&expr->subs, &it)) {
                            COMPILE_DECREF_ZREF(module,
                                                builder,
                                                *rand,
                                                args[it.iter]);
                        }

                        free(args);
                    }
                }
                break;

            default:
                {
                    char buf[1024];
                    LLVMValueRef ref;
                    LLVMValueRef fn;

                    assert(builder != NULL);

                    if (expr->value.ref->fparam) {
                        LLVMBasicBlockRef bb;

                        bb = LLVMGetInsertBlock(builder);
                        fn = LLVMGetBasicBlockParent(bb);
                        v = LLVMGetParam(fn, expr->value.ref->fparam_idx);

                    } else {
                        bytes_t *qual_name = NULL;

                        ref = find_named_global(ectx,
                                                module,
                                                expr->name,
                                                &qual_name);
                        if (ref == NULL) {
                            LLVMDumpModule(module);
                            TRACE("could not find name: %s",
                                  expr->name->data);
                            lkit_expr_dump(expr);
                            FAIL("find_named_global");
                        }

                        snprintf(buf,
                                 sizeof(buf),
                                 "_mrklkit.%s.init",
                                 (char *)qual_name->data);

                        if ((fn = LLVMGetNamedFunction(module, buf)) != NULL) {
                            if (expr->value.ref->lazy_init) {
                                LLVMValueRef testv, test;
                                LLVMBasicBlockRef currblock,
                                                  nextblock,
                                                  tblock,
                                                  fblock;

                                currblock = LLVMGetInsertBlock(builder);
                                nextblock = LLVMGetNextBasicBlock(currblock);

                                if (nextblock == NULL) {
                                    LLVMValueRef parent;
                                    parent = LLVMGetBasicBlockParent(currblock);
                                    fblock = LLVMAppendBasicBlockInContext(
                                        lctx, parent, NEWVAR("D.if.false"));
                                } else {
                                    fblock = LLVMInsertBasicBlockInContext(
                                        lctx, nextblock, NEWVAR("D.if.false"));
                                }

                                tblock = LLVMInsertBasicBlockInContext(
                                    lctx, fblock, NEWVAR("D.if.true"));

                                snprintf(buf,
                                         sizeof(buf),
                                         "_mrklkit.%s.init.done",
                                         (char *)qual_name->data);

                                if ((testv = LLVMGetNamedGlobal(module,
                                                                buf)) == NULL) {
                                    TRACE("not found: %s", buf);
                                    LLVMDumpModule(module);
                                    FAIL("LLVMGetNamedGlobal");
                                }

                                test = LLVMBuildICmp(
                                        builder,
                                        LLVMIntEQ,
                                        LLVMBuildLoad(builder,
                                            testv,
                                            NEWVAR("test")),
                                        LLVMConstInt(
                                            LLVMInt1TypeInContext(lctx), 0, 0),
                                        NEWVAR("test"));
                                (void)LLVMBuildCondBr(builder,
                                                      test,
                                                      tblock,
                                                      fblock);

                                LLVMPositionBuilderAtEnd(builder, tblock);

                                LLVMBuildCall(builder, fn, NULL, 0, "");
                                (void)LLVMBuildBr(builder, fblock);

                                LLVMPositionBuilderAtEnd(builder, fblock);
                            }
                        }

                        if (v == NULL) {
                            v = LLVMBuildLoad(builder, ref, NEWVAR("load"));
                        }

                        if (expr->type->tag == LKIT_STR &&
                                LKIT_EXPR_CONSTANT(expr)) {
                            v = LLVMBuildPointerCast(
                                    builder,
                                    v,
                                    mrklkit_ctx_get_type_backend(
                                        mctx, expr->type), NEWVAR("cast"));
                        }
                        BYTES_DECREF(&qual_name);
                    }
                }
                break;
            }
        }

    } else {
        if (expr->value.literal != NULL) {
            switch (expr->type->tag) {
            case LKIT_INT:
            case LKIT_INT_MIN:
            case LKIT_INT_MAX:
                {
                    int64_t *pv = (int64_t *)expr->value.literal->body;
                    v = LLVMConstInt(mrklkit_ctx_get_type_backend(mctx,
                                                                  expr->type),
                                     *pv,
                                     1);
                }
                break;

            case LKIT_FLOAT:
            case LKIT_FLOAT_MIN:
            case LKIT_FLOAT_MAX:
                {
                    double *pv = (double *)expr->value.literal->body;
                    v = LLVMConstReal(mrklkit_ctx_get_type_backend(mctx,
                                                                   expr->type),
                                      *pv);
                }
                break;

            case LKIT_BOOL:
                v = LLVMConstInt(mrklkit_ctx_get_type_backend(mctx,
                                                              expr->type),
                                 expr->value.literal->body[0],
                                 0);
                break;

            case LKIT_STR:
                {
                    bytes_t *b;
#ifdef DO_MEMDEBUG
                    LLVMValueRef binit[5], vv;

                    assert(sizeof(bytes_t) == 4 * sizeof(uint64_t));
                    b = (bytes_t *)expr->value.literal->body;
                    binit[0] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            0x0, 0);
                    binit[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            0xdada, 0);
                    binit[2] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            b->sz, 0);
                    binit[3] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            bytes_hash(b), 0);
                    binit[4] = LLVMConstStringInContext(lctx,
                                                        (char *)b->data,
                                                        b->sz,
                                                        1);
#else
                    LLVMValueRef binit[4], vv;

                    assert(sizeof(bytes_t) == 3 * sizeof(uint64_t));
                    b = (bytes_t *)expr->value.literal->body;
                    binit[0] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            0xdada, 0);
                    binit[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            b->sz, 0);
                    binit[2] = LLVMConstInt(LLVMInt64TypeInContext(lctx),
                                            bytes_hash(b), 0);
                    binit[3] = LLVMConstStringInContext(lctx,
                                                        (char *)b->data,
                                                        b->sz,
                                                        1);
#endif
                    vv = LLVMConstStructInContext(lctx,
                                                  binit,
                                                  countof(binit),
                                                  0);
                    v = LLVMAddGlobal(module,
                                      LLVMTypeOf(vv),
                                      NEWVAR(".mrklkit.val"));
                    LLVMSetInitializer(v, vv);
                    LLVMSetLinkage(v, LLVMPrivateLinkage);
#if LLVM_VERSION_NUM >= 3005
                    LLVMSetUnnamedAddr(v, 1);
#endif
                    v = LLVMBuildPointerCast(builder,
                                             v,
                                             mrklkit_ctx_get_type_backend(
                                                 mctx, expr->type),
                                             NEWVAR("cast"));
                }
                break;

            case LKIT_NULL:
                v = LLVMConstPointerNull(
                        mrklkit_ctx_get_type_backend(mctx, expr->type));

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:
            case LKIT_VOID:
                break;

            default:
                lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(LKIT_COMPILE_EXPR + 6);
                break;

            }
        } else {
            lkit_expr_t **psub;
            array_iter_t it;

            for (psub = array_first(&expr->subs, &it);
                 psub != NULL;
                 psub = array_next(&expr->subs, &it)) {
                if (lkit_compile_expr(mctx,
                                      ectx,
                                      module,
                                      builder,
                                      *psub,
                                      udata) == NULL) {
                    TR(LKIT_COMPILE_EXPR + 7);
                    break;
                }
            }

            //TRACE("null literal in expr (compiling retval to void pointer):");
            //lkit_expr_dump(expr);

            v = LLVMConstPointerNull(
                    mrklkit_ctx_get_type_backend(mctx, expr->type));
        }
    }

    return v;
}


static int
compile_dynamic_initializer(mrklkit_ctx_t *mctx,
                            lkit_cexpr_t *ectx,
                            LLVMModuleRef module,
                            bytes_t *name,
                            lkit_expr_t *expr,
                            void *udata)
{
    char buf[1024];
    LLVMContextRef lctx;
    LLVMBasicBlockRef bb;
    LLVMBuilderRef builder;
    LLVMValueRef v, fn, storedv, torfn;
    char *torprefix;

    v = NULL;

    switch (expr->type->tag) {
    case LKIT_STR:
        torprefix = "mrklkit_rt_bytes_";
        break;
    case LKIT_ARRAY:
        torprefix = "mrklkit_rt_array_";
        break;
    case LKIT_DICT:
        torprefix = "mrklkit_rt_dict_";
        break;
    case LKIT_STRUCT:
        torprefix = "mrklkit_rt_struct_";
        break;
    default:
        torprefix = NULL;
    }
    /* dynamic initializer */
    lctx = LLVMGetModuleContext(module);

    snprintf(buf, sizeof(buf), "_mrklkit.%s.init", (char *)name->data);

    fn = LLVMAddFunction(module,
                         buf,
                         LLVMFunctionType(LLVMVoidTypeInContext(lctx),
                                          NULL,
                                          0,
                                          0));
    builder = LLVMCreateBuilderInContext(lctx);
    bb = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR("D.dyninit"));
    LLVMPositionBuilderAtEnd(builder, bb);

    if ((storedv = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     expr,
                                     udata)) == NULL) {
        TRRET(COMPILE_DYNAMIC_INITIALIZER + 1);
    }

    if (expr->type->tag != LKIT_VOID) {
        v = LLVMAddGlobal(module, LLVMTypeOf(storedv), (char *)name->data);
        if (LKIT_TAG_POINTER(expr->type->tag)) {
            LLVMSetInitializer(v, LLVMConstPointerNull(LLVMTypeOf(storedv)));
            //LLVMSetInitializer(v, LLVMGetUndef(LLVMTypeOf(storedv)));
        } else {
            LLVMSetInitializer(v, LLVMGetUndef(LLVMTypeOf(storedv)));
        }

        /*
         * decref old value
         */
        if (expr->mpolicy != LKIT_MPMPOOL &&
            torprefix != NULL &&
            expr->type->tag != LKIT_VOID) {
            LLVMValueRef tmp;

            (void)snprintf(buf, sizeof(buf), "%sdecref", torprefix);
            if ((torfn = LLVMGetNamedFunction(module, buf)) == NULL) {
                TRACE("no such function: %s", buf);
                FAIL("compile_dynamic_initializer");
            }
            tmp = LLVMBuildPointerCast(builder,
                                       v,
                                       LLVMTypeOf(LLVMGetParam(torfn, 0)),
                                       NEWVAR("cast"));
            LLVMBuildCall(builder, torfn, &tmp, 1, "");
        }
        LLVMBuildStore(builder, storedv, v);
    }

    if (expr->lazy_init) {
        LLVMValueRef testv;

        snprintf(buf, sizeof(buf), "_mrklkit.%s.init.done", (char *)name->data);
        testv = LLVMAddGlobal(module, LLVMInt1TypeInContext(lctx), buf);
        LLVMSetLinkage(testv, LLVMPrivateLinkage);
        LLVMSetInitializer(testv,
                           LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0));


        if (expr->type->tag != LKIT_VOID) {
            LLVMBuildStore(builder,
                           LLVMConstInt(LLVMInt1TypeInContext(lctx), 1, 0),
                           testv);
        }
    }

    /*
     * incref
     */
    if (expr->mpolicy != LKIT_MPMPOOL &&
        torprefix != NULL &&
        expr->type->tag != LKIT_VOID) {

        assert(expr->zref == 0);

        (void)snprintf(buf, sizeof(buf), "%sincref", torprefix);

        if ((torfn = LLVMGetNamedFunction(module, buf)) == NULL) {
            TRACE("no such function: %s", buf);
            FAIL("compile_dynamic_initializer");
        }

        storedv = LLVMBuildPointerCast(builder,
                                       storedv,
                                       LLVMTypeOf(LLVMGetParam(torfn, 0)),
                                       NEWVAR("cast"));
        LLVMBuildCall(builder, torfn, &storedv, 1, "");
    }

    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    if (expr->type->tag != LKIT_VOID) {
        LLVMSetLinkage(v, LLVMPrivateLinkage);
        LLVMSetLinkage(fn, LLVMLinkOnceODRLinkage);
    }
    return 0;
}


static int
call_eager_initializer(lkit_gitem_t **gitem, void *udata)
{
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;
    struct {
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } *params = udata;
    char buf[1024];
    LLVMValueRef fn;

    snprintf(buf, sizeof(buf), "_mrklkit.%s.init", (char *)name->data);

    if ((fn = LLVMGetNamedFunction(params->module, buf)) != NULL) {
        if (!expr->lazy_init) {
            if (expr->referenced) {
                if (LLVMBuildCall(params->builder,
                                  fn,
                                  NULL,
                                  0,
                                  "") == NULL) {
                    TR(CALL_EAGER_INITIALIZER + 1);
                }
            }
        } else {
            //TRACE("not calling");
        }
    } else {
        //TRACE("not found");
    }
    return 0;
}


static int
call_finalizer(lkit_gitem_t **gitem, void *udata)
{
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;
    struct {
        lkit_cexpr_t *ectx;
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } *params = udata;

    if (expr->ismacro) {
        //TRACE("macro cannot be finalized: %s", name->data);
        return 0;
    }

    if (expr->referenced && expr->isref) {
        char buf[1024];
        LLVMContextRef lctx;
        LLVMValueRef v, dtor;

        if (expr->lazy_init) {
            lctx = LLVMGetModuleContext(params->module);

            snprintf(buf,
                     sizeof(buf),
                     "_mrklkit.%s.init.done",
                     (char *)name->data);

            if ((v = LLVMGetNamedGlobal(params->module, buf)) == NULL) {
                FAIL("call_finalizer");
            }

            LLVMBuildStore(params->builder,
                           LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0),
                           v);
        }


        if (expr->mpolicy != LKIT_MPMPOOL && expr->type->tag != LKIT_VOID) {
            bytes_t *qual_name;

            qual_name = NULL;
            v = find_named_global(params->ectx,
                                  params->module,
                                  name,
                                  &qual_name);
            BYTES_DECREF(&qual_name);

            //if ((v = LLVMGetNamedGlobal(params->module,
            //                            (char *)name->data)) == NULL) {
            //    TRACE("cannot find %s", name->data);
            //    FAIL("call_finalizer");
            //}

            switch (expr->type->tag) {
            case LKIT_STR:
                (void)snprintf(buf, sizeof(buf), "mrklkit_rt_bytes_decref");
                break;
            case LKIT_ARRAY:
                (void)snprintf(buf, sizeof(buf), "mrklkit_rt_array_recref");
                break;
            case LKIT_DICT:
                (void)snprintf(buf, sizeof(buf), "mrklkit_rt_dict_decref");
                break;
            case LKIT_STRUCT:
                (void)snprintf(buf, sizeof(buf), "mrklkit_rt_struct_decref");
                break;
            default:
                goto dtor_done;
                break;
            }

            if ((dtor = LLVMGetNamedFunction(params->module, buf)) == NULL) {
                TRACE("missing symbol: %s", buf);
                FAIL("call_finalizer");
            }

            v = LLVMBuildPointerCast(params->builder,
                                     v,
                                     LLVMTypeOf(LLVMGetParam(dtor, 0)),
                                     NEWVAR("cast"));

            LLVMBuildCall(params->builder, dtor, &v, 1, "");
dtor_done:
            ;
        }
    }

    return 0;
}


static int
_compile(lkit_gitem_t **gitem, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        lkit_cexpr_t *ectx;
        LLVMModuleRef module;
        void *udata;
    } *params = udata;
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;

    if (expr->isbuiltin) {
        return 0;
    }

    if (expr->ismacro) {
        return 0;
    }

    if (expr->isref) {
        if (compile_dynamic_initializer(params->mctx,
                                        params->ectx,
                                        params->module,
                                        name,
                                        expr,
                                        params->udata) != 0) {
            TRRET(SYM_COMPILE + 1);
        }
    } else {
        if (expr->value.literal != NULL) {
            LLVMValueRef initv, v;

            initv = lkit_compile_expr(params->mctx,
                                      params->ectx,
                                      params->module,
                                      NULL,
                                      expr,
                                      params->udata);
            assert(initv != NULL);
            v = LLVMAddGlobal(params->module,
                              LLVMTypeOf(initv),
                              (char *)name->data);
            LLVMSetLinkage(v, LLVMPrivateLinkage);
            LLVMSetInitializer(v, initv);
        } else {
            LLVMValueRef fn;

            /* declaration, */
            switch (expr->type->tag) {
            case LKIT_FUNC:
                fn = LLVMAddFunction(params->module,
                                     (char *)name->data,
                                     mrklkit_ctx_get_type_backend(
                                         params->mctx, expr->type));
                LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);

                /* and definition */
                if (expr->subs.elnum) {
                    lkit_expr_t **body;
                    bytes_t *s;
                    mrklkit_modaux_t *modaux;
                    char *error_msg;
                    LLVMContextRef lctx;

                    if ((body = array_get(&expr->subs, 0)) == NULL) {
                        FAIL("array_get");
                    }
                    assert(*body != NULL);

                    lctx = LLVMGetModuleContext(params->module);

                    /* crazy ir case */
                    if ((*body)->type->tag == LKIT_IR) {
                        if ((modaux =
                                array_incr(&params->mctx->modaux)) == NULL) {
                            FAIL("array_incr");
                        }

                        s = (bytes_t *)((*body)->value.literal->body);
                        //D8(s->data, s->sz);

                        if ((modaux->buf =
                                LLVMCreateMemoryBufferWithMemoryRange(
                                    (char *)s->data,
                                    s->sz - 1,
                                    NEWVAR("aux"), 0)) == NULL) {
                            FAIL("LLVMCreateMemoryBufferWithMemoryRange");
                        }
                        if (LLVMParseIRInContext(lctx,
                                                 modaux->buf,
                                                 &modaux->module,
                                                 &error_msg) != 0) {
                            TRACE("%s", error_msg);
                            LLVMDisposeMessage(error_msg);
                            error_msg = NULL;
                            TRRET(SYM_COMPILE + 3);
                        }
                    } else {
                        lkit_expr_t **pbody;
                        LLVMBuilderRef builder;
                        LLVMBasicBlockRef bb;
                        LLVMValueRef v = NULL;
                        array_iter_t it;

                        builder = LLVMCreateBuilderInContext(lctx);
                        bb = LLVMAppendBasicBlockInContext(lctx,
                                                           fn,
                                                           NEWVAR("F.start"));
                        LLVMPositionBuilderAtEnd(builder, bb);

                        for (pbody = array_first(&expr->subs, &it);
                             pbody != NULL;
                             pbody = array_next(&expr->subs, &it)) {
                            if (!(*pbody)->isref &&
                                    (*pbody)->value.literal == NULL) {
                                continue;
                            }
                            if ((v = lkit_compile_expr(
                                            params->mctx,
                                            params->ectx,
                                            params->module,
                                            builder,
                                            *pbody,
                                            params->udata)) == NULL) {
                                TRRET(SYM_COMPILE + 1);
                            }
                        }

                        if (lkit_expr_type_of(expr)->tag == LKIT_VOID) {
                            LLVMBuildRetVoid(builder);
                        } else {
                            LLVMBuildRet(builder, v);
                        }
                        LLVMDisposeBuilder(builder);
                    }
                }
                break;

            default:
                fn = LLVMAddGlobal(params->module,
                                   mrklkit_ctx_get_type_backend(
                                       params->mctx, expr->type),
                                   (char *)name->data);
                //FAIL("compile_decl: not implemented");
            }
        }
    }

    return 0;
}


int
lkit_expr_ctx_compile(mrklkit_ctx_t *mctx,
                      lkit_cexpr_t *ectx,
                      LLVMModuleRef module,
                      void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        lkit_cexpr_t *ectx;
        LLVMModuleRef module;
        void *udata;
    } params_compile = { mctx, ectx, module, udata };

    if (array_traverse(&ectx->glist,
                       (array_traverser_t)_compile,
                       &params_compile) != 0) {
        TRRET(LKIT_EXPR_CTX_COMPILE + 1);
    }
    return 0;
}


int
lkit_expr_ctx_compile_pre(lkit_cexpr_t *ectx,
                          LLVMModuleRef module,
                          LLVMBuilderRef builder)
{
    struct {
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } params = {module, builder};
    if (array_traverse(&ectx->glist,
                       (array_traverser_t)call_eager_initializer,
                       &params) != 0) {
        TRRET(LKIT_EXPR_CTX_CALL_EAGER_INITIALIZERS + 3);
    }
    return 0;
}



int
lkit_expr_ctx_compile_post(lkit_cexpr_t *ectx,
                           LLVMModuleRef module,
                           LLVMBuilderRef builder)
{
    struct {
        lkit_cexpr_t *ectx;
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } params = {ectx, module, builder};
    if (array_traverse(&ectx->glist,
                       (array_traverser_t)call_finalizer,
                       &params) != 0) {
        TRRET(LKIT_EXPR_CTX_COMPILE_POST + 1);
    }
    return 0;
}

