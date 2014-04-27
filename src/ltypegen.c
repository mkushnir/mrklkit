#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>

#include "diag.h"


/**
 * Generators
 *
 */

int
ltype_compile(lkit_type_t *ty,
                          UNUSED void *udata)
{
    lkit_struct_t *ts;
    lkit_func_t *tf;
    LLVMTypeRef retty, *bfields = NULL;
    lkit_type_t **field;
    array_iter_t it;

    if (ty->backend != NULL) {
        return 0;
    }

    switch (ty->tag) {
    case LKIT_VOID:
        ty->backend = LLVMVoidType();
        break;

    case LKIT_INT:
        ty->backend = LLVMInt64Type();
        break;

    case LKIT_STR:
        {
            LLVMTypeRef fields[3];

            /*
             * bytes_t *
             */
            fields[0] = LLVMInt64Type();
            fields[1] = LLVMInt64Type();
            fields[2] = LLVMArrayType(LLVMInt8Type(), 0);
            ty->backend = LLVMPointerType(LLVMStructType(fields, 3, 0), 0);
        }
        break;

    case LKIT_FLOAT:
        ty->backend = LLVMDoubleType();
        break;

    case LKIT_BOOL:
        ty->backend = LLVMInt1Type();
        break;

    case LKIT_ANY:
        ty->backend = LLVMPointerType(LLVMVoidType(), 0);
        break;

    case LKIT_UNDEF:
        //ty->backend = LLVMPointerType(LLVMInt8Type(), 0);
        ty->backend = LLVMPointerType(LLVMVoidType(), 0);
        break;

    case LKIT_ARRAY:
        ty->backend = LLVMPointerType(LLVMStructType(NULL, 0, 0), 0);
        break;

    case LKIT_DICT:
        ty->backend = LLVMPointerType(LLVMStructType(NULL, 0, 0), 0);
        break;

    case LKIT_STRUCT:
        ts = (lkit_struct_t *)ty;
        if ((bfields = malloc(sizeof(LLVMTypeRef) * ts->fields.elnum)) == NULL) {
            FAIL("malloc");
        }
        for (field = array_first(&ts->fields, &it);
             field != NULL;
             field = array_next(&ts->fields, &it)) {

            if ((*field)->tag == LKIT_UNDEF || (*field)->tag == LKIT_VARARG) {
                goto end_struct;
            }

            if (ltype_compile(*field, udata) != 0) {
                TRRET(LTYPE_COMPILE + 1);
            }

            bfields[it.iter] = (*field)->backend;
            ts->base.rtsz += (*field)->rtsz;
        }

        ts->deref_backend = LLVMStructType(bfields, ts->fields.elnum, 0);
        ty->backend = LLVMPointerType(ts->deref_backend, 0);

end_struct:
        break;

    case LKIT_FUNC:
        tf = (lkit_func_t *)ty;
        if ((bfields = malloc(sizeof(LLVMTypeRef) * tf->fields.elnum - 1)) == NULL) {
            FAIL("malloc");
        }

        if ((field = array_first(&tf->fields, &it)) == NULL) {
            FAIL("array_first");
        }
        //if ((*field)->tag == LKIT_UNDEF) {
        //    goto end_func;
        //}
        if ((*field)->tag == LKIT_VARARG) {
            TRRET(LTYPE_COMPILE + 2);
        }
        if (ltype_compile(*field, udata) != 0) {
            TRRET(LTYPE_COMPILE + 3);
        }

        retty = (*field)->backend;

        for (field = array_next(&tf->fields, &it);
             field != NULL;
             field = array_next(&tf->fields, &it)) {

            //if ((*field)->tag == LKIT_UNDEF) {
            //    goto end_func;
            //}

            if ((*field)->tag == LKIT_VARARG) {
                break;
            }

            if (ltype_compile(*field, udata) != 0) {
                TRRET(LTYPE_COMPILE + 4);
            }

            bfields[it.iter - 1] = (*field)->backend;
        }

        if ((field = array_last(&tf->fields, &it)) == NULL) {
            FAIL("array_first");
        }
        if ((*field)->tag == LKIT_VARARG) {
            ty->backend = LLVMFunctionType(retty, bfields, tf->fields.elnum - 2, 1);
        } else {
            ty->backend = LLVMFunctionType(retty, bfields, tf->fields.elnum - 1, 0);
        }


//end_func:
        break;

    default:
        break;

    }

    if (bfields != NULL) {
        free(bfields);
        bfields = NULL;
    }

    return 0;
}

