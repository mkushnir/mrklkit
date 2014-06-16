#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/module.h>
#include <mrklkit/mrklkit.h>

#include <mrklkit/lruntime.h>

#include "diag.h"

const char *mrklkit_meta = "; libc\n"
"(sym malloc (func any int))\n"
"(sym free (func int any))\n"
"(sym printf (func int any ...))\n"
"(sym strcmp (func int any any))\n"
//"(sym mrklkit_rt_strcmp (func int any any))\n"
"(sym strstr (func any any any))\n"
"\n"
"; mrklkit backend\n"
"(sym dparse_struct_item_ra_int         (func int undef int))\n"
"(sym dparse_struct_item_ra_float       (func float undef int))\n"
"(sym dparse_struct_item_ra_bool        (func bool undef int))\n"
"(sym dparse_struct_item_ra_str         (func str undef int))\n"
"(sym dparse_struct_item_ra_array       (func any undef int))\n"
"(sym dparse_struct_item_ra_struct      (func any undef int))\n"
"(sym dparse_struct_item_ra_dict        (func any undef int))\n"
"(sym dparse_struct_item_seq_int        (func int undef int))\n"
"(sym dparse_struct_item_seq_float      (func float undef int))\n"
"(sym dparse_struct_item_seq_bool       (func bool undef int))\n"
"(sym dparse_struct_item_seq_str        (func str undef int))\n"
"(sym dparse_struct_item_seq_array      (func any undef int))\n"
"(sym dparse_struct_item_seq_struct     (func any undef int))\n"
"(sym dparse_struct_item_seq_dict       (func any undef int))\n"

"(sym dparse_struct_pi_pos              (func int any))\n"

"(sym mrklkit_rt_get_struct_item_int    (func int undef int))\n"
"(sym mrklkit_rt_get_struct_item_float  (func float undef int))\n"
"(sym mrklkit_rt_get_struct_item_bool   (func bool undef int))\n"
"(sym mrklkit_rt_get_struct_item_str    (func str undef int))\n"
"(sym mrklkit_rt_get_struct_item_array  (func any undef int))\n"
"(sym mrklkit_rt_get_struct_item_struct (func any undef int))\n"
"(sym mrklkit_rt_get_struct_item_dict   (func any undef int))\n"
"(sym mrklkit_rt_get_array_item_int     (func int (array int) int int))\n"
"(sym mrklkit_rt_get_array_item_float   (func float (array float) int float))\n"
"(sym mrklkit_rt_get_array_item_str     (func str (array str) int str))\n"
"(sym mrklkit_rt_get_dict_item_int      (func int (dict int) str int))\n"
"(sym mrklkit_rt_get_dict_item_float    (func float (dict float) str float))\n"
"(sym mrklkit_rt_get_dict_item_str      (func str (dict str) str str))\n"

"(sym mrklkit_bytes_new (func str int))\n"
"(sym mrklkit_bytes_copy (func int str str int))\n"
";(sym mrklkit_bytes_decref (func int any))\n"
";(sym mrklkit_bytes_decref_fast (func int str))\n"
";(sym mrklkit_bytes_incref (func int str))\n"
"(sym mrklkit_rt_bytes_slice_gc (func str str int int))\n"
"(sym mrklkit_rt_array_split_gc (func (array str) any str str))\n"
"(sym mrklkit_rt_array_destroy (func int any))\n"
"(sym mrklkit_rt_dict_destroy (func int any))\n"
"(sym mrklkit_rt_struct_destroy (func int any))\n"
"(sym mrklkit_strtoi64 (func int str int))\n"
"(sym mrklkit_strtod (func float str float))\n"
"\n"
"; builtin\n"
"(builtin , (func undef ...))\n"
"(builtin if (func undef bool undef undef))\n"
"(builtin print (func undef undef ...))\n"
"\n"
"(builtin + (func undef undef ...))\n"
"(builtin - (func undef undef ...))\n"
"(builtin * (func undef undef ...))\n"
"(builtin / (func undef undef ...))\n"
"(builtin % (func undef undef ...))\n"
"(builtin min (func undef undef ...))\n"
"(builtin max (func undef undef ...))\n"
"\n"
"(builtin and (func bool bool ...))\n"
"(builtin or (func bool bool ...))\n"
"(builtin not (func bool bool))\n"
"\n"
"(builtin == (func bool undef undef))\n"
"(builtin = (func bool undef undef))\n" /* compat */
"(builtin != (func bool undef undef))\n"
"(builtin < (func bool undef undef))\n"
"(builtin <= (func bool undef undef))\n"
"(builtin > (func bool undef undef))\n"
"(builtin >= (func bool undef undef))\n"
"\n"
"(builtin parse (func undef undef undef ...))\n"
"(builtin get (func undef undef undef ...))\n"
"(builtin get-index (func undef undef undef ...))\n" /* compat */
"(builtin get-key (func undef undef undef ...))\n" /* compat */
"(builtin set (func undef undef undef undef))\n"
"(builtin del (func undef undef undef))\n"
"(builtin has (func bool undef undef))\n"
"\n"
"(builtin itof (func float int))\n"
"(builtin float (func float int))\n" /* compat */
"(builtin ftoi (func int float))\n"
"(builtin int (func int float))\n" /* compat */
"(builtin atoi (func int str int))\n"
"(builtin atof (func float str float))\n"
"(builtin startswith (func bool str str))\n"
"(builtin endswith (func bool str str))\n"
"(builtin tostr (func str undef))\n"
"(builtin in (func bool undef ...))\n"
"(builtin substr (func str str int int))\n"
"(builtin strfind (func int str str))\n"
"(builtin split (func (array str) str str))\n"
"(builtin dp-info (func undef undef str))\n"
"\n"
"(pragma user)\n"
"\n"
;

