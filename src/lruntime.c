#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

//UNUSED static array_t objects;


/**
 * array
 */
rt_array_t *
mrklkit_rt_array_new(lkit_array_t *ty)
{
    rt_array_t *res;

    if ((res = malloc(sizeof(rt_array_t))) == NULL) {
        FAIL("malloc");
    }
    res->nref = 0;
    res->type = ty;
    array_init(&res->fields, sizeof(void *), 0, NULL, ty->fini);
    return res;
}


void
mrklkit_rt_array_destroy(rt_array_t **value)
{
    ARRAY_DECREF(value);
}


int64_t
mrklkit_rt_get_array_item_int(rt_array_t *value, int64_t idx, int64_t dflt)
{
    int64_t *res;

    if ((res = array_get(&value->fields,
                         idx % value->fields.elnum)) == NULL) {
        return dflt;
    }
    return *res;
}


double
mrklkit_rt_get_array_item_float(rt_array_t *value, int64_t idx, double dflt)
{
    union {
        void **v;
        double *d;
    } res;

    assert(sizeof(void *) == sizeof(double));

    if ((res.v = array_get(&value->fields,
                           idx % value->fields.elnum)) == NULL) {
        return dflt;
    }
    return *res.d;
}


bytes_t *
mrklkit_rt_get_array_item_str(rt_array_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if ((res = array_get(&value->fields,
                         idx % value->fields.elnum)) == NULL) {
        return dflt;
    }
    return *res;
}


/**
 * dict
 */
rt_dict_t *
mrklkit_rt_dict_new(lkit_dict_t *ty)
{
    rt_dict_t *res;

    if ((res = malloc(sizeof(rt_dict_t))) == NULL) {
        FAIL("malloc");
    }

    res->nref = 0;
    res->type = ty;
    dict_init(&res->fields,
              17,
              (dict_hashfn_t)bytes_hash,
              (dict_item_comparator_t)bytes_cmp,
              ty->fini);

    return res;
}


void
mrklkit_rt_dict_destroy(rt_dict_t **value)
{
    DICT_DECREF(value);
}


int64_t
mrklkit_rt_get_dict_item_int(rt_dict_t *value, bytes_t *key, int64_t dflt)
{
    union {
        void *v;
        int64_t i;
    } res;

    if ((res.v = dict_get_item(&value->fields, key)) == NULL) {
        res.i = dflt;
    }
    return res.i;
}


double
mrklkit_rt_get_dict_item_float(rt_dict_t *value, bytes_t *key, double dflt)
{
    union {
        void *v;
        double d;
    } res;

    if ((res.v = dict_get_item(&value->fields, key)) == NULL) {
        res.d = dflt;
    }
    return res.d;
}


