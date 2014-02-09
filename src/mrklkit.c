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
#include <mrklkit/mrklkit.h>

#include <mrklkit/dsource.h>
#include <mrklkit/sample.h>

#include "diag.h"

LLVMModuleRef module;
LLVMPassManagerRef pm;
LLVMExecutionEngineRef ee;

/* mrklkit ctx */
static fparser_datum_t *root;

/**
 * Generic form parser
 *
 * types? vars?
 *
 */
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
                if (strcmp((char *)first, "type") == 0) {
                    if (lkit_parse_typedef(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        goto err;
                    }


                } else if (strcmp((char *)first, "var") == 0) {
                    if (lexpr_parse(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        goto err;
                    }

                } else if (strcmp((char *)first, "dsource") == 0) {

                    if (dsource_parse(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        goto err;
                    }

                } else {
                    /* ignore */
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

int
mrklkit_compile(int fd)
{
    int res;
    char *error_msg = NULL;
    LLVMTargetRef tr;
    LLVMTargetMachineRef tmr;
    LLVMMemoryBufferRef mb = NULL;
    //char *s;
    //size_t sz;

    if (mrklkit_parse(fd) != 0) {
        TRRET(MRKLKIT_COMPILE + 1);
    }
    if (ltype_transform((dict_traverser_t)ltype_compile,
                        NULL) != 0) {
        TRRET(MRKLKIT_COMPILE + 2);
    }
    if (lexpr_transform((dict_traverser_t)sample_remove_undef, NULL) != 0) {
        TRRET(MRKLKIT_COMPILE + 3);
    }

    if (lexpr_transform((dict_traverser_t)sample_compile_globals,
                        module) != 0) {
        TRRET(MRKLKIT_COMPILE + 4);
    }

    res = LLVMRunPassManager(pm, module);
    //TRACE("res=%d", res);
    //if (res != 0) {
    //    TRACE("module was modified");
    //}

    TRACEC("-----------------------------------------------\n");
    LLVMDumpModule(module);
    tr = LLVMGetFirstTarget();
    //TRACE("target name=%s descr=%s jit=%d tm=%d asm=%d triple=%s",
    //      LLVMGetTargetName(tr),
    //      LLVMGetTargetDescription(tr),
    //      LLVMTargetHasJIT(tr),
    //      LLVMTargetHasTargetMachine(tr),
    //      LLVMTargetHasAsmBackend(tr),
    //      LLVMGetTarget(module));
    tmr = LLVMCreateTargetMachine(tr,
                                  (char *)LLVMGetTarget(module),
                                  "",
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
    TRACEC("-----------------------------------------------\n%s", LLVMGetBufferStart(mb));
    LLVMDisposeMemoryBuffer(mb);

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
    TRACE("res=%d", res);
    if (res == 0) {
        rv = LLVMRunFunction(ee, fn, 0, NULL);
        TRACE("rv=%p", rv);
        TRACE("rv=%llu", LLVMGenericValueToInt(rv, 0));
        LLVMDisposeGenericValue(rv);
    }
    LLVMRunStaticDestructors(ee);
    return res;
}

int LTEST
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

    LLVMInitializeAllAsmPrinters();
    LLVMInitializeNativeTarget();
    if ((pr = LLVMGetGlobalPassRegistry()) == NULL) {
        FAIL("LLVMGetGlobalRegistry");
    }
    LLVMInitializeCore(pr);

    LLVMInitializeTransformUtils(pr);
    LLVMInitializeScalarOpts(pr);
    LLVMInitializeObjCARCOpts(pr);

    //?LLVMInitializeVectorization(pr);

    LLVMInitializeInstCombine(pr);

    LLVMInitializeIPO(pr);
    //?LLVMInitializeInstrumentation(pr);

    LLVMInitializeAnalysis(pr);
    LLVMInitializeIPA(pr);
    LLVMInitializeCodeGen(pr);

    LLVMInitializeTarget(pr);

    if ((module = LLVMModuleCreateWithName("test")) == NULL) {
        FAIL("LLVMModuleCreateWithName");
    }

    LLVMLinkInJIT();
    //LLVMLinkInMCJIT();
    //LLVMLinkInInterpreter();

    //if (LLVMCreateJITCompilerForModule(&ee,
    //                                   module,
    //                                   0,
    //                                   &error_msg) != 0) {
    //    TRACE("%s", error_msg);
    //    FAIL("LLVMCreateExecutionEngineForModule");
    //}
    //if (LLVMCreateInterpreterForModule(&ee,
    if (LLVMCreateExecutionEngineForModule(&ee,
                                           module,
                                           &error_msg) != 0) {
        TRACE("%s", error_msg);
        FAIL("LLVMCreateExecutionEngineForModule");
    }

    if ((pm = LLVMCreatePassManager()) == NULL) {
        FAIL("LLVMCreatePassManager");
    }
    LLVMAddBasicAliasAnalysisPass(pm);
    LLVMAddDeadStoreEliminationPass(pm);
    LLVMAddConstantPropagationPass(pm);
    LLVMAddInstructionCombiningPass(pm);
    LLVMAddReassociatePass(pm);
    LLVMAddGVNPass(pm);
    LLVMAddCFGSimplificationPass(pm);
    LLVMAddPromoteMemoryToRegisterPass(pm);
    LLVMAddSimplifyLibCallsPass(pm);
    LLVMAddTailCallEliminationPass(pm);
    LLVMAddConstantMergePass(pm);
}

static void
llvm_fini(void)
{
    LLVMDisposePassManager(pm);
    LLVMDisposeExecutionEngine(ee);
}

void mrklkit_init_module(void)
{
    llvm_init();

    root = NULL;

    ltype_init();
    lexpr_init();

    /* dsource module? */
    dsource_init_module();

}


void
mrklkit_fini_module(void)
{
    /* dsource module? */
    dsource_fini_module();

    lexpr_fini();
    ltype_fini();

    if (root != NULL) {
        fparser_datum_destroy(&root);
    }

    llvm_fini();
}

