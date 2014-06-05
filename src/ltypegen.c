#include <assert.h>
#include <stdint.h> /* INT64_MAX */
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>

#include "diag.h"


/**
 * Generators
 *
 */

int
ltype_compile(lkit_type_t *ty, LLVMContextRef lctx)
{
    LLVMTypeRef retty;
    lkit_type_t **field;
    array_iter_t it;

    if (ty->backend != NULL) {
        return 0;
    }

    switch (ty->tag) {
    case LKIT_VOID:
        ty->backend = LLVMVoidTypeInContext(lctx);
        break;

    case LKIT_INT:
    case LKIT_INT_MIN:
    case LKIT_INT_MAX:
        ty->backend = LLVMInt64TypeInContext(lctx);
        break;

    case LKIT_STR:
        {
            lkit_str_t *tc;
            LLVMTypeRef fields[4];

            /*
             * bytes_t *
             */
            fields[0] = LLVMInt64TypeInContext(lctx);
            fields[1] = LLVMInt64TypeInContext(lctx);
            fields[2] = LLVMInt64TypeInContext(lctx);
            fields[3] = LLVMArrayType(LLVMInt8TypeInContext(lctx), 0);
            tc = (lkit_str_t *)ty;
            //tc->deref_backend = LLVMStructTypeInContext(lctx,
            //                                            fields,
            //                                            countof(fields), 0);
            tc->deref_backend = LLVMStructCreateNamed(lctx, "bytes_t");
            LLVMStructSetBody(tc->deref_backend,
                              fields,
                              countof(fields),
                              0);
            ty->backend = LLVMPointerType(tc->deref_backend, 0);
        }
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        ty->backend = LLVMDoubleTypeInContext(lctx);
        break;

    case LKIT_BOOL:
        ty->backend = LLVMInt1TypeInContext(lctx);
        break;

    case LKIT_ANY:
        ty->backend = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
        break;

    case LKIT_UNDEF:
        ty->backend = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
        break;

    case LKIT_ARRAY:
        //ty->backend = LLVMPointerType(LLVMStructTypeInContext(lctx,
        //                                                      NULL,
        //                                                      0,
        //                                                      0), 0);
        ty->backend = LLVMStructCreateNamed(lctx, "rt_array_t");
        LLVMStructSetBody(ty->backend, NULL, 0, 0);
        ty->backend = LLVMPointerType(ty->backend, 0);
        break;

    case LKIT_DICT:
        //ty->backend = LLVMPointerType(LLVMStructTypeInContext(lctx,
        //                                                      NULL,
        //                                                      0,
        //                                                      0), 0);
        ty->backend = LLVMStructCreateNamed(lctx, "rt_dict_t");
        LLVMStructSetBody(ty->backend, NULL, 0, 0);
        ty->backend = LLVMPointerType(ty->backend, 0);
        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            ts = (lkit_struct_t *)ty;
            LLVMTypeRef *bfields;

            if ((bfields =
                    malloc(sizeof(LLVMTypeRef) * ts->fields.elnum)) == NULL) {
                FAIL("malloc");
            }

            for (field = array_first(&ts->fields, &it);
                 field != NULL;
                 field = array_next(&ts->fields, &it)) {

                if ((*field)->tag == LKIT_UNDEF || (*field)->tag == LKIT_VARARG) {
                    free(bfields);
                    goto end_struct;
                }

                if (ltype_compile(*field, lctx) != 0) {
                    free(bfields);
                    TRRET(LTYPE_COMPILE + 1);
                }

                bfields[it.iter] = (*field)->backend;
                ts->base.rtsz += (*field)->rtsz;
            }

            //ts->deref_backend = LLVMStructTypeInContext(lctx,
            //                                            bfields,
            //                                            ts->fields.elnum,
            //                                            0);
            ts->deref_backend = LLVMStructCreateNamed(lctx, NEWVAR("rt_struct_t"));
            LLVMStructSetBody(ts->deref_backend,
                              bfields,
                              ts->fields.elnum,
                              0);
            free(bfields);
            ty->backend = LLVMPointerType(ts->deref_backend, 0);
        }

end_struct:
        break;

    case LKIT_FUNC:
        {
            LLVMTypeRef *bfields;
            lkit_func_t *tf;

            tf = (lkit_func_t *)ty;
            if ((bfields =
                    malloc(sizeof(LLVMTypeRef) * tf->fields.elnum - 1)) == NULL) {
                FAIL("malloc");
            }

            if ((field = array_first(&tf->fields, &it)) == NULL) {
                FAIL("array_first");
            }
            //if ((*field)->tag == LKIT_UNDEF) {
            //    goto end_func;
            //}
            if ((*field)->tag == LKIT_VARARG) {
                free(bfields);
                TRRET(LTYPE_COMPILE + 2);
            }
            if (ltype_compile(*field, lctx) != 0) {
                free(bfields);
                TRRET(LTYPE_COMPILE + 3);
            }

            retty = (*field)->backend;

            for (field = array_next(&tf->fields, &it);
                 field != NULL;
                 field = array_next(&tf->fields, &it)) {

                if ((*field)->tag == LKIT_VARARG) {
                    break;
                }

                if (ltype_compile(*field, lctx) != 0) {
                    free(bfields);
                    TRRET(LTYPE_COMPILE + 4);
                }

                bfields[it.iter - 1] = (*field)->backend;
            }

            if ((field = array_last(&tf->fields, &it)) == NULL) {
                FAIL("array_first");
            }
            if ((*field)->tag == LKIT_VARARG) {
                ty->backend = LLVMFunctionType(retty,
                                               bfields,
                                               tf->fields.elnum - 2,
                                               1);
            } else {
                ty->backend = LLVMFunctionType(retty,
                                               bfields,
                                               tf->fields.elnum - 1,
                                               0);
            }
            free(bfields);

        }
        break;

    default:
        /*
         * tell llvm that all user-defined types are just opaque void *
         */
        ty->backend = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
    }

    return 0;
}


