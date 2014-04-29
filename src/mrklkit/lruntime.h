#ifndef LRUNTIME_H_DEFINED
#define LRUNTIME_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rt_struct {
    ssize_t current;
    void (*init)(void **);
    void (*fini)(void **);
    /* array of void *  */
    array_t fields;
} rt_struct_t;

void mrklkit_rt_dict_init(dict_t *, lkit_type_t *);
void mrklkit_rt_dict_destroy(dict_t **);
int64_t mrklkit_rt_get_dict_item_int(dict_t *, bytes_t *, int64_t);
double mrklkit_rt_get_dict_item_float(dict_t *, bytes_t *, double);
bytes_t *mrklkit_rt_get_dict_item_str(dict_t *, bytes_t *, bytes_t *);

void mrklkit_rt_array_init(array_t *, lkit_type_t *);
void mrklkit_rt_array_destroy(array_t **);
int64_t mrklkit_rt_get_array_item_int(array_t *, int64_t, int64_t);
double mrklkit_rt_get_array_item_float(array_t *, int64_t, double);
bytes_t *mrklkit_rt_get_array_item_str(array_t *, int64_t, bytes_t *);

rt_struct_t *mrklkit_rt_struct_new(lkit_struct_t *);
void mrklkit_rt_struct_init(rt_struct_t *, lkit_struct_t *);
int mrklkit_rt_struct_fini(rt_struct_t *);
void mrklkit_rt_struct_destroy(rt_struct_t **);
int64_t mrklkit_rt_get_struct_item_int(rt_struct_t *, int64_t, int64_t);
double mrklkit_rt_get_struct_item_float(rt_struct_t *, int64_t, double);
int64_t mrklkit_rt_get_struct_item_bool(rt_struct_t *, int64_t, int64_t);
bytes_t *mrklkit_rt_get_struct_item_str(rt_struct_t *, int64_t, bytes_t *);
array_t *mrklkit_rt_get_struct_item_array(rt_struct_t *, int64_t, array_t *);
dict_t *mrklkit_rt_get_struct_item_dict(rt_struct_t *, int64_t, dict_t *);
rt_struct_t *mrklkit_rt_get_struct_item_struct(rt_struct_t *, int64_t, rt_struct_t *);
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
