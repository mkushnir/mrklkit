#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/builtin.h>
#include <mrklkit/util.h>

#include "builtin_private.h"

#include "diag.h"

static lkit_expr_t root;

/*
 * libc decls:
 *  (sym printf (func int str ...))
 *  (sym strcmp (func int str str))
 */

/*
 * (sym , (func undef undef ...)) done
 * (sym print (func undef undef ...)) done
 *
 * (sym if (func undef bool undef undef)) done
 *
 * (sym + (func undef undef ...)) done
 * (sym - (func undef undef ...)) done
 * (sym / (func undef undef ...)) done
 * (sym * (func undef undef ...)) done
 * (sym % (func undef undef ...)) done
 * (sym min (func undef undef ...)) done
 * (sym max (func undef undef ...)) done
 *
 * (sym and (func bool bool ...)) done
 * (sym or (func bool bool ...)) done
 * (sym not (func bool bool)) done
 *
 * (sym == (func bool undef undef)) done
 * (sym != (func bool undef undef)) done
 * (sym < (func bool undef undef)) done
 * (sym <= (func bool undef undef)) done
 * (sym > (func bool undef undef)) done
 * (sym >= (func bool undef undef)) done
 *
 * (sym get (func undef undef undef undef)) done
 * (sym set (func undef undef undef)) done
 * (sym del (func undef undef undef)) done
 *
 * (sym itof (func float int)) done
 * (sym ftoi (func int float)) done
 * (sym tostr (func str undef))
 *
 * (sym map (func undef undef undef))
 * (sym reduce (func undef undef undef))
 */

int
builtin_sym_parse(array_t *form, array_iter_t *it)
{
    return lkit_parse_exprdef(&root, form, it);
}


