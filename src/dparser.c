#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <bzlib.h>

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

#define DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR 1

static mpool_ctx_t *mpool;

typedef void (*dparse_struct_item_cb_t)(bytestream_t *, char, off_t, off_t, void **);

/*
 * reach
 */

/*
 * deprecate
 */
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
dparser_reach_delim_readmore_unix(bytestream_t *bs, int fd, off_t epos)
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
        if (SPCHR(bs) == '\n' || SPCHR(bs) == '\0') {
            return 0;
        }
        SINCR(bs);
    }
    return DPARSE_EOD;
}


static int
dparser_reach_delim_readmore_win(bytestream_t *bs, int fd, off_t epos)
{
    int state;

    state = 0;
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
        if (SPCHR(bs) == '\r') {
            state = DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR;
        } else if (SPCHR(bs) == '\n') {
            if (state == DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR) {
                return 0;
            } else {
                state = 0;
            }
        } else if (SPCHR(bs) == '\0') {
            return 0;
        }
        SINCR(bs);
    }
    return DPARSE_EOD;
}


static ssize_t
bytestream_read_more_bz2(bytestream_t *bs, BZFILE *bzf, ssize_t sz)
{
    ssize_t nread;
    ssize_t need;
    dparse_bz2_ctx_t *ctx;

    assert(bzf != NULL);
    ctx = bs->udata;

    need = (bs->eod + sz) - bs->buf.sz;
    if (need > 0) {
        if (bytestream_grow(bs,
                            (need < bs->growsz) ?
                            bs->growsz :
                            need) != 0) {
            return -1;
        }
    }
    if ((nread = BZ2_bzread(bzf, bs->buf.data + bs->eod, sz)) >= 0) {
        bs->eod += nread;
    }
    ctx->fpos += nread;
    return nread;
}


static int
dparser_reach_delim_readmore_bz2_unix(bytestream_t *bs,
                                      BZFILE *bzf,
                                      off_t epos)
{
    while (SPOS(bs) <= epos) {
        //TRACE("SNEEDMORE=%d SPOS=%ld epos=%ld",
        //      SNEEDMORE(bs), SPOS(bs), epos);
        //TRACE("SPCHR='%c'", SPCHR(bs));
        if (SNEEDMORE(bs)) {
            if (bytestream_read_more_bz2(bs, bzf, bs->growsz) <= 0) {
                return DPARSE_EOD;
            }
            epos = SEOD(bs);
        }
        if (SPCHR(bs) == '\n' || SPCHR(bs) == '\0') {
            return 0;
        }
        SINCR(bs);
    }
    return DPARSE_EOD;
}


static int
dparser_reach_delim_readmore_bz2_win(bytestream_t *bs,
                                     BZFILE *bzf,
                                     off_t epos)
{
    int state;

    state = 0;
    while (SPOS(bs) <= epos) {
        //TRACE("SNEEDMORE=%d SPOS=%ld epos=%ld",
        //      SNEEDMORE(bs), SPOS(bs), epos);
        //TRACE("SPCHR='%c'", SPCHR(bs));
        if (SNEEDMORE(bs)) {
            if (bytestream_read_more_bz2(bs, bzf, bs->growsz) <= 0) {
                return DPARSE_EOD;
            }
            epos = SEOD(bs);
        }
        if (SPCHR(bs) == '\r') {
            state = DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR;
        } else if (SPCHR(bs) == '\n') {
            if (state == DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR) {
                return 0;
            } else {
                state = 0;
            }
        } else if (SPCHR(bs) == '\0') {
            return 0;
        }
        SINCR(bs);
    }
    return DPARSE_EOD;
}


static void
dparser_reach_end(bytestream_t *bs, off_t epos)
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


/*
 * __a1
 *  bs: SNCHR(bs, spos)
 *  bytes: str->data[spos]
 */
#define DPARSER_REACH_DELIM_POS_BODY(__a1)     \
    while (spos < epos) {                      \
        if (__a1 == delim || __a1 == '\0') {   \
            break;                             \
        }                                      \
        ++spos;                                \
    }                                          \
    return spos;                               \


static off_t
dparser_reach_delim_pos_bs(bytestream_t *bs,
                           char delim,
                           off_t spos,
                           off_t epos)
{
    DPARSER_REACH_DELIM_POS_BODY(SNCHR(bs, spos))
}


