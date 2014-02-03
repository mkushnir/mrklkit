#ifndef LTYPE_H_DEFINED
#define LTYPE_H_DEFINED

#include <mrkcommon/array.h>

#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _lkit_tag {
    LKIT_INT,
    LKIT_TIMESTAMP,
    LKIT_STR,
    LKIT_FLOAT,
    _LKIT_END_OF_BUILTIN_TYPES,
    /* custom types */
    LKIT_ARRAY,
    LKIT_DICT,
    LKIT_STRUCT,
    LKIT_FUNC,
    LKIT_UNDEF,
} lkit_tag_t;

#define LKIT_TAG_STR(tag) ( \
    (tag) == LKIT_INT ? "INT" : \
    (tag) == LKIT_TIMESTAMP ? "TIMESTAMP" : \
    (tag) == LKIT_STR ? "STR" : \
    (tag) == LKIT_FLOAT ? "FLOAT" : \
    (tag) == LKIT_ARRAY ? "ARRAY" : \
    (tag) == LKIT_DICT ? "DICT" : \
    (tag) == LKIT_STRUCT ? "STRUCT" : \
    (tag) == LKIT_FUNC ? "FUNC" : \
    (tag) == LKIT_UNDEF ? "UNDEF" : \
    "<unknown>" \
)

typedef enum _lkit_parser {
    LKIT_PARSER_NONE,
    LKIT_PARSER_DELIM,
    LKIT_PARSER_W3C,
} lkit_parser_t;

typedef struct _lkit_type {
    lkit_tag_t tag;
    /* weak ref */
    char *name;
    int error:1;
} lkit_type_t;

#define LKIT_ERROR(pty) (((lkit_type_t *)(pty))->error)

typedef struct _lkit_int {
    struct _lkit_type base;
} lkit_int_t;

typedef struct _lkit_timestamp {
    struct _lkit_type base;
} lkit_timestamp_t;

typedef struct _lkit_float {
    struct _lkit_type base;
} lkit_float_t;

typedef struct _lkit_str {
    struct _lkit_type base;
} lkit_str_t;

typedef struct _lkit_array {
    struct _lkit_type base;
    lkit_parser_t parser;
    /* weak ref */
    unsigned char *delim;
    /* lkit_type_t * */
    array_t fields;
} lkit_array_t;

typedef struct _lkit_dict {
    struct _lkit_type base;
    /* weak ref */
    unsigned char *kvdelim;
    /* weak ref */
    unsigned char *fdelim;
    /* lkit_type_t * */
    array_t fields;
} lkit_dict_t;

typedef struct _lkit_struct {
    struct _lkit_type base;
    lkit_parser_t parser;
    /* weak ref */
    unsigned char *delim;
    /* lkit_type_t * */
    array_t fields;
    /* weak refs */
    array_t names;
} lkit_struct_t;

typedef struct _lkit_func {
    struct _lkit_type base;
    /* lkit_type_t * */
    array_t fields;
} lkit_func_t;

typedef int (*lkit_type_traverser_t)(lkit_type_t *, void *);
int lkit_type_traverse(lkit_type_t *, lkit_type_traverser_t, void *);
void lkit_type_dump(lkit_type_t *);
int lkit_type_destroy(lkit_type_t **);
int ltype_next_struct(array_t *, array_iter_t *, lkit_struct_t **, int);
lkit_type_t *ltype_parse_type(fparser_datum_t *);

void ltype_init(void);
void ltype_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LTYPE_H_DEFINED */