int
ltype_compile_methods(lkit_type_t *ty,
                      LLVMModuleRef module,
                      bytes_t *name)
{
    char *buf1 = NULL, *buf2 = NULL;
    LLVMContextRef lctx;

    lctx = LLVMGetModuleContext(module);

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

            b1 = LLVMCreateBuilderInContext(lctx);
            b2 = LLVMCreateBuilderInContext(lctx);

            snprintf(buf1, name->sz + 64, ".mrklkit.init.%s", name->data);
            snprintf(buf2, name->sz + 64, ".mrklkit.fini.%s", name->data);

            if (LLVMGetNamedFunction(module, buf1) != NULL) {
                TRRET(LTYPE_COMPILE_METHODS + 1);
            }
            if (LLVMGetNamedFunction(module, buf1) != NULL) {
                TRRET(LTYPE_COMPILE_METHODS + 2);
            }

            argty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);

            fn1 = LLVMAddFunction(module,
                                  buf1,
                                  LLVMFunctionType(LLVMInt64TypeInContext(lctx),
                                                   &argty,
                                                   1,
                                                   0));
            fn2 = LLVMAddFunction(module,
                                  buf2,
                                  LLVMFunctionType(LLVMInt64TypeInContext(lctx),
                                                   &argty,
                                                   1,
                                                   0));

            //lkit_type_dump(ty);
            //TRACE("ts->base.backend=%p", ts->base.backend);
            //LLVMDumpType(ts->base.backend);
            //TRACE();
            //LLVMDumpValue(fn1);

            bb = LLVMAppendBasicBlockInContext(lctx, fn1, NEWVAR("L"));
            LLVMPositionBuilderAtEnd(b1, bb);
            bb = LLVMAppendBasicBlockInContext(lctx, fn2, NEWVAR("L"));
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

