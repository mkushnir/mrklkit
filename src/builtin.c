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

/*
 * bytes_t *, lkit_expr_t *
 */
static lkit_expr_t root;

int
builtin_sym_parse(array_t *form, array_iter_t *it)
{
    return lkit_parse_exprdef(&root, form, it);
}

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


static int
remove_undef(lkit_expr_t *value)
{
    char *name = (value->name != NULL) ? (char *)value->name->data : ")(";

    //TRACEN("%s: ", name);
    if (value->type->tag == LKIT_UNDEF) {

        //TRACEC("undef\n");

        if (strcmp(name, "if") == 0) {
            lkit_expr_t **cond, **texpr, **fexpr;
            lkit_type_t *tty, *fty;

            /*
             * (sym if (func undef bool undef undef))
             */
            if ((cond = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }
            if (remove_undef(*cond) != 0) {
                TRRET(REMOVE_UNDEF + 1);
            }

            if ((*cond)->type->tag != LKIT_BOOL) {
                (*cond)->error = 1;
                //lkit_expr_dump(*cond);
                TRRET(REMOVE_UNDEF + 2);
            }

            if ((texpr = array_get(&value->subs, 1)) == NULL) {
                FAIL("array_get");
            }

            if (remove_undef(*texpr) != 0) {
                TRRET(REMOVE_UNDEF + 3);
            }

            if ((fexpr = array_get(&value->subs, 2)) == NULL) {
                FAIL("array_get");
            }

            if (remove_undef(*fexpr) != 0) {
                TRRET(REMOVE_UNDEF + 4);
            }

            tty = (*texpr)->type;
            fty = (*fexpr)->type;

            if (lkit_type_cmp(tty, fty) == 0) {
                value->type = tty;
            } else {
                (*texpr)->error = 1;
                (*fexpr)->error = 1;
                //lkit_expr_dump((*texpr));
                //lkit_expr_dump((*fexpr));
                TRRET(REMOVE_UNDEF + 5);
            }

        } else if (strcmp(name, "print") == 0) {
            lkit_expr_t **arg;

            /*
             * (sym print (func undef undef))
             */
            if ((arg = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }

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

            /*
             * (sym +|*|%|-|/ (func undef undef ...))
             */
            if ((aexpr = array_first(&value->subs, &it)) == NULL) {
                FAIL("array_get");
            }

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

            if ((cont = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }

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

                    if ((name = array_get(&value->subs, 1)) == NULL) {
                        FAIL("array_get");
                    }

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR || !LKIT_EXPR_CONSTANT(*name)) {
                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    bname = (bytes_t *)(*name)->value.literal->body;
                    if ((elty = lkit_struct_get_field_type(ts, bname)) == NULL) {
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

                    if ((name = array_get(&value->subs, 1)) == NULL) {
                        FAIL("array_get");
                    }

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR || !LKIT_EXPR_CONSTANT(*name)) {
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

                    if ((name = array_get(&value->subs, 1)) == NULL) {
                        FAIL("array_get");
                    }

                    /* constant int */
                    if ((*name)->type->tag != LKIT_INT || !LKIT_EXPR_CONSTANT(*name)) {
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

            if ((dflt = array_get(&value->subs, 2)) == NULL) {
                FAIL("array_get");
            }

            if (lkit_type_cmp(ty, (*dflt)->type) != 0) {
                (*dflt)->error = 1;
                TRRET(REMOVE_UNDEF + 20);
            }

        } else {
            /* not undef */
        }
    } else {
        //TRACEC("\n");
    }
    return 0;
}

int
_acb(lkit_gitem_t **gitem, UNUSED void *udata)
{
    return remove_undef((*gitem)->expr);
}

int
builtin_remove_undef(void)
{
    return array_traverse(&root.glist, (array_traverser_t)_acb, NULL);
}

static LLVMValueRef
compile_builtin(UNUSED LLVMModuleRef module,
                       UNUSED LLVMBuilderRef builder,
                       UNUSED lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;

    //lkit_expr_dump(expr);
    TRACE("builtin %s", expr->name->data);

    //if () {
    //}

    return v;
}

static LLVMValueRef
builtin_compile_expr(LLVMModuleRef module,
                    LLVMBuilderRef builder,
                    lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;

    if (expr->isref) {
        switch (expr->value.ref->type->tag) {
        case LKIT_FUNC:
            assert(builder != NULL);

            if ((v = compile_builtin(module, builder, expr)) == NULL) {
                LLVMValueRef ref;
                /* first try user defined functions */
                ref = LLVMGetNamedFunction(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TRACE("failed to find builtin %s",
                          (char *)expr->name->data);
                    TR(BUILTIN_COMPILE_EXPR + 1);
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
                            builtin_compile_expr(module, builder, *rand);
                    }

                    v = LLVMBuildCall(builder,
                                      ref,
                                      args,
                                      expr->subs.elnum,
                                      NEWVAR((char *)expr->name->data));
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
                    TR(BUILTIN_COMPILE_EXPR + 2);
                } else {
                    v = LLVMBuildLoad(builder, ref, NEWVAR((char *)expr->name->data));
                }
            }

            break;

        default:
            TR(BUILTIN_COMPILE_EXPR + 3);
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
                    LLVMValueRef tmp;
                    bytes_t *b;

                    b = (bytes_t *)expr->value.literal->body;
                    tmp = LLVMAddGlobal(module,
                                        LLVMArrayType(LLVMIntType(8), b->sz + 1),
                                        NEWVAR(".mrklkit.tmp"));
                    LLVMSetLinkage(tmp, LLVMPrivateLinkage);
                    LLVMSetInitializer(tmp, LLVMConstString((char *)b->data, b->sz, 0));
                    v = LLVMConstBitCast(tmp, expr->type->backend);
                }
                break;

            case LKIT_ARRAY:
            case LKIT_DICT:
            case LKIT_STRUCT:

            default:

                //lkit_expr_dump(expr);
                //LLVMDumpModule(module);
                TR(BUILTIN_COMPILE_EXPR + 4);
               break;

            }
        } else {
            TR(BUILTIN_COMPILE_EXPR + 5);
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
    LLVMValueRef fn;

    /* dynamic initializer */
    //lkit_type_dump(value->type);
    //lkit_expr_dump(value);
    snprintf(buf, sizeof(buf), ".mrklkit.init.%s", (char *)name->data);
    fn = LLVMAddFunction(module,
                         buf,
                         LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
    builder = LLVMCreateBuilder();
    bb = LLVMAppendBasicBlock(fn, "L1");
    LLVMPositionBuilderAtEnd(builder, bb);
    LLVMBuildStore(builder, builtin_compile_expr(module, builder, value), v);
    //LLVMInsertIntoBuilder(builder, LLVMBuildStore());
    LLVMBuildRetVoid(builder);
    LLVMDisposeBuilder(builder);
    LLVMSetLinkage(v, LLVMPrivateLinkage);
}

static LLVMValueRef
builtin_compile_decl(LLVMModuleRef module, bytes_t *name, lkit_type_t *type)
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


    if (expr->isref) {
        v = LLVMAddGlobal(module, expr->type->backend, (char *)name->data);
        LLVMSetLinkage(v, LLVMPrivateLinkage);
        LLVMSetInitializer(v, LLVMGetUndef(expr->type->backend));
        compile_dynamic_initializer(module, v, name, expr);
    } else {
        if (expr->value.literal != NULL) {
            v = LLVMAddGlobal(module, expr->type->backend, (char *)name->data);
            LLVMSetLinkage(v, LLVMPrivateLinkage);
            LLVMSetInitializer(v, builtin_compile_expr(module, NULL, expr));
        } else {
            /* declaration, */
            builtin_compile_decl(module, name, expr->type);
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

