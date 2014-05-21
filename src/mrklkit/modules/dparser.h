#ifndef DPARSER_H
#define DPARSER_H
/**
 * Line-orientred textual data parser: decimal numbers, strings, quoted
 * strings, configurable field and end-of-record single-character delimiters.
 */

#include <unistd.h>

#include <mrkcommon/bytestream.h>

#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPARSE_NEEDMORE (-1)
#define DPARSE_ERRORVALUE (-2)
#define DPARSE_READ (-3)
#define DPARSE_EOD (-4)

#define DPARSE_MERGEDELIM 0x01
#define DPARSE_RESETONERROR 0x02

#define DPARSE_READSZ (1024 * 4)

void qstr_unescape(char *, const char *, size_t);
void dparser_reach_delim(bytestream_t *, char, off_t);
int dparser_reach_delim_readmore(bytestream_t *, int, char, off_t);
void dparser_reach_value(bytestream_t *, char, off_t);
void dparser_reach_value_m(bytestream_t *, char, off_t);
typedef int (*dparser_read_lines_cb_t)(bytestream_t *, byterange_t *, void *);
int dparser_read_lines(int,
                       dparser_read_lines_cb_t,
                       void *);

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
int dparse_qstr(bytestream_t *,
                char,
                off_t,
                bytes_t **,
                unsigned int);
int dparse_str(bytestream_t *,
               char,
               off_t,
               bytes_t **,
               unsigned int);
int dparse_array(bytestream_t *,
                 char,
                 off_t,
                 rt_array_t *,
                 unsigned int);
int dparse_dict(bytestream_t *,
                char,
                off_t,
                rt_dict_t *,
                unsigned int);
int dparse_struct(bytestream_t *,
                  char,
                  off_t,
                  rt_struct_t *,
                  char *,
                  unsigned int);

#ifdef __cplusplus
}
#endif

#endif