static off_t
dparser_reach_delim_pos_bytes(bytes_t *str,
                              char delim,
                              off_t spos,
                              off_t epos)
{
    DPARSER_REACH_DELIM_POS_BODY(str->data[spos])
}


/*
 * __a1
 *  bs: SNCHR(bs, spos)
 *  bytes: str->data[spos]
 */
#define DPARSER_REACH_VALUE_POS_BODY(__a1)     \
    if (spos < epos && __a1 == delim) {        \
        ++spos;                                \
    }                                          \
    return spos;                               \


static off_t
dparser_reach_value_pos_bs(bytestream_t *bs,
                           char delim,
                           off_t spos,
                           off_t epos)
{
    DPARSER_REACH_VALUE_POS_BODY(SNCHR(bs, spos))
}


static off_t
dparser_reach_value_pos_bytes(bytes_t *str,
                              char delim,
                              off_t spos,
                              off_t epos)
{
    DPARSER_REACH_VALUE_POS_BODY(str->data[spos])
}


static off_t
dparser_reach_value_m_pos(bytestream_t *bs, char delim, off_t spos, off_t epos)
{
    while (spos < epos && SNCHR(bs, spos) == delim) {
        ++spos;
    }
    return spos;
}


/*
 * strto*
 */

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


/*
 * int
 */

/*
 * __a1
 *  bs: SDATA(bs, spos)
 *  bytes: str->data + spos
 */
#define DPARSE_INT_POS_BODY(__a1)                      \
    char *ptr = (char *)__a1;                          \
    char *endptr = ptr;                                \
    *val = dparser_strtoi64(ptr, &endptr, delim);      \
    return spos + endptr - ptr;                        \


static off_t
dparse_int_pos_bs(bytestream_t *bs,
                  char delim,
                  off_t spos,
                  UNUSED off_t epos,
                  int64_t *val)
{
    DPARSE_INT_POS_BODY(SDATA(bs, spos))
}


static off_t
dparse_int_pos_bytes(bytes_t *str,
                     char delim,
                     off_t spos,
                     UNUSED off_t epos,
                     int64_t *val)
{
    DPARSE_INT_POS_BODY(str->data + spos)
}


/*
 * float
 */

/*
 * __a1
 *  bs: SDATA(bs, spos)
 *  bytes: str->data + spos
 */
#define DPARSE_FLOAT_POS_BODY(__a1)            \
    char *ptr = (char *)__a1;                  \
    char *endptr = ptr;                        \
    *val = dparser_strtod(ptr, &endptr, delim);\
    return spos + endptr - ptr;                \


static off_t
dparse_float_pos_bs(bytestream_t *bs,
                    char delim,
                    off_t spos,
                    UNUSED off_t epos,
                    double *val)
{
    DPARSE_FLOAT_POS_BODY(SDATA(bs, spos))
}


static off_t
dparse_float_pos_bytes(bytes_t *str,
                       char delim,
                       off_t spos,
                       UNUSED off_t epos,
                       double *val)
{
    DPARSE_FLOAT_POS_BODY(str->data + spos)
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


/*
 * str
 */

/*
 * __a1
 *  bs: SNCHR(bs, i)
 *  bytes: (str)->data[i]
 *
 * __a2
 *  bs: SDATA(bs, spos)
 *  bytes: (str)->data + spos
 */
#define DPARSE_STR_POS_BODY(__a1, __a2)                        \
    off_t i;                                                   \
    for (i = spos; i < epos && (char)__a1 != delim; ++i) {     \
        ;                                                      \
    }                                                          \
    *val = mrklkit_rt_bytes_new_gc(i - spos + 1);              \
    memcpy((*val)->data, __a2, (*val)->sz - 1);                \
    *((*val)->data + (*val)->sz - 1) = '\0';                   \
    BYTES_INCREF(*val);                                        \
    return i;                                                  \


static off_t
dparse_str_pos_bs(bytestream_t *bs,
                  char delim,
                  off_t spos,
                  off_t epos,
                  bytes_t **val)
{
    DPARSE_STR_POS_BODY(SNCHR(bs, i), SDATA(bs, spos))
}


static off_t
dparse_str_pos_bytes(bytes_t *str,
                     char delim,
                     off_t spos,
                     off_t epos,
                     bytes_t **val)
{
    DPARSE_STR_POS_BODY(str->data[i], str->data + spos)
}


#define DPARSE_STR_POS_BRUSHDOWN_BODY(__a1, __a2)              \
    off_t i;                                                   \
    for (i = spos; i < epos && (char)__a1 != delim; ++i) {     \
        ;                                                      \
    }                                                          \
    *val = mrklkit_rt_bytes_new_gc(i - spos + 1);              \
    memcpy((*val)->data, __a2, (*val)->sz - 1);                \
    *((*val)->data + (*val)->sz - 1) = '\0';                   \
    bytes_brushdown(*val);                                     \
    BYTES_INCREF(*val);                                        \
    return i;                                                  \


static off_t
dparse_str_pos_brushdown_bs(bytestream_t *bs,
                            char delim,
                            off_t spos,
                            off_t epos,
                            bytes_t **val)
{
    DPARSE_STR_POS_BRUSHDOWN_BODY(SNCHR(bs, i), SDATA(bs, spos))
}


static off_t
dparse_str_pos_brushdown_bytes(bytes_t *str,
                               char delim,
                               off_t spos,
                               off_t epos,
                               bytes_t **val)
{
    DPARSE_STR_POS_BRUSHDOWN_BODY(str->data[i], str->data + spos)
}


static size_t
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
    return i;
}


