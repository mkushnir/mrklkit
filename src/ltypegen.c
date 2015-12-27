#include <assert.h>
#include <stdint.h> /* INT64_MAX */
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/hash.h>
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/module.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(ltypegen);
#endif

static LLVMTypeRef
ltype_compile(mrklkit_ctx_t *mctx, lkit_type_t *ty, LLVMModuleRef module)
{
    hash_item_t *dit;
    mrklkit_backend_t *backend;

    if ((dit = hash_get_item(&mctx->backends, ty)) == NULL) {
        LLVMContextRef lctx;
        lkit_type_t **field;
        array_iter_t it;


        if ((backend = malloc(sizeof(mrklkit_backend_t))) == NULL) {
            FAIL("malloc");
        }
        backend->deref = NULL;

        lctx = LLVMGetModuleContext(module);

        switch (ty->tag) {
        case LKIT_VOID:
            backend->ty = LLVMVoidTypeInContext(lctx);
            break;

        case LKIT_NULL:
            /* XXX */
            backend->ty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
            break;

        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            backend->ty = LLVMInt64TypeInContext(lctx);
            break;

        case LKIT_STR:
            {
                /*
                 * bytes_t *
                 */
#ifdef DO_MEMDEBUG
                LLVMTypeRef fields[5];

                assert(sizeof(bytes_t) == 4 * sizeof(uint64_t));
                fields[0] = LLVMInt64TypeInContext(lctx);
                fields[1] = LLVMInt64TypeInContext(lctx);
                fields[2] = LLVMInt64TypeInContext(lctx);
                fields[3] = LLVMInt64TypeInContext(lctx);
                fields[4] = LLVMArrayType(LLVMInt8TypeInContext(lctx), 0);
#else
                LLVMTypeRef fields[4];

                assert(sizeof(bytes_t) == 3 * sizeof(uint64_t));
                fields[0] = LLVMInt64TypeInContext(lctx);
                fields[1] = LLVMInt64TypeInContext(lctx);
                fields[2] = LLVMInt64TypeInContext(lctx);
                fields[3] = LLVMArrayType(LLVMInt8TypeInContext(lctx), 0);
#endif
                backend->deref = LLVMStructCreateNamed(lctx, "bytes_t");
                LLVMStructSetBody(backend->deref,
                                  fields,
                                  countof(fields),
                                  0);
                backend->ty = LLVMPointerType(backend->deref, 0);
            }
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            backend->ty = LLVMDoubleTypeInContext(lctx);
            break;

        case LKIT_BOOL:
            backend->ty = LLVMInt1TypeInContext(lctx);
            break;

        case LKIT_ANY:
            backend->ty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
            break;

        case LKIT_UNDEF:
            backend->ty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
            break;

        case LKIT_ARRAY:
            {
                char buf[1024];

                snprintf(buf,
                         sizeof(buf),
                         "rt_array_t__%s",
                         lkit_array_get_element_type((lkit_array_t *)ty)->name);
                if ((backend->deref = LLVMGetTypeByName(module, buf)) == NULL) {
                    backend->deref = LLVMStructCreateNamed(lctx, buf);
                    LLVMStructSetBody(backend->deref, NULL, 0, 0);
                }
                backend->ty = LLVMPointerType(backend->deref, 0);
            }
            break;

        case LKIT_DICT:
            {
                char buf[1024];

                snprintf(buf,
                         sizeof(buf),
                         "rt_dict_t__%s",
                         lkit_dict_get_element_type((lkit_dict_t *)ty)->name);
                if ((backend->deref = LLVMGetTypeByName(module, buf)) == NULL) {
                    backend->deref = LLVMStructCreateNamed(lctx, buf);
                    LLVMStructSetBody(backend->deref, NULL, 0, 0);
                }
                backend->ty = LLVMPointerType(backend->deref, 0);
            }
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

                    if ((*field)->tag == LKIT_UNDEF ||
                        (*field)->tag == LKIT_VARARG) {

                        free(bfields);
                        goto end_struct;
                    }

                    if ((bfields[it.iter] = ltype_compile(mctx,
                                                          *field,
                                                          module)) == NULL) {
                        free(bfields);
                        TRRETNULL(LTYPE_COMPILE + 1);
                    }
                }

                backend->deref = LLVMStructCreateNamed(lctx,
                                                      NEWVAR("rt_struct_t"));
                LLVMStructSetBody(backend->deref,
                                  bfields,
                                  ts->fields.elnum,
                                  0);
                free(bfields);
                backend->ty = LLVMPointerType(backend->deref, 0);
            }

    end_struct:
            break;

        case LKIT_FUNC:
            {
                LLVMTypeRef retty;
                LLVMTypeRef *bfields;
                lkit_func_t *tf;

                tf = (lkit_func_t *)ty;
                if ((bfields =
                        malloc(sizeof(LLVMTypeRef) *
                               tf->fields.elnum - 1)) == NULL) {
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
                    TRRETNULL(LTYPE_COMPILE + 2);
                }
                if ((retty = ltype_compile(mctx, *field, module)) == NULL) {
                    free(bfields);
                    TRRETNULL(LTYPE_COMPILE + 3);
                }

                for (field = array_next(&tf->fields, &it);
                     field != NULL;
                     field = array_next(&tf->fields, &it)) {

                    if ((*field)->tag == LKIT_VARARG) {
                        break;
                    }

                    if ((bfields[it.iter - 1] = ltype_compile(
                            mctx, *field, module)) == NULL) {
                        free(bfields);
                        TRRETNULL(LTYPE_COMPILE + 4);
                    }
                }

                if ((field = array_last(&tf->fields, &it)) == NULL) {
                    FAIL("array_first");
                }
                if ((*field)->tag == LKIT_VARARG) {
                    backend->ty = LLVMFunctionType(retty,
                                                   bfields,
                                                   tf->fields.elnum - 2,
                                                   1);
                } else {
                    backend->ty = LLVMFunctionType(retty,
                                                   bfields,
                                                   tf->fields.elnum - 1,
                                                   0);
                }
                free(bfields);

            }
            break;

        case LKIT_PARSER:
            backend->ty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
            break;

        default:
            /*
             * tell llvm that all user-defined types are just opaque void *
             */
            backend->ty = LLVMPointerType(LLVMInt8TypeInContext(lctx), 0);
        }
        hash_set_item(&mctx->backends, ty, backend);
    } else {
        backend = dit->value;
    }
    return backend->ty;
}


