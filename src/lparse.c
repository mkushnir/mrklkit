#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/array.h>
#include <fparser.h>

#include "lparse.h"

#include "diag.h"

int
lparse_first_word(array_t *form, array_iter_t *it, unsigned char **value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_first(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_WORD) {
        bytes_t *v = (bytes_t *)((*tok)->body);
        *value = (unsigned char *)(v->data);
        return 0;
    }
    *value = NULL;
    (*tok)->error = seterror;
    return 1;
}

int
lparse_next_word(array_t *form, array_iter_t *it, unsigned char **value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_WORD) {
        bytes_t *v = (bytes_t *)((*tok)->body);
        *value = (unsigned char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*tok)->error = seterror;
    return 1;
}

int
lparse_next_str(array_t *form, array_iter_t *it, unsigned char **value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_next(form, it)) == NULL) {
        *value = NULL;
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_STR) {
        bytes_t *v = (bytes_t *)((*tok)->body);
        *value = (unsigned char *)(v->data);
        return 0;
    }
    (void)array_prev(form, it);
    *value = NULL;
    (*tok)->error = seterror;
    return 1;
}

int
lparse_first_int(array_t *form, array_iter_t *it, int64_t *value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_first(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_INT) {
        *value = *((int64_t *)((*tok)->body));
        return 0;
    }
    (*tok)->error = seterror;
    return 1;
}

int
lparse_next_int(array_t *form, array_iter_t *it, int64_t *value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_INT) {
        *value = *((int64_t *)((*tok)->body));
        return 0;
    }
    (void)array_prev(form, it);
    (*tok)->error = seterror;
    return 1;
}

int
lparse_first_double(array_t *form, array_iter_t *it, double *value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_first(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_FLOAT) {
        *value = *((double *)((*tok)->body));
        return 0;
    }
    (*tok)->error = seterror;
    return 1;
}

int
lparse_next_double(array_t *form, array_iter_t *it, double *value, int seterror)
{
    fparser_datum_t **tok;
    fparser_tag_t tag;
    if ((tok = array_next(form, it)) == NULL) {
        return 1;
    }
    tag = FPARSER_DATUM_TAG(*tok);
    if (tag == FPARSER_FLOAT) {
        *value = *((double *)((*tok)->body));
        return 0;
    }
    (void)array_prev(form, it);
    (*tok)->error = seterror;
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
        unsigned char *qual = NULL;

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

