#ifndef LPARSE_H_DEFINED
#define LPARSE_H_DEFINED

#include <mrkcommon/array.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* quals_parser_t)(array_t *,
                               array_iter_t *,
                               unsigned char *,
                               void *);

int lparse_first_word(array_t *, array_iter_t *, unsigned char **, int);
int lparse_next_word(array_t *, array_iter_t *, unsigned char **, int);
int lparse_next_str(array_t *, array_iter_t *, unsigned char **, int);
int lparse_first_int(array_t *, array_iter_t *, int64_t *, int);
int lparse_next_int(array_t *, array_iter_t *, int64_t *, int);
int lparse_first_double(array_t *, array_iter_t *, double *, int);
int lparse_next_double(array_t *, array_iter_t *, double *, int);
int lparse_quals(array_t *,
                 array_iter_t *,
                 quals_parser_t,
                 void *);
#ifdef __cplusplus
}
#endif
#endif /* LPARSE_H_DEFINED */
