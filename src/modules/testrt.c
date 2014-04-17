//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>

#include <mrklkit/module.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/testrt.h>

static lkit_expr_t root;

static int
testrt_parse(UNUSED array_t *form, UNUSED array_iter_t *it)
{
    /* (testrt name value) */
    return lkit_parse_exprdef(&root, form, it);
}

UNUSED static int
_acb(lkit_gitem_t **gitem, void *udata)
{
    int (*cb)(lkit_expr_t *) = udata;
    return cb((*gitem)->expr);
}

static int
compile(lkit_gitem_t **gitem, UNUSED void *udata)
{
    bytes_t *name = (*gitem)->name;
    lkit_expr_t *expr = (*gitem)->expr;
    //LLVMModuleRef module = (LLVMModuleRef)udata;

    TRACE("compiling %s", name->data);
    lkit_expr_dump(expr);
    return 0;
}

static int
testrt_compile(UNUSED LLVMModuleRef module)
{
    if (array_traverse(&root.glist, (array_traverser_t)compile, module) != 0) {
        return 1;
    }
    return 0;
}

static void
testrt_init(void)
{
    lexpr_init_ctx(&root);
}

static void
testrt_fini(void)
{
    lexpr_fini_ctx(&root);
}

static mrklkit_parser_info_t parsers[] = {
    {"testrt", testrt_parse},
    {NULL, NULL},
};

mrklkit_module_t testrt_module = {
    testrt_init,
    testrt_fini,
    parsers,
    testrt_compile,
};
