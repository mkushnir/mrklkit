#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
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
        int64_t value = 0;
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
    bytestream_fini(&bs);

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
    bytestream_fini(&bs);

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
        bytes_t *value = NULL;
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
                               &value,
                               &delim,
                               DPFLAGS)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok %s", value->data);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        bytes_destroy(&value);
    }
    bytestream_fini(&bs);

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
        bytes_t *value = NULL;
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
                              &value,
                              &delim,
                              DPFLAGS)) == DPARSE_NEEDMORE) {
            continue;
        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
        } else {
            TRACE("ok '%s'", value->data);
        }
        if (delim == '\n') {
            TRACE("EOL");
        }
        bytes_destroy(&value);
    }
    bytestream_fini(&bs);

    close(fd);
}


static int
dump_int_array(int64_t *v, UNUSED void *udata)
{
    TRACE("v=%ld", *v);
    return 0;
}


static int
dump_float_array(double *v, UNUSED void *udata)
{
    TRACE("v=%lf", *v);
    return 0;
}


static int
dump_bytes_array(bytes_t **v, UNUSED void *udata)
{
    TRACE("v=%s", (*v)->data);
    return 0;
}


static int
dump_int_dict(bytes_t *k, int64_t v, UNUSED void *udata)
{
    TRACE("%s:%ld", k->data, v);
    return 0;
}


static int
dump_float_dict(bytes_t *k, void *v, UNUSED void *udata)
{
    union {
        void *p;
        double d;
    } vv;
    vv.p = v;
    TRACE("%s:%lf", k->data, vv.d);
    return 0;
}


