#ifndef MODULE_H_DEFINED
#define MODULE_H_DEFINED

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mrklkit_module_initializer_t)(void *);
typedef void (*mrklkit_module_finalizer_t)(void *);

typedef int (*mrklkit_type_parser_t)(void *, array_t *,
                                     array_iter_t *);

typedef int (*mrklkit_type_compiler_t)(void *, LLVMModuleRef);

typedef int (*mrklkit_expr_parser_t)(void *,
                                     const char *,
                                     array_t *,
                                     array_iter_t *);

typedef int (*mrklkit_post_parser_t)(void *);

typedef LLVMValueRef (*mrklkit_expr_compiler_t)(mrklkit_ctx_t *,
                                                lkit_expr_t *,
                                                LLVMModuleRef,
                                                LLVMBuilderRef,
                                                lkit_expr_t *);

typedef int (*mrklkit_module_compiler_t)(void *, LLVMModuleRef);

typedef int (*mrklkit_module_linker_t)(void *,
                                       LLVMExecutionEngineRef,
                                       LLVMModuleRef);

typedef struct _mrklkit_module {
    mrklkit_module_initializer_t init;
    mrklkit_module_finalizer_t fini;
    mrklkit_expr_parser_t parse_expr;
    mrklkit_post_parser_t post_parse;
    mrklkit_type_compiler_t compile_type;
    mrklkit_expr_compiler_t compile_expr;
    mrklkit_module_compiler_t compile;
    mrklkit_module_linker_t link;
} mrklkit_module_t;


#ifdef __cplusplus
}
#endif
#endif /* MODULE_H_DEFINED */

