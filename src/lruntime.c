#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

//UNUSED static array_t objects;


/**
 * array
 */
rt_array_t *
mrklkit_rt_array_new(lkit_type_t *ty)
{
    rt_array_t *res;

    if ((res = malloc(sizeof(rt_array_t))) == NULL) {
        FAIL("malloc");
    }

    res->nref = 0;

    switch (ty->tag) {
    case LKIT_INT:
    case LKIT_FLOAT:
    case LKIT_STR:
        array_init(&res->fields, sizeof(void *), 0, NULL, NULL);

    default:
        FAIL("mrklkit_rt_array_init");
    }
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

    if ((res = array_get(&value->fields, idx)) == NULL) {
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

    if ((res.v = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res.d;
}


bytes_t *
mrklkit_rt_get_array_item_str(rt_array_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


/**
 * dict
 */
static void
rt_dict_fini_keyonly(bytes_t *key, UNUSED void *val)
{
    if (key != NULL) {
        free(key);
    }
}

static void
rt_dict_fini_keyval(bytes_t *key, void *val)
{
    if (key != NULL) {
        free(key);
    }
    if (val != NULL) {
        free(val);
    }
}


rt_dict_t *
mrklkit_rt_dict_new(lkit_type_t *ty)
{
    rt_dict_t *res;

    if ((res = malloc(sizeof(rt_dict_t))) == NULL) {
        FAIL("malloc");
    }
    res->nref = 0;

    switch (ty->tag) {
    case LKIT_INT:
    case LKIT_FLOAT:
        dict_init(&res->fields,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyonly);
        break;

    case LKIT_STR:
        dict_init(&res->fields,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyval);
        break;

    default:
        FAIL("mrklkit_rt_dict_init");
    }
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
mrklkit_rt_struct_new(lkit_struct_t *stty)
{
    rt_struct_t *v;

    size_t sz = stty->fields.elnum * sizeof(void *);

    if ((v = malloc(sizeof(rt_struct_t) + sz)) == NULL) {
        FAIL("malloc");
    }
    v->fnum = stty->fields.elnum;
    v->nref = 1;
    v->init = stty->init;
    v->fini = stty->fini;
    v->current = 0;
    if (v->init != NULL) {
        v->init(v->fields);
    }
    return v;
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    if (*value != NULL) {
        if ((*value)->fini != NULL) {
            (*value)->fini((*value)->fields);
        }
        free(*value); \
        *value = NULL;
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
    BYTES_INCREF(*res);
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
mrklkit_rt_struct_shallow_copy(rt_struct_t *dst, rt_struct_t *src)
{
    assert(dst->fnum == src->fnum);
    memcpy(dst->fields, src->fields, dst->fnum * sizeof(void *));
}


void
lruntime_init(void)
{
}


void
lruntime_fini(void)
{
}
