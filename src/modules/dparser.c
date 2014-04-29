#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dparser.h>

#include "diag.h"

#define DPARSE_TEST_DELIM(ch) ((ch) == fdelim || (ch) == rdelim[0] || (ch) == rdelim[1])


void
qstr_unescape(char *dst, const char *src, size_t sz)
{
    size_t i;

    /* sz should not count terminating \0 */
    for (i = 0; i < sz; ++i) {
        if (*(src + i) == '"') {
            if (*(src + i + 1) == '"') {
                ++i;
                *dst++ = *(src + i);
            }
        } else {
            *dst++ = *(src + i);
        }
    }
    *dst = '\0';
}


static void
reach_value(bytestream_t *bs, char fdelim)
{
    do {
        SINCR(bs);
        if (SPCHR(bs) != fdelim) {
            break;
        }
    } while (!SNEEDMORE(bs));
}


static void
reach_delim(bytestream_t *bs, char fdelim, char rdelim[2])
{
    do {
        SINCR(bs);
        if (DPARSE_TEST_DELIM(SPCHR(bs))) {
            break;
        }
    } while (!SNEEDMORE(bs));
}


#define strtoll10(ptr, endptr) strtoll((ptr), (endptr), 10)

#define DPARSE_INTFLOAT(ty, fn) \
    off_t spos = SPOS(bs); \
    while (!SNEEDMORE(bs)) { \
        *ch = SPCHR(bs); \
        if (DPARSE_TEST_DELIM(*ch)) { \
            off_t epos; \
            char *endptr = NULL; \
            epos = SPOS(bs); \
            if (epos == spos) { \
                /* empty field */ \
                *value = 0; \
                SINCR(bs); \
                if (flags & DPARSE_RESETONERROR) { \
                    SPOS(bs) = spos; \
                } else { \
                    if (flags & DPARSE_MERGEDELIM) { \
                        reach_value(bs, fdelim); \
                    } \
                } \
                return DPARSE_ERRORVALUE; \
            } else { \
                *value = (ty)fn(SDATA(bs, spos), &endptr); \
            } \
            if ((*value == 0 && errno == EINVAL) || errno == ERANGE) { \
                goto err; \
            } else { \
                /* at the first char of the next field */ \
                if (flags & DPARSE_MERGEDELIM) { \
                    reach_value(bs, fdelim); \
                } else { \
                    SINCR(bs); \
                } \
            } \
            return 0; \
        } \
        SINCR(bs); \
    } \
    return DPARSE_NEEDMORE; \
err: \
    SPOS(bs) = spos; \
    if (!(flags & DPARSE_RESETONERROR)) { \
        reach_delim(bs, fdelim, rdelim); \
        if (flags & DPARSE_MERGEDELIM) { \
            reach_value(bs, fdelim); \
        } else { \
            SINCR(bs); \
        } \
    } \
    return DPARSE_ERRORVALUE;


#define DPARSE_KV_INTFLOAT(ty, fn) \
    int res; \
    off_t spos = SPOS(bs); \
    bytes_t *key = NULL; \
    union { \
        ty v; \
        void *p; \
    } val; \
    char krdelim[2] = {fdelim, rdelim[0]}; \
    while (!SNEEDMORE(bs)) { \
        if ((res = dparse_str(bs, \
                              kvdelim, \
                              krdelim, \
                              &key, \
                              ch, \
                              flags)) == DPARSE_NEEDMORE) { \
            goto needmore; \
        } else if (res == DPARSE_ERRORVALUE) { \
            goto err; \
        } \
        /* \
         * XXX we "require" here that sizeof(ty) == sizeof(void *), \
         * sigh ... \
         */ \
        assert(sizeof(void *) == sizeof(ty)); \
        if ((res = fn(bs, \
                      fdelim, \
                      rdelim, \
                      &val.v, \
                      ch, \
                      flags)) == DPARSE_NEEDMORE) { \
            goto needmore; \
        } else if (res == DPARSE_ERRORVALUE) { \
            goto err; \
        } \
        dict_set_item(&value->fields, key, val.p); \
        return 0; \
    } \
needmore: \
    return DPARSE_NEEDMORE; \
err: \
    bytes_destroy(&key); \
    if (flags & DPARSE_RESETONERROR) { \
        SPOS(bs) = spos; \
    } \
    return DPARSE_ERRORVALUE;


int
dparse_int(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           OUT int64_t *value,
           char *ch,
           unsigned int flags)
{
    DPARSE_INTFLOAT(int64_t, strtoll10);
}


int
dparse_float(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           OUT double *value,
           char *ch,
           unsigned int flags)
{
    DPARSE_INTFLOAT(double, strtod);
}


