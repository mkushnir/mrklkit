#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/builtin.h>
#include <mrklkit/util.h>

#include "diag.h"

static LLVMValueRef compile_expr(LLVMModuleRef, LLVMBuilderRef, lkit_expr_t *);

static lkit_expr_t root;

int
builtin_sym_parse(array_t *form, array_iter_t *it)
{
    return lkit_parse_exprdef(&root, form, it);
}
/*
 * libc decls:
 *  (sym printf (func int str ...))
 */

/*
 * (sym if (func undef bool undef undef))
 * (sym print (func undef undef))
 *
 * (sym + (func undef undef ...))
 * (sym - (func undef undef ...))
 * (sym / (func undef undef ...))
 * (sym * (func undef undef ...))
 * (sym % (func undef undef ...))
 * (sym min (func undef undef ...))
 * (sym max (func undef undef ...))
 *
 * (sym and (func bool bool bool ...))
 * (sym or (func bool bool bool ...))
 * (sym not (func bool bool))
 *
 * (sym == (func bool undef undef))
 * (sym != (func bool undef undef))
 * (sym < (func bool undef undef))
 * (sym <= (func bool undef undef))
 * (sym > (func bool undef undef))
 * (sym >= (func bool undef undef))
 *
 * (sym get (func undef undef undef undef))
 * (sym set (func undef undef undef))
 * (sym del (func undef undef undef))
 *
 * (sym itof (func float int float))
 * (sym ftoi (func int float int))
 * (sym tostr (func str undef))
 * (sym printf (func str str ...)) str|int|float|obj ?
 *
 * (sym map (func undef undef undef))
 * (sym reduce (func undef undef undef))
 */

UNUSED static int
check_function(bytes_t *name)
{
    char *n = (char *)name->data;
    if (strcmp(n, "if") == 0) {
        return 0;
    }
    if (strcmp(n, "print") == 0) {
        return 0;
    }
    if (strcmp(n, "+") == 0) {
        return 0;
    }
    if (strcmp(n, "-") == 0) {
        return 0;
    }
    return 1;
}


