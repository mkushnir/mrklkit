#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>

#include "unittest.h"
#include "diag.h"
#include <mrkdata.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

LLVMModuleRef module;
LLVMPassManagerRef pm;

const char *fname;

UNUSED static void
test0(void)
{
    struct {
        long rnd;
        int in;
        int expected;
    } data[] = {
        {0, 0, 0},
    };
    UNITTEST_PROLOG_RAND;

    FOREACHDATA {
        TRACE("in=%d expected=%d", CDATA.in, CDATA.expected);
        assert(CDATA.in == CDATA.expected);
    }
}

UNUSED static void
test1(void)
{
    if ((module = LLVMModuleCreateWithName("test")) == NULL) {
        FAIL("LLVMModuleCreateWithName");
    }

    if ((pm = LLVMCreateFunctionPassManagerForModule(module)) == NULL) {
        FAIL("LLVMCreateFunctionPassManagerForModule");
    }

    LLVMInitializeFunctionPassManager(pm);
    LLVMFinalizeFunctionPassManager(pm);

    LLVMDisposePassManager(pm);
    LLVMDisposeModule(module);
    module = NULL;
}

UNUSED static void
test2(void)
{
    int res;
    char *error_msg = NULL;
    LLVMTypeRef ty;
    //LLVMValueRef val, alias;
    LLVMValueRef fn;
    LLVMGenericValueRef rv;
    LLVMTypeRef fty;
    LLVMBasicBlockRef bb;
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef ee;

    if ((module = LLVMModuleCreateWithName("test")) == NULL) {
        FAIL("LLVMModuleCreateWithName");
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

    ty = LLVMStructType(NULL, 0, 0);
    //val = LLVMAddGlobal(module, ty, "main");
    //alias = LLVMAddAlias(module, ty, val, "asd");

    fty = LLVMFunctionType(LLVMInt64Type(), NULL, 0, 0);
    fn = LLVMAddFunction(module, "zxc", fty);
    LLVMSetLinkage(fn, LLVMExternalLinkage);
    bb = LLVMAppendBasicBlock(fn, "L1");

    builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, bb);
    //LLVMBuildRetVoid(builder);
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt64Type(), 123, 0));
    LLVMDisposeBuilder(builder);

    res = LLVMRunPassManager(pm, module);
    TRACE("res=%d", res);
    if (res != 0) {
        TRACE("erorr: %s", error_msg);
    }

    LLVMDumpModule(module);
    //LLVMPrintModuleToFile(module, "qwe", &error_msg);

    res = LLVMCreateExecutionEngineForModule(&ee, module, &error_msg);
    //res = LLVMCreateJITCompilerForModule(&ee, module, 0, &error_msg);
    TRACE("res=%d", res);
    if (res != 0) {
        TRACE("erorr: %s", error_msg);
    }

    LLVMRunStaticConstructors(ee);

    res = LLVMFindFunction(ee, "zxc", &fn);
    TRACE("res=%d", res);
    rv = LLVMRunFunction(ee, fn, 0, NULL);
    TRACE("rv=%p", rv);
    TRACE("rv=%llu", LLVMGenericValueToInt(rv, 0));
    LLVMDisposeGenericValue(rv);

    LLVMRunStaticDestructors(ee);

    LLVMDisposePassManager(pm);
    LLVMDisposeExecutionEngine(ee);
    //LLVMDisposeModule(module);
    //module = NULL;
}

UNUSED static void
test_init(void)
{
    LLVMPassRegistryRef pr;

    mrkdata_init();
    if ((pr = LLVMGetGlobalPassRegistry()) == NULL) {
        FAIL("LLVMGetGlobalRegistry");
    }
    LLVMInitializeCore(pr);
    LLVMInitializeTransformUtils(pr);
    LLVMInitializeScalarOpts(pr);
    LLVMInitializeObjCARCOpts(pr);
    //LLVMInitializeVectorization(pr);
    LLVMInitializeInstCombine(pr);
    //LLVMInitializeIPO(pr);
    //LLVMInitializeInstrumentation(pr);
    LLVMInitializeAnalysis(pr);
    LLVMInitializeIPA(pr);
    LLVMInitializeCodeGen(pr);
    LLVMInitializeTarget(pr);
    LLVMInitializeNativeTarget();

    //LLVMLinkInJIT();
    LLVMLinkInInterpreter();
}

UNUSED static void
test_fini(void)
{
    mrkdata_fini();
}


UNUSED static void
test3(void)
{
    int fd;
    int res;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    if ((res = mrklkit_compile(fd)) != 0) {
        perror("mrklkit_compile");
    } else {
        if ((res = mrklkit_run(".mrklkit.init.qwe")) != 0) {
            perror("mrklkit_run");
        }
        if ((res = mrklkit_run(".mrklkit.init.asd")) != 0) {
            perror("mrklkit_run");
        }
    }


    close(fd);
}

int
main(int argc, char **argv)
{

    if (argc > 1) {
        fname = argv[1];
    }
    //test_init();
    //test1();
    //test2();
    //test_fini();
    //
    mrklkit_init_module();
    test3();
    mrklkit_fini_module();
    return 0;
}
