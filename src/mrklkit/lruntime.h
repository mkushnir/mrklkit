#ifndef LRUNTIME_H_DEFINED
#define LRUNTIME_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dict.h>

#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rt_array {
    ssize_t nref;
    lkit_array_t *type;
    array_t fields;
} rt_array_t;

#define ARRAY_INCREF(ar) (++(ar)->nref)

#define ARRAY_DECREF(ar) \
do { \
    if (*(ar) != NULL) { \
        --(*(ar))->nref; \
        if ((*(ar))->nref <= 0) { \
            array_fini(&(*(ar))->fields); \
            free(*(ar)); \
        } \
        *(ar) = NULL; \
    } \
} while (0)

typedef struct _rt_dict {
    ssize_t nref;
    lkit_dict_t *type;
    dict_t fields;
} rt_dict_t;

#define DICT_INCREF(dc) (++(dc)->nref)

#define DICT_DECREF(dc) \
do { \
    if (*(dc) != NULL) { \
        --(*(dc))->nref; \
        if ((*(dc))->nref <= 0) { \
            dict_fini(&(*(dc))->fields); \
            free(*(dc)); \
        } \
        *(dc) = NULL; \
    } \
} while (0)

typedef struct _rt_struct {
    ssize_t nref;
    lkit_struct_t *type;
    /* weak ref */
    struct {
        bytestream_t *bs;
        byterange_t br;
        off_t pos;
    } parser_info;
    int next_delim;
    int current;
    /* delimiter positions */
    off_t *dpos;
    /* array of void * + array of int  */
    void *fields[];
} rt_struct_t;

#define STRUCT_INCREF(st) \
do { \
    /* TRACE(">>> %ld %p", (st)->nref, (st)); */ \
    (++(st)->nref); \
} while (0)

#define STRUCT_DECREF(st) \
do { \
    if (*(st) != NULL) { \
        --(*(st))->nref; \
        /* TRACE("<<< %ld %p", (*(st))->nref, *(st)); */ \
        if ((*(st))->nref <= 0) { \
            if ((*(st))->type->fini != NULL) { \
                (*(st))->type->fini((*(st))->fields); \
            } \
            free(*(st)); \
        } \
        *(st) = NULL; \
    } \
} while (0)

rt_array_t *mrklkit_rt_array_new(lkit_array_t *);
void mrklkit_rt_array_destroy(rt_array_t **);
void mrklkit_rt_array_dump(rt_array_t *);
int64_t mrklkit_rt_get_array_item_int(rt_array_t *, int64_t, int64_t);
double mrklkit_rt_get_array_item_float(rt_array_t *, int64_t, double);
bytes_t *mrklkit_rt_get_array_item_str(rt_array_t *, int64_t, bytes_t *);

rt_dict_t *mrklkit_rt_dict_new(lkit_dict_t *);
void mrklkit_rt_dict_destroy(rt_dict_t **);
void mrklkit_rt_dict_dump(rt_dict_t *);
int64_t mrklkit_rt_get_dict_item_int(rt_dict_t *, bytes_t *, int64_t);
double mrklkit_rt_get_dict_item_float(rt_dict_t *, bytes_t *, double);
bytes_t *mrklkit_rt_get_dict_item_str(rt_dict_t *, bytes_t *, bytes_t *);

rt_struct_t *mrklkit_rt_struct_new(lkit_struct_t *);
void mrklkit_rt_struct_destroy(rt_struct_t **);
void mrklkit_rt_struct_dump(rt_struct_t *);
void **mrklkit_rt_get_struct_item_addr(rt_struct_t *, int64_t);
#define MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(val, idx) ((val)->fields + (idx))
int64_t mrklkit_rt_get_struct_item_int(rt_struct_t *, int64_t);
double mrklkit_rt_get_struct_item_float(rt_struct_t *, int64_t);
int64_t mrklkit_rt_get_struct_item_bool(rt_struct_t *, int64_t);
bytes_t *mrklkit_rt_get_struct_item_str(rt_struct_t *, int64_t);
rt_array_t *mrklkit_rt_get_struct_item_array(rt_struct_t *, int64_t);
rt_dict_t *mrklkit_rt_get_struct_item_dict(rt_struct_t *, int64_t);
rt_struct_t *mrklkit_rt_get_struct_item_struct(rt_struct_t *, int64_t);
void mrklkit_rt_set_struct_item_int(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_set_struct_item_float(rt_struct_t *, int64_t, double);
void mrklkit_rt_set_struct_item_bool(rt_struct_t *, int64_t, int64_t);
void mrklkit_rt_set_struct_item_str(rt_struct_t *, int64_t, bytes_t *);

void mrklkit_rt_struct_shallow_copy(rt_struct_t *, rt_struct_t *);

void lruntime_init(void);
void lruntime_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LRUNTIME_H_DEFINED */