int
dparse_qstr(bytestream_t *bs,
            char fdelim,
            char rdelim[2],
            OUT bytes_t **value,
            char *ch,
            unsigned int flags)
{
    byterange_t br;
#   define QSTR_ST_START   (0)
#   define QSTR_ST_QUOTE   (1)
#   define QSTR_ST_IN      (2)
#   define QSTR_ST_OUT     (3)
    int state = QSTR_ST_START;

    br.start = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        br.end = SPOS(bs);
        *ch = SPCHR(bs);

        switch (state) {
        case QSTR_ST_START:
            if (*ch == '"') {
                state = QSTR_ST_IN;
            } else {
                /* garbage before the opening " */
                goto err;
            }
            break;

        case QSTR_ST_IN:
            if (*ch == '"') {
                state = QSTR_ST_QUOTE;
            }
            break;

        case QSTR_ST_QUOTE:
            if (*ch == '"') {
                state = QSTR_ST_IN;
            } else {
                /* one beyond the closing " */
                state = QSTR_ST_OUT;

                *value = bytes_new(br.end - br.start + 1);
                BYTES_INCREF(*value);
                /* unescape starting from next by initial " */
                qstr_unescape((char *)(*value)->data,
                              SDATA(bs, br.start + 1),
                              /* minus "" */
                              br.end - br.start - 2);

                if (DPARSE_TEST_DELIM(*ch)) {
                    if (flags & DPARSE_MERGEDELIM) {
                        reach_value(bs, fdelim);
                    } else {
                        SINCR(bs);
                    }
                    return 0;

                } else {
                    /* garbage beyond closing " */
                    goto err;
                }
            }

            break;

        default:
            assert(0);
        }

        //TRACE("ch='%c' st=%s", *ch,
        //      (state == QSTR_ST_START) ? "START" :
        //      (state == QSTR_ST_QUOTE) ? "QUOTE" :
        //      (state == QSTR_ST_IN) ? "IN" :
        //      (state == QSTR_ST_OUT) ? "OUT" :
        //      "...");

        SINCR(bs);
    }

    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = br.start;
    } else {
        if (!DPARSE_TEST_DELIM(SPCHR(bs))) {
            reach_delim(bs, fdelim, rdelim);
        }
        if (flags & DPARSE_MERGEDELIM) {
            reach_value(bs, fdelim);
        } else {
            SINCR(bs);
        }
    }
    return DPARSE_ERRORVALUE;
}

int
dparse_str(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           OUT bytes_t **value,
           char *ch,
           unsigned int flags)
{
    byterange_t br;

    br.start = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (DPARSE_TEST_DELIM(*ch)) {
            br.end = SPOS(bs);
            *value = bytes_new(br.end - br.start + 1);
            BYTES_INCREF(*value);
            memcpy((*value)->data, SDATA(bs, br.start), br.end - br.start); \
            (*value)->data[br.end - br.start] = '\0'; \
            if (flags & DPARSE_MERGEDELIM) {
                reach_value(bs, fdelim);
            } else {
                SINCR(bs);
            }
            return 0;
        } else {
            SINCR(bs);
        }
    }

    return DPARSE_NEEDMORE;
}


static int
dparse_kv_int(bytestream_t *bs,
              char kvdelim,
              char fdelim,
              char rdelim[2],
              OUT rt_dict_t *value,
              char *ch,
              unsigned int flags)
{
    DPARSE_KV_INTFLOAT(int64_t, dparse_int);
}


static int
dparse_kv_float(bytestream_t *bs,
                char kvdelim,
                char fdelim,
                char rdelim[2],
                OUT rt_dict_t *value,
                char *ch,
                unsigned int flags)
{
    DPARSE_KV_INTFLOAT(double, dparse_float);
}


static int
dparse_kv_str_bytes(bytestream_t *bs,
                    char kvdelim,
                    char fdelim,
                    char rdelim[2],
                    OUT rt_dict_t *value,
                    char *ch,
                    unsigned int flags)
{
    int res;
    off_t spos = SPOS(bs);
    bytes_t *key = NULL;
    bytes_t *val = NULL;
    char krdelim[2] = {fdelim, rdelim[0]};
    while (!SNEEDMORE(bs)) {
        if ((res = dparse_str(bs,
                              kvdelim,
                              krdelim,
                              &key,
                              ch,
                              flags)) == DPARSE_NEEDMORE) {
            goto needmore;
        } else if (res == DPARSE_ERRORVALUE) {
            goto err;
        }
        if ((res = dparse_str(bs,
                              fdelim,
                              rdelim,
                              &val,
                              ch,
                              flags)) == DPARSE_NEEDMORE) {
            goto needmore;
        } else if (res == DPARSE_ERRORVALUE) {
            goto err;
        }
        dict_set_item(&value->fields, key, val);
        return 0;
    }

needmore:
    return DPARSE_NEEDMORE;
err:
    bytes_destroy(&key);
    bytes_destroy(&val);
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    }
    return DPARSE_ERRORVALUE;
}


