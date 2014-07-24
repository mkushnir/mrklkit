#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
//#define TRRET_DEBUG_VERBOSE
//#define TRRET_DEBUG
#include <mrkcommon/dumpm.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/util.h>

#include <mrklkit/dparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"

static mpool_ctx_t *mpool;

typedef void (*dparse_struct_item_cb_t)(bytestream_t *, char, off_t, off_t, void **);

static void
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


static int
dparser_reach_delim_readmore(bytestream_t *bs, int fd, char delim, off_t epos)
{
    while (SPOS(bs) <= epos) {
        //TRACE("SNEEDMORE=%d SPOS=%ld epos=%ld",
        //      SNEEDMORE(bs), SPOS(bs), epos);
        //TRACE("SPCHR='%c'", SPCHR(bs));
        if (SNEEDMORE(bs)) {
            if (bytestream_read_more(bs, fd, bs->growsz) <= 0) {
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
dparser_reach_value(bytestream_t *bs, UNUSED char delim, off_t epos)
{
    if (SPOS(bs) < epos) {
        SINCR(bs);
    }
}


void
dparser_reach_value_m(bytestream_t *bs, char delim, off_t epos)
{
    while (SPOS(bs) < epos && SPCHR(bs) == delim) {
        SINCR(bs);
    }
}


int64_t
dparser_strtoi64(char *ptr, char **endptr, char delim)
{
    int64_t res = 0;
    int64_t sign; /* XXX make it uint64_t, set to 0x8000000000000000 ... */
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
double
dparser_strtod(char *ptr, char **endptr, char delim)
{
    uint64_t integ = 0;
    uint64_t frac = 0;
    double sign;
    UNUSED char *p;
    char ch;
    uint64_t factor = 1;
    UNUSED char *dot = NULL;
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

    *val = dparser_strtoi64(ptr, &endptr, delim);

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
    *val = dparser_strtod(ptr, &endptr, delim);

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

    *val = mrklkit_rt_bytes_new_gc(br.end - br.start + 1);
    memcpy((*val)->data, SDATA(bs, br.start), (*val)->sz - 1);
    *((*val)->data + (*val)->sz - 1) = '\0';
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

    *value = mrklkit_rt_bytes_new_gc(br.end - br.start + 1);
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
             UNUSED unsigned int flags)
{
    char fdelim;
    lkit_type_t *afty;
    byterange_t br;
    void (*_dparser_reach_value)(bytestream_t *, char, off_t);

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    fdelim = value->type->delim;
    if ((afty = lkit_array_get_element_type(value->type)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

    if (value->type->parser == LKIT_PARSER_MDELIM) {
        _dparser_reach_value = dparser_reach_value_m;
    } else {
        _dparser_reach_value = dparser_reach_value;
    }

#define DPARSE_ARRAY_CASE(ty, fn) \
        while (SPOS(bs) < br.end && SPCHR(bs) != delim) { \
            union { \
                void *v; \
                ty x; \
            } u; \
            void **v; \
            if (fn(bs, fdelim, br.end, &u.x, flags) != 0) { \
                /* \
                 * error parsing value would indicate end of array: \
                 * SPCHR(bs) can be either delim, or any higher level \
                 * delimiter \
                 */ \
                /* TRACE("SPCHR='%c' delim='%c' fdelim='%c'", SPCHR(bs), delim, fdelim); */ \
                if (SPCHR(bs) != fdelim) { \
                    if ((v = array_incr_mpool(mpool, &value->fields)) == NULL) { \
                        FAIL("array_incr_mpool"); \
                    } \
                    *v = u.v; \
                    break; \
                } \
            } \
            if ((v = array_incr_mpool(mpool, &value->fields)) == NULL) { \
                FAIL("array_incr_mpool"); \
            } \
            *v = u.v; \
            _dparser_reach_value(bs, fdelim, br.end); \
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

#undef DPARSE_ARRAY_CASE

    return 0;
}


int
dparse_dict(bytestream_t *bs,
            char delim,
            off_t epos,
            OUT rt_dict_t *value,
            UNUSED unsigned int flags)
{
    char kvdelim, fdelim;
    lkit_type_t *dfty;
    byterange_t br;
    void (*_dparser_reach_value)(bytestream_t *, char, off_t);

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    kvdelim = value->type->kvdelim;
    fdelim = value->type->fdelim;
    if ((dfty = lkit_dict_get_element_type(value->type)) == NULL) {
        FAIL("lkit_dict_get_element_type");
    }

    if (value->type->parser == LKIT_PARSER_MDELIM) {
        _dparser_reach_value = dparser_reach_value_m;
    } else {
        _dparser_reach_value = dparser_reach_value;
    }

#define DPARSE_DICT_CASE(ty, fn) \
        while (SPOS(bs) < br.end && SPCHR(bs) != delim) { \
            dict_item_t *it; \
            bytes_t *key = NULL; \
            union { \
                void *v; \
                ty x; \
            } u; \
            if (dparse_str(bs, kvdelim, br.end, &key, flags) != 0) { \
                BYTES_DECREF(&key); \
                _dparser_reach_value(bs, fdelim, br.end); \
                continue; \
            } else { \
                /* \
                if (dfty->tag == LKIT_INT) { \
                    TRACE("intkey=%s", key->data); \
                } \
                */ \
                _dparser_reach_value(bs, kvdelim, br.end); \
                if ((it = dict_get_item(&value->fields, key)) == NULL) { \
                    if (fn(bs, fdelim, br.end, &u.x, flags) != 0) { \
                        /* \
                         * error parsing value would indicate end of dict: \
                         * SPCHR(bs) can be either fdelim, or any higher level \
                         * delimiter \
                         */ \
                        /* TRACE("SPCHR='%c' delim='%c' fdelim='%c'", SPCHR(bs), delim, fdelim); */ \
                        if (SPCHR(bs) != fdelim) { \
                            dict_set_item_mpool(mpool, &value->fields, key, u.v); \
                            break; \
                        } \
                    } else { \
                        dict_set_item_mpool(mpool, &value->fields, key, u.v); \
                    } \
                } else { \
                    BYTES_DECREF(&key); \
                } \
                _dparser_reach_value(bs, fdelim, br.end); \
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
        FAIL("dparse_dict");
    }

#undef DPARSE_DICT_CASE

    return 0;
}


int
dparse_struct(bytestream_t *bs,
              char delim,
              off_t epos,
              rt_struct_t *value,
              char *ch,
              UNUSED unsigned int flags)
{
    char sdelim;
    byterange_t br;
    lkit_type_t **fty;
    void **val;
    void (*_dparser_reach_value)(bytestream_t *, char, off_t);

    sdelim = value->type->delim;
    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    if (value->type->parser == LKIT_PARSER_MDELIM) {
        _dparser_reach_value = dparser_reach_value_m;
    } else {
        _dparser_reach_value = dparser_reach_value;
    }

    //D32(SPDATA(bs), epos - SPOS(bs));

    for (fty = ARRAY_GET(lkit_type_t*,
                         &value->type->fields,
                         value->current);
         value->current < (ssize_t)value->type->fields.elnum;
         fty = ARRAY_GET(lkit_type_t *,
                         &value->type->fields,
                         ++(value->current))) {

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
                } u;
                assert(sizeof(double) == sizeof(void *));
                if (dparse_float(bs,
                                 sdelim,
                                 br.end,
                                 &u.d,
                                 flags) != 0) {
                    //goto err;
                    //TRACE("E:%lf", u.d);
                }
                *val = u.v;
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
                *val = mrklkit_rt_array_new_gc(arty);
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
                *val = mrklkit_rt_dict_new_gc(dcty);
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
                *val = mrklkit_rt_struct_new_gc(ts);
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
            //if (dparse_str(bs,
            //               sdelim,
            //               br.end,
            //               (bytes_t **)val,
            //               flags) != 0) {
            //}
            FAIL("dparse_struct: need more types to handle in dparse");
        }

        //TRACE("SPCHR='%c' sdelim='%c'", SPCHR(bs), sdelim);
        _dparser_reach_value(bs, sdelim, epos);
    }

    SPOS(bs) = br.end;
    *ch = SPCHR(bs);

    return 0;
}









static off_t
dparser_reach_delim_pos(bytestream_t *bs, char delim, off_t spos, off_t epos)
{
    while (spos < epos) {
        if (SNCHR(bs, spos) == delim || SNCHR(bs, spos) == '\0') {
            break;
        }
        ++spos;
    }
    return spos;
}


static off_t
dparser_reach_value_pos(bytestream_t *bs, char delim, off_t spos, off_t epos)
{
    if (spos < epos && SNCHR(bs, spos) == delim) {
        ++spos;
    }
    return spos;
}


static off_t
dparser_reach_value_m_pos(bytestream_t *bs, char delim, off_t spos, off_t epos)
{
    while (spos < epos && SNCHR(bs, spos) == delim) {
        ++spos;
    }
    return spos;
}


static off_t
dparse_int_pos(bytestream_t *bs,
               char delim,
               off_t spos,
               UNUSED off_t epos,
               int64_t *val)
{
    char *ptr = (char *)SDATA(bs, spos);
    char *endptr = ptr;

    *val = dparser_strtoi64(ptr, &endptr, delim);
    return spos + endptr - ptr;
}

static off_t
dparse_float_pos(bytestream_t *bs,
               char delim,
               off_t spos,
               UNUSED off_t epos,
               double *val)
{
    char *ptr = (char *)SDATA(bs, spos);
    char *endptr = ptr;

    *val = dparser_strtod(ptr, &endptr, delim);
    return spos + endptr - ptr;
}

static off_t
dparse_float_pos_pvoid(bytestream_t *bs,
               char delim,
               off_t spos,
               UNUSED off_t epos,
               void **vv)
{
    char *ptr = (char *)SDATA(bs, spos);
    char *endptr = ptr;
    union {
        void **v;
        double *d;
    } u;
    u.v = vv;

    *u.d = dparser_strtod(ptr, &endptr, delim);
    return spos + endptr - ptr;
}

static off_t
dparse_str_pos(bytestream_t *bs,
               char delim,
               off_t spos,
               off_t epos,
               bytes_t **val)
{
    off_t i;

    for (i = spos; i < epos && SNCHR(bs, i) != delim; ++i) {
        ;
    }

    *val = mrklkit_rt_bytes_new_gc(i - spos + 1);
    memcpy((*val)->data, SDATA(bs, spos), (*val)->sz - 1);
    *((*val)->data + (*val)->sz - 1) = '\0';
    //BYTES_INCREF(*val);
    return i;
}

static off_t
dparse_str_pos_brushdown(bytestream_t *bs,
                         char delim,
                         off_t spos,
                         off_t epos,
                         bytes_t **val)
{
    off_t i;

    for (i = spos; i < epos && SNCHR(bs, i) != delim; ++i) {
        ;
    }

    *val = mrklkit_rt_bytes_new_gc(i - spos + 1);
    memcpy((*val)->data, SDATA(bs, spos), (*val)->sz - 1);
    *((*val)->data + (*val)->sz - 1) = '\0';
    bytes_brushdown(*val);
    //BYTES_INCREF(*val);
    return i;
}

static void
dparse_array_pos(bytestream_t *bs,
                 char delim,
                 off_t spos,
                 off_t epos,
                 rt_array_t **val)
{
    char fdelim;
    lkit_type_t *afty;

    fdelim = (*val)->type->delim;
    if ((afty = lkit_array_get_element_type((*val)->type)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

#define DPARSE_ARRAY_CASE(ty, fn) \
    while (spos < epos && SNCHR(bs, spos) != delim) { \
        off_t fpos; \
        union { \
            void *v; \
            ty x; \
        } u; \
        void **v; \
        fpos = dparser_reach_delim_pos(bs, fdelim, spos, epos); \
        (void)fn(bs, fdelim, spos, fpos, &u.x); \
        if ((v = array_incr_mpool(mpool, &(*val)->fields)) == NULL) { \
            FAIL("array_incr_mpool"); \
        } \
        *v = u.v; \
        spos = dparser_reach_value_pos(bs, fdelim, fpos, epos); \
    }

    switch (afty->tag) {
    case LKIT_STR:
        DPARSE_ARRAY_CASE(bytes_t *, dparse_str_pos);
        break;

    case LKIT_INT:
        DPARSE_ARRAY_CASE(int64_t, dparse_int_pos);
        break;

    case LKIT_FLOAT:
        DPARSE_ARRAY_CASE(double, dparse_float_pos);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_array");
    }
}


static void
dparse_dict_pos(bytestream_t *bs,
                char delim,
                off_t spos,
                off_t epos,
                rt_dict_t **val)
{
    char kvdelim;
    char fdelim;
    lkit_type_t *dfty;
    off_t (*_dparse_str_pos)(bytestream_t *, char, off_t, off_t, bytes_t **);

    kvdelim = (*val)->type->kvdelim;
    fdelim = (*val)->type->fdelim;
    if ((dfty = lkit_dict_get_element_type((*val)->type)) == NULL) {
        FAIL("lkit_dict_get_element_type");
    }

    if ((*val)->type->parser == LKIT_PARSER_SMARTDELIM) {
        _dparse_str_pos = dparse_str_pos_brushdown;
    } else {
        _dparse_str_pos = dparse_str_pos;
    }

#define DPARSE_DICT_CASE(ty, fn) \
        while (spos < epos && SNCHR(bs, spos) != delim) { \
            off_t kvpos, fpos; \
            dict_item_t *it; \
            bytes_t *key = NULL; \
            union { \
                void *v; \
                ty x; \
            } u; \
            kvpos = dparser_reach_delim_pos(bs, kvdelim, spos, epos); \
            (void)_dparse_str_pos(bs, kvdelim, spos, kvpos, &key); \
            spos = dparser_reach_value_pos(bs, kvdelim, kvpos, epos); \
            if ((it = dict_get_item(&(*val)->fields, key)) == NULL) { \
                fpos = fn(bs, fdelim, spos, epos, &u.x); \
                dict_set_item_mpool(mpool, &(*val)->fields, key, u.v); \
                fpos = dparser_reach_delim_pos(bs, fdelim, fpos, epos); \
            } else { \
                /* BYTES_DECREF(&key); */ \
                fpos = dparser_reach_delim_pos(bs, fdelim, spos, epos); \
            } \
            spos = dparser_reach_value_pos(bs, fdelim, fpos, epos); \
        }

    switch (dfty->tag) {
    case LKIT_STR:
        DPARSE_DICT_CASE(bytes_t *, dparse_str_pos);
        break;

    case LKIT_INT:
        DPARSE_DICT_CASE(int64_t, dparse_int_pos);
        break;

    case LKIT_FLOAT:
        DPARSE_DICT_CASE(double, dparse_float_pos);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_dict_pos");
    }

}


static void
dparse_struct_pos(bytestream_t *bs,
                UNUSED char delim,
                off_t spos,
                off_t epos,
                rt_struct_t **val)
{
    (*val)->parser_info.bs = bs;
    (*val)->parser_info.br.start = spos;
    (*val)->parser_info.br.end = epos;
}


void
dparse_struct_setup(bytestream_t *bs,
                  const byterange_t *br,
                  rt_struct_t *value)
{
    value->parser_info.bs = bs;
    value->parser_info.br = *br;
    value->parser_info.pos = br->start;
}


off_t
dparse_struct_pi_pos(rt_struct_t *value)
{
    return value->parser_info.pos;
}


/**
 * struct item sequential access, implies "loose" delimiter grammar
 */
UNUSED static void **
dparse_struct_item_seq(rt_struct_t *value,
                       int64_t idx,
                       UNUSED dparse_struct_item_cb_t cb)
{
    void **val;
    int nidx = idx + 1;

    if (nidx >= value->next_delim) {
        off_t (*_dparser_reach_value)(bytestream_t *, char, off_t, off_t);
        bytestream_t *bs;
        char delim;

        bs = value->parser_info.bs;
        delim = value->type->delim;

        if (value->type->parser == LKIT_PARSER_SMDELIM) {
            _dparser_reach_value = dparser_reach_value_m_pos;
        } else {
            _dparser_reach_value = dparser_reach_value_pos;
        }

        for (; value->next_delim < nidx; ++value->next_delim) {
            off_t pos;

            pos = value->parser_info.pos;

            pos = _dparser_reach_value(bs,
                                       delim,
                                       pos,
                                       value->parser_info.br.end);
        }
    }

    val = MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);

    return val;
}




#define DPARSE_STRUCT_ITEM_RA_BODY(ty, val, cb) \
    assert(idx < (ssize_t)value->type->fields.elnum); \
    assert(value->parser_info.bs != NULL); \
    if (!(value->dpos[idx] & 0x80000000)) { \
        bytestream_t *bs; \
        off_t spos, epos; \
        char delim; \
        int nidx = idx + 1; \
        off_t (*_dparser_reach_value)(bytestream_t *, char, off_t, off_t); \
        bs = value->parser_info.bs; \
        delim = value->type->delim; \
        if (value->type->parser == LKIT_PARSER_MDELIM) { \
            _dparser_reach_value = dparser_reach_value_m_pos; \
        } else { \
            _dparser_reach_value = dparser_reach_value_pos; \
        } \
        if (nidx >= value->next_delim) { \
            off_t pos; \
            pos = value->parser_info.pos; \
            do { \
                value->dpos[value->next_delim] = pos; \
                pos = _dparser_reach_value(bs, \
                                           delim, \
                                           pos, \
                                           value->parser_info.br.end); \
                pos = dparser_reach_delim_pos(bs, \
                                              delim, \
                                              pos, \
                                              value->parser_info.br.end); \
            } while (value->next_delim++ < nidx); \
            assert(value->next_delim == (nidx + 1)); \
            value->parser_info.pos = pos; \
        } \
        spos = _dparser_reach_value(bs, \
                                    delim, \
                                    value->dpos[idx], \
                                    value->parser_info.br.end); \
        epos = value->dpos[nidx] & ~0x80000000; \
        value->dpos[idx] |= 0x80000000; \
        val = (ty)MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx); \
        cb(bs, delim, spos, epos, val); \
    } else { \
        val = (ty)MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx); \
    }





/**
 * struct item random access, implies "strict" delimiter grammar
 */
static void **
dparse_struct_item_ra(rt_struct_t *value,
                      int64_t idx,
                      dparse_struct_item_cb_t cb)
{
    void **val;
    assert(idx < (ssize_t)value->type->fields.elnum);
    assert(value->parser_info.bs != NULL);

    //TRACE("value=%p idx=%ld value->next_delim=%d", value, idx, value->next_delim);

    if (!(value->dpos[idx] & 0x80000000)) {
        bytestream_t *bs;
        off_t spos, epos;
        char delim;
        int nidx = idx + 1;
        lkit_type_t **fty;
        off_t (*_dparser_reach_value)(bytestream_t *, char, off_t, off_t);

        bs = value->parser_info.bs;
        delim = value->type->delim;

        if (value->type->parser == LKIT_PARSER_MDELIM) {
            _dparser_reach_value = dparser_reach_value_m_pos;
        } else {
            _dparser_reach_value = dparser_reach_value_pos;
        }


        if (nidx >= value->next_delim) {
            off_t pos;

            /*
             * XXX vmethod in lkit_struct_t ?
             * XXX value->type->_dparser_reach_value()
             */
            //TRACE("br %ld:%ld",
            //      value->parser_info.br.start,
            //      value->parser_info.br.end);

            pos = value->parser_info.pos;

            do {

                value->dpos[value->next_delim] = pos;
                //TRACE("value->dpos[%d]=%ld", value->next_delim, value->dpos[value->next_delim]);
                pos = _dparser_reach_value(bs,
                                           delim,
                                           pos,
                                           value->parser_info.br.end);
                pos = dparser_reach_delim_pos(bs,
                                              delim,
                                              pos,
                                              value->parser_info.br.end);
            } while (value->next_delim++ < nidx);
            assert(value->next_delim == (nidx + 1));

            value->parser_info.pos = pos;
        }

        //TRACE("value->next_delim=%d nidx=%d", value->next_delim, nidx);

        spos = _dparser_reach_value(bs,
                                    delim,
                                    value->dpos[idx],
                                    value->parser_info.br.end);
        epos = value->dpos[nidx] & ~0x80000000;
        value->dpos[idx] |= 0x80000000;

        //TRACE("idx=%ld spos=%ld epos=%ld", idx, spos, epos);

        val = MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);
        //TRACE("value->next_delim=%d val=%p", value->next_delim, val);

        //D32(value->dpos, sizeof(off_t) * nidx);

        fty = ARRAY_GET(lkit_type_t *, &value->type->fields, idx);

        switch ((*fty)->tag) {
        case LKIT_ARRAY:
            *val = mrklkit_rt_array_new_gc((lkit_array_t *)*fty);
            break;

        case LKIT_DICT:
            *val = mrklkit_rt_dict_new_gc((lkit_dict_t *)*fty);
            break;

        case LKIT_STRUCT:
            {
                byterange_t br;

                br.start = spos;
                br.end = epos;
                *val = mrklkit_rt_struct_new_gc((lkit_struct_t *)*fty);
                dparse_struct_setup(bs, &br, *val);
            }
            break;

        default:
            break;
        }

        //TRACE("spos=%ld epos=%ld", spos, epos);
        //TRACE("c='%c'", SNCHR(bs, epos));
        cb(bs, delim, spos, epos, val);

    } else {
        val = MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);
    }

    return val;
}


int64_t
dparse_struct_item_seq_int(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    val = (int64_t *)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_int_pos);
    return *val;
}


int64_t
dparse_struct_item_ra_int(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(int64_t *, val, dparse_int_pos);
    return *val;
}


double
dparse_struct_item_seq_float(rt_struct_t *value, int64_t idx)
{
    union {
        void **v;
        double *d;
    } u;

    assert(sizeof(double) == sizeof(void *));
    u.v = dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_float_pos);
    return *u.d;
}


double
dparse_struct_item_ra_float(rt_struct_t *value, int64_t idx)
{
    union {
        void **v;
        double *d;
    } u;

    assert(sizeof(double) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(void **, u.v, dparse_float_pos_pvoid);
    return *u.d;
}


int64_t
dparse_struct_item_seq_bool(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    val = (int64_t *)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_int_pos);
    return *val;
}


int64_t
dparse_struct_item_ra_bool(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    val = (int64_t *)dparse_struct_item_ra(value,
            idx, (dparse_struct_item_cb_t)dparse_int_pos);
    return *val;
}


bytes_t *
dparse_struct_item_seq_str(rt_struct_t *value, int64_t idx)
{
    UNUSED bytes_t **val;

    assert(sizeof(bytes_t *) == sizeof(void *));
    val = (bytes_t **)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_str_pos);
    return *(bytes_t **)(value->fields + idx);
}


bytes_t *
dparse_struct_item_ra_str(rt_struct_t *value, int64_t idx)
{
    UNUSED bytes_t **val;

    assert(sizeof(bytes_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(bytes_t **, val, dparse_str_pos);
    return *(bytes_t **)(value->fields + idx);
}


rt_array_t *
dparse_struct_item_seq_array(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_array_t **val;

    assert(sizeof(rt_array_t *) == sizeof(void *));
    val = (rt_array_t **)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_array_pos);
    return *(rt_array_t **)(value->fields + idx);
}


rt_array_t *
dparse_struct_item_ra_array(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_array_t **val;

    assert(sizeof(rt_array_t *) == sizeof(void *));
    val = (rt_array_t **)dparse_struct_item_ra(value,
            idx, (dparse_struct_item_cb_t)dparse_array_pos);
    return *(rt_array_t **)(value->fields + idx);
}


rt_dict_t *
dparse_struct_item_seq_dict(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_dict_t **val;

    assert(sizeof(rt_dict_t *) == sizeof(void *));
    val = (rt_dict_t **)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_dict_pos);
    return *(rt_dict_t **)(value->fields + idx);
}


rt_dict_t *
dparse_struct_item_ra_dict(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_dict_t **val;

    assert(sizeof(rt_dict_t *) == sizeof(void *));
    val = (rt_dict_t **)dparse_struct_item_ra(value,
            idx, (dparse_struct_item_cb_t)dparse_dict_pos);
    return *(rt_dict_t **)(value->fields + idx);
}


rt_struct_t *
dparse_struct_item_seq_struct(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_struct_t **val;

    assert(sizeof(rt_struct_t *) == sizeof(void *));

    val = (rt_struct_t **)dparse_struct_item_seq(value,
            idx, (dparse_struct_item_cb_t)dparse_struct_pos);
    //TRACE("val=%p", val);
    return *(rt_struct_t **)(value->fields + idx);
}


rt_struct_t *
dparse_struct_item_ra_struct(rt_struct_t *value, int64_t idx)
{
    UNUSED rt_struct_t **val;

    assert(sizeof(rt_struct_t *) == sizeof(void *));

    val = (rt_struct_t **)dparse_struct_item_ra(value,
            idx, (dparse_struct_item_cb_t)dparse_struct_pos);
    //TRACE("val=%p", val);
    return *(rt_struct_t **)(value->fields + idx);
}


void
dparse_rt_struct_dump(rt_struct_t *value)
{
    lkit_type_t **fty;
    array_iter_t it;

    //TRACE("value=%p", value);

    TRACEC("< ");
    for (fty = array_first(&value->type->fields, &it);
         fty != NULL;
         fty = array_next(&value->type->fields, &it)) {

        switch ((*fty)->tag) {
        case LKIT_INT:
            TRACEC("%ld ", dparse_struct_item_seq_int(value, it.iter));
            break;

        case LKIT_FLOAT:
            TRACEC("%lf ", dparse_struct_item_seq_float(value, it.iter));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = dparse_struct_item_seq_str(value, it.iter);
                TRACEC("'%s' ", v != NULL ? v->data : NULL);
            }
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_dump(
                dparse_struct_item_seq_array(value, it.iter));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_dump(
                dparse_struct_item_seq_dict(value, it.iter));
            break;

        case LKIT_STRUCT:
            dparse_rt_struct_dump(
                dparse_struct_item_seq_struct(value, it.iter));
            break;

        default:
            FAIL("dparse_rt_struct_dump");
        }
    }
    TRACEC("> ");
}


