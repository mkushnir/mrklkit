#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <mrkcommon/bytes.h>
#define TRRET_DEBUG
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/dparser.h>

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
mycb(UNUSED bytestream_t *bs, const byterange_t *br, UNUSED void *udata)
{
    //D8(SDATA(bs, br->start), br->end - br->start);
    TRACE("%ld/%ld", br->start, br->end);
    return 0;
}


static int
myrcb(UNUSED void *udata)
{
    return 0;
}



int
main(int argc, char *argv[])
{
    mrklkit_ctx_t mctx;

    mrklkit_init();

    mrklkit_ctx_init(&mctx, "test", NULL, NULL, 0);

    if (argc > 2) {
        int fd;
        bytestream_t bs;
        size_t nlines = 0;
        size_t nbytes = 0;

        dtype = argv[1];
        fname = argv[2];

        if ((fd = open(fname, O_RDONLY)) == -1) {
            FAIL("open");
        }


        bytestream_init(&bs, 65536);


        if (strcmp(dtype, "int") == 0) {
        } else if (strcmp(dtype, "float") == 0) {
        } else if (strcmp(dtype, "qstr") == 0) {
        } else if (strcmp(dtype, "str") == 0) {
        } else if (strcmp(dtype, "aint") == 0) {
        } else if (strcmp(dtype, "afloat") == 0) {
        } else if (strcmp(dtype, "astr") == 0) {
        } else if (strcmp(dtype, "dint") == 0) {
        } else if (strcmp(dtype, "dfloat") == 0) {
        } else if (strcmp(dtype, "dstr") == 0) {
        } else if (strcmp(dtype, "st00") == 0) {
        } else {
            //assert(0)const ;
        }

        nlines = 0;
        nbytes = 0;
        while (1) {
            int res;

            TRACE("%ld/%ld", nlines, nbytes);
            if ((res = dparser_read_lines_unix(fd,
                                               &bs,
                                               mycb,
                                               myrcb,
                                               NULL,
                                               &nlines,
                                               &nbytes)) != 0) {
                if (res == DPARSE_EOD) {
                    TRACE("DPARSE_EOD: for more data ...");
                    sleep(1);
                    continue;
                }
                TR(res);
                break;
            }
        }

        bytestream_fini(&bs);
        close(fd);

    } else {
        assert(0);
    }

    mrklkit_ctx_fini(&mctx);
    mrklkit_fini();
    return 0;
}
