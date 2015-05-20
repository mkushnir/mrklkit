#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/util.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/jparse.h>

#include <mrklkit/dparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

static mpool_ctx_t *mpool;


#define POISON_NREF 0x11110000

int
mrklkit_rt_strcmp(const char *a, const char *b)
{
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
    res->nref = POISON_NREF;
    //TRACE("GC>>> %p", *res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_new_from_str_gc(const char *s)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, s);
    //TRACE("GC>>> %p", *res);
    res->nref = POISON_NREF;
    return res;
}


bytes_t *
mrklkit_rt_bytes_new_from_int_gc(int64_t i)
{
    char buf[1024];
    bytes_t *res;

    snprintf(buf, sizeof(buf), "%ld", i);
    res = bytes_new_from_str_mpool(mpool, buf);
    //TRACE("GC>>> %p", *res);
    res->nref = POISON_NREF;
    return res;
}


bytes_t *
mrklkit_rt_bytes_new_from_float_gc(double f)
{
    char buf[1024];
    bytes_t *res;

    snprintf(buf, sizeof(buf), "%lf", f);
    res = bytes_new_from_str_mpool(mpool, buf);
    //TRACE("GC>>> %p", *res);
    res->nref = POISON_NREF;
    return res;
}


bytes_t *
mrklkit_rt_bytes_new_from_bool_gc(char b)
{
    char buf[1024];
    bytes_t *res;

    snprintf(buf, sizeof(buf), "%s", b ? "true" : "false");
    res = bytes_new_from_str_mpool(mpool, buf);
    //TRACE("GC>>> %p", *res);
    res->nref = POISON_NREF;
    return res;
}


bytes_t *
mrklkit_rt_bytes_slice_gc(bytes_t *str, int64_t begin, int64_t end)
{
    bytes_t *res;
    ssize_t sz0, sz1;

    assert(str->sz > 0);
    sz0 = str->sz - 1; /* cut off zero-term */
    if (sz0 == 0) {
        goto empty;
    }
    begin = (begin + sz0) % sz0;
    end = (end + sz0) % sz0;
    sz1 = end - begin;
    if (sz1 < 0) {
        goto empty;
    }
    ++sz1; /* "end" including the last char */
    res = bytes_new_mpool(mpool, sz1 + 1);
    memcpy(res->data, str->data + begin, sz1);
    res->data[sz1] = '\0';

end:
    res->nref = POISON_NREF;
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
    res->nref = POISON_NREF;
    return res;
}


bytes_t *
mrklkit_rt_bytes_urldecode_gc(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, (char *)str->data);
    bytes_urldecode(res);
    res->nref = POISON_NREF;
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


int64_t
mrklkit_strtoi64_loose(bytes_t *str)
{
    char *endptr;
    int64_t res;
    res = dparser_strtoi64((char *)str->data, &endptr, '\0');
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


/*
 * The same as mrklkit_strtod() except ignoring errno
 */
double
mrklkit_strtod_loose(bytes_t *str)
{
    char *endptr;
    double res;
    res = dparser_strtod((char *)str->data, &endptr, '\0');
    return res;
}


int64_t
mrklkit_strptime(const bytes_t *str, const bytes_t *fmt)
{
    char *rv;
    struct tm t;
    time_t lt;

    time(&lt);
    localtime_r(&lt, &t);
    rv = strptime((char *)str->data, (char *)fmt->data, &t);
    if (rv == NULL) {
        return 0;
    }
    return (int64_t)mktime(&t);
}


double
mrklkit_timef(void)
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    (void)gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double) tv.tv_usec / 1000000.;
}


void
mrklkit_rt_bytes_incref(bytes_t *s)
{
    if (s != NULL) {
        BYTES_INCREF(s);
    }
}


void
mrklkit_rt_bytes_destroy(bytes_t **value)
{
    BYTES_DECREF(value);
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


#define MRKLKIT_RT_ARRAY_NEW_BODY(malloc_fn, array_ensure_datasz_fn)   \
    rt_array_t *res;                                                   \
                                                                       \
    if ((res = malloc_fn(sizeof(rt_array_t))) == NULL) {               \
        FAIL("malloc");                                                \
    }                                                                  \
    res->nref = 0;                                                     \
    res->type = ty;                                                    \
    array_init(&res->fields,                                           \
               sizeof(void *),                                         \
               0,                                                      \
               (array_initializer_t)null_init,                         \
               NULL);                                                  \
    array_ensure_datasz_fn(&res->fields, ty->nreserved, 0);            \



rt_array_t *
mrklkit_rt_array_new(lkit_array_t *ty)
{
    MRKLKIT_RT_ARRAY_NEW_BODY(malloc, array_ensure_datasz);
    return res;
}


rt_array_t *
mrklkit_rt_array_new_gc(lkit_array_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
#define _array_ensure_datasz(ar,                               \
                             datasz,                           \
                             flags)                            \
    array_ensure_datasz_mpool(mpool, (ar), (datasz), (flags))  \


    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc, _array_ensure_datasz);
    res->nref = POISON_NREF;
    return res;
#undef _malloc
#undef _array_ensure_datasz
}


