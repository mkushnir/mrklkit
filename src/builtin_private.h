#ifndef BUILTIN_PRIVATE_H_DEFINED
#define BUILTIN_PRIVATE_H_DEFINED

#include <mrklkit/lexpr.h>

#ifdef __cplusplus
extern "C" {
#endif

int builtingen_call_eager_initializer(lkit_gitem_t **, void *);
#ifdef __cplusplus
}
#endif

#include <mrklkit/builtin.h>

#endif /* BUILTIN_PRIVATE_H_DEFINED */