/**
 * Generic form parser
 *
 * types? vars?
 *
 */

static int
mrklkit_parse(mrklkit_ctx_t *ctx, int fd, void *udata)
{
    int res = 0;
    array_t *form;
    array_iter_t it;
    fparser_datum_t **fnode;

    if ((ctx->datum_root = fparser_parse(fd, NULL, NULL)) == NULL) {
        FAIL("fparser_parse");
    }


    if (FPARSER_DATUM_TAG(ctx->datum_root) != FPARSER_SEQ) {
        ctx->datum_root->error = 1;
        res = MRKLKIT_PARSE + 1;
        goto err;
    }

    form = (array_t *)ctx->datum_root->body;
    for (fnode = array_first(form, &it);
         fnode != NULL;
         fnode = array_next(form, &it)) {


        /* here tag can be either SEQ, or one of INT, WORD, FLOAT */
        switch (FPARSER_DATUM_TAG(*fnode)) {
            array_t *nform;
            array_iter_t nit;
            char *first = NULL;

        case FPARSER_SEQ:
            nform = (array_t *)((*fnode)->body);

            if (lparse_first_word(nform, &nit, &first, 1) == 0) {
                /*
                 * control statements:
                 *  - mode
                 *  - pragma
                 */
                if (strcmp((char *)first, "mode") == 0) {
                    TRACE("mode ...");
                } else if (strcmp((char *)first, "pragma") == 0) {
                    char *arg = NULL;

                    if (lparse_next_word(nform, &nit, &arg, 1) == 0) {
                        //TRACE("pragma arg: %s", arg);
                    } else {
                        TRACE("ignoring empty pragma");
                    }

                } else {
                    mrklkit_module_t **mod;
                    array_iter_t it;

                    /*
                     * required:
                     *  - type
                     *  - builtin
                     *  - sym
                     */
                    for (mod = array_first(ctx->modules, &it);
                         mod != NULL;
                         mod = array_next(ctx->modules, &it)) {

                        if ((*mod)->parse_expr != NULL) {
                            if ((*mod)->parse_expr(udata,
                                                   (const char *)first,
                                                   nform,
                                                   &nit) != 0) {
                                (*fnode)->error = 1;
                                res = MRKLKIT_PARSE + 2;
                                goto err;
                            }
                        }
                    }
                }
            } else {
                /* ignore ? */
                res = MRKLKIT_PARSE + 3;
                goto err;
            }

            break;

        default:
            /* ignore ? */
            res = MRKLKIT_PARSE + 4;
            goto err;
        }
    }

end:
    TRRET(res);

err:
    fparser_datum_dump_formatted(ctx->datum_root);
    goto end;
}