/**
 *
 *
 */
int
dparser_read_lines(int fd,
                   bytestream_t *bs,
                   dparser_read_lines_cb_t cb,
                   dparser_bytestream_recycle_cb_t rcb,
                   void *udata,
                   size_t *nlines)
{
    int res = 0;
    off_t diff;
    byterange_t br;

    br.end = OFF_MAX;
    bytestream_rewind(bs);

    while (1) {


        //sleep(1);

        br.start = SPOS(bs);
        if (dparser_reach_delim_readmore(bs,
                                         fd,
                                         '\n',
                                         br.end) == DPARSE_EOD) {

            res = DPARSE_EOD;
            break;
        }

        br.end = SPOS(bs);

        //TRACE("start=%ld end=%ld sz=%ld", br.start, br.end, br.end - br.start);
        //D32(SDATA(bs, br.start), br.end - br.start);

        //if ((br.end - br.start) <= 0) {
        //    res = DPARSE_EOD;
        //    break;
        //}

        if ((res = cb(bs, &br, udata)) != 0) {
            ++(*nlines);
            break;
        }

        dparser_reach_value(bs, '\n', SEOD(bs));
        br.end = SEOD(bs);

        if (SPOS(bs) >= bs->growsz - 8192) {
            UNUSED off_t recycled;

            recycled = bytestream_recycle(bs, 0, SPOS(bs));
            br.end = SEOD(bs);

            if (rcb != NULL && rcb(udata) != 0) {
                res = DPARSER_READ_LINES + 1;
                ++(*nlines);
                break;
            }
        }
        ++(*nlines);
    }

    diff = SPOS(bs) - SEOD(bs);
    assert(diff <= 0);

    /* shift back ... */
    if (diff < 0) {
        if (lseek(fd, diff, SEEK_CUR) < 0) {
            res = DPARSER_READ_LINES + 2;
        }
    }

    TRRET(res);
}

void
dparser_set_mpool(mpool_ctx_t *m)
{
    mpool = m;
}