int
builtin_remove_undef(lkit_expr_t *expr)
{
    char *name = (expr->name != NULL) ? (char *)expr->name->data : ")(";

    //TRACEN("%s: ", name);
    if (expr->type->tag == LKIT_UNDEF) {

        //TRACEC("undef\n");

        if (strcmp(name, "if") == 0) {
            lkit_expr_t **cond, **texp, **fexp;
            lkit_type_t *tty, *fty;

            //expr->isbuiltin = 1;

            /*
             * (sym if (func undef bool undef undef))
             */
            cond = array_get(&expr->subs, 0);
            assert(cond != NULL);

            if (builtin_remove_undef(*cond) != 0) {
                TRRET(REMOVE_UNDEF + 1);
            }

            if ((*cond)->type->tag != LKIT_BOOL) {
                (*cond)->error = 1;
                //lkit_expr_dump(*cond);
                TRRET(REMOVE_UNDEF + 2);
            }

            texp = array_get(&expr->subs, 1);
            assert(texp != NULL);

            if (builtin_remove_undef(*texp) != 0) {
                TRRET(REMOVE_UNDEF + 3);
            }

            fexp = array_get(&expr->subs, 2);
            assert(fexp != NULL);

            if (builtin_remove_undef(*fexp) != 0) {
                TRRET(REMOVE_UNDEF + 4);
            }

            tty = (*texp)->type;
            fty = (*fexp)->type;

            if (lkit_type_cmp(tty, fty) == 0) {
                expr->type = tty;
            } else {
                (*texp)->error = 1;
                (*fexp)->error = 1;
                //lkit_expr_dump((*texp));
                //lkit_expr_dump((*fexp));
                TRRET(REMOVE_UNDEF + 5);
            }

        } else if (strcmp(name, ",") == 0) {
            lkit_expr_t **arg;
            array_iter_t it;

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                if (builtin_remove_undef(*arg) != 0) {
                    TRRET(REMOVE_UNDEF + 6);
                }
            }
            if ((arg = array_last(&expr->subs, &it)) == NULL) {
                TRRET(REMOVE_UNDEF + 7);
            }

            expr->type = (*arg)->type;

        } else if (strcmp(name, "print") == 0) {
            lkit_expr_t **arg;
            array_iter_t it;

            //expr->isbuiltin = 1;

            /*
             * (sym print (func undef ...))
             */

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {
                if (builtin_remove_undef(*arg) != 0) {
                    TRRET(REMOVE_UNDEF + 8);
                }
            }

            arg = array_last(&expr->subs, &it);
            assert(arg != NULL);
            expr->type = (*arg)->type;

        } else if (strcmp(name, "+") == 0 ||
                   strcmp(name, "*") == 0 ||
                   strcmp(name, "%") == 0 ||
                   strcmp(name, "-") == 0 ||
                   strcmp(name, "/") == 0 ||
                   strcmp(name, "min") == 0 ||
                   strcmp(name, "max") == 0) {

            lkit_expr_t **aarg, **barg;
            array_iter_t it;

            //expr->isbuiltin = 1;

            /*
             * (sym +|*|%|-|/ (func undef undef ...))
             */
            aarg = array_first(&expr->subs, &it);
            assert(aarg != NULL);

            if (builtin_remove_undef(*aarg) != 0) {
                TRRET(REMOVE_UNDEF + 9);
            }

            for (barg = array_next(&expr->subs, &it);
                 barg != NULL;
                 barg = array_next(&expr->subs, &it)) {

                if (builtin_remove_undef(*barg) != 0) {
                    TRRET(REMOVE_UNDEF + 10);
                }

                if (lkit_type_cmp((*aarg)->type, (*barg)->type) != 0) {
                    (*barg)->error = 1;
                    //lkit_expr_dump((*aarg));
                    //lkit_expr_dump((*barg));
                    TRRET(REMOVE_UNDEF + 11);
                }
            }
            expr->type = (*aarg)->type;

        } else if (strcmp(name, "get") == 0) {
            /*
             * (sym get (func undef struct conststr undef))
             * (sym get (func undef dict conststr undef))
             * (sym get (func undef array constint undef))
             */
            lkit_expr_t **cont;
            lkit_type_t *ty;

            //expr->isbuiltin = 1;

            cont = array_get(&expr->subs, 0);
            assert(cont != NULL);

            if (builtin_remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 12);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_struct_t *ts = (lkit_struct_t *)ty;
                    lkit_expr_t **name;
                    bytes_t *bname;
                    lkit_type_t *elty;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 13);
                    }

                    bname = (bytes_t *)(*name)->value.literal->body;
                    //TRACE("bname=%s ts=%p", bname->data, ts);
                    if ((elty =
                            lkit_struct_get_field_type(ts, bname)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 14);
                    }

                    expr->type = elty;
                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *td = (lkit_dict_t *)ty;
                    lkit_type_t *elty;
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 15);
                    }

                    if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 16);
                    }

                    expr->type = elty;
                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_array_t *ta = (lkit_array_t *)ty;
                    lkit_type_t *elty;
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant int */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 17);
                    }

                    if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 18);
                    }

                    expr->type = elty;
                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 19);
            }

            if (ty->tag != LKIT_STRUCT) {
                lkit_expr_t **dflt;

                dflt = array_get(&expr->subs, 2);
                assert(dflt != NULL);

                if (builtin_remove_undef(*dflt) != 0) {
                    TRRET(REMOVE_UNDEF + 20);
                }

                if (lkit_type_cmp(expr->type, (*dflt)->type) != 0) {
                    (*dflt)->error = 1;
                    TRRET(REMOVE_UNDEF + 21);
                }
            }

        } else if (strcmp(name, "set") == 0) {
            /*
             * (sym set (func undef struct conststr undef))
             * (sym set (func undef dict conststr undef))
             * (sym set (func undef array constint undef))
             */
            lkit_expr_t **cont, **setv;
            lkit_type_t *ty;
            lkit_type_t *elty;

            //expr->isbuiltin = 1;

            cont = array_get(&expr->subs, 0);
            assert(cont != NULL);

            if (builtin_remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 22);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_struct_t *ts = (lkit_struct_t *)ty;
                    lkit_expr_t **name;
                    bytes_t *bname;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 23);
                    }

                    bname = (bytes_t *)(*name)->value.literal->body;
                    if ((elty =
                            lkit_struct_get_field_type(ts, bname)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 24);
                    }

                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *td = (lkit_dict_t *)ty;
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 25);
                    }

                    if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 26);
                    }

                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_array_t *ta = (lkit_array_t *)ty;
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 27);
                    }

                    if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                        (*cont)->error = 1;
                        TRRET(REMOVE_UNDEF + 28);
                    }

                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 29);

            }

            expr->type = ty;

            setv = array_get(&expr->subs, 2);
            assert(setv != NULL);

            if (builtin_remove_undef(*setv) != 0) {
                TRRET(REMOVE_UNDEF + 30);
            }

            if (lkit_type_cmp((*setv)->type, elty) != 0) {
                (*setv)->error = 1;
                TRRET(REMOVE_UNDEF + 31);
            }

        } else if (strcmp(name, "del") == 0) {
            /*
             * (sym del (func undef struct conststr))
             * (sym del (func undef dict conststr))
             * (sym del (func undef array constint))
             */
            lkit_expr_t **cont;
            lkit_type_t *ty;

            //expr->isbuiltin = 1;

            cont = array_get(&expr->subs, 0);
            assert(cont != NULL);

            if (builtin_remove_undef(*cont) != 0) {
                TRRET(REMOVE_UNDEF + 32);
            }

            ty = (*cont)->type;

            switch (ty->tag) {
            case LKIT_STRUCT:
                {
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 33);
                    }
                }
                break;

            case LKIT_DICT:
                {
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_STR ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 34);
                    }
                }
                break;

            case LKIT_ARRAY:
                {
                    lkit_expr_t **name;

                    name = array_get(&expr->subs, 1);
                    assert(name != NULL);

                    /* constant str */
                    if ((*name)->type->tag != LKIT_INT ||
                        !LKIT_EXPR_CONSTANT(*name)) {

                        (*name)->error = 1;
                        TRRET(REMOVE_UNDEF + 35);
                    }
                }
                break;

            default:
                (*cont)->error = 1;
                TRRET(REMOVE_UNDEF + 36);

            }

            expr->type = ty;

        } else {
            lkit_expr_t *ref;

            /* not builtin */
            if ((ref = dict_get_item(&root.ctx, expr->name)) == NULL) {
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 37);
            }
            if (builtin_remove_undef(ref) != 0) {
                TRRET(REMOVE_UNDEF + 38);
            }
            expr->type = ref->type;
        }

    } else {
        if (strcmp(name, "==") == 0 ||
                   strcmp(name, "!=") == 0 ||
                   strcmp(name, "<") == 0 ||
                   strcmp(name, "<=") == 0 ||
                   strcmp(name, ">") == 0 ||
                   strcmp(name, ">=") == 0) {

            lkit_expr_t **arg;
            array_iter_t it;

            //expr->isbuiltin = 1;

            for (arg = array_first(&expr->subs, &it);
                 arg != NULL;
                 arg = array_next(&expr->subs, &it)) {

                if (builtin_remove_undef(*arg) != 0) {
                    TRRET(REMOVE_UNDEF + 39);
                }
            }

        } else if (strcmp(name, "tostr") == 0) {
            /*
             * (sym tostr (func str undef))
             */
            lkit_expr_t **arg;

            //expr->isbuiltin = 1;

            arg = array_get(&expr->subs, 0);
            assert(arg != NULL);

            if (builtin_remove_undef(*arg) != 0) {
                TRRET(REMOVE_UNDEF + 40);
            }

        } else {
            //TRACEC("\n");
            //lkit_expr_dump(expr);
            //TRACE("........");
        }

    }
    if (expr->type->tag == LKIT_UNDEF) {
        /* should not be reached */
        lkit_type_dump(expr->type);
        lkit_expr_dump(expr);
        assert(0);
    }
    return 0;
}