int
dparse_array(bytestream_t *bs,
             char fdelim,
             char rdelim[2],
             lkit_array_t *arty,
             OUT rt_array_t *value,
             char *ch,
             unsigned int flags)
{
    off_t spos = SPOS(bs);
    char afdelim;
    char ardelim[2] = {fdelim, rdelim[0]};
    lkit_type_t *afty;

    afdelim = arty->delim[0];
    if ((afty = lkit_array_get_element_type(arty)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

#define DPARSE_AR_CASE(ty, fn) \
        while (!SNEEDMORE(bs)) { \
            int res; \
            ty *val; \
            if ((val = array_incr(&value->fields)) == NULL) { \
                FAIL("array_incr"); \
            } \
            if ((res = fn(bs, \
                          afdelim, \
                          ardelim, \
                          val, \
                          ch, \
                          flags)) == \
                    DPARSE_NEEDMORE) { \
                goto needmore; \
            } else if (res == DPARSE_ERRORVALUE) { \
                goto err; \
            } \
            if (DPARSE_TEST_DELIM(*ch)) { \
                break; \
            } \
        }

    if (afty->tag == LKIT_INT) {
        DPARSE_AR_CASE(int64_t, dparse_int);

    } else if (afty->tag == LKIT_FLOAT) {
        DPARSE_AR_CASE(double, dparse_float);

    } else if (afty->tag == LKIT_STR) {
        DPARSE_AR_CASE(bytes_t *, dparse_str);

    } else {
        /* cannot be recursively nested */
        FAIL("dparse_array");
    }

    if (flags & DPARSE_MERGEDELIM) {
        if (DPARSE_TEST_DELIM(SPCHR(bs))) {
            reach_value(bs, fdelim);
        }
    }
    return 0;

needmore:
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    }
    return DPARSE_ERRORVALUE;
}


int
dparse_dict(bytestream_t *bs,
            char fdelim,
            char rdelim[2],
            lkit_dict_t *dcty,
            OUT rt_dict_t *value,
            char *ch,
            unsigned int flags)
{
    off_t spos = SPOS(bs);
    char dfdelim;
    char drdelim[2] = {fdelim, rdelim[0]};
    lkit_type_t *dfty;

    dfdelim = dcty->fdelim[0];
    if ((dfty = lkit_dict_get_element_type(dcty)) == NULL) {
        FAIL("lkit_dict_get_element_type");
    }

#define DPARSE_KV_CASE(fn) \
        while (!SNEEDMORE(bs)) { \
            int res; \
            if ((res = fn(bs, \
                          dcty->kvdelim[0], \
                          dfdelim, \
                          drdelim, \
                          value, \
                          ch, \
                          flags)) == \
                 DPARSE_NEEDMORE) { \
                goto needmore; \
            } else if (res == DPARSE_ERRORVALUE) { \
                goto err; \
            } \
            if (DPARSE_TEST_DELIM(*ch)) { \
                break; \
            } \
        }

    if (dfty->tag == LKIT_INT) {
        DPARSE_KV_CASE(dparse_kv_int);

    } else if (dfty->tag == LKIT_FLOAT) {
        DPARSE_KV_CASE(dparse_kv_float);

    } else if (dfty->tag == LKIT_STR) {
        DPARSE_KV_CASE(dparse_kv_str_bytes);

    } else {
        /* cannot be recursively nested */
        FAIL("dparse_dict");
    }

    if (flags & DPARSE_MERGEDELIM) {
        if (DPARSE_TEST_DELIM(SPCHR(bs))) {
            reach_value(bs, fdelim);
        }
    }
    return 0;

needmore:
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    }
    return DPARSE_ERRORVALUE;
}


int
dparse_struct(bytestream_t *bs,
              char fdelim,
              char rdelim[2],
              lkit_struct_t *stty,
              OUT rt_struct_t *value,
              char *ch,
              unsigned int flags)
{
    off_t spos = SPOS(bs);
    lkit_type_t **fty;
    void **val;
    ssize_t idx;

    idx = value->current;
    for (fty = array_get(&stty->fields, idx);
         fty != NULL;
         fty = array_get(&stty->fields, ++idx)) {

        val = mrklkit_rt_get_struct_item_addr(value, idx);

        switch ((*fty)->tag) {
        case LKIT_INT:
            if (dparse_int(bs, fdelim, rdelim, (int64_t *)val, ch, flags) != 0) {
                goto err;
            }
            break;

        case LKIT_FLOAT:
            {
                union {
                    double d;
                    void *v;
                } v;
                assert(sizeof(double) == sizeof(void *));
                if (dparse_float(bs, fdelim, rdelim, &v.d, ch, flags) != 0) {
                    goto err;
                }
                *val = v.v;
            }
            break;

        case LKIT_STR:
            if (dparse_str(bs, fdelim, rdelim, (bytes_t **)val, ch, flags) != 0) {
                goto err;
            }
            break;

        default:
            FAIL("dparse_struct: need more types to handle in dparse");
        }
    }

    if (flags & DPARSE_MERGEDELIM) {
        if (DPARSE_TEST_DELIM(SPCHR(bs))) {
            reach_value(bs, fdelim);
        }
    }

    value->current = idx;
    return 0;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    }
    return DPARSE_ERRORVALUE;
}

