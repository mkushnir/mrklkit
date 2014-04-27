#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/module.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dsource.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _testrt {
    bytes_t *dsource;
    uint64_t id;
    bytes_t *name;
    /*
     * doexpr and takeexpr are not actually expressions. we use
     * lkit_expr_t here to conveniently store some sequence of operations.
     */
    lkit_expr_t *doexpr;
    lkit_expr_t *takeexpr;
    lkit_expr_t *seeexpr;
    /*
     * concatenation of doexpr->type and takeexpr->type
     */
    lkit_struct_t *type;
} testrt_t;

typedef struct _testrt_target {
    /* meta */
    uint64_t id;
    /* weak ref */
    lkit_struct_t *kty;
    /* weak ref */
    lkit_struct_t *vty;
    uint64_t hash;
    /* can point at data, or any other location */
    void **value;
    /* optional location of data, a sequence of:
     *  LKIT_INT     int64_t
     *  LKIT_BOOL    char
     *  LKIT_FLOAT   double
     *  LKIT_STR     bytes_t *
     */
    char data[];
} testrt_target_t;

#define TESTRT_START "testrt.start"
extern mrklkit_module_t testrt_module;
extern rt_struct_t *testrt_source;

void *testrt_get_target(testrt_t *, void *);
int testrt_run(bytestream_t *, dsource_t *);


#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
