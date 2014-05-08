#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

static const char *fname;
static uint64_t flags;

static mrklkit_ctx_t ctx;

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

    if ((res = mrklkit_compile(&ctx, fd, flags, NULL)) != 0) {
        perror("mrklkit_compile");
    } else {
        TRACE(FGREEN("running qwe"));
        if ((res = mrklkit_call_void(&ctx, ".mrklkit.init.qwe")) != 0) {
            perror("mrklkit_call_void");
        }
        TRACE(FGREEN("running asd"));
        if ((res = mrklkit_call_void(&ctx, ".mrklkit.init.asd")) != 0) {
            perror("mrklkit_call_void");
        }
    }


    close(fd);
}


static void
usage(const char *progname)
{
    printf("Usage: %s [ -f d0 | -f d1 | -f d2 ]\n", progname);
}


int
main(int argc, char **argv)
{
    int ch;
    array_t modules;
    mrklkit_module_t **m;

    while ((ch = getopt(argc, argv, "f:")) != -1) {
        switch (ch) {
        case 'f':
            if (strcmp(optarg, "d0") == 0) {
                flags |= MRKLKIT_COMPILE_DUMP0;
            } else if (strcmp(optarg, "d1") == 0) {
                flags |= MRKLKIT_COMPILE_DUMP1;
            } else if (strcmp(optarg, "d2") == 0) {
                flags |= MRKLKIT_COMPILE_DUMP2;
            } else {
                usage(argv[0]);
                exit(1);
            }
            break;
        }
    }
    argc -= (optind - 1);
    argv += (optind - 1);

    array_init(&modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    m = array_incr(&modules);
    *m = &testrt_module;

    if (argc > 1) {
        fname = argv[1];
    }
    mrklkit_init();
    mrklkit_ctx_init(&ctx, "test", &modules, NULL);
    test1();
    mrklkit_ctx_fini(&ctx, NULL);
    mrklkit_fini();
    array_fini(&modules);
    return 0;
}
