#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>
#include <stddef.h>

#include <mrkcommon/dumpm.h>

#ifdef __cplusplus
extern "C" {
#endif

void reset_newvar_counter(void);
char *newvar(char *, size_t, const char *);

#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#define FNULL(m) m
#define ERRCOLOR FRED
#define SIZEOFCOLOR(c) (sizeof(c("")) - 1)

#define MRKLKIT_INDEXOF(ty, field) (offsetof(ty, field) / sizeof(uint64_t))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
