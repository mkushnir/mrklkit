#ifndef LRUNTIME_H_DEFINED
#define LRUNTIME_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/hash.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/jparse.h>

#include <mrklkit/ltype.h>

#ifdef DO_MEMDEBUG
#define MEMDEBUG_ENTER_LRT(self)                               \
{                                                              \
    int mdtag;                                                 \
    mdtag = memdebug_set_runtime_scope((int)(self)->mdtag);    \


#define MEMDEBUG_LEAVE_LRT(self)               \
    (void)memdebug_set_runtime_scope(mdtag);   \
}                                              \


#else
#define MEMDEBUG_ENTER_LRT(self)
#define MEMDEBUG_LEAVE_LRT(self)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rt_array {
#ifdef DO_MEMDEBUG
    uint64_t mdtag;
#endif
    ssize_t nref;
    lkit_array_t *type;
    array_t fields;
} rt_array_t;

#define ARRAY_INCREF(ar) (++(ar)->nref)

#define ARRAY_DECREF(ar)                       \
do {                                           \
    if (*(ar) != NULL) {                       \
        --(*(ar))->nref;                       \
        if ((*(ar))->nref <= 0) {              \
            MEMDEBUG_ENTER_LRT(*(ar));         \
            array_fini(&(*(ar))->fields);      \
            free(*(ar));                       \
            MEMDEBUG_LEAVE_LRT(*(ar));         \
        }                                      \
        *(ar) = NULL;                          \
    }                                          \
} while (0)

#define ARRAY_DECREF_FAST(ar)          \
do {                                   \
    --(ar)->nref;                      \
    if ((ar)->nref <= 0) {             \
        MEMDEBUG_ENTER_LRT(ar);        \
        array_fini(&(ar)->fields);     \
        free(ar);                      \
        MEMDEBUG_LEAVE_LRT(ar);        \
    }                                  \
} while (0)

typedef struct _rt_dict {
#ifdef DO_MEMDEBUG
    uint64_t mdtag;
#endif
    ssize_t nref;
    lkit_dict_t *type;
    hash_t fields;
} rt_dict_t;

#define DICT_INCREF(dc) (++(dc)->nref)

#define DICT_DECREF(dc)                        \
do {                                           \
    if (*(dc) != NULL) {                       \
        --(*(dc))->nref;                       \
        if ((*(dc))->nref <= 0) {              \
            MEMDEBUG_ENTER_LRT(*(dc));         \
            hash_fini(&(*(dc))->fields);       \
            free(*(dc));                       \
            MEMDEBUG_LEAVE_LRT(*(dc));         \
        }                                      \
        *(dc) = NULL;                          \
    }                                          \
} while (0)

#define DICT_DECREF_FAST(dc)           \
do {                                   \
    --(dc)->nref;                      \
    if ((dc)->nref <= 0) {             \
        MEMDEBUG_ENTER_LRT(dc);        \
        hash_fini(&(dc)->fields);      \
        free(dc);                      \
        MEMDEBUG_LEAVE_LRT(dc);        \
    }                                  \
} while (0)

typedef struct _rt_struct {
#ifdef DO_MEMDEBUG
    uint64_t mdtag;
#endif
    ssize_t nref;
    lkit_struct_t *type;
    /* array of void * */
    void *fields[];
} rt_struct_t;


#define STRUCT_INCREF(st)                              \
do {                                                   \
    /* TRACE(">>> %ld %p", (st)->nref, (st)); */       \
    (++(st)->nref);                                    \
} while (0)

#define STRUCT_DECREF_NO_DESTRUCT(st)  \
do {                                   \
    if (*(st) != NULL) {               \
        --(*(st))->nref;               \
        if ((*(st))->nref <= 0) {      \
            MEMDEBUG_ENTER_LRT(*(st)); \
            free(*(st));               \
            MEMDEBUG_LEAVE_LRT(*(st)); \
        }                              \
        *(st) = NULL;                  \
    }                                  \
} while (0)

#define STRUCT_DECREF(st)                              \
do {                                                   \
    if (*(st) != NULL) {                               \
        --(*(st))->nref;                               \
        if ((*(st))->nref <= 0) {                      \
            MEMDEBUG_ENTER_LRT(*(st));                 \
            if ((*(st))->type->fini != NULL) {         \
                (*(st))->type->fini((*(st))->fields);  \
            }                                          \
            free(*(st));                               \
            MEMDEBUG_LEAVE_LRT(*(st));                 \
        }                                              \
        *(st) = NULL;                                  \
    }                                                  \
} while (0)

#define STRUCT_DECREF_FAST(st)                 \
do {                                           \
    --(st)->nref;                              \
    if ((st)->nref <= 0) {                     \
        MEMDEBUG_ENTER_LRT(st);                \
        if ((st)->type->fini != NULL) {        \
            (st)->type->fini((st)->fields);    \
        }                                      \
        free(st);                              \
        MEMDEBUG_LEAVE_LRT(st);                \
    }                                          \
} while (0)

bytes_t *mrklkit_rt_bytes_new(size_t);
bytes_t *mrklkit_rt_bytes_new_mpool(size_t);
bytes_t *mrklkit_rt_bytes_new_from_str(const char *);
bytes_t *mrklkit_rt_bytes_new_from_str_mpool(const char *);
bytes_t *mrklkit_rt_bytes_new_from_int(int64_t);
bytes_t *mrklkit_rt_bytes_new_from_int_mpool(int64_t);
bytes_t *mrklkit_rt_bytes_new_from_float(double);
bytes_t *mrklkit_rt_bytes_new_from_float_mpool(double);
bytes_t *mrklkit_rt_bytes_new_from_bool(char);
bytes_t *mrklkit_rt_bytes_new_from_bool_mpool(char);