static int
_acb(lkit_gitem_t **gitem, void *udata)
{
    int (*cb)(lkit_expr_t *) = udata;
    return cb((*gitem)->expr);
}


int
builtin_sym_compile(LLVMModuleRef module)
{
    //array_traverse(&root.glist, (array_traverser_t)_acb, lkit_expr_dump);
    if (array_traverse(&root.glist,
                       (array_traverser_t)_acb,
                       builtin_remove_undef) != 0) {
        TRRET(BUILTIN_SYM_COMPILE + 1);
    }

    if (array_traverse(&root.glist,
                       (array_traverser_t)builtin_compile,
                       module) != 0) {
        TRRET(BUILTIN_SYM_COMPILE + 2);
    }
    return 0;
}

int
builtin_call_eager_initializers(LLVMModuleRef module, LLVMBuilderRef builder)
{
    struct {
        LLVMModuleRef module;
        LLVMBuilderRef builder;
    } params = {module, builder};
    if (array_traverse(&root.glist,
                       (array_traverser_t)builtingen_call_eager_initializer,
                       &params) != 0) {
        TRRET(BUILTIN_SYM_COMPILE + 2);
    }
    return 0;
}


lkit_expr_t *
builtin_get_root_ctx(void)
{
    return &root;
}

void
builtin_init(void)
{
    lkit_expr_init(&root, NULL);
}

void
builtin_fini(void)
{
    lkit_expr_fini(&root);
}