void
ltype_maybe_compile_type(mrklkit_ctx_t *mctx,
                         lkit_type_t *ty,
                         LLVMModuleRef module)
{
    if (hash_get_item(&mctx->backends, ty) == NULL) {
        (void)ltype_compile(mctx, ty, module);
    }
}


static mrklkit_backend_t *
ltype_get_backend(mrklkit_ctx_t *mctx, lkit_type_t *ty)
{
    hash_item_t *dit;

    if ((dit = hash_get_item(&mctx->backends, ty)) == NULL) {
        FAIL("mrklkit_ctx_get_type_backend");
    }
    return dit->value;
}


static int
ltype_compile_methods(mrklkit_ctx_t *mctx,
                      lkit_type_t *ty,
                      LLVMModuleRef module)
{
    int res;
    char buf1[1024], buf2[1024];
    LLVMContextRef lctx;
    const char *name;
    mrklkit_backend_t *backend;

    res = 0;

    if (!(ty->tag == LKIT_ARRAY ||
          ty->tag == LKIT_DICT ||
          ty->tag == LKIT_STRUCT ||
          ty->tag == LKIT_STR ||
          /* ty->tag == LKIT_PARSER */ 0 )) {
        goto end;
    }

    lctx = LLVMGetModuleContext(module);
    backend = ltype_get_backend(mctx, ty);
    name = LLVMGetStructName(backend->deref);

    snprintf(buf1, sizeof(buf1), "_mrklkit.%s.init", name);
    snprintf(buf2, sizeof(buf2), "_mrklkit.%s.fini", name);

#define BUILDCODE                                      \
     UNUSED LLVMValueRef pnull, dtor, dparam;          \
     /* ctor, just set NULL */                         \
     pnull = LLVMConstPointerNull(                     \
             ltype_compile(mctx, *fty, module));       \
     LLVMBuildStore(b1, pnull, gep1);                  \
     /* dtor, call mrklkit_rt_NNN_decref() */          \
     if ((dtor = LLVMGetNamedFunction(module,          \
             dtor_name)) == NULL) {                    \
         TRACE("no name: %s", dtor_name);              \
         res = LTYPE_COMPILE_METHODS + 3;              \
         goto err;                                     \
     }                                                 \
     dparam = LLVMGetFirstParam(dtor);                 \
     gep2 = LLVMBuildPointerCast(b2,                   \
                                 gep2,                 \
                                 LLVMTypeOf(dparam),   \
                                 NEWVAR("cast"));      \
     LLVMBuildCall(b2, dtor, &gep2, 1, "");            \


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

            ts = (lkit_struct_t *)ty;

            if (LLVMGetNamedFunction(module, buf1) != NULL) {
                //TRACE("non unique name: %s", buf1);
                goto end;
            }
            if (LLVMGetNamedFunction(module, buf2) != NULL) {
                TRACE("non unique name: %s", buf2);
                res = LTYPE_COMPILE_METHODS + 2;
                goto err;
            }

            b1 = LLVMCreateBuilderInContext(lctx);
            b2 = LLVMCreateBuilderInContext(lctx);

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

            bb = LLVMAppendBasicBlockInContext(lctx, fn1, NEWVAR("L"));
            LLVMPositionBuilderAtEnd(b1, bb);
            bb = LLVMAppendBasicBlockInContext(lctx, fn2, NEWVAR("L"));
            LLVMPositionBuilderAtEnd(b2, bb);

            cast1 = LLVMBuildPointerCast(b1,
                                         LLVMGetParam(fn1, 0),
                                         ltype_compile(mctx, ty, module),
                                         NEWVAR("cast"));
            cast2 = LLVMBuildPointerCast(b2,
                                         LLVMGetParam(fn2, 0),
                                         ltype_compile(mctx, ty, module),
                                         NEWVAR("cast"));

            for (fty = array_first(&ts->fields, &it);
                 fty != NULL;
                 fty = array_next(&ts->fields, &it)) {

                LLVMValueRef gep1, gep2;
                const char *dtor_name = NULL;

                gep1 = LLVMBuildStructGEP(b1, cast1, it.iter, NEWVAR("gep"));
                gep2 = LLVMBuildStructGEP(b2, cast2, it.iter, NEWVAR("gep"));

                switch ((*fty)->tag) {
                case LKIT_STR:
                    {
                        dtor_name = "mrklkit_rt_bytes_decref";
                        BUILDCODE;
                    }
                    break;

                case LKIT_ARRAY:
                    {
                        dtor_name = "mrklkit_rt_array_decref";
                        BUILDCODE;
                    }
                    break;

                case LKIT_DICT:
                    {
                        dtor_name = "mrklkit_rt_dict_decref";
                        BUILDCODE;
                    }
                    break;

                case LKIT_STRUCT:
                    {
                        if (ltype_compile_methods(mctx,
                                                  *fty,
                                                  module) != 0) {
                            res = LTYPE_COMPILE_METHODS + 4;
                            goto err;
                        }
                        dtor_name = "mrklkit_rt_struct_decref";
                        BUILDCODE;
                    }

                    break;

                case LKIT_INT:
                case LKIT_INT_MIN:
                case LKIT_INT_MAX:
                case LKIT_BOOL:
                    {
                        LLVMValueRef zero;

                        if ((*fty)->tag == LKIT_INT_MAX) {
                            zero = LLVMConstInt(ltype_compile(mctx,
                                                              *fty,
                                                              module),
                                                INT64_MAX,
                                                1);
                        } else if ((*fty)->tag == LKIT_INT_MIN) {
                            zero = LLVMConstInt(ltype_compile(mctx,
                                                              *fty,
                                                              module),
                                                INT64_MIN,
                                                1);
                        } else {
                            zero = LLVMConstInt(ltype_compile(mctx,
                                                              *fty,
                                                              module),
                                                0,
                                                1);
                        }
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                case LKIT_VOID:
                    {
                    }
                    break;

                case LKIT_FLOAT:
                case LKIT_FLOAT_MIN:
                case LKIT_FLOAT_MAX:
                    {
                        LLVMValueRef zero;

                        if ((*fty)->tag == LKIT_FLOAT_MAX) {
                            zero = LLVMConstReal(ltype_compile(mctx,
                                                               *fty,
                                                               module),
                                                 INFINITY);
                        } else if ((*fty)->tag == LKIT_FLOAT_MIN) {
                            zero = LLVMConstReal(ltype_compile(mctx,
                                                               *fty,
                                                               module),
                                                 -INFINITY);
                        } else {
                            zero = LLVMConstReal(ltype_compile(mctx,
                                                               *fty,
                                                               module),
                                                 0.0);
                        }
                        LLVMBuildStore(b1, zero, gep1);
                        LLVMBuildStore(b2, zero, gep2);
                    }
                    break;

                default:
                    {
                        char dtor_name[1024];

                        snprintf(dtor_name,
                                 sizeof(dtor_name),
                                 "%s_decref", (*fty)->name);
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
        {
            mrklkit_module_t **mod;
            array_iter_t it;

            for (mod = array_first(&mctx->modules, &it);
                 mod != NULL;
                 mod = array_next(&mctx->modules, &it)) {

                if ((*mod)->compile_type_method != NULL) {
                    if ((*mod)->compile_type_method(mctx, ty, module) == 0) {
                        break;
                    }
                }
            }
        }
        break;
    }

end:
    return res;

err:
    lkit_type_dump(ty);
    LLVMDumpModule(module);
    goto end;
}


static int
rt_array_bytes_fini(bytes_t **value, UNUSED void *udata)
{
    BYTES_DECREF(value);
    return 0;
}


static int
rt_array_array_fini(rt_array_t **value, UNUSED void *udata)
{
    ARRAY_DECREF(value);
    return 0;
}


static int
rt_array_hash_fini(rt_dict_t **value, UNUSED void *udata)
{
    DICT_DECREF(value);
    return 0;
}


static int
rt_array_struct_fini(rt_struct_t **value, UNUSED void *udata)
{
    STRUCT_DECREF(value);
    return 0;
}


static int
rt_dict_fini_keyonly(bytes_t *key, UNUSED void *val)
{
    BYTES_DECREF(&key);
    return 0;
}

static int
rt_dict_fini_keyval_str(bytes_t *key, bytes_t *val)
{
    BYTES_DECREF(&key);
    BYTES_DECREF(&val);
    return 0;
}


static int
rt_dict_fini_keyval_array(bytes_t *key, rt_array_t *val)
{
    BYTES_DECREF(&key);
    ARRAY_DECREF(&val);
    return 0;
}


static int
rt_dict_fini_keyval_dict(bytes_t *key, rt_dict_t *val)
{
    BYTES_DECREF(&key);
    DICT_DECREF(&val);
    return 0;
}


static int
rt_dict_fini_keyval_struct(bytes_t *key, rt_struct_t *val)
{
    BYTES_DECREF(&key);
    STRUCT_DECREF(&val);
    return 0;
}


static int
ltype_link_methods(mrklkit_ctx_t *mctx,
                   lkit_type_t *ty,
                   LLVMExecutionEngineRef ee,
                   LLVMModuleRef module)
{

    char buf[1024];
    const char *name;
    mrklkit_backend_t *backend;

    if (!(ty->tag == LKIT_ARRAY ||
          ty->tag == LKIT_DICT ||
          ty->tag == LKIT_STRUCT ||
          /* ty->tag == LKIT_PARSER */ 0)) {
        return 0;
    }

    backend = ltype_get_backend(mctx, ty);
    name = LLVMGetStructName(backend->deref);

    switch (ty->tag) {
    case LKIT_STRUCT:
        {
            lkit_type_t **fty;
            array_iter_t it;
            lkit_struct_t *ts;
            void *p = NULL;
            LLVMValueRef g = NULL;

            ts = (lkit_struct_t *)ty;

            snprintf(buf, sizeof(buf), "_mrklkit.%s.init", name);
            if (LLVMFindFunction(ee, buf, &g) != 0) {
                TRRET(LTYPE_LINK_METHODS + 1);
            }
            if ((p = LLVMGetPointerToGlobal(ee, g)) == NULL) {
                TRRET(LTYPE_LINK_METHODS + 2);
            }
            //TRACE("%s:%p", buf, p);
            ts->init = p;

            snprintf(buf, sizeof(buf), "_mrklkit.%s.fini", name);
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

                if (ltype_link_methods(mctx, *fty, ee, module) != 0) {
                    TRRET(LTYPE_LINK_METHODS + 5);
                }
            }
        }
        break;

    case LKIT_ARRAY:
        {
            lkit_array_t *ta;
            lkit_type_t *elty;

            ta = (lkit_array_t *)ty;
            if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                FAIL("lkit_array_get_element_type");
            }

            switch (elty->tag) {
            case LKIT_INT:
            case LKIT_INT_MAX:
            case LKIT_INT_MIN:
            case LKIT_FLOAT:
            case LKIT_FLOAT_MAX:
            case LKIT_FLOAT_MIN:
            case LKIT_BOOL:
                break;

            case LKIT_STR:
                ta->fini = (array_finalizer_t)rt_array_bytes_fini;
                break;

            case LKIT_ARRAY:
                ta->fini = (array_finalizer_t)rt_array_array_fini;
                break;

            case LKIT_DICT:
                ta->fini = (array_finalizer_t)rt_array_hash_fini;
                break;

            case LKIT_STRUCT:
                ta->fini = (array_finalizer_t)rt_array_struct_fini;
                break;

            default:
                break;
            }

        }
        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            lkit_type_t *elty;

            td = (lkit_dict_t *)ty;
            if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                FAIL("lkit_dict_get_element_type");
            }

            switch (elty->tag) {
            case LKIT_INT:
            case LKIT_INT_MAX:
            case LKIT_INT_MIN:
            case LKIT_FLOAT:
            case LKIT_FLOAT_MAX:
            case LKIT_FLOAT_MIN:
            case LKIT_BOOL:
                td->fini = (hash_item_finalizer_t)rt_dict_fini_keyonly;
                break;

            case LKIT_STR:
                td->fini = (hash_item_finalizer_t)rt_dict_fini_keyval_str;
                break;

            case LKIT_ARRAY:
                td->fini = (hash_item_finalizer_t)rt_dict_fini_keyval_array;
                break;

            case LKIT_DICT:
                td->fini = (hash_item_finalizer_t)rt_dict_fini_keyval_dict;
                break;

            case LKIT_STRUCT:
                td->fini = (hash_item_finalizer_t)rt_dict_fini_keyval_struct;
                break;

            default:
                break;
            }

        }
        break;

    default:
        {
            mrklkit_module_t **mod;
            array_iter_t it;

            for (mod = array_first(&mctx->modules, &it);
                 mod != NULL;
                 mod = array_next(&mctx->modules, &it)) {

                if ((*mod)->method_link != NULL) {
                    if ((*mod)->method_link(mctx, ty, ee, module) == 0) {
                        break;
                    }
                }
            }
        }
        break;
    }

    return 0;
}


