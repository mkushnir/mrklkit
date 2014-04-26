#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/module.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dsource.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _testrt {
    bytes_t *dsource;
    bytes_t *id;
    lkit_expr_t *doexpr;
    lkit_expr_t *takeexpr;
    lkit_expr_t *seeexpr;
    lkit_struct_t *type;
    LLVMTypeRef backend;
} testrt_t;

#define TESTRT_START "testrt.start"
#define TESTRT_TARGET "testrt_target"
extern mrklkit_module_t testrt_module;
extern rt_struct_t *testrt_source;

void *testrt_get_target(void);
int testrt_run(bytestream_t *, dsource_t *);


#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