rt_array_t *
mrklkit_rt_array_new_mpool(mpool_ctx_t *mpool, lkit_array_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
#define _array_ensure_datasz(ar,                               \
                             datasz,                           \
                             flags)                            \
    array_ensure_datasz_mpool(mpool, (ar), (datasz), (flags))  \


    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc, _array_ensure_datasz);
    res->nref = POISON_NREF;
    return res;
#undef _malloc
#undef _array_ensure_datasz
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
            res = ARRAY_GET(int64_t,
                            &value->fields,
                            idx % value->fields.elnum);
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
            res.v = ARRAY_GET(void *,
                              &value->fields,
                              idx % value->fields.elnum);
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
            res = ARRAY_GET(bytes_t *,
                            &value->fields,
                            idx % value->fields.elnum);
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
            BYTES_INCREF(*item);
            memcpy((*item)->data, s0, sz);
            (*item)->data[sz] = '\0';
            s0 = s1 + 1;
        }
    }

    res->nref = POISON_NREF;
    return res;
}


int64_t
mrklkit_rt_array_len(rt_array_t *ar)
{
    return (int64_t)ar->fields.elnum;
}


void
mrklkit_rt_array_incref(rt_array_t *a)
{
    if (a != NULL) {
        ARRAY_INCREF(a);
    }
}


static int
array_traverse_cb(void *i, void *udata)
{
    struct {
        void (*cb)(void *);
    } *params = udata;
    params->cb(i);
    return 0;
}

