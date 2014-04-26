#ifndef BUILTIN_PRIVATE_H_DEFINED
#define BUILTIN_PRIVATE_H_DEFINED

#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int builtingen_sym_compile(lkit_gitem_t **gitem, void *udata);
int builtin_call_eager_iitializers(LLVMModuleRef, LLVMBuilderRef);
#ifdef __cplusplus
}
#endif

#include <mrklkit/builtin.h>

#endif /* BUILTIN_PRIVATE_H_DEFINED */
