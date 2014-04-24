#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include <mrkcommon/dumpm.h>

#include "unittest.h"
#include "diag.h"

#include <mrklkit/mrklkit.h>
#include <mrklkit/modules/dsource.h>
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

    if ((fd = open(prog, O_RDONLY)) == -1) {
        FAIL("open");
    }

    if ((res = mrklkit_compile(fd)) != 0) {
        perror("mrklkit_compile");
    } else {
        dsource_t *ds;

        if ((ds = dsource_get("qwe")) == NULL) {
            FAIL("dsource_get");
        }
    }

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
