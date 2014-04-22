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
     * LKIT_STR     bytes_t
     * LKIT_ARRAY   array_t
     * LKIT_STRUCT  array_t of tobj_t
     * LKIT_DICT    dict_t
     */
    void *value;
} tobj_t;

#define LRUNTIME_V2O(v) ((v) - sizeof(lkit_type_t *))

void mrklkit_rt_gc(void);

//tobj_t *mrklkit_rt_dict_new(lkit_dict_t *);
void mrklkit_rt_dict_init(dict_t *, lkit_type_t *);
void mrklkit_rt_dict_dtor(dict_t **);

//tobj_t *mrklkit_rt_array_new(lkit_array_t *);
void mrklkit_rt_array_init(array_t *, lkit_type_t *);
void mrklkit_rt_array_dtor(array_t **);

//tobj_t *mrklkit_rt_struct_new(lkit_struct_t *);
void mrklkit_rt_struct_init(array_t *);
void mrklkit_rt_struct_dtor(array_t **);

//char *mrklkit_rt_str_new(size_t);
//void mrklkit_rt_str_cat(char *, size_t, char *, size_t);
void mrklkit_rt_str_init(array_t *);
void mrklkit_rt_str_fini(void *);

void lruntime_init(void);
void lruntime_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LRUNTIME_H_DEFINED */
