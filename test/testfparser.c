#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>
#include <mrklkit/fparser.h>

#include "unittest.h"
#include "diag.h"

static int
mycb(UNUSED const char *buf,
     fparser_datum_t *dat,
     UNUSED void *udata)
{
    return fparser_datum_dump(&dat, udata);
}

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
    fparser_datum_t *root = NULL;

    if ((fd = open("data-04", O_RDONLY)) < 0) {
        FAIL("open");
    }

    root = fparser_parse(fd, mycb, NULL);
    assert(root != NULL);
    close(fd);
    fparser_datum_destroy(&root);
}

static void
test2(void)
{
    int fd;
    fparser_datum_t *root = NULL;

    if ((fd = open("data-03", O_RDONLY)) < 0) {
        FAIL("open");
    }

    root = fparser_parse(fd, mycb, NULL);
    assert(root != NULL);
    close(fd);
    fparser_datum_dump(&root, NULL);
    fparser_datum_destroy(&root);
}

int
main(void)
{
    test0();
    test1();
    test2();
    return 0;
}
