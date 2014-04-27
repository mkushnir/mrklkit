#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>

#include <mrklkit/modules/dsource.h>

#include "diag.h"

/**
 * data source example
 *
 */

static array_t dsources;

static void
dsource_init(dsource_t *dsource)
{
    dsource->timestamp_index = -1;
    dsource->date_index = -1;
    dsource->time_index = -1;
    dsource->duration_index = -1;
    dsource->error = 0;
    /* weak ref */
    dsource->kind = NULL;
    dsource->_struct = NULL;
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
    dsource->kind = NULL;
    /* weak ref */
    dsource->_struct = NULL;
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

static void
dsource_destroy(dsource_t **dsource)
{
    if (*dsource != NULL) {
        dsource_fini(*dsource);
        free(*dsource);
        *dsource = NULL;
    }
}


/**
 * dsource ::= (dsource quals? kind _struct?)
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
dsource_parse(array_t *form, array_iter_t *it)
{
    dsource_t **dsource;


    if ((dsource = array_incr(&dsources)) == NULL) {
        FAIL("array_incr");
    }
    *dsource = dsource_new();

    /* kind */
    if (lparse_next_word_bytes(form, it, &(*dsource)->kind, 1) != 0) {
        (*dsource)->error = 1;
        return 1;
    }
    /* quals */
    lparse_quals(form, it, (quals_parser_t)parse_dsource_quals, *dsource);

    if (ltype_next_struct(form, it, &(*dsource)->_struct, 1) != 0) {
        (*dsource)->error = 1;
        return 1;
    }

    return 0;
}


dsource_t *
dsource_get(const char *name)
{
    dsource_t **v;
    array_iter_t it;

    for (v = array_first(&dsources, &it);
         v != NULL;
         v = array_next(&dsources, &it)) {

        if (strcmp(name, (const char *)(*v)->kind->data) == 0) {
            return *v;
        }
    }

    return NULL;
}


int
dsource_compile(LLVMModuleRef module)
{
    array_iter_t it;
    dsource_t **ds;
    for (ds = array_first(&dsources, &it);
         ds != NULL;
         ds = array_next(&dsources, &it)) {
        if (ltype_compile_methods((lkit_type_t *)(*ds)->_struct,
                                  module,
                                  (*ds)->kind)) {
            TRRET(DSOURCE + 100);
        }
    }
    return 0;
}


void
dsource_init_module(void)
{
    if (array_init(&dsources, sizeof(dsource_t *), 0,
                   NULL,
                   (array_finalizer_t)dsource_destroy) != 0) {
        FAIL("array_init");
    }
}

void
dsource_fini_module(void)
{
    array_fini(&dsources);
}

