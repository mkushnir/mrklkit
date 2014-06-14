#include <string.h>
#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

static array_t bytes_gc;
static array_iter_t bytes_gc_it;
static array_t array_gc;
static array_iter_t array_gc_it;
static array_t dict_gc;
static array_iter_t dict_gc_it;
static array_t struct_gc;
static array_iter_t struct_gc_it;


static void
mrklkit_rt_gc_init(void)
{
    array_init(&bytes_gc, sizeof(bytes_t *), 0, NULL, NULL);
    bytes_gc_it.iter = 0;
    array_init(&array_gc, sizeof(rt_array_t *), 0, NULL, NULL);
    array_gc_it.iter = 0;
    array_init(&dict_gc, sizeof(rt_dict_t *), 0, NULL, NULL);
    dict_gc_it.iter = 0;
    array_init(&struct_gc, sizeof(rt_struct_t *), 0, NULL, NULL);
    struct_gc_it.iter = 0;
}


static void
mrklkit_rt_gc_fini(void)
{
    array_fini(&bytes_gc);
    array_fini(&array_gc);
    array_fini(&dict_gc);
    array_fini(&struct_gc);
}


/**
 * str
 */
bytes_t *
mrklkit_rt_bytes_new_gc(size_t sz)
{
    bytes_t **res;

    if ((res = array_get_iter(&bytes_gc, &bytes_gc_it)) == NULL) {
        res = array_incr(&bytes_gc);
    }
    ++bytes_gc_it.iter;
    *res = mrklkit_bytes_new(sz);
    //TRACE("GC>>> %p", *res);
    return *res;
}


bytes_t *
mrklkit_rt_bytes_new_from_str_gc(const char *s)
{
    bytes_t **res;

    if ((res = array_get_iter(&bytes_gc, &bytes_gc_it)) == NULL) {
        res = array_incr(&bytes_gc);
    }
    ++bytes_gc_it.iter;
    *res = mrklkit_bytes_new_from_str(s);
    //TRACE("GC>>> %p", *res);
    return *res;
}


static void
mrklkit_rt_bytes_do_gc(void)
{
    size_t i;

    for (i = 0; i < bytes_gc_it.iter; ++i) {
        bytes_t **v;

        if ((v = array_get(&bytes_gc, i)) == NULL) {
            FAIL("array_get");
        }
        //TRACE("GC<<< %p %ld", *v, (*v)->nref);
        BYTES_DECREF_FAST(*v);
    }
    bytes_gc_it.iter = 0;
}


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
    array_init(&res->fields, sizeof(void *), 0, NULL, NULL);
    return res;
}


rt_array_t *
mrklkit_rt_array_new_gc(lkit_array_t *ty)
{
    rt_array_t **res;

    if ((res = array_get_iter(&array_gc, &array_gc_it)) == NULL) {
        res = array_incr(&array_gc);
    }
    ++array_gc_it.iter;
    *res = mrklkit_rt_array_new(ty);
    //TRACE("GC>>> %p", *res);
    return *res;
}


static void
mrklkit_rt_array_do_gc(void)
{
    size_t i;

    for (i = 0; i < array_gc_it.iter; ++i) {
        rt_array_t **v;

        if ((v = array_get(&array_gc, i)) == NULL) {
            FAIL("array_get");
        }
        //TRACE("GC<<< %p %ld", *v, (*v)->nref);
        ARRAY_DECREF_FAST(*v);
    }
    array_gc_it.iter = 0;
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


rt_array_t *
mrklkit_rt_array_split_gc(lkit_array_t *ty, bytes_t *str, bytes_t *delim)
{
    rt_array_t *res;
    char ch;
    char *s0, *s1;

    res = mrklkit_rt_array_new_gc(ty);
    ch = delim->data[0];
    for (s0 = (char *)str->data, s1 = s0;
         s0 < (char *)str->data + str->sz;
         ++s1) {
        if (*s1 == ch) {
            bytes_t **item;
            size_t sz;

            if ((item = array_incr(&res->fields)) == NULL) {
                FAIL("array_incr");
            }
            sz = s1 - s0;
            *item = mrklkit_rt_bytes_new_gc(sz + 1);
            memcpy((*item)->data, s0, sz);
            (*item)->data[sz] = '\0';
            s0 = s1 + 1;
        }
    }

    //mrklkit_rt_array_dump(res);
    return res;
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
              NULL);

    return res;
}