int
ltype_compile_methods(lkit_type_t *ty,
                      LLVMModuleRef module,
                      bytes_t *name)
{
    char *buf1 = NULL, *buf2 = NULL;

    switch (ty->tag) {
    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            array_iter_t it;
            LLVMBuilderRef b1, b2;
            LLVMValueRef fn1, fn2, cast1, cast2;
            LLVMTypeRef argty;
            lkit_type_t **fty;
            LLVMBasicBlockRef bb;

            if ((buf1 = malloc(name->sz + 64)) == NULL) {
                FAIL("malloc");
            }
            if ((buf2 = malloc(name->sz + 64)) == NULL) {
                FAIL("malloc");
            }

            ts = (lkit_struct_t *)ty;

            b1 = LLVMCreateBuilder();
            b2 = LLVMCreateBuilder();

            snprintf(buf1, name->sz + 32, ".mrklkit.init.%s", name->data);
            snprintf(buf2, name->sz + 32, ".mrklkit.fini.%s", name->data);

            if (LLVMGetNamedFunction(module, buf1) != NULL) {
                TRRET(LTYPE_COMPILE_METHODS + 1);
            }
            if (LLVMGetNamedFunction(module, buf1) != NULL) {
                TRRET(LTYPE_COMPILE_METHODS + 2);
            }

            argty = LLVMPointerType(LLVMVoidType(), 0);

            fn1 = LLVMAddFunction(module,
                                  buf1,
                                  LLVMFunctionType(LLVMVoidType(),
                                                   &argty,
                                                   1,
                                                   0));
            fn2 = LLVMAddFunction(module,
                                  buf2,
                                  LLVMFunctionType(LLVMVoidType(),
                                                   &argty,
                                                   1,
                                                   0));

            bb = LLVMAppendBasicBlock(fn1, NEWVAR(".BB"));
            LLVMPositionBuilderAtEnd(b1, bb);
            bb = LLVMAppendBasicBlock(fn2, NEWVAR(".BB"));
            LLVMPositionBuilderAtEnd(b2, bb);

            cast1 = LLVMBuildPointerCast(b1,
                                         LLVMGetParam(fn1, 0),
                                         ts->base.backend,
                                         NEWVAR("cast"));
            cast2 = LLVMBuildPointerCast(b2,
                                         LLVMGetParam(fn2, 0),
                                         ts->base.backend,
                                         NEWVAR("cast"));

            for (fty = array_first(&ts->fields, &it);
                 fty != NULL;
                 fty = array_next(&ts->fields, &it)) {

                LLVMValueRef gep1, gep2;
                const char *dtor_name = NULL;

                gep1 = LLVMBuildStructGEP(b1, cast1, it.iter, NEWVAR("gep"));
                gep2 = LLVMBuildStructGEP(b2, cast2, it.iter, NEWVAR("gep"));

#define CALLDTOR \
{ \
    LLVMValueRef pnull, dtor; \
    pnull = LLVMConstPointerNull((*fty)->backend); \
    LLVMBuildStore(b1, pnull, gep1); \
    if ((dtor = LLVMGetNamedFunction(module, \
                                     dtor_name)) == NULL) { \
        TRACE("no name: %s", dtor_name); \
        TRRET(LTYPE_COMPILE_METHODS + 3); \
    } \
    LLVMBuildCall(b2, dtor, &gep2, 1, NEWVAR("call")); \
}

                switch ((*fty)->tag) {
                case LKIT_STR:
                    dtor_name = "bytes_destroy";
                    CALLDTOR;
                    break;
                case LKIT_ARRAY:
                    dtor_name = "mrklkit_rt_array_destroy";
                    CALLDTOR;
                    break;
                case LKIT_DICT:
                    dtor_name = "mrklkit_rt_dict_destroy";
                    CALLDTOR;
                    break;
                case LKIT_STRUCT:
                    dtor_name = "mrklkit_rt_struct_destroy";
                    CALLDTOR;
                    break;

                case LKIT_INT:
                case LKIT_BOOL:
                    {
                        LLVMValueRef zero;

                        zero = LLVMConstInt((*fty)->backend, 0, 1);
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                case LKIT_FLOAT:
                    {
                        LLVMValueRef zero;

                        zero = LLVMConstReal((*fty)->backend, 0.0);
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                default:
                    TRRET(LTYPE_COMPILE_METHODS + 4);
                }

            }

            LLVMBuildRetVoid(b1);
            LLVMBuildRetVoid(b2);

            LLVMDisposeBuilder(b1);
            LLVMDisposeBuilder(b2);
        }
        break;

    default:
        break;
    }
    if (buf1 != NULL) {
        free(buf1);
    }
    if (buf2 != NULL) {
        free(buf2);
    }

    return 0;
}

