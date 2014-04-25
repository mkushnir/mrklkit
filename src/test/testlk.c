#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include <mrkcommon/dumpm.h>

#include "unittest.h"
#include "diag.h"

#include <mrklkit/mrklkit.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/modules/dsource.h>
#include <mrklkit/modules/dparser.h>
#include <mrklkit/modules/testrt.h>

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

LLVMModuleRef module;
LLVMPassManagerRef pm;

const char *prog, *input;

UNUSED static void
test0(void)
{
    struct {
        long rnd;
        int in;
        int expected;
    } data[] = {
        {0, 0, 0},
    };
    UNITTEST_PROLOG_RAND;

    FOREACHDATA {
        TRACE("in=%d expected=%d", CDATA.in, CDATA.expected);
        assert(CDATA.in == CDATA.expected);
    }
}

UNUSED static void
test1(void)
{
    int fd;
    int res;
    dsource_t *ds;
    bytestream_t bs;
    ssize_t nread;

    if ((fd = open(prog, O_RDONLY)) == -1) {
        FAIL("open");
    }

    if ((res = mrklkit_compile(fd)) != 0) {
        FAIL("mrklkit_compile");
    }

    close(fd);

    if ((ds = dsource_get("qwe")) == NULL) {
        FAIL("dsource_get");
    }

    ds->rdelim[0] = '\n';
    ds->rdelim[1] = '\0';
    ds->fdelim = ds->_struct->delim[0];

    if ((fd = open(input, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        rt_struct_t value;
        char delim = 0;

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        mrklkit_rt_struct_init(&value, ds->_struct);

        if ((res = dparse_struct(&bs,
                                 ds->fdelim,
                                 ds->rdelim,
                                 ds->_struct,
                                 &value,
                                 &delim,
                                 ds->parse_flags)) == DPARSE_NEEDMORE) {
            mrklkit_rt_struct_fini(&value);
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            array_traverse(&value.fields, (array_traverser_t)tobj_dump, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value.fields, (array_traverser_t)tobj_dump, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        mrklkit_rt_struct_fini(&value);
    }
    bytestream_fini(&bs);
    close(fd);


}

static void
run(void)
{
    array_t modules;
    mrklkit_module_t **m;

    array_init(&modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    m = array_incr(&modules);
    *m = &testrt_module;

    mrklkit_init(&modules);

    test1();

    mrklkit_fini();
    array_fini(&modules);
}

int
main(int argc, char **argv)
{
    if (argc > 2) {
        prog = argv[1];
        input = argv[2];
        run();
    } else {
        FAIL("main");
    }
    return 0;
}
