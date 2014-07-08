#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/dumpm.h>
#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>

#include "diag.h"

int
lparse_first_word(array_t *form,
                  array_iter_t *it,
                  char **value,
                  int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_first(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_WORD) {
        bytes_t *v = (bytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_first_word_bytes(array_t *form,
                        array_iter_t *it,
                        bytes_t **value,
                        int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_first(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_WORD) {
        *value = (bytes_t *)((*node)->body);
        return 0;
    }
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_word(array_t *form,
                 array_iter_t *it,
                 char **value,
                 int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_WORD) {
        bytes_t *v = (bytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_word_datum(array_t *form,
                       array_iter_t *it,
                       fparser_datum_t **value,
                       int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_WORD) {
        *value = *node;
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_word_bytes(array_t *form,
                       array_iter_t *it,
                       bytes_t **value,
                       int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_WORD) {
        *value = (bytes_t *)((*node)->body);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}


int
lparse_next_str(array_t *form,
                array_iter_t *it,
                char **value,
                int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_STR) {
        bytes_t *v = (bytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}


int
lparse_next_char(array_t *form,
                 array_iter_t *it,
                 char *value,
                 int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_STR) {
        bytes_t *v = (bytes_t *)((*node)->body);
        *value = ((char *)(v->data))[0];
        return 0;
    }
    (void)array_prev(form, it);
    *value = '\0';
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_str_bytes(array_t *form,
                      array_iter_t *it,
                      bytes_t **value,
                      int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_STR) {
        bytes_t *v = (bytes_t *)((*node)->body);
        *value = v;
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_str_datum(array_t *form,
                      array_iter_t *it,
                      fparser_datum_t **value,
                      int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_STR) {
        *value = *node;
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_first_int(array_t *form,
                 array_iter_t *it,
                 int64_t *value,
                 int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_first(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_INT) {
        int64_t *v = (int64_t *)((*node)->body);
        *value = *v;
        return 0;
    }
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_int(array_t *form,
                array_iter_t *it,
                int64_t *value,
                int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_INT) {
        int64_t *v = (int64_t *)((*node)->body);
        *value = *v;
        return 0;
    }
    (void)array_prev(form, it);
    (*node)->error = seterror;
    return 1;
}

int
lparse_first_double(array_t *form,
                    array_iter_t *it,
                    double *value,
                    int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_first(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_FLOAT) {
        double *v = (double *)((*node)->body);
        *value = *v;
        return 0;
    }
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_double(array_t *form,
                   array_iter_t *it,
                   double *value,
                   int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_FLOAT) {
        double *v = (double *)((*node)->body);
        *value = *v;
        return 0;
    }
    (void)array_prev(form, it);
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_sequence(array_t *form,
                     array_iter_t *it,
                     fparser_datum_t **value,
                     int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_SEQ) {
        *value = *node;
        return 0;
    }
    (void)array_prev(form, it);
    (*node)->error = seterror;
    return 1;
}

int
lparse_quals(array_t *form,
             array_iter_t *it,
             quals_parser_t cb,
             void *udata)
{
    int res = 0;

    while (1) {
        char *qual = NULL;

        if (lparse_next_word(form, it, &qual, 0) != 0) {
            break;
        }
        if (qual[0] == ':') {
            if (cb(form, it, qual, udata) != 0) {
                res = 1;
                break;
            }
        } else {
            (void)array_prev(form, it);
            break;
        }
    }
    return res;
}