#   define QSTR_ST_START   (0)
#   define QSTR_ST_QUOTE   (1)
#   define QSTR_ST_IN      (2)
#   define QSTR_ST_OUT     (3)
/*
 * __a1
 *  bs: SNCHR(bs, br.end)
 *  bytes: str->data[br.end]
 *
 * __a2
 *  bs: SDATA(bs, br.start + 1)
 *  bytes: str->data + br.start + 1
 */
#define DPARSE_QSTR_POS_BODY(__a1, __a2)                       \
    byterange_t br;                                            \
    int state;                                                 \
    size_t sz;                                                 \
    state = QSTR_ST_START;                                     \
    br.start = spos;                                           \
    br.end = spos;                                             \
    for (br.end = spos; br.end < epos; ++br.end) {             \
        char ch;                                               \
        ch = (char)__a1;                                       \
        switch (state) {                                       \
        case QSTR_ST_START:                                    \
            if (ch == '"') {                                   \
                state = QSTR_ST_IN;                            \
            } else {                                           \
                /*                                             \
                 * garbage before the opening "                \
                 */                                            \
                goto end;                                      \
            }                                                  \
            break;                                             \
        case QSTR_ST_IN:                                       \
            if (ch == '"') {                                   \
                state = QSTR_ST_QUOTE;                         \
            }                                                  \
            break;                                             \
        case QSTR_ST_QUOTE:                                    \
            if (ch == '"') {                                   \
                state = QSTR_ST_IN;                            \
            } else {                                           \
                /* one beyond the closing " */                 \
                state = QSTR_ST_OUT;                           \
                goto end;                                      \
            }                                                  \
                                                               \
            break;                                             \
        default:                                               \
            assert(0);                                         \
        }                                                      \
    }                                                          \
end:                                                           \
    if (state != QSTR_ST_OUT && state != QSTR_ST_QUOTE) {      \
        sz = 0;                                                \
    } else {                                                   \
        sz = br.end - br.start;                                \
    }                                                          \
    *val = mrklkit_rt_bytes_new_gc(sz + 1);                    \
    /* unescape starting from next to the starting " */        \
    if (sz > 2) {                                              \
        sz = qstr_unescape((char *)(*val)->data,               \
                           (char *)__a2,                       \
                           /* minus "" */                      \
                           br.end - br.start - 2);             \
    } else {                                                   \
        sz = 0;                                                \
    }                                                          \
    (*val)->data[sz] = '\0';                                   \
    (*val)->sz = sz + 1;                                       \
    BYTES_INCREF(*val);                                        \
    return br.end;                                             \


static off_t
dparse_qstr_pos_bs(bytestream_t *bs,
                   off_t spos,
                   off_t epos,
                   bytes_t **val)
{
    DPARSE_QSTR_POS_BODY(SNCHR(bs, br.end), SDATA(bs, br.start + 1))
}


