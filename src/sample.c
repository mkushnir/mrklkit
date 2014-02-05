#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/sample.h>

#include "diag.h"

int
sample_remove_undef(bytes_t *key,
             lkit_expr_t *value,
             UNUSED void *udata)
{
    lkit_type_t *ty = lkit_type_of_expr(value);

    if (ty->tag == LKIT_UNDEF) {
        char *name = (value->name != NULL) ? (char *)value->name->data : ")(";
        char *kname = (key != NULL) ? (char *)key->data : "][";

        TRACE("undef: %s/%s", kname, name);

        if (strcmp(name, "if") == 0) {
            lkit_expr_t **cond, **texpr, **fexpr;

            if ((cond = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *cond, udata);

            if (lkit_type_of_expr(*cond)->tag != LKIT_BOOL) {
                (*cond)->error = 1;
                lkit_expr_dump(*cond);
                TRRET(SAMPLE_REMOVE_UNDEF + 1);
            }

            if ((texpr = array_get(&value->subs, 1)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *texpr, udata);

            if ((fexpr = array_get(&value->subs, 2)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *fexpr, udata);

            if (lkit_type_cmp((*texpr)->type, (*fexpr)->type) == 0) {
                value->type = (*texpr)->type;
            } else {
                (*texpr)->error = 1;
                (*fexpr)->error = 1;
                lkit_expr_dump((*texpr));
                lkit_expr_dump((*fexpr));
                TRRET(SAMPLE_REMOVE_UNDEF + 2);
            }

        } else if (strcmp(name, "print") == 0) {
            lkit_expr_t **arg;

            if ((arg = array_get(&value->subs, 0)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *arg, udata);
            value->type = (*arg)->type;

        } else if (strcmp(name, "+") == 0 ||
                   strcmp(name, "*") == 0) {

            lkit_expr_t **aexpr, **bexpr;
            array_iter_t it;

            if ((aexpr = array_first(&value->subs, &it)) == NULL) {
                FAIL("array_get");
            }
            sample_remove_undef(NULL, *aexpr, udata);

            for (bexpr = array_next(&value->subs, &it);
                 bexpr != NULL;
                 bexpr = array_next(&value->subs, &it)) {

                sample_remove_undef(NULL, *bexpr, udata);

                if (lkit_type_cmp((*aexpr)->type, (*bexpr)->type) != 0) {
                    (*bexpr)->error = 1;
                    lkit_expr_dump((*aexpr));
                    lkit_expr_dump((*bexpr));
                    TRRET(SAMPLE_REMOVE_UNDEF + 3);
                }
            }
            value->type = (*aexpr)->type;


        } else {
        }
    }
    return 0;
}