rt_array_t *mrklkit_rt_array_new(lkit_array_t *);
rt_array_t *mrklkit_rt_array_new_mpool(lkit_array_t *);
rt_array_t *mrklkit_rt_array_new_mpool_sz(lkit_array_t *, int);
void mrklkit_rt_array_decref(rt_array_t **);
void mrklkit_rt_array_dump(rt_array_t *);
void mrklkit_rt_array_print(rt_array_t *);
int64_t mrklkit_rt_array_get_item_int(rt_array_t *, int64_t, int64_t);
double mrklkit_rt_array_get_item_float(rt_array_t *, int64_t, double);
bytes_t *mrklkit_rt_array_get_item_str(rt_array_t *, int64_t, bytes_t *);
int64_t mrklkit_rt_array_len(rt_array_t *);

rt_dict_t *mrklkit_rt_dict_new(lkit_dict_t *);
rt_dict_t *mrklkit_rt_dict_new_sz(lkit_dict_t *, int);
rt_dict_t *mrklkit_rt_dict_new_mpool(lkit_dict_t *);
rt_dict_t *mrklkit_rt_dict_new_mpool_sz(lkit_dict_t *, int);
void mrklkit_rt_dict_decref(rt_dict_t **);
void mrklkit_rt_dict_dump(rt_dict_t *);
void mrklkit_rt_dict_print(rt_dict_t *);
int64_t mrklkit_rt_dict_get_item_int(rt_dict_t *, bytes_t *, int64_t);
double mrklkit_rt_dict_get_item_float(rt_dict_t *, bytes_t *, double);
bytes_t *mrklkit_rt_dict_get_item_str(rt_dict_t *, bytes_t *, bytes_t *);
rt_struct_t *mrklkit_rt_dict_get_item_struct(rt_dict_t *, bytes_t *, rt_struct_t *);

rt_struct_t *mrklkit_rt_struct_new(lkit_struct_t *);
rt_struct_t *mrklkit_rt_struct_new_mpool(lkit_struct_t *);
void mrklkit_rt_struct_decref(rt_struct_t **);
void mrklkit_rt_struct_decref_no_destruct(rt_struct_t **);
int mrklkit_rt_struct_init(rt_struct_t *);
int mrklkit_rt_struct_fini(rt_struct_t *);
void mrklkit_rt_struct_dump(rt_struct_t *);
void mrklkit_rt_struct_print(rt_struct_t *);

void **mrklkit_rt_struct_get_item_addr(rt_struct_t *,
                                       int64_t);
#define MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(val, idx) ((val)->fields + (idx))
int64_t mrklkit_rt_struct_get_item_int(rt_struct_t *,
                                       int64_t,
                                       int64_t);
double mrklkit_rt_struct_get_item_float(rt_struct_t *,
                                        int64_t,
                                        double);
int8_t mrklkit_rt_struct_get_item_bool(rt_struct_t *,
                                       int64_t,
                                       int8_t);
bytes_t *mrklkit_rt_struct_get_item_str(rt_struct_t *,
                                        int64_t,
                                        bytes_t *);
rt_array_t *mrklkit_rt_struct_get_item_array(rt_struct_t *,
                                             int64_t,
                                             rt_array_t *);
rt_dict_t *mrklkit_rt_struct_get_item_dict(rt_struct_t *,
                                           int64_t,
                                           rt_dict_t *);
rt_struct_t *mrklkit_rt_struct_get_item_struct(rt_struct_t *,
                                               int64_t,
                                               rt_struct_t *);
void mrklkit_rt_struct_set_item_int(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_struct_set_item_int_mpool(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_struct_set_item_float(rt_struct_t *, int64_t, double);
void mrklkit_rt_struct_set_item_float_mpool(rt_struct_t *, int64_t, double);
void mrklkit_rt_struct_set_item_bool(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_struct_set_item_bool_mpool(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_struct_set_item_str(rt_struct_t *, int64_t, bytes_t *);
void mrklkit_rt_struct_set_item_str_mpool(rt_struct_t *, int64_t, bytes_t *);

void mrklkit_rt_struct_shallow_copy(rt_struct_t *, rt_struct_t *);
void mrklkit_rt_struct_deep_copy(rt_struct_t *, rt_struct_t *);
void mrklkit_rt_struct_deep_copy_mpool(rt_struct_t *, rt_struct_t *);

void rt_array_dump_json(rt_array_t *, bytestream_t *);
void rt_dict_dump_json(rt_dict_t *, bytestream_t *);
void rt_struct_dump_json(rt_struct_t *, bytestream_t *);
int rt_array_load_json(rt_array_t *, jparse_ctx_t *);
int rt_dict_load_json(rt_dict_t *, jparse_ctx_t *);
int rt_struct_load_json(rt_struct_t *, jparse_ctx_t *);
int rt_struct_load_fields_json(rt_struct_t *, jparse_ctx_t *);

void lruntime_set_mpool(mpool_ctx_t *);
void lruntime_init(void);
void lruntime_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LRUNTIME_H_DEFINED */
