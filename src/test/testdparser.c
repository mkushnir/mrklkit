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


static int
test_int(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    while (SPOS(bs) < br->end) {
        int64_t value;

        if (dparse_int(bs,
                       FDELIM,
                       br->end,
                       &value,
                       DPFLAGS) == DPARSE_ERRORVALUE) {
            //TRACE("err %ld", value);
        } else {
            //TRACE("ok %ld", value);
        }
        //D8(SPDATA(bs), br->end - SPOS(bs));
        //dparser_reach_delim(bs, FDELIM, br->end);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS);
        //D8(SPDATA(bs), br->end - SPOS(bs));
    }
    return 0;
}


static int
test_float(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    while (SPOS(bs) < br->end) {
        double value;

        if (dparse_float(bs,
                         FDELIM,
                         br->end,
                         &value,
                         DPFLAGS) == DPARSE_ERRORVALUE) {
            TRACE("err %lf", value);
        } else {
            TRACE("ok %lf", value);
        }
        //D8(SPDATA(bs), br->end - SPOS(bs));
        //dparser_reach_delim(bs, FDELIM, br->end);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS);
        //D8(SPDATA(bs), br->end - SPOS(bs));
    }
    return 0;
}


static int
test_str(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    while (SPOS(bs) < br->end) {
        bytes_t *value = NULL;

        if (dparse_str(bs,
                         FDELIM,
                         br->end,
                         &value,
                         DPFLAGS) == DPARSE_ERRORVALUE) {
            TRACE("err %s", value->data);
        } else {
            TRACE("ok %s", value->data);
        }
        //D8(SPDATA(bs), br->end - SPOS(bs));
        //dparser_reach_delim(bs, FDELIM, br->end);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        mrklkit_bytes_destroy(&value);
    }
    return 0;
}


static int
test_qstr(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    while (SPOS(bs) < br->end) {
        bytes_t *value = NULL;

        if (dparse_qstr(bs,
                         FDELIM,
                         br->end,
                         &value,
                         DPFLAGS) == DPARSE_ERRORVALUE) {
            TRACE("err %s", value != NULL ? value->data : NULL);
        } else {
            TRACE("ok %s", value->data);
        }
        //D8(SPDATA(bs), br->end - SPOS(bs));
        //dparser_reach_delim(bs, FDELIM, br->end);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS);
        //D8(SPDATA(bs), br->end - SPOS(bs));
        mrklkit_bytes_destroy(&value);
    }
    return 0;
}


#define TEST_ARRAY(tag) \
    lkit_array_t *arty; \
    lkit_type_t **fty; \
    arty = (lkit_array_t *)lkit_type_get(LKIT_ARRAY); \
    arty->delim = (unsigned char *)","; \
    fty = array_incr(&arty->fields); \
    *fty = lkit_type_get(tag); \
    while (SPOS(bs) < br->end) { \
        rt_array_t *value = NULL; \
        value = mrklkit_rt_array_new(arty); \
        if (dparse_array(bs, \
                         FDELIM, \
                         br->end, \
                         value, \
                         DPFLAGS) == DPARSE_ERRORVALUE) { \
            TRACE("err"); \
        } else { \
            TRACE("ok"); \
        } \
        if (value != NULL) { \
            mrklkit_rt_array_dump(value); \
            TRACEC("\n"); \
        } \
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS); \
        mrklkit_rt_array_destroy(&value); \
    } \
    lkit_type_destroy((lkit_type_t **)&arty); \

static int
test_array_int(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_ARRAY(LKIT_INT);
    return 0;
}


static int
test_array_float(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_ARRAY(LKIT_FLOAT);
    return 0;
}


static int
test_array_str(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_ARRAY(LKIT_STR);
    return 0;
}


#define TEST_DICT(tag) \
    lkit_dict_t *dcty; \
    lkit_type_t **fty; \
    dcty = (lkit_dict_t *)lkit_type_get(LKIT_DICT); \
    dcty->kvdelim = (unsigned char *)":"; \
    dcty->fdelim = (unsigned char *)","; \
    fty = array_incr(&dcty->fields); \
    *fty = lkit_type_get(tag); \
    while (SPOS(bs) < br->end) { \
        rt_dict_t *value = NULL; \
        value = mrklkit_rt_dict_new(dcty); \
        if (dparse_dict(bs, \
                        FDELIM, \
                        br->end, \
                        value, \
                        DPFLAGS) == DPARSE_ERRORVALUE) { \
            TRACE("err"); \
        } else { \
            TRACE("ok"); \
        } \
        if (value != NULL) { \
            mrklkit_rt_dict_dump(value); \
            TRACEC("\n"); \
        } \
        dparser_reach_value(bs, FDELIM, br->end, DPFLAGS); \
        mrklkit_rt_dict_destroy(&value); \
    } \
    lkit_type_destroy((lkit_type_t **)&dcty);


static int
test_dict_int(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_DICT(LKIT_INT);
    return 0;
}


static int
test_dict_float(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_DICT(LKIT_FLOAT);
    return 0;
}


static int
test_dict_str(bytestream_t *bs, byterange_t *br, UNUSED void *udata)
{
    TEST_DICT(LKIT_STR);
    return 0;
}


#if 0
UNUSED static void
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

UNUSED static void
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

UNUSED static void
struct_00_fini(void **value)
{
    bytes_t **fstr;
    fstr = (bytes_t **)(value + 2);
    mrklkit_bytes_destroy(fstr);
}


UNUSED static void
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

    bytestream_init(&bs, 4096);
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

#endif

int
main(int argc, char *argv[])
{
    mrklkit_init();
    if (argc > 2) {
        int fd;

        dtype = argv[1];
        fname = argv[2];

        if ((fd = open(fname, O_RDONLY)) == -1) {
            FAIL("open");
        }


        if (strcmp(dtype, "int") == 0) {
            dparser_read_lines(fd, test_int, NULL);
        } else if (strcmp(dtype, "float") == 0) {
            dparser_read_lines(fd, test_float, NULL);
        } else if (strcmp(dtype, "qstr") == 0) {
            dparser_read_lines(fd, test_qstr, NULL);
        } else if (strcmp(dtype, "str") == 0) {
            dparser_read_lines(fd, test_str, NULL);
        } else if (strcmp(dtype, "aint") == 0) {
            dparser_read_lines(fd, test_array_int, NULL);
        } else if (strcmp(dtype, "afloat") == 0) {
            dparser_read_lines(fd, test_array_float, NULL);
        } else if (strcmp(dtype, "astr") == 0) {
            dparser_read_lines(fd, test_array_str, NULL);
        } else if (strcmp(dtype, "dint") == 0) {
            dparser_read_lines(fd, test_dict_int, NULL);
        } else if (strcmp(dtype, "dfloat") == 0) {
            dparser_read_lines(fd, test_dict_float, NULL);
        } else if (strcmp(dtype, "dstr") == 0) {
            dparser_read_lines(fd, test_dict_str, NULL);
        //} else if (strcmp(dtype, "st00") == 0) {
        //    test_struct_00();
        } else {
            assert(0);
        }

        close(fd);

    } else {
        assert(0);
    }
    mrklkit_fini();
    return 0;
}