void
mrklkit_rt_array_traverse(rt_array_t *a, void (*cb)(void *))
{
    struct {
        void (*cb)(void *);
    } params;

    params.cb = cb;
    (void)array_traverse(&a->fields, array_traverse_cb, &params);
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


#define MRKLKIT_RT_DICT_NEW_BODY(malloc_fn, dict_init_fn)      \
    rt_dict_t *res;                                            \
    if ((res = malloc_fn(sizeof(rt_dict_t))) == NULL) {        \
        FAIL("malloc_fn");                                     \
    }                                                          \
    res->nref = 0;                                             \
    res->type = ty;                                            \
    dict_init_fn(&res->fields,                                 \
              17,                                              \
              (dict_hashfn_t)bytes_hash,                       \
              (dict_item_comparator_t)bytes_cmp,               \
              ty->fini);                                       \


rt_dict_t *
mrklkit_rt_dict_new(lkit_dict_t *ty)
{
    MRKLKIT_RT_DICT_NEW_BODY(malloc, dict_init);
    return res;
}


rt_dict_t *
mrklkit_rt_dict_new_gc(lkit_dict_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
#define _dict_init(dict,                                               \
                   sz,                                                 \
                   hashfn,                                             \
                   cmpfn,                                              \
                   finifn)                                             \
    dict_init_mpool(mpool, (dict), (sz), (hashfn), (cmpfn), (finifn))  \


    MRKLKIT_RT_DICT_NEW_BODY(_malloc, _dict_init);
    res->nref = POISON_NREF;
    return res;
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


rt_struct_t *
mrklkit_rt_get_dict_item_struct(rt_dict_t *value,
                                bytes_t *key,
                                rt_struct_t *dflt)
{
    dict_item_t *it;
    rt_struct_t *res;

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


void
mrklkit_rt_dict_incref(rt_dict_t *d)
{
    if (d != NULL) {
        DICT_INCREF(d);
    }
}


void
mrklkit_rt_set_dict_item_int(rt_dict_t *value, bytes_t *key, int64_t val)
{
    union {
        int64_t i;
        void *v;
    } v;
    v.i = val;
    BYTES_INCREF(key);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_float(rt_dict_t *value, bytes_t *key, double val)
{
    union {
        double f;
        void *v;
    } v;
    v.f = val;
    BYTES_INCREF(key);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_bool(rt_dict_t *value, bytes_t *key, char val)
{
    union {
        char b;
        void *v;
    } v;
    v.b = val;
    BYTES_INCREF(key);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_str(rt_dict_t *value, bytes_t *key, bytes_t *val)
{
    union {
        bytes_t *s;
        void *v;
    } v;
    v.s = val;
    BYTES_INCREF(key);
    BYTES_INCREF(val);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_array(rt_dict_t *value, bytes_t *key, rt_array_t *val)
{
    union {
        rt_array_t *a;
        void *v;
    } v;
    v.a = val;
    BYTES_INCREF(key);
    ARRAY_INCREF(val);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_dict(rt_dict_t *value, bytes_t *key, rt_dict_t *val)
{
    union {
        rt_dict_t *d;
        void *v;
    } v;
    v.d = val;
    BYTES_INCREF(key);
    DICT_INCREF(val);
    dict_set_item(&value->fields, key, v.v);
}


void
mrklkit_rt_set_dict_item_struct(rt_dict_t *value,
                                bytes_t *key,
                                rt_struct_t *val)
{
    union {
        rt_struct_t *s;
        void *v;
    } v;
    v.s = val;
    BYTES_INCREF(key);
    STRUCT_INCREF(val);
    dict_set_item(&value->fields, key, v.v);
}


static int
dict_traverse_cb(bytes_t *key, void *value, void *udata)
{
    struct {
        void (*cb)(bytes_t *, void *);
    } *params = udata;
    params->cb(key, value);
    return 0;
}

void
mrklkit_rt_dict_traverse(rt_dict_t *value, void (*cb)(bytes_t *, void *))
{
    struct {
        void (*cb)(bytes_t *, void *);
    } params;

    params.cb = cb;
    (void)dict_traverse(&value->fields,
                        (dict_traverser_t)dict_traverse_cb,
                        &params);
}

/**
 * struct
 */

#define MRKLKIT_RT_STRUCT_NEW_BODY(malloc_fn)                                  \
    rt_struct_t *v;                                                            \
    size_t sz = ty->fields.elnum * sizeof(void *) +                            \
                (ty->fields.elnum + 1) * sizeof(off_t);                        \
    if ((v = malloc_fn(sizeof(rt_struct_t) + sz)) == NULL) {                   \
        FAIL("malloc");                                                        \
    }                                                                          \
    v->nref = 0;                                                               \
    v->type = ty;                                                              \
    v->parser_info.bs = NULL;                                                  \
    v->parser_info.pos = 0;                                                    \
    v->next_delim = 0;                                                         \
    v->current = 0;                                                            \
    v->dpos = (off_t *)(&v->fields[ty->fields.elnum]);                         \
    (void)memset(v->dpos, '\0', (ty->fields.elnum + 1) * sizeof(off_t));       \
    if (ty->init != NULL) {                                                    \
        ty->init(v->fields);                                                   \
    }                                                                          \


rt_struct_t *
mrklkit_rt_struct_new(lkit_struct_t *ty)
{
    MRKLKIT_RT_STRUCT_NEW_BODY(malloc);
    return v;
}


rt_struct_t *
mrklkit_rt_struct_new_gc(lkit_struct_t *ty)
{
#define _malloc(sz) mpool_malloc(mpool, (sz))
    MRKLKIT_RT_STRUCT_NEW_BODY(_malloc);
    v->nref = POISON_NREF;
    return v;
#undef _malloc
}


void
mrklkit_rt_struct_destroy(rt_struct_t **value)
{
    STRUCT_DECREF(value);
}


int
mrklkit_rt_struct_init(rt_struct_t *value)
{
    if (value->type->init != NULL) {
        (void)value->type->init(value->fields);
    }
    return 0;
}


int
mrklkit_rt_struct_fini(rt_struct_t *value)
{
    if (value->type->fini != NULL) {
        (void)value->type->fini(value->fields);
    }
    return 0;
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
            TRACEC("%ld ",
                   mrklkit_rt_get_struct_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf ",
                   mrklkit_rt_get_struct_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_get_struct_item_str(value, it.iter, NULL);
                TRACEC("'%s' ", v != NULL ? v->data : NULL);
            }
            break;

        case LKIT_BOOL:
            TRACEC("%s ",
                   (int8_t)mrklkit_rt_get_struct_item_bool(
                       value, it.iter, 0) ? "#t" : "#f");
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_dump(
                mrklkit_rt_get_struct_item_array(value, it.iter, NULL));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_dump(
                mrklkit_rt_get_struct_item_dict(value, it.iter, NULL));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_dump(
                mrklkit_rt_get_struct_item_struct(value, it.iter, NULL));
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
            TRACEC("%ld",
                   mrklkit_rt_get_struct_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf",
                   mrklkit_rt_get_struct_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_get_struct_item_str(value, it.iter, NULL);
                TRACEC("%s", v != NULL ? v->data : (unsigned char *)"");
            }
            break;

        case LKIT_BOOL:
            TRACEC("%hhd", mrklkit_rt_get_struct_item_bool(value, it.iter, 0));
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_print(
                mrklkit_rt_get_struct_item_array(value, it.iter, NULL));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_print(
                mrklkit_rt_get_struct_item_dict(value, it.iter, NULL));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_print(
                mrklkit_rt_get_struct_item_struct(value, it.iter, NULL));
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
mrklkit_rt_get_struct_item_int(rt_struct_t *value,
                               int64_t idx,
                               UNUSED int64_t dflt)
{
    int64_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int64_t *)(value->fields + idx);
    return *v;
}


double
mrklkit_rt_get_struct_item_float(rt_struct_t *value,
                                 int64_t idx,
                                 UNUSED double dflt)
{
    union {
        void **v;
        double *d;
    } res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res.v = value->fields + idx;
    return *res.d;
}


int8_t
mrklkit_rt_get_struct_item_bool(rt_struct_t *value,
                                int64_t idx,
                                UNUSED int8_t dflt)
{
    int8_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int8_t *)((intptr_t *)(value->fields + idx));
    return *v;
}


bytes_t *
mrklkit_rt_get_struct_item_str(rt_struct_t *value,
                               int64_t idx,
                               bytes_t *dflt)
{
    bytes_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (bytes_t **)(value->fields + idx);
    if (*res == NULL) {
        *res = dflt;
    }
    return *res;
}


rt_array_t *
mrklkit_rt_get_struct_item_array(rt_struct_t *value,
                                 int64_t idx,
                                 rt_array_t *dflt)
{
    rt_array_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_array_t **)(value->fields + idx);
    if (*res == NULL) {
        *res = dflt;
    }
    return *res;
}


rt_dict_t *
mrklkit_rt_get_struct_item_dict(rt_struct_t *value,
                                int64_t idx,
                                rt_dict_t *dflt)
{
    rt_dict_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_dict_t **)(value->fields + idx);
    if (*res == NULL) {
        *res = dflt;
    }
    return *res;
}


rt_struct_t *
mrklkit_rt_get_struct_item_struct(rt_struct_t *value,
                                  int64_t idx,
                                  rt_struct_t *dflt)
{
    rt_struct_t **res;

    assert(idx < (ssize_t)value->type->fields.elnum);
    res = (rt_struct_t **)(value->fields + idx);
    if (*res == NULL) {
        *res = dflt;
    }
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
mrklkit_rt_set_struct_item_str_unsafe(rt_struct_t *value,
                                      int64_t idx,
                                      bytes_t *val)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    *p = val;
    BYTES_INCREF(*p);
}


void
mrklkit_rt_set_struct_item_str(rt_struct_t *value, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    BYTES_DECREF(p);
    *p = val;
    BYTES_INCREF(*p);
}


void
mrklkit_rt_set_struct_item_array(rt_struct_t *value,
                                 int64_t idx,
                                 rt_array_t *val)
{
    rt_array_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_array_t **)(value->fields + idx);
    ARRAY_DECREF(p);
    *p = val;
    ARRAY_INCREF(*p);
}


void
mrklkit_rt_set_struct_item_dict(rt_struct_t *value, int64_t idx, rt_dict_t *val)
{
    rt_dict_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_dict_t **)(value->fields + idx);
    DICT_DECREF(p);
    *p = val;
    DICT_INCREF(*p);
}


void
mrklkit_rt_set_struct_item_struct(rt_struct_t *value,
                                  int64_t idx,
                                  rt_struct_t *val)
{
    rt_struct_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_struct_t **)(value->fields + idx);
    STRUCT_DECREF(p);
    *p = val;
    STRUCT_INCREF(*p);
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


#define MRKLKIT_RT_STRUCT_DEEP_COPY_BODY(bytes_new_fn)                 \
    lkit_type_t **fty;                                                 \
    array_iter_t it;                                                   \
    assert(dst->type->fields.elnum == src->type->fields.elnum);        \
    for (fty = array_first(&dst->type->fields, &it);                   \
         fty != NULL;                                                  \
         fty = array_next(&dst->type->fields, &it)) {                  \
        switch ((*fty)->tag) {                                         \
        case LKIT_STR:                                                 \
            {                                                          \
                bytes_t *a, *b;                                        \
                a = (bytes_t *)*(src->fields + it.iter);               \
                if (a != NULL) {                                       \
                    b = bytes_new_fn(a->sz);                           \
                    bytes_copy(b, a, 0);                               \
                } else {                                               \
                    b = bytes_new_fn(1);                               \
                    b->data[0] = '\0';                                 \
                }                                                      \
                *((bytes_t **)dst->fields + it.iter) = b;              \
                BYTES_INCREF(b);                                       \
            }                                                          \
            break;                                                     \
        case LKIT_ARRAY:                                               \
            FAIL("mrklkit_rt_struct_deep_copy not implement array");   \
            break;                                                     \
        case LKIT_DICT:                                                \
            FAIL("mrklkit_rt_struct_deep_copy not implement dict");    \
            break;                                                     \
        case LKIT_STRUCT:                                              \
            FAIL("mrklkit_rt_struct_deep_copy not implement struct");  \
            break;                                                     \
        default:                                                       \
            *(dst->fields + it.iter) = *(src->fields + it.iter);       \
        }                                                              \
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
    res->nref = POISON_NREF;
    return res;
}


void
mrklkit_rt_struct_incref(rt_struct_t *s)
{
    if (s != NULL) {
        STRUCT_INCREF(s);
    }
}


/*
 * json support
 */

void
lruntime_dump_json(lkit_type_t *ty, void *value, bytestream_t *bs)
{
    switch (ty->tag) {
    case LKIT_INT:
    case LKIT_INT_MIN:
    case LKIT_INT_MAX:
        {
            union {
                void *v;
                int64_t i;
            } v;
            v.v = value;
            bytestream_nprintf(bs, 1024, "%ld", v.i);
        }
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        {
            union {
                void *v;
                double d;
            } v;
            v.v = value;
            bytestream_nprintf(bs, 1024, "%lf", v.d);
        }
        break;

    case LKIT_BOOL:
        {
            union {
                void *v;
                char c;
            } v;
            v.v = value;
            bytestream_nprintf(bs, 1024, "%s", v.c ? "true" : "false");
        }
        break;

    case LKIT_STR:
        {
            union {
                void *v;
                bytes_t *s;
            } v;
            v.v = value;
            bytestream_nprintf(bs,
                               1024,
                               "\"%s\"",
                               v.s != NULL ? (char *)v.s->data : "null");
        }
        break;

    case LKIT_ARRAY:
        {
            union {
                void *v;
                rt_array_t *a;
            } v;
            v.v = value;
            rt_array_dump_json(v.a, bs);
        }
        break;

    case LKIT_DICT:
        {
            union {
                void *v;
                rt_dict_t *d;
            } v;
            v.v = value;
            rt_dict_dump_json(v.d, bs);
        }
        break;

    case LKIT_STRUCT:
        {
            union {
                void *v;
                rt_struct_t *s;
            } v;
            v.v = value;
            rt_struct_dump_json(v.s, bs);
        }
        break;

    default:
        FAIL("lruntime_dump_json");

    }

}


static int
rt_array_dump_json_item(void **v, void *udata)
{
    struct {
        lkit_type_t *ty;
        bytestream_t *bs;
    } *params;
    params = udata;
    lruntime_dump_json(params->ty, *v, params->bs);
    bytestream_cat(params->bs, 1, ",");
    return 0;
}

void
rt_array_dump_json(rt_array_t *value, bytestream_t *bs)
{
    if (value == NULL) {
        bytestream_cat(bs, 4, "null");
    } else {
        off_t eod;
        struct {
            lkit_type_t *ty;
            bytestream_t *bs;
        } params;

        params.ty = lkit_array_get_element_type(value->type);
        params.bs = bs;
        bytestream_cat(bs, 1, "[");
        eod = SEOD(bs);
        (void)array_traverse(&value->fields,
                             (array_traverser_t)rt_array_dump_json_item,
                             &params);
        if (SEOD(bs) > eod) {
            SADVANCEEOD(bs, -1);
        }
        bytestream_cat(bs, 1, "]");
    }
}


static int
rt_dict_dump_json_item(void *key, void *value, void *udata)
{
    bytes_t *k;
    struct {
        lkit_type_t *ty;
        bytestream_t *bs;
    } *params;

    k = key;
    params = udata;
    bytestream_nprintf(params->bs, 1024, "\"%s\":", k->data);
    lruntime_dump_json(params->ty, value, params->bs);
    bytestream_cat(params->bs, 1, ",");
    return 0;
}


void
rt_dict_dump_json(rt_dict_t *value, bytestream_t *bs)
{
    if (value == NULL) {
        bytestream_cat(bs, 4, "null");
    } else {
        off_t eod;
        struct {
            lkit_type_t *ty;
            bytestream_t *bs;
        } params;

        params.ty = lkit_dict_get_element_type(value->type);
        params.bs = bs;
        bytestream_cat(bs, 1, "{");
        eod = SEOD(bs);
        (void)dict_traverse(&value->fields,
                            (dict_traverser_t)rt_dict_dump_json_item,
                            &params);
        if (SEOD(bs) > eod) {
            SADVANCEEOD(bs, -1);
        }
        bytestream_cat(bs, 1, "}");
    }
}


void
rt_struct_dump_json(rt_struct_t *value, bytestream_t *bs)
{
    if (value == NULL) {
        bytestream_cat(bs, 4, "null");
    } else {
        bytes_t **name;
        array_iter_t it;
        off_t eod;

        bytestream_cat(bs, 1, "{");
        eod = SEOD(bs);
        for (name = array_first(&value->type->names, &it);
             name != NULL;
             name = array_next(&value->type->names, &it)) {

            lkit_type_t **fty;
            void **v;

            fty = array_get(&value->type->fields, it.iter);
            bytestream_nprintf(bs, 1024, "\"%s\":", (*name)->data);
            v = MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, it.iter);
            lruntime_dump_json(*fty, *v, bs);
            bytestream_cat(bs, 1, ",");
        }
        if (SEOD(bs) > eod) {
            SADVANCEEOD(bs, -1);
        }
        bytestream_cat(bs, 1, "}");
    }
}









static int _rt_array_expect_item_int_cb(jparse_ctx_t *,
                                        jparse_value_t *,
                                        void *);
static int _rt_array_expect_item_float_cb(jparse_ctx_t *,
                                          jparse_value_t *,
                                          void *);
static int _rt_array_expect_item_str_cb(jparse_ctx_t *,
                                        jparse_value_t *,
                                        void *);
static int _rt_array_expect_item_bool_cb(jparse_ctx_t *,
                                         jparse_value_t *,
                                         void *);
static int _rt_array_expect_item_array_cb(jparse_ctx_t *,
                                          jparse_value_t *,
                                          void *);
static int _rt_array_expect_item_dict_cb(jparse_ctx_t *,
                                         jparse_value_t *,
                                         void *);
static int _rt_array_expect_item_struct_cb(jparse_ctx_t *,
                                           jparse_value_t *,
                                           void *);

static int _rt_dict_expect_item_int_cb(jparse_ctx_t *,
                                       jparse_value_t *,
                                       void *);
static int _rt_dict_expect_item_float_cb(jparse_ctx_t *,
                                         jparse_value_t *,
                                         void *);
static int _rt_dict_expect_item_str_cb(jparse_ctx_t *,
                                       jparse_value_t *,
                                       void *);
static int _rt_dict_expect_item_bool_cb(jparse_ctx_t *,
                                        jparse_value_t *,
                                        void *);
static int _rt_dict_expect_item_array_cb(jparse_ctx_t *,
                                         jparse_value_t *,
                                         void *);
static int _rt_dict_expect_item_dict_cb(jparse_ctx_t *,
                                        jparse_value_t *,
                                        void *);
static int _rt_dict_expect_item_struct_cb(jparse_ctx_t *,
                                          jparse_value_t *,
                                          void *);
static int _rt_struct_expect_fields_cb(jparse_ctx_t *,
                                       jparse_value_t *,
                                       void *);





static int
_rt_array_expect_item_int_cb(jparse_ctx_t *jctx,
                             UNUSED jparse_value_t *jval,
                             void *udata)
{
    rt_array_t *value;
    int64_t *v;

    value = udata;
    v = array_incr(&value->fields);
    return jparse_expect_item_int(jctx, (long *)v, NULL);
}


static int
_rt_array_expect_item_float_cb(jparse_ctx_t *jctx,
                               UNUSED jparse_value_t *jval,
                               void *udata)
{
    rt_array_t *value;
    double *v;

    value = udata;
    v = array_incr(&value->fields);
    return jparse_expect_item_float(jctx, v, NULL);
}


static int
_rt_array_expect_item_str_cb(jparse_ctx_t *jctx,
                             UNUSED jparse_value_t *jval,
                             void *udata)
{
    rt_array_t *value;
    bytes_t **v;

    value = udata;
    v = array_incr(&value->fields);
    return jparse_expect_item_str(jctx, v, NULL);
}


static int
_rt_array_expect_item_bool_cb(jparse_ctx_t *jctx,
                              UNUSED jparse_value_t *jval,
                              void *udata)
{
    rt_array_t *value;
    char *v;

    value = udata;
    v = array_incr(&value->fields);
    return jparse_expect_item_bool(jctx, v, NULL);
}


static int
_rt_array_expect_item_array_cb(jparse_ctx_t *jctx,
                               jparse_value_t *jval,
                               void *udata)
{
    rt_array_t *value;
    lkit_type_t *fty;
    lkit_array_t *ta;
    rt_array_t **v;
    jparse_expect_cb_t cb;

    value = udata;
    fty = lkit_array_get_element_type(value->type);
    assert(fty->tag == LKIT_ARRAY);
    ta = (lkit_array_t *)fty;
    v = array_incr(&value->fields);
    *v = mrklkit_rt_array_new(ta);
    ARRAY_INCREF(*v);
    fty = lkit_array_get_element_type(ta);

    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_array_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_array_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_array_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_array_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_array_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_array_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_array_expect_item_struct_cb;
        break;

    default:
        FAIL("_rt_array_expect_item_array_cb");
    }
    return jparse_expect_item_array_iter(jctx, cb, jval, *v);
}


