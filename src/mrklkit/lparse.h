#ifndef LPARSE_H_DEFINED
#define LPARSE_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrklkit/fparser.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* quals_parser_t)(array_t *,
                               array_iter_t *,
                               char *,
                               void *);

int lparse_first_word(array_t *, array_iter_t *, char **, int);
int lparse_first_word_bytes(array_t *, array_iter_t *, bytes_t **, int);
int lparse_first_word_datum(array_t *, array_iter_t *, fparser_datum_t **, int);
int lparse_next_word(array_t *, array_iter_t *, char **, int);
int lparse_next_word_bytes(array_t *, array_iter_t *, bytes_t **, int);
int lparse_next_alnum_bytes(array_t *, array_iter_t *, bytes_t **, int);
int lparse_next_word_datum(array_t *, array_iter_t *, fparser_datum_t **, int);
int lparse_next_str(array_t *, array_iter_t *, char **, int);
int lparse_next_char(array_t *, array_iter_t *, char *, int);
int lparse_next_str_bytes(array_t *, array_iter_t *, bytes_t **, int);
int lparse_next_str_datum(array_t *, array_iter_t *, fparser_datum_t **, int);
int lparse_first_int(array_t *, array_iter_t *, int64_t *, int);
int lparse_next_int(array_t *, array_iter_t *, int64_t *, int);
int lparse_first_double(array_t *, array_iter_t *, double *, int);
int lparse_next_double(array_t *, array_iter_t *, double *, int);
int lparse_next_bool(array_t *, array_iter_t *, char *, int);
int lparse_next_sequence(array_t *, array_iter_t *, fparser_datum_t **, int);
int lparse_quals(array_t *,
                 array_iter_t *,
                 quals_parser_t,
                 void *);
#ifdef __cplusplus
}
#endif
#endif /* LPARSE_H_DEFINED */
