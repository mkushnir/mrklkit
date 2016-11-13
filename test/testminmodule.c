#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Target.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/builtin.h>
#include <mrklkit/module.h>
#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"


/*
 * minimal module template
 */
typedef struct {
    mrklkit_ctx_t mctx;
    fparser_datum_t *datum_root;
    lkit_cexpr_t builtin;
} minmodule_ctx_t;


static void
minmodule_ctx_init(minmodule_ctx_t *ctx)
{
    ctx->datum_root = NULL;
    lkit_cexpr_init(&ctx->builtin, NULL);
}


static void
minmodule_ctx_fini(minmodule_ctx_t *ctx)
{
    lkit_cexpr_fini(&ctx->builtin);
    fparser_datum_destroy(&ctx->datum_root);
}


static int
parse_load(UNUSED minmodule_ctx_t *ctx,
           UNUSED array_t *form,
           UNUSED array_iter_t *it)
{
    return 0;
}


static int
parse_logging(UNUSED minmodule_ctx_t *ctx,
              UNUSED array_t *form,
              UNUSED array_iter_t *it)
{
    return 0;
}


static int
parse_daemon(UNUSED minmodule_ctx_t *ctx,
             UNUSED array_t *form,
             UNUSED array_iter_t *it)
{
    return 0;
}


static int
parse_vhost(UNUSED minmodule_ctx_t *ctx,
            UNUSED array_t *form,
            UNUSED array_iter_t *it)
{
    return 0;
}


typedef struct _minmodule_config {
    const char *name;
    int (*parser)(minmodule_ctx_t *, array_t *, array_iter_t *);
} minmodule_config_t;

static minmodule_config_t minmodule_toplevels[] = {
    {"load", parse_load},
    {"logging", parse_logging},
    {"daemon", parse_daemon},
    {"vhost", parse_vhost},
};

static int
minmodule_parse_top_level(minmodule_ctx_t *ctx,
                         const char *name,
                         array_t *form,
                         array_iter_t *it)
{
    /*
     * required by mrklkit
     */
    if (strcmp(name, "type") == 0) {
        return lkit_parse_typedef(&ctx->mctx, form, it);
    } else if (strcmp(name, "builtin") == 0) {
        return builtin_parse_exprdef(&ctx->mctx,
                                     &ctx->builtin,
                                     form,
                                     it,
                                     LKIT_BUILTIN_PARSE_EXPRDEF_ISBLTIN);
    } else if (strcmp(name, "macro") == 0) {
        return builtin_parse_exprdef(&ctx->mctx,
                                     &ctx->builtin,
                                     form,
                                     it,
                                     LKIT_BUILTIN_PARSE_EXPRDEF_MACRO);
    } else if (strcmp(name, "sym") == 0) {
        return builtin_parse_exprdef(&ctx->mctx, &ctx->builtin, form, it, 0);
    } else if (strcmp(name, "load") == 0){
        return parse_load(ctx, form, it);
    } else if (strcmp(name, "pragma-if") == 0){
    } else {
        unsigned i;

        /*
         * custom dsl
         */
        for (i = 0; i < countof(minmodule_toplevels); ++i) {
            if (strcmp(name, minmodule_toplevels[i].name) == 0) {
                return minmodule_toplevels[i].parser(ctx, form, it);
            }
        }
    }
    return 1;
}


static int
minmodule_parse(mrklkit_ctx_t *mctx, int fd, minmodule_ctx_t *ctx)
{
    return mrklkit_parse(mctx, fd, ctx, &ctx->datum_root);
}


static int
minmodule_post_parse(UNUSED minmodule_ctx_t *ctx)
{
    return 0;
}


static int
minmodule_remove_undef(UNUSED mrklkit_ctx_t *mctx,
                       UNUSED lkit_cexpr_t *cexpr,
                       UNUSED lkit_expr_t *expr)
{
    return 0;
}


static int
minmodule_compile_type_method(UNUSED mrklkit_ctx_t *mctx,
                              UNUSED lkit_type_t *ty,
                              UNUSED LLVMModuleRef module)
{
    return 0;
}


static int
minmodule_compile_type(minmodule_ctx_t *ctx, LLVMModuleRef module)
{
    return lkit_compile_types(&ctx->mctx, module);
}


static int
minmodule_compile_expr(UNUSED mrklkit_ctx_t *mctx,
                       UNUSED lkit_cexpr_t *cexpr,
                       UNUSED LLVMModuleRef module,
                       UNUSED LLVMBuilderRef builder,
                       UNUSED lkit_expr_t *expr,
                       UNUSED minmodule_ctx_t *ctx)
{
    return 0;
}


