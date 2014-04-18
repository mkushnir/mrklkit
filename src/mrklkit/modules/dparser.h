#ifndef DPARSER_H
#define DPARSER_H
/**
 * Line-orientred textual data parser: decimal numbers, strings, quoted
 * strings, configurable field and end-of-record single-character delimiters.
 */
#include <mrkcommon/bytestream.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPARSE_NEEDMORE (-1)
#define DPARSE_ERRORVALUE (-2)

#define DPARSE_MERGEDELIM 0x01
#define DPARSE_RESETONERROR 0x02
int dparse_int(bytestream_t *, char, char, uint64_t *, char *, unsigned int);
int dparse_float(bytestream_t *, char, char, double *, char *, unsigned int);
int dparse_str(bytestream_t *, char, char, char *, size_t, char *, unsigned int);
int dparse_qstr(bytestream_t *, char, char, char *, size_t, char *, unsigned int);

#ifdef __cplusplus
}
#endif

#endif

