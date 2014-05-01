#include <assert.h>
#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/lexpr.h>
#include "builtin_private.h"

#include "diag.h"


static char *fnames[] = {
    ",",
    "if",
    "print",
    "+",
    "-",
    "*",
    "/",
    "%",
    "min",
    "max",
    "and",
    "or",
    "not",
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",
    "get",
    "set",
    "del",
    "itof",
    "ftoi",
    "tostr",
};
static int
check_function(bytes_t *name)
{
    char *n = (char *)name->data;
    size_t i;

    for (i = 0; i < countof(fnames); ++i) {
        if (strcmp(fnames[i], n) == 0) {
            return 0;
        }
    }
    return 1;
}


/**
 * LLVM IR emitter.
 */
static LLVMValueRef
compile_if(LLVMModuleRef module,
           LLVMBuilderRef builder,
           lkit_expr_t *cexpr,
           lkit_expr_t *texpr,
           lkit_expr_t *fexpr,
           lkit_type_t *restype)
{
    LLVMValueRef v = NULL, res, cond, texp, fexp, fn, iexp[2];
    LLVMBasicBlockRef currblock, endblock, tblock, fblock, iblock[2];

    currblock = LLVMGetInsertBlock(builder);
    fn = LLVMGetBasicBlockParent(currblock);

    /**/
    cond = builtin_compile_expr(module, builder, cexpr);
    assert(cond != NULL);

    endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

    /**/
    tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
    LLVMPositionBuilderAtEnd(builder, tblock);

    if (texpr != NULL) {
        texp = builtin_compile_expr(module, builder, texpr);
        assert(texp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    /**/
    fblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.false"));
    LLVMPositionBuilderAtEnd(builder, fblock);

    if (fexpr != NULL) {
        fexp = builtin_compile_expr(module, builder, fexpr);
        assert(fexp != NULL);
        res = LLVMBuildBr(builder, endblock);
        assert(res != NULL);
    } else {
        assert(restype == NULL);
    }

    /**/
    LLVMPositionBuilderAtEnd(builder, currblock);
    LLVMBuildCondBr(builder, cond, tblock, fblock);
    LLVMPositionBuilderAtEnd(builder, endblock);
    if (restype != NULL) {
        v = LLVMBuildPhi(builder, restype->backend, NEWVAR("if.result"));
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
compile_cmp(LLVMModuleRef module,
           LLVMBuilderRef builder,
           lkit_expr_t *expr,
           LLVMIntPredicate ip,
           LLVMRealPredicate rp)
{
    LLVMValueRef v = NULL;
    lkit_expr_t **arg;
    array_iter_t it;
    LLVMValueRef rand;

    arg = array_first(&expr->subs, &it);
    if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
        TR(COMPILE_CMP + 1);
        goto end;
    }

    arg = array_next(&expr->subs, &it);
    if ((rand = builtin_compile_expr(module, builder, *arg)) == NULL) {
        TR(COMPILE_CMP + 2);
        goto end;
    }

    if ((*arg)->type->tag == LKIT_INT || (*arg)->type->tag == LKIT_BOOL) {
        v = LLVMBuildICmp(builder, ip, v, rand, NEWVAR("cmp"));

    } else if ((*arg)->type->tag == LKIT_FLOAT) {
        v = LLVMBuildFCmp(builder, rp, v, rand, NEWVAR("cmp"));

    } else if ((*arg)->type->tag == LKIT_STR) {
        LLVMValueRef ref, args[2], rv;

        /*
         * (sym strcmp (func int str str))
         *
         * XXX use bytes_cmp() ?
         */
        ref = LLVMGetNamedFunction(module, "strcmp");

        if (ref == NULL) {
            v = NULL;
            TR(COMPILE_CMP + 3);
            goto end;

        } else {
            /*
             * bytes_t *
             *
             */
            args[0] = LLVMBuildStructGEP(builder,
                                         v,
                                         BYTES_DATA_IDX,
                                         NEWVAR("tmp"));
            args[1] = LLVMBuildStructGEP(builder,
                                         rand,
                                         BYTES_DATA_IDX,
                                         NEWVAR("tmp"));
            rv = LLVMBuildCall(builder,
                              ref,
                              args,
                              2,
                              NEWVAR("strcmp"));
            //LLVMSetTailCall(v, 1);
            v = LLVMBuildICmp(builder,
                              ip,
                              rv,
                              LLVMConstInt(LLVMInt64Type(), 0, 0),
                              NEWVAR("cmp"));
        }

    } else {
        TR(COMPILE_CMP + 4);
        goto end;
    }

end:
    return v;
}


static LLVMValueRef
compile_function(LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;
    char *name = (char *)expr->name->data;

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
        //TRACE("%p %p %p", cexpr, texpr, fexpr);
        v = compile_if(module, builder, *cexpr, *texpr, *fexpr, expr->type);

    } else if (strcmp(name, ",") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym , (func undef undef ...))
        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
                TR(COMPILE_FUNCTION + 200);
                goto err;
            }
        }

    } else if (strcmp(name, "print") == 0) {
        LLVMValueRef fn, args[2];

        // (sym print (func undef undef ...))
        if ((fn = LLVMGetNamedFunction(module, "printf")) == NULL) {
            TR(COMPILE_FUNCTION + 300);
            goto err;
        } else {
            lkit_expr_t **arg;
            array_iter_t it;

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 301);
                    goto err;
                }

                switch ((*arg)->type->tag) {
                case LKIT_INT:
                    args[0] = LLVMBuildGlobalStringPtr(builder,
                                                       "%ld ",
                                                       NEWVAR("printf.fmt"));
                    args[1] = v;
                    break;

                case LKIT_FLOAT:
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
        if (expr->type->tag == LKIT_INT) {
            v = LLVMConstInt(expr->type->backend, 0, 1);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 400);
                    goto err;
                }

                v = LLVMBuildAdd(builder, v, rand, NEWVAR("plus"));
            }

        } else if (expr->type->tag == LKIT_FLOAT) {
            v = LLVMConstReal(expr->type->backend, 0.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 401);
                    goto err;
                }

                v = LLVMBuildFAdd(builder, v, rand, NEWVAR("plus"));
            }
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
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 500);
            goto err;
        }
        if (expr->type->tag == LKIT_INT) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 501);
                    goto err;
                }

                v = LLVMBuildSub(builder, v, rand, NEWVAR("minus"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 600);
            goto err;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 601);
                    goto err;
                }

                v = LLVMBuildSDiv(builder, v, rand, NEWVAR("div"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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
        if (expr->type->tag == LKIT_INT) {
            v = LLVMConstInt(expr->type->backend, 1, 1);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 700);
                    goto err;
                }

                v = LLVMBuildMul(builder, v, rand, NEWVAR("mul"));
            }

        } else if (expr->type->tag == LKIT_FLOAT) {
            v = LLVMConstReal(expr->type->backend, 1.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 701);
                    goto err;
                }

                v = LLVMBuildFAdd(builder, v, rand, NEWVAR("mul"));
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
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 800);
            goto err;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 801);
                    goto err;
                }

                v = LLVMBuildSRem(builder, v, rand, NEWVAR("rem"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 900);
            goto err;
        }

