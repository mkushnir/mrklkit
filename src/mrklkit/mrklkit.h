#ifndef MRKLKIT_H_DEFINED
#define MRKLKIT_H_DEFINED

#include <stdint.h> /* uint64_t */

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/hash.h>

#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _mrklkit_modaux {
    LLVMContextRef lctx;
    LLVMMemoryBufferRef buf;
    LLVMModuleRef module;
    LLVMExecutionEngineRef ee;
} mrklkit_modaux_t;

typedef struct _mrklkit_backend {
    /* weakrefs */
    LLVMTypeRef ty;
    LLVMTypeRef deref;
} mrklkit_backend_t;

typedef enum _lkit_mpolicy {
    LKIT_MPUNDEF = 0,
    LKIT_MPMPOOL = 1,
    LKIT_MPMALLOC = 2,
} lkit_mpolicy_t;
#define MRKLKIT_DEFAULT_MPOLICY LKIT_MPMPOOL


typedef struct _mrklkit_ctx {
    /*
     * lkit_type_t *, lkit_type_t *
     */
    hash_t types;
    array_t builtin_types;
    /*
     * bytes_t *, lkit_type_t *
     */
    hash_t typedefs;

    array_t modules;

    /*
     * program
     */
    /* weakref lkit_type_t*, strongref mrklkit_backend_t* */
    hash_t backends;
    LLVMContextRef lctx;
    LLVMModuleRef module;
    /* mrklkit_modaux_t */
    array_t modaux;
    LLVMExecutionEngineRef ee;

    lkit_mpolicy_t dparse_mpolicy;
    int mark_referenced:1;
} mrklkit_ctx_t;

extern const char *mrklkit_meta;
const char *mrklkit_diag_str(int);

int mrklkit_parse(mrklkit_ctx_t *, int fd, void *, fparser_datum_t **);
#define MRKLKIT_COMPILE_DUMP0 0x01
#define MRKLKIT_COMPILE_DUMP1 0x02
#define MRKLKIT_COMPILE_DUMP2 0x04
int mrklkit_compile(mrklkit_ctx_t *, int, uint64_t, void *);
int mrklkit_compile_incomplete(mrklkit_ctx_t *, int, uint64_t, void *);
void mrklkit_ctx_setup_runtime(mrklkit_ctx_t *, void *);
void mrklkit_ctx_cleanup_runtime(mrklkit_ctx_t *, void *);
void mrklkit_ctx_cleanup_runtime_dirty(mrklkit_ctx_t *, void *);
int mrklkit_call_void(mrklkit_ctx_t *, const char *);
struct _mrklkit_module;
void mrklkit_ctx_init(mrklkit_ctx_t *,
                      const char *,
                      void *,
                      struct _mrklkit_module *[],
                      size_t);
void mrklkit_ctx_fini(mrklkit_ctx_t *);
void mrklkit_init(void);
void mrklkit_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_H_DEFINED */
