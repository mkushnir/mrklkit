#include <assert.h>
#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/lexpr.h>
#include "builtin_private.h"

#include "diag.h"

static LLVMValueRef compile_expr(LLVMModuleRef, LLVMBuilderRef, lkit_expr_t *);

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
    cond = compile_expr(module, builder, cexpr);
    assert(cond != NULL);

    endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

    /**/
    tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
    LLVMPositionBuilderAtEnd(builder, tblock);

    if (texpr != NULL) {
        texp = compile_expr(module, builder, texpr);
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
        fexp = compile_expr(module, builder, fexpr);
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
    if ((v = compile_expr(module, builder, *arg)) == NULL) {
        TR(COMPILE_CMP + 1);
        goto end;
    }

    arg = array_next(&expr->subs, &it);
    if ((rand = compile_expr(module, builder, *arg)) == NULL) {
        TR(COMPILE_CMP + 2);
        goto end;
    }

    if ((*arg)->type->tag == LKIT_INT || (*arg)->type->tag == LKIT_BOOL) {
        v = LLVMBuildICmp(builder, ip, v, rand, NEWVAR("cmp"));

    } else if ((*arg)->type->tag == LKIT_FLOAT) {
        v = LLVMBuildFCmp(builder, rp, v, rand, NEWVAR("cmp"));

    } else if ((*arg)->type->tag == LKIT_STR) {
        LLVMValueRef ref, args[2], rv;

        /* then user-defined */
        ref = LLVMGetNamedFunction(module, "strcmp");

        if (ref == NULL) {
            v = NULL;
            TR(COMPILE_CMP + 3);
            goto end;

        } else {
            /*
             * bytes_t *
             */
            args[0] = LLVMBuildStructGEP(builder, v, 2, NEWVAR("tmp"));
            args[1] = LLVMBuildStructGEP(builder, rand, 2, NEWVAR("tmp"));
            rv = LLVMBuildCall(builder,
                              ref,
                              args,
                              2,
                              NEWVAR("strcmp"));
            //LLVMSetTailCall(v, 1);
            v = LLVMBuildICmp(builder,
                              ip,
                              rv,
                              LLVMConstInt(LLVMIntType(64), 0, 0),
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
            TR(COMPILE_FUNCTION + 1);
            return v;
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

            if ((v = compile_expr(module, builder, *arg)) == NULL) {
                TR(COMPILE_FUNCTION + 1);
                break;
            }
        }

    } else if (strcmp(name, "print") == 0) {
        LLVMValueRef fn, args[2];

        // (sym print (func undef undef ...))
        if ((fn = LLVMGetNamedFunction(module, "printf")) == NULL) {
            TR(COMPILE_FUNCTION + 2);
        } else {
            lkit_expr_t **arg;
            array_iter_t it;

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                if ((v = compile_expr(module, builder, *arg)) == NULL) {
                    TR(COMPILE_FUNCTION + 1);
                    return v;
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
                    args[1] = LLVMBuildStructGEP(builder, v, 2, NEWVAR((char *)(*arg)->name->data));
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

            args[0] = LLVMBuildGlobalStringPtr(builder, "\n", NEWVAR("printf.fmt"));
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

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 3);
                    break;
                }

                v = LLVMBuildAdd(builder, v, rand, NEWVAR("plus"));
            }

        } else if (expr->type->tag == LKIT_FLOAT) {
            v = LLVMConstReal(expr->type->backend, 0.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 4);
                    break;
                }

                v = LLVMBuildFAdd(builder, v, rand, NEWVAR("plus"));
            }
        } else {
            TR(COMPILE_FUNCTION + 5);
            return v;
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
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 6);
            return v;
        }
        if (expr->type->tag == LKIT_INT) {

            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 7);
                    break;
                }

                v = LLVMBuildSub(builder, v, rand, NEWVAR("minus"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 8);
                    break;
                }

                v = LLVMBuildFSub(builder, v, rand, NEWVAR("minus"));
            }
        } else {
            TR(COMPILE_FUNCTION + 9);
            return v;
        }

    } else if (strcmp(name , "/") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym / (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 10);
            return v;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 11);
                    break;
                }

                v = LLVMBuildSDiv(builder, v, rand, NEWVAR("div"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 12);
                    break;
                }

                v = LLVMBuildFDiv(builder, v, rand, NEWVAR("div"));
            }
        } else {
            TR(COMPILE_FUNCTION + 13);
            return v;
        }

    } else if (strcmp(name , "*") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym * (func undef undef ...))
        if (expr->type->tag == LKIT_INT) {
            v = LLVMConstInt(expr->type->backend, 0, 1);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 14);
                    break;
                }

                v = LLVMBuildMul(builder, v, rand, NEWVAR("mul"));
            }

        } else if (expr->type->tag == LKIT_FLOAT) {
            v = LLVMConstReal(expr->type->backend, 0.0);
            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 15);
                    break;
                }

                v = LLVMBuildFAdd(builder, v, rand, NEWVAR("mul"));
            }
        } else {
            TR(COMPILE_FUNCTION + 16);
            return v;
        }

    } else if (strcmp(name , "%") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym % (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 17);
            return v;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 18);
                    break;
                }

                v = LLVMBuildSRem(builder, v, rand, NEWVAR("rem"));
            }
        } else if (expr->type->tag == LKIT_FLOAT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 19);
                    break;
                }

                v = LLVMBuildFRem(builder, v, rand, NEWVAR("rem"));
            }
        } else {
            TR(COMPILE_FUNCTION + 20);
            return v;
        }

    } else if (strcmp(name , "min") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym min (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 21);
            return v;
        }