static int
minmodule_compile(minmodule_ctx_t *ctx, LLVMModuleRef module)
{
    /*
     * builtins
     */
    if (lkit_expr_ctx_analyze(&ctx->mctx, &ctx->builtin) != 0) {
        return 1;
    }

    if (lkit_expr_ctx_compile(&ctx->mctx,
                              &ctx->builtin,
                              module,
                              ctx) != 0) {
        return 1;
    }

    /*
     * type methods, depend on builtins
     */
    if (lkit_compile_type_methods(&ctx->mctx, module) != 0) {
        return 1;
    }

    return 0;
}


static int
minmodule_method_link(UNUSED mrklkit_ctx_t *mctx,
                      UNUSED lkit_type_t *ty,
                      UNUSED LLVMExecutionEngineRef ee,
                      UNUSED LLVMModuleRef module)
{
    return 0;
}


static int
minmodule_link(minmodule_ctx_t *ctx,
               LLVMExecutionEngineRef ee,
               LLVMModuleRef module)
{
    return lkit_link_types(&ctx->mctx, ee, module);
}


static int
minmodule_method_unlink(UNUSED mrklkit_ctx_t *mctx,
                      UNUSED lkit_type_t *ty)
{
    return 0;
}


static int
minmodule_unlink(minmodule_ctx_t *ctx)
{
    return lkit_unlink_types(&ctx->mctx);
}


mrklkit_module_t minmodule = {
    (mrklkit_module_initializer_t)minmodule_ctx_init,
    (mrklkit_module_finalizer_t)minmodule_ctx_fini,
    (mrklkit_expr_parser_t)minmodule_parse_top_level,
    (mrklkit_parser_t)minmodule_parse,
    (mrklkit_post_parser_t)minmodule_post_parse,
    /* (mrklkit_remove_undef_t) */ minmodule_remove_undef,
    /* (mrklkit_type_method_compiler_t) */ minmodule_compile_type_method,
    (mrklkit_type_compiler_t)minmodule_compile_type,
    (mrklkit_expr_compiler_t)minmodule_compile_expr,
    (mrklkit_module_compiler_t)minmodule_compile,
    /* (mrklkit_method_linker_t) */ minmodule_method_link,
    (mrklkit_module_linker_t)minmodule_link,
    /* (mrklkit_method_unlinker_t) */ minmodule_method_unlink,
    (mrklkit_module_unlinker_t)minmodule_unlink,
    NULL, /* mrklkit_module_dpexpr_find_t dpexpr_find; */
};

mrklkit_module_t *modules[] = {
    &minmodule,
};


static int
mycompile(minmodule_ctx_t *ctx, int fd)
{
    FILE *tmpfd;
    size_t nwritten;

    if ((tmpfd = tmpfile()) == NULL) {
        FAIL("tmpfile");
    }
    if ((nwritten = fwrite(mrklkit_meta,
                           1,
                           strlen(mrklkit_meta),
                           tmpfd)) == 0) {
        FAIL("fwrite");
    }

    while (true) {
        char buf[1024];
        ssize_t nread;

        if ((nread = read(fd, buf, sizeof(buf))) <= 0) {
            break;
        }
        if ((nwritten = fwrite(buf,
                               1,
                               nread,
                               tmpfd)) == 0) {
            FAIL("fwrite");
        }
    }

    if (fseek(tmpfd, 0L, SEEK_SET) < 0) {
        FAIL("fseek");
    }

    if (mrklkit_compile(
            &ctx->mctx,
            fileno(tmpfd),
            MRKLKIT_COMPILE_DUMP0|MRKLKIT_COMPILE_DUMP1|MRKLKIT_COMPILE_DUMP2,
            ctx) != 0) {
        FAIL("mrklkit_compile");
    }
    fclose(tmpfd);

    return 0;
}


int
main(int argc, char **argv)
{
    int res;
    int fd;
    minmodule_ctx_t ctx;

    if (argc > 1) {
        if ((fd = open(argv[1], O_RDONLY)) < 0) {
            FAIL("open");
        }
    } else {
        fd = 0;
    }

    mrklkit_init();

    mrklkit_ctx_init(&ctx.mctx, "qwe", &ctx, modules, 1);

    res = mycompile(&ctx, fd);
    assert(res == 0);

    mrklkit_ctx_setup_runtime(&ctx.mctx, &ctx);

    mrklkit_ctx_cleanup_runtime(&ctx.mctx, &ctx);

    mrklkit_ctx_fini(&ctx.mctx);

    mrklkit_fini();

    return 0;
}
