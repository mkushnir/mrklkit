#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/mrklkit.h>

#include <mrklkit/dsource.h>

#include "diag.h"

LLVMModuleRef module;
LLVMPassManagerRef pm;
LLVMExecutionEngineRef ee;

/* mrklkit ctx */
static fparser_datum_t *root;

/**
 * Generic form parser
 *
 * dsource vars? queries?
 *
 */
int
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
        return 1;
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
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }


                } else if (strcmp((char *)first, "var") == 0) {
                    if (lkit_expr_parse(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }

                } else if (strcmp((char *)first, "dsource") == 0) {

                    if (lkit_parse_dsource(nform, &nit) != 0) {
                        (*fnode)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
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

    fparser_datum_dump_formatted(root);
    return res;
}

int
mrklkit_compile(int fd)
{
    if (mrklkit_parse(fd) != 0) {
        return 1;
    }
    return 0;
}

static void
llvm_init(void)
{
    LLVMPassRegistryRef pr;
    char *error_msg = NULL;

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

    if ((module = LLVMModuleCreateWithName("test")) == NULL) {
        FAIL("LLVMModuleCreateWithName");
    }

    if (LLVMCreateExecutionEngineForModule(&ee,
                                           module,
                                           &error_msg) != 0) {
        FAIL("LLVMCreateExecutionEngineForModule");
    }
    LLVMRunStaticConstructors(ee);

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
}

static void
llvm_fini(void)
{
    LLVMRunStaticDestructors(ee);
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

