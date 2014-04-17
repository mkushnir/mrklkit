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

#define LKIT_UTAG_BUILTIN_IF 1
#define LKIT_UTAG_BUILTIN_PRINT 1
#define LKIT_UTAG_BUILTIN_PLUS 1
#define LKIT_UTAG_BUILTIN_MINUS 1
#define LKIT_UTAG_BUILTIN_DIV 1
#define LKIT_UTAG_BUILTIN_MUL 1

int builtin_sym_parse(array_t *, array_iter_t *);
int builtin_sym_compile(LLVMModuleRef);


void builtin_init(void);
void builtin_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
