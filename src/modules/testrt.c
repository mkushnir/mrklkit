#include <stdio.h>
//#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dumpm.h>

#include <mrklkit/module.h>
#include <mrklkit/lparse.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/builtin.h>
#include <mrklkit/util.h>
#include <mrklkit/modules/testrt.h>
#include <mrklkit/modules/dparser.h>
#include <mrklkit/modules/dsource.h>

#include "builtin_private.h"
#include "diag.h"

static lkit_expr_t root;

rt_struct_t *testrt_source = NULL;


int
testrt_run(bytestream_t *bs, dsource_t *ds)
{
    int res = 0;
    char enddelim = '\0';

    testrt_source = mrklkit_rt_struct_new(ds->_struct);

    if ((res = dparse_struct(bs,
                             ds->fdelim,
                             ds->rdelim,
                             ds->_struct,
                             testrt_source,
                             &enddelim,
                             ds->parse_flags)) == DPARSE_NEEDMORE) {
        goto end;

    } else if (res == DPARSE_ERRORVALUE) {
        goto end;

    } else {
        if (enddelim != '\n') {
            /* truncated input? */
            res = 1;
            goto end;
        }
    }

end:
    mrklkit_rt_struct_dtor(&testrt_source);
    return res;
}


static int
parse_quals(array_t *form,
                 array_iter_t *it,
                 unsigned char *qual,
                 testrt_t *trt)
{
    char *s = (char *)qual;

    if (strcmp(s, ":dsource") == 0) {
        bytes_t *value;
        if (lparse_next_word_bytes(form, it, &value, 1) != 0) {
            return 1;
        }
        trt->dsource = value;
    } else if (strcmp(s, ":id") == 0) {
        int64_t val;
        if (lparse_next_int(form, it, &val, 1) != 0) {
            return 1;
        }
        trt->id = bytes_new(64);
        snprintf((char *)trt->id->data, trt->id->sz, "testrt.%016lx", val);
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}


static testrt_t *
testrt_new(void)
{
    testrt_t *res;

    if ((res = malloc(sizeof(testrt_t))) == NULL) {
        FAIL("malloc");
    }

    res->expr = &root;
    res->dsource = NULL;
    res->id = NULL;

    return res;
}


UNUSED static void
testrt_destroy(testrt_t **trt)
{
    if (*trt != NULL) {
        bytes_destroy(&(*trt)->id);
        free(*trt);
        *trt = NULL;
    }
}


static int
_parse(array_t *form, array_iter_t *it)
{
    fparser_datum_t **node = NULL;
    lkit_gitem_t **gitem;
    testrt_t *trt;

    if ((trt = testrt_new()) == NULL) {
        TRRET(TESTRT_PARSE + 1);
    }

    /* (testrt quals value) */

    /* quals */
    if (lparse_quals(form,
                 it,
                 (quals_parser_t)parse_quals,
                 trt) != 0) {
        TRRET(TESTRT_PARSE + 2);
    }

    if (trt->id == NULL) {
        TRRET(TESTRT_PARSE + 3);
    }

    /* value */
    if ((node = array_next(form, it)) == NULL) {
        TRRET(TESTRT_PARSE + 4);
    }

    if ((trt->expr = lkit_expr_parse(&root, *node, 1)) == NULL) {
        (*node)->error = 1;
        TRRET(TESTRT_PARSE + 5);
    }

    if ((gitem = array_incr(&root.glist)) == NULL) {
        FAIL("array_incr");
    }
    (*gitem)->name = trt->id;
    (*gitem)->expr = trt->expr;

    return 0;
}


UNUSED static int
_acb(lkit_gitem_t **gitem, void *udata)
{
    int (*cb)(lkit_expr_t *) = udata;
    return cb((*gitem)->expr);
}


UNUSED static int
__compile(lkit_gitem_t **gitem, UNUSED void *udata)
{
    UNUSED bytes_t *name = (*gitem)->name;
    UNUSED lkit_expr_t *expr = (*gitem)->expr;
    UNUSED LLVMModuleRef module = (LLVMModuleRef)udata;

    //TRACE("compiling %s", name->data);
    //lkit_expr_dump(expr);
    return builtingen_sym_compile(gitem, udata);
    return 0;
}


static int
_compile(UNUSED LLVMModuleRef module)
{
    //array_traverse(&root.glist, (array_traverser_t)_acb, lkit_expr_dump);

    if (array_traverse(&root.glist,
                       (array_traverser_t)__compile,
                       module) != 0) {
        return 1;
    }

    //if (array_traverse(&root.glist,
    //                   (array_traverser_t)builtingen_sym_compile,
    //                   module) != 0) {
    //    return 1;
    //}
    return 0;
}


static void
_init(void)
{
    lkit_expr_init(&root, builtin_get_root_ctx());
    dsource_init_module();
}


static void
_fini(void)
{
    dsource_fini_module();
    lkit_expr_fini(&root);
}

static mrklkit_parser_info_t _parsers[] = {
    {"testrt", _parse},
    {"dsource", dsource_parse},
    {NULL, NULL},
};


mrklkit_module_t testrt_module = {
    _init,
    _fini,
    _parsers,
    _compile,
};
