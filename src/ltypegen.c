#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
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
        }

        ty->backend = LLVMPointerType(LLVMStructType(bfields, ts->fields.elnum, 0), 0);

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

