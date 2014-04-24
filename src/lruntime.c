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
