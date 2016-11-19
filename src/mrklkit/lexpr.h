#ifndef LEXPR_H_DEFINED
#define LEXPR_H_DEFINED

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/hash.h>

#include <mrklkit/fparser.h>
#include <mrklkit/mrklkit.h>
#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _lkit_type;
struct _lkit_expr;

typedef struct _lkit_expr {
    struct _lkit_type *type;
    /* weakref */
    bytes_t *name;
    /* weakref */
    bytes_t *title;
    union {
        fparser_datum_t *literal;   /* !isref */
        struct _lkit_expr *ref;     /*  isref */
    } value;

    /*
     * strongref lkit_expr_t *
     */
    array_t subs;

    /* applies if (fparam == 1) */
    char fparam_idx;

    int referenced;
    lkit_mpolicy_t mpolicy;
    /* fix in lexpr_dump_flags() */
    int isctx:1;
    int error:1;
    int isref:1;
    int isbuiltin:1;
    int ismacro:1;
    int lazy_init:1;
    int undef_removed:1;
    int fparam:1;
    int zref:1;
} lkit_expr_t;


/*
 * a named scope-visible item
 */
typedef struct _lkit_gitem {
    /* strongref */
    bytes_t *name;
    /* weakref */
    struct _lkit_expr *expr;
} lkit_gitem_t;


typedef struct _lkit_cexpr {
    struct _lkit_expr base;
    /* weekref */
    struct _lkit_cexpr *parent;

    /*
     * glist defines order of ctx's items
     * lkit_gitem_t strongref
     */
    array_t glist;
    /*
     * weakref bytes_t *, weakref lkit_expr_t *
     */
    hash_t ctx;
} lkit_cexpr_t;

#define LKIT_EXPR_PARSE_FIXTURE_TRYAGAIN (-2)
typedef int (*lkit_expr_parse_fixture_t)(mrklkit_ctx_t *,
                                         lkit_cexpr_t *,
                                         fparser_datum_t *,
                                         void *,
                                         int);

#define LKIT_EXPR_NAME(expr) ((expr)->name != NULL ? (expr)->name->data : NULL)


#define LKIT_EXPR_CONSTANT(expr) (                             \
        (!(expr)->isref) && ((expr)->value.literal != NULL))   \


#define LKIT_EXPR_FLAG_WITH_REF(e, f) (                                        \
    (e)->f || ((e)->isref && (e)->value.ref != NULL && (e)->value.ref->f)      \
)                                                                              \


int lkit_expr_dump(lkit_expr_t *);
int lkit_expr_promote_policy(mrklkit_ctx_t *, lkit_expr_t *, lkit_mpolicy_t);
lkit_expr_t *lkit_expr_parse(mrklkit_ctx_t *,
                             lkit_cexpr_t *,
                             fparser_datum_t *,
                             int);
int lkit_expr_parse2(mrklkit_ctx_t *,
                     lkit_cexpr_t *,
                     fparser_datum_t *,
                     int,
                     lkit_expr_t **,
                     lkit_expr_parse_fixture_t,
                     void *);
lkit_expr_t *lkit_expr_find(lkit_cexpr_t *, bytes_t *);
void lkit_expr_set_referenced(lkit_expr_t *);
int lkit_parse_exprdef(lkit_expr_t *,
                       array_t *,
                       array_iter_t *,
                       void *);

void lexpr_init_ctx(lkit_cexpr_t *);
void lexpr_add_to_ctx(lkit_cexpr_t *, bytes_t *, lkit_expr_t *);

lkit_expr_t *lkit_expr_new(void);
lkit_cexpr_t *lkit_cexpr_new(lkit_cexpr_t *);
lkit_expr_t *lkit_expr_build_literal(mrklkit_ctx_t *,
                                     fparser_datum_t *);
lkit_expr_t *lkit_expr_build_ref(lkit_cexpr_t *, bytes_t *);
lkit_expr_t *lkit_expr_add_sub(lkit_expr_t *, lkit_expr_t *);
struct _lkit_type *lkit_expr_type_of(lkit_expr_t *);
void lkit_expr_init(lkit_expr_t *);
void lkit_expr_fini(lkit_expr_t *);
int lkit_expr_destroy(lkit_expr_t **);
void lkit_cexpr_init(lkit_cexpr_t *, lkit_cexpr_t *);
void lkit_cexpr_fini(lkit_cexpr_t *);
bytes_t *lkit_cexpr_qual_name(lkit_cexpr_t *, bytes_t *);
int lkit_expr_is_constant(lkit_expr_t *);
void lexpr_init(void);
void lexpr_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LEXPR_H_DEFINED */
