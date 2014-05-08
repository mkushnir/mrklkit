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

#include <mrklkit/builtin.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

/**
 * Generic form parser
 *
 * types? vars?
 *
 */

int mrklkit_register_parser(UNUSED const char *keyword,
                            UNUSED mrklkit_parser_t cb,
                            UNUSED void *udata)
{
    return 0;
}

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
        TR(MRKLKIT_PARSE + 1);
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
            unsigned char *first = NULL;

        case FPARSER_SEQ:
            nform = (array_t *)((*fnode)->body);

            if (lparse_first_word(nform, &nit, &first, 1) == 0) {
                /* statements */
                if (strcmp((char *)first, "type") == 0) {
                    if (lkit_parse_typedef(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        TR(MRKLKIT_PARSE + 2);
                        goto err;
                    }

                } else if (strcmp((char *)first, "sym") == 0) {
                    if (builtin_sym_parse(ctx, nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        TR(MRKLKIT_PARSE + 3);
                        goto err;
                    }

                } else {
                    mrklkit_module_t **mod;
                    array_iter_t it;

                    for (mod = array_first(ctx->modules, &it);
                         mod != NULL;
                         mod = array_next(ctx->modules, &it)) {

                        if ((*mod)->parsers != NULL) {
                            mrklkit_parser_info_t *pi;

                            for (pi = (*mod)->parsers;
                                 pi->keyword != NULL;
                                 ++pi) {

                                if (strcmp((char *)first, pi->keyword) == 0) {
                                    if (pi->parser(udata, nform, &nit) != 0) {
                                        (*fnode)->error = 1;
                                        TR(MRKLKIT_PARSE + 2);
                                        goto err;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                /* ignore */
            }

            break;

        default:
            /* ignore */
            ;
        }
    }

end:
    return res;

err:
    fparser_datum_dump_formatted(ctx->datum_root);
    res = 1;
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


static int
ltype_compile_cb(lkit_type_t *kty, UNUSED lkit_type_t *vty, void *udata)
{
    return ltype_compile(kty, udata);
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

    /* precompile */

    for (mod = array_first(ctx->modules, &it);
         mod != NULL;
         mod = array_next(ctx->modules, &it)) {

        if ((*mod)->precompile != NULL) {
            if ((*mod)->precompile(udata)) {
                TRRET(MRKLKIT_COMPILE + 2);
            }
        }
    }


    if (ltype_transform((dict_traverser_t)ltype_compile_cb,
                        ctx) != 0) {
        TRRET(MRKLKIT_COMPILE + 3);
    }

    /* compile */

    /* builtin */
    if (builtin_sym_compile(ctx, ctx->module) != 0) {
        TRRET(MRKLKIT_COMPILE + 4);
    }

    /* modules */

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
mrklkit_init_runtime(mrklkit_ctx_t *ctx, void *udata)
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


void
mrklkit_ctx_init(mrklkit_ctx_t *ctx,
                 const char *name,
                 array_t *mod,
                 void *udata)
{
    array_iter_t it;
    mrklkit_module_t **m;

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

    ctx->datum_root = NULL;

    lkit_expr_init(&ctx->builtin_root, NULL);

    ctx->lctx = LLVMContextCreate();
    if ((ctx->module = LLVMModuleCreateWithNameInContext(name,
            ctx->lctx)) == NULL) {
        FAIL("LLVMModuleCreateWithNameInContext");
    }
    ctx->ee = NULL;
}


void
mrklkit_ctx_fini(mrklkit_ctx_t *ctx, void *udata)
{
    array_iter_t it;
    mrklkit_module_t **mod;

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

    lkit_expr_fini(&ctx->builtin_root);

    if (ctx->datum_root != NULL) {
        fparser_datum_destroy(&ctx->datum_root);
    }

    if (ctx->modules != NULL) {
        for (mod = array_last(ctx->modules, &it);
             mod != NULL;
             mod = array_prev(ctx->modules, &it)) {
            if ((*mod)->fini != NULL) {
                (*mod)->fini(udata);
            }
        }
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
    builtin_init();
    lruntime_init();
}


void
mrklkit_fini(void)
{
    lruntime_fini();
    builtin_fini();
    lexpr_fini();
    ltype_fini();
    llvm_fini();
}

