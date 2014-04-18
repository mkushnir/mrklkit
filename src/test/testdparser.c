#include <assert.h>
#include <stdlib.h>
#include <string.h>
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

#define FDELIM ' '
//#define DPFLAGS DPARSE_NEXTONERROR
#define DPFLAGS DPARSE_MERGEDELIM
//#define DPFLAGS 0

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
        char rdelim[2] = {'\n', '\0'};

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_int(&bs,
                              FDELIM,
                              rdelim,
                              &value,
                              &delim,
                              DPFLAGS)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok, delim='%c' %ld", delim, value);
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
        char rdelim[2] = {'\n', '\0'};

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_float(&bs,
                                FDELIM,
                                rdelim,
                                &value,
                                &delim,
                                DPFLAGS)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok, delim='%c' %lf", delim, value);
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
        char rdelim[2] = {'\n', '\0'};

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_qstr(&bs,
                               FDELIM,
                               rdelim,
                               value,
                               sizeof(value), &delim,
                               DPFLAGS)) == DPARSE_NEEDMORE) {
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

static void
test_str(void)
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
        char rdelim[2] = {'\n', '\0'};

        //sleep(1);

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        if ((res = dparse_str(&bs,
                              FDELIM,
                              rdelim,
                              value,
                              sizeof(value),
                              &delim,
                              DPFLAGS)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok '%s'", value);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
    }

    close(fd);
}

static int
dump_int(uint64_t *v, UNUSED void *udata)
{
    TRACE("v=%ld", *v);
    return 0;
}

static int
dump_float(double *v, UNUSED void *udata)
{
    TRACE("v=%lf", *v);
    return 0;
}

static int
dump_byterange(byterange_t *v, bytestream_t *bs)
{
    char buf[1024];
    //D8(SDATA(bs, v->start), v->end - v->start);
    memset(buf, '\0', sizeof(buf));
    qstr_unescape(buf, SDATA(bs, v->start), v->end - v->start);
    TRACE("buf=%s", buf);
    return 0;
}

static void
test_array_int(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_array_t *arty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    arty = (lkit_array_t *)lkit_type_new(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_new(LKIT_INT);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        array_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        array_init(&value, sizeof(uint64_t), 0, NULL, NULL);

        if ((res = dparse_array(&bs,
                                FDELIM,
                                rdelim,
                                arty,
                                &value,
                                &delim,
                                DPFLAGS)) == DPARSE_NEEDMORE) {
            array_fini(&value);
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            array_traverse(&value, (array_traverser_t)dump_int, NULL);
        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_int, NULL);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        array_fini(&value);
    }

    close(fd);
}

static void
test_array_float(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_array_t *arty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    arty = (lkit_array_t *)lkit_type_new(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_new(LKIT_FLOAT);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        array_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        array_init(&value, sizeof(double), 0, NULL, NULL);

        if ((res = dparse_array(&bs,
                                FDELIM,
                                rdelim,
                                arty,
                                &value,
                                &delim,
                                DPFLAGS)) == DPARSE_NEEDMORE) {
            array_fini(&value);
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            array_traverse(&value, (array_traverser_t)dump_float, NULL);
        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_float, NULL);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        array_fini(&value);
    }

    close(fd);
}

static void
test_array_str(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_array_t *arty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    arty = (lkit_array_t *)lkit_type_new(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_new(LKIT_STR);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        array_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        array_init(&value, sizeof(byterange_t), 0, NULL, NULL);

        if ((res = dparse_array(&bs,
                                FDELIM,
                                rdelim,
                                arty,
                                &value,
                                &delim,
                                DPFLAGS)) == DPARSE_NEEDMORE) {
            array_fini(&value);
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            array_traverse(&value, (array_traverser_t)dump_byterange, NULL);
        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_byterange, &bs);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        array_fini(&value);
    }

    close(fd);
}

static void
test_array_qstr(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_array_t *arty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    arty = (lkit_array_t *)lkit_type_new(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_new(LKIT_STR);
    (*fty)->tag = LKIT_QSTR;

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        array_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        array_init(&value, sizeof(byterange_t), 0, NULL, NULL);

        if ((res = dparse_array(&bs,
                                FDELIM,
                                rdelim,
                                arty,
                                &value,
                                &delim,
                                DPFLAGS)) == DPARSE_NEEDMORE) {
            array_fini(&value);
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            array_traverse(&value, (array_traverser_t)dump_byterange, NULL);
        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_byterange, &bs);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        array_fini(&value);
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
        } else if (strcmp(dtype, "str") == 0) {
            test_str();
        } else if (strcmp(dtype, "array_int") == 0) {
            test_array_int();
        } else if (strcmp(dtype, "array_float") == 0) {
            test_array_float();
        } else if (strcmp(dtype, "array_str") == 0) {
            test_array_str();
        } else if (strcmp(dtype, "array_qstr") == 0) {
            test_array_qstr();
        } else {
            assert(0);
        }
    } else {
        assert(0);
    }
    return 0;
}