rt_dict_t *
mrklkit_rt_dict_new_gc(lkit_dict_t *ty)
{
    rt_dict_t **res;

    if ((res = array_get_iter(&dict_gc, &dict_gc_it)) == NULL) {
        res = array_incr(&dict_gc);
    }
    ++dict_gc_it.iter;
    *res = mrklkit_rt_dict_new(ty);
    //TRACE("GC>>> %p", *res);
    return *res;
}


static void
mrklkit_rt_dict_do_gc(void)
{
    size_t i;

    for (i = 0; i < dict_gc_it.iter; ++i) {
        rt_dict_t **v;

        if ((v = array_get(&dict_gc, i)) == NULL) {
            FAIL("array_get");
        }
        //TRACE("GC<<< %p %ld", *v, (*v)->nref);
        DICT_DECREF_FAST(*v);
    }
    dict_gc_it.iter = 0;
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


rt_struct_t *
mrklkit_rt_struct_new_gc(lkit_struct_t *ty)
{
    rt_struct_t **res;

    if ((res = array_get_iter(&struct_gc, &struct_gc_it)) == NULL) {
        res = array_incr(&struct_gc);
    }
    ++struct_gc_it.iter;
    *res = mrklkit_rt_struct_new(ty);
    //TRACE("GC>>> %p", *res);
    return *res;
}


static void
mrklkit_rt_struct_do_gc(void)
{
    size_t i;

    for (i = 0; i < struct_gc_it.iter; ++i) {
        rt_struct_t **v;

        if ((v = array_get(&struct_gc, i)) == NULL) {
            FAIL("array_get");
        }
        //TRACE("GC<<< %p %ld", *v, (*v)->nref);
        STRUCT_DECREF_FAST(*v);
    }
    struct_gc_it.iter = 0;
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
}


void
mrklkit_rt_struct_deep_copy(rt_struct_t *dst,
                            rt_struct_t *src)
{
    lkit_type_t **fty;
    array_iter_t it;

    assert(dst->type->fields.elnum == src->type->fields.elnum);

    for (fty = array_first(&dst->type->fields, &it);
         fty != NULL;
         fty = array_next(&dst->type->fields, &it)) {


        switch ((*fty)->tag) {
        case LKIT_STR:
            {
                bytes_t *a, *b;

                a = (bytes_t *)*(src->fields + it.iter);
                b = mrklkit_bytes_new(a->sz);
                BYTES_INCREF(b);
                mrklkit_bytes_copy(b, a, 0);
                *((bytes_t **)dst->fields + it.iter) = b;
            }
            break;

        case LKIT_ARRAY:
            FAIL("mrklkit_rt_struct_deep_copy not implement array");
            break;

        case LKIT_DICT:
            FAIL("mrklkit_rt_struct_deep_copy not implement array");
            break;

        case LKIT_STRUCT:
            FAIL("mrklkit_rt_struct_deep_copy not implement array");
            break;

        default:
            *(dst->fields + it.iter) = *(src->fields + it.iter);
        }
    }
}


void
mrklkit_rt_do_gc(void)
{
    mrklkit_rt_bytes_do_gc();
    mrklkit_rt_array_do_gc();
    mrklkit_rt_dict_do_gc();
    mrklkit_rt_struct_do_gc();
}


void
lruntime_init(void)
{
    mrklkit_rt_gc_init();
}


void
lruntime_fini(void)
{
    mrklkit_rt_gc_fini();
}
