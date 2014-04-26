#ifndef MODULE_H_DEFINED
#define MODULE_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _mrklkit_parser_info {
    const char *keyword;
    int (*parser)(array_t *, array_iter_t *);
} mrklkit_parser_info_t;

typedef struct _mrklkit_module {
    void (*init)(void);
    void (*fini)(void);
    mrklkit_parser_info_t *parsers;
    int (*precompile)(void);
    int (*compile)(LLVMModuleRef);
} mrklkit_module_t;


#ifdef __cplusplus
}
#endif
#endif /* MODULE_H_DEFINED */

