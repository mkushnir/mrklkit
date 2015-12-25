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

#include <mrkcommon/mpool.h>

#include <mrklkit/lruntime.h>
#include <mrklkit/dpexpr.h>
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


typedef struct _rt_parser_info {
    /* weak ref */
    bytestream_t *bs;
    byterange_t br;
    lkit_dpstruct_t *dpexpr;
    rt_struct_t *value;
    off_t pos;
    int next_delim;
    int current;
    /* delimiter positions */
    off_t *dpos;
} rt_parser_info_t;


void rt_parser_info_init(rt_parser_info_t *,
                         bytestream_t *,
                         const byterange_t *,
                         lkit_dpstruct_t *,
                         rt_struct_t *);
void rt_parser_info_fini(rt_parser_info_t *);
bytes_t *rt_parser_info_data_mpool(rt_parser_info_t *);
bytes_t *rt_parser_info_data(rt_parser_info_t *);
off_t rt_parser_info_pos(rt_parser_info_t *);


int64_t dparse_struct_item_ra_int(rt_parser_info_t *, int64_t);
double dparse_struct_item_ra_float(rt_parser_info_t *, int64_t);
int64_t dparse_struct_item_ra_bool(rt_parser_info_t *, int64_t);
bytes_t *dparse_struct_item_ra_str(rt_parser_info_t *, int64_t);
rt_array_t *dparse_struct_item_ra_array(rt_parser_info_t *, int64_t);
rt_dict_t *dparse_struct_item_ra_dict(rt_parser_info_t *, int64_t);
rt_struct_t *dparse_struct_item_ra_struct(rt_parser_info_t *, int64_t);

int64_t dparser_strtoi64(char *, char **, char);
double dparser_strtod(char *, char **, char);

rt_dict_t *dparse_dict_from_bytes(lkit_dpdict_t *, bytes_t *);
rt_array_t *dparse_array_from_bytes(lkit_dparray_t *, bytes_t *);

off_t dparse_pi_pos(rt_parser_info_t *);

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

void dparser_set_mpool(mpool_ctx_t *);
#ifdef __cplusplus
}
#endif

#endif

