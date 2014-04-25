#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

UNUSED static array_t objects;

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
mrklkit_rt_dict_dtor(dict_t **value)
{
    if (*value != NULL) {
        dict_fini(*value);
        free(*value);
        *value = NULL;
    }
}


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


void *
mrklkit_rt_get_array_item(array_t *value, int64_t idx, void *dflt)
{
    void **res;

    if ((res = array_get(value, idx)) == NULL) {
        *res = dflt;
    }
    return *res;
}


void *
mrklkit_rt_get_dict_item(dict_t *value, bytes_t *key, void *dflt)
{
    void *res;

    if ((res = dict_get_item(value, key)) == NULL) {
        res = dflt;
    }
    return res;
}


void *
mrklkit_rt_get_struct_item(rt_struct_t *value, int64_t idx, void *dflt)
{
    void **res;

    if ((res = array_get(&value->fields, idx)) == NULL) {
        *res = dflt;
    }
    return *res;
}


void
mrklkit_rt_array_dtor(array_t **value)
{
    if (*value != NULL) {
        array_fini(*value);
        free(*value);
        *value = NULL;
    }
}


static int
dump_int_array(int64_t *v, UNUSED void *udata)
{
    TRACE("v=%ld", *v);
    return 0;
}


static int
dump_float_array(double *v, UNUSED void *udata)
{
    TRACE("v=%lf", *v);
    return 0;
}


static int
dump_bytes_array(bytes_t **v, UNUSED void *udata)
{
    TRACE("v=%s", (*v)->data);
    return 0;
}


static int
dump_int_dict(bytes_t *k, int64_t v, UNUSED void *udata)
{
    TRACE("%s:%ld", k->data, v);
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
    TRACE("%s:%lf", k->data, vv.d);
    return 0;
}


static int
dump_bytes_dict(bytes_t *k, bytes_t *v, UNUSED void *udata)
{
    TRACE("%s:%s", k->data, v->data);
    return 0;
}


int
tobj_dump(tobj_t *o, void *udata)
{
    if (o->type != NULL) {
        switch (o->type->tag) {
        case LKIT_INT:
            TRACE(" %ld", (int64_t)o->value);
            break;

        case LKIT_FLOAT:
            {
                union {
                    void *p;
                    double d;
                } v;

                assert(sizeof(double) == sizeof(void *));
                v.p = o->value;
                TRACE(" %lf", v.d);
            }
            break;

        case LKIT_STR:
            TRACE(" %s", ((bytes_t *)o->value)->data);
            break;

        case LKIT_ARRAY:
            {
                lkit_array_t *ta;
                lkit_type_t *fty;

                ta = (lkit_array_t *)o->type;
                fty = lkit_array_get_element_type(ta);
                switch (fty->tag) {
                case LKIT_INT:
                    array_traverse((array_t *)o->value,
                                   (array_traverser_t)dump_int_array, NULL);
                    break;

                case LKIT_FLOAT:
                    array_traverse((array_t *)o->value,
                                   (array_traverser_t)dump_float_array, NULL);
                    break;

                case LKIT_STR:
                    array_traverse((array_t *)o->value,
                                   (array_traverser_t)dump_bytes_array, NULL);
                    break;

                default:
                    assert(0);
                }

            }

        case LKIT_DICT:
            {
                lkit_dict_t *td;
                lkit_type_t *fty;

                td = (lkit_dict_t *)o->type;
                fty = lkit_dict_get_element_type(td);
                switch (fty->tag) {
                case LKIT_INT:
                    dict_traverse((dict_t *)o->value,
                                   (dict_traverser_t)dump_int_dict, NULL);
                    break;

                case LKIT_FLOAT:
                    dict_traverse((dict_t *)o->value,
                                   (dict_traverser_t)dump_float_dict, NULL);
                    break;

                case LKIT_STR:
                    dict_traverse((dict_t *)o->value,
                                   (dict_traverser_t)dump_bytes_dict, NULL);
                    break;

                default:
                    assert(0);
                }
            }
            break;

        case LKIT_STRUCT:
            tobj_dump(o->value, udata);
            break;

        default:
            assert(0);
        }
    }
    return 0;
}


static int
tobj_init(tobj_t *o)
{
    o->type = NULL;
    o->value = NULL;
    return 0;
}


static int
tobj_fini(tobj_t *o)
{
    if (o->type != NULL) {
        if (o->type->dtor != NULL) {
            o->type->dtor(&o->value);
        }
    }
    return 0;
}


void
mrklkit_rt_struct_init(rt_struct_t *value, lkit_struct_t *stty)
{
    value->current = 0;
    array_init(&value->fields, sizeof(tobj_t), stty->fields.elnum,
               (array_initializer_t)tobj_init,
               (array_finalizer_t)tobj_fini);
}

int
mrklkit_rt_struct_fini(rt_struct_t *value)
{
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
mrklkit_rt_struct_dtor(rt_struct_t **value)
{
    if (*value != NULL) {
        (void)mrklkit_rt_struct_fini(*value);
        free(*value);
        *value = NULL;
    }
}


/**
 * tobj_t operations
 */
void
mrklkit_rt_gc(void)
{
    //array_fini(&objects);
}


void
lruntime_init(void)
{
    //array_init(&objects, sizeof(tobj_t), 0,
    //           NULL,
    //           (array_finalizer_t)tobj_fini);
}


void
lruntime_fini(void)
{
    //array_fini(&objects);
}