static int
_rt_array_expect_item_dict_cb(jparse_ctx_t *jctx,
                              jparse_value_t *jval,
                              void *udata)
{
    rt_array_t *value;
    lkit_type_t *fty;
    lkit_dict_t *td;
    rt_dict_t **v;
    jparse_expect_cb_t cb;

    value = udata;
    fty = lkit_array_get_element_type(value->type);
    assert(fty->tag == LKIT_DICT);
    td = (lkit_dict_t *)fty;
    v = array_incr(&value->fields);
    *v = mrklkit_rt_dict_new(td);
    DICT_INCREF(*v);
    fty = lkit_dict_get_element_type(td);

    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_dict_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_dict_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_dict_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_dict_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_dict_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_dict_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_dict_expect_item_struct_cb;
        break;

    default:
        FAIL("_rt_array_expect_item_dict_cb");
    }
    return jparse_expect_item_object_iter(jctx, cb, jval, *v);
}


static int
_rt_array_expect_item_struct_cb(jparse_ctx_t *jctx,
                                jparse_value_t *jval,
                                void *udata)
{
    rt_array_t *value;
    lkit_type_t *fty;
    lkit_struct_t *ts;
    rt_struct_t **v;

    value = udata;
    fty = lkit_array_get_element_type(value->type);
    assert(fty->tag == LKIT_STRUCT);
    ts = (lkit_struct_t *)fty;
    v = array_incr(&value->fields);
    *v = mrklkit_rt_struct_new(ts);
    STRUCT_INCREF(*v);
    return jparse_expect_item_object(jctx,
                                     _rt_struct_expect_fields_cb,
                                     jval,
                                     *v);
}


