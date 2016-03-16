#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(builtin);
#endif

#define ETR(m, expr, aty, pty)         \
do {                                   \
    TRACE(m);                          \
    lkit_type_dump((lkit_type_t *)aty);\
    TRACEC(" !~\n");                   \
    lkit_type_dump((lkit_type_t *)pty);\
    TRACEC("in:\n");                   \
    lkit_expr_dump(expr);\
} while (0)                            \


static bytes_t _copy = BYTES_INITIALIZER("copy");

/*
 * (keyword name value)
 */
int
builtin_parse_exprdef(mrklkit_ctx_t *mctx,
                     lkit_cexpr_t *ectx,
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

    if ((pexpr = array_incr(&ectx->base.subs)) == NULL) {
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
        lkit_cexpr_t *cexpr;
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

        assert((*pexpr)->isctx);
        cexpr = (lkit_cexpr_t *)(*pexpr);

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

            if ((parg = array_incr(&(*pexpr)->subs)) == NULL) {
                FAIL("array_incr");
            }
            if ((*parg = lkit_expr_new()) == NULL) {
                TR(LKIT_PARSE_EXPRDEF + 5);
                goto err;
            }
            (*parg)->type = *pargty;
            (*parg)->fparam = 1;
            (*parg)->fparam_idx = it.iter - 1;

            if ((pargname = array_get(&ft->names, it.iter)) == NULL) {
                FAIL("array_get");
            }
            if (*pargname == NULL) {
                TRACE("named argument expected for %d in: %s",
                      it.iter - 1,
                      name->data);
                TR(LKIT_PARSE_EXPRDEF + 5);
                //lkit_expr_dump(*pexpr);
                //lkit_type_dump((lkit_type_t *)ft);
                //lkit_expr_dump(*parg);
                goto err;
            }
            lexpr_add_to_ctx(cexpr, *pargname, *parg);
        }

        if ((body = array_incr(&(*pexpr)->subs)) == NULL) {
            FAIL("array_incr");
        }

        //if ((*body = lkit_expr_parse(mctx, *pexpr, *node, 1)) == NULL) {
        if ((*body = lkit_expr_parse(mctx, cexpr, *node, 1)) == NULL) {
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
builtin_remove_undef(mrklkit_ctx_t *mctx,
                     lkit_cexpr_t *ectx,
                     lkit_expr_t *expr)
{
    char *name;
    lkit_expr_t **psub;
    array_iter_t it;

    if (expr->undef_removed) {
        return 0;
    }

    for (psub = array_first(&expr->subs, &it);
         psub != NULL;
         psub = array_next(&expr->subs, &it)) {

        if (builtin_remove_undef(mctx, ectx, *psub) != 0) {
            TRRET(REMOVE_UNDEF + 2);
        }
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

        if ((*cond)->type->tag != LKIT_BOOL) {
            (*cond)->error = 1;
            TRACE("condition not bool in:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 11);
        }

        texp = array_get(&expr->subs, 1);
        assert(texp != NULL);

        fexp = array_get(&expr->subs, 2);
        assert(fexp != NULL);

        tty = (*texp)->type;
        fty = (*fexp)->type;

        if (lkit_type_cmp(tty, fty) == 0) {
            expr->type = tty;
        } else {
            (*texp)->error = 1;
            (*fexp)->error = 1;
            TRACE("types of true and false expressions don't match:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 14);
        }
        expr->zref = (*texp)->zref || (*fexp)->zref;

    } else if (strcmp(name, ",") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        if ((arg = array_last(&expr->subs, &it)) != NULL) {
            expr->type = (*arg)->type;
            expr->zref = (*arg)->zref;
        }

    } else if (strcmp(name, "print") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;

        /*
         * (sym print (func undef ...))
         */

        arg = array_last(&expr->subs, &it);
        if (arg == NULL) {
            expr->error = 1;
            TRACE("empty print");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 21);
        }
        expr->type = (*arg)->type;
        expr->zref = (*arg)->zref;

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

        for (barg = array_next(&expr->subs, &it);
             barg != NULL;
             barg = array_next(&expr->subs, &it)) {

            if (lkit_type_cmp((*aarg)->type, (*barg)->type) != 0) {
                (*barg)->error = 1;
                TRACE("incompatible argument types in:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 42);
            }
        }
        expr->type = (*aarg)->type;
        if (LKIT_TAG_POINTER(expr->type->tag)) {
            /* (+ str str), see in builtingen.c */
            expr->zref = 1;
        }

    } else if (strcmp(name, "get") == 0 ||
               strcmp(name, "get-index") == 0 || /* compat*/
               strcmp(name, "get-key") == 0 /* compat*/) {
        /*
         * (sym {get} (func undef struct conststr undef))
         * (sym {get} (func undef dict str undef))
         * (sym {get} (func undef array constint undef))
         */
        lkit_expr_t **cont;
        lkit_type_t *ty;
        lkit_expr_t **dflt;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

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
                    TRRET(REMOVE_UNDEF + 51);
                }

                bname = (bytes_t *)(*name)->value.literal->body;
                //TRACE("bname=%s ts=%p", bname->data, ts);
                if ((elty =
                        lkit_struct_get_field_type(ts, bname)) == NULL) {
                    (*cont)->error = 1;
                    TRACE("problem name %s", bname->data);
                    lkit_type_dump((lkit_type_t *)ts);
                    TRRET(REMOVE_UNDEF + 52);
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

                if ((*name)->type->tag != LKIT_STR) {
                    (*name)->error = 1;
                    TRACE("expected string as a dict key:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 55);
                }

                if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 56);
                }

                if ((*cont)->mpolicy != (*name)->mpolicy) {
                    if (lkit_expr_promote_policy(mctx,
                                                 *name,
                                                 (*cont)->mpolicy) != 0) {
                        lkit_expr_t *tmp;

                        tmp = lkit_expr_build_ref(ectx, &_copy);
                        tmp->mpolicy = (*cont)->mpolicy;
                        (void)lkit_expr_add_sub(tmp, *name);
                        *name = tmp;
                        if (builtin_remove_undef(mctx, ectx, *name) != 0) {
                            (*cont)->error = 1;
                            (*name)->error = 1;
                            TRRET(REMOVE_UNDEF + 57);
                        }
                    }
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
                    TRRET(REMOVE_UNDEF + 57);
                }

                if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 58);
                }

                expr->type = elty;
            }
            break;

        case LKIT_PARSER:
            {
                lkit_parser_t *tp = (lkit_parser_t *)ty;
                lkit_expr_t **name;
                bytes_t *bname;
                lkit_type_t *pty, *elty;

                name = array_get(&expr->subs, 1);
                assert(name != NULL);

                pty = LKIT_PARSER_GET_TYPE(tp);

                /* constant str */
                if ((*name)->type->tag != LKIT_STR ||
                    !LKIT_EXPR_CONSTANT(*name)) {

                    (*name)->error = 1;
                    TRACE("expected string literal as a struct member:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 59);
                }

                bname = (bytes_t *)(*name)->value.literal->body;

                switch (pty->tag) {
                case LKIT_ARRAY:
                    if ((elty = lkit_array_get_element_type(
                                    (lkit_array_t *)pty)) == NULL) {
                        (*cont)->error = 1;
                        TRACE("problem array type");
                        lkit_type_dump((lkit_type_t *)pty);
                        TRRET(REMOVE_UNDEF + 60);
                    }
                    break;

                case LKIT_DICT:
                    if ((elty = lkit_dict_get_element_type(
                                    (lkit_dict_t *)pty)) == NULL) {
                        (*cont)->error = 1;
                        TRACE("problem dict type");
                        lkit_type_dump((lkit_type_t *)pty);
                        TRRET(REMOVE_UNDEF + 61);
                    }
                    break;

                case LKIT_STRUCT:
                    if ((elty = lkit_struct_get_field_type(
                                    (lkit_struct_t *)pty, bname)) == NULL) {
                        (*cont)->error = 1;
                        TRACE("problem struct name %s", bname->data);
                        lkit_type_dump((lkit_type_t *)pty);
                        TRRET(REMOVE_UNDEF + 62);
                    }
                    break;

                default:
                    (*name)->error = 1;
                    TRACE("parser type not supported:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 63);
                }

                expr->type = elty;
            }
            break;

        default:
            TRACE("container not supported:");
            lkit_expr_dump(expr);
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 64);
        }

        if (expr->type->tag == LKIT_PARSER) {
            expr->type = LKIT_PARSER_GET_TYPE(expr->type);
        }

        dflt = array_get(&expr->subs, 2);
        assert(dflt != NULL);

        if ((*dflt)->type->tag != LKIT_NULL) {
            lkit_type_t *dfty;

            dfty = (*dflt)->type;
            if (lkit_type_cmp(expr->type, dfty) != 0) {
                (*dflt)->error = 1;
                TRACE("type of default value doesn't match item type:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 72);
            }
        }

        if ((*cont)->mpolicy != (*dflt)->mpolicy) {
            if (lkit_expr_promote_policy(mctx,
                                         *dflt,
                                         (*cont)->mpolicy) != 0) {
                lkit_expr_t *tmp;

                tmp = lkit_expr_build_ref(ectx, &_copy);
                tmp->mpolicy = (*cont)->mpolicy;
                (void)lkit_expr_add_sub(tmp, *dflt);
                *dflt = tmp;
                if (builtin_remove_undef(mctx, ectx, *dflt) != 0) {
                    (*cont)->error = 1;
                    (*dflt)->error = 1;
                    TRRET(REMOVE_UNDEF + 73);
                }
            }
        }

        expr->mpolicy = (*cont)->mpolicy;

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

        ty = (*cont)->type;

        if ((*cont)->zref) {
            (*cont)->error = 1;
            lkit_expr_dump((*cont)->value.ref);
            TRRET(REMOVE_UNDEF + 80);
        }

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
                    TRRET(REMOVE_UNDEF + 81);
                }

                bname = (bytes_t *)(*name)->value.literal->body;
                if ((elty =
                        lkit_struct_get_field_type(ts, bname)) == NULL) {
                    (*cont)->error = 1;
                    TRACE("problem name: %s", bname->data);
                    TRRET(REMOVE_UNDEF + 82);
                }

            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *td = (lkit_dict_t *)ty;
                lkit_expr_t **name;

                name = array_get(&expr->subs, 1);
                assert(name != NULL);

                if ((*name)->type->tag != LKIT_STR) {
                    (*name)->error = 1;
                    TRACE("expected string as a dict key:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 84);
                }

                if ((elty = lkit_dict_get_element_type(td)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 85);
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
                    TRRET(REMOVE_UNDEF + 86);
                }

                if ((elty = lkit_array_get_element_type(ta)) == NULL) {
                    (*cont)->error = 1;
                    TRRET(REMOVE_UNDEF + 87);
                }

            }
            break;

        default:
            (*cont)->error = 1;
            TRACE("container not supported:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 88);
        }

        setv = array_get(&expr->subs, 2);
        assert(setv != NULL);

        if (lkit_type_cmp((*setv)->type, elty) != 0) {
            (*setv)->error = 1;
            TRRET(REMOVE_UNDEF + 90);
        }

        if ((*cont)->mpolicy != (*setv)->mpolicy) {
            if (lkit_expr_promote_policy(mctx, *setv, (*cont)->mpolicy) != 0) {
                lkit_expr_t *tmp;

                tmp = lkit_expr_build_ref(ectx, &_copy);
                tmp->mpolicy = (*cont)->mpolicy;
                (void)lkit_expr_add_sub(tmp, *setv);
                *setv = tmp;
                if (builtin_remove_undef(mctx, ectx, *setv) != 0) {
                    (*cont)->error = 1;
                    (*setv)->error = 1;
                    TRRET(REMOVE_UNDEF + 91);
                }
            }
        }
        expr->mpolicy = (*cont)->mpolicy;

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

        ty = (*cont)->type;

        if ((*cont)->zref) {
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 100);
        }

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
                    TRRET(REMOVE_UNDEF + 101);
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
                    TRRET(REMOVE_UNDEF + 102);
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
                    TRRET(REMOVE_UNDEF + 103);
                }
            }
            break;

        default:
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 104);

        }
        expr->mpolicy = (*cont)->mpolicy;

    } else if (strcmp(name, "parse") == 0) {
        lkit_expr_t **cont, **name;
        bytes_t *bname;
        lkit_parser_t *tp;
        lkit_struct_t *ts;
        lkit_type_t *elty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if ((*cont)->zref) {
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 280);
        }

        if ((*cont)->mpolicy != mctx->dparse_mpolicy) {
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 281);
        }

        tp = (lkit_parser_t *)(*cont)->type;

        if (tp->base.tag != LKIT_PARSER) {
            TRRET(REMOVE_UNDEF + 282);
        }
        ts = (lkit_struct_t *)LKIT_PARSER_GET_TYPE(tp);
        assert(ts->base.tag == LKIT_STRUCT);

        name = array_get(&expr->subs, 1);
        assert(name != NULL);

        /* constant str */
        if ((*name)->type->tag != LKIT_STR ||
            !LKIT_EXPR_CONSTANT(*name)) {
            (*name)->error = 1;
            TRRET(REMOVE_UNDEF + 283);
        }

        bname = (bytes_t *)(*name)->value.literal->body;
        if ((elty = lkit_struct_get_field_type(ts, bname)) == NULL) {
            (*cont)->error = 1;
            TRACE("problem name: %s", bname->data);
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 284);
        }
        expr->type = elty;
        expr->mpolicy = (*cont)->mpolicy;

    } else if (strcmp(name, "dp-info") == 0) {
        /*
         * (sym dp-info (func undef conststr))
         */
        lkit_expr_t **cont;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if ((*cont)->type->tag == LKIT_UNDEF) {
            TRRET(REMOVE_UNDEF + 111);
        } else {
            lkit_expr_t **opt;
            bytes_t *optname;

            if ((opt = array_get(&expr->subs, 1)) == NULL) {
                expr->error = 1;
                TRRET(REMOVE_UNDEF + 112);
            }

            /* constant str */
            if ((*opt)->type->tag != LKIT_STR ||
                !LKIT_EXPR_CONSTANT(*opt)) {

                (*opt)->error = 1;
                TRRET(REMOVE_UNDEF + 113);
            }

            optname = (bytes_t *)(*opt)->value.literal->body;
            if (strcmp((char *)optname->data, "pos") == 0) {
                expr->type = lkit_type_get(mctx, LKIT_INT);

            } else if (strcmp((char *)optname->data, "data") == 0) {
                expr->type = lkit_type_get(mctx, LKIT_STR);
                expr->zref = 1;

            } else {
                //lkit_expr_dump(expr);
                (*opt)->error = 1;
                TRRET(REMOVE_UNDEF + 114);
            }

            expr->mpolicy = mctx->dparse_mpolicy;
        }

    } else if (strcmp(name, "==") == 0 ||
               strcmp(name, "=") == 0 || /* compat */
               strcmp(name, "!=") == 0 ||
               strcmp(name, "<") == 0 ||
               strcmp(name, "<=") == 0 ||
               strcmp(name, ">") == 0 ||
               strcmp(name, ">=") == 0 ||
               strcmp(name, "and") == 0 ||
               strcmp(name, "or") == 0) {

        lkit_expr_t **a, **b;
        array_iter_t it;

        a = array_first(&expr->subs, &it);
        assert(a != NULL);
        for (b = array_next(&expr->subs, &it);
             b != NULL;
             b = array_next(&expr->subs, &it)) {
            if (lkit_type_cmp_loose((*a)->type, (*b)->type) != 0) {
                (*b)->error = 1;
                TRACE("incompatible argument types in:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 120);
            }
        }

    } else if (strcmp(name, "tostr") == 0) {
        /*
         * (sym tostr (func str undef))
         */
        lkit_expr_t **arg;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);
        if ((*arg)->type->tag == LKIT_STR) {
            /* see in builtingen.c */
            expr->zref = (*arg)->zref;
            expr->mpolicy = (*arg)->mpolicy;
        } else {
            expr->zref = 1;
            //expr->mpolicy = ?
        }


    } else if (strcmp(name, "has") == 0 ||
               strcmp(name, "has-key") == 0) {

        lkit_expr_t **cont;
        lkit_type_t *ty;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        ty = (*cont)->type;

        if ((*cont)->zref) {
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 140);
        }

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
                    TRRET(REMOVE_UNDEF + 141);
                }
            }
            break;

        default:
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 142);

        }

    } else if (strcmp(name, "in") == 0) {
        lkit_expr_t **arg;
        array_iter_t it;
        lkit_type_t *ty;

        ty = NULL;
        for (arg = array_first(&expr->subs, &it);
             arg!= NULL;
             arg = array_next(&expr->subs, &it)) {

            if (ty != NULL) {
                if (ty->tag != (*arg)->type->tag) {
                    (*arg)->error = 1;
                    TRACE("type of the argument does not match "
                          "those previous:");
                    lkit_expr_dump(expr);
                    TRRET(REMOVE_UNDEF + 151);
                }
            } else {
                ty = (*arg)->type;
            }
            /*
             * XXX see in builtingen.c
             */
            if ((*arg)->zref) {
                (*arg)->error = 1;
                TRACE("cannot use zref argument in:");
                lkit_expr_dump(expr);
                TRRET(REMOVE_UNDEF + 152);
            }
        }

    } else if (strcmp(name, "len") == 0) {
        lkit_expr_t **subj;

        subj = array_get(&expr->subs, 0);
        assert(subj != NULL);

        if (expr->type->tag != LKIT_INT) {
            FAIL("builtin_remove_undef");
        }

    } else if (strcmp(name, "atoty") == 0) {
        /* (func undef ty str) */
        lkit_expr_t **arg;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);
        expr->type = (*arg)->type;

        if (expr->type->tag == LKIT_PARSER) {
            expr->type = LKIT_PARSER_GET_TYPE(expr->type);
        }

        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "new") == 0) {
        /* (func undef ty) */
        lkit_expr_t **arg;
        lkit_type_t *ty;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

        ty = (*arg)->type;

        if (ty->tag == LKIT_PARSER) {
            ty = LKIT_PARSER_GET_TYPE(ty);
        }

        switch (ty->tag) {
        case LKIT_ARRAY:
        case LKIT_DICT:
        case LKIT_STRUCT:
            break;

        default:
            (*arg)->error = 1;
            TRACE("type is not supported in new:");
            lkit_expr_dump(expr);
            TRRET(REMOVE_UNDEF + 180);
        }

        expr->type = ty;
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "isnull") == 0) {
        lkit_expr_t **arg;

        arg = array_get(&expr->subs, 0);
        assert(arg != NULL);

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
            TRRET(REMOVE_UNDEF + 191);
        }

    } else if (strcmp(name, "traverse") == 0) {
        lkit_expr_t **cont, **cbwrap, **cb;
        lkit_func_t *cbty;

        /*
         * (traverse cont (addrof fn))
         */

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        cbwrap = array_get(&expr->subs, 1);
        assert(cbwrap != NULL);

        cb = array_get(&(*cbwrap)->subs, 0);
        assert(cb != NULL);

        if ((*cb)->type->tag != LKIT_FUNC) {
            TRACE("Callback can only a function, found:");
            lkit_expr_dump(*cb);
            TRRET(REMOVE_UNDEF + 202);
        }

        cbty = (lkit_func_t *)(*cb)->type;

        if ((*cont)->type->tag == LKIT_ARRAY) {
            lkit_array_t *ta;
            lkit_type_t *elty;
            lkit_type_t *cbparamty;

            ta = (lkit_array_t *)(*cont)->type;
            elty = lkit_array_get_element_type(ta);
            cbparamty = lkit_func_get_arg_type(cbty, 0);

            if (cbparamty == NULL || lkit_type_cmp(elty, cbparamty) != 0) {
                TRRET(REMOVE_UNDEF + 203);
            }

        } else if ((*cont)->type->tag == LKIT_DICT) {
            lkit_dict_t *td;
            lkit_type_t *elty;
            lkit_type_t *cbparamty;

            td = (lkit_dict_t *)(*cont)->type;
            elty = lkit_dict_get_element_type(td);
            cbparamty = lkit_func_get_arg_type(cbty, 0);
            if (cbparamty == NULL || cbparamty->tag != LKIT_STR) {
                TRRET(REMOVE_UNDEF + 204);
            }

            cbparamty = lkit_func_get_arg_type(cbty, 1);
            if (cbparamty == NULL || lkit_type_cmp(elty, cbparamty) != 0) {
                TRRET(REMOVE_UNDEF + 205);
            }

        } else {
            TRACE("Container can only be dict or array, found:");
            lkit_expr_dump(*cont);
            TRRET(REMOVE_UNDEF + 206);
        }

    } else if (strcmp(name, "addrof") == 0) {
        lkit_expr_t **arg;

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            TRRET(REMOVE_UNDEF + 210);
        }

    } else if (strcmp(name, "copy") == 0) {
        lkit_expr_t **arg;

        if ((arg = array_get(&expr->subs, 0)) == NULL) {
            TRRET(REMOVE_UNDEF + 220);
        }

        expr->type = (*arg)->type;
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "substr") == 0) {
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "split") == 0) {
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "brushdown") == 0) {
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "urldecode") == 0) {
        expr->zref = 1;
        //expr->mpolicy = ?

    } else if (strcmp(name, "traverse") == 0) {
        lkit_expr_t **cont;

        cont = array_get(&expr->subs, 0);
        assert(cont != NULL);

        if ((*cont)->zref) {
            (*cont)->error = 1;
            TRRET(REMOVE_UNDEF + 290);
        }

    } else {
        /*
         * all others
         */
    }

    if (expr->isref && expr->type->tag == LKIT_UNDEF) {
        lkit_expr_t *ref = NULL;
        lkit_type_t *refty = NULL;

        ref = expr->value.ref;
        refty = lkit_expr_type_of(ref);
        expr->type = refty;
        expr->mpolicy = ref->mpolicy;
        expr->zref = ref->zref;
        assert(refty != NULL);
    }

    if (expr->type->tag == LKIT_UNDEF) {
        mrklkit_module_t **mod;
        array_iter_t it;

        for (mod = array_first(&mctx->modules, &it);
             mod != NULL;
             mod = array_next(&mctx->modules, &it)) {
            if ((*mod)->remove_undef != NULL) {
                if ((*mod)->remove_undef(mctx, ectx, expr) != 0) {
                    TR(REMOVE_UNDEF + 2000);
                    break;
                }
            }
        }
    }

    /*
     * special check for function call parameters (both undef and not)
     */
    if (expr->isref && expr->value.ref->type->tag == LKIT_FUNC) {
        lkit_func_t *tf = NULL;
        array_iter_t it;
        lkit_expr_t **psub;

        tf = (lkit_func_t *)(expr->value.ref->type);
        for (psub = array_first(&expr->subs, &it);
             psub != NULL;
             psub = array_next(&expr->subs, &it)) {
            lkit_type_t *aty, *pty;

            aty = lkit_func_get_arg_type(tf, it.iter);
            pty = lkit_expr_type_of(*psub);
            if (aty != NULL &&
                pty != NULL &&
                !(aty->tag == LKIT_UNDEF ||
                  aty->tag == LKIT_IR ||
                  aty->tag == LKIT_TY ||
                  aty->tag == LKIT_VOID ||
                  aty->tag == LKIT_NULL ||
                  aty->tag == LKIT_ANY ||
                  aty->tag == LKIT_VARARG)) {
                if (lkit_type_cmp_loose(aty, pty) != 0) {
                    ETR("argument type does not match formal "
                        "parameter type:", expr, aty, pty);
                    TRRET(REMOVE_UNDEF + 500);
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


static int
_cb1(lkit_gitem_t **gitem, void *udata)
{
    int res;

    struct {
        int (*cb)(mrklkit_ctx_t *, lkit_expr_t *, lkit_expr_t *);
        mrklkit_ctx_t *mctx;
        lkit_expr_t *ectx;
    } *params = udata;
    res = params->cb(params->mctx, params->ectx, (*gitem)->expr);
    /*
     * clear zref tag because this is going to be incref'ed expr, see
     * compile_dynamic_initializer()
     */
    (*gitem)->expr->zref = 0;
    return res;
}


int
lkit_expr_ctx_analyze(mrklkit_ctx_t *mctx,
                      lkit_cexpr_t *ectx)
{
    struct {
        int (*cb)(mrklkit_ctx_t *, lkit_cexpr_t *, lkit_expr_t *);
        mrklkit_ctx_t *mctx;
        lkit_cexpr_t *ectx;
    } params_remove_undef = { builtin_remove_undef, mctx, ectx };

    if (array_traverse(&ectx->glist,
                       (array_traverser_t)_cb1,
                       &params_remove_undef) != 0) {
        TRRET(LKIT_EXPR_CTX_ANALYZE + 1);
    }

    return 0;
}