static void
ltype_unlink_methods(mrklkit_ctx_t *mctx, lkit_type_t *ty)
{
    if (ty == NULL) {
        return;
    }

    switch (ty->tag) {
    case LKIT_STRUCT:
        {
            lkit_type_t **fty;
            array_iter_t it;
            lkit_struct_t *ts;

            ts = (lkit_struct_t *)ty;
            ts->init = NULL;
            ts->fini = NULL;
            for (fty = array_first(&ts->fields, &it);
                 fty != NULL;
                 fty = array_next(&ts->fields, &it)) {
                ltype_unlink_methods(mctx, *fty);
            }
        }
        break;

    case LKIT_ARRAY:
        {
            lkit_array_t *ta;
            ta = (lkit_array_t *)ty;
            ta->fini = NULL;
        }

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            td = (lkit_dict_t *)ty;
            td->fini = NULL;
        }

    default:
        {
            mrklkit_module_t **mod;
            array_iter_t it;

            for (mod = array_first(&mctx->modules, &it);
                 mod != NULL;
                 mod = array_next(&mctx->modules, &it)) {

                if ((*mod)->method_unlink != NULL) {
                    if ((*mod)->method_unlink(mctx, ty) == 0) {
                        break;
                    }
                }
            }
        }
        break;
    }
}


