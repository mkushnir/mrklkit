#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/array.h>
#include <mrkcommon/util.h>
#include <mrklkit/fparser.h>

#include "dsource.h"
#include "lparse.h"
#include "ltype.h"

#include "diag.h"

/**
 * data source example
 *
 */

static void
dsource_init(dsource_t *dsource)
{
    dsource->timestamp_index = -1;
    dsource->date_index = -1;
    dsource->time_index = -1;
    dsource->duration_index = -1;
    dsource->error = 0;
    /* weak ref */
    dsource->logtype = NULL;
    dsource->fields = NULL;
}

static void
dsource_fini(dsource_t *dsource)
{
    dsource->timestamp_index = -1;
    dsource->date_index = -1;
    dsource->time_index = -1;
    dsource->duration_index = -1;
    dsource->error = 0;
    /* weak ref */
    dsource->logtype = NULL;
    /* weak ref */
    dsource->fields = NULL;
}

static dsource_t *
dsource_new(void)
{
    dsource_t *dsource;

    if ((dsource = malloc(sizeof(dsource_t))) == NULL) {
        FAIL("malloc");
    }
    dsource_init(dsource);
    return dsource;
}

UNUSED static void
dsource_destroy(dsource_t **dsource)
{
    if (*dsource != NULL) {
        dsource_fini(*dsource);
        free(*dsource);
        *dsource = NULL;
    }
}


UNUSED static void
dsource_dump(dsource_t *dsource)
{
    if (dsource->error) {
        TRACEC("-->(dsource ");
    } else {
        TRACEC("(dsource ");
    }
    if (dsource->timestamp_index != -1) {
        TRACEC(":timestamp-index %d ", dsource->timestamp_index);
    }
    if (dsource->date_index != -1) {
        TRACEC(":date-index %d ", dsource->date_index);
    }
    if (dsource->time_index != -1) {
        TRACEC(":time-index %d ", dsource->time_index);
    }
    lkit_type_dump((lkit_type_t *)(dsource->fields));
    if (dsource->error) {
        TRACEC(")<--");
    } else {
        TRACEC(")");
    }
}


/**
 * dsource ::= (dsource quals? logtype fields?)
 *
 */
static int
parse_dsource_quals(array_t *form,
                   array_iter_t *it,
                   unsigned char *qual,
                   dsource_t *dsource)
{
    char *s = (char *)qual;

#define DSOURCE_SET_INT(m) \
    int64_t value; \
    if (lparse_next_int(form, it, &value, 1) != 0) { \
        dsource->error = 1; \
        return 1; \
    } \
    dsource->m = (int)value;

    if (strcmp(s, ":tsidx") == 0) {
        DSOURCE_SET_INT(timestamp_index);
    } else if (strcmp(s, ":dtidx") == 0) {
        DSOURCE_SET_INT(date_index);
    } else if (strcmp(s, ":tmidx") == 0) {
        DSOURCE_SET_INT(time_index);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

int
lkit_parse_dsource(array_t *form,
             array_iter_t *it,
             dsource_t **dsource)
{
    *dsource = dsource_new();

    /* logtype */
    if (lparse_next_word(form, it, &(*dsource)->logtype, 1) != 0) {
        (*dsource)->error = 1;
        return 1;
    }
    /* quals */
    lparse_quals(form, it, (quals_parser_t)parse_dsource_quals, *dsource);

    if (ltype_next_struct(form, it, &(*dsource)->fields, 1) != 0) {
        (*dsource)->error = 1;
        return 1;
    }
    return 0;
}

