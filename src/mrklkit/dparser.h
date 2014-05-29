#ifndef DPARSER_H
#define DPARSER_H
/**
 * Line-orientred textual data parser: decimal numbers, strings, quoted
 * strings, configurable field and end-of-record single-character delimiters.
 */

#include <unistd.h>

#include <mrkcommon/bytestream.h>
#include <mrkcommon/util.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPARSE_NEEDMORE (-1)
#define DPARSE_ERRORVALUE (-2)
#define DPARSE_READ (-3)
#define DPARSE_EOD (-4)

#define DPARSE_MERGEDELIM 0x01
#define DPARSE_RESETONERROR 0x02


void dparse_rt_struct_dump(rt_struct_t *);
int64_t dparse_struct_item_int(rt_struct_t *, int64_t);
double dparse_struct_item_float(rt_struct_t *, int64_t);
int64_t dparse_struct_item_bool(rt_struct_t *, int64_t);
bytes_t *dparse_struct_item_str(rt_struct_t *, int64_t);
rt_array_t *dparse_struct_item_array(rt_struct_t *, int64_t);
rt_dict_t *dparse_struct_item_dict(rt_struct_t *, int64_t);
rt_struct_t *dparse_struct_item_struct(rt_struct_t *, int64_t);

void dparser_reach_delim(bytestream_t *, char, off_t);
void dparser_reach_value(bytestream_t *,
                         char,
                         off_t);
void dparser_reach_value_m(bytestream_t *,
                           char, off_t);

int dparse_int(bytestream_t *,
               char,
               off_t,
               int64_t *,
               unsigned int);
int dparse_float(bytestream_t *,
                 char,
                 off_t,
                 double *,
                 unsigned int);
int dparse_str(bytestream_t *,
               char,
               off_t,
               OUT bytes_t **,
               unsigned int);
int dparse_qstr(bytestream_t *,
                char,
                off_t,
                OUT bytes_t **,
                unsigned int);
int dparse_array(bytestream_t *,
                 char,
                 off_t,
                 OUT rt_array_t *,
                 unsigned int);
int dparse_dict(bytestream_t *,
                char,
                off_t,
                OUT rt_dict_t *,
                unsigned int);

int dparse_struct(bytestream_t *,
                  char,
                  off_t,
                  rt_struct_t *,
                  char *,
                  unsigned int);

void
dparse_struct_setup(bytestream_t *,
                    const byterange_t *,
                    rt_struct_t *);

typedef int (*dparser_read_lines_cb_t)(bytestream_t *, const byterange_t *, void *);
typedef int (*dparser_bytestream_recycle_cb_t)(void *);
int dparser_read_lines(int,
                       bytestream_t *,
                       dparser_read_lines_cb_t,
                       dparser_bytestream_recycle_cb_t,
                       void *);

#ifdef __cplusplus
}
#endif

#endif

