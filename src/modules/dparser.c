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



#if 0
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
dparse_kv_bytes(bytestream_t *bs,
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
        if (*ch == krdelim[0] || *ch == krdelim[1]) {
            TRACE("kvdelim was '%c' krdelim was '%c' '%c' rdelim[1] was '%c' ch='%c'",
                   kvdelim, krdelim[0], krdelim[1], rdelim[1], *ch);
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
    mrklkit_bytes_destroy(&key);
    mrklkit_bytes_destroy(&val);
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    }
    return DPARSE_ERRORVALUE;
}

#endif



















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
dparser_read_lines(int fd, void (*cb)(bytestream_t *, byterange_t *))
{
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

        TRACE("start=%ld end=%ld", br.start, br.end);
        //D32(SPDATA(&bs), br.end - br.start);

        cb(&bs, &br);

        dparser_reach_value(&bs, '\n', SEOD(&bs), 0);

        br.end = SEOD(&bs);

    }

    bytestream_fini(&bs);

    return 0;
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
    char afdelim;
    lkit_type_t *afty;
    byterange_t br;

    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    br.end = SPOS(bs);
    SPOS(bs) = br.start;

    afdelim = value->type->delim[0];
    if ((afty = lkit_array_get_element_type(value->type)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

#define DPARSE_ARRAY_CASE(fn, ty) \
        while (SPOS(bs) < epos && SPCHR(bs) != delim) { \
            union { \
                void *v; \
                int64_t x; \
            } val; \
            void **v; \
            if (dparse_int(bs, afdelim, epos, &val.x, flags) != 0) { \
                goto err; \
            } \
            if ((v = array_incr(&value->fields)) == NULL) { \
                FAIL("array_incr"); \
            } \
            *v = val.v; \
            dparser_reach_value(bs, afdelim, epos, flags); \
        }

    switch (afty->tag) {
    case LKIT_INT:
        while (SPOS(bs) < br.end && SPCHR(bs) != delim) {
            union {
                void *v;
                int64_t x;
            } val;
            void **v;

            TRACE("SPCHR0='%c'", SPCHR(bs));

            if (dparse_int(bs, afdelim, br.end, &val.x, flags) != 0) {
                if ((v = array_incr(&value->fields)) == NULL) {
                    FAIL("array_incr");
                }
                *v = val.v;
                break;
            }
            if ((v = array_incr(&value->fields)) == NULL) {
                FAIL("array_incr");
            }
            *v = val.v;
            dparser_reach_value(bs, afdelim, br.end, flags);
            TRACE("SPCHR1='%c' SPOS=%ld br.end=%ld", SPCHR(bs), SPOS(bs), br.end);
        }
        break;

    case LKIT_FLOAT:
        DPARSE_ARRAY_CASE(dparse_float, double);
        break;

    case LKIT_STR:
        DPARSE_ARRAY_CASE(dparse_str, bytes_t *);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_array");
    }

    return 0;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = br.start;
    }
    return DPARSE_ERRORVALUE;
}


int
dparse_dict(UNUSED bytestream_t *bs,
            UNUSED char delim,
            UNUSED off_t epos,
            UNUSED OUT rt_dict_t *value,
            UNUSED unsigned int flags)
{
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

    sdelim = value->type->delim[0];
    /* save */
    br.start = SPOS(bs);
    dparser_reach_delim(bs, delim, epos);
    /* adjust epos */
    br.end = SPOS(bs);

    if (!SNEEDMORE(bs)) {
        lkit_type_t **fty;
        void **val;

        /* rewind bs */
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
                    if (dparse_float(bs,
                                     sdelim,
                                     br.end,
                                     &v.d,
                                     flags) != 0) {
                        goto err;
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
                    goto err;
                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_array_t *arty = (lkit_array_t *)*fty;
                    *val = mrklkit_rt_array_new(arty);
                    if (dparse_array(bs,
                                     sdelim,
                                     br.end,
                                     (rt_array_t *)*val,
                                     flags) != 0) {
                        goto err;
                    }
                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *dcty = (lkit_dict_t *)*fty;
                    *val = mrklkit_rt_dict_new(dcty);
                    if (dparse_dict(bs,
                                    sdelim,
                                    br.end,
                                    (rt_dict_t *)*val,
                                    flags) != 0) {
                        goto err;
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
                                      ts->delim[0],
                                      br.end,
                                      (rt_struct_t *)(*val),
                                      &sch,
                                      flags) != 0) {
                        goto err;
                    }
                }
                break;

            default:
                FAIL("dparse_struct: need more types to handle in dparse");
            }
        }

        /* set end position at the delim */
        SPOS(bs) = br.end;
        *ch = SPCHR(bs);
        return 0;
    }

    return DPARSE_NEEDMORE;

err:
    return DPARSE_ERRORVALUE;
}


