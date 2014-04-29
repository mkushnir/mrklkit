#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

//UNUSED static array_t objects;


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


void
mrklkit_rt_dict_init(dict_t *value, lkit_type_t *ty)
{
    switch (ty->tag) {
    case LKIT_INT:
    case LKIT_FLOAT:
        dict_init(value,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyonly);
        break;

    case LKIT_STR:
        dict_init(value,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyval);
        break;

    default:
        FAIL("mrklkit_rt_dict_init");
    }
}


void
mrklkit_rt_dict_destroy(dict_t **value)
{
    if (*value != NULL) {
        dict_fini(*value);
        free(*value);
        *value = NULL;
    }
}


int64_t
mrklkit_rt_get_dict_item_int(dict_t *value, bytes_t *key, int64_t dflt)
{
    union {
        void *v;
        int64_t i;
    } res;

    if ((res.v = dict_get_item(value, key)) == NULL) {
        res.i = dflt;
    }
    return res.i;
}


double
mrklkit_rt_get_dict_item_float(dict_t *value, bytes_t *key, double dflt)
{
    union {
        void *v;
        double d;
    } res;

    if ((res.v = dict_get_item(value, key)) == NULL) {
        res.d = dflt;
    }
    return res.d;
}


bytes_t *
mrklkit_rt_get_dict_item_str(dict_t *value, bytes_t *key, bytes_t *dflt)
{
    bytes_t *res;

    if ((res = dict_get_item(value, key)) == NULL) {
        res = dflt;
    }
    return res;
}


/**
 * array
 */
void
mrklkit_rt_array_init(array_t *value, lkit_type_t *ty)
{
    switch (ty->tag) {
    case LKIT_INT:
        array_init(value, sizeof(int64_t), 0, NULL, NULL);

    case LKIT_FLOAT:

        array_init(value, sizeof(double), 0, NULL, NULL);
        break;

    case LKIT_STR:
        array_init(value, sizeof(byterange_t), 0,
                   NULL,
                   (array_finalizer_t)bytes_destroy);

    default:
        FAIL("mrklkit_rt_array_init");
    }
}


void
mrklkit_rt_array_destroy(array_t **value)
{
    if (*value != NULL) {
        array_fini(*value);
        free(*value);
        *value = NULL;
    }
}


int64_t
mrklkit_rt_get_array_item_int(array_t *value, int64_t idx, int64_t dflt)
{
    int64_t *res;

    if ((res = array_get(value, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


double
mrklkit_rt_get_array_item_float(array_t *value, int64_t idx, double dflt)
{
    double *res;

    if ((res = array_get(value, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


bytes_t *
mrklkit_rt_get_array_item_str(array_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if ((res = array_get(value, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


/**
 * struct
 */
void
mrklkit_rt_struct_init(rt_struct_t *value, lkit_struct_t *stty)
{
    value->current = 0;
    value->init = stty->init;
    value->fini = stty->fini;
    array_init(&value->fields, sizeof(void *), stty->fields.elnum, NULL, NULL);
    if (value->init != NULL) {
        value->init(value->fields.data);
    }
}

int
mrklkit_rt_struct_fini(rt_struct_t *value)
{
    if (value->fini != NULL) {
        value->fini(value->fields.data);
    }
    array_fini(&value->fields);
    value->current = 0;
    return 0;
}

rt_struct_t *
mrklkit_rt_struct_new(lkit_struct_t *stty)
{
    rt_struct_t *v;

    if ((v = malloc(sizeof(rt_struct_t))) == NULL) {
        FAIL("malloc");
    }
    mrklkit_rt_struct_init(v, stty);
    return v;
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    if (*value != NULL) {
        (void)mrklkit_rt_struct_fini(*value);
        free(*value); \
        *value = NULL;
    }
}


int64_t
mrklkit_rt_get_struct_item_int(rt_struct_t *value, int64_t idx, int64_t dflt)
{
    int64_t *res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


double
mrklkit_rt_get_struct_item_float(rt_struct_t *value, int64_t idx, double dflt)
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


int64_t
mrklkit_rt_get_struct_item_bool(rt_struct_t *value, int64_t idx, int64_t dflt)
{
    int64_t *res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


bytes_t *
mrklkit_rt_get_struct_item_str(rt_struct_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    BYTES_INCREF(*res);
    return *res;
}


array_t *
mrklkit_rt_get_struct_item_array(rt_struct_t *value, int64_t idx, array_t *dflt)
{
    array_t **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


dict_t *
mrklkit_rt_get_struct_item_dict(rt_struct_t *value, int64_t idx, dict_t *dflt)
{
    dict_t **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


rt_struct_t *
mrklkit_rt_get_struct_item_struct(rt_struct_t *value, int64_t idx, rt_struct_t *dflt)
{
    rt_struct_t **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        return dflt;
    }
    return *res;
}


void
mrklkit_rt_set_struct_item_int(rt_struct_t *s, int64_t idx, int64_t val)
{
    int64_t *p;

    if ((p = array_get(&s->fields, idx)) == NULL) {
        FAIL("array_get");
    }
    *p = val;
}


void
mrklkit_rt_set_struct_item_float(rt_struct_t *s, int64_t idx, double val)
{
    union {
        void **v;
        double *d;
    } p;

    if ((p.v = array_get(&s->fields, idx)) == NULL) {
        FAIL("array_get");
    }
    *p.d = val;
}


void
mrklkit_rt_set_struct_item_bool(rt_struct_t *s, int64_t idx, int64_t val)
{
    int64_t *p;

    if ((p = array_get(&s->fields, idx)) == NULL) {
        FAIL("array_get");
    }
    *p = val;
}


void
mrklkit_rt_set_struct_item_str(rt_struct_t *s, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    if ((p = array_get(&s->fields, idx)) == NULL) {
        FAIL("array_get");
    }
    *p = val;
}


void
mrklkit_rt_struct_shallow_copy(rt_struct_t *dst, rt_struct_t *src)
{
    array_copy(&dst->fields, &src->fields);
}


void
lruntime_init(void)
{
}


void
lruntime_fini(void)
{
}
