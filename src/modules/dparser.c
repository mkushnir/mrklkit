#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dparser.h>

#include "diag.h"


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


void
dparser_reach_delim(bytestream_t *bs, char delim, off_t epos)
{
    while (SPOS(bs) < epos) {
        if (SPCHR(bs) == delim || SPCHR(bs) == '\0') {
            break;
        }
        SINCR(bs);
    }
}


int
dparser_reach_delim_readmore(bytestream_t *bs, int fd, char delim, off_t epos)
{
    while (SPOS(bs) <= epos) {
        //TRACE("SNEEDMORE=%d SPOS=%ld epos=%ld",
        //      SNEEDMORE(bs), SPOS(bs), epos);
        //TRACE("SPCHR='%c'", SPCHR(bs));
        if (SNEEDMORE(bs)) {
            if (bytestream_read_more(bs, fd, DPARSE_READSZ) <= 0) {
                return DPARSE_EOD;
            }
            epos = SEOD(bs);
        }
        if (SPCHR(bs) == delim || SPCHR(bs) == '\0') {
            return 0;
        }
        SINCR(bs);
    }
    return DPARSE_EOD;
}


void
dparser_reach_value(bytestream_t *bs, char delim, off_t epos, int flags)
{
    if (flags & DPARSE_MERGEDELIM) {
        while (SPOS(bs) < epos && SPCHR(bs) == delim) {
            SINCR(bs);
        }
    } else {
        if (SPOS(bs) < epos) {
            SINCR(bs);
        }
    }
}


int
dparser_read_lines(int fd,
                   dparser_read_lines_cb_t cb,
                   void *udata)
{
    int res = 0;
    bytestream_t bs;
    ssize_t nread;
    byterange_t br;

    bytestream_init(&bs, 4096);
    nread = 0xffffffff;
    br.start = 0;
    br.end = 0x7fffffffffffffff;

    while (nread > 0) {

        //sleep(1);

        br.start = SPOS(&bs);
        if (dparser_reach_delim_readmore(&bs,
                                         fd,
                                         '\n',
                                         br.end) == DPARSE_EOD) {
            break;
        }

        br.end = SPOS(&bs);
        SPOS(&bs) = br.start;

        //TRACE("start=%ld end=%ld", br.start, br.end);
        //D32(SPDATA(&bs), br.end - br.start);

        if ((res = cb(&bs, &br, udata)) != 0) {
            break;
        }

        dparser_reach_value(&bs, '\n', SEOD(&bs), 0);
        br.end = SEOD(&bs);
    }

    bytestream_fini(&bs);
    return res;
}


static int64_t
_strtoi64(char *ptr, char **endptr, char delim)
{
    int64_t res = 0;
    int64_t sign; /* XXX make if uint64_t, set to 0x8000000000000000 ... */
    char ch;

    errno = 0;
    if (*ptr == '-') {
        ++ptr;
        sign = -1;
    } else {
        sign = 1;
    }
    for (ch = *ptr; ch != delim; ch = *(++ptr)) {
        //TRACE("ch='%c' delim='%c'", ch, delim);
        if (ch >= '0' && ch <= '9') {
            res = res * 10 + ch - '0';
        } else {
            goto err;
        }
    }

end:
    *endptr = ptr;
    return res * sign;
err:
    errno = EINVAL;
    goto end;
}


