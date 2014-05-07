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

static const char *prog, *input;
static uint64_t flags;

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

    if ((res = mrklkit_compile(fd, flags)) != 0) {
        FAIL("mrklkit_compile");
    }

    close(fd);

    /* post-init, need to integrate it better */
    if ((ds = dsource_get("qwe")) == NULL) {
        FAIL("dsource_get");
    }
    ds->rdelim[0] = '\n';
    ds->rdelim[1] = '\0';

    if (mrklkit_init_runtime() != 0) {
        FAIL("mrklkit_init_runtime");
    }

    if ((fd = open(input, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs, 4096);
    nread = 0xffffffff;
    while (nread > 0) {
        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 4096);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = testrt_run(&bs, ds)) == DPARSE_NEEDMORE) {
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            break;
        } else {
            ;
        }
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

    array_init(&modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    m = array_incr(&modules);
    *m = &testrt_module;

    mrklkit_init(&modules);

    test1();

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
