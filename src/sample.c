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
    if (value->type->tag == LKIT_UNDEF) {
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

            if ((*cond)->type->tag != LKIT_BOOL) {
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
sample_compile_expr(LLVMModuleRef module,
                    LLVMBuilderRef builder,
                    lkit_expr_t *expr)
{
    LLVMValueRef v = NULL;

    if (expr->isref) {
        switch (expr->value.ref->type->tag) {
        case LKIT_FUNC:
            {
                LLVMValueRef ref;

                assert(builder != NULL);

                ref = LLVMGetNamedFunction(module, (char *)expr->name->data);
                if (ref == NULL) {
                    //LLVMDumpModule(module);
                    TRACE("failed to find function %s",
                          (char *)expr->name->data);
                    TR(SAMPLE_COMPILE_EXPR + 1);
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
                            sample_compile_expr(module, builder, *rand);
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
                    TR(SAMPLE_COMPILE_EXPR + 2);
                } else {
                    v = LLVMBuildLoad(builder, ref, NEWVAR((char *)expr->name->data));
                }
            }

            break;

        default:
            TR(SAMPLE_COMPILE_EXPR + 3);
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
                                        NEWVAR("tmp"));
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
                TR(SAMPLE_COMPILE_EXPR + 4);
               break;

            }
        } else {
            TR(SAMPLE_COMPILE_EXPR + 5);
        }
    }

    return v;
}

static void
compile_dynamic_initializer(LLVMModuleRef module,
                            LLVMValueRef v,
                            bytes_t *key,
                            lkit_expr_t *value)
{
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
    LLVMSetLinkage(v, LLVMPrivateLinkage);
}

static LLVMValueRef
sample_compile_decl(LLVMModuleRef module, bytes_t *name, lkit_type_t *type)
{
    assert(type->tag == LKIT_FUNC);
    assert(type->backend != NULL);

    return LLVMAddFunction(module, (char *)name->data, type->backend);
}

int
sample_compile_globals(bytes_t *key,
                       lkit_expr_t *value,
                       void *udata)
{
    LLVMModuleRef module = (LLVMModuleRef)udata;
    LLVMValueRef v;

    TRACE("%p/%s %d", value->type->backend, (char *)key->data, value->isref);
    lkit_type_dump(value->type);
    lkit_expr_dump(value);


    if (value->isref) {
        v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
        LLVMSetLinkage(v, LLVMPrivateLinkage);
        LLVMSetInitializer(v, LLVMGetUndef(value->type->backend));
        compile_dynamic_initializer(module, v, key, value);
    } else {
        if (value->value.literal != NULL) {
            v = LLVMAddGlobal(module, value->type->backend, (char *)key->data);
            LLVMSetLinkage(v, LLVMPrivateLinkage);
            LLVMSetInitializer(v, sample_compile_expr(module, NULL, value));
        } else {
            /* declaration, */
            sample_compile_decl(module, key, value->type);
        }
    }

    return 0;
}