static off_t
dparse_qstr_pos_bytes(bytes_t *str,
                      off_t spos,
                      off_t epos,
                      bytes_t **val)
{
    DPARSE_QSTR_POS_BODY(str->data[br.end], str->data + br.start + 1)
}


/*
 * __a1
 *  bs: SNCHR(bs, spos)
 *  bytes: str->data[spos]
 */
#define DPARSE_OPTQSTR_POS_BODY(kind, in, __a1)                        \
    if (__a1 == '"') {                                                 \
        return dparse_qstr_pos_##kind(in, spos, epos, val);            \
    } else {                                                           \
        return dparse_str_pos_##kind(in, delim, spos, epos, val);      \
    }                                                                  \
    return -1;                                                         \


static off_t
dparse_optqstr_pos_bs(bytestream_t *bs,
                      char delim,
                      off_t spos,
                      off_t epos,
                      bytes_t **val)
{
    DPARSE_OPTQSTR_POS_BODY(bs, bs, SNCHR(bs, spos))
}


static off_t
dparse_optqstr_pos_bytes(bytes_t *str,
                         char delim,
                         off_t spos,
                         off_t epos,
                         bytes_t **val)
{
    DPARSE_OPTQSTR_POS_BODY(bytes, str, str->data[spos])
}


/*
 * array
 */
static void
dparse_array_pos(bytestream_t *bs,
                 char delim,
                 off_t spos,
                 off_t epos,
                 rt_array_t **val)
{
    char fdelim;
    lkit_type_t *afty;
    unsigned idx;

    fdelim = (*val)->type->delim;
    if ((afty = lkit_array_get_element_type((*val)->type)) == NULL) {
        FAIL("lkit_array_get_element_type");
    }

#define DPARSE_ARRAY_CASE(ty, fn)                                      \
    idx = 0;                                                           \
    while (spos < epos && SNCHR(bs, spos) != delim) {                  \
        off_t fpos;                                                    \
        union {                                                        \
            void *v;                                                   \
            ty x;                                                      \
        } u;                                                           \
        void **v;                                                      \
        fpos = dparser_reach_delim_pos_bs(bs, fdelim, spos, epos);     \
        (void)fn(bs, fdelim, spos, fpos, &u.x);                        \
        v = array_get_safe_mpool(mpool, &(*val)->fields, idx);         \
        *v = u.v;                                                      \
        spos = dparser_reach_value_pos_bs(bs, fdelim, fpos, epos);     \
        ++idx;                                                         \
    }                                                                  \

    switch (afty->tag) {
    case LKIT_STR:
        DPARSE_ARRAY_CASE(bytes_t *, dparse_str_pos_bs);
        break;

    case LKIT_INT:
        DPARSE_ARRAY_CASE(int64_t, dparse_int_pos_bs);
        break;

    case LKIT_FLOAT:
        DPARSE_ARRAY_CASE(double, dparse_float_pos_bs);
        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_array_pos");
    }
}

/*
 * dict
 */

/*
 * __a1
 *  bs: SNCHR(bs, spos)
 *  bytes: str->data[spos]
 */
#define DPARSE_DICT_CASE(kind, in, ty, fn, __a1)                               \
    while (spos < epos && __a1 != delim) {                                     \
        off_t kvpos, fpos;                                                     \
        dict_item_t *it;                                                       \
        bytes_t *key = NULL;                                                   \
        union {                                                                \
            void *v;                                                           \
            ty x;                                                              \
        } u;                                                                   \
        kvpos = dparser_reach_delim_pos_##kind(in, kvdelim, spos, epos);       \
        (void)_dparse_str_pos_k(in, kvdelim, spos, kvpos, &key);               \
        spos = dparser_reach_value_pos_##kind(in, kvdelim, kvpos, epos);       \
        if ((it = dict_get_item(&(*val)->fields, key)) == NULL) {              \
            fpos = fn(in, fdelim, spos, epos, &u.x);                           \
            dict_set_item_mpool(mpool, &(*val)->fields, key, u.v);             \
            fpos = dparser_reach_delim_pos_##kind(in, fdelim, fpos, epos);     \
        } else {                                                               \
            BYTES_DECREF(&key);                                          \
            fpos = dparser_reach_delim_pos_##kind(in, fdelim, spos, epos);     \
        }                                                                      \
        spos = dparser_reach_value_pos_##kind(in, fdelim, fpos, epos);         \
    }                                                                          \