static void
do_analysis(mrklkit_ctx_t *ctx)
{
    LLVMPassManagerBuilderRef pmb;
    LLVMPassManagerRef fpm;
    LLVMPassManagerRef pm;
    UNUSED int res = 0;
    LLVMValueRef fn = NULL;

    if ((fpm = LLVMCreateFunctionPassManagerForModule(ctx->module)) == NULL) {
        FAIL("LLVMCreateFunctionPassManagerForModule");
    }

    if ((pm = LLVMCreatePassManager()) == NULL) {
        FAIL("LLVMCreatePassManager");
    }

    pmb = LLVMPassManagerBuilderCreate();
    LLVMPassManagerBuilderSetOptLevel(pmb, 3);

    LLVMPassManagerBuilderPopulateFunctionPassManager(pmb, fpm);
    LLVMPassManagerBuilderPopulateModulePassManager(pmb, pm);

    if (LLVMInitializeFunctionPassManager(fpm)) {
        TRACE("LLVMInitializeFunctionPassManager ...");
    }

    for (fn = LLVMGetFirstFunction(ctx->module);
         fn != NULL;
         fn = LLVMGetNextFunction(fn)) {

        //TRACE("Passing %s", LLVMGetValueName(fn));
        //LLVMDumpValue(fn);
        if (LLVMRunFunctionPassManager(fpm, fn)) {
            //TRACE("%s was modified:", LLVMGetValueName(fn));
            //LLVMDumpValue(fn);
        }
    }

    res = LLVMRunPassManager(pm, ctx->module);
    //TRACE("res=%d", res);
    //if (res != 0) {
    //    TRACE("module was modified");
    //}

    LLVMPassManagerBuilderDispose(pmb);
    LLVMDisposePassManager(pm);
    LLVMDisposePassManager(fpm);

}


int
mrklkit_compile(mrklkit_ctx_t *ctx, int fd, uint64_t flags, void *udata)
{
    char *error_msg = NULL;
    int res = 0;
    LLVMTargetRef tr;
    LLVMTargetMachineRef tmr;
    LLVMMemoryBufferRef mb = NULL;
    mrklkit_module_t **mod;
    array_iter_t it;

    /* parse */
    if (mrklkit_parse(ctx, fd, udata) != 0) {
        TRRET(MRKLKIT_COMPILE + 1);
    }

    /* post parse */
    for (mod = array_first(ctx->modules, &it);
         mod != NULL;
         mod = array_next(ctx->modules, &it)) {

        if ((*mod)->post_parse != NULL) {
            if ((*mod)->post_parse(udata)) {
                TRRET(MRKLKIT_COMPILE + 5);
            }
        }
    }

    /* compile */

    reset_newvar_counter();

    for (mod = array_first(ctx->modules, &it);
         mod != NULL;
         mod = array_next(ctx->modules, &it)) {

        if ((*mod)->compile_type != NULL) {
            if ((*mod)->compile_type(udata, ctx->module)) {
                TRRET(MRKLKIT_COMPILE + 5);
            }
        }
    }

    for (mod = array_first(ctx->modules, &it);
         mod != NULL;
         mod = array_next(ctx->modules, &it)) {

        if ((*mod)->compile != NULL) {
            if ((*mod)->compile(udata, ctx->module)) {
                TRRET(MRKLKIT_COMPILE + 5);
            }
        }
    }

    if (flags & MRKLKIT_COMPILE_DUMP0) {
        LLVMDumpModule(ctx->module);
    }

    /* LLVM analysis */
    do_analysis(ctx);

    if (flags & MRKLKIT_COMPILE_DUMP1) {
        TRACEC("-----------------------------------------------\n");
        LLVMDumpModule(ctx->module);
    }

    if (flags & MRKLKIT_COMPILE_DUMP2) {
        TRACEC("-----------------------------------------------\n");
        tr = LLVMGetFirstTarget();
        TRACE("target name=%s descr=%s jit=%d tm=%d asm=%d triple=%s",
              LLVMGetTargetName(tr),
              LLVMGetTargetDescription(tr),
              LLVMTargetHasJIT(tr),
              LLVMTargetHasTargetMachine(tr),
              LLVMTargetHasAsmBackend(tr),
              LLVMGetTarget(ctx->module));

        TRACEC("-----------------------------------------------\n");
        tmr = LLVMCreateTargetMachine(tr,
                                      (char *)LLVMGetTarget(ctx->module),
                                      "x86-64",
                                      "",
                                      LLVMCodeGenLevelDefault,
                                      LLVMRelocDefault,
                                      LLVMCodeModelDefault);

        res = LLVMTargetMachineEmitToMemoryBuffer(tmr,
                                                  ctx->module,
                                                  LLVMAssemblyFile,
                                                  &error_msg,
                                                  &mb);
        if (res != 0) {
            TRACE("res=%d %s", res, error_msg);
            TRRET(MRKLKIT_COMPILE + 6);
        }
        TRACEC("%s", LLVMGetBufferStart(mb));
        LLVMDisposeMemoryBuffer(mb);
        LLVMDisposeTargetMachine(tmr);
    }

    return 0;
}


