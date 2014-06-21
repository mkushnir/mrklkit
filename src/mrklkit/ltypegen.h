#ifndef LTYPEGEN_H_DEFINED
#define LTYPEGEN_H_DEFINED

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

int ltype_compile(lkit_type_t *, LLVMModuleRef);
int ltype_compile_methods(lkit_type_t *,
                          LLVMModuleRef,
                          bytes_t *,
                          int);
int ltype_link_methods(lkit_type_t *,
                       LLVMExecutionEngineRef,
                       LLVMModuleRef,
                       bytes_t *);
#ifdef __cplusplus
}
#endif
#endif /* LTYPEGEN_H_DEFINED */
