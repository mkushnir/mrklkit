#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#include <mrkcommon/fasthash.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _bytes {
    size_t sz;
    uint64_t hash;
    unsigned char data[];
} bytes_t;


bytes_t *bytes_new(size_t);
void bytes_fini(void *);
uint64_t bytes_hash(bytes_t *);
int bytes_cmp(bytes_t *, bytes_t *);
char *newvar(char *, size_t, const char *);
#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
