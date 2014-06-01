#ifndef BUILTIN_H_DEFINED
#define BUILTIN_H_DEFINED

#include <stdint.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>

#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int builtin_parse_exprdef(mrklkit_ctx_t *,
                          lkit_expr_t *,
                          array_t *,
                          array_iter_t *);

int builtin_remove_undef(mrklkit_ctx_t *,
                         lkit_expr_t *,
                         lkit_expr_t *);

int builtin_sym_compile(mrklkit_ctx_t *,
                        lkit_expr_t *,
                        LLVMModuleRef);

int builtin_compile(lkit_gitem_t **, void *);

int builtin_call_eager_initializers(lkit_expr_t *,
                                    LLVMModuleRef,
                                    LLVMBuilderRef);

LLVMValueRef builtin_compile_expr(LLVMModuleRef,
                                  LLVMBuilderRef,
                                  lkit_expr_t *);

int builtin_sym_compile_post(lkit_expr_t *, LLVMModuleRef, LLVMBuilderRef);
int builtin_call_lazy_finalizer(lkit_gitem_t **, void *);

#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
