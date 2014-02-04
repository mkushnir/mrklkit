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
    fparser_datum_t **glob_node;
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
    for (glob_node = array_first(form, &it);
         glob_node != NULL;
         glob_node = array_next(form, &it)) {


        /* here tag can be either SEQ, or one of INT, WORD, FLOAT */
        switch (FPARSER_DATUM_TAG(*glob_node)) {
            array_t *glob_form;
            array_iter_t glob_it;
            unsigned char *first = NULL;

        case FPARSER_SEQ:
            glob_form = (array_t *)((*glob_node)->body);

            if (lparse_first_word(glob_form, &glob_it, &first, 1) == 0) {

                if (strcmp((char *)first, "type") == 0) {
                    if (lkit_parse_typedef(glob_form, &glob_it) != 0) {
                        (*glob_node)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }


                } else if (strcmp((char *)first, "var") == 0) {
                    unsigned char *varname = NULL;
                    fparser_datum_t **loc_node;
                    lkit_type_t *ty;

                    if (lparse_next_word(glob_form,
                                         &glob_it,
                                         &varname,
                                         1) != 0) {
                        (*glob_node)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }

                    if ((loc_node = array_next(glob_form, &glob_it)) == NULL) {
                        (*glob_node)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }

                    if ((ty = lkit_type_parse(*loc_node)) == NULL) {
                        /* expr found ? */
                    } else {
                    }


                } else if (strcmp((char *)first, "dsource") == 0) {
                    dsource_t *dsource = NULL;
                    bytestream_t bs;

                    if (lkit_parse_dsource(glob_form,
                                            &glob_it,
                                            &dsource) != 0) {
                        (*glob_node)->error = 1;
                        fparser_datum_dump_formatted(root);
                        return 1;
                    }

                    bytestream_init(&bs);
                    lkit_type_str((lkit_type_t *)(dsource->fields), &bs);
                    bytestream_cat(&bs, 2, "\n\0");
                    bytestream_write(&bs, 2, bs.eod);
                    bytestream_fini(&bs);

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

