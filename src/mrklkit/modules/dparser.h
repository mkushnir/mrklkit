#ifndef DPARSER_H
#define DPARSER_H
/**
 * Line-orientred textual data parser: decimal numbers, strings, quoted
 * strings, configurable field and end-of-record single-character delimiters.
 */
#include <mrkcommon/bytestream.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lruntime.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPARSE_NEEDMORE (-1)
#define DPARSE_ERRORVALUE (-2)

#define DPARSE_MERGEDELIM 0x01
#define DPARSE_RESETONERROR 0x02
void qstr_unescape(char *, const char *, size_t);
int dparse_int(bytestream_t *,
               char,
               char[2],
               int64_t *,
               char *,
               unsigned int);
int dparse_float(bytestream_t *,
                 char,
                 char[2],
                 double *,
                 char *,
                 unsigned int);
int dparse_qstr(bytestream_t *,
                char,
                char[2],
                bytes_t **,
                char *,
                unsigned int);
int dparse_str(bytestream_t *,
               char,
               char[2],
               bytes_t **,
               char *,
               unsigned int);
int dparse_array(bytestream_t *,
                 char,
                 char[2],
                 rt_array_t *,
                 char *,
                 unsigned int);
int dparse_dict(bytestream_t *,
                char,
                char[2],
                rt_dict_t *,
                char *,
                unsigned int);
int dparse_struct(bytestream_t *,
                  char,
                  char[2],
                  lkit_struct_t *,
                  rt_struct_t *,
                  char *,
                  unsigned int);

#ifdef __cplusplus
}
#endif

#endif

