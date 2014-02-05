#ifndef LEXPR_H_DEFINED
#define LEXPR_H_DEFINED

#include <mrkcommon/array.h>
#include <mrkcommon/dict.h>

#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lkit_expr {
    lkit_type_t *type;
    union {
        fparser_datum_t *literal;
        struct _lkit_expr *ref;
    } value;
    int isref:1;
    /* lkit_expr_t * */
    array_t subs;
    /* bytes_t *, lkit_expr_t * */
    dict_t ctx;
    struct _lkit_expr *parent;

} lkit_expr_t;

lkit_expr_t *lkit_expr_parse(lkit_expr_t *, fparser_datum_t *, int);
lkit_expr_t *lkit_expr_find(lkit_expr_t *, bytes_t *);
int lkit_parse_exprdef(lkit_expr_t *, array_t *, array_iter_t *);
int lexpr_parse(array_t *, array_iter_t *);

void lexpr_init(void);
void lexpr_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LEXPR_H_DEFINED */