#               define BUILDCODE \
                    { \
                        LLVMValueRef pnull, dtor, dparam; \
                        /* ctor, just set NULL */ \
                        pnull = LLVMConstPointerNull((*fty)->backend); \
                        LLVMBuildStore(b1, pnull, gep1); \
                        /* dtor, call mrklkit_rt_NNN_destroy() */ \
                        if ((dtor = LLVMGetNamedFunction(module, \
                                dtor_name)) == NULL) { \
                            TRACE("no name: %s", dtor_name); \
                            TRRET(LTYPE_COMPILE_METHODS + 3); \
                        } \
                        dparam = LLVMGetFirstParam(dtor); \
                        gep2 = LLVMBuildPointerCast(b2, \
                                                    gep2, \
                                                    LLVMTypeOf(dparam), \
                                                    NEWVAR("cast")); \
                        LLVMBuildCall(b2, dtor, &gep2, 1, NEWVAR("call")); \
                    }

                switch ((*fty)->tag) {
                case LKIT_STR:
                    dtor_name = "mrklkit_bytes_decref";
                    BUILDCODE;
                    break;

                case LKIT_ARRAY:
                    dtor_name = "mrklkit_rt_array_destroy";
                    BUILDCODE;
                    break;

                case LKIT_DICT:
                    dtor_name = "mrklkit_rt_dict_destroy";
                    BUILDCODE;
                    break;

                case LKIT_STRUCT:
                    {
                        char buf[1024];
                        bytes_t **nm, *nnm;

                        if ((nm = array_get(&ts->names, it.iter)) == NULL) {
                            FAIL("array_get");
                        }
                        snprintf(buf,
                                 sizeof(buf),
                                 "%s.%s",
                                 name->data,
                                 (*nm)->data);
                        //TRACE("subfield %s", buf);
                        nnm = bytes_new_from_str(buf);
                        if (ltype_compile_methods(*fty, module, nnm) != 0) {
                            TRRET(LTYPE_COMPILE_METHODS + 4);
                        }
                        bytes_decref(&nnm);
                    }

                    dtor_name = "mrklkit_rt_struct_destroy";
                    BUILDCODE;
                    break;

                case LKIT_INT:
                case LKIT_INT_MIN:
                case LKIT_INT_MAX:
                case LKIT_BOOL:
                    {
                        LLVMValueRef zero;

                        if ((*fty)->tag == LKIT_INT_MAX) {
                            zero = LLVMConstInt((*fty)->backend, INT64_MAX, 1);
                        } else if ((*fty)->tag == LKIT_INT_MIN) {
                            zero = LLVMConstInt((*fty)->backend, INT64_MIN, 1);
                        } else {
                            zero = LLVMConstInt((*fty)->backend, 0, 1);
                        }
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                case LKIT_FLOAT:
                case LKIT_FLOAT_MIN:
                case LKIT_FLOAT_MAX:
                    {
                        LLVMValueRef zero;

                        if ((*fty)->tag == LKIT_FLOAT_MAX) {
                            zero = LLVMConstReal((*fty)->backend, INFINITY);
                        } else if ((*fty)->tag == LKIT_FLOAT_MIN) {
                            zero = LLVMConstReal((*fty)->backend, -INFINITY);
                        } else {
                            zero = LLVMConstReal((*fty)->backend, 0.0);
                        }
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                default:
                    {
                        char dtor_name[1024];

                        assert((*fty)->backend != NULL);
                        snprintf(dtor_name,
                                 sizeof(dtor_name),
                                 "%s_destroy", (*fty)->name);
                        BUILDCODE;
                    }
                    break;
                }

            }

            LLVMBuildRet(b1, LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0));
            LLVMBuildRet(b2, LLVMConstInt(LLVMInt64TypeInContext(lctx), 0, 0));

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


int
ltype_link_methods(lkit_type_t *ty,
                   LLVMExecutionEngineRef ee,
                   LLVMModuleRef module,
                   bytes_t *name)
{

    char *buf = NULL;

    switch (ty->tag) {
    case LKIT_STRUCT:
        {
            lkit_type_t **fty;
            array_iter_t it;
            lkit_struct_t *ts;
            void *p;
            LLVMValueRef g = NULL;

            //TRACE("linking: %s", name->data);

            if ((buf = malloc(name->sz + 64)) == NULL) {
                FAIL("malloc");
            }

            ts = (lkit_struct_t *)ty;

            snprintf(buf, name->sz + 64, ".mrklkit.init.%s", name->data);
            if (LLVMFindFunction(ee, buf, &g) != 0) {
                TRRET(LTYPE_LINK_METHODS + 1);
            }
            if ((p = LLVMGetPointerToGlobal(ee, g)) == NULL) {
                TRRET(LTYPE_LINK_METHODS + 2);
            }
            //TRACE("%s:%p", buf, p);
            ts->init = p;

            snprintf(buf, name->sz + 64, ".mrklkit.fini.%s", name->data);
            if (LLVMFindFunction(ee, buf, &g) != 0) {
                TRRET(LTYPE_LINK_METHODS + 3);
            }
            if ((p = LLVMGetPointerToGlobal(ee, g)) == NULL) {
                TRRET(LTYPE_LINK_METHODS + 4);
            }
            //TRACE("%s:%p", buf, p);
            ts->fini = p;

            for (fty = array_first(&ts->fields, &it);
                 fty != NULL;
                 fty = array_next(&ts->fields, &it)) {

                char fbuf[1024];
                bytes_t **nm, *nnm;

                //lkit_type_dump(*fty);

                if ((nm = array_get(&ts->names, it.iter)) == NULL) {
                    /* hack ... */
                    if ((*fty)->tag == LKIT_STRUCT) {
                        TRACE("!name=%s", name->data);
                        lkit_type_dump(ty);
                        FAIL("array_get");
                    } else {
                        /* unnamed simple simple structs ... */
                        continue;
                    }
                }

                snprintf(fbuf, sizeof(fbuf), "%s.%s", name->data, (*nm)->data);
                //TRACE("need link %s", fbuf);
                nnm = bytes_new_from_str(fbuf);
                if (ltype_link_methods(*fty, ee, module, nnm) != 0) {
                    TRRET(LTYPE_LINK_METHODS + 5);
                }
                bytes_decref(&nnm);
            }

        }
        break;

    default:
        break;
    }

    if (buf != NULL) {
        free(buf);
    }

    return 0;
}

