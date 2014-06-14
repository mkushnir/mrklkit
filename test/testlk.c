#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include <mrkcommon/bytestream.h>
#include <mrkcommon/dumpm.h>

#include "unittest.h"
#include "diag.h"

#include <mrklkit/mrklkit.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/dparser.h>
#include <mrklkit/modules/testrt.h>

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

static const char *prog, *input;
static uint64_t flags;

testrt_ctx_t tctx;

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
    bytestream_t bs;
    size_t nlines = 0;

    if ((fd = open(prog, O_RDONLY)) == -1) {
        FAIL("open");
    }

    if ((res = mrklkit_compile(&tctx.mctx, fd, flags, &tctx)) != 0) {
        FAIL("mrklkit_compile");
    }

    close(fd);

    /* post-init, need to integrate it better */
    if ((tctx.ds = dsource_get("qwe")) == NULL) {
        FAIL("dsource_get");
    }
    tctx.ds->rdelim[0] = '\n';
    tctx.ds->rdelim[1] = '\0';

    if (mrklkit_ctx_init_runtime(&tctx.mctx, &tctx) != 0) {
        FAIL("mrklkit_ctx_init_runtime");
    }

    if ((fd = open(input, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs, 1024*1024);

    if (dparser_read_lines(fd,
                           &bs,
                           (dparser_read_lines_cb_t)testrt_run_once,
                           NULL,
                           &tctx,
                           &nlines) != 0) {
        TRACE("error");
    }

    bytestream_fini(&bs);

    close(fd);

    /**/
    testrt_dump_targets();
}


static void
run(void)
{
    array_t modules;
    mrklkit_module_t **m;

    mrklkit_init();

    array_init(&modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    m = array_incr(&modules);
    *m = &testrt_module;
    mrklkit_ctx_init(&tctx.mctx, "test", &modules, &tctx);

    test1();

    mrklkit_ctx_fini(&tctx.mctx, &tctx);
    mrklkit_fini();
    array_fini(&modules);
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

    if (argc > 2) {
        prog = argv[1];
        input = argv[2];
        run();
    } else {
        FAIL("main");
    }
    return 0;
}
