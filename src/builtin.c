#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/module.h>
#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/builtin.h>
#include <mrklkit/util.h>

#include "diag.h"

/*
 * (keyword name value)
 */
int
builtin_parse_exprdef(mrklkit_ctx_t *mctx,
                     lkit_expr_t *ectx,
                     array_t *form,
                     array_iter_t *it,
                     int flags)
{
    int res = 0;
    bytes_t *name = NULL;
    lkit_expr_t **pexpr = NULL;
    fparser_datum_t **node = NULL;

    /* name */
    if (lparse_next_word_bytes(form, it, &name, 1) != 0) {
        TR(LKIT_PARSE_EXPRDEF + 1);
        goto err;
    }

    /* must be unique */
    if (lkit_expr_find(ectx, name) != NULL) {
        TRACE("not unique: " ERRCOLOR("%s"), name->data);
        TR(LKIT_PARSE_EXPRDEF + 2);
        goto err;
    }

    /* type or value ? */
    if ((node = array_next(form, it)) == NULL) {
        TR(LKIT_PARSE_EXPRDEF + 3);
        goto err;
    }

    if ((pexpr = array_incr(&ectx->subs)) == NULL) {
        FAIL("array_incr");
    }

    if ((*pexpr = lkit_expr_parse(mctx,
                                  ectx,
                                  *node,
                                  1)) == NULL) {
        (*node)->error = 1;
        TR(LKIT_PARSE_EXPRDEF + 4);
        goto err;
    }

    if ((node = array_next(form, it)) != NULL) {
        lkit_type_t **pargty;
        array_iter_t it;
        lkit_func_t *ft;
        lkit_expr_t **body;

        /*
         * test if we have function def here
         */
        assert(*node != NULL);
        if ((*pexpr)->isref) {
            /*
             * cannot be ref here
             */
            FAIL("builtin_parse_exprdef");
        }
        if ((*pexpr)->type->tag != LKIT_FUNC) {
            (*node)->error = 1;
            TRACE("only func can expect body in: %s", name->data);
            TR(LKIT_PARSE_EXPRDEF + 5);
            goto err;
        }

        ft = (lkit_func_t *)(*pexpr)->type;

        /*
         * create expressions for function arguments
         */
        /* skip through rettype */
        if ((pargty = array_first(&ft->fields, &it)) == NULL) {
            FAIL("array_first");
        }

        for (pargty = array_next(&ft->fields, &it);
             pargty != NULL;
             pargty = array_next(&ft->fields, &it)) {
            bytes_t **pargname;
            lkit_expr_t **parg;

            if ((pargname = array_get(&ft->names, it.iter)) == NULL) {
                FAIL("array_get");
            }
            if (*pargname == NULL) {
                TRACE("named argument expected for %d in: %s", it.iter - 1, name->data);
                TR(LKIT_PARSE_EXPRDEF + 5);
                goto err;
            }
            if ((parg = array_incr(&(*pexpr)->subs)) == NULL) {
                FAIL("array_incr");
            }
            if ((*parg = lkit_expr_new(*pexpr)) == NULL) {
                TR(LKIT_PARSE_EXPRDEF + 5);
                goto err;
            }
            (*parg)->type = *pargty;
            (*parg)->fparam = 1;
            (*parg)->fparam_idx = it.iter - 1;
            lexpr_add_to_ctx(*pexpr, *pargname, *parg);
        }



        if ((body = array_incr(&(*pexpr)->subs)) == NULL) {
            FAIL("array_incr");
        }

        if ((*body = lkit_expr_parse(mctx, *pexpr, *node, 1)) == NULL) {
            (*node)->error = 1;
            TRACE("failed to parse function %s body", name->data);
            TR(LKIT_PARSE_EXPRDEF + 6);
            goto err;
        }
    }

    (*pexpr)->isbuiltin = (flags & LKIT_BUILTIN_PARSE_EXPRDEF_ISBLTIN);
    if (flags & LKIT_BUILTIN_PARSE_EXPRDEF_FORCELAZY) {
        (*pexpr)->lazy_init = 1;
    }
    if (flags & LKIT_BUILTIN_PARSE_EXPRDEF_MACRO) {
        (*pexpr)->ismacro = 1;
    }

    lexpr_add_to_ctx(ectx, name, *pexpr);

end:
    return res;

err:
    if (pexpr != NULL) {
        (void)lkit_expr_destroy(pexpr);
    }
    res = 1;
    goto end;
}