static int
_cb0(lkit_type_t *key, UNUSED lkit_type_t *value, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMModuleRef module;
    } *params = udata;

    (void)ltype_compile(params->mctx, key, params->module);
    return 0;
}


int
lkit_compile_types(mrklkit_ctx_t *mctx, LLVMModuleRef module)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMModuleRef module;
    } params = { mctx, module };
    return lkit_traverse_types((hash_traverser_t)_cb0, &params);
}


static int
_cb1(lkit_type_t *key, UNUSED lkit_type_t *value, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMModuleRef module;
    } *params = udata;

    (void)ltype_compile_methods(params->mctx, key, params->module);
    return 0;
}


int
lkit_compile_type_methods(mrklkit_ctx_t *mctx, LLVMModuleRef module)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMModuleRef module;
    } params = { mctx, module };
    return lkit_traverse_types((hash_traverser_t)_cb1, &params);
}


static int
_cb2(lkit_type_t *key, UNUSED lkit_type_t *value, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMExecutionEngineRef ee;
        LLVMModuleRef module;
    } *params = udata;

    (void)ltype_link_methods(params->mctx, key, params->ee, params->module);
    return 0;
}


int
lkit_link_types(mrklkit_ctx_t *mctx,
                LLVMExecutionEngineRef ee,
                LLVMModuleRef module)
{
    struct {
        mrklkit_ctx_t *mctx;
        LLVMExecutionEngineRef ee;
        LLVMModuleRef module;
    } params = { mctx, ee, module };

    return lkit_traverse_types((hash_traverser_t)_cb2, &params);
}


static int
_cb3(lkit_type_t *key, UNUSED lkit_type_t *value, void *udata)
{
    struct {
        mrklkit_ctx_t *mctx;
    } *params = udata;

    ltype_unlink_methods(params->mctx, key);
    return 0;
}


int
lkit_unlink_types(mrklkit_ctx_t *mctx)
{
    struct {
        mrklkit_ctx_t *mctx;
    } params = { mctx };

    return lkit_traverse_types((hash_traverser_t)_cb3, &params);
}

