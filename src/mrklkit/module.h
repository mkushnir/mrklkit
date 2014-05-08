#ifndef MODULE_H_DEFINED
#define MODULE_H_DEFINED

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mrklkit_module_parser_t)(void *, array_t *, array_iter_t *);
typedef struct _mrklkit_parser_info {
    const char *keyword;
    mrklkit_module_parser_t parser;
} mrklkit_parser_info_t;

typedef void (*mrklkit_module_initializer_t)(void *);
typedef void (*mrklkit_module_finalizer_t)(void *);
typedef int (*mrklkit_module_precompiler_t)(void *);
typedef int (*mrklkit_module_compiler_t)(void *, LLVMModuleRef);
typedef int (*mrklkit_module_linker_t)(void *, LLVMExecutionEngineRef, LLVMModuleRef);
typedef struct _mrklkit_module {
    mrklkit_module_initializer_t init;
    mrklkit_module_finalizer_t fini;
    mrklkit_parser_info_t *parsers;
    mrklkit_module_precompiler_t precompile;
    mrklkit_module_compiler_t compile;
    mrklkit_module_linker_t link;
} mrklkit_module_t;


#ifdef __cplusplus
}
#endif
#endif /* MODULE_H_DEFINED */

