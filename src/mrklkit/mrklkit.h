#ifndef MRKLKIT_H_DEFINED
#define MRKLKIT_H_DEFINED

#include <stdint.h> /* uint64_t */

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _mrklkit_ctx {
    array_t *modules;
    fparser_datum_t *datum_root;
    lkit_expr_t builtin_root;
    /* backend */
    LLVMContextRef lctx;
    LLVMModuleRef module;
    LLVMExecutionEngineRef ee;
} mrklkit_ctx_t;

const char *mrklkit_diag_str(int);

typedef int (*mrklkit_parser_t)(fparser_datum_t *, array_iter_t *, void *);
int mrklkit_register_parser(const char *, mrklkit_parser_t, void *);

#define MRKLKIT_COMPILE_DUMP0 0x01
#define MRKLKIT_COMPILE_DUMP1 0x02
#define MRKLKIT_COMPILE_DUMP2 0x04
int mrklkit_compile(mrklkit_ctx_t *, int, uint64_t, void *);
int mrklkit_init_runtime(mrklkit_ctx_t *, void *);
int mrklkit_call_void(mrklkit_ctx_t *, const char *);
void mrklkit_ctx_init(mrklkit_ctx_t *, const char *, array_t *, void *);
void mrklkit_ctx_fini(mrklkit_ctx_t *, void *);
void mrklkit_init(void);
void mrklkit_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_H_DEFINED */
