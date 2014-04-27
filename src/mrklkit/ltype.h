#ifndef LTYPE_H_DEFINED
#define LTYPE_H_DEFINED

#include <stdint.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _lkit_tag {
    LKIT_UNDEF,
    LKIT_VOID,
    LKIT_INT,
    LKIT_STR,
    LKIT_FLOAT,
    LKIT_BOOL,
    LKIT_ANY,
    LKIT_VARARG,
    _LKIT_END_OF_BUILTIN_TYPES,
    /* custom types */
    LKIT_ARRAY,
    LKIT_DICT,
    LKIT_STRUCT,
    LKIT_FUNC,
} lkit_tag_t;

#define LKIT_TAG_STR(tag) ( \
    (tag) == LKIT_VOID ? "VOID" : \
    (tag) == LKIT_INT ? "INT" : \
    (tag) == LKIT_STR ? "STR" : \
    (tag) == LKIT_FLOAT ? "FLOAT" : \
    (tag) == LKIT_BOOL ? "BOOL" : \
    (tag) == LKIT_ANY ? "ANY" : \
    (tag) == LKIT_VARARG ? "VARARG" : \
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

struct _lkit_type;

typedef void (*lkit_type_dtor_t)(void **);
typedef struct _lkit_type {
    lkit_tag_t tag;
    /* weak ref */
    char *name;
    uint64_t hash;
    /* run time size, or 0 if not known at compile time */
    size_t rtsz;
    LLVMTypeRef backend;
    int error:1;
    lkit_type_dtor_t dtor;
} lkit_type_t;

#define LKIT_ERROR(pty) (((lkit_type_t *)(pty))->error)

typedef struct _lkit_void {
    struct _lkit_type base;
} lkit_void_t;

typedef struct _lkit_int {
    struct _lkit_type base;
} lkit_int_t;

typedef struct _lkit_float {
    struct _lkit_type base;
} lkit_float_t;

typedef struct _lkit_bool {
    struct _lkit_type base;
} lkit_bool_t;

typedef struct _lkit_any {
    struct _lkit_type base;
} lkit_any_t;

typedef struct _lkit_vararg {
    struct _lkit_type base;
} lkit_vararg_t;

typedef struct _lkit_str {
    struct _lkit_type base;
} lkit_str_t;

typedef struct _lkit_array {
    struct _lkit_type base;
    lkit_parser_t parser;
    /* weak ref, will use delim[0] */
    unsigned char *delim;
    /* lkit_type_t * */
    array_t fields;
} lkit_array_t;

typedef struct _lkit_dict {
    struct _lkit_type base;
    /* weak ref, will use kvdelim[0] */
    unsigned char *kvdelim;
    /* weak ref, will use fdelim[0] */
    unsigned char *fdelim;
    /* lkit_type_t * */
    array_t fields;
} lkit_dict_t;

typedef struct _lkit_struct {
    struct _lkit_type base;
    LLVMTypeRef deref_backend;
    lkit_parser_t parser;
    /* weak ref, will use delim[0] */
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

typedef struct _lkit_typedef {
    /* weakref */
    unsigned char *name;
    lkit_type_t *type;
} lkit_typedef_t;

int ltype_next_struct(array_t *, array_iter_t *, lkit_struct_t **, int);
typedef int (*lkit_type_traverser_t)(lkit_type_t *, void *);
int lkit_type_traverse(lkit_type_t *, lkit_type_traverser_t, void *);
int ltype_transform(dict_traverser_t, void *);

void lkit_type_dump(lkit_type_t *);
void lkit_type_str(lkit_type_t *, bytestream_t *);
int lkit_type_destroy(lkit_type_t **);
int lkit_type_fini_dict(lkit_type_t *, lkit_type_t *);
lkit_type_t *lkit_type_get(lkit_tag_t);
lkit_type_t *lkit_type_parse(fparser_datum_t *, int);
lkit_type_t *lkit_type_finalize(lkit_type_t *);
int lkit_parse_typedef(array_t *, array_iter_t *);
uint64_t lkit_type_hash(lkit_type_t *);
int lkit_type_cmp(lkit_type_t *, lkit_type_t *);
lkit_type_t *lkit_array_get_element_type(lkit_array_t *);
lkit_type_t *lkit_dict_get_element_type(lkit_dict_t *);
lkit_type_t *lkit_struct_get_field_type(lkit_struct_t *, bytes_t *);
int lkit_struct_get_field_index(lkit_struct_t *, bytes_t *);

void ltype_init(void);
void ltype_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LTYPE_H_DEFINED */
