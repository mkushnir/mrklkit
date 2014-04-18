#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>

#include <mrklkit/ltype.h>
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

int
dparse_int(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           uint64_t *value,
           char *ch,
           unsigned int flags)
{
    off_t spos = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (DPARSE_TEST_DELIM(*ch)) {
            off_t epos;
            char *endptr = NULL;

            epos = SPOS(bs);
            if (epos == spos) {
                /* empty field */
                *value = 0;
                SINCR(bs);
                if (flags & DPARSE_RESETONERROR) {
                    SPOS(bs) = spos;
                } else {
                    if (flags & DPARSE_MERGEDELIM) {
                        reach_value(bs, fdelim);
                    }
                }
                return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;

            } else {
                *value = (int64_t)strtoll(SDATA(bs, spos), &endptr, 10);
            }

            if ((*value == 0 && errno == EINVAL) || errno == ERANGE) {
                goto err;

            } else {
                /* at the first char of the next field */
                if (flags & DPARSE_MERGEDELIM) {
                    reach_value(bs, fdelim);
                } else {
                    SINCR(bs);
                }
            }

            return 0;
        }

        SINCR(bs);
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    } else {
        reach_delim(bs, fdelim, rdelim);
        if (flags & DPARSE_MERGEDELIM) {
            reach_value(bs, fdelim);
        } else {
            SINCR(bs);
        }
    }
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}


int
dparse_float(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           double *value,
           char *ch,
           unsigned int flags)
{
    off_t spos = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (DPARSE_TEST_DELIM(*ch)) {
            off_t epos;
            char *endptr = NULL;

            epos = SPOS(bs);
            if (epos == spos) {
                /* empty field */
                *value = 0.0;
                SINCR(bs);
                if (flags & DPARSE_RESETONERROR) {
                    SPOS(bs) = spos;
                } else {
                    if (flags & DPARSE_MERGEDELIM) {
                        reach_value(bs, fdelim);
                    }
                }
                return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;

            } else {
                *value = (double)strtod(SDATA(bs, spos), &endptr);
            }

            if ((*value == 0.0 && errno == EINVAL) || errno == ERANGE) {
                goto err;

            } else {
                /* at the first char of the next field */
                if (flags & DPARSE_MERGEDELIM) {
                    reach_value(bs, fdelim);
                } else {
                    SINCR(bs);
                }
            }

            return 0;
        }

        SINCR(bs);
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    } else {
        reach_delim(bs, fdelim, rdelim);
        if (flags & DPARSE_MERGEDELIM) {
            reach_value(bs, fdelim);
        } else {
            SINCR(bs);
        }
    }
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}


int
dparse_qstr(bytestream_t *bs,
            char fdelim,
            char rdelim[2],
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

        if (vpos >= sz) {
            goto err;
        }

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
            } else {
                *(value + vpos++) = *ch;
            }
            break;

        case QSTR_ST_QUOTE:
            if (*ch == '"') {
                state = QSTR_ST_IN;
                *(value + vpos++) = *ch;
            } else {
                /* one beyond the closing " */
                state = QSTR_ST_OUT;
                *(value + vpos++) = '\0';

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

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
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
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}

int
dparse_str(bytestream_t *bs,
           char fdelim,
           char rdelim[2],
           char *value,
           size_t sz,
           char *ch,
           unsigned int flags)
{
    off_t spos = SPOS(bs);
    size_t vpos = 0;

    while (!SNEEDMORE(bs)) {
        if (vpos >= sz) {
            goto err;
        }

        *ch = SPCHR(bs);
        if (DPARSE_TEST_DELIM(*ch)) {
            if (flags & DPARSE_MERGEDELIM) {
                reach_value(bs, fdelim);
            } else {
                SINCR(bs);
            }
            *(value + vpos) = '\0';
            return 0;
        } else {
            *(value + vpos++) = SPCHR(bs);
            SINCR(bs);
        }
    }

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    } else {
        reach_delim(bs, fdelim, rdelim);
        if (flags & DPARSE_MERGEDELIM) {
            reach_value(bs, fdelim);
        } else {
            SINCR(bs);
        }
    }
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}


int
dparse_str_byterange(bytestream_t *bs,
                     char fdelim,
                     char rdelim[2],
                     byterange_t *value,
                     char *ch,
                     unsigned int flags)
{
    off_t spos = SPOS(bs);

    value->start = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);
        if (DPARSE_TEST_DELIM(*ch)) {
            value->end = SPOS(bs);
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

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;
}