int
rt_array_load_json(rt_array_t *value, jparse_ctx_t *jctx)
{
    lkit_type_t *fty;
    jparse_expect_cb_t cb = NULL;
    jparse_value_t jval;

    fty = lkit_array_get_element_type(value->type);
    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_array_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_array_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_array_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_array_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_array_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_array_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_array_expect_item_struct_cb;
        break;

    default:
        FAIL("rt_array_load_json");
    }

    return jparse_expect_array_iter(jctx, cb, &jval, value);
}





static int
_rt_dict_expect_item_int_cb(jparse_ctx_t *jctx,
                            UNUSED jparse_value_t *jval,
                            void *udata)
{
    int res;
    rt_dict_t *value;
    bytes_t *k;
    union {
        long i;
        void *v;
    } v;

    value = udata;
    if ((res = jparse_expect_anykvp_int(jctx, &k, &v.i, NULL)) != 0) {
        return res;
    }

    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    dict_set_item(&value->fields, k, v.v);
    return 0;
}


static int
_rt_dict_expect_item_float_cb(jparse_ctx_t *jctx,
                              UNUSED jparse_value_t *jval,
                              void *udata)
{
    int res;
    rt_dict_t *value;
    bytes_t *k;
    union {
        double f;
        void *v;
    } v;

    value = udata;
    if ((res = jparse_expect_anykvp_float(jctx, &k, &v.f, NULL)) != 0) {
        return res;
    }

    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    dict_set_item(&value->fields, k, v.v);
    return 0;
}


