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
#include <fparser.h>

#include "lparse.h"
#include "ltype.h"
#include "mrklkit.h"

#include "diag.h"

LLVMModuleRef module;
LLVMPassManagerRef pm;
LLVMExecutionEngineRef ee;

/* mrklkit ctx */
static fparser_datum_t *root;
static array_t dsources;
static array_t defvars;
static array_t defqueries;

/**
 * Generic form parser
 *
 * dsource defvars? defqueries?
 *
 */
int
mrklkit_parse(int fd)
{
    int res = 0;
    fparser_datum_t **glob_tok;
    array_t *form;
    array_iter_t it;

    if ((root = fparser_parse(fd, NULL, NULL)) == NULL) {
        FAIL("fparser_parse");
    }


    if (FPARSER_DATUM_TAG(root) != FPARSER_SEQ) {
        root->error = 1;
        return 1;
    }

    form = (array_t *)root->body;
    for (glob_tok = array_first(form, &it);
         glob_tok != NULL;
         glob_tok = array_next(form, &it)) {


        /* here tag can be either SEQ, or one of INT, WORD, FLOAT */
        //TRACE("tag=%s", FPARSER_TAG_STR(FPARSER_DATUM_TAG(*tok)));

        switch (FPARSER_DATUM_TAG(*glob_tok)) {
            array_t *glob_form;
            array_iter_t glob_it;
            unsigned char *first = NULL;

        case FPARSER_SEQ:
            glob_form = (array_t *)((*glob_tok)->body);

            if (lparse_first_word(glob_form, &glob_it, &first, 1) == 0) {
                if (strcmp((char *)first, "dsource") == 0) {
                    dsource_t *dsource = NULL;
                    if (ltype_parse_dsource(glob_form, &glob_it, &dsource) != 0) {
                        (*glob_tok)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }

                } else if (strcmp((char *)first, "defvar") == 0){
                } else if (strcmp((char *)first, "defquery") == 0){
                } else {
                    /* ignore */
                }
            } else {
                /* ignore */
            }

            break;

        default:
            /* ignore */
            //fparser_datum_dump(tok, NULL);
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

    ltype_init();
    root = NULL;

    if (array_init(&dsources, sizeof(dsource_t *), 0, NULL, NULL) != 0) {
        FAIL("array_init");
    }

    if (array_init(&defvars, sizeof(defvar_t *), 0, NULL, NULL) != 0) {
        FAIL("array_init");
    }

    if (array_init(&defqueries, sizeof(defquery_t *), 0, NULL, NULL) != 0) {
        FAIL("array_init");
    }

}


void
mrklkit_fini_module(void)
{
    array_fini(&defqueries);
    array_fini(&defvars);
    array_fini(&dsources);

    if (root != NULL) {
        fparser_datum_destroy(&root);
    }
    ltype_fini();

    llvm_fini();
}

