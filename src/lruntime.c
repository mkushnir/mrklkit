#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

static array_t objects;

void
rt_dict_fini_keyonly(bytes_t *key, UNUSED void *val)
{
    if (key != NULL) {
        free(key);
    }
}

void
rt_dict_fini_keyval(bytes_t *key, UNUSED void *val)
{
    if (key != NULL) {
        free(key);
    }
}


void
mrklkit_rt_dict_init(dict_t *dict, lkit_type_t *ty)
{
    switch (ty->tag) {
    case LKIT_INT:
    case LKIT_FLOAT:
        dict_init(dict,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyonly);
        break;

    case LKIT_STR:
        dict_init(dict,
                  17,
                  (dict_hashfn_t)bytes_hash,
                  (dict_item_comparator_t)bytes_cmp,
                  (dict_item_finalizer_t)rt_dict_fini_keyval);
    default:
        FAIL("mrklkit_rt_dict_init");
    }
}


void
mrklkit_rt_array_dtor(void *o)
{
    array_t *v = o;
    array_fini(v);
}

void
mrklkit_rt_dict_dtor(void *o)
{
    dict_t *v = o;
    dict_fini(v);
}

static int
tobj_fini(tobj_t **o)
{
    if (*o != NULL) {
        if ((*o)->type->dtor != NULL) {
            (*o)->type->dtor((*o)->value);
        }
        free(*o);
        *o = NULL;
    }
    return 0;
}

void
mrklkit_rt_gc(void)
{
    array_fini(&objects);
}

void
lruntime_init(void)
{
    array_init(&objects, sizeof(tobj_t *), 0,
               NULL,
               (array_finalizer_t)tobj_fini);
}

void
lruntime_fini(void)
{
    array_fini(&objects);
}