static int
remove_undef(lkit_expr_t *value)
{
    char *name = (value->name != NULL) ? (char *)value->name->data : ")(";

    //TRACEN("%s: ", name);
    if (value->type->tag == LKIT_UNDEF) {

        //TRACEC("undef\n");

        if (strcmp(name, "if") == 0) {
            lkit_expr_t **cond, **texp, **fexp;
            lkit_type_t *tty, *fty;

            //value->isbuiltin = 1;

            /*
             * (sym if (func undef bool undef undef))
             */
            cond = array_get(&value->subs, 0);
            assert(cond != NULL);

            if (remove_undef(*cond) != 0) {
                TRRET(REMOVE_UNDEF + 1);
            }

            if ((*cond)->type->tag != LKIT_BOOL) {
                (*cond)->error = 1;
                //lkit_expr_dump(*cond);
                TRRET(REMOVE_UNDEF + 2);
            }

            texp = array_get(&value->subs, 1);
            assert(texp != NULL);

            if (remove_undef(*texp) != 0) {
                TRRET(REMOVE_UNDEF + 3);
            }

            fexp = array_get(&value->subs, 2);
            assert(fexp != NULL);

            if (remove_undef(*fexp) != 0) {
                TRRET(REMOVE_UNDEF + 4);
            }

            tty = (*texp)->type;
            fty = (*fexp)->type;

            if (lkit_type_cmp(tty, fty) == 0) {
                value->type = tty;
            } else {
                (*texp)->error = 1;
                (*fexp)->error = 1;
                //lkit_expr_dump((*texp));
                //lkit_expr_dump((*fexp));
                TRRET(REMOVE_UNDEF + 5);
            }

        } else if (strcmp(name, "print") == 0) {
            lkit_expr_t **arg;

            //value->isbuiltin = 1;

            /*
             * (sym print (func undef undef))
             */
            arg = array_get(&value->subs, 0);
            assert(arg != NULL);

            if (remove_undef(*arg) != 0) {
                TRRET(REMOVE_UNDEF + 6);
            }

            value->type = (*arg)->type;

        } else if (strcmp(name, "+") == 0 ||
                   strcmp(name, "*") == 0 ||
                   strcmp(name, "%") == 0 ||
                   strcmp(name, "-") == 0 ||
                   strcmp(name, "/") == 0 ||
                   strcmp(name, "min") == 0 ||
                   strcmp(name, "max") == 0) {

            lkit_expr_t **aexpr, **bexpr;
            array_iter_t it;

            //value->isbuiltin = 1;

            /*
             * (sym +|*|%|-|/ (func undef undef ...))
             */
            aexpr = array_first(&value->subs, &it);
            assert(axpr != NULL);

            if (remove_undef(*aexpr) != 0) {
                TRRET(REMOVE_UNDEF + 7);
            }

            for (bexpr = array_next(&value->subs, &it);
                 bexpr != NULL;
                 bexpr = array_next(&value->subs, &it)) {

                if (remove_undef(*bexpr) != 0) {
                    TRRET(REMOVE_UNDEF + 8);
                }

                if (lkit_type_cmp((*aexpr)->type, (*bexpr)->type) != 0) {
                    (*bexpr)->error = 1;
                    //lkit_expr_dump((*aexpr));
                    //lkit_expr_dump((*bexpr));
                    TRRET(REMOVE_UNDEF + 9);
                }
            }
            value->type = (*aexpr)->type;

        } else if (strcmp(name, "==") == 0 ||
                   strcmp(name, "!=") == 0 ||
                   strcmp(name, "<") == 0 ||
                   strcmp(name, "<=") == 0 ||
                   strcmp(name, ">") == 0 ||
                   strcmp(name, ">=") == 0) {

            lkit_expr_t **expr;
            array_iter_t it;

            //value->isbuiltin = 1;

            for (expr = array_first(&value->subs, &it);
                 expr != NULL;
                 expr = array_next(&value->subs, &it)) {

                if (remove_undef(*expr) != 0) {
                    TRRET(REMOVE_UNDEF + 10);
                }

                if ((*expr)->type->tag != LKIT_BOOL) {
                    (*expr)->error = 1;
                    TRRET(REMOVE_UNDEF + 11);
                }
            }

        } else if (strcmp(name, "get") == 0) {
            /*
             * (sym get (func undef struct conststr undef))
             * (sym get (func undef dict conststr undef))
             * (sym get (func undef array constint undef))
             */
            lkit_expr_t **cont, **dflt;
            lkit_type_t *ty;

            //value->isbuiltin = 1;

            cont = array_get(&value->subs, 0);
            assert(cont != NULL);

            if (remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 12);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_struct_t *ts = (lkit_struct_t *)*cont;
                    lkit_expr_t **name;
                    bytes_t *bname;
                    lkit_type_t *elty;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    bname = (bytes_t *)(*name)->value.literal->body;
                    if ((elty =
                            lkit_struct_get_field_type(ts, bname)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 14);
                    }

                    value->type = elty;
                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *td = (lkit_dict_t *)*cont;
                    lkit_type_t *elty;
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 15);
                    }

                    if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 16);
                    }

                    value->type = elty;
                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_array_t *ta = (lkit_array_t *)*cont;
                    lkit_type_t *elty;
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant int */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 17);
                    }

                    if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 18);
                    }

                    value->type = elty;
                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 19);
            }

            dflt = array_get(&value->subs, 2);
            assert(dflt != NULL);

            if (remove_undef(*dflt) != 0) {
                TRRET(REMOVE_UNDEF + 20);
            }

            if (lkit_type_cmp(value->type, (*dflt)->type) != 0) {
                (*dflt)->error = 1;
                TRRET(REMOVE_UNDEF + 21);
            }

        } else if (strcmp(name, "set") == 0) {
            /*
             * (sym set (func undef struct conststr undef))
             * (sym set (func undef dict conststr undef))
             * (sym set (func undef array constint undef))
             */
            lkit_expr_t **cont, **setv;
            lkit_type_t *ty;
            lkit_type_t *elty;

            //value->isbuiltin = 1;

            cont = array_get(&value->subs, 0);
            assert(cont != NULL);

            if (remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 12);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_struct_t *ts = (lkit_struct_t *)*cont;
                    lkit_expr_t **name;
                    bytes_t *bname;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    bname = (bytes_t *)(*name)->value.literal->body;
                    if ((elty =
                            lkit_struct_get_field_type(ts, bname)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 14);
                    }

                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *td = (lkit_dict_t *)*cont;
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 14);
                    }

                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_array_t *ta = (lkit_array_t *)*cont;
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 14);
                    }

                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 19);

            }

            value->type = ty;

            setv = array_get(&value->subs, 2);
            assert(setv != NULL);

            if (remove_undef(*setv) != 0) {
                TRRET(REMOVE_UNDEF + 20);
            }

            if (lkit_type_cmp((*setv)->type, elty) != 0) {
                (*setv)->error = 1;
                TRRET(REMOVE_UNDEF + 20);
            }

        } else if (strcmp(name, "del") == 0) {
            /*
             * (sym del (func undef struct conststr))
             * (sym del (func undef dict conststr))
             * (sym del (func undef array constint))
             */
            lkit_expr_t **cont;
            lkit_type_t *ty;

            //value->isbuiltin = 1;

            cont = array_get(&value->subs, 0);
            assert(cont != NULL);

            if (remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 12);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }
                }
                break;

            case LKIT_DICT:
                {
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }
                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_expr_t **name;

                    name = array_get(&value->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }
                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 19);

            }

            value->type = ty;

        } else if (strcmp(name, "tostr") == 0) {
            /*
             * (sym tostr (func str undef))
             */
            lkit_expr_t **arg;

            //value->isbuiltin = 1;

            arg = array_get(&value->subs, 0);
            assert(arg != NULL);

            if (remove_undef(*arg) != 0) {
                TRRET(REMOVE_UNDEF + 12);
            }

        } else {
            lkit_expr_t *ref;

            /* not builtin */
            if ((ref = dict_get_item(&root.ctx, value->name)) == NULL) {
                TRRET(REMOVE_UNDEF + 13);
            }
            value->type = ref->type;
        }

    } else {
        //TRACEC("\n");
    }
    if (value->type->tag == LKIT_UNDEF) {
        lkit_type_dump(value->type);
        lkit_expr_dump(value);
        assert(0);
    }
    return 0;
}

