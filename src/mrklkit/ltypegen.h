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
int lkit_compile_type_methods(mrklkit_ctx_t *, LLVMModuleRef);
int lkit_link_types(mrklkit_ctx_t *, LLVMExecutionEngineRef, LLVMModuleRef);
int lkit_unlink_types(mrklkit_ctx_t *);
#ifdef __cplusplus
}
#endif
#endif /* LTYPEGEN_H_DEFINED */
