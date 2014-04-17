#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

static array_t objects;

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
