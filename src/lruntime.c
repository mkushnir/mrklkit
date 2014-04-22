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
    case LKIT_QSTR:
        array_init(value, sizeof(byterange_t), 0,
                   NULL,
                   (array_finalizer_t)bytes_destroy);

    default:
        FAIL("mrklkit_rt_array_init");
    }
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
tobj_fini(tobj_t *o)
{
    if (o->type->dtor != NULL) {
        o->type->dtor(&o->value);
    }
    return 0;
}



void
mrklkit_rt_struct_init(array_t *value)
{
    array_init(value, sizeof(tobj_t), 0, NULL,
               (array_finalizer_t)tobj_fini);
}


void
mrklkit_rt_struct_dtor(array_t **value)
{
    if (*value != NULL) {
        array_fini(*value);
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
