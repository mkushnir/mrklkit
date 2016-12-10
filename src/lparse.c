#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/dumpm.h>
#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(lparse);
#endif

int
lparse_first_word(mnarray_t *form,
                  mnarray_iter_t *it,
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
        mnbytes_t *v = (mnbytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_first_word_bytes(mnarray_t *form,
                        mnarray_iter_t *it,
                        mnbytes_t **value,
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
        *value = (mnbytes_t *)((*node)->body);
        return 0;
    }
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_word(mnarray_t *form,
                 mnarray_iter_t *it,
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
        mnbytes_t *v = (mnbytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_word_datum(mnarray_t *form,
                       mnarray_iter_t *it,
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
lparse_next_word_bytes(mnarray_t *form,
                       mnarray_iter_t *it,
                       mnbytes_t **value,
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
        *value = (mnbytes_t *)((*node)->body);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}


int
lparse_next_alnum_bytes(mnarray_t *form,
                        mnarray_iter_t *it,
                        mnbytes_t **value,
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
        *value = (mnbytes_t *)((*node)->body);
        return 0;
    } else if (tag == FPARSER_INT) {
        int64_t *v;
        char buf[32];

        v = (int64_t *)((*node)->body);
        snprintf(buf, sizeof(buf), "%ld", *v);
        fparser_datum_destroy(node);
        *node = fparser_datum_build_str(buf);
        *value = (mnbytes_t *)((*node)->body);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}


int
lparse_next_str(mnarray_t *form,
                mnarray_iter_t *it,
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
        mnbytes_t *v = (mnbytes_t *)((*node)->body);
        *value = (char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}


int
lparse_next_char(mnarray_t *form,
                 mnarray_iter_t *it,
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
        mnbytes_t *v = (mnbytes_t *)((*node)->body);
        *value = ((char *)(v->data))[0];
        return 0;
    }
    (void)array_prev(form, it);
    *value = '\0';
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_str_bytes(mnarray_t *form,
                      mnarray_iter_t *it,
                      mnbytes_t **value,
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
        mnbytes_t *v = (mnbytes_t *)((*node)->body);
        *value = v;
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_str_datum(mnarray_t *form,
                      mnarray_iter_t *it,
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
lparse_first_int(mnarray_t *form,
                 mnarray_iter_t *it,
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
lparse_next_int(mnarray_t *form,
                mnarray_iter_t *it,
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
lparse_first_double(mnarray_t *form,
                    mnarray_iter_t *it,
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
lparse_next_double(mnarray_t *form,
                   mnarray_iter_t *it,
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
lparse_next_bool(mnarray_t *form,
                 mnarray_iter_t *it,
                 char *value,
                 int seterror)
{
    fparser_datum_t **node;
    fparser_tag_t tag;
    if ((node = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*node);
    if (tag == FPARSER_BOOL) {
        char *v = (char *)((*node)->body);
        *value = *v;
        return 0;
    }
    (void)array_prev(form, it);
    (*node)->error = seterror;
    return 1;
}

int
lparse_next_sequence(mnarray_t *form,
                     mnarray_iter_t *it,
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
lparse_quals(mnarray_t *form,
             mnarray_iter_t *it,
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

