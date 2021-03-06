#ifndef MODULE_H_DEFINED
#define MODULE_H_DEFINED

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/ltype.h>
#include <mrklkit/dpexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mrklkit_module_initializer_t)(void *);

typedef void (*mrklkit_module_finalizer_t)(void *);

typedef int (*mrklkit_type_parser_t)(void *, mnarray_t *,
                                     mnarray_iter_t *);

typedef int (*mrklkit_type_method_compiler_t)(mrklkit_ctx_t *,
                                              lkit_type_t *,
                                              LLVMModuleRef);

typedef int (*mrklkit_type_compiler_t)(void *, LLVMModuleRef);

typedef int (*mrklkit_expr_parser_t)(void *,
                                     const char *,
                                     mnarray_t *,
                                     mnarray_iter_t *);

typedef int (*mrklkit_parser_t)(mrklkit_ctx_t *,
                                int,
                                void *);

typedef int (*mrklkit_post_parser_t)(void *);
typedef int (*mrklkit_remove_undef_t)(mrklkit_ctx_t *,
                                      lkit_cexpr_t *,
                                      lkit_expr_t *);

typedef LLVMValueRef (*mrklkit_expr_compiler_t)(mrklkit_ctx_t *,
                                                lkit_cexpr_t *,
                                                LLVMModuleRef,
                                                LLVMBuilderRef,
                                                lkit_expr_t *,
                                                void *);

typedef int (*mrklkit_module_compiler_t)(void *, LLVMModuleRef);

typedef int (*mrklkit_method_linker_t)(mrklkit_ctx_t *,
                                       lkit_type_t *,
                                       LLVMExecutionEngineRef,
                                       LLVMModuleRef);

typedef int (*mrklkit_module_linker_t)(void *,
                                       LLVMExecutionEngineRef,
                                       LLVMModuleRef);

typedef int (*mrklkit_method_unlinker_t)(mrklkit_ctx_t *,
                                         lkit_type_t *);

typedef int (*mrklkit_module_unlinker_t)(void *);

typedef lkit_dpexpr_t *(*mrklkit_module_dpexpr_find_t)(mrklkit_ctx_t *, mnbytes_t *, void *);

typedef struct _mrklkit_module {
    mrklkit_module_initializer_t init;
    mrklkit_module_finalizer_t fini;
    mrklkit_expr_parser_t parse_expr;
    mrklkit_parser_t parse;
    mrklkit_post_parser_t post_parse;
    mrklkit_remove_undef_t remove_undef;
    mrklkit_type_method_compiler_t compile_type_method;
    mrklkit_type_compiler_t compile_type;
    mrklkit_expr_compiler_t compile_expr;
    mrklkit_module_compiler_t compile;
    mrklkit_method_linker_t method_link;
    mrklkit_module_linker_t link;
    mrklkit_method_unlinker_t method_unlink;
    mrklkit_module_unlinker_t unlink;
    mrklkit_module_dpexpr_find_t dpexpr_find;
} mrklkit_module_t;


#ifdef __cplusplus
}
#endif
#endif /* MODULE_H_DEFINED */
