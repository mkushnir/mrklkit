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
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/module.h>
#include <mrklkit/mrklkit.h>

#include <mrklkit/dsource.h>
#include <mrklkit/builtin.h>
#include <mrklkit/testrt.h>

#include "diag.h"

LLVMModuleRef module;
LLVMExecutionEngineRef ee;

/* mrklkit ctx */
static fparser_datum_t *root;
static array_t *modules = NULL;

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
mrklkit_parse(int fd)
{
    int res = 0;
    array_t *form;
    array_iter_t it;
    fparser_datum_t **fnode;

    if ((root = fparser_parse(fd, NULL, NULL)) == NULL) {
        FAIL("fparser_parse");
    }


    if (FPARSER_DATUM_TAG(root) != FPARSER_SEQ) {
        root->error = 1;
        goto err;
    }

    form = (array_t *)root->body;
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
                        goto err;
                    }

                } else if (strcmp((char *)first, "sym") == 0) {
                    if (builtin_sym_parse(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        goto err;
                    }

                //} else if (strcmp((char *)first, "testrt") == 0) {
                //    if (testrt_parse(nform, &nit) != 0) {
                //        (*fnode)->error = 1;
                //        goto err;
                //    }

                //} else if (strcmp((char *)first, "dsource") == 0) {
                //    if (dsource_parse(nform, &nit) != 0) {
                //        (*fnode)->error = 1;
                //        goto err;
                //    }

                } else {
                    mrklkit_module_t **mod;
                    array_iter_t it;

                    for (mod = array_first(modules, &it);
                         mod != NULL;
                         mod = array_next(modules, &it)) {

                        if ((*mod)->parsers != NULL) {
                            mrklkit_parser_info_t *pi;

                            for (pi = (*mod)->parsers;
                                 pi->keyword != NULL;
                                 ++pi) {

                                if (strcmp((char *)first, pi->keyword) == 0) {
                                    if (pi->parser(nform, &nit) != 0) {
                                        (*fnode)->error = 1;
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
    fparser_datum_dump_formatted(root);
    return res;

err:
    res = 1;
    goto end;
}


static void
do_analysis(void)
{
    LLVMPassManagerBuilderRef pmb;
    LLVMPassManagerRef fpm;
    LLVMPassManagerRef pm;
    int res = 0;
    LLVMValueRef fn = NULL;

    if ((fpm = LLVMCreateFunctionPassManagerForModule(module)) == NULL) {
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

    for (fn = LLVMGetFirstFunction(module);
         fn != NULL;
         fn = LLVMGetNextFunction(fn)) {

        //TRACE("Passing %s", LLVMGetValueName(fn));
        //LLVMDumpValue(fn);
        if (LLVMRunFunctionPassManager(fpm, fn)) {
            //TRACE("%s was modified:", LLVMGetValueName(fn));
            //LLVMDumpValue(fn);
        }
    }

    res = LLVMRunPassManager(pm, module);
    //TRACE("res=%d", res);
    //if (res != 0) {
    //    TRACE("module was modified");
    //}

    LLVMPassManagerBuilderDispose(pmb);
    LLVMDisposePassManager(pm);
    LLVMDisposePassManager(fpm);

}


int
mrklkit_compile(int fd)
{
    UNUSED int res = 0;
    UNUSED char *error_msg = NULL;
    UNUSED LLVMTargetRef tr;
    UNUSED LLVMTargetMachineRef tmr;
    UNUSED LLVMMemoryBufferRef mb = NULL;
    mrklkit_module_t **mod;
    array_iter_t it;

    if (mrklkit_parse(fd) != 0) {
        TRRET(MRKLKIT_COMPILE + 1);
    }
    if (ltype_transform((dict_traverser_t)ltype_compile,
                        NULL) != 0) {
        TRRET(MRKLKIT_COMPILE + 2);
    }

    /* builtin */
    if (builtin_sym_compile(module) != 0) {
        TRRET(MRKLKIT_COMPILE + 3);
    }

    for (mod = array_first(modules, &it);
         mod != NULL;
         mod = array_next(modules, &it)) {

        if ((*mod)->compile != NULL) {
            if ((*mod)->compile(module)) {
                TRRET(MRKLKIT_COMPILE + 4);
            }
        }
    }

#if 1
    TRACEC("-----------------------------------------------\n");
    LLVMDumpModule(module);
#endif

    /* LLVM analysis */
    do_analysis();

#if 0
    TRACEC("-----------------------------------------------\n");
    LLVMDumpModule(module);
#endif

#if 0
    TRACEC("-----------------------------------------------\n");
    tr = LLVMGetFirstTarget();
    TRACE("target name=%s descr=%s jit=%d tm=%d asm=%d triple=%s",
          LLVMGetTargetName(tr),
          LLVMGetTargetDescription(tr),
          LLVMTargetHasJIT(tr),
          LLVMTargetHasTargetMachine(tr),
          LLVMTargetHasAsmBackend(tr),
          LLVMGetTarget(module));

    TRACEC("-----------------------------------------------\n");
    tmr = LLVMCreateTargetMachine(tr,
                                  (char *)LLVMGetTarget(module),
                                  "x86-64",
                                  "",
                                  LLVMCodeGenLevelDefault,
                                  LLVMRelocDefault,
                                  LLVMCodeModelDefault);

    res = LLVMTargetMachineEmitToMemoryBuffer(tmr,
                                              module,
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
#endif

    return 0;
}

int
mrklkit_run(const char *name)
{
    int res;
    LLVMValueRef fn;
    LLVMGenericValueRef rv;

    LLVMRunStaticConstructors(ee);

    res = LLVMFindFunction(ee, name, &fn);
    //TRACE("res=%d", res);
    if (res == 0) {
        rv = LLVMRunFunction(ee, fn, 0, NULL);
        //TRACE("rv=%p", rv);
        //TRACE("rv=%llu", LLVMGenericValueToInt(rv, 0));
        LLVMDisposeGenericValue(rv);
    }
    LLVMRunStaticDestructors(ee);
    return res;
}

int64_t LTEST
(char *arg)
{
    TRACE("arg %p", arg);
    TRACE("argval: '%s'", arg);
    return 123;
}

/*
 * Module
 *
 */

static void
llvm_init(void)
{
    LLVMPassRegistryRef pr;
    char *error_msg = NULL;
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

    if ((module = LLVMModuleCreateWithName("test")) == NULL) {
        FAIL("LLVMModuleCreateWithName");
    }

    LLVMLinkInJIT();

    if (LLVMCreateJITCompilerForModule(&ee,
                                       module,
                                       0,
                                       &error_msg) != 0) {
        TRACE("%s", error_msg);
        FAIL("LLVMCreateExecutionEngineForModule");
    }
    //LLVMLinkInInterpreter();
    //if (LLVMCreateInterpreterForModule(&ee,
    //if (LLVMCreateExecutionEngineForModule(&ee,
    //                                       module,
    //                                       &error_msg) != 0) {
    //    TRACE("%s", error_msg);
    //    FAIL("LLVMCreateExecutionEngineForModule");
    //}

    //LLVMLinkInMCJIT();
    //LLVMInitializeMCJITCompilerOptions(&opts, sizeof(opts));
    //opts.NoFramePointerElim = 1;
    //opts.EnableFastISel = 1;
    //if (LLVMCreateMCJITCompilerForModule(&ee,
    //                                     module,
    //                                     &opts, sizeof(opts),
    //                                     &error_msg) != 0) {
    //    TRACE("%s", error_msg);
    //    FAIL("LLVMCreateMCJITCompilerForModule");
    //}

}

static void
llvm_fini(void)
{
    LLVMDisposeExecutionEngine(ee);
}

void
mrklkit_init(array_t *mod)
{
    array_iter_t it;
    mrklkit_module_t **m;

    llvm_init();
    root = NULL;
    ltype_init();
    lexpr_init();
    builtin_init();

    modules = mod;
    if (modules != NULL) {
        for (m = array_first(modules, &it);
             m != NULL;
             m = array_next(modules, &it)) {
            if ((*m)->init != NULL) {
                (*m)->init();
            }
        }
    }

    /* dsource module? */
    //dsource_init_module();
}


void
mrklkit_fini(void)
{
    array_iter_t it;
    mrklkit_module_t **mod;

    if (modules != NULL) {
        for (mod = array_last(modules, &it);
             mod != NULL;
             mod = array_prev(modules, &it)) {
            if ((*mod)->fini != NULL) {
                (*mod)->fini();
            }
        }
    }
    ///* dsource module? */
    //dsource_fini_module();

    builtin_fini();
    lexpr_fini();
    ltype_fini();

    /* reverse to mrklkit_parse */
    if (root != NULL) {
        fparser_datum_destroy(&root);
    }

    llvm_fini();
}

