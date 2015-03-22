#ifndef LTYPE_H_DEFINED
#define LTYPE_H_DEFINED

#include <stdint.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/fparser.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _lkit_tag {
    LKIT_UNDEF,
    LKIT_TY,
    LKIT_VOID,
    LKIT_INT,
    LKIT_INT_MIN,
    LKIT_INT_MAX,
    LKIT_STR,
    LKIT_FLOAT,
    LKIT_FLOAT_MIN,
    LKIT_FLOAT_MAX,
    LKIT_BOOL,
    LKIT_ANY,
    LKIT_VARARG,
    _LKIT_END_OF_BUILTIN_TYPES,
    /* custom types */
    LKIT_ARRAY,
    LKIT_DICT,
    LKIT_STRUCT,
    LKIT_FUNC,
    LKIT_USER = 0x3fffffff,
    /*
     * for all user-defined types:
     *  assert(tag & LKIT_USER)
     */
} lkit_tag_t;

#define LKIT_TAG_STR(tag) (                    \
    (tag) == LKIT_UNDEF ? "UNDEF" :            \
    (tag) == LKIT_TY ? "TY" :                  \
    (tag) == LKIT_VOID ? "VOID" :              \
    (tag) == LKIT_INT ? "INT" :                \
    (tag) == LKIT_INT_MIN ? "INTMIN" :         \
    (tag) == LKIT_INT_MAX ? "INTMAX" :         \
    (tag) == LKIT_STR ? "STR" :                \
    (tag) == LKIT_FLOAT ? "FLOAT" :            \
    (tag) == LKIT_FLOAT_MIN ? "FLOATMIN" :     \
    (tag) == LKIT_FLOAT_MAX ? "FLOATMAX" :     \
    (tag) == LKIT_BOOL ? "BOOL" :              \
    (tag) == LKIT_ANY ? "ANY" :                \
    (tag) == LKIT_VARARG ? "VARARG" :          \
    (tag) == LKIT_ARRAY ? "ARRAY" :            \
    (tag) == LKIT_DICT ? "DICT" :              \
    (tag) == LKIT_STRUCT ? "STRUCT" :          \
    (tag) == LKIT_FUNC ? "FUNC" :              \
    (tag) == LKIT_USER ? "USER" :              \
    "<unknown>"                                \
)

typedef enum _lkit_parser {
    LKIT_PARSER_NONE,
    LKIT_PARSER_DELIM, /* normal single */
    LKIT_PARSER_MDELIM, /* normal multiple */
    LKIT_PARSER_SMARTDELIM, /* smart for dict/arrays */
    LKIT_PARSER_OPTQSTRDELIM, /* opt-qstr for dict/arrays */
    LKIT_PARSER_QSTR,
    LKIT_PARSER_OPTQSTR,
} lkit_parser_t;


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


struct _lkit_type;
struct _lkit_expr;

typedef struct _lkit_type {
    int tag;
    /* weak ref */
    char *name;
    uint64_t hash;
    LLVMTypeRef (*compile)(struct _lkit_type *, LLVMContextRef);
    int (*compile_setup)(struct _lkit_expr *,
                         LLVMModuleRef,
                         LLVMBuilderRef,
                         struct _lkit_expr *,
                         bytes_t *);
    int (*compile_cleanup)(struct _lkit_expr *,
                           LLVMModuleRef,
                           LLVMBuilderRef,
                           struct _lkit_expr *,
                           bytes_t *);
} lkit_type_t;

#define LTYPE_ERROR(pty) (((lkit_type_t *)(pty))->error)

typedef struct _lkit_ty {
    struct _lkit_type base;
} lkit_ty_t;

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
    //array_finalizer_t fini;
    lkit_parser_t parser;
    /* weak ref, will use delim[0] */
    char delim;
    /* lkit_type_t * */
    array_t fields;
    int64_t nreserved;
} lkit_array_t;

typedef struct _lkit_dict {
    struct _lkit_type base;
    //dict_item_finalizer_t fini;
    lkit_parser_t parser;
    /* weak ref, will use kvdelim[0] */
    char kvdelim;
    /* weak ref, will use fdelim[0] */
    char fdelim;
    /* lkit_type_t * */
    array_t fields;
} lkit_dict_t;

typedef struct _lkit_struct {
    struct _lkit_type base;
    void (*init)(void **);
    void (*fini)(void **);
    lkit_parser_t parser;
    /* weak ref, will use delim[0] */
    char delim;
    /* lkit_type_t * */
    array_t fields;
    /* weak refs */
    array_t names;
    array_t parsers;
} lkit_struct_t;

typedef struct _lkit_func {
    struct _lkit_type base;
    /* lkit_type_t * */
    array_t fields;
} lkit_func_t;

typedef int (*lkit_type_traverser_t)(lkit_type_t *, void *);
int lkit_type_traverse(lkit_type_t *, lkit_type_traverser_t, void *);
int lkit_traverse_types(dict_traverser_t, void *);
void lkit_type_dump(lkit_type_t *);
void lkit_type_str(lkit_type_t *, bytestream_t *);
lkit_type_t * lkit_type_new(lkit_tag_t);
int lkit_type_destroy(lkit_type_t **);
int lkit_parse_typedef(mrklkit_ctx_t *,
                       array_t *,
                       array_iter_t *);
lkit_type_t *lkit_type_parse(mrklkit_ctx_t *,
                             fparser_datum_t *,
                             int);
lkit_array_t *lkit_type_get_array(mrklkit_ctx_t *, int);
lkit_dict_t *lkit_type_get_dict(mrklkit_ctx_t *, int);
lkit_type_t *lkit_type_finalize(lkit_type_t *);
void lkit_register_typedef(mrklkit_ctx_t *, lkit_type_t *, bytes_t *);
lkit_type_t *lkit_typedef_get(mrklkit_ctx_t *, bytes_t *);
uint64_t lkit_type_hash(lkit_type_t *);
int lkit_type_cmp(lkit_type_t *, lkit_type_t *);
int lkit_type_cmp_loose(lkit_type_t *, lkit_type_t *);
lkit_type_t *lkit_type_get(mrklkit_ctx_t *, int);
lkit_type_t *lkit_array_get_element_type(lkit_array_t *);
lkit_type_t *lkit_dict_get_element_type(lkit_dict_t *);
lkit_type_t *lkit_struct_get_field_type(lkit_struct_t *, bytes_t *);
int lkit_struct_get_field_index(lkit_struct_t *, bytes_t *);
lkit_type_t *lkit_func_get_arg_type(lkit_func_t *, size_t);

LLVMTypeRef mrklkit_ctx_get_type_backend(mrklkit_ctx_t *, lkit_type_t *);

void ltype_init(void);
void ltype_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LTYPE_H_DEFINED */