#if 0
        LLVMValueRef mem;
        mem = LLVMBuildAlloca(builder, expr->type->backend, NEWVAR("mem"));
        LLVMBuildStore(builder, v, mem);
        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                v = NULL;
                TR(COMPILE_FUNCTION + 2);
                break;
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

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 22);
                    break;
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

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 23);
                    break;
                }

                cond = LLVMBuildFCmp(builder,
                                     LLVMRealULT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("min"));
            }

        } else {
            TR(COMPILE_FUNCTION + 24);
            return v;
        }

    } else if (strcmp(name , "max") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym max (func undef undef ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 25);
            return v;
        }

        if (expr->type->tag == LKIT_INT) {
            for (arg = array_next(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                LLVMValueRef rand, cond;

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 26);
                    break;
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

                if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                    v = NULL;
                    TR(COMPILE_FUNCTION + 27);
                    break;
                }

                cond = LLVMBuildFCmp(builder,
                                     LLVMRealUGT,
                                     v,
                                     rand,
                                     NEWVAR("cond"));

                v = LLVMBuildSelect(builder, cond, v, rand, NEWVAR("max"));
            }

        } else {
            TR(COMPILE_FUNCTION + 28);
            return v;
        }

    } else if (strcmp(name , "and") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym and (func bool bool ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 29);
            return v;
        }

        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                v = NULL;
                TR(COMPILE_FUNCTION + 30);
                break;
            }

            v = LLVMBuildAnd(builder, v, rand, NEWVAR("and"));
        }

    } else if (strcmp(name , "or") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym or (func bool bool ...))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 31);
            return v;
        }

        for (arg = array_next(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                v = NULL;
                TR(COMPILE_FUNCTION + 32);
                break;
            }

            v = LLVMBuildOr(builder, v, rand, NEWVAR("or"));
        }

    } else if (strcmp(name , "not") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym not (func bool bool))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 33);
            return v;
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

    } else if (strcmp(name , "itof") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym itof (func float int))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 34);
            return v;
        }
        v = LLVMBuildSIToFP(builder, v, LLVMDoubleType(), NEWVAR("itof"));

    } else if (strcmp(name , "ftoi") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym ftoi (func int float))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 35);
            return v;
        }
        v = LLVMBuildFPToSI(builder, v, LLVMIntType(64), NEWVAR("ftoi"));

    } else if (strcmp(name , "tostr") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        //(sym tostr (func str undef))
        arg = array_first(&expr->subs, &it);
        if ((v = compile_expr(module, builder, *arg)) == NULL) {
            v = NULL;
            TR(COMPILE_FUNCTION + 36);
            return v;
        }

        if ((*arg)->type->tag == LKIT_INT) {
        } else {
        }

    } else {
        TR(COMPILE_FUNCTION + 37);
    }

    return v;
}

static LLVMTypeRef
compile_bytes_t(size_t sz)
{

    LLVMTypeRef fields[3], ty;

    fields[0] = LLVMIntType(64);
    fields[1] = LLVMIntType(64);
    fields[2] = LLVMArrayType(LLVMIntType(8), sz + 1);
    ty = LLVMStructType(fields, 3, 0);
    return ty;
}