int
dparse_qstr_byterange(bytestream_t *bs,
                      char fdelim,
                      char rdelim[2],
                      byterange_t *value,
                      char *ch,
                      unsigned int flags)
{
    off_t spos = SPOS(bs);
#   define QSTR_ST_START   (0)
#   define QSTR_ST_QUOTE   (1)
#   define QSTR_ST_IN      (2)
#   define QSTR_ST_OUT     (3)
    int state = QSTR_ST_START;

    value->start = SPOS(bs);

    while (!SNEEDMORE(bs)) {
        *ch = SPCHR(bs);

        switch (state) {
        case QSTR_ST_START:
            if (*ch == '"') {
                state = QSTR_ST_IN;
            } else {
                /* garbage before the opening " */
                value->end = SPOS(bs);
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
                value->end = SPOS(bs);

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

    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
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
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}


int
dparse_array(bytestream_t *bs,
             char fdelim,
             char rdelim[2],
             lkit_array_t *arty,
             array_t *value,
             char *ch,
             unsigned int flags)
{
    off_t spos = SPOS(bs);
    char afdelim;
    char ardelim[2] = {fdelim, rdelim[0]};
    lkit_type_t **afty;

    afdelim = arty->delim[0];
    if ((afty = array_get(&arty->fields, 0)) == NULL) {
        FAIL("array_get");
    }

    if ((*afty)->tag == LKIT_INT) {
        while (!SNEEDMORE(bs)) {
            int res;
            uint64_t *val;

            //D8(SPDATA(bs), SEOD(bs) - SPOS(bs));

            if ((val = array_incr(value)) == NULL) {
                FAIL("array_incr");
            }

            if ((res = dparse_int(bs,
                                  afdelim,
                                  ardelim,
                                  val,
                                  ch,
                                  flags | DPARSE_NEXTONERROR)) ==
                    DPARSE_NEEDMORE) {
                goto needmore;

            } else if (res == DPARSE_ERRORVALUE) {
                goto err;
            }
            //TRACE("val=%ld", *val);

            if (*ch == fdelim || *ch == rdelim[0] || *ch == rdelim[1]) {
                break;
            }
        }

    } else if ((*afty)->tag == LKIT_FLOAT) {
        while (!SNEEDMORE(bs)) {
            int res;
            double *val;

            //D8(SPDATA(bs), SEOD(bs) - SPOS(bs));

            if ((val = array_incr(value)) == NULL) {
                FAIL("array_incr");
            }

            if ((res = dparse_float(bs,
                                  afdelim,
                                  ardelim,
                                  val,
                                  ch,
                                  flags | DPARSE_NEXTONERROR)) ==
                    DPARSE_NEEDMORE) {
                goto needmore;

            } else if (res == DPARSE_ERRORVALUE) {
                goto err;
            }
            //TRACE("val=%ld", *val);

            if (*ch == fdelim || *ch == rdelim[0] || *ch == rdelim[1]) {
                break;
            }
        }

    } else if ((*afty)->tag == LKIT_STR) {
        while (!SNEEDMORE(bs)) {
            int res;
            byterange_t *val;

            //D8(SPDATA(bs), SEOD(bs) - SPOS(bs));

            if ((val = array_incr(value)) == NULL) {
                FAIL("array_incr");
            }

            if ((res = dparse_str_byterange(bs,
                                            afdelim,
                                            ardelim,
                                            val,
                                            ch,
                                            flags | DPARSE_NEXTONERROR)) ==
                    DPARSE_NEEDMORE) {
                goto needmore;

            } else if (res == DPARSE_ERRORVALUE) {
                goto err;
            }
            //TRACE("val=%ld", *val);

            if (*ch == fdelim || *ch == rdelim[0] || *ch == rdelim[1]) {
                break;
            }
        }

    } else if ((*afty)->tag == LKIT_QSTR) {
        while (!SNEEDMORE(bs)) {
            int res;
            byterange_t *val;

            //D8(SPDATA(bs), SEOD(bs) - SPOS(bs));

            if ((val = array_incr(value)) == NULL) {
                FAIL("array_incr");
            }

            if ((res = dparse_qstr_byterange(bs,
                                            afdelim,
                                            ardelim,
                                            val,
                                            ch,
                                            flags | DPARSE_NEXTONERROR)) ==
                    DPARSE_NEEDMORE) {
                goto needmore;

            } else if (res == DPARSE_ERRORVALUE) {
                goto err;
            }
            //TRACE("val=%ld", *val);

            if (*ch == fdelim || *ch == rdelim[0] || *ch == rdelim[1]) {
                break;
            }
        }

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
    SPOS(bs) = spos;
    return DPARSE_NEEDMORE;

err:
    if (flags & DPARSE_RESETONERROR) {
        SPOS(bs) = spos;
    } else {
        reach_delim(bs, fdelim, rdelim);
        if (flags & DPARSE_MERGEDELIM) {
            reach_value(bs, fdelim);
        } else {
            SINCR(bs);
        }
    }
    return flags & DPARSE_NEXTONERROR ? 0 : DPARSE_ERRORVALUE;
}
