#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream_aux.h>
#include <mrkcommon/hash.h>
#include <mrkcommon/util.h>
//#define TRRET_DEBUG
#include <mrkcommon/dumpm.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/jparse.h>

#include <mrklkit/dparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(lruntime);

#define MEMDEBUG_INIT(self)                                    \
do {                                                           \
    (self)->mdtag = (uint64_t)memdebug_get_runtime_scope();    \
} while (0)                                                    \


#define MEMDEBUG_ENTER(self)                                   \
{                                                              \
    int mdtag;                                                 \
    mdtag = memdebug_set_runtime_scope((int)(self)->mdtag);    \


#define MEMDEBUG_LEAVE(self)                   \
    (void)memdebug_set_runtime_scope(mdtag);   \
}                                              \


#else
#define MEMDEBUG_INIT(self)
#define MEMDEBUG_ENTER(self)
#define MEMDEBUG_LEAVE(self)
#endif

static mpool_ctx_t *mpool;


#define POISON_NREF 0x11110000


#define _bytes_new_from_str(s) bytes_new_from_str_mpool(mpool, (s))


#define _bytes_new(sz) bytes_new_mpool(mpool, (sz))


#define _malloc(sz) mpool_malloc(mpool, (sz))


#define _array_ensure_datasz(ar,                               \
                             datasz,                           \
                             flags)                            \
    array_ensure_datasz_mpool(mpool, (ar), (datasz), (flags))  \


#define _array_incr(a) array_incr_mpool(mpool, (a))


#define _dict_init(dict,                                               \
                   sz,                                                 \
                   hashfn,                                             \
                   cmpfn,                                              \
                   finifn)                                             \
    hash_init_mpool(mpool, (dict), (sz), (hashfn), (cmpfn), (finifn))  \


#define _hash_set_item(d, k, v) hash_set_item_mpool(mpool, (d), (k), (v))


void
mrklkit_rt_debug(void *v)
{
    TRACE("v=%p", v);
}


int
mrklkit_rt_strcmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

/**
 * str
 */
#define BYTES_NEW_BODY(new_fn, a, __a1)\
    bytes_t *res;                      \
    res = new_fn(a) ;                  \
    __a1                               \
    return res;                        \


bytes_t *
mrklkit_rt_bytes_new(size_t sz)
{
    BYTES_NEW_BODY(bytes_new, sz,)
}


bytes_t *
mrklkit_rt_bytes_new_mpool(size_t sz)
{
    BYTES_NEW_BODY(_bytes_new, sz, res->nref = POISON_NREF;)
}


bytes_t *
mrklkit_rt_bytes_new_from_str(const char *s)
{
    BYTES_NEW_BODY(bytes_new_from_str, s,)
}


bytes_t *
mrklkit_rt_bytes_new_from_str_mpool(const char *s)
{
    BYTES_NEW_BODY(_bytes_new_from_str, s, res->nref = POISON_NREF;)
}


#define BYTES_NEW_FROM_TY_BODY(new_fn, a, f, __a1)     \
    char buf[1024];                                    \
    bytes_t *res;                                      \
    (void)snprintf(buf, sizeof(buf), f, a);            \
    res = new_fn(buf);                                 \
    __a1                                               \
    return res;                                        \


bytes_t *
mrklkit_rt_bytes_new_from_int(int64_t i)
{
    BYTES_NEW_FROM_TY_BODY(bytes_new_from_str, i, "%ld",)
}


bytes_t *
mrklkit_rt_bytes_new_from_int_mpool(int64_t i)
{
    BYTES_NEW_FROM_TY_BODY(_bytes_new_from_str,
                           i,
                           "%ld",
                           res->nref = POISON_NREF;)
}


bytes_t *
mrklkit_rt_bytes_new_from_float(double f)
{
    BYTES_NEW_FROM_TY_BODY(bytes_new_from_str, f, "%lf",)
}


bytes_t *
mrklkit_rt_bytes_new_from_float_mpool(double f)
{
    BYTES_NEW_FROM_TY_BODY(_bytes_new_from_str,
                           f,
                           "%lf",
                           res->nref = POISON_NREF;)
}


bytes_t *
mrklkit_rt_bytes_new_from_bool(char b)
{
    BYTES_NEW_FROM_TY_BODY(bytes_new_from_str, b ? "true" : "false", "%s",)
}