static LLVMValueRef
compile_expr(LLVMModuleRef module,
                    LLVMBuilderRef builder,
                    lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;

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
                    TR(COMPILE_EXPR + 1);

                } else {
                    lkit_expr_t **rand;
                    array_iter_t it;
                    LLVMValueRef *args = NULL;
                    lkit_func_t *tf = (lkit_func_t *)expr->value.ref->type;

                    /* XXX check args */

                    if (tf->fields.elnum != (expr->subs.elnum + 1)) {
                        TR(COMPILE_EXPR + 2);
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
                            compile_expr(module, builder, *rand);
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
            {
                LLVMValueRef ref;

                assert(builder != NULL);

                ref = LLVMGetNamedGlobal(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TR(COMPILE_EXPR + 3);

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
                                TR(COMPILE_EXPR + 4);
                            }
                        }
                    }

                    v = LLVMBuildLoad(builder,
                                      ref,
                                      NEWVAR((char *)expr->name->data));
                }
            }

            break;

        case LKIT_STR:
            {
                LLVMValueRef ref;

                assert(builder != NULL);

                ref = LLVMGetNamedGlobal(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TR(COMPILE_EXPR + 3);

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
                                TR(COMPILE_EXPR + 4);
                            }
                        }
                    }

                    v = LLVMBuildLoad(builder, ref, NEWVAR((char *)expr->name->data));
                }
            }

            break;

        default:
            TR(COMPILE_EXPR + 5);
            break;

        }

    } else {
        if (expr->value.literal != NULL) {
            assert(expr->type->backend != NULL);

            switch (expr->type->tag) {
            case LKIT_INT:
                v = LLVMConstInt(expr->type->backend,
                                 *(uint64_t *)expr->value.literal->body, 1);
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
#if 0
                    bytes_t *b;
                    LLVMValueRef tmp;

                    b = (bytes_t *)expr->value.literal->body;
                    tmp = LLVMAddGlobal(module,
                                        LLVMArrayType(LLVMIntType(8),
                                                      b->sz + 1),
                                        NEWVAR(".mrklkit.literal"));
                    LLVMSetLinkage(tmp, LLVMPrivateLinkage);
                    LLVMSetInitializer(tmp,
                                       LLVMConstString((char *)b->data,
                                                       b->sz,
                                                       0));
                    v = LLVMConstBitCast(tmp, expr->type->backend);
#endif

#if 1
                    bytes_t *b;
                    LLVMValueRef binit[3];

                    b = (bytes_t *)expr->value.literal->body;
                    v = LLVMAddGlobal(module,
                                         compile_bytes_t(b->sz),
                                         NEWVAR(".mrklkit.val"));
                    binit[0] = LLVMConstInt(LLVMIntType(64), b->sz, 0);
                    binit[1] = LLVMConstInt(LLVMIntType(64), bytes_hash(b), 0);
                    binit[2] = LLVMConstString((char *)b->data, b->sz, 0);
                    LLVMSetInitializer(v, LLVMConstStruct(binit, 3, 0));
#endif
                }
                break;

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:

            default:

                //lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(COMPILE_EXPR + 6);
               break;

            }
        } else {
            TR(COMPILE_EXPR + 7);
        }
    }

    return v;
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
        LLVMValueRef res, cond;
        LLVMBasicBlockRef currblock, endblock, tblock, fblock;

        snprintf(buf, sizeof(buf), ".mrklkit.init.done.%s", (char *)name->data);
        chkv = LLVMAddGlobal(module, LLVMIntType(1), buf);
        LLVMSetLinkage(chkv, LLVMPrivateLinkage);
        LLVMSetInitializer(chkv, LLVMConstInt(LLVMIntType(1), 0, 0));

        currblock = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit"));
        tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
        fblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.false"));
        endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

        LLVMPositionBuilderAtEnd(builder, tblock);

        if ((storedv = compile_expr(module, builder, value)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 1);
        }
        LLVMBuildStore(builder, storedv, v);
        LLVMBuildStore(builder, LLVMConstInt(LLVMIntType(1), 1, 0), chkv);
        res = LLVMBuildBr(builder, endblock);

        LLVMPositionBuilderAtEnd(builder, fblock);
        res = LLVMBuildBr(builder, endblock);

        LLVMPositionBuilderAtEnd(builder, currblock);
        cond = LLVMBuildICmp(builder,
                             LLVMIntEQ,
                             LLVMBuildLoad(builder, chkv, NEWVAR("test")),
                             LLVMConstInt(LLVMIntType(1), 0, 0),
                             NEWVAR("test"));
        LLVMPositionBuilderAtEnd(builder, currblock);
        LLVMBuildCondBr(builder, cond, tblock, fblock);
        LLVMPositionBuilderAtEnd(builder, endblock);

    } else {
        bb = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit"));
        LLVMPositionBuilderAtEnd(builder, bb);
        if ((storedv = compile_expr(module, builder, value)) == NULL) {
            TRRET(COMPILE_DYNAMIC_INITIALIZER + 2);
        }
        LLVMBuildStore(builder, storedv, v);
    }

    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(v, LLVMPrivateLinkage);
    return 0;
}

static LLVMValueRef
compile_decl(LLVMModuleRef module, bytes_t *name, lkit_type_t *type)
{
    LLVMValueRef fn;

    assert(type->tag == LKIT_FUNC);
    assert(type->backend != NULL);

    fn = LLVMAddFunction(module, (char *)name->data, type->backend);
    LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    return fn;
}

int
builtingen_sym_compile(lkit_gitem_t **gitem, void *udata)
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
            initv = compile_expr(module, NULL, expr);
            assert(initv != NULL);
            LLVMSetInitializer(v, initv);
        } else {
            /* declaration, */
            compile_decl(module, name, expr->type);
        }
    }

    return 0;
}
