#include <string.h>
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
void
mrklkit_rt_array_dump(rt_array_t *value)
{
    void **val;
    array_iter_t it;
    lkit_type_t *fty;

    fty = lkit_array_get_element_type(value->type);

    TRACEC("[ ");
    for (val = array_first(&value->fields, &it);
         val != NULL;
         val = array_next(&value->fields, &it)) {

        switch (fty->tag) {
        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            TRACEC("%ld ",
                   mrklkit_rt_get_array_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf ",
                   mrklkit_rt_get_array_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *val;

                val = mrklkit_rt_get_array_item_str(value, it.iter, NULL);
                TRACEC("%s ", val != NULL ? val->data : NULL);
            }
            break;

        default:
            FAIL("mrklkit_rt_array_dump");

        }
    }
    TRACEC("] ");
}


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

    if ((res = ARRAY_GET(int64_t,
                         &value->fields,
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

    if ((res.v = ARRAY_GET(void *,
                           &value->fields,
                           idx % value->fields.elnum)) == NULL) {
        return dflt;
    }
    return *res.d;
}


bytes_t *
mrklkit_rt_get_array_item_str(rt_array_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if ((res = ARRAY_GET(bytes_t *,
                         &value->fields,
                         idx % value->fields.elnum)) == NULL) {
        return dflt;
    }
    return *res;
}


/**
 * dict
 */
static int
dump_int_dict(bytes_t *k, int64_t v, UNUSED void *udata)
{
    TRACEC("%s:%ld ", k->data, v);
    return 0;
}


static int
dump_float_dict(bytes_t *k, void *v, UNUSED void *udata)
{
    union {
        void *p;
        double d;
    } vv;
    vv.p = v;
    TRACEC("%s:%lf ", k->data, vv.d);
    return 0;
}


static int
dump_bytes_dict(bytes_t *k, bytes_t *v, UNUSED void *udata)
{
    TRACEC("%s:%s ", k->data, v->data);
    return 0;
}


void
mrklkit_rt_dict_dump(rt_dict_t *value)
{
    lkit_type_t *fty;

    fty = lkit_dict_get_element_type(value->type);

    TRACEC("{ ");
    switch (fty->tag) {
    case LKIT_INT:
    case LKIT_INT_MIN:
    case LKIT_INT_MAX:
        dict_traverse(&value->fields,
                      (dict_traverser_t)dump_int_dict,
                      NULL);
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        dict_traverse(&value->fields,
                      (dict_traverser_t)dump_float_dict,
                      NULL);
        break;

    case LKIT_STR:
        dict_traverse(&value->fields,
                      (dict_traverser_t)dump_bytes_dict,
                      NULL);
        break;

    default:
        FAIL("mrklkit_rt_dict_dump");
    }
    TRACEC("} ");
}


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
    dict_item_t *it;
    union {
        void *v;
        int64_t i;
    } res;

    if ((it = dict_get_item(&value->fields, key)) == NULL) {
        res.i = dflt;
    } else {
        res.v = it->value;
    }
    return res.i;
}


double
mrklkit_rt_get_dict_item_float(rt_dict_t *value, bytes_t *key, double dflt)
{
    dict_item_t *it;
    union {
        void *v;
        double d;
    } res;

    if ((it = dict_get_item(&value->fields, key)) == NULL) {
        res.d = dflt;
    } else {
        res.v = it->value;
    }
    return res.d;
}


bytes_t *
mrklkit_rt_get_dict_item_str(rt_dict_t *value, bytes_t *key, bytes_t *dflt)
{
    dict_item_t *it;
    bytes_t *res;

    if ((it = dict_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    } else {
        res = it->value;
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

    size_t sz = ty->fields.elnum * sizeof(void *) +
                (ty->fields.elnum + 1) * sizeof(off_t);

    if ((v = malloc(sizeof(rt_struct_t) + sz)) == NULL) {
        FAIL("malloc");
    }
    v->nref = 0;
    v->type = ty;
    v->parser_info.bs = NULL;
    v->parser_info.pos = 0;
    v->next_delim = 0;
    v->current = 0;
    v->dpos = (off_t *)(&v->fields[ty->fields.elnum]);
    (void)memset(v->dpos, '\0', (ty->fields.elnum + 1) * sizeof(off_t));
    if (ty->init != NULL) {
        ty->init(v->fields);
    }
    return v;
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    STRUCT_DECREF(value);
}


void
mrklkit_rt_struct_dump(rt_struct_t *value)
{
    lkit_type_t **fty;
    array_iter_t it;

    TRACEC("< ");
    //TRACE("ty=%p", ty);
    for (fty = array_first(&value->type->fields, &it);
         fty != NULL;
         fty = array_next(&value->type->fields, &it)) {

        switch ((*fty)->tag) {
        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            TRACEC("%ld ", mrklkit_rt_get_struct_item_int(value, it.iter));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf ", mrklkit_rt_get_struct_item_float(value, it.iter));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_get_struct_item_str(value, it.iter);
                TRACEC("'%s' ", v != NULL ? v->data : NULL);
            }
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_dump(
                mrklkit_rt_get_struct_item_array(value, it.iter));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_dump(
                mrklkit_rt_get_struct_item_dict(value, it.iter));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_dump(
                mrklkit_rt_get_struct_item_struct(value, it.iter));
            break;

        default:
            FAIL("mrklkit_rt_struct_dump");
        }
    }
    TRACEC("> ");
}


void **
mrklkit_rt_get_struct_item_addr(rt_struct_t *value, int64_t idx)
{
    assert(idx < (ssize_t)value->type->fields.elnum);
    return value->fields + idx;
}


int64_t
mrklkit_rt_get_struct_item_int(rt_struct_t *value, int64_t idx)
{
    assert(idx < (ssize_t)value->type->fields.elnum);
    return *(int64_t *)(value->fields + idx);
}


double
mrklkit_rt_get_struct_item_float(rt_struct_t *value, int64_t idx)
{
    union {
        void **v;
        double *d;
    } res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res.v = value->fields + idx;
    return *res.d;
}


int64_t
mrklkit_rt_get_struct_item_bool(rt_struct_t *value, int64_t idx)
{
    assert(idx < (ssize_t)value->type->fields.elnum);
    return *(int64_t *)(value->fields + idx);
}


bytes_t *
mrklkit_rt_get_struct_item_str(rt_struct_t *value, int64_t idx)
{
    bytes_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (bytes_t **)(value->fields + idx);
    return *res;
}


rt_array_t *
mrklkit_rt_get_struct_item_array(rt_struct_t *value, int64_t idx)
{
    rt_array_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_array_t **)(value->fields + idx);
    return *res;
}


rt_dict_t *
mrklkit_rt_get_struct_item_dict(rt_struct_t *value, int64_t idx)
{
    rt_dict_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_dict_t **)(value->fields + idx);
    return *res;
}


rt_struct_t *
mrklkit_rt_get_struct_item_struct(rt_struct_t *value, int64_t idx)
{
    rt_struct_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_struct_t **)(value->fields + idx);
    return *res;
}


void
mrklkit_rt_set_struct_item_int(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
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

    assert(idx < (ssize_t)value->type->fields.elnum);
    p.v = (value->fields + idx);
    *p.d = val;
}


void
mrklkit_rt_set_struct_item_bool(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_set_struct_item_str(rt_struct_t *value, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_shallow_copy(rt_struct_t *dst,
                               rt_struct_t *src)
{
    lkit_type_t **fty;
    array_iter_t it;

    assert(dst->type->fields.elnum == src->type->fields.elnum);

#define MRKLKIT_RT_STRUCT_SHALLOW_COPY_SLOW

#ifdef MRKLKIT_RT_STRUCT_SHALLOW_COPY_SLOW
    for (fty = array_first(&dst->type->fields, &it);
         fty != NULL;
         fty = array_next(&dst->type->fields, &it)) {

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
#else
    memcpy(dst->fields, src->fields, dst->type->fields.elnum * sizeof(void *));
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