static int
_rt_dict_expect_item_str_cb(jparse_ctx_t *jctx,
                            UNUSED jparse_value_t *jval,
                            void *udata)
{
    int res;
    rt_dict_t *value;
    bytes_t *k;
    union {
        bytes_t *s;
        void *v;
    } v;

    value = udata;
    if ((res = jparse_expect_anykvp_str(jctx, &k, &v.s, NULL)) != 0) {
        return res;
    }

    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    v.s = bytes_new_from_bytes(v.s);
    BYTES_INCREF(v.s);
    dict_set_item(&value->fields, k, v.v);
    return 0;
}


static int
_rt_dict_expect_item_bool_cb(jparse_ctx_t *jctx,
                             UNUSED jparse_value_t *jval,
                             void *udata)
{
    int res;
    rt_dict_t *value;
    bytes_t *k;
    union {
        char b;
        void *v;
    } v;

    value = udata;
    if ((res = jparse_expect_anykvp_bool(jctx, &k, &v.b, NULL)) != 0) {
        return res;
    }

    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    dict_set_item(&value->fields, k, v.v);
    return 0;
}


static int
_rt_dict_expect_item_array_cb(jparse_ctx_t *jctx,
                              jparse_value_t *jval,
                              void *udata)
{
    int res;
    rt_dict_t *value;
    lkit_type_t *fty;
    lkit_array_t *ta;
    jparse_expect_cb_t cb;
    bytes_t *k;
    rt_array_t *v;

    value = udata;
    fty = lkit_dict_get_element_type(value->type);
    assert(fty->tag == LKIT_ARRAY);
    ta = (lkit_array_t *)fty;
    fty = lkit_array_get_element_type(ta);

    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_array_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_array_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_array_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_array_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_array_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_array_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_array_expect_item_struct_cb;
        break;

    default:
        FAIL("_rt_dict_expect_item_array_cb");
    }

    v = mrklkit_rt_array_new(ta);
    res = jparse_expect_anykvp_array_iter(jctx, &k, cb, jval, v);
    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    ARRAY_INCREF(v);
    dict_set_item(&value->fields, k, v);
    return res;
}


