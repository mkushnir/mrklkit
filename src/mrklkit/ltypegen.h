#ifndef LTYPEGEN_H_DEFINED
#define LTYPEGEN_H_DEFINED

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/bytes.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

int lkit_compile_types(mrklkit_ctx_t *, LLVMModuleRef);
LLVMTypeRef ltype_compile(mrklkit_ctx_t *, lkit_type_t *, LLVMModuleRef);
int ltype_compile_methods(mrklkit_ctx_t *,
                          lkit_type_t *,
                          LLVMModuleRef,
                          bytes_t *,
                          int);
int ltype_link_methods(lkit_type_t *,
                       LLVMExecutionEngineRef,
                       LLVMModuleRef,
                       bytes_t *);
void ltype_unlink_methods(lkit_type_t *);
#ifdef __cplusplus
}
#endif
#endif /* LTYPEGEN_H_DEFINED */
