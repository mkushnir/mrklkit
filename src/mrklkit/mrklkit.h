#ifndef MRKLKIT_H_DEFINED
#define MRKLKIT_H_DEFINED

#include <stdint.h> /* uint64_t */

#include <mrkcommon/array.h>
#include "ltype.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *mrklkit_diag_str(int);

typedef struct _mrklkit_expr {
    lkit_type_t *type;
    array_t quals; /* should be dict for fast lookup */
#define MRKLKIT_EXPR_FLAG_LITERAL 0x01
    unsigned flags;
    union {
        /* XXX ??? */
        struct _mrklkit_expr *ref;
    } value;
    /* mrklkit_expr_t * */
    array_t referrals;
} mrklkit_expr_t;

typedef struct _var {
    unsigned char *name;
    mrklkit_expr_t value;
} var_t;

typedef struct _query {
    uint64_t id;
} query_t;


int mrklkit_compile(int);
int mrklkit_run(const char *);
void mrklkit_init_module(void);
void mrklkit_fini_module(void);

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_H_DEFINED */