bytes_t *
mrklkit_rt_get_dict_item_str(rt_dict_t *value, bytes_t *key, bytes_t *dflt)
{
    bytes_t *res;

    if ((res = dict_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    }
    return res;
}


/**
 * struct
 */
rt_struct_t *
mrklkit_rt_struct_new(lkit_struct_t *ty)
{
    rt_struct_t *v;

    size_t sz = ty->fields.elnum * sizeof(void *);

    if ((v = malloc(sizeof(rt_struct_t) + sz)) == NULL) {
        FAIL("malloc");
    }
    v->fnum = ty->fields.elnum;
    v->nref = 0;
    v->init = ty->init;
    v->fini = ty->fini;
    v->current = 0;
    if (v->init != NULL) {
        v->init(v->fields);
    }
    return v;
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    STRUCT_DECREF(value);
}


void
mrklkit_rt_struct_dump(rt_struct_t *value, lkit_struct_t *ty)
{
    lkit_type_t **fty;
    array_iter_t it;

    //TRACE("ty=%p", ty);
    for (fty = array_first(&ty->fields, &it);
         fty != NULL;
         fty = array_next(&ty->fields, &it)) {

        switch ((*fty)->tag) {
        case LKIT_INT:
            TRACEC("%ld ", mrklkit_rt_get_struct_item_int(value, it.iter));
            break;

        case LKIT_FLOAT:
            TRACEC("%lf ", mrklkit_rt_get_struct_item_float(value, it.iter));
            break;

        case LKIT_STR:
            TRACEC("%s ", mrklkit_rt_get_struct_item_str(value, it.iter)->data);
            break;

        default:
            FAIL("mrklkit_rt_struct_dump");
        }
    }
}


void **
mrklkit_rt_get_struct_item_addr(rt_struct_t *value, int64_t idx)
{
    assert(idx < value->fnum);
    return value->fields + idx;
}


int64_t
mrklkit_rt_get_struct_item_int(rt_struct_t *value, int64_t idx)
{
    assert(idx < value->fnum);
    return *(int64_t *)(value->fields + idx);
}


double
mrklkit_rt_get_struct_item_float(rt_struct_t *value, int64_t idx)
{
    union {
        void **v;
        double *d;
    } res;

    assert(idx < value->fnum);
    res.v = value->fields + idx;
    return *res.d;
}


int64_t
mrklkit_rt_get_struct_item_bool(rt_struct_t *value, int64_t idx)
{
    assert(idx < value->fnum);
    return *(int64_t *)(value->fields + idx);
}


bytes_t *
mrklkit_rt_get_struct_item_str(rt_struct_t *value, int64_t idx)
{
    bytes_t **res;

    assert(idx < value->fnum);
    res = (bytes_t **)(value->fields + idx);
    return *res;
}


array_t *
mrklkit_rt_get_struct_item_array(rt_struct_t *value, int64_t idx)
{
    array_t **res;

    assert(idx < value->fnum);
    res = (array_t **)(value->fields + idx);
    return *res;
}


dict_t *
mrklkit_rt_get_struct_item_dict(rt_struct_t *value, int64_t idx)
{
    dict_t **res;

    assert(idx < value->fnum);
    res = (dict_t **)(value->fields + idx);
    return *res;
}


rt_struct_t *
mrklkit_rt_get_struct_item_struct(rt_struct_t *value, int64_t idx)
{
    rt_struct_t **res;

    assert(idx < value->fnum);
    res = (rt_struct_t **)(value->fields + idx);
    return *res;
}


void
mrklkit_rt_set_struct_item_int(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < value->fnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_set_struct_item_float(rt_struct_t *value, int64_t idx, double val)
{
    union {
        void **v;
        double *d;
    } p;

    assert(idx < value->fnum);
    p.v = (value->fields + idx);
    *p.d = val;
}


void
mrklkit_rt_set_struct_item_bool(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < value->fnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_set_struct_item_str(rt_struct_t *value, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    assert(idx < value->fnum);
    p = (bytes_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_shallow_copy(rt_struct_t *dst,
                               rt_struct_t *src,
                               UNUSED lkit_struct_t *ty)
{
    UNUSED lkit_type_t **fty;
    UNUSED array_iter_t it;

    assert(dst->fnum == src->fnum);

#if 1
    for (fty = array_first(&ty->fields, &it);
         fty != NULL;
         fty = array_next(&ty->fields, &it)) {

        *(dst->fields + it.iter) = *(src->fields + it.iter);

        switch ((*fty)->tag) {
        case LKIT_STR:
            BYTES_INCREF((bytes_t *)(*(dst->fields + it.iter)));
            break;

        case LKIT_ARRAY:
            ARRAY_INCREF((rt_array_t *)(*(dst->fields + it.iter)));
            break;

        case LKIT_DICT:
            DICT_INCREF((rt_dict_t *)(*(dst->fields + it.iter)));
            break;

        case LKIT_STRUCT:
            STRUCT_INCREF((rt_struct_t *)(*(dst->fields + it.iter)));
            break;

        default:
            break;
        }
    }
#endif

#if 0
    memcpy(dst->fields, src->fields, dst->fnum * sizeof(void *));
#endif

}


void
lruntime_init(void)
{
}


void
lruntime_fini(void)
{
}