#define DPARSE_DICT_POS_BODY(kind, in, __a1)                                   \
    char kvdelim;                                                              \
    char fdelim;                                                               \
    lkit_type_t *dfty;                                                         \
    off_t (*_dparse_str_pos_k)(__typeof(in), char, off_t, off_t, bytes_t **);  \
    kvdelim = (*val)->type->kvdelim;                                           \
    fdelim = (*val)->type->fdelim;                                             \
    if ((dfty = lkit_dict_get_element_type((*val)->type)) == NULL) {           \
        FAIL("lkit_dict_get_element_type");                                    \
    }                                                                          \
    if ((*val)->type->parser == LKIT_PARSER_SMARTDELIM) {                      \
        _dparse_str_pos_k = dparse_str_pos_brushdown_##kind;                   \
    } else {                                                                   \
        _dparse_str_pos_k = dparse_str_pos_##kind;                             \
    }                                                                          \
    switch (dfty->tag) {                                                       \
    case LKIT_STR:                                                             \
        {                                                                      \
            off_t (*_dparse_str_pos_v)(__typeof(in),                           \
                                       char,                                   \
                                       off_t,                                  \
                                       off_t,                                  \
                                       bytes_t **);                            \
            if ((*val)->type->parser == LKIT_PARSER_OPTQSTRDELIM) {            \
                _dparse_str_pos_v = dparse_optqstr_pos_##kind;                 \
            } else {                                                           \
                _dparse_str_pos_v = dparse_str_pos_##kind;                     \
            }                                                                  \
            DPARSE_DICT_CASE(kind, in, bytes_t *, _dparse_str_pos_v, __a1);    \
        }                                                                      \
        break;                                                                 \
                                                                               \
    case LKIT_INT:                                                             \
        DPARSE_DICT_CASE(kind, in, int64_t, dparse_int_pos_##kind, __a1);      \
        break;                                                                 \
                                                                               \
    case LKIT_FLOAT:                                                           \
        DPARSE_DICT_CASE(kind, in, double, dparse_float_pos_##kind, __a1);     \
        break;                                                                 \
                                                                               \
    default:                                                                   \
        /* cannot be recursively nested */                                     \
        FAIL("dparse_dict_pos");                                               \
    }                                                                          \



static void
dparse_dict_pos_bs(bytestream_t *bs,
                char delim,
                off_t spos,
                off_t epos,
                rt_dict_t **val)
{
    DPARSE_DICT_POS_BODY(bs, bs, SNCHR(bs, spos))
}


static void
dparse_dict_pos_bytes(bytes_t *str,
                      char delim,
                      off_t spos,
                      off_t epos,
                      rt_dict_t **val)
{
    DPARSE_DICT_POS_BODY(bytes, str, str->data[spos])
}


rt_dict_t *
dparse_dict_from_bytes(lkit_dict_t *ty, bytes_t *str)
{
    rt_dict_t *val;

    val = mrklkit_rt_dict_new_gc(ty);
    dparse_dict_pos_bytes(str, '\0', 0, str->sz, &val);
    DICT_INCREF(val);
    return val;
}


/*
 * struct
 */
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


#define DPARSE_STRUCT_ITEM_RA_BODY(val, cb, __a1)                              \
    assert(idx < (ssize_t)value->type->fields.elnum);                          \
    assert(value->parser_info.bs != NULL);                                     \
    if (!(value->dpos[idx] & 0x80000000)) {                                    \
        bytestream_t *bs;                                                      \
        off_t spos, epos;                                                      \
        char delim;                                                            \
        int nidx = idx + 1;                                                    \
        off_t (*_dparser_reach_value)(bytestream_t *, char, off_t, off_t);     \
        lkit_type_t **fty;                                                     \
        bs = value->parser_info.bs;                                            \
        delim = value->type->delim;                                            \
        if (value->type->parser == LKIT_PARSER_MDELIM) {                       \
            _dparser_reach_value = dparser_reach_value_m_pos;                  \
        } else {                                                               \
            _dparser_reach_value = dparser_reach_value_pos_bs;                 \
        }                                                                      \
        if (nidx >= value->next_delim) {                                       \
            off_t pos;                                                         \
            pos = value->parser_info.pos;                                      \
            do {                                                               \
                lkit_parser_t *fpa;                                            \
                value->dpos[value->next_delim] = pos;                          \
                pos = _dparser_reach_value(bs,                                 \
                                           delim,                              \
                                           pos,                                \
                                           value->parser_info.br.end);         \
                if (value->next_delim < (ssize_t)value->type->fields.elnum) {  \
                    fty = ARRAY_GET(lkit_type_t *,                             \
                                    &value->type->fields,                      \
                                    value->next_delim);                        \
                    fpa = ARRAY_GET(lkit_parser_t,                             \
                                    &value->type->parsers,                     \
                                    value->next_delim);                        \
                    if ((*fty)->tag == LKIT_STR) {                             \
                        val = (__typeof(val))                                  \
                                MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value,         \
                            value->next_delim);                                \
                        if (*fpa == LKIT_PARSER_QSTR) {                        \
                            /*                                                 \
                             * Special case for quoted string.                 \
                             */                                                \
                            pos = dparse_qstr_pos_bs(                          \
                                bs, pos, value->parser_info.br.end,            \
                                (bytes_t **)val);                              \
                            value->dpos[value->next_delim] |= 0x80000000;      \
                        } else if (*fpa == LKIT_PARSER_OPTQSTR) {              \
                            /*                                                 \
                             * Special case for optionally quoted string.      \
                             */                                                \
                            pos = dparse_optqstr_pos_bs(                       \
                                bs, delim, pos, value->parser_info.br.end,     \
                                (bytes_t **)val);                              \
                            value->dpos[value->next_delim] |= 0x80000000;      \
                        } else {                                               \
                            pos = dparser_reach_delim_pos_bs(                  \
                                bs, delim, pos, value->parser_info.br.end);    \
                        }                                                      \
                    } else {                                                   \
                        pos = dparser_reach_delim_pos_bs(                      \
                            bs, delim, pos, value->parser_info.br.end);        \
                    }                                                          \
                } else {                                                       \
                    pos = dparser_reach_delim_pos_bs(                          \
                        bs, delim, pos, value->parser_info.br.end);            \
                }                                                              \
            } while (value->next_delim++ < nidx);                              \
            assert(value->next_delim == (nidx + 1) ||                          \
                   value->next_delim == (ssize_t)value->type->fields.elnum);   \
            value->parser_info.pos = pos;                                      \
        }                                                                      \
        if (!(value->dpos[idx] & 0x80000000)) {                                \
            spos = _dparser_reach_value(bs,                                    \
                                        delim,                                 \
                                        value->dpos[idx],                      \
                                        value->parser_info.br.end);            \
            epos = value->dpos[nidx] & ~0x80000000;                            \
            value->dpos[idx] |= 0x80000000;                                    \
            val = (__typeof(val))MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);  \
            __a1;                                                              \
            cb(bs, delim, spos, epos, val);                                    \
        } else {                                                               \
            val = (__typeof(val))MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);  \
        }                                                                      \
    } else {                                                                   \
        val = (__typeof(val))MRKLKIT_RT_GET_STRUCT_ITEM_ADDR(value, idx);      \
    }                                                                          \


