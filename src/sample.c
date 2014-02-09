#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/sample.h>
#include <mrklkit/util.h>

#include "diag.h"

int
sample_remove_undef(UNUSED bytes_t *key,
                    lkit_expr_t *value,
                    UNUSED void *udata)
{
    lkit_type_t *ty = lkit_type_of_expr(value);

    if (ty->tag == LKIT_UNDEF) {
        char *name = (value->name != NULL) ? (char *)value->name->data : ")(";
        //char *kname = (key != NULL) ? (char *)key->data : "][";

        //TRACE("undef: %s/%s", kname, name);

        if (strcmp(name, "if") == 0) {
            lkit_expr_t **cond, **texpr, **fexpr;

            /*
             * (var if (func undef bool undef undef))
             */
            if ((cond = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *cond, udata);

            if (lkit_type_of_expr(*cond)->tag != LKIT_BOOL) {
                (*cond)->error = 1;
                //lkit_expr_dump(*cond);
                TRRET(SAMPLE_REMOVE_UNDEF + 1);
            }

            if ((texpr = array_get(&value->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *texpr, udata);

            if ((fexpr = array_get(&value->subs, 2)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *fexpr, udata);

            if (lkit_type_cmp((*texpr)->type, (*fexpr)->type) == 0) {
                value->type = (*texpr)->type;
            } else {
                (*texpr)->error = 1;
                (*fexpr)->error = 1;
                //lkit_expr_dump((*texpr));
                //lkit_expr_dump((*fexpr));
                TRRET(SAMPLE_REMOVE_UNDEF + 2);
            }

        } else if (strcmp(name, "print") == 0) {
            lkit_expr_t **arg;

            /*
             * (var print (func undef undef))
             */
            if ((arg = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *arg, udata);
            value->type = (*arg)->type;

        } else if (strcmp(name, "+") == 0 ||
                   strcmp(name, "*") == 0) {

            lkit_expr_t **aexpr, **bexpr;
            array_iter_t it;

            /*
             * (var + (func undef ...))
             * (var * (func undef ...))
             */
            if ((aexpr = array_first(&value->subs, &it)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *aexpr, udata);

            for (bexpr = array_next(&value->subs, &it);
                 bexpr != NULL;
                 bexpr = array_next(&value->subs, &it)) {

                sample_remove_undef(NULL, *bexpr, udata);

                if (lkit_type_cmp((*aexpr)->type, (*bexpr)->type) != 0) {
                    (*bexpr)->error = 1;
                    //lkit_expr_dump((*aexpr));
                    //lkit_expr_dump((*bexpr));
                    TRRET(SAMPLE_REMOVE_UNDEF + 3);
                }
            }
            value->type = (*aexpr)->type;

        } else if (strcmp(name, "item") == 0) {
            /*
             * (var item (func undef struct str))
             * (var item (func undef dict str))
             * (var item (func undef array int))
             */

        } else {
        }
    }
    return 0;
}

static LLVMValueRef
sample_compile_expr(LLVMModuleRef module, LLVMBuilderRef builder, lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;
    lkit_expr_t **a;
    array_iter_t it;

    if (!expr->isref) {
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

            default:
                TR(SAMPLE_COMPILE_EXPR + 1);
               break;

            }
        } else {
            TR(SAMPLE_COMPILE_EXPR + 2);
        }
    } else {
        if (strcmp((char *)expr->name->data, "+") == 0) {
            if (expr->type->tag == LKIT_INT) {
                v = LLVMConstInt(LLVMInt64Type(), 0, 1);
                for (a = array_last(&expr->subs, &it);
                     a != NULL;
                     a = array_prev(&expr->subs, &it)) {

                    v = LLVMBuildAdd(builder, v, sample_compile_expr(module, builder, *a),
                                     NEWVAR("plus"));
                }
            } else {
                TR(SAMPLE_COMPILE_EXPR + 3);
            }
        } else {
            /* load ref */
            LLVMValueRef ref;
            ref = LLVMGetNamedGlobal(module, (char *)expr->name->data);
            if (ref == NULL) {
                TR(SAMPLE_COMPILE_EXPR + 4);
            }
            v = LLVMBuildLoad(builder, ref, NEWVAR((char *)expr->name->data));
        }
    }

    return v;
}


int
sample_compile_globals(UNUSED bytes_t *key,
                   lkit_expr_t *value,
                   void *udata)
{
    LLVMModuleRef module = (LLVMModuleRef)udata;
    LLVMValueRef v;
    //lkit_type_t *rty = lkit_type_of_expr(value);

    switch (value->type->tag) {
    case LKIT_INT:
        TRACE("%p/%s %d",
              value->type->backend, (char *)key->data, value->isref);
        v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
        if (!value->isref) {
            if (value->value.literal != NULL) {
                LLVMSetInitializer(v,
                                   LLVMConstInt(value->type->backend,
                                   *(uint64_t *)value->value.literal->body,
                                   1));
            } else {
                LLVMSetExternallyInitialized(v, 1);
            }
        } else {
            char buf[1024];
            LLVMBasicBlockRef bb;
            LLVMBuilderRef builder;
            LLVMValueRef fn;

            /* dynamic initializer */
            //lkit_type_dump(value->type);
            //lkit_expr_dump(value);
            snprintf(buf, sizeof(buf), ".mrklkit.init.%s", (char *)key->data);
            fn = LLVMAddFunction(module,
                                 buf,
                                 LLVMFunctionType(LLVMVoidType(), NULL, 0, 0));
            builder = LLVMCreateBuilder();
            bb = LLVMAppendBasicBlock(fn, "L1");
            LLVMPositionBuilderAtEnd(builder, bb);
            LLVMBuildStore(builder, sample_compile_expr(module, builder, value), v);
            //LLVMInsertIntoBuilder(builder, LLVMBuildStore());
            LLVMBuildRetVoid(builder);
            LLVMDisposeBuilder(builder);
            //TRRET(SAMPLE_COMPILE_GLOBALS + 1);
        }

        break;

    case LKIT_FLOAT:
        //TRACE("%p/%s %d", value->type->backend, (char *)key->data, value->isref);
        v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
        if (!value->isref) {
            if (value->value.literal != NULL) {
                LLVMSetInitializer(v,
                                   LLVMConstReal(value->type->backend,
                                                 *(double *)value->value.literal->body));
            } else {
                LLVMSetExternallyInitialized(v, 1);
            }
        } else {
            /* dynamic initializer */
            TRRET(SAMPLE_COMPILE_GLOBALS + 2);
        }

        break;

    case LKIT_BOOL:
        //TRACE("%p/%s %d", value->type->backend, (char *)key->data, value->isref);
        v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
        if (!value->isref) {
            if (value->value.literal != NULL) {
                LLVMSetInitializer(v,
                                   LLVMConstInt(value->type->backend,
                                   *((char *)(value->value.literal->body)), 1));
            } else {
                LLVMSetExternallyInitialized(v, 1);
            }
        } else {
            /* dynamic initializer */
            TRRET(SAMPLE_COMPILE_GLOBALS + 3);
        }

        break;

    case LKIT_STR:
        //TRACE("%p/%s %d", value->type->backend, (char *)key->data, value->isref);
        if (!value->isref) {
            if (value->value.literal != NULL) {
                bytes_t *b;
                LLVMTypeRef strty;

                b = (bytes_t *)value->value.literal->body;
                strty = LLVMArrayType(value->type->backend, b->sz + 1);
                v = LLVMAddGlobal(module, strty, (char *)key->data);
                //LLVMSetLinkage(v, LLVMInternalLinkage);
                LLVMSetInitializer(v, LLVMConstString((char *)b->data, b->sz, 0));

            } else {
                v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
                LLVMSetExternallyInitialized(v, 1);
            }
        } else {
            /* dynamic initializer */
            TRRET(SAMPLE_COMPILE_GLOBALS + 4);
        }

        break;

    case LKIT_ARRAY:
    case LKIT_DICT:
    case LKIT_STRUCT:
    case LKIT_FUNC:
        //TRACE("%p/%s %d", value->type->backend, (char *)key->data, value->isref);
        if (value->type->backend != NULL) {
            v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
            if (!value->isref) {
                if (value->value.literal != NULL) {
                    /* no definitions ATM */
                    TRRET(SAMPLE_COMPILE_GLOBALS + 5);
                } else {
                    LLVMSetExternallyInitialized(v, 1);
                }
            } else {
                /* no constant initializer */
                TRRET(SAMPLE_COMPILE_GLOBALS + 6);
            }
        } else {
            /* undef */
        }


        break;

    default:
        break;

    }

    return 0;
}