#if 0
        LLVMValueRef mem;
        mem = LLVMBuildAlloca(builder, expr->type->backend, NEWVAR("mem"));
        LLVMBuildStore(builder, v, mem);
        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = builtin_compile_expr(module, builder, *arg)) == NULL) {
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

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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

        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1000);
            goto err;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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

        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = builtin_compile_expr(module,
                                                 builder,
                                                 *arg)) == NULL) {
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

    } else if (strcmp(name , "and") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym and (func bool bool ...))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1100);
            goto err;
        }

        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = builtin_compile_expr(module, builder, *arg)) == NULL) {
                TR(COMPILE_FUNCTION + 1101);
                goto err;
            }

            v = LLVMBuildAnd(builder, v, rand, NEWVAR("and"));
        }

    } else if (strcmp(name , "or") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym or (func bool bool ...))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1200);
            goto err;
        }

        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = builtin_compile_expr(module, builder, *arg)) == NULL) {
                TR(COMPILE_FUNCTION + 1201);
                goto err;
            }

            v = LLVMBuildOr(builder, v, rand, NEWVAR("or"));
        }

    } else if (strcmp(name , "not") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym not (func bool bool))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1300);
            goto err;
        }
        v = LLVMBuildNot(builder, v, NEWVAR("not"));

    } else if (strcmp(name , "==") == 0) {
        //(sym == (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntEQ, LLVMRealUEQ);

    } else if (strcmp(name , "!=") == 0) {
        //(sym != (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntNE, LLVMRealUNE);

    } else if (strcmp(name , "<") == 0) {
        //(sym < (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntSLT, LLVMRealULT);

    } else if (strcmp(name , "<=") == 0) {
        //(sym <= (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntSLE, LLVMRealULE);

    } else if (strcmp(name , ">") == 0) {
        //(sym > (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntSGT, LLVMRealUGT);

    } else if (strcmp(name , ">=") == 0) {
        //(sym >= (func bool undef undef)) done
        v = compile_cmp(module, builder, expr, LLVMIntSGE, LLVMRealUGE);

    } else if (strcmp(name , "get") == 0) {
        //(sym get (func undef undef undef under)) done
        lkit_expr_t **arg, **cont;
        LLVMValueRef fn, args[3];

        /* container */
        if ((cont = array_get(&expr->subs, 0)) == NULL) {
            FAIL("array_get");
        }

        if ((*cont)->type->tag != LKIT_STRUCT) {
            /* default */
            if ((arg = array_get(&expr->subs, 2)) == NULL) {
                FAIL("array_get");
            }
            if ((args[2] = builtin_compile_expr(module,
                                                builder,
                                                *arg)) == NULL) {
                TR(COMPILE_FUNCTION + 1300);
                goto err;
            }
        }

        if ((args[0] = builtin_compile_expr(module, builder, *cont)) == NULL) {
            TR(COMPILE_FUNCTION + 1301);
            goto err;
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
                if ((args[1] = builtin_compile_expr(module,
                                                    builder,
                                                    *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 1302);
                    goto err;
                }

                if ((fty = lkit_array_get_element_type(
                            (lkit_array_t *)(*cont)->type)) == NULL) {
                    TR(COMPILE_FUNCTION + 1303);
                    goto err;
                }
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_array_item_%s",
                         fty->name);

                //(sym mrklkit_rt_get_array_item (func undef array int undef))
                if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                    TR(COMPILE_FUNCTION + 1304);
                    goto err;
                }
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
                if ((args[1] = builtin_compile_expr(module,
                                                    builder,
                                                    *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 1305);
                    goto err;
                }

                if ((fty = lkit_dict_get_element_type(
                            (lkit_dict_t *)(*cont)->type)) == NULL) {
                    TR(COMPILE_FUNCTION + 1306);
                    goto err;
                }
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_dict_item_%s",
                         fty->name);

                if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                    TR(COMPILE_FUNCTION + 1307);
                    goto err;
                }
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
                    TR(COMPILE_FUNCTION + 1308);
                    goto err;
                }

                if ((fty = lkit_struct_get_field_type(
                            (lkit_struct_t *)(*cont)->type,
                            (bytes_t *)(*arg)->value.literal->body)) == NULL) {
                    TR(COMPILE_FUNCTION + 1309);
                    goto err;
                }
                snprintf(buf,
                         sizeof(buf),
                         "mrklkit_rt_get_struct_item_%s",
                         fty->name);

                args[1] = LLVMConstInt(LLVMInt64Type(), idx, 1);

                if ((fn = LLVMGetNamedFunction(module, buf)) == NULL) {
                    TR(COMPILE_FUNCTION + 1310);
                    goto err;
                }
                v = LLVMBuildCall(builder, fn, args, 2, NEWVAR(name));
            }

            break;

        default:
            FAIL("compile_function");
        }

    } else if (strcmp(name , "itof") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym itof (func float int))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1400);
            goto err;
        }
        v = LLVMBuildSIToFP(builder, v, LLVMDoubleType(), NEWVAR("itof"));

    } else if (strcmp(name , "ftoi") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym ftoi (func int float))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1500);
            goto err;
        }
        v = LLVMBuildFPToSI(builder, v, LLVMInt64Type(), NEWVAR("ftoi"));

    } else if (strcmp(name , "tostr") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym tostr (func str undef))
        arg = array_first(&expr->subs, &it);
        if ((v = builtin_compile_expr(module, builder, *arg)) == NULL) {
            TR(COMPILE_FUNCTION + 1600);
            goto err;
        }

        if ((*arg)->type->tag == LKIT_INT) {
        } else {
        }

    } else {
        TR(COMPILE_FUNCTION + 1700);
        goto err;
    }