static int
_rt_dict_expect_item_dict_cb(jparse_ctx_t *jctx,
                             jparse_value_t *jval,
                             void *udata)
{
    int res;
    rt_dict_t *value;
    lkit_type_t *fty;
    lkit_dict_t *td;
    jparse_expect_cb_t cb;
    bytes_t *k;
    rt_dict_t *v;

    value = udata;
    fty = lkit_dict_get_element_type(value->type);
    assert(fty->tag == LKIT_DICT);
    td = (lkit_dict_t *)fty;
    fty = lkit_dict_get_element_type(td);

    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_dict_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_dict_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_dict_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_dict_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_dict_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_dict_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_dict_expect_item_struct_cb;
        break;

    default:
        FAIL("_rt_dict_expect_item_dict_cb");
    }

    v = mrklkit_rt_dict_new(td);
    res = jparse_expect_anykvp_object_iter(jctx, &k, cb, jval, v);
    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    dict_set_item(&value->fields, k, v);
    return res;
}


static int
_rt_dict_expect_item_struct_cb(jparse_ctx_t *jctx,
                               jparse_value_t *jval,
                               void *udata)
{
    int res;
    rt_dict_t *value;
    lkit_type_t *fty;
    lkit_struct_t *ts;
    bytes_t *k;
    rt_struct_t *v;

    value = udata;
    fty = lkit_dict_get_element_type(value->type);
    assert(fty->tag == LKIT_STRUCT);
    ts = (lkit_struct_t *)fty;
    v = mrklkit_rt_struct_new(ts);
    res = jparse_expect_anykvp_object(jctx,
                                      &k,
                                      _rt_struct_expect_fields_cb,
                                      jval,
                                      v);
    k = bytes_new_from_bytes(k);
    BYTES_INCREF(k);
    STRUCT_INCREF(v);
    dict_set_item(&value->fields, k, v);
    return res;
}


int
rt_dict_load_json(rt_dict_t *value, jparse_ctx_t *jctx)
{
    lkit_type_t *fty;
    jparse_expect_cb_t cb;
    jparse_value_t jval;

    fty = lkit_dict_get_element_type(value->type);
    switch (fty->tag) {
    case LKIT_INT:
        cb = _rt_dict_expect_item_int_cb;
        break;

    case LKIT_FLOAT:
        cb = _rt_dict_expect_item_float_cb;
        break;

    case LKIT_STR:
        cb = _rt_dict_expect_item_str_cb;
        break;

    case LKIT_BOOL:
        cb = _rt_dict_expect_item_bool_cb;
        break;

    case LKIT_ARRAY:
        cb = _rt_dict_expect_item_array_cb;
        break;

    case LKIT_DICT:
        cb = _rt_dict_expect_item_dict_cb;
        break;

    case LKIT_STRUCT:
        cb = _rt_dict_expect_item_struct_cb;
        break;

    default:
        FAIL("rt_dict_load_json");
    }
    return jparse_expect_object_iter(jctx, cb, &jval, value);
}


