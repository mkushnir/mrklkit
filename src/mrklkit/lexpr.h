#ifndef LEXPR_H_DEFINED
#define LEXPR_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/fparser.h>
#include <mrklkit/mrklkit.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _lkit_type;
typedef struct _lkit_expr {
    struct _lkit_type *type;
    bytes_t *name;
    bytes_t *title;
    union {
        fparser_datum_t *literal;   /* !isref */
        struct _lkit_expr *ref;     /*  isref */
    } value;

    /*
     * lkit_expr_t *
     * owned by subs
     */
    array_t subs;

    /*
     * bytes_t *, lkit_expr_t *
     * values owned by ctx (think more about it ...)
     * ctx and subs should never intersect
     */
    dict_t ctx;

    /*
     * lkit_gitem_t
     */
    array_t glist;

    /* weekref */
    struct _lkit_expr *parent;

    int referenced;
    int isref:1;
    int isbuiltin:1;
    int error:1;
    int ismacro:1;
    int lazy_init:1;

} lkit_expr_t;

#define LKIT_EXPR_NAME(expr) ((expr)->name != NULL ? (expr)->name->data : NULL)
#define LKIT_EXPR_CONSTANT(expr) ((!(expr)->isref) && ((expr)->value.literal != NULL))

typedef struct _lkit_gitem {
    /* strongref */
    bytes_t *name;
    /* weakref */
    lkit_expr_t *expr;
} lkit_gitem_t;


int lkit_expr_dump(lkit_expr_t *);
lkit_expr_t *lkit_expr_parse(mrklkit_ctx_t *,
                             lkit_expr_t *,
                             fparser_datum_t *,
                             int);
lkit_expr_t *lkit_expr_find(lkit_expr_t *, bytes_t *);
void lkit_expr_set_referenced(lkit_expr_t *);
int lkit_parse_exprdef(lkit_expr_t *,
                       array_t *,
                       array_iter_t *,
                       void *);

void lexpr_init_ctx(lkit_expr_t *);
void lexpr_add_to_ctx(lkit_expr_t *, bytes_t *, lkit_expr_t *);
void lexpr_fini_ctx(lkit_expr_t *);

lkit_expr_t *lkit_expr_new(lkit_expr_t *);
lkit_expr_t *lkit_expr_build_literal(mrklkit_ctx_t *,
                                     lkit_expr_t *,
                                     fparser_datum_t *);
lkit_expr_t *lkit_expr_build_ref(lkit_expr_t *, bytes_t *);
lkit_expr_t *lkit_expr_add_sub(lkit_expr_t *, lkit_expr_t *);
struct _lkit_type *lkit_expr_type_of(lkit_expr_t *);
void lkit_expr_init(lkit_expr_t *, lkit_expr_t *);
void lkit_expr_fini(lkit_expr_t *);
int lkit_expr_destroy(lkit_expr_t **);
bytes_t *lkit_expr_qual_name(lkit_expr_t *, bytes_t *);
int lkit_expr_is_constant(lkit_expr_t *);
void lexpr_init(void);
void lexpr_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LEXPR_H_DEFINED */