end:
    return v;

err:
    v = NULL;
    goto end;
}


static LLVMTypeRef
compile_bytes_t(size_t sz)
{
    LLVMTypeRef fields[4], ty;

    fields[0] = LLVMInt64Type();
    fields[1] = LLVMInt64Type();
    fields[2] = LLVMInt64Type();
    fields[3] = LLVMArrayType(LLVMInt8Type(), sz + 1);
    ty = LLVMStructType(fields, countof(fields), 0);
    return ty;
}


static int
compile_dynamic_initializer(LLVMModuleRef module,
                            LLVMValueRef v,
                            bytes_t *name,
                            lkit_expr_t *value)
{
    char buf[1024];
    LLVMBasicBlockRef bb;
    LLVMBuilderRef builder;
    LLVMValueRef chkv, fn, storedv;

    /* dynamic initializer */
    //lkit_type_dump(value->type);
    //lkit_expr_dump(value);

    snprintf(buf, sizeof(buf), ".mrklkit.init.%s", (char *)name->data);

    fn = LLVMAddFunction(module,
                         buf,
                         LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    builder = LLVMCreateBuilder();

    if (value->lazy_init) {
        LLVMValueRef UNUSED res, cond;
        LLVMBasicBlockRef currblock, endblock, tblock, fblock;

        snprintf(buf, sizeof(buf), ".mrklkit.init.done.%s", (char *)name->data);
        chkv = LLVMAddGlobal(module, LLVMInt1Type(), buf);
        LLVMSetLinkage(chkv, LLVMPrivateLinkage);
        LLVMSetInitializer(chkv, LLVMConstInt(LLVMInt1Type(), 0, 0));

        currblock = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit"));
        tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
        fblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.false"));
        endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

        LLVMPositionBuilderAtEnd(builder, tblock);

        if ((storedv = builtin_compile_expr(module, builder, value)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 1);
        }
        LLVMBuildStore(builder, storedv, v);
        LLVMBuildStore(builder, LLVMConstInt(LLVMInt1Type(), 1, 0), chkv);
        res = LLVMBuildBr(builder, endblock);

        LLVMPositionBuilderAtEnd(builder, fblock);
        res = LLVMBuildBr(builder, endblock);

        LLVMPositionBuilderAtEnd(builder, currblock);
        cond = LLVMBuildICmp(builder,
                             LLVMIntEQ,
                             LLVMBuildLoad(builder, chkv, NEWVAR("test")),
                             LLVMConstInt(LLVMInt1Type(), 0, 0),
                             NEWVAR("test"));
        LLVMPositionBuilderAtEnd(builder, currblock);
        LLVMBuildCondBr(builder, cond, tblock, fblock);
        LLVMPositionBuilderAtEnd(builder, endblock);

    } else {
        bb = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit"));
        LLVMPositionBuilderAtEnd(builder, bb);
        if ((storedv = builtin_compile_expr(module, builder, value)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 2);
        }
        LLVMBuildStore(builder, storedv, v);
    }

    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(v, LLVMPrivateLinkage);
    return 0;
}


LLVMValueRef
builtin_compile_expr(LLVMModuleRef module,
                    LLVMBuilderRef builder,
                    lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;

    //lkit_expr_dump(expr);
    if (expr->isref) {
        switch (expr->value.ref->type->tag) {
        case LKIT_FUNC:
            assert(builder != NULL);

            /* first try pre-defined functions */
            if ((v = compile_function(module, builder, expr)) == NULL) {
                LLVMValueRef ref;

                /* then user-defined */
                ref = LLVMGetNamedFunction(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    //TRACE("failed to find builtin (normally must be "
                    //      "mrklkit_rt_* or a standard C library, found %s)",
                    //      (char *)expr->name->data);
                    TR(BUILTIN_COMPILE_EXPR + 1);

                } else {
                    lkit_expr_t **rand;
                    array_iter_t it;
                    LLVMValueRef *args = NULL;
                    lkit_func_t *tf = (lkit_func_t *)expr->value.ref->type;

                    /* XXX check args */

                    if (tf->fields.elnum != (expr->subs.elnum + 1)) {
                        TR(BUILTIN_COMPILE_EXPR + 2);
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
                            builtin_compile_expr(module, builder, *rand);
                        assert(args[it.iter] != NULL);
                    }

                    v = LLVMBuildCall(builder,
                                      ref,
                                      args,
                                      expr->subs.elnum,
                                      NEWVAR((char *)expr->name->data));
                    //LLVMSetTailCall(v, 1);
                    free(args);
                }

            }
            break;

        case LKIT_INT:
        case LKIT_FLOAT:
        case LKIT_BOOL:
        case LKIT_STR:
        case LKIT_ARRAY:
        case LKIT_DICT:
        case LKIT_STRUCT:
            {
                LLVMValueRef ref;

                assert(builder != NULL);

                ref = LLVMGetNamedGlobal(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TR(BUILTIN_COMPILE_EXPR + 3);

                } else {
                    char buf[1024];
                    LLVMValueRef fn;

                    snprintf(buf,
                             sizeof(buf),
                             ".mrklkit.init.%s",
                             (char *)expr->name->data);
                    //TRACE("init: %s", buf);

                    if ((fn = LLVMGetNamedFunction(module, buf)) != NULL) {
                        if (expr->value.ref->lazy_init) {
                            if (LLVMBuildCall(builder,
                                              fn,
                                              NULL,
                                              0,
                                              NEWVAR("tmp")) == NULL) {
                                TR(BUILTIN_COMPILE_EXPR + 4);
                            }
                        }
                    }

                    v = LLVMBuildLoad(builder,
                                      ref,
                                      NEWVAR((char *)expr->name->data));
                }
            }

            break;

        default:
            lkit_expr_dump(expr);
            TR(BUILTIN_COMPILE_EXPR + 5);
            break;

        }

    } else {
        if (expr->value.literal != NULL) {
            assert(expr->type->backend != NULL);

            switch (expr->type->tag) {
            case LKIT_INT:
                v = LLVMConstInt(expr->type->backend,
                                 *(int64_t *)expr->value.literal->body, 1);
                break;

            case LKIT_FLOAT:
                v = LLVMConstReal(expr->type->backend,
                                  *(double *)expr->value.literal->body);
                break;

            case LKIT_BOOL:
                v = LLVMConstInt(expr->type->backend,
                                 expr->value.literal->body[0],
                                 0);
                break;

            case LKIT_STR:
                {
                    bytes_t *b;
                    LLVMValueRef binit[4];

                    b = (bytes_t *)expr->value.literal->body;
                    v = LLVMAddGlobal(module,
                                      compile_bytes_t(b->sz),
                                      NEWVAR(".mrklkit.val"));
                    binit[0] = LLVMConstInt(LLVMInt64Type(), 0xdada, 0);
                    binit[1] = LLVMConstInt(LLVMInt64Type(), b->sz, 0);
                    binit[2] = LLVMConstInt(LLVMInt64Type(), bytes_hash(b), 0);
                    binit[3] = LLVMConstString((char *)b->data, b->sz, 1);
                    LLVMSetInitializer(v,
                        LLVMConstStruct(binit, countof(binit), 0));
                }
                break;

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:

            default:

                //lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(BUILTIN_COMPILE_EXPR + 7);
                break;

            }
        } else {
            TR(BUILTIN_COMPILE_EXPR + 8);
        }
    }

    return v;
}


static LLVMValueRef
compile_decl(LLVMModuleRef module, bytes_t *name, lkit_type_t *type)
{
    LLVMValueRef fn;

    //lkit_type_dump(type);
    assert(type->backend != NULL);
    //assert(type->tag == LKIT_FUNC);

    switch (type->tag) {
    case LKIT_FUNC:
        fn = LLVMAddFunction(module, (char *)name->data, type->backend);
        LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
        break;

    default:
        fn = LLVMAddGlobal(module, type->backend, (char *)name->data);
        //FAIL("compile_decl: not implemented");
    }
    return fn;
}


int
builtin_compile(lkit_gitem_t **gitem, void *udata)
{
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;
    LLVMModuleRef module = (LLVMModuleRef)udata;
    LLVMValueRef v;

    //TRACE("%p/%s %d", expr->type->backend, (char *)name->data, expr->isref);
    //lkit_type_dump(expr->type);
    //lkit_expr_dump(expr);

    /* check for function */
    if (check_function(name) == 0) {
        return 0;
    }

    if (expr->isref) {
        v = LLVMAddGlobal(module, expr->type->backend, (char *)name->data);
        LLVMSetLinkage(v, LLVMPrivateLinkage);
        LLVMSetInitializer(v, LLVMGetUndef(expr->type->backend));
        if (compile_dynamic_initializer(module, v, name, expr) != 0) {
            TRRET(SYM_COMPILE + 1);
        }
    } else {
        if (expr->value.literal != NULL) {
            LLVMValueRef initv;

            v = LLVMAddGlobal(module, expr->type->backend, (char *)name->data);
            LLVMSetLinkage(v, LLVMPrivateLinkage);
            initv = builtin_compile_expr(module, NULL, expr);
            assert(initv != NULL);
            LLVMSetInitializer(v, initv);
        } else {
            /* declaration, */
            compile_decl(module, name, expr->type);
        }
    }

    return 0;
}


int
builtingen_call_eager_initializer(lkit_gitem_t **gitem, void *udata)
{
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;
    struct {
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } *params = udata;
    char buf[1024];
    LLVMValueRef fn;

    snprintf(buf, sizeof(buf), ".mrklkit.init.%s", (char *)name->data);

    //TRACE("eager: %s %s lazy=%s",
    //      buf,
    //      expr->name ? expr->name->data : NULL,
    //      expr->lazy_init ? "Y" : "N");

    if ((fn = LLVMGetNamedFunction(params->module, buf)) != NULL) {
        if (!expr->lazy_init) {
            if (LLVMBuildCall(params->builder,
                              fn,
                              NULL,
                              0,
                              NEWVAR("tmp")) == NULL) {
                TR(BUILTINGEN_CALL_EAGER_INITIALIZER + 1);
            }
        } else {
            //TRACE("not calling");
        }
    } else {
        //TRACE("not found");
    }
    return 0;
}