static int
dump_bytes_dict(bytes_t *k, bytes_t *v, UNUSED void *udata)
{
    TRACE("%s:%s", k->data, v->data);
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

    arty = (lkit_array_t *)lkit_type_get(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_get(LKIT_INT);

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

        mrklkit_rt_array_init(&value, *fty);

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
            array_traverse(&value, (array_traverser_t)dump_int_array, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_int_array, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        array_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&arty);

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

    arty = (lkit_array_t *)lkit_type_get(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_get(LKIT_FLOAT);

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

        mrklkit_rt_array_init(&value, *fty);

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
            array_traverse(&value, (array_traverser_t)dump_float_array, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_float_array, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        array_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&arty);

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

    arty = (lkit_array_t *)lkit_type_get(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_get(LKIT_STR);

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

        mrklkit_rt_array_init(&value, *fty);

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
            array_traverse(&value, (array_traverser_t)dump_bytes_array, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_bytes_array, &bs);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        array_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&arty);

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

    arty = (lkit_array_t *)lkit_type_get(LKIT_ARRAY);
    //arty->parser = PLIT_PARSER_DELIM;
    arty->delim = (unsigned char *)",";
    fty = array_incr(&arty->fields);
    *fty = lkit_type_get(LKIT_STR);

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

        mrklkit_rt_array_init(&value, *fty);

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
            array_traverse(&value, (array_traverser_t)dump_bytes_array, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            array_traverse(&value, (array_traverser_t)dump_bytes_array, &bs);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        array_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&arty);

    close(fd);
}


static void
test_dict_int(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_dict_t *dcty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    dcty = (lkit_dict_t *)lkit_type_get(LKIT_DICT);
    dcty->kvdelim = (unsigned char *)":";
    dcty->fdelim = (unsigned char *)",";
    fty = array_incr(&dcty->fields);
    *fty = lkit_type_get(LKIT_INT);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        dict_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        mrklkit_rt_dict_init(&value, *fty);

        if ((res = dparse_dict(&bs,
                               FDELIM,
                               rdelim,
                               dcty,
                               &value,
                               &delim,
                               DPFLAGS)) == DPARSE_NEEDMORE) {
            dict_fini(&value);
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            dict_traverse(&value, (dict_traverser_t)dump_int_dict, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            dict_traverse(&value, (dict_traverser_t)dump_int_dict, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        dict_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&dcty);

    close(fd);
}


static void
test_dict_float(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_dict_t *dcty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    dcty = (lkit_dict_t *)lkit_type_get(LKIT_DICT);
    dcty->kvdelim = (unsigned char *)":";
    dcty->fdelim = (unsigned char *)",";
    fty = array_incr(&dcty->fields);
    *fty = lkit_type_get(LKIT_FLOAT);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        dict_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        mrklkit_rt_dict_init(&value, *fty);

        if ((res = dparse_dict(&bs,
                               FDELIM,
                               rdelim,
                               dcty,
                               &value,
                               &delim,
                               DPFLAGS)) == DPARSE_NEEDMORE) {
            dict_fini(&value);
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            dict_traverse(&value, (dict_traverser_t)dump_float_dict, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            dict_traverse(&value, (dict_traverser_t)dump_float_dict, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        dict_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&dcty);

    close(fd);
}


static void
test_dict_str(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_dict_t *dcty;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    dcty = (lkit_dict_t *)lkit_type_get(LKIT_DICT);
    dcty->kvdelim = (unsigned char *)":";
    dcty->fdelim = (unsigned char *)",";
    fty = array_incr(&dcty->fields);
    *fty = lkit_type_get(LKIT_STR);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        dict_t value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        mrklkit_rt_dict_init(&value, *fty);

        if ((res = dparse_dict(&bs,
                               FDELIM,
                               rdelim,
                               dcty,
                               &value,
                               &delim,
                               DPFLAGS)) == DPARSE_NEEDMORE) {
            dict_fini(&value);
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            dict_traverse(&value, (dict_traverser_t)dump_bytes_dict, NULL);

        } else {
            TRACE("ok, delim='%c':", delim);
            dict_traverse(&value, (dict_traverser_t)dump_bytes_dict, NULL);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        dict_fini(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&dcty);

    close(fd);
}

static void
struct_00_dump(rt_struct_t *value)
{
    int64_t fint;
    double ffloat;
    bytes_t *fstr;

    fint = mrklkit_rt_get_struct_item_int(value, 0);
    TRACE("val=%ld", fint);
    ffloat = mrklkit_rt_get_struct_item_float(value, 1);
    TRACE("val=%lf", ffloat);
    fstr = mrklkit_rt_get_struct_item_str(value, 2);
    TRACE("val=%s", fstr->data);
}

static void
struct_00_init(void **value)
{
    int64_t *fint;
    union {
        void **v;
        double *d;
    } ffloat;
    bytes_t **fstr;

    fint = (int64_t *)value;
    *fint = 0;
    ffloat.v = value + 1;
    *ffloat.d = 0.0;
    fstr = (bytes_t **)(value + 2);
    *fstr = NULL;
}

static void
struct_00_fini(void **value)
{
    bytes_t **fstr;
    fstr = (bytes_t **)(value + 2);
    bytes_destroy(fstr);
}


static void
test_struct_00(void)
{
    int fd;
    bytestream_t bs;
    ssize_t nread;
    lkit_struct_t *stty;
    char **fnam;
    lkit_type_t **fty;

    if ((fd = open(fname, O_RDONLY)) == -1) {
        FAIL("open");
    }

    /*
     * (type foo (struct (fint int) (ffloat float) (fstr str)))
     */
    stty = (lkit_struct_t *)lkit_type_get(LKIT_STRUCT);
    stty->delim = (unsigned char *)" ";
    stty->init = struct_00_init;
    stty->fini = struct_00_fini;

    fnam = array_incr(&stty->names);
    *fnam = "fint";
    fty = array_incr(&stty->fields);
    *fty = lkit_type_get(LKIT_INT);

    fnam = array_incr(&stty->names);
    *fnam = "ffloat";
    fty = array_incr(&stty->fields);
    *fty = lkit_type_get(LKIT_FLOAT);

    fnam = array_incr(&stty->names);
    *fnam = "fstr";
    fty = array_incr(&stty->fields);
    *fty = lkit_type_get(LKIT_STR);

    bytestream_init(&bs);
    nread = 0xffffffff;
    while (nread > 0) {
        int res;
        rt_struct_t *value;
        char delim = 0;
        char rdelim[2] = {'\n', '\0'};

        if (SNEEDMORE(&bs)) {
            nread = bytestream_read_more(&bs, fd, 1024);
            //TRACE("nread=%ld", nread);
            continue;
        }

        value = mrklkit_rt_struct_new(stty);

        if ((res = dparse_struct(&bs,
                                 FDELIM,
                                 rdelim,
                                 stty,
                                 value,
                                 &delim,
                                 DPFLAGS)) == DPARSE_NEEDMORE) {
            mrklkit_rt_struct_destroy(&value);
            continue;

        } else if (res == DPARSE_ERRORVALUE) {
            TRACE("error, delim='%c'", delim);
            struct_00_dump(value);

        } else {
            TRACE("ok, delim='%c':", delim);
            struct_00_dump(value);
        }

        if (delim == '\n') {
            TRACE("EOL");
        }

        mrklkit_rt_struct_destroy(&value);
    }
    bytestream_fini(&bs);
    lkit_type_destroy((lkit_type_t **)&stty);

    close(fd);
}


int
main(int argc, char *argv[])
{
    mrklkit_init(NULL);
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
        } else if (strcmp(dtype, "dict_int") == 0) {
            test_dict_int();
        } else if (strcmp(dtype, "dict_float") == 0) {
            test_dict_float();
        } else if (strcmp(dtype, "dict_str") == 0) {
            test_dict_str();
        } else if (strcmp(dtype, "struct_00") == 0) {
            test_struct_00();
        } else {
            assert(0);
        }
    } else {
        assert(0);
    }
    mrklkit_fini();
    return 0;
}