int64_t
dparse_struct_item_ra_int(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(val, dparse_int_pos_bs,);
    return *val;
}


double
dparse_struct_item_ra_float(rt_struct_t *value, int64_t idx)
{
    union {
        void **v;
        double *d;
    } u;

    assert(sizeof(double) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(u.v, dparse_float_pos_pvoid,);
    return *u.d;
}


int64_t
dparse_struct_item_ra_bool(rt_struct_t *value, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(val,
                               dparse_int_pos_bs,);
    return *val;
}


bytes_t *
dparse_struct_item_ra_str(rt_struct_t *value, int64_t idx)
{
    bytes_t **val;

    assert(sizeof(bytes_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(val, dparse_str_pos_bs,);
    return *(bytes_t **)(value->fields + idx);
}


rt_array_t *
dparse_struct_item_ra_array(rt_struct_t *value, int64_t idx)
{
    rt_array_t **val;

    assert(sizeof(rt_array_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(
        val,
        dparse_array_pos,
            fty = ARRAY_GET(lkit_type_t *, &value->type->fields, idx);
            *val = mrklkit_rt_array_new_gc((lkit_array_t *)*fty);
            ARRAY_INCREF(*val);
    );
    return *(rt_array_t **)(value->fields + idx);
}


rt_dict_t *
dparse_struct_item_ra_dict(rt_struct_t *value, int64_t idx)
{
    rt_dict_t **val;

    assert(sizeof(rt_dict_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(
        val,
        dparse_dict_pos_bs,
            fty = ARRAY_GET(lkit_type_t *, &value->type->fields, idx);
            *val = mrklkit_rt_dict_new_gc((lkit_dict_t *)*fty);
            DICT_INCREF(*val);
    );
    return *(rt_dict_t **)(value->fields + idx);
}


rt_struct_t *
dparse_struct_item_ra_struct(rt_struct_t *value, int64_t idx)
{
    rt_struct_t **val;

    assert(sizeof(rt_struct_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(
        val,
        dparse_struct_pos,
        {
            byterange_t br;
            br.start = spos;
            br.end = epos;
            fty = ARRAY_GET(lkit_type_t *, &value->type->fields, idx);
            *val = mrklkit_rt_struct_new_gc((lkit_struct_t *)*fty);
            dparse_struct_setup(bs, &br, *val);
            STRUCT_INCREF(*val);
        });
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
            TRACEC("%ld ", dparse_struct_item_ra_int(value, it.iter));
            break;

        case LKIT_FLOAT:
            TRACEC("%lf ", dparse_struct_item_ra_float(value, it.iter));
            break;

        case LKIT_STR:
            {
                bytes_t *v;

                v = dparse_struct_item_ra_str(value, it.iter);
                TRACEC("'%s' ", v != NULL ? v->data : NULL);
            }
            break;

        case LKIT_ARRAY:
            mrklkit_rt_array_dump(
                dparse_struct_item_ra_array(value, it.iter));
            break;

        case LKIT_DICT:
            mrklkit_rt_dict_dump(
                dparse_struct_item_ra_dict(value, it.iter));
            break;

        case LKIT_STRUCT:
            dparse_rt_struct_dump(
                dparse_struct_item_ra_struct(value, it.iter));
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

#define DPARSER_READ_LINES_BODY(dparser_reach_delim_readmore_fn, __a1) \
    int res = 0;                                                       \
    off_t diff;                                                        \
    byterange_t br;                                                    \
    br.end = OFF_MAX;                                                  \
    bytestream_rewind(bs);                                             \
    while (1) {                                                        \
        br.start = SPOS(bs);                                           \
        if (dparser_reach_delim_readmore_fn(bs,                        \
                                            fd,                        \
                                            br.end) == DPARSE_EOD) {   \
            res = DPARSE_EOD;                                          \
            break;                                                     \
        }                                                              \
        br.end = SPOS(bs);                                             \
        (*nbytes) += br.end - br.start + 1;                            \
        ++(*nlines);                                                   \
        __a1;                                                          \
        if ((res = cb(bs, &br, udata)) != 0) {                         \
            break;                                                     \
        }                                                              \
        dparser_reach_end(bs, SEOD(bs));                               \
        br.end = SEOD(bs);                                             \
        if (SPOS(bs) >= bs->growsz - 8192) {                           \
            off_t recycled;                                            \
            recycled = bytestream_recycle(bs, 0, SPOS(bs));            \
            br.end = SEOD(bs);                                         \
            br.start -= recycled;                                      \
            if (rcb != NULL && rcb(udata) != 0) {                      \
                res = DPARSER_READ_LINES + 1;                          \
                break;                                                 \
            }                                                          \
        }                                                              \
    }                                                                  \
    diff = SPOS(bs) - SEOD(bs);                                        \
    assert(diff <= 0);                                                 \
    /* shift back ... */                                               \
    if (diff < 0) {                                                    \
        if (lseek(fd, diff, SEEK_CUR) < 0) {                           \
            res = DPARSER_READ_LINES + 2;                              \
        }                                                              \
    }                                                                  \
    TRRET(res);


int
dparser_read_lines_unix(int fd,
                        bytestream_t *bs,
                        dparser_read_lines_cb_t cb,
                        dparser_bytestream_recycle_cb_t rcb,
                        void *udata,
                        size_t *nlines,
                        size_t *nbytes)
{
    DPARSER_READ_LINES_BODY(dparser_reach_delim_readmore_unix,)
}


int
dparser_read_lines_win(int fd,
                       bytestream_t *bs,
                       dparser_read_lines_cb_t cb,
                       dparser_bytestream_recycle_cb_t rcb,
                       void *udata,
                       size_t *nlines,
                       size_t *nbytes)
{
    DPARSER_READ_LINES_BODY(dparser_reach_delim_readmore_win, --br.end)
}


#define DPARSER_READ_LINES_BZ2_BODY(dparser_reach_delim_readmore_bz2_fn, __a1) \
    int res = 0;                                                               \
    off_t diff;                                                                \
    byterange_t br;                                                            \
    dparse_bz2_ctx_t *bz2ctx;                                                  \
    /*                                                                         \
     * check for dparse_bz2_ctx_t *                                            \
     */                                                                        \
    assert(bs->udata != NULL);                                                 \
    bz2ctx = bs->udata;                                                        \
    bytestream_rewind(bs);                                                     \
    if (bz2ctx->tail != NULL) {                                                \
        bytestream_cat(bs, bz2ctx->tail->sz, (char *)bz2ctx->tail->data);      \
        BYTES_DECREF(&bz2ctx->tail);                                           \
    }                                                                          \
    while (1) {                                                                \
        br.start = SPOS(bs);                                                   \
        if (dparser_reach_delim_readmore_bz2_fn(bs,                            \
                                                bzf,                           \
                                                br.end) == DPARSE_EOD) {       \
            res = DPARSE_EOD;                                                  \
            break;                                                             \
        }                                                                      \
        br.end = SPOS(bs);                                                     \
        (*nbytes) += br.end - br.start + 1;                                    \
        ++(*nlines);                                                           \
        __a1;                                                                  \
        if ((res = cb(bs, &br, udata)) != 0) {                                 \
            break;                                                             \
        }                                                                      \
        dparser_reach_end(bs, SEOD(bs));                                       \
        br.end = SEOD(bs);                                                     \
        if (SPOS(bs) >= bs->growsz - 8192) {                                   \
            off_t recycled;                                                    \
            recycled = bytestream_recycle(bs, 0, SPOS(bs));                    \
            br.end = SEOD(bs);                                                 \
            br.start -= recycled;                                              \
            if (rcb != NULL && rcb(udata) != 0) {                              \
                res = DPARSER_READ_LINES + 1;                                  \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    }                                                                          \
    diff = SPOS(bs) - SEOD(bs);                                                \
    assert(diff <= 0);                                                         \
    /*                                                                         \
     * shift back ... save SPOS(bs)/SEOD(bs) to bz2ctx->tail                   \
     */                                                                        \
    if (diff < 0) {                                                            \
        diff = -diff;                                                          \
        bz2ctx->fpos -= diff;                                                  \
        assert(bz2ctx->tail == NULL);                                          \
        if ((bz2ctx->tail = bytes_new(diff)) == NULL) {                        \
            FAIL("malloc");                                                    \
        }                                                                      \
        memcpy(bz2ctx->tail->data, SPDATA(bs), diff);                          \
    }                                                                          \
    TRRET(res);


int
dparser_read_lines_bz2_unix(BZFILE *bzf,
                            bytestream_t *bs,
                            dparser_read_lines_cb_t cb,
                            dparser_bytestream_recycle_cb_t rcb,
                            void *udata,
                            size_t *nlines,
                            size_t *nbytes)
{
    DPARSER_READ_LINES_BZ2_BODY(dparser_reach_delim_readmore_bz2_unix,)
}


int
dparser_read_lines_bz2_win(BZFILE *bzf,
                           bytestream_t *bs,
                           dparser_read_lines_cb_t cb,
                           dparser_bytestream_recycle_cb_t rcb,
                           void *udata,
                           size_t *nlines,
                           size_t *nbytes)
{
    DPARSER_READ_LINES_BZ2_BODY(dparser_reach_delim_readmore_bz2_win, --br.end)
}


void
dparser_bz2_ctx_init(dparse_bz2_ctx_t *bz2ctx)
{
    bz2ctx->fpos = 0;
    bz2ctx->tail = NULL;
}


void
dparser_bz2_ctx_fini(dparse_bz2_ctx_t *bz2ctx)
{
    BYTES_DECREF(&bz2ctx->tail);
}


void
dparser_set_mpool(mpool_ctx_t *m)
{
    mpool = m;
}