int
_acb(lkit_gitem_t **gitem, void *udata)
{
    int (*cb)(lkit_expr_t *) = udata;
    return cb((*gitem)->expr);
}

int
builtin_remove_undef(void)
{
    int res = array_traverse(&root.glist,
                             (array_traverser_t)_acb,
                             remove_undef);
    //array_traverse(&root.glist, (array_traverser_t)_acb, lkit_expr_dump);
    return res;
}

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
compile_function(LLVMModuleRef module,
                 LLVMBuilderRef builder,
                 lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;
    char *name = (char *)expr->name->data;

    //lkit_expr_dump(expr);
    //TRACE("trying %s", expr->name->data);

    if (strcmp(name, "if") == 0) {
        //lkit_expr_t **arg;
        //LLVMValueRef res, cond, texp, fexp, fn, iexp[2];
        //LLVMBasicBlockRef currblock, endblock, tblock, fblock, iblock[2];

        //currblock = LLVMGetInsertBlock(builder);
        //fn = LLVMGetBasicBlockParent(currblock);

        ///**/
        //arg = array_get(&expr->subs, 0);
        //cond = compile_expr(module, builder, *arg);
        //assert(cond != NULL);

        //endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

        ///**/
        //tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
        //LLVMPositionBuilderAtEnd(builder, tblock);

        //arg = array_get(&expr->subs, 1);
        //texp = compile_expr(module, builder, *arg);
        //assert(texp != NULL);
        //res = LLVMBuildBr(builder, endblock);
        //assert(res != NULL);

        ///**/
        //fblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.false"));
        //LLVMPositionBuilderAtEnd(builder, fblock);

        //arg = array_get(&expr->subs, 2);
        //fexp = compile_expr(module, builder, *arg);
        //assert(fexp != NULL);
        //res = LLVMBuildBr(builder, endblock);
        //assert(res != NULL);

        ///**/
        //LLVMPositionBuilderAtEnd(builder, currblock);
        //LLVMBuildCondBr(builder, cond, tblock, fblock);
        //LLVMPositionBuilderAtEnd(builder, endblock);
        //v = LLVMBuildPhi(builder, expr->type->backend, NEWVAR("if.result"));
        //iexp[0] = texp;
        //iblock[0] = LLVMIsConstant(texp) ? tblock :
        //            LLVMGetInstructionParent(texp);
        //iexp[1] = fexp;
        //iblock[1] = LLVMIsConstant(fexp) ? fblock :
        //            LLVMGetInstructionParent(fexp);
        //LLVMAddIncoming(v, iexp, iblock, 2);

        lkit_expr_t **cexpr, **texpr, **fexpr;
        cexpr = array_get(&expr->subs, 0);
        texpr = array_get(&expr->subs, 1);
        fexpr = array_get(&expr->subs, 2);
        v = compile_if(module, builder, *cexpr, *texpr, *fexpr, expr->type);



    } else if (strcmp(name , "print") == 0) {
        LLVMValueRef fn, res, args[2];

        if ((fn = LLVMGetNamedFunction(module, "printf")) == NULL) {
            TR(COMPILE_FUNCTION + 1);
        } else {
            lkit_expr_t **arg;
            arg = array_get(&expr->subs, 0);
            v = compile_expr(module, builder, *arg);
            assert(v != NULL);

            switch (expr->type->tag) {
            case LKIT_INT:
                args[0] = LLVMBuildGlobalStringPtr(builder,
                                                   "%ld \n",
                                                   NEWVAR("printf.fmt"));
                break;

            case LKIT_FLOAT:
                args[0] = LLVMBuildGlobalStringPtr(builder,
                                                   "%lf \n",
                                                   NEWVAR("printf.fmt"));
                break;

            case LKIT_STR:
                args[0] = LLVMBuildGlobalStringPtr(builder,
                                                   "%s \n",
                                                   NEWVAR("printf.fmt"));
                break;

            case LKIT_BOOL:
                args[0] = LLVMBuildGlobalStringPtr(builder,
                                                   "%hhd \n",
                                                   NEWVAR("printf.fmt"));
                break;

            default:
                args[0] = LLVMBuildGlobalStringPtr(builder,
                                                   "%p \n",
                                                   NEWVAR("printf.fmt"));
                break;

            }

            args[1] = v;
            res = LLVMBuildCall(builder,
                              fn,
                              args,
                              2,
                              NEWVAR(name));
        }


    } else if (strcmp(name , "+") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        v = LLVMConstInt(LLVMInt64Type(), 0, 1);

        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                v = NULL;
                TR(COMPILE_FUNCTION + 2);
                break;
            }

            v = LLVMBuildAdd(builder, v, rand, NEWVAR("plus"));
        }

    } else if (strcmp(name , "-") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        v = LLVMConstInt(LLVMInt64Type(), 0, 1);

        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            LLVMValueRef rand;

            if ((rand = compile_expr(module, builder, *arg)) == NULL) {
                v = NULL;
                TR(COMPILE_FUNCTION + 2);
                break;
            }

            v = LLVMBuildSub(builder, v, rand, NEWVAR("minus"));
        }

    } else {
    }

    return v;
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
                    TRACE("failed to find builtin %s",
                          (char *)expr->name->data);
                    TR(COMPILE_EXPR + 1);
                } else {
                    lkit_expr_t **rand;
                    array_iter_t it;
                    LLVMValueRef *args = NULL;
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
        case LKIT_STR:
            {
                LLVMValueRef ref;

                assert(builder != NULL);

                ref = LLVMGetNamedGlobal(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TR(COMPILE_EXPR + 2);
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
                                TR(COMPILE_EXPR + 3);
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
            TR(COMPILE_EXPR + 3);
            break;

        }

    } else {
        if (expr->value.literal != NULL) {

            assert(builder != NULL);
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
                    LLVMValueRef tmp;
                    bytes_t *b;

                    b = (bytes_t *)expr->value.literal->body;
                    tmp = LLVMAddGlobal(module,
                                        LLVMArrayType(LLVMIntType(8),
                                                      b->sz + 1),
                                        NEWVAR(".mrklkit.tmp"));
                    LLVMSetLinkage(tmp, LLVMPrivateLinkage);
                    LLVMSetInitializer(tmp,
                                       LLVMConstString((char *)b->data,
                                                       b->sz,
                                                       0));
                    v = LLVMConstBitCast(tmp, expr->type->backend);
                }
                break;

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:

            default:

                //lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(COMPILE_EXPR + 4);
               break;

            }
        } else {
            TR(COMPILE_EXPR + 5);
        }
    }

    return v;
}

