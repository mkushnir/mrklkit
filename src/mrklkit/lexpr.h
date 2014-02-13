#ifndef LEXPR_H_DEFINED
#define LEXPR_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lkit_user_class {
} lkit_user_class_t;

typedef struct _lkit_expr {
    lkit_type_t *type;
    bytes_t *name;
    union {
        fparser_datum_t *literal;
        struct _lkit_expr *ref;
    } value;
    /* lkit_expr_t * */
    array_t subs;

    /* bytes_t *, lkit_expr_t * */
    dict_t ctx;
    /* lkit_gitem_t */
    array_t glist;
    struct _lkit_expr *parent;
    lkit_user_class_t *uclass;

    int isref:1;
    int error:1;
    //int isbuiltin:1;

} lkit_expr_t;

#define LKIT_EXPR_CONSTANT(expr) ((!(expr)->isref) && ((expr)->value.literal != NULL))

typedef struct _lkit_gitem {
    /* weakref */
    bytes_t *name;
    /* weakref */
    lkit_expr_t *expr;
} lkit_gitem_t;

void lkit_expr_dump(lkit_expr_t *);
lkit_expr_t *lkit_expr_parse(lkit_expr_t *, fparser_datum_t *, int);
lkit_expr_t *lkit_expr_find(lkit_expr_t *, bytes_t *);
int lkit_parse_exprdef(lkit_expr_t *, array_t *, array_iter_t *);

void lexpr_init_ctx(lkit_expr_t *);
void lexpr_fini_ctx(lkit_expr_t *);

void lexpr_init(void);
void lexpr_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LEXPR_H_DEFINED */