static int
_rt_struct_expect_fields_cb(jparse_ctx_t *jctx,
                            jparse_value_t *jval,
                            void *udata)
{
    int res;
    rt_struct_t *value;
    bytes_t **name;
    array_iter_t it;

    value = udata;
    res = 0;
    for (name = array_first(&value->type->names, &it);
         name != NULL;
         name = array_next(&value->type->names, &it)) {
        lkit_type_t **fty;
        void **v;

        fty = array_get(&value->type->fields, it.iter);
        v = MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, it.iter);
        switch ((*fty)->tag) {
        case LKIT_INT:
            res = jparse_expect_kvp_int(jctx, *name, (long *)v, NULL);
            break;

        case LKIT_FLOAT:
            res = jparse_expect_kvp_float(jctx, *name, (double *)v, NULL);
            break;

        case LKIT_BOOL:
            res = jparse_expect_kvp_bool(jctx, *name, (char *)v, NULL);
            break;

        case LKIT_STR:
            res = jparse_expect_kvp_str(jctx, *name, (bytes_t **)v, NULL);
            break;


        case LKIT_ARRAY:
            {
                rt_array_t *ary;
                lkit_array_t *ta;
                lkit_type_t *afty;
                jparse_expect_cb_t cb;

                ta = (lkit_array_t *)(*fty);
                ary = mrklkit_rt_array_new(ta);
                afty = lkit_array_get_element_type(ta);
                switch (afty->tag) {
                    case LKIT_INT:
                        cb = _rt_array_expect_item_int_cb;
                        break;

                    case LKIT_FLOAT:
                        cb = _rt_array_expect_item_float_cb;
                        break;

                    case LKIT_STR:
                        cb = _rt_array_expect_item_str_cb;
                        break;

                    case LKIT_BOOL:
                        cb = _rt_array_expect_item_bool_cb;
                        break;

                    case LKIT_ARRAY:
                        cb = _rt_array_expect_item_array_cb;
                        break;

                    case LKIT_DICT:
                        cb = _rt_array_expect_item_dict_cb;
                        break;

                    case LKIT_STRUCT:
                        cb = _rt_array_expect_item_struct_cb;
                        break;

                    default:
                        FAIL("_rt_struct_expect_fields_cb");

                }
                res = jparse_expect_kvp_array_iter(jctx, *name, cb, jval, ary);
                ARRAY_INCREF(ary);
                *v = ary;
            }
            break;

        case LKIT_DICT:
            {
                rt_dict_t *dict;
                lkit_dict_t *td;
                lkit_type_t *dfty;
                jparse_expect_cb_t cb;

                td = (lkit_dict_t *)(*fty);
                dict = mrklkit_rt_dict_new(td);
                dfty = lkit_dict_get_element_type(td);
                switch (dfty->tag) {
                    case LKIT_INT:
                        cb = _rt_dict_expect_item_int_cb;
                        break;

                    case LKIT_FLOAT:
                        cb = _rt_dict_expect_item_float_cb;
                        break;

                    case LKIT_STR:
                        cb = _rt_dict_expect_item_str_cb;
                        break;

                    case LKIT_BOOL:
                        cb = _rt_dict_expect_item_bool_cb;
                        break;

                    case LKIT_ARRAY:
                        cb = _rt_dict_expect_item_array_cb;
                        break;

                    case LKIT_DICT:
                        cb = _rt_dict_expect_item_dict_cb;
                        break;

                    case LKIT_STRUCT:
                        cb = _rt_dict_expect_item_struct_cb;
                        break;

                    default:
                        FAIL("_rt_struct_expect_fields_cb");

                }
                res = jparse_expect_kvp_object_iter(jctx,
                                                    *name,
                                                    cb,
                                                    jval,
                                                    dict);
                DICT_INCREF(dict);
                *v = dict;
            }
            break;

        case LKIT_STRUCT:
            {
                rt_struct_t *st;
                lkit_struct_t *ts;

                ts = (lkit_struct_t *)(*fty);
                st = mrklkit_rt_struct_new(ts);
                res = jparse_expect_kvp_object(jctx,
                                               *name,
                                               _rt_struct_expect_fields_cb,
                                               NULL,
                                               st);
                STRUCT_INCREF(st);
                *v = st;
            }
            break;


        default:
            lkit_type_dump(*fty);
            FAIL("rt_struct_load_field_json");
        }
        if (res != 0) {
            break;
        }
    }
    return res;

}


int
rt_struct_load_json(rt_struct_t *value, jparse_ctx_t *jctx)
{
    return jparse_expect_object(jctx, _rt_struct_expect_fields_cb, NULL, value);
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
