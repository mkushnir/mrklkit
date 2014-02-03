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

typedef struct _defvar {
    unsigned char *name;
    mrklkit_expr_t value;
} defvar_t;

typedef struct _defquery {
    uint64_t id;
} defquery_t;


int mrklkit_parse(int);
int mrklkit_compile(int);
void mrklkit_init_module(void);
void mrklkit_fini_module(void);

#ifdef __cplusplus
}
#endif
#endif /* MRKLKIT_H_DEFINED */
