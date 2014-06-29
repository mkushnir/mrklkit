#ifndef MRKLKIT_UTILS_H_DEFINED
#define MRKLKIT_UTILS_H_DEFINED

#include <sys/types.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/fasthash.h>
#include <mrkcommon/mpool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XXX check out compile_bytes_t(), lkit_compile_expr(), and ltype_compile()
 */
typedef struct _bytes {
    ssize_t nref;
#define BYTES_SZ_IDX 1
    size_t sz;
    uint64_t hash;
#define BYTES_DATA_IDX 3
    unsigned char data[];
} bytes_t;

#define BYTES_INCREF(b) \
do { \
    (++(b)->nref); \
    /* TRACE("B>>> %p %ld", (b), (b)->nref); */ \
} while (0)

#define BYTES_DECREF(pb) \
do { \
    if (*(pb) != NULL) { \
        --(*(pb))->nref; \
        if ((*(pb))->nref <= 0) { \
            /* TRACE("B<<< %p %ld '%s'", *(pb), (*(pb))->nref, (*(pb))->data); */ \
            free(*(pb)); \
        } \
        *(pb) = NULL; \
    } \
} while (0)

#define BYTES_DECREF_FAST(b) \
do { \
    --((b))->nref; \
    if (((b))->nref <= 0) { \
        /* TRACE("B<<< %p %ld '%s'", b, (b)->nref, (b)->data); */ \
        free((b)); \
    } \
} while (0)


bytes_t *mrklkit_bytes_new(size_t);
bytes_t *mrklkit_bytes_new_mpool(mpool_ctx_t *, size_t);
#define bytes_new mrklkit_bytes_new
bytes_t *mrklkit_bytes_new_from_str(const char *);
bytes_t *mrklkit_bytes_new_from_str_mpool(mpool_ctx_t *, const char *);
#define bytes_new_from_str mrklkit_bytes_new_from_str
#define bytes_new_from_str_mpool mrklkit_bytes_new_from_str_mpool
void mrklkit_bytes_incref(bytes_t *);
#define bytes_incref mrklkit_bytes_incref
void mrklkit_bytes_decref(bytes_t **);
#define bytes_decref mrklkit_bytes_decref
void mrklkit_bytes_decref_fast(bytes_t *);
uint64_t mrklkit_bytes_hash(bytes_t *);
#define bytes_hash mrklkit_bytes_hash
int mrklkit_bytes_cmp(bytes_t *, bytes_t *);
#define bytes_cmp mrklkit_bytes_cmp
void mrklkit_bytes_copy(bytes_t *, bytes_t *, size_t);
#define bytes_copy mrklkit_bytes_copy
void mrklkit_bytes_brushdown(bytes_t *);
#define bytes_brushdown mrklkit_bytes_brushdown

bytes_t *mrklkit_bytes_json_escape(bytes_t *);

void reset_newvar_counter(void);
char *newvar(char *, size_t, const char *);

#define NEWVAR(prefix) newvar(NULL, 0, (prefix))

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_UTILS_H_DEFINED */
