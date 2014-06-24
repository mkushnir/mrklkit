#ifndef BUILTIN_H_DEFINED
#define BUILTIN_H_DEFINED

#include <stdint.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>

#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LKIT_BUILTIN_PARSE_EXPRDEF_ISBLTIN (0x01)
#define LKIT_BUILTIN_PARSE_EXPRDEF_FORCELAZY (0x02)
#define LKIT_BUILTIN_PARSE_EXPRDEF_CUSTOMCOMPILE (0x04)
int builtin_parse_exprdef(mrklkit_ctx_t *,
                          lkit_expr_t *,
                          array_t *,
                          array_iter_t *,
                          int);

int builtin_remove_undef(mrklkit_ctx_t *,
                         lkit_expr_t *,
                         lkit_expr_t *);

LLVMValueRef lkit_compile_expr(mrklkit_ctx_t *,
                               lkit_expr_t *,
                               LLVMModuleRef,
                               LLVMBuilderRef,
                               lkit_expr_t *,
                               void *);


int lkit_expr_ctx_compile(mrklkit_ctx_t *,
                          lkit_expr_t *,
                          LLVMModuleRef,
                          void *udata);

int lkit_expr_ctx_compile_pre(lkit_expr_t *, LLVMModuleRef, LLVMBuilderRef);
int lkit_expr_ctx_compile_post(lkit_expr_t *, LLVMModuleRef, LLVMBuilderRef);

#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
