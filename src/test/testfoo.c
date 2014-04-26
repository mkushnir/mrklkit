#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>

#include "unittest.h"
#include "diag.h"
#include <mrkcommon/array.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/module.h>
#include <mrklkit/modules/testrt.h>

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

LLVMModuleRef module;
LLVMPassManagerRef pm;

const char *fname;

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

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    if ((res = mrklkit_compile(fd)) != 0) {
        perror("mrklkit_compile");
    } else {
        TRACE(FGREEN("running qwe"));
        if ((res = mrklkit_call_void(".mrklkit.init.qwe")) != 0) {
            perror("mrklkit_call_void");
        }
        TRACE(FGREEN("running asd"));
        if ((res = mrklkit_call_void(".mrklkit.init.asd")) != 0) {
            perror("mrklkit_call_void");
        }
    }


    close(fd);
}

int
main(int argc, char **argv)
{
    array_t modules;
    mrklkit_module_t **m;

    array_init(&modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    m = array_incr(&modules);
    *m = &testrt_module;

    if (argc > 1) {
        fname = argv[1];
    }
    mrklkit_init(&modules);
    test1();
    mrklkit_fini();
    array_fini(&modules);
    return 0;
}