/* a quick naive parser */
static double
_strtod(char *ptr, char **endptr, char delim)
{
    uint64_t integ = 0;
    uint64_t frac = 0;
    double sign;
    char *p;
    char ch;
    uint64_t factor = 1;
    char *dot = NULL;
    UNUSED int exp = 0;

    errno = 0;
    p = ptr;
    if (*ptr == '-') {
        ++ptr;
        sign = -1.0;
    } else {
        sign = 1.0;
    }
    for (ch = *ptr; ch != delim; ch = *(++ptr)) {
        if (ch >= '0' && ch <= '9') {
            //integ = integ * 10.0 + (double)(ch - '0');
            integ = integ * 10 + (ch - '0');
        } else {
            break;
        }
    }

    if (*ptr == '.') {
        ++ptr;
        dot = ptr;
        for (ch = *ptr;
             ch != delim;
             ch = *(++ptr), factor *= 10) {
            if (ch >= '0' && ch <= '9') {
                frac = frac * 10 + (ch - '0');
            } else {
                goto err;
            }
        }
    } else if (*ptr == delim) {
        goto end;
    } else {
        goto err;
    }


end:
#if 0
    exp = dot ? ptr - dot : 0;
    D8(p, ptr - p);
    TRACE("integ=%ld frac=%ld factor=%ld exp=%d strtod=%lf",
          integ, frac, factor, exp, strtod(p, NULL));
#endif
    *endptr = ptr;
    return ((double)integ + (double)frac / (double)factor) * sign;

err:
    errno = EINVAL;
    goto end;
}


int
dparse_int(bytestream_t *bs,
           char delim,
           off_t epos,
           int64_t *val,
           UNUSED unsigned int flags)
{
    int res = 0;
    char *ptr = (char *)SPDATA(bs);
    char *endptr = ptr;

    *val = _strtoi64(ptr, &endptr, delim);

    if (errno == EINVAL || errno == ERANGE) {
        res = DPARSE_ERRORVALUE;
        /* pass garbage */
        dparser_reach_delim(bs, delim, epos);
        goto end;
    }
    SADVANCEPOS(bs, endptr - ptr);
    dparser_reach_delim(bs, delim, epos);

end:
    return res;
}


int
dparse_float(bytestream_t *bs,
             char delim,
             off_t epos,
             double *val,
             UNUSED unsigned int flags)
{
    int res = 0;
    char *ptr = (char *)SPDATA(bs);
    char *endptr = ptr;

    /*
     * XXX  optimize out to support only (+-)m.n notation and handle
     * XXX  empty fields with DPARSE_MERGEDELIM off
     */
    //*val = strtod(ptr, &endptr);
    *val = _strtod(ptr, &endptr, delim);

    if (errno == EINVAL || errno == ERANGE) {
        res = DPARSE_ERRORVALUE;
        /* pass garbage */
        dparser_reach_delim(bs, delim, epos);
        goto end;
    }
    SADVANCEPOS(bs, endptr - ptr);
    dparser_reach_delim(bs, delim, epos);

end:
    return res;
}


int
dparse_str(bytestream_t *bs,
           char delim,
           off_t epos,
           OUT bytes_t **val,
           UNUSED unsigned int flags)
{

    byterange_t br;

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);

    *val = bytes_new(br.end - br.start + 1);
    memcpy((*val)->data, SDATA(bs, br.start), (*val)->sz);
    *((*val)->data + (*val)->sz) = '\0';
    BYTES_INCREF(*val);

    return 0;
}


int
dparse_qstr(bytestream_t *bs,
            UNUSED char delim,
            off_t epos,
            OUT bytes_t **value,
            UNUSED unsigned int flags)
{
    byterange_t br;
#   define QSTR_ST_START   (0)
#   define QSTR_ST_QUOTE   (1)
#   define QSTR_ST_IN      (2)
#   define QSTR_ST_OUT     (3)
    int state = QSTR_ST_START;

    br.start = SPOS(bs);

    for (br.end = SPOS(bs);
         br.end < epos;
         br.end = SINCR(bs)) {

        char ch;

        ch = SPCHR(bs);
        //TRACE("ch='%c'", ch);

        switch (state) {
        case QSTR_ST_START:
            if (ch == '"') {
                state = QSTR_ST_IN;
            } else {
                /* garbage before the opening " */
                return DPARSE_ERRORVALUE;
            }
            break;

        case QSTR_ST_IN:
            if (ch == '"') {
                state = QSTR_ST_QUOTE;
            }
            break;

        case QSTR_ST_QUOTE:
            if (ch == '"') {
                state = QSTR_ST_IN;
            } else {
                /* one beyond the closing " */
                state = QSTR_ST_OUT;
                goto end;
            }

            break;

        default:
            assert(0);
        }

    }

    //return DPARSE_ERRORVALUE;

end:
    //TRACE("st=%s",
    //      (state == QSTR_ST_START) ? "START" :
    //      (state == QSTR_ST_QUOTE) ? "QUOTE" :
    //      (state == QSTR_ST_IN) ? "IN" :
    //      (state == QSTR_ST_OUT) ? "OUT" :
    //      "...");

    *value = bytes_new(br.end - br.start + 1);
    BYTES_INCREF(*value);
    /* unescape starting from next by initial " */
    qstr_unescape((char *)(*value)->data,
                  SDATA(bs, br.start + 1),
                  /* minus "" */
                  br.end - br.start - 2);

    return state == QSTR_ST_OUT ? 0 : DPARSE_ERRORVALUE;
}


