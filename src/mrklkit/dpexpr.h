#ifndef DPEXPR_H
#define DPEXPR_H

#include <mrklkit/ltype.h>
#include <mrklkit/mrklkit.h>
#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _lkit_dpkind {
    LKIT_PARSER_NONE,
    LKIT_PARSER_DELIM, /* normal single */
    LKIT_PARSER_MDELIM, /* normal multiple */
    LKIT_PARSER_SMARTDELIM, /* smart for dict/arrays */
    LKIT_PARSER_OPTQSTRDELIM, /* opt-qstr for dict/arrays */
    LKIT_PARSER_QSTR,
    LKIT_PARSER_OPTQSTR,
} lkit_dpkind_t;


#define LKIT_PARSER_STR(p) (                           \
    (p) == LKIT_PARSER_NONE ? "NONE" :                 \
    (p) == LKIT_PARSER_DELIM ? "DELIM" :               \
    (p) == LKIT_PARSER_MDELIM ? "MDELIM" :             \
    (p) == LKIT_PARSER_SMARTDELIM ? "SMARDELIM" :      \
    (p) == LKIT_PARSER_QSTR ? "QSTR" :                 \
    (p) == LKIT_PARSER_OPTQSTR ? "OPTQSTR" :           \
    "<unknown>"                                        \
                                                       \
)                                                      \


typedef struct _lkit_dpexpr {
    lkit_parser_t *ty;
    lkit_dpkind_t parser;
} lkit_dpexpr_t;

typedef struct _lkit_dpint {
    struct _lkit_dpexpr base;
} lkit_dpint_t;

typedef struct _lkit_dpfloat {
    struct _lkit_dpexpr base;
} lkit_dpfloat_t;

typedef struct _lkit_dpbool {
    struct _lkit_dpexpr base;
} lkit_dpbool_t;

typedef struct _lkit_dpstr {
    struct _lkit_dpexpr base;
} lkit_dpstr_t;

typedef struct _lkit_dparray {
    struct _lkit_dpexpr base;
    /* strong lkit_dpexpr_t * */
    array_t fields;
    int64_t nreserved;
    char fdelim;
} lkit_dparray_t;

typedef struct _lkit_dpdict {
    struct _lkit_dpexpr base;
    /* strong lkit_dpexpr_t * */
    array_t fields;
    int64_t nreserved;
    char fdelim; /* field delimiter */
    char pdelim; /* pair delimiter */
} lkit_dpdict_t;

typedef struct _lkit_dpstruct {
    struct _lkit_dpexpr base;
    /* strong lkit_dpexpr_t * */
    array_t fields;
    /* weakref byte_t * */
    array_t names;
    char fdelim;
} lkit_dpstruct_t;


lkit_dpexpr_t *lkit_dpstruct_get_field_parser(lkit_dpstruct_t *, int);
lkit_dpexpr_t *lkit_dpdict_get_element_parser(lkit_dpdict_t *);
lkit_dpexpr_t *lkit_dparray_get_element_parser(lkit_dparray_t *);

lkit_dpexpr_t *lkit_dpexpr_find(mrklkit_ctx_t *, bytes_t *, void *);

lkit_dpexpr_t *lkit_dpexpr_parse(mrklkit_ctx_t *,
                                 fparser_datum_t *,
                                 int);
void lkit_dpexpr_destroy(lkit_dpexpr_t **);

#ifdef __cplusplus
}
#endif

#endif

