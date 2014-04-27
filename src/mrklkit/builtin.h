#ifndef BUILTIN_H_DEFINED
#define BUILTIN_H_DEFINED

#include <stdint.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int builtin_sym_parse(array_t *, array_iter_t *);
int builtin_remove_undef(lkit_expr_t *);

int builtin_sym_compile(LLVMModuleRef);
int builtin_compile(lkit_gitem_t **gitem, void *udata);
int builtin_call_eager_initializers(LLVMModuleRef, LLVMBuilderRef);
LLVMValueRef builtin_compile_expr(LLVMModuleRef,
                                  LLVMBuilderRef,
                                  lkit_expr_t *);

lkit_expr_t *builtin_get_root_ctx(void);

void builtin_init(void);
void builtin_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
