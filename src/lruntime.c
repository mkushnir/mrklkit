#include <errno.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/mpool.h>

#include <mrklkit/dparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

static mpool_ctx_t *mpool;


int
mrklkit_rt_strcmp(const char *a, const char *b)
{
    TRACE("b=%s", b);
    TRACE("a=%s", a);
    return strcmp(a, b);
}

/**
 * str
 */
bytes_t *
mrklkit_rt_bytes_new_gc(size_t sz)
{
    bytes_t *res;

    res = bytes_new_mpool(mpool, sz);
    //TRACE("GC>>> %p", *res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_new_from_str_gc(const char *s)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, s);
    //TRACE("GC>>> %p", *res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_slice_gc(bytes_t *str, int64_t begin, int64_t end)
{
    bytes_t *res;
    size_t sz0, sz1;

    sz0 = str->sz; /* expect zero-term */
    if (sz0 <= 1) {
        goto empty;
    }
    begin = begin % sz0;
    end = end % sz0;
    sz1 = end - begin;
    if (sz1 >= sz0) {
        goto empty;
    }
    ++sz1; /* "end" including the last char */
    res = bytes_new_mpool(mpool, sz1 + 1);
    memcpy(res->data, str->data + begin, sz1);
    res->data[sz1] = '\0';

end:
    return res;

empty:
    res = bytes_new_mpool(mpool, 1);
    res->data[0] = '\0';
    goto end;
}


bytes_t *
mrklkit_rt_bytes_brushdown_gc(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, (char *)str->data);
    bytes_brushdown(res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_urldecode_gc(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, (char *)str->data);
    bytes_urldecode(res);
    return res;
}


int64_t
mrklkit_strtoi64(bytes_t *str, int64_t dflt)
{
    char *endptr;
    int64_t res;
    res = dparser_strtoi64((char *)str->data, &endptr, '\0');
    if (errno == EINVAL) {
        res = dflt;
    }
    return res;
}

double
mrklkit_strtod(bytes_t *str, double dflt)
{
    char *endptr;
    double res;
    res = dparser_strtod((char *)str->data, &endptr, '\0');
    if (errno == EINVAL) {
        res = dflt;
    }
    return res;
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


void
mrklkit_rt_array_print(rt_array_t *value)
{
    void **val;
    array_iter_t it;
    lkit_type_t *fty;

    fty = lkit_array_get_element_type(value->type);

    TRACEC("\t");
    for (val = array_first(&value->fields, &it);
         val != NULL;
         val = array_next(&value->fields, &it)) {

        switch (fty->tag) {
        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            TRACEC("%ld",
                   mrklkit_rt_get_array_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf",
                   mrklkit_rt_get_array_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *val;

                val = mrklkit_rt_get_array_item_str(value, it.iter, NULL);
                TRACEC("%s", val != NULL ? val->data : NULL);
            }
            break;

        default:
            FAIL("mrklkit_rt_array_print");

        }
    }
}


static int
null_init(void **v)
{
    *v = NULL;
    return 0;
}


#define MRKLKIT_RT_ARRAY_NEW_BODY(malloc_fn) \
    rt_array_t *res; \
 \
    if ((res = malloc_fn(sizeof(rt_array_t))) == NULL) { \
        FAIL("malloc"); \
    } \
    res->nref = 0; \
    res->type = ty; \
    array_init(&res->fields, \
               sizeof(void *), \
               0, \
               (array_initializer_t)null_init, \
               NULL); \
    array_ensure_datasz(&res->fields, ty->nreserved, 0); \
    return res



rt_array_t *
mrklkit_rt_array_new(lkit_array_t *ty)
{
    MRKLKIT_RT_ARRAY_NEW_BODY(malloc);
}


rt_array_t *
mrklkit_rt_array_new_gc(lkit_array_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc);
#undef _malloc
}


rt_array_t *
mrklkit_rt_array_new_mpool(mpool_ctx_t *mpool, lkit_array_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc);
#undef _malloc
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

    if (idx < (ssize_t)value->fields.elnum) {
        if (value->fields.elnum) {
            res = ARRAY_GET(int64_t, &value->fields, idx % value->fields.elnum);
        } else {
            res = &dflt;
        }
    } else {
        res = &dflt;
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

    if (idx < (ssize_t)value->fields.elnum) {
        if (value->fields.elnum) {
            res.v = ARRAY_GET(void *, &value->fields, idx % value->fields.elnum);
        } else {
            res.d = &dflt;
        }
    } else {
        res.d = &dflt;
    }
    return *res.d;
}


bytes_t *
mrklkit_rt_get_array_item_str(rt_array_t *value, int64_t idx, bytes_t *dflt)
{
    bytes_t **res;

    if (idx < (ssize_t)value->fields.elnum) {
        if (value->fields.elnum) {
            res = ARRAY_GET(bytes_t *, &value->fields, idx % value->fields.elnum);
            if (*res == NULL) {
                res = &dflt;
            }
        } else {
            res = &dflt;
        }
    } else {
        res = &dflt;
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

            if ((item = array_incr_mpool(mpool, &res->fields)) == NULL) {
                FAIL("array_incr_mpool");
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


int64_t
mrklkit_rt_array_len(rt_array_t *ar)
{
    return (int64_t)ar->fields.elnum;
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


void
mrklkit_rt_dict_print(rt_dict_t *value)
{
    lkit_type_t *fty;

    fty = lkit_dict_get_element_type(value->type);

    TRACEC("\t");
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
        FAIL("mrklkit_rt_dict_print");
    }
}


#define MRKLKIT_RT_DICT_NEW_BODY(malloc_fn, dict_init_fn) \
    rt_dict_t *res; \
    if ((res = malloc_fn(sizeof(rt_dict_t))) == NULL) { \
        FAIL("malloc_fn"); \
    } \
    res->nref = 0; \
    res->type = ty; \
    dict_init_fn(&res->fields, \
              17, \
              (dict_hashfn_t)bytes_hash, \
              (dict_item_comparator_t)bytes_cmp, \
              NULL); \
    return res

rt_dict_t *
mrklkit_rt_dict_new(lkit_dict_t *ty)
{
    MRKLKIT_RT_DICT_NEW_BODY(malloc, dict_init);
}


rt_dict_t *
mrklkit_rt_dict_new_gc(lkit_dict_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
#define _dict_init(dict, sz, hashfn, cmpfn, finifn) dict_init_mpool(mpool, (dict), (sz), (hashfn), (cmpfn), (finifn))
    MRKLKIT_RT_DICT_NEW_BODY(_malloc, _dict_init);
#undef _malloc
#undef _dict_init
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
        res = it->value == NULL ? dflt : it->value;
    }
    return res;
}


int64_t
mrklkit_rt_dict_has_item(rt_dict_t *value, bytes_t *key)
{
    return dict_get_item(&value->fields, key) != NULL;
}

/**
 * struct
 */

#define MRKLKIT_RT_STRUCT_NEW_BODY(malloc_fn) \
    rt_struct_t *v; \
    size_t sz = ty->fields.elnum * sizeof(void *) + \
                (ty->fields.elnum + 1) * sizeof(off_t); \
    if ((v = malloc_fn(sizeof(rt_struct_t) + sz)) == NULL) { \
        FAIL("malloc"); \
    } \
    v->nref = 0; \
    v->type = ty; \
    v->parser_info.bs = NULL; \
    v->parser_info.pos = 0; \
    v->next_delim = 0; \
    v->current = 0; \
    v->dpos = (off_t *)(&v->fields[ty->fields.elnum]); \
    (void)memset(v->dpos, '\0', (ty->fields.elnum + 1) * sizeof(off_t)); \
    if (ty->init != NULL) { \
        ty->init(v->fields); \
    } \
    return v

rt_struct_t *
mrklkit_rt_struct_new(lkit_struct_t *ty)
{
    MRKLKIT_RT_STRUCT_NEW_BODY(malloc);
}


rt_struct_t *
mrklkit_rt_struct_new_gc(lkit_struct_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
    MRKLKIT_RT_STRUCT_NEW_BODY(_malloc);
#undef _malloc
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    STRUCT_DECREF(value);
}


void
mrklkit_rt_struct_destroy_no_destruct(rt_struct_t **value)
{
    STRUCT_DECREF_NO_DESTRUCT(value);
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

        case LKIT_BOOL:
            TRACEC("%s ", (int8_t)mrklkit_rt_get_struct_item_int(value, it.iter) ? "#t" : "#f");
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


void
mrklkit_rt_struct_print(rt_struct_t *value)
{
    lkit_type_t **fty;
    array_iter_t it;

    for (fty = array_first(&value->type->fields, &it);
         fty != NULL;
         fty = array_next(&value->type->fields, &it)) {

        TRACEC("\t");

        switch ((*fty)->tag) {
        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            TRACEC("%ld", mrklkit_rt_get_struct_item_int(value, it.iter));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf", mrklkit_rt_get_struct_item_float(value, it.iter));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_get_struct_item_str(value, it.iter);
                TRACEC("%s", v != NULL ? v->data : (unsigned char *)"");
            }
            break;

        case LKIT_BOOL:
            TRACEC("%ld", mrklkit_rt_get_struct_item_int(value, it.iter));
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_print(
                mrklkit_rt_get_struct_item_array(value, it.iter));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_print(
                mrklkit_rt_get_struct_item_dict(value, it.iter));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_print(
                mrklkit_rt_get_struct_item_struct(value, it.iter));
            break;

        default:
            FAIL("mrklkit_rt_struct_print");
        }
    }
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
    int64_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int64_t *)(value->fields + idx);
    return *v;
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
    int64_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int64_t *)(value->fields + idx);
    return *v;
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


#define MRKLKIT_RT_STRUCT_DEEP_COPY_BODY(bytes_new_fn) \
    lkit_type_t **fty; \
    array_iter_t it; \
    assert(dst->type->fields.elnum == src->type->fields.elnum); \
    for (fty = array_first(&dst->type->fields, &it); \
         fty != NULL; \
         fty = array_next(&dst->type->fields, &it)) { \
        switch ((*fty)->tag) { \
        case LKIT_STR: \
            { \
                bytes_t *a, *b; \
                a = (bytes_t *)*(src->fields + it.iter); \
                if (a != NULL) { \
                    b = bytes_new_fn(a->sz); \
                    BYTES_INCREF(b); \
                    bytes_copy(b, a, 0); \
                } else { \
                    b = bytes_new_fn(1); \
                    BYTES_INCREF(b); \
                    b->data[0] = '\0'; \
                } \
                *((bytes_t **)dst->fields + it.iter) = b; \
            } \
            break; \
        case LKIT_ARRAY: \
            FAIL("mrklkit_rt_struct_deep_copy not implement array"); \
            break; \
        case LKIT_DICT: \
            FAIL("mrklkit_rt_struct_deep_copy not implement array"); \
            break; \
        case LKIT_STRUCT: \
            FAIL("mrklkit_rt_struct_deep_copy not implement array"); \
            break; \
        default: \
            *(dst->fields + it.iter) = *(src->fields + it.iter); \
        } \
    }


void
mrklkit_rt_struct_deep_copy(rt_struct_t *dst,
                            rt_struct_t *src)
{
    MRKLKIT_RT_STRUCT_DEEP_COPY_BODY(bytes_new);
}


void
mrklkit_rt_struct_deep_copy_gc(rt_struct_t *dst,
                               rt_struct_t *src)
{
#define _bytes_new(sz) bytes_new_mpool(mpool, (sz))
    MRKLKIT_RT_STRUCT_DEEP_COPY_BODY(_bytes_new);
#undef _bytes_new
}


bytes_t *
mrklkit_rt_struct_pi_data_gc(rt_struct_t *value)
{
    bytes_t *res;
    size_t sz;

    sz = value->parser_info.br.end - value->parser_info.br.start;
    res = mrklkit_rt_bytes_new_gc(sz + 1);
    memcpy(res->data,
           SDATA(value->parser_info.bs,
                 value->parser_info.br.start),
           sz);
    res->data[sz] = '\0';

    return res;
}


void
lruntime_set_mpool(mpool_ctx_t *m)
{
    mpool = m;
}

void
lruntime_init(void)
{
}


void
lruntime_fini(void)
{
}
