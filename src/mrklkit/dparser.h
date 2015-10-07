#ifndef DPARSER_H
#define DPARSER_H
/**
 * Line-orientred textual data parser: decimal numbers, strings, quoted
 * strings, configurable field and end-of-record single-character delimiters.
 */

#include <limits.h>
#ifndef OFF_MAX
#   define OFF_MAX LONG_MAX
#endif
#include <bzlib.h>
#include <unistd.h>

#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#ifdef USE_MPOOL
#include <mrkcommon/mpool.h>
#endif
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPARSE_NEEDMORE (-1)
#define DPARSE_ERRORVALUE (-2)
#define DPARSE_EOD (-3)

#define DPARSE_MERGEDELIM 0x01
#define DPARSE_RESETONERROR 0x02

typedef struct _dparse_bz2_ctx {
    off_t fpos;
    bytes_t *tail;
} dparse_bz2_ctx_t;

void dparser_bz2_ctx_init(dparse_bz2_ctx_t *);
void dparser_bz2_ctx_fini(dparse_bz2_ctx_t *);

void dparse_rt_struct_dump(rt_struct_t *);
int64_t dparse_struct_item_ra_int(rt_struct_t *, int64_t);
double dparse_struct_item_ra_float(rt_struct_t *, int64_t);
int64_t dparse_struct_item_ra_bool(rt_struct_t *, int64_t);
bytes_t *dparse_struct_item_ra_str(rt_struct_t *, int64_t);
rt_array_t *dparse_struct_item_ra_array(rt_struct_t *, int64_t);
rt_dict_t *dparse_struct_item_ra_dict(rt_struct_t *, int64_t);
rt_struct_t *dparse_struct_item_ra_struct(rt_struct_t *, int64_t);

void dparser_reach_delim(bytestream_t *, char, off_t);
void dparser_reach_value_m(bytestream_t *,
                           char, off_t);
int64_t dparser_strtoi64(char *, char **, char);
double dparser_strtod(char *, char **, char);

void
dparse_struct_setup(bytestream_t *,
                    const byterange_t *,
                    rt_struct_t *);

off_t dparse_struct_pi_pos(rt_struct_t *);

typedef int (*dparser_read_lines_cb_t)(bytestream_t *,
                                       const byterange_t *,
                                       void *);

typedef int (*dparser_bytestream_recycle_cb_t)(void *);

int dparser_read_lines_unix(int,
                            bytestream_t *,
                            dparser_read_lines_cb_t,
                            dparser_bytestream_recycle_cb_t,
                            void *,
                            size_t *,
                            size_t *);

int dparser_read_lines_win(int,
                           bytestream_t *,
                           dparser_read_lines_cb_t,
                           dparser_bytestream_recycle_cb_t,
                           void *,
                           size_t *,
                           size_t *);

int dparser_read_lines_bz2_unix(BZFILE *,
                                bytestream_t *,
                                dparser_read_lines_cb_t,
                                dparser_bytestream_recycle_cb_t,
                                void *,
                                size_t *,
                                size_t *);

int dparser_read_lines_bz2_win(BZFILE *,
                               bytestream_t *,
                               dparser_read_lines_cb_t,
                               dparser_bytestream_recycle_cb_t,
                               void *,
                               size_t *,
                               size_t *);

#ifdef USE_MPOOL
void dparser_set_mpool(mpool_ctx_t *);
#endif
#ifdef __cplusplus
}
#endif

#endif

