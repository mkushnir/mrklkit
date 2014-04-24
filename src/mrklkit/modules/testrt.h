#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#include <mrklkit/module.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/modules/dsource.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _testrt {
    lkit_expr_t *expr;
    bytes_t *dsource;
    bytes_t *id;
} testrt_t;

extern mrklkit_module_t testrt_module;

#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