bytes_t *
mrklkit_rt_bytes_new_from_bool_mpool(char b)
{
    BYTES_NEW_FROM_TY_BODY(_bytes_new_from_str,
                           b ? "true" : "false",
                           "%s",
                           res->nref = POISON_NREF;)
}


#define BYTES_SLICE_BODY(new_fn, __a1)         \
    bytes_t *res;                              \
    ssize_t sz0, sz1;                          \
    assert(str->sz > 0);                       \
    sz0 = str->sz - 1; /* cut off zero-term */ \
    if (sz0 == 0) {                            \
        goto empty;                            \
    }                                          \
    begin = (begin + sz0) % sz0;               \
    end = (end + sz0) % sz0;                   \
    sz1 = end - begin;                         \
    if (sz1 < 0) {                             \
        goto empty;                            \
    }                                          \
    ++sz1; /* "end" including the last char */ \
    res = new_fn(sz1 + 1);                     \
    __a1                                       \
    memcpy(res->data, str->data + begin, sz1); \
    res->data[sz1] = '\0';                     \
end:                                           \
    return res;                                \
empty:                                         \
    res = new_fn(1);                           \
    __a1                                       \
    res->data[0] = '\0';                       \
    goto end;                                  \


bytes_t *
mrklkit_rt_bytes_slice(bytes_t *str, int64_t begin, int64_t end)
{
    BYTES_SLICE_BODY(bytes_new,)
}


bytes_t *
mrklkit_rt_bytes_slice_mpool(bytes_t *str, int64_t begin, int64_t end)
{
    BYTES_SLICE_BODY(_bytes_new, res->nref = POISON_NREF;)
}


