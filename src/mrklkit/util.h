#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void reset_newvar_counter(void);
char *newvar(char *, size_t, const char *);

#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
