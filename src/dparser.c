#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <bzlib.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream_aux.h>
//#define TRRET_DEBUG_VERBOSE
//#define TRRET_DEBUG
#include <mrkcommon/dumpm.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/util.h>

#include <mrklkit/dparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#include "diag.h"


#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(dparser);
#endif

#define DPARSER_REACH_DELIM_READMORE_WIN_STATE_CR 1


#define _array_get_safe(ar, idx) array_get_safe_mpool(mpool, (ar), (idx))


#define _dict_set_item(d, k, v) dict_set_item_mpool(mpool, (d), (k), (v))


static mpool_ctx_t *mpool;


typedef off_t (*dparse_ty_pos_t)(rt_parser_info_t *,
                                 int64_t,
                                 char,
                                 off_t,
                                 off_t,
                                 void *);
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


static off_t
dparser_reach_delim_pos(bytestream_t *bs,
                        char delim,
                        off_t spos,
                        off_t epos)
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
dparser_reach_value_pos(bytestream_t *bs,
                        char delim,
                        off_t spos,
                        off_t epos)
{
    if (spos < epos && SNCHR(bs, spos) == delim) {
        ++spos;
    }
    return spos;
}


static off_t
dparser_reach_value_m_pos(bytestream_t *bs,
                          char delim,
                          off_t spos,
                          off_t epos)
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
            // 922337203685477580
            if (res >= 0xccccccccccccccc) {
                res = INT64_MAX;
                continue;
            }
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
static off_t
dparse_int_pos(rt_parser_info_t *pi,
               UNUSED int64_t idx,
               char delim,
               off_t spos,
               UNUSED off_t epos,
               int64_t *val)
{
    char *ptr = (char *)SDATA(pi->bs, spos);
    char *endptr = ptr;
    *val = dparser_strtoi64(ptr, &endptr, delim);
    return spos + endptr - ptr;
}


/*
 * float
 */

static off_t
dparse_float_pos(rt_parser_info_t *pi,
                 UNUSED int64_t idx,
                 char delim,
                 off_t spos,
                 UNUSED off_t epos,
                 double *val)
{
    char *ptr = (char *)SDATA(pi->bs, spos);
    char *endptr = ptr;
    *val = dparser_strtod(ptr, &endptr, delim);
    return spos + endptr - ptr;
}


