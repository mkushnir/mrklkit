#ifndef BUILTIN_H_DEFINED
#define BUILTIN_H_DEFINED

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int builtin_remove_undef(bytes_t *, lkit_expr_t *, void *);
int builtin_compile_globals(bytes_t *, lkit_expr_t *, void *);
#ifdef __cplusplus
}
#endif
#endif /* BUILTIN_H_DEFINED */
