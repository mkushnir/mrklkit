#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#include <mrkcommon/bytestream.h>
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>

#include <mrklkit/modules/dparser.h>

#include "diag.h"

static void
swallow_chars(bytestream_t *bs, char ch)
{
    do {
        SINCR(bs);
        if (SPCHR(bs) != ch) {
            SDECR(bs);
            break;
        }
    } while (!SNEEDMORE(bs));
}

int
dparse_int(bytestream_t *bs,
           char fdelim,
           char rdelim,
           uint64_t *value,
           char *ch,
           unsigned int flags)
{
    off_t spos = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (*ch == fdelim || *ch == rdelim || *ch == '\0') {
            off_t epos;
            char *endptr = NULL;
            intptr_t delimited, parsed;

            epos = SPOS(bs);
            delimited = (intptr_t)(SDATA(bs, epos) - SDATA(bs, spos));

            *value = (int64_t)strtoll(SDATA(bs, spos), &endptr, 10);
            parsed = (intptr_t)(endptr - SDATA(bs, spos));

            //TRACE("p=%ld d=%ld", parsed, delimited);

            if (flags & DPARSE_MERGEDELIM) {
                swallow_chars(bs, fdelim);
                epos = SPOS(bs);
                *ch = SPCHR(bs);
                delimited = (intptr_t)(SDATA(bs, epos) - SDATA(bs, spos));
                //TRACE(" d=%ld", delimited);
            }
            if (parsed > delimited) {
                *value = 0;
                errno = EINVAL;
            }

            if (*value == 0 && (errno == EINVAL || errno == ERANGE)) {
                if (flags & DPARSE_RESETONERROR) {
                    SPOS(bs) = spos;
                } else {
                    SINCR(bs);
                }
                return DPARSE_ERRORVALUE;
            } else {
                SINCR(bs);
            }

            return 0;
        }

        SINCR(bs);
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;
}


int
dparse_float(bytestream_t *bs,
           char fdelim,
           char rdelim,
           double *value,
           char *ch,
           unsigned int flags)
{
    off_t spos = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (*ch == fdelim || *ch == rdelim || *ch == '\0') {
            off_t epos;
            char *endptr = NULL;
            intptr_t delimited, parsed;

            epos = SPOS(bs);
            delimited = (intptr_t)(SDATA(bs, epos) - SDATA(bs, spos));

            *value = (double)strtod(SDATA(bs, spos), &endptr);
            parsed = (intptr_t)(endptr - SDATA(bs, spos));

            //TRACE("p=%ld d=%ld", parsed, delimited);

            if (flags & DPARSE_MERGEDELIM) {
                swallow_chars(bs, fdelim);
                epos = SPOS(bs);
                *ch = SPCHR(bs);
                delimited = (intptr_t)(SDATA(bs, epos) - SDATA(bs, spos));
                //TRACE(" d=%ld", delimited);
            }
            if (parsed > delimited) {
                *value = 0.0;
                errno = EINVAL;
            }

            if (*value == 0.0 && (errno == EINVAL || errno == ERANGE)) {
                if (flags & DPARSE_RESETONERROR) {
                    SPOS(bs) = spos;
                } else {
                    SINCR(bs);
                }
                return DPARSE_ERRORVALUE;
            } else {
                SINCR(bs);
            }

            return 0;
        }

        SINCR(bs);
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;
}


int
dparse_qstr(bytestream_t *bs,
            char fdelim,
            char rdelim,
            char *value,
            size_t sz,
            char *ch,
            unsigned int flags)
{
    off_t spos = SPOS(bs);
    size_t vpos = 0;
#   define QSTR_ST_START   (0)
#   define QSTR_ST_QUOTE   (1)
#   define QSTR_ST_IN      (2)
#   define QSTR_ST_OUT     (3)
    int state = QSTR_ST_START;

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);

        //TRACE("ch='%c' st=%s", *ch,
        //      (state == QSTR_ST_START) ? "START" :
        //      (state == QSTR_ST_QUOTE) ? "QUOTE" :
        //      (state == QSTR_ST_IN) ? "IN" :
        //      (state == QSTR_ST_OUT) ? "OUT" :
        //      "...");

        if (vpos >= sz) {
            goto err;
        }

        switch (state) {
        case QSTR_ST_START:
            if (*ch == '"') {
                state = QSTR_ST_IN;
            } else {
                goto err;
            }
            break;

        case QSTR_ST_IN:
            if (*ch == '"') {
                state = QSTR_ST_QUOTE;
            } else {
                *(value + vpos++) = *ch;
            }
            break;

        case QSTR_ST_QUOTE:
            if (*ch == '"') {
                state = QSTR_ST_IN;
                *(value + vpos++) = *ch;
            } else {
                state = QSTR_ST_OUT;
                continue;
            }
            break;

        case QSTR_ST_OUT:
            *(value + vpos++) = '\0';
            if (*ch == fdelim || *ch == rdelim || *ch == '\0') {
                if (flags & DPARSE_MERGEDELIM) {
                    swallow_chars(bs, fdelim);
                }
                SINCR(bs);
                return 0;
            } else {
                goto err;
            }

        default:
            assert(0);
        }

        SINCR(bs);
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    } else {
        if (flags & DPARSE_MERGEDELIM) {
            swallow_chars(bs, fdelim);
        }
        SINCR(bs);
    }
    return DPARSE_ERRORVALUE;
}

