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
#include <mrklkit/lexpr.h>
#include <mrklkit/builtin.h>
#include <mrklkit/module.h>

#include "diag.h"


/**
 * LLVM IR emitter.
 */
static LLVMValueRef
compile_if(mrklkit_ctx_t *mctx,
           lkit_expr_t *ectx,
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
    LLVMBuildCondBr(builder, cond, tblock, fblock);

    /**/
    LLVMPositionBuilderAtEnd(builder, tblock);

    if (texpr != NULL) {
        texp = lkit_compile_expr(mctx, ectx, module, builder, texpr, udata);
        assert(texp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    /**/
    LLVMPositionBuilderAtEnd(builder, fblock);

    if (fexpr != NULL) {
        fexp = lkit_compile_expr(mctx, ectx, module, builder, fexpr, udata);
        assert(fexp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    LLVMPositionBuilderAtEnd(builder, endblock);
    if (restype != NULL) {
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
    }

    return v;
}


static LLVMValueRef
_compile_cmp(mrklkit_ctx_t *mctx,
             lkit_expr_t *ectx,
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
                          LLVMBuildCast(builder, LLVMZExt, va, ty, NEWVAR("cast")),
                          LLVMBuildCast(builder, LLVMZExt, vb, ty, NEWVAR("cast")),
                          NEWVAR("cmp"));

    } else if (a->type->tag == LKIT_FLOAT ||
               a->type->tag == LKIT_FLOAT_MIN ||
               a->type->tag == LKIT_FLOAT_MAX) {

        v = LLVMBuildFCmp(builder, rp, va, vb, NEWVAR("cmp"));

    } else if (a->type->tag == LKIT_STR) {
        LLVMValueRef fn, args[2], rv;

        if ((fn = LLVMGetNamedFunction(module, "bytes_cmp")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = va;
        args[1] = vb;
        LLVMTypeRef ty;

        ty = LLVMInt8TypeInContext(lctx);
        rv = LLVMBuildCall(builder,
                          fn,
                          args,
                          2,
                          NEWVAR("strcmp"));
        rv = LLVMBuildCast(builder, LLVMTrunc, rv, ty, NEWVAR("cast"));
        v = LLVMBuildICmp(builder,
                          ip,
                          rv,
                          LLVMConstInt(ty, 0, 0),
                          NEWVAR("cmp"));

    } else {
        TR(COMPILE_CMP + 4);
        goto end;
    }

end:
    return v;
}


static LLVMValueRef
compile_cmp(mrklkit_ctx_t *mctx,
            lkit_expr_t *ectx,
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
                 lkit_expr_t *ectx,
                 LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr,
                 int flag,
                 void *udata)
{
    LLVMContextRef lctx;
    LLVMValueRef v = NULL;
    lkit_expr_t **arg, **cont;
    LLVMValueRef fn, args[3];
    char *name = (char *)expr->name->data;

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
        TR(COMPILE_GET + 1);
        goto err;
    }

    if ((*cont)->type->tag != LKIT_STRUCT) {
        /* default */
        if ((arg = array_get(&expr->subs, 2)) == NULL) {
            FAIL("array_get");
        }
        if ((args[2] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_GET + 2);
            goto err;
        }
    }

    switch ((*cont)->type->tag) {
    case LKIT_ARRAY:
        /* idx */
        {
            char buf[64];
            lkit_type_t *fty;

            if ((arg = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *arg,
                                             udata)) == NULL) {
                TR(COMPILE_GET + 100);
                goto err;
            }

            if ((fty = lkit_array_get_element_type(
                        (lkit_array_t *)(*cont)->type)) == NULL) {
                TR(COMPILE_GET + 102);
                goto err;
            }
            if (flag & COMPILE_GET_GET) {
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_array_item_%s",
                         fty->name);
            } else {
                TR(COMPILE_GET + 103);
                goto err;
            }

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));


            v = LLVMBuildCall(builder, fn, args, 3, NEWVAR(name));
        }
        break;

    case LKIT_DICT:
        /* key */
        {
            char buf[64];
            lkit_type_t *fty;

            if ((arg = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }

            if ((args[1] = lkit_compile_expr(mctx,
                                             ectx,
                                             module,
                                             builder,
                                             *arg,
                                             udata)) == NULL) {
                TR(COMPILE_GET + 200);
                goto err;
            }

            if ((fty = lkit_dict_get_element_type(
                        (lkit_dict_t *)(*cont)->type)) == NULL) {
                TR(COMPILE_GET + 201);
                goto err;
            }

            if (flag & COMPILE_GET_GET) {
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_dict_item_%s",
                         fty->name);
            } else {
                TR(COMPILE_GET + 202);
                goto err;
            }

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }

            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));
            v = LLVMBuildCall(builder, fn, args, 3, NEWVAR(name));
        }
        break;

    case LKIT_STRUCT:
        /* idx */
        {
            int idx;
            lkit_type_t *fty;
            char buf[64];

            if ((arg = array_get(&expr->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            assert((*arg)->type->tag == LKIT_STR &&
                   !(*arg)->isref &&
                   (*arg)->value.literal != NULL);

            if ((idx = lkit_struct_get_field_index(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == -1) {
                TR(COMPILE_GET + 300);
                goto err;
            }

            if ((fty = lkit_struct_get_field_type(
                        (lkit_struct_t *)(*cont)->type,
                        (bytes_t *)(*arg)->value.literal->body)) == NULL) {
                TR(COMPILE_GET + 301);
                goto err;
            }

            if (flag & COMPILE_GET_GET) {
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_struct_item_%s",
                         fty->name);
            } else {
                lkit_struct_t *ts;

                ts = (lkit_struct_t *)(*cont)->type;

                if (ts->parser == LKIT_PARSER_NONE ||
                    ts->parser == LKIT_PARSER_DELIM ||
                    ts->parser == LKIT_PARSER_MDELIM) {

                    (void)snprintf(buf,
                                   sizeof(buf),
                                   "dparse_struct_item_ra_%s",
                                   fty->name);

                } else {
                    //TRACE("ts->parser=%d", ts->parser);
                    //lkit_type_dump((*cont)->type);
                    TR(COMPILE_GET + 302);
                    goto err;
                }
            }

            args[1] = LLVMConstInt(LLVMInt64TypeInContext(lctx), idx, 1);

            if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            args[0] = LLVMBuildPointerCast(builder,
                                           args[0],
                                           LLVMTypeOf(LLVMGetParam(fn, 0)),
                                           NEWVAR("cast"));
            v = LLVMBuildCall(builder, fn, args, 2, NEWVAR(name));
        }

        break;

    default:
        FAIL("compile_get");
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
                 lkit_expr_t *ectx,
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
    //if ((bifn = LLVMGetNamedFunction(module,
    //                                 "bytes_incref")) == NULL) {
    //    FAIL("LLVMGetNamedFunction");
    //}
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
        asz[it.iter] = LLVMBuildStructGEP(builder,
                                          av[it.iter],
                                          BYTES_SZ_IDX,
                                          NEWVAR("gep"));
        asz[it.iter] = LLVMBuildLoad(builder, asz[it.iter], NEWVAR("load"));
        accum = LLVMBuildAdd(builder, accum, asz[it.iter], NEWVAR("plus"));
        // minus intermediate zero term
        accum = LLVMBuildSub(builder, accum, const1, NEWVAR("dec"));
        //(void)LLVMBuildCall(builder, bifn, av + it.iter, 1, NEWVAR("call"));
    }
    // plus final zero term
    accum = LLVMBuildAdd(builder, accum, const1, NEWVAR("plus"));

    if ((bnfn = LLVMGetNamedFunction(module,
                                     "mrklkit_rt_bytes_new_gc")) == NULL) {
        FAIL("LLVMGetNamedFunction");
    }
    if ((bcfn = LLVMGetNamedFunction(module,
                                     "bytes_copyz")) == NULL) {
        FAIL("LLVMGetNamedFunction");
    }
    //if ((bdfn = LLVMGetNamedFunction(module,
    //                                 "bytes_decref_fast")) == NULL) {
    //    FAIL("LLVMGetNamedFunction");
    //}
    v = LLVMBuildCall(builder, bnfn, &accum, 1, NEWVAR("call"));
    tmp = const0;
    for (i = 0; i < expr->subs.elnum; ++i) {
        LLVMValueRef args[3];

        args[0] = v;
        args[1] = av[i];
        args[2] = tmp;
        (void)LLVMBuildCall(builder, bcfn, args, countof(args), NEWVAR("call"));
        //(void)LLVMBuildCall(builder, bdfn, args + 1, 1, NEWVAR("call"));
        tmp = LLVMBuildAdd(builder, tmp, asz[i], NEWVAR("plus"));
        tmp = LLVMBuildSub(builder, tmp, const1, NEWVAR("dec")); /* zero term */
    }
    free(av);
    free(asz);

    return v;
}


#define COMPILE_STRSTR_START 0
#define COMPILE_STRSTR_END 1
UNUSED static LLVMValueRef
compile_strstr(mrklkit_ctx_t *mctx,
               lkit_expr_t *ectx,
               LLVMContextRef lctx,
               LLVMModuleRef module,
               LLVMBuilderRef builder,
               lkit_expr_t *expr,
               int flag,
               void *udata)
{
    const char *fnname;
    lkit_expr_t **arg;
    LLVMValueRef a0, a1;
    LLVMValueRef fn, fnparam, args[2], fnrv, tmp, tmp1;

    if (flag == COMPILE_STRSTR_END) {
        fnname = "mrklkit_strrstr";
    } else {
        fnname = "strstr";
    }
    if ((fn = LLVMGetNamedFunction(module, fnname)) == NULL) {
        FAIL("LLVMGetNamedFunction");
    }

    arg = array_get(&expr->subs, 0);
    assert(arg != NULL);
    a0 = lkit_compile_expr(mctx, ectx, module, builder, *arg, udata);
    assert(a0 != NULL);
    tmp = LLVMBuildStructGEP(builder, a0, BYTES_DATA_IDX, NEWVAR("gep"));
    fnparam = LLVMGetParam(fn, 0);
    args[0] = LLVMBuildPointerCast(builder,
                                   tmp,
                                   LLVMTypeOf(fnparam),
                                   NEWVAR("cast"));

    arg = array_get(&expr->subs, 1);
    assert(arg != NULL);
    a1 = lkit_compile_expr(mctx, ectx, module, builder, *arg, udata);
    assert(a1 != NULL);
    tmp = LLVMBuildStructGEP(builder, a1, BYTES_DATA_IDX, NEWVAR("gep"));
    fnparam = LLVMGetParam(fn, 1);
    args[1] = LLVMBuildPointerCast(builder,
                                   tmp,
                                   LLVMTypeOf(fnparam),
                                   NEWVAR("cast"));

    /*
     * p = strstr(a, b);
     */
    fnrv = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
    tmp = LLVMBuildPtrToInt(builder,
                            fnrv,
                            LLVMInt64TypeInContext(lctx),
                            NEWVAR("cast"));
    tmp1 = LLVMBuildPtrToInt(builder,
                             args[0],
                             LLVMInt64TypeInContext(lctx),
                             NEWVAR("cast"));

    if (flag == COMPILE_STRSTR_END) {
        LLVMValueRef const1, sz;

        /*
         * p = p - len(a) + len(b);
         */
        const1 = LLVMConstInt(LLVMInt64TypeInContext(lctx), 1, 0);

        sz = LLVMBuildStructGEP(builder, a0, BYTES_SZ_IDX, NEWVAR("gep"));
        sz = LLVMBuildLoad(builder, sz, NEWVAR("load"));
        sz = LLVMBuildSub(builder, sz, const1, NEWVAR("dec")); /* zero term */
        tmp = LLVMBuildSub(builder, tmp, sz, NEWVAR("minus"));

        sz = LLVMBuildStructGEP(builder, a1, BYTES_SZ_IDX, NEWVAR("gep"));
        sz = LLVMBuildLoad(builder, sz, NEWVAR("load"));
        sz = LLVMBuildSub(builder, sz, const1, NEWVAR("dec")); /* zero term */
        tmp = LLVMBuildAdd(builder, tmp, sz, NEWVAR("plus"));
    }
    /*
     * p == a
     */
    return LLVMBuildICmp(builder, LLVMIntEQ, tmp, tmp1, NEWVAR("test"));
}



static LLVMValueRef
compile_function(mrklkit_ctx_t *mctx,
                 lkit_expr_t *ectx,
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
               strcmp(name, "get-index") == 0 || /* compat */
               strcmp(name, "get-key") == 0 /* compat */
              ) {
        //(sym get (func undef undef undef under)) done
        v = lkit_compile_get(mctx,
                             ectx,
                             module,
                             builder,
                             expr,
                             COMPILE_GET_GET,
                             udata);

    } else if (strcmp(name, "parse") == 0) {
        //(sym parse (func undef undef undef under)) done
        v = lkit_compile_get(mctx,
                             ectx,
                             module,
                             builder,
                             expr,
                             COMPILE_GET_PARSE,
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
        lkit_expr_t **arg;
        // (builtin atoi (func int str int))


        if ((fn = LLVMGetNamedFunction(module, "mrklkit_strtoi64")) == NULL) {
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
            TR(COMPILE_FUNCTION + 1601);
            goto err;
        }

        if ((arg = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *arg,
                                     udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1602);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

    } else if (strcmp(name, "atoi-loose") == 0) {
        LLVMValueRef fn, args[1];
        lkit_expr_t **arg;
        // (builtin atoi (func int str))


        if ((fn = LLVMGetNamedFunction(module, "mrklkit_strtoi64_loose")) == NULL) {
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

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

    } else if (strcmp(name, "atof") == 0) {
        LLVMValueRef fn, args[2];
        lkit_expr_t **arg;
        // (builtin atof (func float str float))


        if ((fn = LLVMGetNamedFunction(module, "mrklkit_strtod")) == NULL) {
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
            TR(COMPILE_FUNCTION + 1701);
            goto err;
        }

        if ((arg = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *arg,
                                     udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1702);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

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

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

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
        lkit_expr_t **arg;
        LLVMValueRef fn, args[2];

        if ((fn = LLVMGetNamedFunction(module, "bytes_startswith")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        arg = array_get(&expr->subs, 0);
        if ((args[0] = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1950);
            goto err;
        }
        arg = array_get(&expr->subs, 1);
        if ((args[1] = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1951);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

    } else if (strcmp(name , "endswith") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef fn, args[2];

        if ((fn = LLVMGetNamedFunction(module, "bytes_endswith")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        arg = array_get(&expr->subs, 0);
        if ((args[0] = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1960);
            goto err;
        }
        arg = array_get(&expr->subs, 1);
        if ((args[1] = lkit_compile_expr(mctx,
                                   ectx,
                                   module,
                                   builder,
                                   *arg,
                                   udata)) == NULL) {
            TR(COMPILE_FUNCTION + 1961);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
#if 0
    } else if (strcmp(name , "startswith") == 0) {
        v = compile_strstr(mctx,
                           ectx,
                           lctx,
                           module,
                           builder,
                           expr,
                           COMPILE_STRSTR_START,
                           udata);

    } else if (strcmp(name , "endswith") == 0) {
        v = compile_strstr(mctx,
                           ectx,
                           lctx,
                           module,
                           builder,
                           expr,
                           COMPILE_STRSTR_END,
                           udata);
#endif

    } else if (strcmp(name , "tostr") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;
        LLVMValueRef fn, fnarg;

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

        switch ((*arg)->type->tag) {
        case LKIT_INT:
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_new_from_int_gc")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            break;

        case LKIT_FLOAT:
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_new_from_float_gc")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            break;

        case LKIT_BOOL:
            if ((fn = LLVMGetNamedFunction(module,
                        "mrklkit_rt_bytes_new_from_bool_gc")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }
            break;

        case LKIT_STR:
            v = fnarg;
            goto tostr_done;
            break;

        default:
            lkit_expr_dump(expr);
            FAIL("compile_function, tostr argument type is not supported");
        }

        v = LLVMBuildCall(builder, fn, &fnarg, 1, NEWVAR("call"));
tostr_done:
        ;


    } else if (strcmp(name, "in") == 0) {
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
                endblock = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR("IN.end"));
            } else {
                endblock = LLVMInsertBasicBlockInContext(lctx,
                                                         nextblock,
                                                         NEWVAR("IN.end"));
            }

            LLVMPositionBuilderAtEnd(builder, endblock);
            v = LLVMBuildPhi(builder, LLVMInt1TypeInContext(lctx), NEWVAR("phi"));

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
                LLVMBuildCondBr(builder, tmp, endblock, testblock);
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
        lkit_expr_t **arg;
        LLVMValueRef fn, args[3];
        //(sym substr (func str str int int))

        if ((fn = LLVMGetNamedFunction(module,
                    "mrklkit_rt_bytes_slice_gc")) == NULL) {
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
            TR(COMPILE_FUNCTION + 2201);
            goto err;
        }

        if ((arg = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        if ((args[1] = lkit_compile_expr(mctx,
                                     ectx,
                                     module,
                                     builder,
                                     *arg,
                                     udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2202);
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
            TR(COMPILE_FUNCTION + 2203);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

    } else if (strcmp(name, "strfind") == 0) {
        FAIL("compile_function, not supported: strfind");

    } else if (strcmp(name, "split") == 0) {
        lkit_expr_t **arg;
        lkit_array_t *ty;
        LLVMValueRef fn, args[3];

        /*
         * very slow b/c array_incr_mpool(), use parse
         */

        //(sym split (func (array str) str str))
        ty = lkit_type_get_array(mctx, LKIT_STR);
        if ((fn = LLVMGetNamedFunction(module,
                    "mrklkit_rt_array_split_gc")) == NULL) {
            FAIL("LLVMGetNamedFunction");
        }

        args[0] = LLVMConstIntToPtr(
                LLVMConstInt(LLVMInt64TypeInContext(lctx), (uintptr_t)ty, 0),
                LLVMTypeOf(LLVMGetParam(fn, 0)));

        arg = array_get(&expr->subs, 0);
        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2301);
            goto err;
        }

        arg = array_get(&expr->subs, 1);
        if ((args[2] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 2302);
            goto err;
        }

        v = LLVMBuildCall(builder,
                          fn,
                          args,
                          countof(args),
                          NEWVAR("call"));

    } else if (strcmp(name, "brushdown") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef fn, args[1];

        //(sym split (func (array str) str str))
        if ((fn = LLVMGetNamedFunction(module,
                    "mrklkit_rt_bytes_brushdown_gc")) == NULL) {
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
            TR(COMPILE_FUNCTION + 2401);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

    } else if (strcmp(name, "urldecode") == 0) {
        lkit_expr_t **arg;
        LLVMValueRef fn, args[1];

        //(sym split (func (array str) str str))
        if ((fn = LLVMGetNamedFunction(module,
                    "mrklkit_rt_bytes_urldecode_gc")) == NULL) {
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
            TR(COMPILE_FUNCTION + 2501);
            goto err;
        }

        v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));

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
                                               //LLVMInt8TypeInContext(lctx), 0),
                                               LLVMVoidTypeInContext(lctx), 0),
                                           NEWVAR("cast"));

            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
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
            v = LLVMBuildStructGEP(builder, tmp, BYTES_SZ_IDX, NEWVAR("gep"));
            v = LLVMBuildLoad(builder, v, NEWVAR("load"));
            const1 = LLVMConstInt(LLVMInt64TypeInContext(lctx), 1, 0);
            v = LLVMBuildSub(builder, v, const1, NEWVAR("dec")); /* zero term */
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
                                               //LLVMInt8TypeInContext(lctx), 0),
                                               LLVMVoidTypeInContext(lctx), 0),
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
            v = LLVMBuildCall(builder, fn, args, countof(args), NEWVAR("call"));
            break;

        default:
            FAIL("compile_function");
        }

    } else if (strcmp(name, "dp-info") == 0) {
        lkit_expr_t **cont, **opt;
        bytes_t *optname;
        LLVMValueRef ref, args[1];

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
                                           //LLVMInt8TypeInContext(lctx), 0),
                                           LLVMVoidTypeInContext(lctx), 0),
                                       NEWVAR("cast"));


        /* option */
        if ((opt = array_get(&expr->subs, 1)) == NULL) {
            FAIL("array_get");
        }

        /* constant str */
        assert(!(*opt)->isref);
        optname = (bytes_t *)(*opt)->value.literal->body;

        if (strcmp((char *)optname->data, "pos") == 0) {
            if ((ref = LLVMGetNamedFunction(module,
                    "dparse_struct_pi_pos")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }

            v = LLVMBuildCall(builder,
                              ref,
                              args,
                              countof(args),
                              NEWVAR("call"));

        } else if (strcmp((char *)optname->data, "data") == 0) {
            if ((ref = LLVMGetNamedFunction(module,
                    "mrklkit_rt_struct_pi_data_gc")) == NULL) {
                FAIL("LLVMGetNamedFunction");
            }

            v = LLVMBuildCall(builder,
                              ref,
                              args,
                              countof(args),
                              NEWVAR("call"));

        } else {
            TR(COMPILE_FUNCTION + 4903);
            goto err;
        }

    } else if (strcmp(name, "new") == 0) {
        lkit_expr_t **arg;
        UNUSED LLVMValueRef fn, args[2];

        arg = array_get(&expr->subs, 1);
        assert(arg != NULL);

        if ((args[1] = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         *arg,
                                         udata)) == NULL) {
            TR(COMPILE_FUNCTION + 5001);
            goto err;
        }

        if ((*arg)->type->tag == LKIT_STR) {
            switch (expr->type->tag) {
            case LKIT_DICT:
                if ((fn = LLVMGetNamedFunction(module,
                        "dparse_dict_from_bytes")) == NULL) {
                    FAIL("LLVMGetNamedFunction");
                }
                break;

            default:
                TR(COMPILE_FUNCTION + 5002);
                goto err;
            }
        } else {
            TR(COMPILE_FUNCTION + 5003);
            goto err;
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
                  lkit_expr_t *ectx,
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

                    } else {
                        lkit_expr_t **rand;
                        array_iter_t it;
                        LLVMValueRef *args = NULL;
                        lkit_func_t *tf = (lkit_func_t *)expr->value.ref->type;

                        /* XXX check args */

                        if (tf->fields.elnum != (expr->subs.elnum + 1)) {
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

                            /*
                             * "generic" cast
                             */
                            args[it.iter] = LLVMBuildPointerCast(
                                builder,
                                args[it.iter],
                                LLVMTypeOf(LLVMGetParam(fn, it.iter)),
                                NEWVAR("cast"));
                        }

                        v = LLVMBuildCall(builder,
                                          fn,
                                          args,
                                          expr->subs.elnum,
                                          NEWVAR((char *)expr->name->data));
                        //LLVMSetTailCall(v, 1);
                        free(args);
                    }

                }
                break;

            default:
                {
                    LLVMValueRef ref;
                    char buf[1024];
                    LLVMValueRef fn;

                    assert(builder != NULL);

                    if ((ref = LLVMGetNamedGlobal(module,
                            (char *)expr->name->data)) == NULL) {

                        bytes_t *qual_name = NULL;

                        qual_name = lkit_expr_qual_name(ectx, expr->name);
                        if ((ref = LLVMGetNamedGlobal(module,
                                (char *)qual_name->data)) == NULL) {
                            LLVMDumpModule(module);
                            TRACE("name %s qual_name %s",
                                  expr->name->data,
                                  qual_name->data);
                            lkit_expr_dump(expr);
                            FAIL("LLVMGetNamedGlobal");
                        }
                        snprintf(buf,
                                 sizeof(buf),
                                 "_mrklkit.%s.init",
                                 (char *)qual_name->data);
                        bytes_decref(&qual_name);
                    } else {
                        snprintf(buf,
                                 sizeof(buf),
                                 "_mrklkit.%s.init",
                                 (char *)expr->name->data);
                    }

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
                                     (char *)expr->name->data);
                            if ((testv = LLVMGetNamedGlobal(module,
                                                            buf)) == NULL) {
                                bytes_t *qual_name = NULL;

                                qual_name = lkit_expr_qual_name(ectx,
                                                                expr->name);
                                snprintf(buf,
                                         sizeof(buf),
                                         "_mrklkit.%s.init.done",
                                         (char *)qual_name->data);
                                bytes_decref(&qual_name);

                                if ((testv = LLVMGetNamedGlobal(module,
                                                                buf)) == NULL) {
                                    TRACE("not found: %s", buf);
                                    LLVMDumpModule(module);
                                    FAIL("LLVMGetNamedGlobal");
                                }
                            }

                            test = LLVMBuildICmp(builder,
                                                 LLVMIntEQ,
                                                 LLVMBuildLoad(builder,
                                                               testv,
                                                               NEWVAR("test")),
                                                 LLVMConstInt(
                                                     LLVMInt1TypeInContext(
                                                         lctx), 0, 0),
                                                 NEWVAR("test"));
                            LLVMBuildCondBr(builder, test, tblock, fblock);

                            LLVMPositionBuilderAtEnd(builder, tblock);

#ifdef MRKLKIT_LLVM_C_PATCH_VOID
                            if (LLVMBuildCall(builder,
                                              fn,
                                              NULL,
                                              0,
                                              "") == NULL) {
                                TR(LKIT_COMPILE_EXPR + 4);
                            }
#else
                            if (LLVMBuildCall(builder,
                                              fn,
                                              NULL,
                                              0,
                                              NEWVAR("tmp")) == NULL) {
                                TR(LKIT_COMPILE_EXPR + 4);
                            }
#endif

                            (void)LLVMBuildBr(builder, fblock);

                            LLVMPositionBuilderAtEnd(builder, fblock);
                        }
                    }

                    if (v == NULL) {
                        v = LLVMBuildLoad(builder, ref, NEWVAR("load"));
                    }

                    if (expr->type->tag == LKIT_STR &&
                            LKIT_EXPR_CONSTANT(expr)) {
                        v = LLVMBuildPointerCast(builder,
                                                 v,
                                                 mrklkit_ctx_get_type_backend(
                                                     mctx, expr->type),
                                                 NEWVAR("cast"));
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
                    LLVMValueRef binit[4], vv;

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

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:

            default:
                lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(LKIT_COMPILE_EXPR + 6);
                break;

            }
        } else {
            TR(LKIT_COMPILE_EXPR + 7);
        }
    }

    return v;
}


static int
compile_dynamic_initializer(mrklkit_ctx_t *mctx,
                            lkit_expr_t *ectx,
                            LLVMModuleRef module,
                            bytes_t *name,
                            lkit_expr_t *expr,
                            void *udata)
{
    char buf[1024];
    LLVMContextRef lctx;
    LLVMBasicBlockRef bb;
    LLVMBuilderRef builder;
    LLVMValueRef v, fn, storedv;

    /* dynamic initializer */
    lctx = LLVMGetModuleContext(module);

    snprintf(buf, sizeof(buf), "_mrklkit.%s.init", (char *)name->data);

#ifdef MRKLKIT_LLVM_C_PATCH_VOID
    fn = LLVMAddFunction(module,
                         buf,
                         LLVMFunctionType(LLVMVoidTypeInContext(lctx),
                                          NULL,
                                          0,
                                          0));
#else
    fn = LLVMAddFunction(module,
                         buf,
                         LLVMFunctionType(LLVMInt64TypeInContext(lctx),
                                          NULL,
                                          0,
                                          0));
#endif
    builder = LLVMCreateBuilderInContext(lctx);
    bb = LLVMAppendBasicBlockInContext(lctx, fn, NEWVAR("D.dyninit"));
    LLVMPositionBuilderAtEnd(builder, bb);

    if (expr->lazy_init) {
        LLVMValueRef testv;

        snprintf(buf, sizeof(buf), "_mrklkit.%s.init.done", (char *)name->data);
        testv = LLVMAddGlobal(module, LLVMInt1TypeInContext(lctx), buf);
        LLVMSetLinkage(testv, LLVMPrivateLinkage);
        LLVMSetInitializer(testv,
                           LLVMConstInt(LLVMInt1TypeInContext(lctx), 0, 0));

        if ((storedv = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         expr,
                                         udata)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 1);
        }

        v = LLVMAddGlobal(module, LLVMTypeOf(storedv), (char *)name->data);
        LLVMSetInitializer(v, LLVMGetUndef(LLVMTypeOf(storedv)));

        LLVMBuildStore(builder, storedv, v);
        LLVMBuildStore(builder,
                       LLVMConstInt(LLVMInt1TypeInContext(lctx), 1, 0),
                       testv);
        if (expr->type->compile_setup != NULL) {
            if (expr->type->compile_setup(ectx,
                                          module,
                                          builder,
                                          expr,
                                          name) != 0) {
                TRRET(COMPILE_DYNAMIC_INITIALIZER + 2);
            }
        }

    } else {
        if ((storedv = lkit_compile_expr(mctx,
                                         ectx,
                                         module,
                                         builder,
                                         expr,
                                         udata)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 3);
        }

        v = LLVMAddGlobal(module, LLVMTypeOf(storedv), (char *)name->data);
        LLVMSetInitializer(v, LLVMGetUndef(LLVMTypeOf(storedv)));

        LLVMBuildStore(builder, storedv, v);
        if (expr->type->compile_setup != NULL) {
            if (expr->type->compile_setup(ectx,
                                          module,
                                          builder,
                                          expr,
                                          name) != 0) {
                TRRET(COMPILE_DYNAMIC_INITIALIZER + 4);
            }
        }
    }

#ifdef MRKLKIT_LLVM_C_PATCH_VOID
    LLVMBuildRetVoid(builder);
#else
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0));
#endif
    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(v, LLVMPrivateLinkage);
    LLVMSetLinkage(fn, LLVMLinkOnceODRLinkage);
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

    //lkit_expr_dump(expr);
    //TRACE("name %s lazy %d referenced %d isref %d ismacro %d",
    //      name ? name->data : NULL,
    //      expr->lazy_init,
    //      expr->referenced,
    //      expr->isref,
    //      expr->ismacro);

    snprintf(buf, sizeof(buf), "_mrklkit.%s.init", (char *)name->data);

    if ((fn = LLVMGetNamedFunction(params->module, buf)) != NULL) {
        if (!expr->lazy_init) {
            if (expr->referenced) {
#ifdef MRKLKIT_LLVM_C_PATCH_VOID
                if (LLVMBuildCall(params->builder,
                                  fn,
                                  NULL,
                                  0,
                                  "") == NULL) {
                    TR(CALL_EAGER_INITIALIZER + 1);
                }
#else
                if (LLVMBuildCall(params->builder,
                                  fn,
                                  NULL,
                                  0,
                                  NEWVAR("tmp")) == NULL) {
                    TR(CALL_EAGER_INITIALIZER + 1);
                }
#endif
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
        lkit_expr_t *ectx;
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } *params = udata;

    //lkit_expr_dump(expr);
    //TRACE("name %s lazy %d referenced %d isref %d ismacro %d",
    //      name ? name->data : NULL,
    //      expr->lazy_init,
    //      expr->referenced,
    //      expr->isref,
    //      expr->ismacro);

    if (expr->lazy_init && expr->referenced && expr->isref) {
        char buf[1024];
        LLVMContextRef lctx;
        LLVMValueRef v;

        if (expr->ismacro) {
            TRACE("macro cannot be lazy: %s", name->data);
            TRRET(CALL_FINALIZER + 1);
        }

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

    if (expr->type->compile_cleanup != NULL) {
        (void)expr->type->compile_cleanup(params->ectx,
                                          params->module,
                                          params->builder,
                                          expr,
                                          name);
    }

    return 0;
}


static int
_compile(lkit_gitem_t **gitem, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        lkit_expr_t *ectx;
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
                    if ((*body)->type->tag != LKIT_IR) {
                        TRRET(SYM_COMPILE + 2);
                    }
                    //lkit_expr_dump(expr);

                    if ((modaux = array_incr(&params->mctx->modaux)) == NULL) {
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
                    lctx = LLVMGetModuleContext(params->module);
                    if (LLVMParseIRInContext(lctx,
                                             modaux->buf,
                                             &modaux->module,
                                             &error_msg) != 0) {
                        TRACE("%s", error_msg);
                        LLVMDisposeMessage(error_msg);
                        error_msg = NULL;
                        TRRET(SYM_COMPILE + 3);
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


static int
_cb1(lkit_gitem_t **gitem, void *udata)
{
    struct {
        int (*cb)(mrklkit_ctx_t *, lkit_expr_t *, lkit_expr_t *);
        mrklkit_ctx_t *mctx;
        lkit_expr_t *ectx;
    } *params = udata;
    return params->cb(params->mctx, params->ectx, (*gitem)->expr);
}


int
lkit_expr_ctx_compile(mrklkit_ctx_t *mctx,
                      lkit_expr_t *ectx,
                      LLVMModuleRef module,
                      void *udata)
{
    struct {
        int (*cb)(mrklkit_ctx_t *, lkit_expr_t *, lkit_expr_t *);
        mrklkit_ctx_t *mctx;
        lkit_expr_t *ectx;
    } params_remove_undef = { builtin_remove_undef, mctx, ectx };
    struct {
        mrklkit_ctx_t *mctx;
        lkit_expr_t *ectx;
        LLVMModuleRef module;
        void *udata;
    } params_compile = { mctx, ectx, module, udata };

    if (array_traverse(&ectx->glist,
                       (array_traverser_t)_cb1,
                       &params_remove_undef) != 0) {
        TRRET(LKIT_EXPR_CTX_COMPILE + 1);
    }

    if (array_traverse(&ectx->glist,
                       (array_traverser_t)_compile,
                       &params_compile) != 0) {
        TRRET(LKIT_EXPR_CTX_COMPILE + 2);
    }
    return 0;
}


int
lkit_expr_ctx_compile_pre(lkit_expr_t *ectx,
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
lkit_expr_ctx_compile_post(lkit_expr_t *ectx,
                           LLVMModuleRef module,
                           LLVMBuilderRef builder)
{
    struct {
        lkit_expr_t *ectx;
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

