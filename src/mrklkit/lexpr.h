#ifndef LEXPR_H_DEFINED
#define LEXPR_H_DEFINED

#include <mrkcommon/array.h>

#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lkit_expr {
    bytes_t *name;
    lkit_type_t *type;
    union {
        fparser_datum_t *literal;
        struct _lkit_expr *ref;
    } value;
    int isref:1;
    /* lkit_expr_t * */
    array_t subs;

} lkit_expr_t;

int lkit_expr_parse(array_t *, array_iter_t *);

void lexpr_init(void);
void lexpr_fini(void);

#ifdef __cplusplus
}
#endif
#endif /* LEXPR_H_DEFINED */
