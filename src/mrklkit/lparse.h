#ifndef LPARSE_H_DEFINED
#define LPARSE_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrklkit/fparser.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* quals_parser_t)(mnarray_t *,
                               mnarray_iter_t *,
                               char *,
                               void *);

int lparse_first_word(mnarray_t *, mnarray_iter_t *, char **, int);
int lparse_first_word_bytes(mnarray_t *, mnarray_iter_t *, mnbytes_t **, int);
int lparse_first_word_datum(mnarray_t *, mnarray_iter_t *, fparser_datum_t **, int);
int lparse_next_word(mnarray_t *, mnarray_iter_t *, char **, int);
int lparse_next_word_bytes(mnarray_t *, mnarray_iter_t *, mnbytes_t **, int);
int lparse_next_alnum_bytes(mnarray_t *, mnarray_iter_t *, mnbytes_t **, int);
int lparse_next_word_datum(mnarray_t *, mnarray_iter_t *, fparser_datum_t **, int);
int lparse_next_str(mnarray_t *, mnarray_iter_t *, char **, int);
int lparse_next_char(mnarray_t *, mnarray_iter_t *, char *, int);
int lparse_next_str_bytes(mnarray_t *, mnarray_iter_t *, mnbytes_t **, int);
int lparse_next_str_datum(mnarray_t *, mnarray_iter_t *, fparser_datum_t **, int);
int lparse_first_int(mnarray_t *, mnarray_iter_t *, int64_t *, int);
int lparse_next_int(mnarray_t *, mnarray_iter_t *, int64_t *, int);
int lparse_first_double(mnarray_t *, mnarray_iter_t *, double *, int);
int lparse_next_double(mnarray_t *, mnarray_iter_t *, double *, int);
int lparse_next_bool(mnarray_t *, mnarray_iter_t *, char *, int);
int lparse_next_sequence(mnarray_t *, mnarray_iter_t *, fparser_datum_t **, int);
int lparse_quals(mnarray_t *,
                 mnarray_iter_t *,
                 quals_parser_t,
                 void *);
#ifdef __cplusplus
}
#endif
#endif /* LPARSE_H_DEFINED */