static off_t
dparse_float_pos_pvoid(rt_parser_info_t *pi,
                       UNUSED int64_t idx,
                       char delim,
                       off_t spos,
                       UNUSED off_t epos,
                       void **vv)
{
    char *ptr = (char *)SDATA(pi->bs, spos);
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

#define STR_POS_BODY(bytes_new_fn,__a1)                                \
    off_t i;                                                           \
    for (i = spos; i < epos && (char)SNCHR(pi->bs, i) != delim; ++i) { \
        ;                                                              \
    }                                                                  \
    *val = bytes_new_fn(i - spos + 1);                                 \
    memcpy((*val)->data, SDATA(pi->bs, spos), (*val)->sz - 1);         \
    *((*val)->data + (*val)->sz - 1) = '\0';                           \
    __a1                                                               \
    return i;                                                          \


static off_t
dparse_str_pos_mpool(rt_parser_info_t *pi,
               UNUSED int64_t idx,
               char delim,
               off_t spos,
               off_t epos,
               bytes_t **val)
{
    STR_POS_BODY(mrklkit_rt_bytes_new_mpool,)
}


static off_t
dparse_str_pos(rt_parser_info_t *pi,
               UNUSED int64_t idx,
               char delim,
               off_t spos,
               off_t epos,
               bytes_t **val)
{
    STR_POS_BODY(mrklkit_rt_bytes_new, BYTES_INCREF(*val);)
}


static off_t
dparse_str_pos_zref(rt_parser_info_t *pi,
                    UNUSED int64_t idx,
                    char delim,
                    off_t spos,
                    off_t epos,
                    bytes_t **val)
{
    STR_POS_BODY(mrklkit_rt_bytes_new,)
}


#define STR_POS_BRUSHDOWN_BODY(bytes_new_fn, __a1)                     \
    off_t i;                                                           \
    for (i = spos; i < epos && (char)SNCHR(pi->bs, i) != delim; ++i) { \
        ;                                                              \
    }                                                                  \
    *val = bytes_new_fn(i - spos + 1);                                 \
    memcpy((*val)->data, SDATA(pi->bs, spos), (*val)->sz - 1);         \
    *((*val)->data + (*val)->sz - 1) = '\0';                           \
    bytes_brushdown(*val);                                             \
    __a1                                                               \
    return i;                                                          \


static off_t
dparse_str_pos_brushdown_mpool(rt_parser_info_t *pi,
                         UNUSED int64_t idx,
                         char delim,
                         off_t spos,
                         off_t epos,
                         bytes_t **val)
{
    STR_POS_BRUSHDOWN_BODY(mrklkit_rt_bytes_new_mpool,)
}


static off_t
dparse_str_pos_brushdown(rt_parser_info_t *pi,
                         UNUSED int64_t idx,
                         char delim,
                         off_t spos,
                         off_t epos,
                         bytes_t **val)
{
    STR_POS_BRUSHDOWN_BODY(mrklkit_rt_bytes_new, BYTES_INCREF(*val);)
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


#define QSTR_ST_START   (0)
#define QSTR_ST_QUOTE   (1)
#define QSTR_ST_IN      (2)
#define QSTR_ST_OUT     (3)

#define QSTR_POS_BODY(bytes_new_fn, __a1)                      \
    byterange_t br;                                            \
    int state;                                                 \
    size_t sz;                                                 \
    state = QSTR_ST_START;                                     \
    br.start = spos;                                           \
    br.end = spos;                                             \
    for (br.end = spos; br.end < epos; ++br.end) {             \
        char ch;                                               \
        ch = (char)SNCHR(pi->bs, br.end);                      \
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
    *val = bytes_new_fn(sz + 1);                               \
    /* unescape starting from next to the starting " */        \
    if (sz > 2) {                                              \
        sz = qstr_unescape((char *)(*val)->data,               \
                           (char *)SDATA(pi->bs, br.start + 1),\
                           /* minus "" */                      \
                           br.end - br.start - 2);             \
    } else {                                                   \
        sz = 0;                                                \
    }                                                          \
    (*val)->data[sz] = '\0';                                   \
    (*val)->sz = sz + 1;                                       \
    __a1                                                       \
    return br.end;                                             \



static off_t
dparse_qstr_pos_mpool(rt_parser_info_t *pi,
                off_t spos,
                off_t epos,
                bytes_t **val)
{
    QSTR_POS_BODY(mrklkit_rt_bytes_new_mpool,)
}


static off_t
dparse_qstr_pos(rt_parser_info_t *pi,
                off_t spos,
                off_t epos,
                bytes_t **val)
{
    QSTR_POS_BODY(mrklkit_rt_bytes_new, BYTES_INCREF(*val);)
}


static off_t
dparse_optqstr_pos_mpool(rt_parser_info_t *pi,
                   UNUSED int64_t idx,
                   char delim,
                   off_t spos,
                   off_t epos,
                   bytes_t **val)
{
    if (SNCHR(pi->bs, spos) == '"') {
        return dparse_qstr_pos_mpool(pi, spos, epos, val);
    } else {
        return dparse_str_pos_mpool(pi, idx, delim, spos, epos, val);
    }
    return -1;
}


static off_t
dparse_optqstr_pos(rt_parser_info_t *pi,
                   UNUSED int64_t idx,
                   char delim,
                   off_t spos,
                   off_t epos,
                   bytes_t **val)
{
    if (SNCHR(pi->bs, spos) == '"') {
        return dparse_qstr_pos(pi, spos, epos, val);
    } else {
        return dparse_str_pos(pi, idx, delim, spos, epos, val);
    }
    return -1;
}


/*
 * array
 */
#define DPARSE_ARRAY_CASE(array_get_safe_fn, ty, fn)                   \
    i = 0;                                                             \
    while (spos < epos && SNCHR(pi->bs, spos) != delim) {              \
        off_t fpos;                                                    \
        union {                                                        \
            void *v;                                                   \
            ty x;                                                      \
        } u;                                                           \
        void **v;                                                      \
        fpos = dparser_reach_delim_pos(pi->bs, fdelim, spos, epos);    \
        (void)fn(pi, -1, fdelim, spos, fpos, &u.x);                    \
        v = array_get_safe_fn(&(*val)->fields, i);                     \
        *v = u.v;                                                      \
        spos = dparser_reach_value_pos(pi->bs, fdelim, fpos, epos);    \
        ++i;                                                           \
    }                                                                  \


#define ARRAY_POS_BODY(array_get_safe_fn, str_pos_fn)                  \
    char fdelim;                                                       \
    lkit_dparray_t *dppa;                                              \
    lkit_dpexpr_t *dfpa;                                               \
    lkit_type_t *fty;                                                  \
    unsigned i;                                                        \
    if ((dppa = (lkit_dparray_t *)lkit_dpstruct_get_field_parser(      \
                    pi->dpexpr, idx)) == NULL) {                       \
        FAIL("lkit_dpstruct_get_field_parser");                        \
    }                                                                  \
    assert(lkit_type_cmp(LKIT_PARSER_GET_TYPE(dppa->base.ty),          \
                         (lkit_type_t *)(*val)->type) == 0);           \
    fdelim = pi->dpexpr->fdelim;                                       \
    if ((dfpa = lkit_dparray_get_element_parser(dppa)) == NULL) {      \
        FAIL("lkit_dparray_get_element_parser");                       \
    }                                                                  \
    fty = LKIT_PARSER_GET_TYPE(dfpa->ty);                              \
    switch (fty->tag) {                                                \
    case LKIT_STR:                                                     \
        DPARSE_ARRAY_CASE(array_get_safe_fn, bytes_t *, str_pos_fn);   \
        break;                                                         \
    case LKIT_INT:                                                     \
        DPARSE_ARRAY_CASE(array_get_safe_fn, int64_t, dparse_int_pos); \
        break;                                                         \
    case LKIT_FLOAT:                                                   \
        DPARSE_ARRAY_CASE(array_get_safe_fn, double, dparse_float_pos);\
        break;                                                         \
    default:                                                           \
        /* cannot be recursively nested */                             \
        FAIL("dparse_array_pos");                                      \
    }                                                                  \


static void
dparse_array_pos_mpool(rt_parser_info_t *pi,
                 int64_t idx,
                 char delim,
                 off_t spos,
                 off_t epos,
                 rt_array_t **val)
{
    ARRAY_POS_BODY(_array_get_safe, dparse_str_pos_mpool)
}


static void
dparse_array_pos(rt_parser_info_t *pi,
                 int64_t idx,
                 char delim,
                 off_t spos,
                 off_t epos,
                 rt_array_t **val)
{
    ARRAY_POS_BODY(array_get_safe, dparse_str_pos)
}


rt_array_t *
dparse_array_from_bytes_mpool(UNUSED lkit_dparray_t *pa, UNUSED bytes_t *str)
{
    rt_array_t *val;

    val = NULL;
    FAIL("not implemented");

    return val;
}


rt_array_t *
dparse_array_from_bytes(UNUSED lkit_dparray_t *pa, UNUSED bytes_t *str)
{
    rt_array_t *val;

    val = NULL;
    FAIL("not implemented");

    return val;
}


/*
 * dict
 */

#define DPARSE_DICT_CASE(dict_set_item_fn, ty, fn)                     \
    while (spos < epos && SNCHR(pi->bs, spos) != delim) {              \
        off_t kvpos, fpos;                                             \
        dict_item_t *dit;                                              \
        bytes_t *key = NULL;                                           \
        union {                                                        \
            void *v;                                                   \
            ty x;                                                      \
        } u;                                                           \
        kvpos = dparser_reach_delim_pos(pi->bs, pdelim, spos, epos);   \
        (void)_dparse_str_pos_k(pi, -1, pdelim, spos, kvpos, &key);    \
        spos = dparser_reach_value_pos(pi->bs, pdelim, kvpos, epos);   \
        if ((dit = dict_get_item(&(*val)->fields, key)) == NULL) {     \
            fpos = fn(pi, -1, fdelim, spos, epos, &u.x);               \
            dict_set_item_fn(&(*val)->fields, key, u.v);               \
            fpos = dparser_reach_delim_pos(pi->bs, fdelim, fpos, epos);\
        } else {                                                       \
            BYTES_DECREF(&key);                                        \
            fpos = dparser_reach_delim_pos(pi->bs, fdelim, spos, epos);\
        }                                                              \
        spos = dparser_reach_value_pos(pi->bs, fdelim, fpos, epos);    \
    }                                                                  \



#define DICT_POS_BODY(str_pos_brushdown_fn,                                    \
                      str_pos_fn,                                              \
                      optqstr_pos_fn,                                          \
                      dict_set_item_fn)                                        \
    char pdelim;                                                               \
    char fdelim;                                                               \
    lkit_dpdict_t *dppa;                                                       \
    lkit_dpexpr_t *dfpa;                                                       \
    lkit_type_t *fty;                                                          \
    off_t (*_dparse_str_pos_k)(rt_parser_info_t *,                             \
                               int64_t,                                        \
                               char,                                           \
                               off_t,                                          \
                               off_t,                                          \
                               bytes_t **);                                    \
    if ((dppa = (lkit_dpdict_t *)lkit_dpstruct_get_field_parser(               \
                    pi->dpexpr, idx)) == NULL) {                               \
        FAIL("lkit_dpstruct_get_field_parser");                                \
    }                                                                          \
    assert(lkit_type_cmp(LKIT_PARSER_GET_TYPE(dppa->base.ty),                  \
                         (lkit_type_t *)(*val)->type) == 0);                   \
    pdelim = dppa->pdelim;                                                     \
    fdelim = dppa->fdelim;                                                     \
    if (dppa->base.parser == LKIT_PARSER_SMARTDELIM) {                         \
        _dparse_str_pos_k = str_pos_brushdown_fn;                              \
    } else {                                                                   \
        _dparse_str_pos_k = str_pos_fn;                                        \
    }                                                                          \
    if ((dfpa = lkit_dpdict_get_element_parser(dppa)) == NULL) {               \
        FAIL("lkit_dpdict_get_element_parser");                                \
    }                                                                          \
    fty = LKIT_PARSER_GET_TYPE(dfpa->ty);                                      \
    switch (fty->tag) {                                                        \
    case LKIT_STR:                                                             \
        {                                                                      \
            off_t (*_dparse_str_pos_v)(rt_parser_info_t *,                     \
                                       int64_t,                                \
                                       char,                                   \
                                       off_t,                                  \
                                       off_t,                                  \
                                       bytes_t **);                            \
            if (dfpa->parser == LKIT_PARSER_OPTQSTRDELIM) {                    \
                _dparse_str_pos_v = optqstr_pos_fn;                            \
            } else {                                                           \
                _dparse_str_pos_v = str_pos_fn;                                \
            }                                                                  \
            DPARSE_DICT_CASE(dict_set_item_fn, bytes_t *, _dparse_str_pos_v);  \
        }                                                                      \
        break;                                                                 \
    case LKIT_INT:                                                             \
        DPARSE_DICT_CASE(dict_set_item_fn, int64_t, dparse_int_pos);           \
        break;                                                                 \
    case LKIT_FLOAT:                                                           \
        DPARSE_DICT_CASE(dict_set_item_fn, double, dparse_float_pos);          \
        break;                                                                 \
    default:                                                                   \
        /* cannot be recursively nested */                                     \
        FAIL("dparse_dict_pos");                                               \
    }                                                                          \



static void
dparse_dict_pos_mpool(rt_parser_info_t *pi,
                int64_t idx,
                char delim,
                off_t spos,
                off_t epos,
                rt_dict_t **val)
{
    DICT_POS_BODY(dparse_str_pos_brushdown_mpool,
                  dparse_str_pos_mpool,
                  dparse_optqstr_pos_mpool,
                  _dict_set_item)
}


static void
dparse_dict_pos(rt_parser_info_t *pi,
                int64_t idx,
                char delim,
                off_t spos,
                off_t epos,
                rt_dict_t **val)
{
    DICT_POS_BODY(dparse_str_pos_brushdown,
                  dparse_str_pos,
                  dparse_optqstr_pos,
                  dict_set_item)
}


#define DPARSER_DICT_CASE_ARRAY(ty, fn)                                        \
    while (spos < epos && SNCHR(pi->bs, spos) != delim) {                      \
        off_t kvpos, fpos;                                                     \
        dict_item_t *dit;                                                      \
        bytes_t *key = NULL;                                                   \
        rt_array_t *aval;                                                      \
        ty *x;                                                                 \
        kvpos = dparser_reach_delim_pos(pi->bs, pdelim, spos, epos);           \
        (void)_dparse_str_pos_k(pi, -1, pdelim, spos, kvpos, &key);            \
        spos = dparser_reach_value_pos(pi->bs, pdelim, kvpos, epos);           \
        if ((dit = dict_get_item(&(*val)->fields, key)) == NULL) {             \
            aval = mrklkit_rt_array_new_mpool_sz((lkit_array_t *)fty, 16);     \
            _dict_set_item(&(*val)->fields, key, aval);                        \
        } else {                                                               \
            aval = dit->value;                                                 \
        }                                                                      \
        if ((x = array_incr_mpool(mpool, &aval->fields)) == NULL) {            \
            FAIL("array_incr");                                                \
        }                                                                      \
        fpos = fn(pi, -1, fdelim, spos, epos, x);                              \
        spos = dparser_reach_value_pos(pi->bs, fdelim, fpos, epos);            \
    }                                                                          \


rt_dict_t *
dparse_dict_from_bytes_mpool(lkit_dpdict_t *pa, bytes_t *str)
{
    char delim;
    off_t spos;
    off_t epos;
    rt_parser_info_t _pi, *pi;
    byterange_t br;
    bytestream_t bs;
    rt_dict_t *_val, **val;
    lkit_dict_t *ty;
    char pdelim;
    char fdelim;
    off_t (*_dparse_str_pos_k)(rt_parser_info_t *,
                               int64_t,
                               char,
                               off_t,
                               off_t,
                               bytes_t **);
    lkit_dpexpr_t *fpa;
    lkit_type_t *fty;

    delim = '\0';
    spos = 0;
    epos = str->sz;

    br.start = 0;
    br.end = epos;
    bytestream_from_bytes(&bs, str);
    rt_parser_info_init(&_pi, &bs, &br, NULL, NULL);
    pi = &_pi;

    ty = (lkit_dict_t *)LKIT_PARSER_GET_TYPE(pa->base.ty);

    _val = mrklkit_rt_dict_new_mpool_sz(ty, 17);
    val = &_val;

    pdelim = pa->pdelim;
    fdelim = pa->fdelim;

    if (pa->base.parser == LKIT_PARSER_SMARTDELIM) {
        _dparse_str_pos_k = dparse_str_pos_brushdown_mpool;
    } else {
        _dparse_str_pos_k = dparse_str_pos_mpool;
    }
    if ((fpa = lkit_dpdict_get_element_parser(pa)) == NULL) {
        FAIL("lkit_dpdict_get_element_parser");
    }
    fty = LKIT_PARSER_GET_TYPE(fpa->ty);

    switch (fty->tag) {
    case LKIT_STR:
        {
            off_t (*_dparse_str_pos_v)(rt_parser_info_t *,
                                       int64_t,
                                       char,
                                       off_t,
                                       off_t,
                                       bytes_t **);
            if (pa->base.parser == LKIT_PARSER_OPTQSTRDELIM) {
                _dparse_str_pos_v = dparse_optqstr_pos_mpool;
            } else {
                _dparse_str_pos_v = dparse_str_pos_mpool;
            }
            DPARSE_DICT_CASE(_dict_set_item, bytes_t *, _dparse_str_pos_v);
        }
        break;

    case LKIT_INT:
        DPARSE_DICT_CASE(_dict_set_item, int64_t, dparse_int_pos);
        break;

    case LKIT_FLOAT:
        DPARSE_DICT_CASE(_dict_set_item, double, dparse_float_pos);
        break;

    case LKIT_ARRAY:
        /* multiple values */
        {
            UNUSED lkit_dparray_t *dpa;
            lkit_type_t *afty;

            /* nreserved */
            dpa = (lkit_dparray_t *)fpa;

            afty = lkit_array_get_element_type((lkit_array_t *)fty);

            switch (afty->tag) {
            case LKIT_INT:
                DPARSER_DICT_CASE_ARRAY(int64_t, dparse_int_pos);
                break;

            case LKIT_FLOAT:
                DPARSER_DICT_CASE_ARRAY(double, dparse_float_pos);
                break;

            case LKIT_STR:
                DPARSER_DICT_CASE_ARRAY(bytes_t *, dparse_str_pos_mpool);
                break;

            default:
                /* cannot be recursively nested */
                FAIL("dparse_dict_pos");
            }
        }

        break;

    default:
        /* cannot be recursively nested */
        FAIL("dparse_dict_pos");
    }

    rt_parser_info_fini(&_pi);
    return _val;
}


/*
 * struct
 */
#define DPARSE_STRUCT_CASE(ty, fn)                                     \
    while (spos < epos && SNCHR(pi->bs, spos) != delim) {              \
        off_t fpos;                                                    \
        union {                                                        \
            void *v;                                                   \
            ty x;                                                      \
        } u;                                                           \
        void **v;                                                      \
        fpos = dparser_reach_delim_pos(pi->bs, fdelim, spos, epos);    \
        (void)fn(pi, -1, fdelim, spos, fpos, &u.x);                    \
        v = (*val)->fields + it.iter;                                  \
        *v = u.v;                                                      \
        spos = dparser_reach_value_pos(pi->bs, fdelim, fpos, epos);    \
    }                                                                  \


#define STRUCT_POS_BODY(str_pos_fn)                                    \
    char fdelim;                                                       \
    lkit_dpstruct_t *dppa;                                             \
    lkit_dpexpr_t **dfpa;                                              \
    array_iter_t it;                                                   \
    if ((dppa = (lkit_dpstruct_t *)lkit_dpstruct_get_field_parser(     \
                    pi->dpexpr, idx)) == NULL) {                       \
        FAIL("lkit_dpstruct_get_field_parser");                        \
    }                                                                  \
    assert(lkit_type_cmp(LKIT_PARSER_GET_TYPE(dppa->base.ty),          \
                         (lkit_type_t *)(*val)->type) == 0);           \
    fdelim = pi->dpexpr->fdelim;                                       \
    for (dfpa = array_first(&dppa->fields, &it);                       \
         dfpa != NULL;                                                 \
         dfpa = array_next(&dppa->fields, &it)) {                      \
        lkit_type_t *fty;                                              \
        fty = LKIT_PARSER_GET_TYPE((*dfpa)->ty);                       \
        switch (fty->tag) {                                            \
        case LKIT_STR:                                                 \
            DPARSE_STRUCT_CASE(bytes_t *, str_pos_fn);                 \
            break;                                                     \
                                                                       \
        case LKIT_INT:                                                 \
            DPARSE_STRUCT_CASE(int64_t, dparse_int_pos);               \
            break;                                                     \
                                                                       \
        case LKIT_FLOAT:                                               \
            DPARSE_STRUCT_CASE(double, dparse_float_pos);              \
            break;                                                     \
                                                                       \
        default:                                                       \
            /* cannot be recursively nested */                         \
            FAIL("dparse_struct_pos");                                 \
        }                                                              \
    }                                                                  \


static void
dparse_struct_pos_mpool(rt_parser_info_t *pi,
                  int64_t idx,
                  char delim,
                  off_t spos,
                  off_t epos,
                  rt_struct_t **val)
{
    STRUCT_POS_BODY(dparse_str_pos_mpool)
}



static void
dparse_struct_pos(rt_parser_info_t *pi,
                  int64_t idx,
                  char delim,
                  off_t spos,
                  off_t epos,
                  rt_struct_t **val)
{
    STRUCT_POS_BODY(dparse_str_pos)
}



#define DPARSE_STRUCT_ITEM_RA_BODY(mpsuffix, val, cb, __a1)                    \
    assert(idx < (ssize_t)pi->dpexpr->fields.elnum);                           \
    assert(pi->bs != NULL);                                                    \
    if (!(pi->dpos[idx] & 0x80000000)) {                                       \
        bytestream_t *bs;                                                      \
        off_t spos, epos;                                                      \
        char fdelim;                                                           \
        int next_idx = idx + 1;                                                \
        off_t (*_dparser_reach_value)(bytestream_t *, char, off_t, off_t);     \
        bs = pi->bs;                                                           \
        fdelim = pi->dpexpr->fdelim;                                           \
        if (pi->dpexpr->base.parser == LKIT_PARSER_MDELIM) {                   \
            _dparser_reach_value = dparser_reach_value_m_pos;                  \
        } else {                                                               \
            _dparser_reach_value = dparser_reach_value_pos;                    \
        }                                                                      \
        if (next_idx >= pi->next_delim) {                                      \
            off_t pos;                                                         \
            pos = pi->pos;                                                     \
            do {                                                               \
                pi->dpos[pi->next_delim] = pos;                                \
                pos = _dparser_reach_value(bs,                                 \
                                           fdelim,                             \
                                           pos,                                \
                                           pi->br.end);                        \
                if (pi->next_delim < (ssize_t)pi->dpexpr->fields.elnum) {      \
                    lkit_dpexpr_t **fpa;                                       \
                    lkit_type_t *fty;                                          \
                    fpa = ARRAY_GET(lkit_dpexpr_t *,                           \
                                    &pi->dpexpr->fields,                       \
                                    pi->next_delim);                           \
                    fty = LKIT_PARSER_GET_TYPE((*fpa)->ty);                    \
                    if (fty->tag == LKIT_STR) {                                \
                        val = (__typeof(val))                                  \
                                MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(pi->value,     \
                            pi->next_delim);                                   \
                        if ((*fpa)->parser == LKIT_PARSER_QSTR) {              \
                            /*                                                 \
                             * Special case for quoted string.                 \
                             */                                                \
                            pos = dparse_qstr_pos##mpsuffix(                   \
                                pi, pos, pi->br.end,                           \
                                (bytes_t **)val);                              \
                            pi->dpos[pi->next_delim] |= 0x80000000;            \
                        } else if ((*fpa)->parser == LKIT_PARSER_OPTQSTR) {    \
                            /*                                                 \
                             * Special case for optionally quoted string.      \
                             */                                                \
                            pos = dparse_optqstr_pos##mpsuffix(                \
                                pi, -1, fdelim, pos, pi->br.end,               \
                                (bytes_t **)val);                              \
                            pi->dpos[pi->next_delim] |= 0x80000000;            \
                        } else {                                               \
                            pos = dparser_reach_delim_pos(                     \
                                bs, fdelim, pos, pi->br.end);                  \
                        }                                                      \
                    } else {                                                   \
                        pos = dparser_reach_delim_pos(                         \
                            bs, fdelim, pos, pi->br.end);                      \
                    }                                                          \
                } else {                                                       \
                    pos = dparser_reach_delim_pos(                             \
                        bs, fdelim, pos, pi->br.end);                          \
                }                                                              \
            } while (pi->next_delim++ < next_idx);                             \
            assert(pi->next_delim == (next_idx + 1) ||                         \
                   pi->next_delim == (ssize_t)pi->dpexpr->fields.elnum);       \
            pi->pos = pos;                                                     \
        }                                                                      \
        if (!(pi->dpos[idx] & 0x80000000)) {                                   \
            spos = _dparser_reach_value(bs,                                    \
                                        fdelim,                                \
                                        pi->dpos[idx],                         \
                                        pi->br.end);                           \
            epos = pi->dpos[next_idx] & ~0x80000000;                           \
            pi->dpos[idx] |= 0x80000000;                                       \
            val = (__typeof(val))MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(              \
                    pi->value, idx);                                           \
            __a1;                                                              \
            (void)cb(pi, idx, fdelim, spos, epos, val);                        \
        } else {                                                               \
            val = (__typeof(val))MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(              \
                    pi->value, idx);                                           \
        }                                                                      \
    } else {                                                                   \
        val = (__typeof(val))MRKLKIT_RT_STRUCT_GET_ITEM_ADDR(                  \
                pi->value, idx);                                               \
    }                                                                          \