static void
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
                         LLVMFunctionType(LLVMIntType(64), NULL, 0, 0));
                         //LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    builder = LLVMCreateBuilder();

    //bcond = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit.cond"));
    //LLVMPositionBuilderAtEnd(builder, bcond);
    //chk = LLVMBuildLoad(builder, chkv, NEWVAR("chk"));
    //LLVMBuildCondBr(builder, )


    if (value->lazy_init) {
        LLVMValueRef res, cond;
        LLVMBasicBlockRef currblock, endblock, tblock, fblock;

        snprintf(buf, sizeof(buf), ".mrklkit.init.done.%s", (char *)name->data);
        chkv = LLVMAddGlobal(module, LLVMIntType(1), buf);
        LLVMSetLinkage(chkv, LLVMPrivateLinkage);
        LLVMSetInitializer(chkv, LLVMConstInt(LLVMIntType(1), 0, 0));

        //compile_if(module, builder, new_expr(), value, NULL, NULL);
        currblock = LLVMAppendBasicBlock(fn, NEWVAR("L.dyninit"));
        tblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.true"));
        fblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.false"));
        endblock = LLVMAppendBasicBlock(fn, NEWVAR("L.if.end"));

        LLVMPositionBuilderAtEnd(builder, tblock);

        storedv = compile_expr(module, builder, value);
        assert(storedv != NULL);
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
        storedv = compile_expr(module, builder, value);
        assert(storedv != NULL);
        LLVMBuildStore(builder, storedv, v);
    }

    //LLVMBuildRetVoid(builder);
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt64Type(), 0, 0));
    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(v, LLVMPrivateLinkage);
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

static int
sym_compile(lkit_gitem_t **gitem, void *udata)
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
        compile_dynamic_initializer(module, v, name, expr);
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

int
builtin_sym_compile(LLVMModuleRef module)
{
    return array_traverse(&root.glist, (array_traverser_t)sym_compile, module);
}


void
builtin_init(void)
{
    lexpr_init_ctx(&root);
}

void
builtin_fini(void)
{
    lexpr_fini_ctx(&root);
}

