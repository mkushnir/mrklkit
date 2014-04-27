#ifndef LTYPEGEN_H_DEFINED
#define LTYPEGEN_H_DEFINED

#include <llvm-c/Core.h>

#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

int ltype_compile(lkit_type_t *, void *);
int ltype_compile_methods(lkit_type_t *,
                          LLVMModuleRef,
                          bytes_t *);

#ifdef __cplusplus
}
#endif
#endif /* LTYPEGEN_H_DEFINED */
