#ifndef SAMPLE_H_DEFINED
#define SAMPLE_H_DEFINED

#include <mrklkit/fparser.h>
#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int sample_remove_undef(bytes_t *, lkit_expr_t *, void *);
int sample_compile_globals(bytes_t *, lkit_expr_t *, void *);
#ifdef __cplusplus
}
#endif
#endif /* SAMPLE_H_DEFINED */