bytes_t *
mrklkit_rt_bytes_brushdown_mpool(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, (char *)str->data);
    res->nref = POISON_NREF;
    bytes_brushdown(res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_brushdown(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str((char *)str->data);
    bytes_brushdown(res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_urldecode_mpool(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str_mpool(mpool, (char *)str->data);
    res->nref = POISON_NREF;
    bytes_urldecode(res);
    return res;
}


bytes_t *
mrklkit_rt_bytes_urldecode(bytes_t *str)
{
    bytes_t *res;

    res = bytes_new_from_str((char *)str->data);
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
mrklkit_rt_bytes_decref(bytes_t **value)
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
                   mrklkit_rt_array_get_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf ",
                   mrklkit_rt_array_get_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *val;

                val = mrklkit_rt_array_get_item_str(value, it.iter, NULL);
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
                   mrklkit_rt_array_get_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf",
                   mrklkit_rt_array_get_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *val;

                val = mrklkit_rt_array_get_item_str(value, it.iter, NULL);
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


#define MRKLKIT_RT_ARRAY_NEW_BODY(malloc_fn,                                   \
                                  array_ensure_datasz_fn,                      \
                                  nreserved,                                   \
                                  ref)                                         \
    rt_array_t *res;                                                           \
                                                                               \
    if ((res = malloc_fn(sizeof(rt_array_t))) == NULL) {                       \
        FAIL("malloc");                                                        \
    }                                                                          \
    MEMDEBUG_INIT(res);                                                        \
    MEMDEBUG_ENTER(res);                                                       \
    res->nref = ref;                                                           \
    res->type = ty;                                                            \
    array_init(&res->fields,                                                   \
               sizeof(void *),                                                 \
               0,                                                              \
               (array_initializer_t)null_init,                                 \
               ty->fini);                                                      \
    if (nreserved > 0) {                                                       \
        array_ensure_datasz_fn(&res->fields, nreserved * sizeof(void *), 0);   \
    }                                                                          \
    MEMDEBUG_LEAVE(res);                                                       \


rt_array_t *
mrklkit_rt_array_new(lkit_array_t *ty)
{
    MRKLKIT_RT_ARRAY_NEW_BODY(malloc, array_ensure_datasz, 0, 0);
    return res;
}


rt_array_t *
mrklkit_rt_array_new_mpool(lkit_array_t *ty)
{
    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc, _array_ensure_datasz, 0, POISON_NREF);
    return res;
}


rt_array_t *
mrklkit_rt_array_new_mpool_sz(lkit_array_t *ty, int nreserved)
{
    MRKLKIT_RT_ARRAY_NEW_BODY(_malloc,
                              _array_ensure_datasz,
                              nreserved,
                              POISON_NREF);
    return res;
}


void
mrklkit_rt_array_decref(rt_array_t **value)
{
    ARRAY_DECREF(value);
}


int64_t
mrklkit_rt_array_get_item_int(rt_array_t *value, int64_t idx, int64_t dflt)
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
mrklkit_rt_array_get_item_float(rt_array_t *value, int64_t idx, double dflt)
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
mrklkit_rt_array_get_item_str(rt_array_t *value, int64_t idx, bytes_t *dflt)
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


#define ARRAY_SPLIT_BODY(array_new_fn, array_incr_fn, bytes_new_fn)    \
    rt_array_t *res;                                                   \
    char ch;                                                           \
    char *s0, *s1;                                                     \
    res = array_new_fn(ty);                                            \
    ch = delim->data[0];                                               \
    for (s0 = (char *)str->data, s1 = s0;                              \
         s0 < (char *)str->data + str->sz;                             \
         ++s1) {                                                       \
        if (*s1 == ch) {                                               \
            bytes_t **item;                                            \
            size_t sz;                                                 \
                                                                       \
            if ((item = array_incr_fn(&res->fields)) == NULL) {        \
                FAIL(#array_incr_fn);                                  \
            }                                                          \
            sz = s1 - s0;                                              \
            *item = bytes_new_fn(sz + 1);                              \
            memcpy((*item)->data, s0, sz);                             \
            (*item)->data[sz] = '\0';                                  \
            s0 = s1 + 1;                                               \
        }                                                              \
    }                                                                  \
    return res;                                                        \


rt_array_t *
mrklkit_rt_array_split(lkit_array_t *ty, bytes_t *str, bytes_t *delim)
{
    ARRAY_SPLIT_BODY(mrklkit_rt_array_new,
                     array_incr,
                     mrklkit_rt_bytes_new)
}


rt_array_t *
mrklkit_rt_array_split_mpool(lkit_array_t *ty, bytes_t *str, bytes_t *delim)
{
    ARRAY_SPLIT_BODY(mrklkit_rt_array_new_mpool,
                     _array_incr,
                     mrklkit_rt_bytes_new_mpool)
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
array_traverse_cb(void **i, void *udata)
{
    struct {
        void (*cb)(void *);
    } *params = udata;
    params->cb(*i);
    return 0;
}

void
mrklkit_rt_array_traverse(rt_array_t *a, void (*cb)(void *))
{
    struct {
        void (*cb)(void *);
    } params;

    params.cb = cb;
    (void)array_traverse(&a->fields,
                         (array_traverser_t)array_traverse_cb,
                         &params);
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


static int
dump_struct_dict(bytes_t *k, rt_struct_t *v, UNUSED void *udata)
{
    TRACEC("%s: ", k->data);
    mrklkit_rt_struct_dump(v);
    return 0;
}


void
mrklkit_rt_dict_dump(rt_dict_t *value)
{

    if (value == NULL) {
        TRACEC("{null}");
    } else {
        lkit_type_t *fty;

        fty = lkit_dict_get_element_type(value->type);

        TRACEC("{ ");
        switch (fty->tag) {
        case LKIT_INT:
        case LKIT_INT_MIN:
        case LKIT_INT_MAX:
            hash_traverse(&value->fields,
                          (hash_traverser_t)dump_int_dict,
                          NULL);
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            hash_traverse(&value->fields,
                          (hash_traverser_t)dump_float_dict,
                          NULL);
            break;

        case LKIT_STR:
            hash_traverse(&value->fields,
                          (hash_traverser_t)dump_bytes_dict,
                          NULL);
            break;

        case LKIT_STRUCT:
            hash_traverse(&value->fields,
                          (hash_traverser_t)dump_struct_dict,
                          NULL);
            break;


        default:
            FAIL("mrklkit_rt_dict_dump");
        }
        TRACEC("} ");
    }
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
        hash_traverse(&value->fields,
                      (hash_traverser_t)dump_int_dict,
                      NULL);
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        hash_traverse(&value->fields,
                      (hash_traverser_t)dump_float_dict,
                      NULL);
        break;

    case LKIT_STR:
        hash_traverse(&value->fields,
                      (hash_traverser_t)dump_bytes_dict,
                      NULL);
        break;

    default:
        FAIL("mrklkit_rt_dict_print");
    }
}


#define MRKLKIT_RT_DICT_NEW_BODY(malloc_fn,            \
                                 hash_init_fn,         \
                                 nreserved,            \
                                 ref)                  \
    rt_dict_t *res;                                    \
    if ((res = malloc_fn(sizeof(rt_dict_t))) == NULL) {\
        FAIL("malloc_fn");                             \
    }                                                  \
    MEMDEBUG_INIT(res);                                \
    MEMDEBUG_ENTER(res);                               \
    res->nref = ref;                                   \
    res->type = ty;                                    \
    hash_init_fn(&res->fields,                         \
              nreserved,                               \
              (hash_hashfn_t)bytes_hash,               \
              (hash_item_comparator_t)bytes_cmp,       \
              ty->fini);                               \
    MEMDEBUG_LEAVE(res);                               \


rt_dict_t *
mrklkit_rt_dict_new(lkit_dict_t *ty)
{
    MRKLKIT_RT_DICT_NEW_BODY(malloc, hash_init, 17, 0);
    return res;
}


rt_dict_t *
mrklkit_rt_dict_new_sz(lkit_dict_t *ty, int nreserved)
{
    MRKLKIT_RT_DICT_NEW_BODY(malloc, hash_init, nreserved, 0);
    return res;
}


rt_dict_t *
mrklkit_rt_dict_new_mpool(lkit_dict_t *ty)
{
    MRKLKIT_RT_DICT_NEW_BODY(_malloc, _dict_init, 17, POISON_NREF);
    return res;
}


rt_dict_t *
mrklkit_rt_dict_new_mpool_sz(lkit_dict_t *ty, int nreserved)
{
    MRKLKIT_RT_DICT_NEW_BODY(_malloc, _dict_init, nreserved, POISON_NREF);
    return res;
}


void
mrklkit_rt_dict_decref(rt_dict_t **value)
{
    DICT_DECREF(value);
}


int64_t
mrklkit_rt_dict_get_item_int(rt_dict_t *value, bytes_t *key, int64_t dflt)
{
    hash_item_t *it;
    union {
        void *v;
        int64_t i;
    } res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res.i = dflt;
    } else {
        res.v = it->value;
    }
    return res.i;
}


double
mrklkit_rt_dict_get_item_float(rt_dict_t *value, bytes_t *key, double dflt)
{
    hash_item_t *it;
    union {
        void *v;
        double d;
    } res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res.d = dflt;
    } else {
        res.v = it->value;
    }
    return res.d;
}


bytes_t *
mrklkit_rt_dict_get_item_str(rt_dict_t *value, bytes_t *key, bytes_t *dflt)
{
    hash_item_t *it;
    bytes_t *res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    } else {
        res = it->value == NULL ? dflt : it->value;
    }
    return res;
}


