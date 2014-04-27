#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#include <mrkcommon/fasthash.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _bytes {
    ssize_t nref;
    size_t sz;
    uint64_t hash;
    unsigned char data[];
} bytes_t;

#define BYTES_INCREF(b) (++(b)->nref)
#define BYTES_DECREF(pb) \
do { \
    if (*(pb) != NULL) { \
        --(*(pb))->nref; \
        if ((*(pb))->nref <= 0) { \
            free(*(pb)); \
        } \
        *(pb) = NULL; \
    } \
} while (0)


bytes_t *bytes_new(size_t);
void bytes_destroy(bytes_t **);
#define bytes_decref bytes_destroy
uint64_t bytes_hash(bytes_t *);
int bytes_cmp(bytes_t *, bytes_t *);
char *newvar(char *, size_t, const char *);
#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
