#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/fasthash.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XXX check out compile_bytes_t(), builtin_compile_expr(), and ltype_compile()
 */
typedef struct _bytes {
    ssize_t nref;
    size_t sz;
    uint64_t hash;
#define BYTES_DATA_IDX 3
    unsigned char data[];
} bytes_t;

#define BYTES_INCREF(b) \
do { \
    (++(b)->nref); \
    /* TRACE(">>> %p nref=%ld sz=%ld data=%p", (b), (b)->nref, (b)->sz, (b)->data); */ \
} while (0)

#define BYTES_DECREF(pb) \
do { \
    if (*(pb) != NULL) { \
        --(*(pb))->nref; \
        /* TRACE("<<< %p nref=%ld sz=%ld data=%s", *(pb), (*(pb))->nref, (*(pb))->sz, (*(pb))->data); */ \
        if ((*(pb))->nref <= 0) { \
            free(*(pb)); \
        } \
        *(pb) = NULL; \
    } \
} while (0)


bytes_t *bytes_new(size_t);
bytes_t *bytes_new_from_str(const char *);
void mrklkit_bytes_incref(bytes_t *);
void mrklkit_bytes_destroy(bytes_t **);
#define bytes_decref mrklkit_bytes_destroy
uint64_t bytes_hash(bytes_t *);
int bytes_cmp(bytes_t *, bytes_t *);
char *newvar(char *, size_t, const char *);
#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
