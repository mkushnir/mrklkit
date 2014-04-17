#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

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
        if (*ch == fdelim || *ch == rdelim) {
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
        if (*ch == fdelim || *ch == rdelim) {
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