rt_array_t *
mrklkit_rt_dict_get_item_array(rt_dict_t *value,
                                bytes_t *key,
                                rt_array_t *dflt)
{
    hash_item_t *it;
    rt_array_t *res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    } else {
        res = it->value == NULL ? dflt : it->value;
    }
    return res;
}


rt_struct_t *
mrklkit_rt_dict_get_item_struct(rt_dict_t *value,
                                bytes_t *key,
                                rt_struct_t *dflt)
{
    hash_item_t *it;
    rt_struct_t *res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    } else {
        res = it->value == NULL ? dflt : it->value;
    }
    return res;
}


rt_dict_t *
mrklkit_rt_dict_get_item_dict(rt_dict_t *value,
                              bytes_t *key,
                              rt_dict_t *dflt)
{
    hash_item_t *it;
    rt_dict_t *res;

    if ((it = hash_get_item(&value->fields, key)) == NULL) {
        res = dflt;
    } else {
        res = it->value == NULL ? dflt : it->value;
    }
    return res;
}


int64_t
mrklkit_rt_dict_has_item(rt_dict_t *value, bytes_t *key)
{
    return hash_get_item(&value->fields, key) != NULL;
}


void
mrklkit_rt_dict_incref(rt_dict_t *d)
{
    if (d != NULL) {
        DICT_INCREF(d);
    }
}


