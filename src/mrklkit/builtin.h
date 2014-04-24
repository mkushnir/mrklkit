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
int builtin_sym_compile(LLVMModuleRef);

lkit_expr_t *builtin_get_root_ctx(void);

void builtin_init(void);
void builtin_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