int
dparse_array(bytestream_t *bs,
             char delim,
             off_t epos,
             OUT rt_array_t *value,
             unsigned int flags)
{
    char fdelim;
    lkit_type_t *afty;
    byterange_t br;

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    fdelim = value->type->delim[0];
    if ((afty = lkit_array_get_element_type(value->type)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

#define DPARSE_ARRAY_CASE(ty, fn) \
        while (SPOS(bs) < br.end && SPCHR(bs) != delim) { \
            union { \
                void *v; \
                ty x; \
            } val; \
            void **v; \
            if (fn(bs, fdelim, br.end, &val.x, flags) != 0) { \
                /* \
                 * error parsing value would indicate end of array: \
                 * SPCHR(bs) can be either delim, or any higher level \
                 * delimiter \
                 */ \
                /* TRACE("SPCHR='%c' delim='%c' fdelim='%c'", SPCHR(bs), delim, fdelim); */ \
                if (SPCHR(bs) != fdelim) { \
                    if ((v = array_incr(&value->fields)) == NULL) { \
                        FAIL("array_incr"); \
                    } \
                    *v = val.v; \
                    break; \
                } \
            } \
            if ((v = array_incr(&value->fields)) == NULL) { \
                FAIL("array_incr"); \
            } \
            *v = val.v; \
            dparser_reach_value(bs, fdelim, br.end, flags); \
        }

    switch (afty->tag) {
    case LKIT_INT:
        DPARSE_ARRAY_CASE(int64_t, dparse_int);
        break;

    case LKIT_FLOAT:
        DPARSE_ARRAY_CASE(double, dparse_float);
        break;

    case LKIT_STR:
        DPARSE_ARRAY_CASE(bytes_t *, dparse_str);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_array");
    }

    return 0;
}


int
dparse_dict(bytestream_t *bs,
            char delim,
            off_t epos,
            OUT rt_dict_t *value,
            unsigned int flags)
{
    char kvdelim, fdelim;
    lkit_type_t *dfty;
    byterange_t br;

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    kvdelim = value->type->kvdelim[0];
    fdelim = value->type->fdelim[0];
    if ((dfty = lkit_dict_get_element_type(value->type)) == NULL) {
        FAIL("lkit_dict_get_element_type");
    }

#define DPARSE_DICT_CASE(ty, fn) \
        while (SPOS(bs) < br.end && SPCHR(bs) != delim) { \
            dict_item_t *it; \
            bytes_t *key = NULL; \
            union { \
                void *v; \
                ty x; \
            } val; \
            if (dparse_str(bs, kvdelim, br.end, &key, flags) != 0) { \
                BYTES_DECREF(&key); \
                dparser_reach_value(bs, fdelim, br.end, flags); \
                continue; \
            } else { \
                /* \
                if (dfty->tag == LKIT_INT) { \
                    TRACE("intkey=%s", key->data); \
                } \
                */ \
                dparser_reach_value(bs, kvdelim, br.end, flags); \
                if ((it = dict_get_item(&value->fields, key)) == NULL) { \
                    if (fn(bs, fdelim, br.end, &val.x, flags) != 0) { \
                        /* \
                         * error parsing value would indicate end of dict: \
                         * SPCHR(bs) can be either fdelim, or any higher level \
                         * delimiter \
                         */ \
                        /* TRACE("SPCHR='%c' delim='%c' fdelim='%c'", SPCHR(bs), delim, fdelim); */ \
                        if (SPCHR(bs) != fdelim) { \
                            dict_set_item(&value->fields, key, val.v); \
                            break; \
                        } \
                    } else { \
                        dict_set_item(&value->fields, key, val.v); \
                    } \
                } else { \
                    BYTES_DECREF(&key); \
                } \
                dparser_reach_value(bs, fdelim, br.end, flags); \
            } \
        }

    switch (dfty->tag) {
    case LKIT_INT:
        DPARSE_DICT_CASE(int64_t, dparse_int);
        break;

    case LKIT_FLOAT:
        DPARSE_DICT_CASE(double, dparse_float);
        break;

    case LKIT_STR:
        DPARSE_DICT_CASE(bytes_t *, dparse_str);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_array");
    }

    return 0;
}


int
dparse_struct(bytestream_t *bs,
              char delim,
              off_t epos,
              rt_struct_t *value,
              char *ch,
              unsigned int flags)
{
    char sdelim;
    byterange_t br;
    lkit_type_t **fty;
    void **val;


    sdelim = value->type->delim[0];
    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    //D32(SPDATA(bs), epos - SPOS(bs));

    for (fty = array_get(&value->type->fields, value->current);
         fty != NULL;
         fty = array_get(&value->type->fields, ++(value->current))) {

        val = mrklkit_rt_get_struct_item_addr(value, value->current);

        switch ((*fty)->tag) {
        case LKIT_INT:
            assert(sizeof(int64_t) == sizeof(void *));
            if (dparse_int(bs,
                           sdelim,
                           br.end,
                           (int64_t *)val,
                           flags) != 0) {
                //goto err;
                //TRACE("E:%ld", *((int64_t *)val));
            }
            break;

        case LKIT_FLOAT:
            {
                union {
                    double d;
                    void *v;
                } v;
                assert(sizeof(double) == sizeof(void *));
                if (dparse_float(bs,
                                 sdelim,
                                 br.end,
                                 &v.d,
                                 flags) != 0) {
                    //goto err;
                    //TRACE("E:%lf", v.d);
                }
                *val = v.v;
            }
            break;

        case LKIT_STR:
            if (dparse_str(bs,
                           sdelim,
                           br.end,
                           (bytes_t **)val,
                           flags) != 0) {
                //goto err;
                //TRACE("E:<str>");
            }
            break;

        case LKIT_ARRAY:
            {
                lkit_array_t *arty = (lkit_array_t *)*fty;
                *val = mrklkit_rt_array_new(arty);
                ARRAY_INCREF((rt_array_t *)*val);
                if (dparse_array(bs,
                                 sdelim,
                                 br.end,
                                 (rt_array_t *)*val,
                                 flags) != 0) {
                    //goto err;
                    //TRACE("E:<arr>");
                }
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *dcty = (lkit_dict_t *)*fty;
                *val = mrklkit_rt_dict_new(dcty);
                DICT_INCREF((rt_dict_t *)*val);
                if (dparse_dict(bs,
                                sdelim,
                                br.end,
                                (rt_dict_t *)*val,
                                flags) != 0) {
                    //goto err;
                    //TRACE("E:<dict>");
                }
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *ts;
                char sch;

                ts = (lkit_struct_t *)*fty;
                *val = mrklkit_rt_struct_new(ts);
                STRUCT_INCREF((rt_struct_t *)*val);
                if (dparse_struct(bs,
                                  sdelim,
                                  br.end,
                                  (rt_struct_t *)(*val),
                                  &sch,
                                  flags) != 0) {
                    //goto err;
                    //TRACE("E:<struct>");
                }
            }
            break;

        default:
            FAIL("dparse_struct: need more types to handle in dparse");
        }

        //TRACE("SPCHR='%c' sdelim='%c'", SPCHR(bs), sdelim);
        dparser_reach_value(bs, sdelim, epos, flags);
    }

    SPOS(bs) = br.end;
    *ch = SPCHR(bs);

    return 0;
}