#define MRKLKI_RT_DICT_SET_ITEM_INT_BODY(hash_set_item_fn)     \
    union {                                                    \
        int64_t i;                                             \
        void *v;                                               \
    } v;                                                       \
    v.i = val;                                                 \
    BYTES_INCREF(key);                                         \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_int(rt_dict_t *value, bytes_t *key, int64_t val)
{
    MRKLKI_RT_DICT_SET_ITEM_INT_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_int_mpool(rt_dict_t *value, bytes_t *key, int64_t val)
{
    MRKLKI_RT_DICT_SET_ITEM_INT_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_FLOAT_BODY(hash_set_item_fn)   \
    union {                                                    \
        double f;                                              \
        void *v;                                               \
    } v;                                                       \
    v.f = val;                                                 \
    BYTES_INCREF(key);                                         \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_float(rt_dict_t *value, bytes_t *key, double val)
{
    MRKLKI_RT_DICT_SET_ITEM_FLOAT_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_float_mpool(rt_dict_t *value, bytes_t *key, double val)
{
    MRKLKI_RT_DICT_SET_ITEM_FLOAT_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_BOOL_BODY(hash_set_item_fn)    \
    union {                                                    \
        char b;                                                \
        void *v;                                               \
    } v;                                                       \
    v.b = val;                                                 \
    BYTES_INCREF(key);                                         \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_bool(rt_dict_t *value, bytes_t *key, char val)
{
    MRKLKI_RT_DICT_SET_ITEM_BOOL_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_bool_mpool(rt_dict_t *value, bytes_t *key, char val)
{
    MRKLKI_RT_DICT_SET_ITEM_BOOL_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_STR_BODY(hash_set_item_fn)     \
    union {                                                    \
        bytes_t *s;                                            \
        void *v;                                               \
    } v;                                                       \
    v.s = val;                                                 \
    BYTES_INCREF(key);                                         \
    BYTES_INCREF(val);                                         \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_str(rt_dict_t *value, bytes_t *key, bytes_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_STR_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_str_mpool(rt_dict_t *value, bytes_t *key, bytes_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_STR_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_ARRAY_BODY(hash_set_item_fn)   \
    union {                                                    \
        rt_array_t *a;                                         \
        void *v;                                               \
    } v;                                                       \
    v.a = val;                                                 \
    BYTES_INCREF(key);                                         \
    ARRAY_INCREF(val);                                         \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_array(rt_dict_t *value, bytes_t *key, rt_array_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_ARRAY_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_array_mpool(rt_dict_t *value, bytes_t *key, rt_array_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_ARRAY_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_DICT_BODY(hash_set_item_fn)    \
    union {                                                    \
        rt_dict_t *d;                                          \
        void *v;                                               \
    } v;                                                       \
    v.d = val;                                                 \
    BYTES_INCREF(key);                                         \
    DICT_INCREF(val);                                          \
    hash_set_item_fn(&value->fields, key, v.v);                \


void
mrklkit_rt_dict_set_item_dict(rt_dict_t *value, bytes_t *key, rt_dict_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_DICT_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_dict_mpool(rt_dict_t *value, bytes_t *key, rt_dict_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_DICT_BODY(_hash_set_item)
}


#define MRKLKI_RT_DICT_SET_ITEM_STRUCT_BODY(hash_set_item_fn)  \
    union {                                                    \
        rt_struct_t *s;                                        \
        void *v;                                               \
    } v;                                                       \
    v.s = val;                                                 \
    BYTES_INCREF(key);                                         \
    STRUCT_INCREF(val);                                        \
    hash_set_item_fn(&value->fields, key, v.v);                \



void
mrklkit_rt_dict_set_item_struct(rt_dict_t *value,
                                bytes_t *key,
                                rt_struct_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_STRUCT_BODY(hash_set_item)
}


void
mrklkit_rt_dict_set_item_struct_mpool(rt_dict_t *value,
                                bytes_t *key,
                                rt_struct_t *val)
{
    MRKLKI_RT_DICT_SET_ITEM_STRUCT_BODY(_hash_set_item)
}


void
mrklkit_rt_dict_del_item_mpool(rt_dict_t *value,
                               bytes_t *key)
{
    hash_item_t *dit;

    if ((dit = hash_get_item(&value->fields, key)) != NULL) {
        hash_delete_pair_no_fini_mpool(mpool, &value->fields, dit);
    }

}


void
mrklkit_rt_dict_del_item(rt_dict_t *value,
                         bytes_t *key)
{
    hash_item_t *dit;

    if ((dit = hash_get_item(&value->fields, key)) != NULL) {
        hash_delete_pair(&value->fields, dit);
    }

}


static int
hash_traverse_cb(bytes_t *key, void *value, void *udata)
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
    (void)hash_traverse(&value->fields,
                        (hash_traverser_t)hash_traverse_cb,
                        &params);
}



/**
 * struct
 */

#define MRKLKIT_RT_STRUCT_NEW_BODY(malloc_fn, ref)             \
    rt_struct_t *v;                                            \
    size_t sz = ty->fields.elnum * sizeof(void *);             \
    if ((v = malloc_fn(sizeof(rt_struct_t) + sz)) == NULL) {   \
        FAIL("malloc");                                        \
    }                                                          \
    MEMDEBUG_INIT(v);                                          \
    MEMDEBUG_ENTER(v);                                         \
    v->nref = ref;                                             \
    v->type = ty;                                              \
    if (ty->init != NULL) {                                    \
        ty->init(v->fields);                                   \
    }                                                          \
    MEMDEBUG_LEAVE(v);                                         \
    return v;                                                  \


rt_struct_t *
mrklkit_rt_struct_new(lkit_struct_t *ty)
{
    MRKLKIT_RT_STRUCT_NEW_BODY(malloc, 0);
}


rt_struct_t *
mrklkit_rt_struct_new_mpool(lkit_struct_t *ty)
{
    MRKLKIT_RT_STRUCT_NEW_BODY(_malloc, POISON_NREF);
}


void
mrklkit_rt_struct_decref(rt_struct_t **value)
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
mrklkit_rt_struct_decref_no_destruct(rt_struct_t **value)
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
                   mrklkit_rt_struct_get_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf ",
                   mrklkit_rt_struct_get_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_struct_get_item_str(value, it.iter, NULL);
                TRACEC("'%s' ", v != NULL ? v->data : NULL);
            }
            break;

        case LKIT_BOOL:
            TRACEC("%s ",
                   (int8_t)mrklkit_rt_struct_get_item_bool(
                       value, it.iter, 0) ? "#t" : "#f");
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_dump(
                mrklkit_rt_struct_get_item_array(value, it.iter, NULL));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_dump(
                mrklkit_rt_struct_get_item_dict(value, it.iter, NULL));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_dump(
                mrklkit_rt_struct_get_item_struct(value, it.iter, NULL));
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
                   mrklkit_rt_struct_get_item_int(value, it.iter, 0));
            break;

        case LKIT_FLOAT:
        case LKIT_FLOAT_MIN:
        case LKIT_FLOAT_MAX:
            TRACEC("%lf",
                   mrklkit_rt_struct_get_item_float(value, it.iter, 0.0));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = mrklkit_rt_struct_get_item_str(value, it.iter, NULL);
                TRACEC("%s", v != NULL ? v->data : (unsigned char *)"");
            }
            break;

        case LKIT_BOOL:
            TRACEC("%hhd", mrklkit_rt_struct_get_item_bool(value, it.iter, 0));
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_print(
                mrklkit_rt_struct_get_item_array(value, it.iter, NULL));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_print(
                mrklkit_rt_struct_get_item_dict(value, it.iter, NULL));
            break;

        case LKIT_STRUCT:
            mrklkit_rt_struct_print(
                mrklkit_rt_struct_get_item_struct(value, it.iter, NULL));
            break;

        default:
            FAIL("mrklkit_rt_struct_print");
        }
    }
}


void **
mrklkit_rt_struct_get_item_addr(rt_struct_t *value, int64_t idx)
{
    assert(idx < (ssize_t)value->type->fields.elnum);
    return value->fields + idx;
}


int64_t
mrklkit_rt_struct_get_item_int(rt_struct_t *value,
                               int64_t idx,
                               UNUSED int64_t dflt)
{
    int64_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int64_t *)(value->fields + idx);
    return *v;
}


double
mrklkit_rt_struct_get_item_float(rt_struct_t *value,
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
mrklkit_rt_struct_get_item_bool(rt_struct_t *value,
                                int64_t idx,
                                UNUSED int8_t dflt)
{
    int8_t *v;

    assert(idx < (ssize_t)value->type->fields.elnum);
    v = (int8_t *)((intptr_t *)(value->fields + idx));
    return *v;
}


bytes_t *
mrklkit_rt_struct_get_item_str(rt_struct_t *value,
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
mrklkit_rt_struct_get_item_array(rt_struct_t *value,
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
mrklkit_rt_struct_get_item_dict(rt_struct_t *value,
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
mrklkit_rt_struct_get_item_struct(rt_struct_t *value,
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
mrklkit_rt_struct_set_item_int(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}

void
mrklkit_rt_struct_set_item_int_mpool(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}

void
mrklkit_rt_struct_set_item_float(rt_struct_t *value, int64_t idx, double val)
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
mrklkit_rt_struct_set_item_float_mpool(rt_struct_t *value, int64_t idx, double val)
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
mrklkit_rt_struct_set_item_bool(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_set_item_bool_mpool(rt_struct_t *value, int64_t idx, int64_t val)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = val;
}

void
mrklkit_rt_struct_set_item_str(rt_struct_t *value, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    BYTES_DECREF(p);
    *p = val;
    BYTES_INCREF(*p);
}


void
mrklkit_rt_struct_set_item_str_mpool(rt_struct_t *value, int64_t idx, bytes_t *val)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_set_item_array(rt_struct_t *value,
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
mrklkit_rt_struct_set_item_array_mpool(rt_struct_t *value,
                                 int64_t idx,
                                 rt_array_t *val)
{
    rt_array_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_array_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_set_item_dict(rt_struct_t *value, int64_t idx, rt_dict_t *val)
{
    rt_dict_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_dict_t **)(value->fields + idx);
    DICT_DECREF(p);
    *p = val;
    DICT_INCREF(*p);
}


void
mrklkit_rt_struct_set_item_dict_mpool(rt_struct_t *value,
                                   int64_t idx,
                                   rt_dict_t *val)
{
    rt_dict_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_dict_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_set_item_struct(rt_struct_t *value,
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
mrklkit_rt_struct_set_item_struct_mpool(rt_struct_t *value,
                                  int64_t idx,
                                  rt_struct_t *val)
{
    rt_struct_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_struct_t **)(value->fields + idx);
    *p = val;
}


void
mrklkit_rt_struct_del_item_int(rt_struct_t *value,
                               int64_t idx)
{
    int64_t *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (int64_t *)(value->fields + idx);
    *p = 0;
}


void
mrklkit_rt_struct_del_item_float(rt_struct_t *value,
                                 int64_t idx)
{
    double *p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (double *)(value->fields + idx);
    *p = 0.0;
}


void
mrklkit_rt_struct_del_item_str_mpool(rt_struct_t *value,
                                     int64_t idx)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    *p = NULL;
}


void
mrklkit_rt_struct_del_item_str(rt_struct_t *value,
                               int64_t idx)
{
    bytes_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (bytes_t **)(value->fields + idx);
    BYTES_DECREF(p);
}


void
mrklkit_rt_struct_del_item_array_mpool(rt_struct_t *value,
                                       int64_t idx)
{
    rt_array_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_array_t **)(value->fields + idx);
    *p = NULL;
}


void
mrklkit_rt_struct_del_item_array(rt_struct_t *value,
                                 int64_t idx)
{
    rt_array_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_array_t **)(value->fields + idx);
    ARRAY_DECREF(p);
}


void
mrklkit_rt_struct_del_item_dict_mpool(rt_struct_t *value,
                                      int64_t idx)
{
    rt_dict_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_dict_t **)(value->fields + idx);
    *p = NULL;
}


void
mrklkit_rt_struct_del_item_dict(rt_struct_t *value,
                                int64_t idx)
{
    rt_dict_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_dict_t **)(value->fields + idx);
    DICT_DECREF(p);
}


void
mrklkit_rt_struct_del_item_struct_mpool(rt_struct_t *value,
                                        int64_t idx)
{
    rt_struct_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_struct_t **)(value->fields + idx);
    *p = NULL;
}


void
mrklkit_rt_struct_del_item_struct(rt_struct_t *value,
                                  int64_t idx)
{
    rt_struct_t **p;

    assert(idx < (ssize_t)value->type->fields.elnum);
    p = (rt_struct_t **)(value->fields + idx);
    STRUCT_DECREF(p);
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
mrklkit_rt_struct_deep_copy_mpool(rt_struct_t *dst,
                               rt_struct_t *src)
{
    MRKLKIT_RT_STRUCT_DEEP_COPY_BODY(_bytes_new);
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
            if (v.s != NULL) {
                bytes_t *escval;

                escval = bytes_json_escape(v.s);
                bytestream_nprintf(bs,
                                   1024 + escval->sz,
                                   "\"%s\"",
                                   (char *)escval->data);
                BYTES_DECREF(&escval);
            } else {
                bytestream_cat(bs, 4, "null");
            }
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
        (void)hash_traverse(&value->fields,
                            (hash_traverser_t)rt_dict_dump_json_item,
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
            v = MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(value, it.iter);
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
    int res;
    rt_array_t *value;
    bytes_t **v;

    value = udata;
    v = array_incr(&value->fields);
    *v = NULL;
    res = jparse_expect_item_str(jctx, v, NULL);
    if (*v != NULL) {
        *v = bytes_new_from_bytes(*v);
        BYTES_INCREF(*v);
    }
    return res;
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
    k = NULL;
    if ((res = jparse_expect_anykvp_int(jctx, &k, &v.i, NULL)) != 0) {
        return res;
    }

    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        hash_set_item(&value->fields, k, v.v);
    }
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
    k = NULL;
    if ((res = jparse_expect_anykvp_float(jctx, &k, &v.f, NULL)) != 0) {
        return res;
    }

    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        hash_set_item(&value->fields, k, v.v);
    }
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
    k = NULL;
    v.s = NULL;
    if ((res = jparse_expect_anykvp_str(jctx, &k, &v.s, NULL)) != 0) {
        return res;
    }

    if (v.s != NULL) {
        v.s = bytes_new_from_bytes(v.s);
        BYTES_INCREF(v.s);
        assert(k != NULL);
    }
    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        hash_set_item(&value->fields, k, v.v);
    } else {
        BYTES_DECREF(&v.s);
    }
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
    k = NULL;
    if ((res = jparse_expect_anykvp_bool(jctx, &k, &v.b, NULL)) != 0) {
        return res;
    }

    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        hash_set_item(&value->fields, k, v.v);
    }
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
    k = NULL;
    res = jparse_expect_anykvp_array_iter(jctx, &k, cb, jval, v);
    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        ARRAY_INCREF(v);
        hash_set_item(&value->fields, k, v);
    } else {
        ARRAY_DECREF(&v);
    }
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
    k = NULL;
    res = jparse_expect_anykvp_object_iter(jctx, &k, cb, jval, v);
    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        DICT_INCREF(v);
        hash_set_item(&value->fields, k, v);
    } else {
        DICT_DECREF(&v);
    }
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
    k = NULL;
    res = jparse_expect_anykvp_object(jctx,
                                      &k,
                                      _rt_struct_expect_fields_cb,
                                      jval,
                                      v);
    if (k != NULL) {
        k = bytes_new_from_bytes(k);
        BYTES_INCREF(k);
        STRUCT_INCREF(v);
        hash_set_item(&value->fields, k, v);
    } else {
        STRUCT_DECREF(&v);
    }
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
        v = MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(value, it.iter);
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
            {
                *v = NULL;
                res = jparse_expect_kvp_str(jctx, *name, (bytes_t **)v, NULL);
                if (*v != NULL) {
                    bytes_t *vv;
                    *v = bytes_new_from_bytes(*v);
                    vv = *v;
                    BYTES_INCREF(vv);
                }
            }

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
                jparse_value_t jv;

                ts = (lkit_struct_t *)(*fty);
                st = mrklkit_rt_struct_new(ts);
                jv.k = *name;
                jv.cb = _rt_struct_expect_fields_cb;
                jv.udata = st;
                jv.ty = JSON_OBJECT;

                res = jparse_expect_kvp_object(jctx,
                                               *name,
                                               _rt_struct_expect_fields_cb,
                                               &jv,
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
    if (res == JPARSE_EOS) {
        res = 0;
    }
    return res;

}


int
rt_struct_load_json(rt_struct_t *value, jparse_ctx_t *jctx)
{
    return jparse_expect_object(jctx, _rt_struct_expect_fields_cb, NULL, value);
}


int
rt_struct_load_fields_json(rt_struct_t *value, jparse_ctx_t *jctx)
{
    jparse_value_t jval;

    return _rt_struct_expect_fields_cb(jctx, &jval, value);
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
