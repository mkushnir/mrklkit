#ifndef LRUNTIME_H_DEFINED
#define LRUNTIME_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* typed object */
typedef struct _tobj {
    /* weak ref */
    lkit_type_t *type;
    /*
     * LKIT_INT     int64_t
     * LKIT_BOOL    char
     * LKIT_FLOAT   double
     * LKIT_STR     bytes_t *
     * LKIT_ARRAY   array_t *
     * LKIT_STRUCT  rt_struct_t * of tobj_t
     * LKIT_DICT    dict_t *
     */
    void *value;
} tobj_t;

typedef struct _rt_struct {
    ssize_t current;
    /* array of tobj_t */
    array_t fields;
} rt_struct_t;

#define LRUNTIME_V2O(v) ((v) - sizeof(lkit_type_t *))

void mrklkit_rt_gc(void);

//tobj_t *mrklkit_rt_dict_new(lkit_dict_t *);
void mrklkit_rt_dict_init(dict_t *, lkit_type_t *);
void mrklkit_rt_dict_dtor(dict_t **);

//tobj_t *mrklkit_rt_array_new(lkit_array_t *);
void mrklkit_rt_array_init(array_t *, lkit_type_t *);
void mrklkit_rt_array_dtor(array_t **);

rt_struct_t *mrklkit_rt_struct_new(lkit_struct_t *);
void mrklkit_rt_struct_init(rt_struct_t *, lkit_struct_t *);
int mrklkit_rt_struct_fini(rt_struct_t *);
void mrklkit_rt_struct_dtor(rt_struct_t **);

//char *mrklkit_rt_str_new(size_t);
//void mrklkit_rt_str_cat(char *, size_t, char *, size_t);
void mrklkit_rt_str_init(array_t *);
void mrklkit_rt_str_fini(void *);

void *mrklkit_rt_get_array_item(array_t *, int64_t, void *);
void *mrklkit_rt_get_dict_item(dict_t *, bytes_t *, void *);
void *mrklkit_rt_get_struct_item(rt_struct_t *, int64_t, void *);

int tobj_dump(tobj_t *, void *);

void lruntime_init(void);
void lruntime_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LRUNTIME_H_DEFINED */
