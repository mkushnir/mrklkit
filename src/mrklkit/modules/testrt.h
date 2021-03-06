#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/module.h>
#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dsource {
    int timestamp_index;
    int date_index;
    int time_index;
    int duration_index;
    int error:1;
    /* weak ref*/
    mnbytes_t *kind;
    lkit_struct_t *_struct;
    char rdelim[2];
    char fdelim;
} dsource_t;

typedef struct _testrt_url {
} testrt_url;

typedef struct _testrt_target {
    /* meta */
    uint64_t id;
    /* weak ref */
    lkit_struct_t *type;
    uint64_t hash;
    rt_struct_t *value;
} testrt_target_t;


typedef struct _testrt {
    mnbytes_t *dsource;
    /* quals */
    uint64_t id;
    mnbytes_t *name;
    /*
     * doexpr and takeexpr are not actually expressions. we use
     * lkit_expr_t here to conveniently store some sequence of operations.
     */
    lkit_expr_t *doexpr;
    lkit_expr_t *takeexpr;
    lkit_expr_t *seeexpr;
    /* weak refs */
    mnarray_t otherexpr;
    testrt_target_t key;
} testrt_t;

typedef struct _testrt_ctx {
    mrklkit_ctx_t mctx;
    fparser_datum_t *datum_root;
    lkit_cexpr_t builtin;
    lkit_cexpr_t root;
    mnarray_t testrts;
    dsource_t *ds;
} testrt_ctx_t;

#define TESTRT_START "testrt.start"
extern mrklkit_module_t testrt_module;
extern rt_struct_t *testrt_source;

dsource_t *dsource_get(const char *);
void *testrt_acquire_take_key(testrt_t *);
void *testrt_get_do(testrt_t *);
int testrt_run_once(mnbytestream_t *, const byterange_t *, testrt_ctx_t *);
void testrt_dump_targets(void);
void testrt_dump_source(rt_struct_t *);


#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