int
mrklkit_ctx_init_runtime(mrklkit_ctx_t *ctx, void *udata)
{
    char *error_msg = NULL;
    mrklkit_module_t **mod;
    array_iter_t it;

    LLVMLinkInJIT();
    assert(ctx->module != NULL);
    if (LLVMCreateJITCompilerForModule(&ctx->ee,
                                       ctx->module,
                                       0,
                                       &error_msg) != 0) {
        TRACE("%s", error_msg);
        FAIL("LLVMCreateExecutionEngineForModule");
    }
    //LLVMLinkInInterpreter();
    //if (LLVMCreateInterpreterForModule(&ctx->ee,
    //if (LLVMCreateExecutionEngineForModule(&ctx->ee,
    //                                       ctx->module,
    //                                       &error_msg) != 0) {
    //    TRACE("%s", error_msg);
    //    FAIL("LLVMCreateExecutionEngineForModule");
    //}

    //LLVMLinkInMCJIT();
    //LLVMInitializeMCJITCompilerOptions(&opts, sizeof(opts));
    //opts.NoFramePointerElim = 1;
    //opts.EnableFastISel = 1;
    //if (LLVMCreateMCJITCompilerForModule(&ctx->ee,
    //                                     ctx->module,
    //                                     &opts, sizeof(opts),
    //                                     &error_msg) != 0) {
    //    TRACE("%s", error_msg);
    //    FAIL("LLVMCreateMCJITCompilerForModule");
    //}

    LLVMRunStaticConstructors(ctx->ee);

    if (ctx->modules != NULL) {
        for (mod = array_last(ctx->modules, &it);
             mod != NULL;
             mod = array_prev(ctx->modules, &it)) {
            if ((*mod)->link != NULL) {
                (*mod)->link(udata, ctx->ee, ctx->module);
            }
        }
    }

    return 0;
}


int
mrklkit_call_void(mrklkit_ctx_t *ctx, const char *name)
{
    int res;
    LLVMValueRef fn;
    LLVMGenericValueRef rv;

    res = LLVMFindFunction(ctx->ee, name, &fn);
    //TRACE("res=%d", res);
    if (res == 0) {
        rv = LLVMRunFunction(ctx->ee, fn, 0, NULL);
        //TRACE("rv=%p", rv);
        //TRACE("rv=%llu", LLVMGenericValueToInt(rv, 0));
        LLVMDisposeGenericValue(rv);
    }
    return res;
}


static int
lkit_type_fini_dict(lkit_type_t *key, UNUSED lkit_type_t *value)
{
    lkit_type_destroy(&key);
    return 0;
}


