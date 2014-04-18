#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/modules/dparser.h>

#include "diag.h"
#include "unittest.h"

const char *fname;
const char *dtype;

static void
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

static void
test_int(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        uint64_t value = 0;
        char delim = 0;

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_int(&bs, ' ', '\n', &value, &delim,
                              0)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error");
        } else {
            TRACE("ok %ld", value);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
    }

    close(fd);
}

static void
test_float(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        double value = 0.0;
        char delim = 0;

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_float(&bs, ' ', '\n', &value, &delim,
                              0)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error");
        } else {
            TRACE("ok %lf", value);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
    }

    close(fd);
}

static void
test_qstr(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        char value[1024];
        char delim = 0;

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_qstr(&bs, ' ', '\n', value, sizeof(value), &delim,
                              DPARSE_MERGEDELIM)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok %s", value);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
    }

    close(fd);
}

int
main(int argc, char *argv[])
{
    test0();
    if (argc > 2) {
        dtype = argv[1];
        fname = argv[2];
        if (strcmp(dtype, "int") == 0) {
            test_int();
        } else if (strcmp(dtype, "float") == 0) {
            test_float();
        } else if (strcmp(dtype, "qstr") == 0) {
            test_qstr();
        } else {
            assert(0);
        }
    } else {
        assert(0);
    }
    return 0;
}
