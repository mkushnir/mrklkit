#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/bytestream.h>

#include <mrklkit/module.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dsource.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _testrt_target {
    /* meta */
    uint64_t id;
    /* weak ref */
    lkit_struct_t *type;
    uint64_t hash;
    rt_struct_t *value;
} testrt_target_t;


typedef struct _testrt {
    bytes_t *dsource;
    /* quals */
    uint64_t id;
    bytes_t *name;
    /*
     * doexpr and takeexpr are not actually expressions. we use
     * lkit_expr_t here to conveniently store some sequence of operations.
     */
    lkit_expr_t *doexpr;
    lkit_expr_t *takeexpr;
    lkit_expr_t *seeexpr;
    /* weak refs */
    array_t otherexpr;
    testrt_target_t key;
} testrt_t;

#define TESTRT_START "testrt.start"
extern mrklkit_module_t testrt_module;
extern rt_struct_t *testrt_source;

void *testrt_acquire_take_key(testrt_t *);
void *testrt_get_do(testrt_t *);
int testrt_run(bytestream_t *, byterange_t *, dsource_t *);
void testrt_dump_targets(void);
void testrt_dump_source(rt_struct_t *);


#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