void
mrklkit_ctx_init(mrklkit_ctx_t *ctx,
                 const char *name,
                 array_t *mod,
                 void *udata)
{
    array_iter_t it;
    mrklkit_module_t **m;
    lkit_type_t *ty, **pty;

    ctx->datum_root = NULL;

    dict_init(&ctx->types, 101,
             (dict_hashfn_t)lkit_type_hash,
             (dict_item_comparator_t)lkit_type_cmp,
             (dict_item_finalizer_t)lkit_type_fini_dict);

    /* builtin types */

    array_init(&ctx->builtin_types,
               sizeof(lkit_type_t *),
               _LKIT_END_OF_BUILTIN_TYPES,
               NULL,
               NULL);

    ty = lkit_type_new(LKIT_UNDEF);
    if ((pty = array_get(&ctx->builtin_types, LKIT_UNDEF)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_VOID);
    if ((pty = array_get(&ctx->builtin_types, LKIT_VOID)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_INT);
    if ((pty = array_get(&ctx->builtin_types, LKIT_INT)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_INT_MIN);
    if ((pty = array_get(&ctx->builtin_types, LKIT_INT_MIN)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_INT_MAX);
    if ((pty = array_get(&ctx->builtin_types, LKIT_INT_MAX)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_STR);
    if ((pty = array_get(&ctx->builtin_types, LKIT_STR)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_FLOAT);
    if ((pty = array_get(&ctx->builtin_types, LKIT_FLOAT)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_FLOAT_MIN);
    if ((pty = array_get(&ctx->builtin_types, LKIT_FLOAT_MIN)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_FLOAT_MAX);
    if ((pty = array_get(&ctx->builtin_types, LKIT_FLOAT_MAX)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_BOOL);
    if ((pty = array_get(&ctx->builtin_types, LKIT_BOOL)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_ANY);
    if ((pty = array_get(&ctx->builtin_types, LKIT_ANY)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    ty = lkit_type_new(LKIT_VARARG);
    if ((pty = array_get(&ctx->builtin_types, LKIT_VARARG)) == NULL) {
        FAIL("array_get");
    }
    *pty = ty;

    dict_init(&ctx->typedefs, 101,
             (dict_hashfn_t)bytes_hash,
             (dict_item_comparator_t)bytes_cmp,
             NULL /* key and value weakrefs */);

    ctx->lctx = LLVMContextCreate();
    if ((ctx->module = LLVMModuleCreateWithNameInContext(name,
            ctx->lctx)) == NULL) {
        FAIL("LLVMModuleCreateWithNameInContext");
    }
    ctx->ee = NULL;

    ctx->modules = mod;
    if (ctx->modules != NULL) {
        for (m = array_first(ctx->modules, &it);
             m != NULL;
             m = array_next(ctx->modules, &it)) {
            if ((*m)->init != NULL) {
                (*m)->init(udata);
            }
        }
    }
}


void
mrklkit_ctx_fini(mrklkit_ctx_t *ctx, void *udata)
{
    array_iter_t it;
    mrklkit_module_t **mod;

    if (ctx->modules != NULL) {
        for (mod = array_last(ctx->modules, &it);
             mod != NULL;
             mod = array_prev(ctx->modules, &it)) {
            if ((*mod)->fini != NULL) {
                (*mod)->fini(udata);
            }
        }
    }

    if (ctx->ee != NULL) {
        LLVMRunStaticDestructors(ctx->ee);
        //LLVMDisposeExecutionEngine(ctx->ee);
        ctx->ee = NULL;
    }
    if (ctx->module != NULL) {
        LLVMDisposeModule(ctx->module);
        ctx->module = NULL;
    }
    LLVMContextDispose(ctx->lctx);
    ctx->lctx = NULL;

    dict_fini(&ctx->typedefs);
    array_fini(&ctx->builtin_types);
    dict_fini(&ctx->types);

    if (ctx->datum_root != NULL) {
        fparser_datum_destroy(&ctx->datum_root);
    }
}



/*
 * Module
 *
 */

static void
llvm_init(void)
{
    LLVMPassRegistryRef pr;
    UNUSED char *error_msg = NULL;
    UNUSED struct LLVMMCJITCompilerOptions opts;

    LLVMInitializeAllAsmPrinters();
    LLVMInitializeNativeTarget();
    if ((pr = LLVMGetGlobalPassRegistry()) == NULL) {
        FAIL("LLVMGetGlobalRegistry");
    }
    LLVMInitializeCore(pr);

    LLVMInitializeTransformUtils(pr);
    LLVMInitializeScalarOpts(pr);
    LLVMInitializeObjCARCOpts(pr);

    LLVMInitializeVectorization(pr);

    LLVMInitializeInstCombine(pr);

    LLVMInitializeIPO(pr);
    LLVMInitializeInstrumentation(pr);

    LLVMInitializeAnalysis(pr);
    LLVMInitializeIPA(pr);
    LLVMInitializeCodeGen(pr);

    LLVMInitializeTarget(pr);

}

static void
llvm_fini(void)
{
    LLVMContextDispose(LLVMGetGlobalContext());
}

void
mrklkit_init(void)
{
    llvm_init();
    ltype_init();
    lexpr_init();
    lruntime_init();
}


void
mrklkit_fini(void)
{
    lruntime_fini();
    lexpr_fini();
    ltype_fini();
    llvm_fini();
}