int
builtin_remove_undef(mrklkit_ctx_t *mctx, lkit_expr_t *ectx, lkit_expr_t *expr)
{
    char *name;

    if (expr->undef_removed) {
        return 0;
    }

    name = (expr->name != NULL) ? (char *)expr->name->data : ")(";

    //TRACE("%s: %s", name, LKIT_TAG_STR(expr->type->tag));
    /*
     * (func undef )
     */
    if (strcmp(name, "if") == 0) {
        lkit_expr_t **cond, **texp, **fexp;
        lkit_type_t *tty, *fty;

        /*
         * (sym if (func undef bool undef undef))
         */
        cond = array_get(&expr->subs, 0);
        assert(cond != NULL);

        if (builtin_remove_undef(mctx, ectx, *cond) != 0) {
            TRRET(REMOVE_UNDEF + 1);
        }

        if ((*cond)->type->tag != LKIT_BOOL) {
            (*cond)->error = 1;
            TRACE("condition not bool in:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 2);
        }

        texp = array_get(&expr->subs, 1);
        assert(texp != NULL);

        if (builtin_remove_undef(mctx, ectx, *texp) != 0) {
            TRRET(REMOVE_UNDEF + 3);
        }

        fexp = array_get(&expr->subs, 2);
        assert(fexp != NULL);

        if (builtin_remove_undef(mctx, ectx, *fexp) != 0) {
            TRRET(REMOVE_UNDEF + 4);
        }

        tty = (*texp)->type;
        fty = (*fexp)->type;

        if (lkit_type_cmp(tty, fty) == 0) {
            expr->type = tty;
        } else {
            (*texp)->error = 1;
            (*fexp)->error = 1;
            TRACE("types of true and false expressions don't match:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 5);
        }

    } else if (strcmp(name, ",") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
                TRRET(REMOVE_UNDEF + 6);
            }
        }
        //if ((arg = array_last(&expr->subs, &it)) == NULL) {
        //    TRACE("empty argument list in comma:");
        //    lkit_expr_dump(expr);
        //    if (expr->parent) {
        //        lkit_expr_dump(expr->parent);
        //    }
        //    TRRET(REMOVE_UNDEF + 7);
        //}

        if ((arg = array_last(&expr->subs, &it)) != NULL) {
            expr->type = (*arg)->type;
        }

    } else if (strcmp(name, "print") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        /*
         * (sym print (func undef ...))
         */

        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {
            if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
                TRRET(REMOVE_UNDEF + 8);
            }
        }

        arg = array_last(&expr->subs, &it);
        if (arg == NULL) {
            expr->error = 1;
            TRACE("empty print");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 9);
        }
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

        /*
         * (sym +|*|%|-|/ (func undef undef ...))
         */
        aarg = array_first(&expr->subs, &it);
        assert(aarg != NULL);

        if (builtin_remove_undef(mctx, ectx, *aarg) != 0) {
            TRRET(REMOVE_UNDEF + 10);
        }

        for (barg = array_next(&expr->subs, &it);
             barg != NULL;
             barg = array_next(&expr->subs, &it)) {

            if (builtin_remove_undef(mctx, ectx, *barg) != 0) {
                TRRET(REMOVE_UNDEF + 11);
            }

            if (lkit_type_cmp((*aarg)->type, (*barg)->type) != 0) {
                (*barg)->error = 1;
                TRACE("incompatible argument types in:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 12);
            }
        }
        expr->type = (*aarg)->type;

    } else if (strcmp(name, "get") == 0 ||
               strcmp(name, "get-index") == 0 || /* compat*/
               strcmp(name, "get-key") == 0 || /* compat*/
               strcmp(name, "parse") == 0) {
        /*
         * (sym {get|parse} (func undef struct conststr undef))
         * (sym {get|parse} (func undef dict conststr undef))
         * (sym {get|parse} (func undef array constint undef))
         */
        lkit_expr_t **cont;
        lkit_type_t *ty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if (builtin_remove_undef(mctx, ectx, *cont) != 0) {
            TRRET(REMOVE_UNDEF + 13);
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
                    TRACE("expected string literal as a struct member:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 14);
                }

                bname = (bytes_t *)(*name)->value.literal->body;
                //TRACE("bname=%s ts=%p", bname->data, ts);
                if ((elty =
                        lkit_struct_get_field_type(ts, bname)) == NULL) {
                    (*cont)->error = 1;
                    TRACE("problem name %s", bname->data);
                    lkit_type_dump((lkit_type_t *)ts);
                    TRRET(REMOVE_UNDEF + 15);
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

                if (builtin_remove_undef(mctx, ectx, *name) != 0) {
                    TRRET(REMOVE_UNDEF + 16);
                }

                /* constant str */
                if ((*name)->type->tag != LKIT_STR) {

                    (*name)->error = 1;
                    TRACE("expected string as a dict key:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 17);
                }

                if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 18);
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
                    TRRET(REMOVE_UNDEF + 18);
                }

                if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 19);
                }

                expr->type = elty;
            }
            break;

        default:
            lkit_expr_dump(expr);
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 20);
        }

        if (strcmp(name, "parse") != 0) {
            lkit_expr_t **dflt;

            dflt = array_get(&expr->subs, 2);
            if (dflt == NULL) {
                expr->error = 1;
                TRACE("expected expected default value:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 21);
            }

            if (builtin_remove_undef(mctx, ectx, *dflt) != 0) {
                TRRET(REMOVE_UNDEF + 22);
            }

            if ((*dflt)->type->tag != LKIT_NULL) {
                if (lkit_type_cmp(expr->type, (*dflt)->type) != 0) {
                    (*dflt)->error = 1;
                    TRACE("type of default value doesn't match item type:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 23);
                }
            }
        } else {
            expr->nodecref = 1;
        }
        if ((*cont)->nodecref) {
            expr->nodecref = 1;
        }


    } else if (strcmp(name, "set") == 0) {
        /*
         * (sym set (func void struct conststr undef))
         * (sym set (func void dict conststr undef))
         * (sym set (func void array constint undef))
         */
        lkit_expr_t **cont, **setv;
        lkit_type_t *ty;
        lkit_type_t *elty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if (builtin_remove_undef(mctx, ectx, *cont) != 0) {
            TRRET(REMOVE_UNDEF + 24);
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
                    TRRET(REMOVE_UNDEF + 25);
                }

                bname = (bytes_t *)(*name)->value.literal->body;
                if ((elty =
                        lkit_struct_get_field_type(ts, bname)) == NULL) {
                    (*cont)->error = 1;
                    TRACE("problem name: %s", bname->data);
                    TRRET(REMOVE_UNDEF + 26);
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
                    TRRET(REMOVE_UNDEF + 27);
                }

                if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 28);
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
                    TRRET(REMOVE_UNDEF + 29);
                }

                if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 30);
                }

            }
            break;

        default:
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 31);

        }

        setv = array_get(&expr->subs, 2);
        assert(setv != NULL);

        if (builtin_remove_undef(mctx, ectx, *setv) != 0) {
            TRRET(REMOVE_UNDEF + 32);
        }

        if (lkit_type_cmp((*setv)->type, elty) != 0) {
            (*setv)->error = 1;
            TRRET(REMOVE_UNDEF + 33);
        }

    } else if (strcmp(name, "del") == 0) {
        /*
         * (sym del (func void struct conststr))
         * (sym del (func void dict conststr))
         * (sym del (func void array constint))
         */
        lkit_expr_t **cont;
        lkit_type_t *ty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if (builtin_remove_undef(mctx, ectx, *cont) != 0) {
            TRRET(REMOVE_UNDEF + 34);
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
                    TRRET(REMOVE_UNDEF + 35);
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
                    TRRET(REMOVE_UNDEF + 36);
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
                    TRRET(REMOVE_UNDEF + 37);
                }
            }
            break;

        default:
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 38);

        }

    } else if (strcmp(name, "dp-info") == 0) {
        /*
         * (sym dp-info (func undef conststr))
         */
        lkit_expr_t **cont;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if (builtin_remove_undef(mctx, ectx, *cont) != 0) {
            TRRET(REMOVE_UNDEF + 39);
        }

        if ((*cont)->type->tag != LKIT_STRUCT) {
            TRRET(REMOVE_UNDEF + 40);
        } else {
            lkit_expr_t **opt;
            bytes_t *optname;

            if ((opt = array_get(&expr->subs, 1)) == NULL) {
                expr->error = 1;
                TRRET(REMOVE_UNDEF + 41);
            }

            /* constant str */
            if ((*opt)->type->tag != LKIT_STR ||
                !LKIT_EXPR_CONSTANT(*opt)) {

                (*opt)->error = 1;
                TRRET(REMOVE_UNDEF + 42);
            }

            optname = (bytes_t *)(*opt)->value.literal->body;
            if (strcmp((char *)optname->data, "pos") == 0) {
                expr->type = lkit_type_get(mctx, LKIT_INT);

            } else if (strcmp((char *)optname->data, "data") == 0) {
                expr->type = lkit_type_get(mctx, LKIT_STR);

            } else {
                //lkit_expr_dump(expr);
                (*opt)->error = 1;
                TRRET(REMOVE_UNDEF + 43);
            }
        }

    } else if (strcmp(name, "==") == 0 ||
               strcmp(name, "=") == 0 || /* compat */
               strcmp(name, "!=") == 0 ||
               strcmp(name, "<") == 0 ||
               strcmp(name, "<=") == 0 ||
               strcmp(name, ">") == 0 ||
               strcmp(name, ">=") == 0) {

        lkit_expr_t **arg;
        array_iter_t it;

        for (arg = array_first(&expr->subs, &it);
             arg != NULL;
             arg = array_next(&expr->subs, &it)) {

            if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
                TRRET(REMOVE_UNDEF + 44);
            }
        }

    } else if (strcmp(name, "tostr") == 0) {
        /*
         * (sym tostr (func str undef))
         */
        lkit_expr_t **arg;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

        if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
            TRRET(REMOVE_UNDEF + 45);
        }

    } else if (strcmp(name, "has") == 0 ||
               strcmp(name, "has-key") == 0) {

        lkit_expr_t **cont;
        lkit_type_t *ty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if (builtin_remove_undef(mctx, ectx, *cont) != 0) {
            TRRET(REMOVE_UNDEF + 46);
        }

        ty = (*cont)->type;

        switch (ty->tag) {
        case LKIT_DICT:
            {
                lkit_expr_t **name;

                name = array_get(&expr->subs, 1);
                assert(name != NULL);

                /* constant str */
                if ((*name)->type->tag != LKIT_STR ||
                    !LKIT_EXPR_CONSTANT(*name)) {

                    (*name)->error = 1;
                    TRACE("expected string literal as a dict key:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 47);
                }
            }
            break;

        default:
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 48);

        }

    } else if (strcmp(name, "in") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;
        lkit_type_t *ty;

        ty = NULL;
        for (arg = array_first(&expr->subs, &it);
             arg!= NULL;
             arg = array_next(&expr->subs, &it)) {

            if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
                TRRET(REMOVE_UNDEF + 49);
            }

            if (ty != NULL) {
                if (ty->tag != (*arg)->type->tag) {
                    (*arg)->error = 1;
                    TRACE("type of the argument does not match those previous:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 50);
                }
            } else {
                ty = (*arg)->type;
            }
        }

    } else if (strcmp(name, "len") == 0) {
        lkit_expr_t **subj;

        subj = array_get(&expr->subs, 0);
        assert(subj != NULL);

        if (builtin_remove_undef(mctx, ectx, *subj) != 0) {
            TRRET(REMOVE_UNDEF + 51);
        }

        if (expr->type->tag != LKIT_INT) {
            FAIL("builtin_remove_undef");
        }

    } else if (strcmp(name, "dp-new") == 0) {
        /* (func undef ty undef) */
        lkit_expr_t **ty, **arg;

        ty = array_get(&expr->subs, 0);
        assert(ty != NULL);

        switch ((*ty)->type->tag) {
        case LKIT_ARRAY:
        case LKIT_DICT:
        case LKIT_STRUCT:
            break;

        default:
            (*ty)->error = 1;
            TRACE("type is not  supported in dp-new:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 52);
        }

        expr->type = (*ty)->type;

        arg = array_get(&expr->subs, 1);
        assert(arg != NULL);
        if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
            TRRET(REMOVE_UNDEF + 53);
        }

        if ((*arg)->type->tag != LKIT_STR) {
            (*arg)->error = 1;
            TRACE("argument must be a string in new:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 54);
        }

    } else if (strcmp(name, "new") == 0) {
        /* (func undef ty) */
        lkit_expr_t **ty;

        ty = array_get(&expr->subs, 0);
        assert(ty != NULL);

        switch ((*ty)->type->tag) {
        case LKIT_ARRAY:
        case LKIT_DICT:
        case LKIT_STRUCT:
            break;

        default:
            (*ty)->error = 1;
            TRACE("type is not supported in new:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 52);
        }

        expr->type = (*ty)->type;

    } else if (strcmp(name, "isnull") == 0) {
        lkit_expr_t **arg;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

        if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
            TRRET(REMOVE_UNDEF + 53);
        }

        if (!((*arg)->type->tag == LKIT_NULL ||
            (*arg)->type->tag == LKIT_STR ||
            (*arg)->type->tag == LKIT_ANY ||
            (*arg)->type->tag == LKIT_ARRAY ||
            (*arg)->type->tag == LKIT_DICT ||
            (*arg)->type->tag == LKIT_STRUCT ||
            (*arg)->type->tag == LKIT_FUNC ||
            (*arg)->type->tag >= LKIT_USER)) {

            (*arg)->error = 1;
            TRACE("arg is not supported in isnull:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 54);
        }

    } else {
        /*
         * all others
         */
        lkit_expr_t *ref = NULL;
        lkit_type_t *refty = NULL;
        lkit_expr_t **arg;
        array_iter_t it;

        if (expr->isref) {
            if ((ref = lkit_expr_find(ectx, expr->name)) == NULL) {
                lkit_expr_dump(expr);
                TRACE("name=%p", expr->name);
                TRRET(REMOVE_UNDEF + 55);
            }

            if (builtin_remove_undef(mctx, ectx, ref) != 0) {
                TRRET(REMOVE_UNDEF + 56);
            }

            /*
             * Automatic check of function arguments
             */
            if (ref->type->tag == LKIT_FUNC) {
                lkit_func_t *fty;
                int isvararg = 0;

                fty = (lkit_func_t *)ref->type;

                for (arg = array_first(&expr->subs, &it);
                     arg != NULL;
                     arg = array_next(&expr->subs, &it)) {


                    if (builtin_remove_undef(mctx, ectx, *arg) != 0) {
                        TRRET(REMOVE_UNDEF + 57);
                    }

                    if (!isvararg) {
                        lkit_type_t *paramty;

                        paramty = lkit_func_get_arg_type(fty, it.iter);
                        if (paramty->tag == LKIT_VARARG) {
                            isvararg = 1;
                        } else {
                            if (paramty->tag != LKIT_TY) {
                                lkit_type_t *argty;

                                argty = lkit_expr_type_of(*arg);
                                if (lkit_type_cmp_loose(argty, paramty) != 0) {
                                    (*arg)->error = 1;
                                    TRACE("type of the argument does not "
                                          "match function definition:");
                                    lkit_expr_dump(expr);
                                    TRRET(REMOVE_UNDEF + 58);
                                }
                            }
                        }
                    }
                }
            }

            refty = lkit_expr_type_of(ref);

            if (expr->type->tag == LKIT_UNDEF) {
                expr->type = refty;
            } else {
                lkit_type_t *exprty;

                exprty = lkit_expr_type_of(expr);
                if (exprty->tag != refty->tag) {
                    TRRET(REMOVE_UNDEF + 59);
                }
            }
        }
    }

    if (expr->type->tag == LKIT_UNDEF) {
        mrklkit_module_t **mod;
        array_iter_t it;

        for (mod = array_first(&mctx->modules, &it);
             mod != NULL;
             mod = array_next(&mctx->modules, &it)) {
            if ((*mod)->remove_undef != NULL) {
                if ((*mod)->remove_undef(mctx, ectx, expr) != 0) {
                    TR(REMOVE_UNDEF + 60);
                    break;
                }
            }
        }
    }

    if (expr->type->tag == LKIT_UNDEF) {
        /* should not be reached */
        lkit_expr_dump(expr);
        FAIL("builtin_remove_undef");
    }

    expr->undef_removed = 1;

    return 0;
}
