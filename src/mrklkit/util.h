#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#include <mrkcommon/fasthash.h>
#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t bytes_hash(bytes_t *);
char *newvar(char *, size_t, const char *);
#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