int64_t
dparse_struct_item_ra_int_mpool(rt_parser_info_t *pi, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool, val, dparse_int_pos,);
    return *val;
}


int64_t
dparse_struct_item_ra_int(rt_parser_info_t *pi, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(, val, dparse_int_pos,);
    return *val;
}


double
dparse_struct_item_ra_float_mpool(rt_parser_info_t *pi, int64_t idx)
{
    union {
        void **v;
        double *d;
    } u;

    assert(sizeof(double) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool, u.v, dparse_float_pos_pvoid,);
    return *u.d;
}


double
dparse_struct_item_ra_float(rt_parser_info_t *pi, int64_t idx)
{
    union {
        void **v;
        double *d;
    } u;

    assert(sizeof(double) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(, u.v, dparse_float_pos_pvoid,);
    return *u.d;
}


int64_t
dparse_struct_item_ra_bool_mpool(rt_parser_info_t *pi, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool, val, dparse_int_pos,);
    return *val;
}


int64_t
dparse_struct_item_ra_bool(rt_parser_info_t *pi, int64_t idx)
{
    int64_t *val;

    assert(sizeof(int64_t) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(, val, dparse_int_pos,);
    return *val;
}


bytes_t *
dparse_struct_item_ra_str_mpool(rt_parser_info_t *pi, int64_t idx)
{
    bytes_t **val;

    assert(sizeof(bytes_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool, val, dparse_str_pos_mpool,);
    return *(bytes_t **)(pi->value->fields + idx);
}


bytes_t *
dparse_struct_item_ra_str(rt_parser_info_t *pi, int64_t idx)
{
    bytes_t **val;

    assert(sizeof(bytes_t *) == sizeof(void *));
    /* zref hack */
    DPARSE_STRUCT_ITEM_RA_BODY(, val, dparse_str_pos_zref,);
    return *(bytes_t **)(pi->value->fields + idx);
}


rt_array_t *
dparse_struct_item_ra_array_mpool(rt_parser_info_t *pi, int64_t idx)
{
    rt_array_t **val;

    assert(sizeof(rt_array_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool,
        val,
        dparse_array_pos_mpool,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_array_new_mpool((lkit_array_t *)*__fty);
    );
    return *(rt_array_t **)(pi->value->fields + idx);
}


rt_array_t *
dparse_struct_item_ra_array(rt_parser_info_t *pi, int64_t idx)
{
    rt_array_t **val;

    assert(sizeof(rt_array_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(,
        val,
        dparse_array_pos,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_array_new((lkit_array_t *)*__fty);
            /* ARRAY_INCREF(*val); zref hack */
    );
    return *(rt_array_t **)(pi->value->fields + idx);
}


rt_dict_t *
dparse_struct_item_ra_dict_mpool(rt_parser_info_t *pi, int64_t idx)
{
    rt_dict_t **val;

    assert(sizeof(rt_dict_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool,
        val,
        dparse_dict_pos_mpool,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_dict_new_mpool((lkit_dict_t *)*__fty);
    );
    return *(rt_dict_t **)(pi->value->fields + idx);
}


rt_dict_t *
dparse_struct_item_ra_dict(rt_parser_info_t *pi, int64_t idx)
{
    rt_dict_t **val;

    assert(sizeof(rt_dict_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(,
        val,
        dparse_dict_pos,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_dict_new((lkit_dict_t *)*__fty);
            /* DICT_INCREF(*val); zref hack */
    );
    return *(rt_dict_t **)(pi->value->fields + idx);
}


rt_struct_t *
dparse_struct_item_ra_struct_mpool(rt_parser_info_t *pi, int64_t idx)
{
    rt_struct_t **val;

    assert(sizeof(rt_struct_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(_mpool,
        val,
        dparse_struct_pos_mpool,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_struct_new_mpool((lkit_struct_t *)*__fty);
        );
    return *(rt_struct_t **)(pi->value->fields + idx);
}


rt_struct_t *
dparse_struct_item_ra_struct(rt_parser_info_t *pi, int64_t idx)
{
    rt_struct_t **val;

    assert(sizeof(rt_struct_t *) == sizeof(void *));
    DPARSE_STRUCT_ITEM_RA_BODY(,
        val,
        dparse_struct_pos,
            lkit_type_t **__fty;
            __fty = ARRAY_GET(lkit_type_t *, &pi->value->type->fields, idx);
            *val = mrklkit_rt_struct_new((lkit_struct_t *)*__fty);
            /* STRUCT_INCREF(*val); zref hack */
        );
    return *(rt_struct_t **)(pi->value->fields + idx);
}


/*
 * rt_parser_info_t
 */
void
rt_parser_info_init(rt_parser_info_t *pi,
                    bytestream_t *bs,
                    const byterange_t *br,
                    lkit_dpstruct_t *dpexpr,
                    rt_struct_t *value)
{
    pi->bs = bs;
    pi->br = *br;
    pi->dpexpr = dpexpr;
    pi->value = value;
    pi->pos = br->start;
    pi->next_delim = 0;
    pi->current = 0;
    if (value != NULL) {
        if ((pi->dpos = malloc(
                    (value->type->fields.elnum + 1) * sizeof(off_t))) == NULL) {
            FAIL("malloc");
        }
        (void)memset(pi->dpos,
                     '\0',
                     (value->type->fields.elnum + 1) * sizeof(off_t));
    } else {
        pi->dpos = NULL;
    }
}


void
rt_parser_info_fini(rt_parser_info_t *pi)
{
    pi->bs = NULL;
    pi->dpexpr = NULL;
    pi->value = NULL;
    if (pi->dpos != NULL) {
        free(pi->dpos);
        pi->dpos = NULL;
    }
}


#define INFO_DATA_BODY(bytes_new_fn)   \
    bytes_t *res;                      \
    size_t sz;                         \
    sz = pi->br.end - pi->br.start;    \
    res = bytes_new_fn(sz + 1);        \
    memcpy(res->data,                  \
           SDATA(pi->bs, pi->br.start),\
           sz);                        \
    res->data[sz] = '\0';              \
    return res;                        \


bytes_t *
rt_parser_info_data_mpool(rt_parser_info_t *pi)
{
    INFO_DATA_BODY(mrklkit_rt_bytes_new_mpool)
}


bytes_t *
rt_parser_info_data(rt_parser_info_t *pi)
{
    INFO_DATA_BODY(mrklkit_rt_bytes_new)
}


off_t
rt_parser_info_pos(rt_parser_info_t *pi)
{
    return pi->pos;
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
            FAIL("bytes_new");                                                 \
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
