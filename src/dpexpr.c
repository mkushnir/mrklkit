#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/hash.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/fasthash.h>
#include <mrkcommon/util.h>

#include <mrklkit/dpexpr.h>
#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/module.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(dpexpr);
#endif


static int
parse_dpstr_quals(array_t *form,
                  array_iter_t *it,
                  unsigned char *qual,
                  lkit_dpstr_t *pa)
{
    char *s = (char *)qual;

    if (strcmp(s, ":parser") == 0) {
        char *v;

        v = NULL;
        if (lparse_next_word(form, it, &v, 1) != 0) {
            return 1;
        }
        assert(v != NULL);
        if (strcmp(v, "quoted") == 0) {
            pa->base.parser = LKIT_PARSER_QSTR;
        } else if (strcmp(v, "opt-quoted") == 0) {
            pa->base.parser = LKIT_PARSER_OPTQSTR;
        } else {
            pa->base.parser = LKIT_PARSER_NONE;
        }
    } else {
        fparser_datum_t **node;

        if ((node = array_next(form, it)) == NULL) {
            TRRET(PARSE_DPSTR_QUALS + 1);
        }
    }
    return 0;
}


static int
parse_dparray_quals(array_t *form,
                    array_iter_t *it,
                    unsigned char *qual,
                    lkit_dparray_t *pa)
{
    char *s = (char *)qual;

    if (strcmp(s, ":parser") == 0) {
        char *parser;

        parser = NULL;
        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp(parser, "delim") == 0) {
                pa->base.parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPARRAY_QUALS + 1);
                }
            } else if (strcmp(parser, "mdelim") == 0) {
                pa->base.parser = LKIT_PARSER_MDELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPARRAY_QUALS + 2);
                }
            } else if (strcmp(parser, "smartdelim") == 0) {
                pa->base.parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPARRAY_QUALS + 3);
                }
            } else {
                TRRET(PARSE_DPARRAY_QUALS + 4);
            }
        } else {
            TRRET(PARSE_DPARRAY_QUALS + 5);
        }
    } else if (strcmp(s, ":reserve") == 0) {
        if (lparse_next_int(form, it, &pa->nreserved, 1) != 0) {
            TRRET(PARSE_DPARRAY_QUALS + 6);
        }
    } else {
        fparser_datum_t **node;

        if ((node = array_next(form, it)) == NULL) {
            TRRET(PARSE_DPARRAY_QUALS + 7);
        }
    }
    return 0;
}


static int
parse_dpdict_quals(array_t *form,
                   array_iter_t *it,
                   unsigned char *qual,
                   lkit_dpdict_t *pa)
{
    char *s = (char *)qual;

    if (strcmp(s, ":parser") == 0) {
        char *parser;

        parser = NULL;
        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp(parser, "delim") == 0) {
                pa->base.parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &pa->pdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 1);
                }
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 2);
                }
            } else if (strcmp(parser, "smartdelim") == 0) {
                pa->base.parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_char(form, it, &pa->pdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 3);
                }
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 4);
                }
            } else if (strcmp(parser, "deilm-opt-quoted") == 0) {
                pa->base.parser = LKIT_PARSER_OPTQSTRDELIM;
                if (lparse_next_char(form, it, &pa->pdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 5);
                }
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPDICT_QUALS + 6);
                }
            } else {
                TRRET(PARSE_DPDICT_QUALS + 7);
            }
        } else {
            TRRET(PARSE_DPDICT_QUALS + 8);
        }
    } else if (strcmp(s, ":reserve") == 0) {
        if (lparse_next_int(form, it, &pa->nreserved, 1) != 0) {
            TRRET(PARSE_DPDICT_QUALS + 9);
        }
    } else {
        fparser_datum_t **node;

        if ((node = array_next(form, it)) == NULL) {
            TRRET(PARSE_DPDICT_QUALS + 10);
        }
    }
    return 0;
}


static int
parse_dpstruct_quals(array_t *form,
                     array_iter_t *it,
                     unsigned char *qual,
                     lkit_dpstruct_t *pa)
{
    char *s = (char *)qual;

    if (strcmp(s, ":parser") == 0) {
        char *parser;

        parser = NULL;
        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp(parser, "delim") == 0) {
                pa->base.parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPSTRUCT_QUALS + 1);
                }
            } else if (strcmp(parser, "mdelim") == 0) {
                pa->base.parser = LKIT_PARSER_MDELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPSTRUCT_QUALS + 2);
                }
            } else if (strcmp(parser, "smartdelim") == 0) {
                pa->base.parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_char(form, it, &pa->fdelim, 1) != 0) {
                    TRRET(PARSE_DPSTRUCT_QUALS + 3);
                }
            } else {
                TRRET(PARSE_DPSTRUCT_QUALS + 4);
            }
        } else {
            TRRET(PARSE_DPSTRUCT_QUALS + 5);
        }
    } else {
        fparser_datum_t **node;

        if ((node = array_next(form, it)) == NULL) {
            TRRET(PARSE_DPSTRUCT_QUALS + 6);
        }
    }
    return 0;
}


static int
dpexpr_fini_item(lkit_dpexpr_t **pa)
{
    if (*pa != NULL) {
        if ((*pa)->ty != NULL) {
            lkit_type_t *pty;

            pty = LKIT_PARSER_GET_TYPE((*pa)->ty);
            switch (pty->tag) {
            case LKIT_ARRAY:
                {
                    lkit_dparray_t **dpa;

                    dpa = (lkit_dparray_t **)pa;
                    array_fini(&(*dpa)->fields);
                }
                break;

            case LKIT_DICT:
                {
                    lkit_dpdict_t **dpd;

                    dpd = (lkit_dpdict_t **)pa;
                    array_fini(&(*dpd)->fields);
                }
                break;

            case LKIT_STRUCT:
                {
                    lkit_dpstruct_t **dps;

                    dps = (lkit_dpstruct_t **)pa;
                    array_fini(&(*dps)->fields);
                    array_fini(&(*dps)->names);
                }
                break;

            default:
                break;
            }
        }
        free(*pa);
        *pa = NULL;
    }
    return 0;
}


void
lkit_dpexpr_destroy(lkit_dpexpr_t **pa)
{
    (void)dpexpr_fini_item(pa);
}


static lkit_dpexpr_t *
lkit_dpexpr_new(mrklkit_ctx_t *mctx, int tag)
{
    lkit_dpexpr_t *res;

    res = NULL;

    switch (tag) {
    case LKIT_INT:
        {
            lkit_dpint_t *dpi;

            dpi = malloc(sizeof(lkit_dpint_t));
            dpi->base.ty = lkit_type_get_parser(mctx, tag);
            dpi->base.parser = LKIT_PARSER_NONE;
            res = (lkit_dpexpr_t *)dpi;
        }
        break;

    case LKIT_FLOAT:
        {
            lkit_dpfloat_t *dpf;

            dpf = malloc(sizeof(lkit_dpfloat_t));
            dpf->base.ty = lkit_type_get_parser(mctx, tag);
            dpf->base.parser = LKIT_PARSER_NONE;
            res = (lkit_dpexpr_t *)dpf;
        }
        break;

    case LKIT_STR:
        {
            lkit_dpstr_t *dps;

            dps = malloc(sizeof(lkit_dpstr_t));
            dps->base.ty = lkit_type_get_parser(mctx, tag);
            dps->base.parser = LKIT_PARSER_NONE;
            res = (lkit_dpexpr_t *)dps;
        }
        break;

    case LKIT_ARRAY:
        {
            lkit_dparray_t *dpa;

            dpa = malloc(sizeof(lkit_dparray_t));
            dpa->base.ty = NULL;
            dpa->base.parser = LKIT_PARSER_NONE;
            array_init(&(dpa)->fields,
                       sizeof(lkit_dpexpr_t *),
                       0,
                       NULL, /* XXX set to NULL manually */
                       (array_finalizer_t)dpexpr_fini_item);
            res = (lkit_dpexpr_t *)dpa;
        }
        break;

    case LKIT_DICT:
        {
            lkit_dpdict_t *dpd;

            dpd = malloc(sizeof(lkit_dpdict_t));
            dpd->base.ty = NULL;
            dpd->base.parser = LKIT_PARSER_NONE;
            array_init(&(dpd)->fields,
                       sizeof(lkit_dpexpr_t *),
                       0,
                       NULL, /* XXX set to NULL manually */
                       (array_finalizer_t)dpexpr_fini_item);
            res = (lkit_dpexpr_t *)dpd;
        }
        break;

    case LKIT_STRUCT:
        {
            lkit_dpstruct_t *dps;

            dps = malloc(sizeof(lkit_dpstruct_t));
            dps->base.ty = NULL;
            dps->base.parser = LKIT_PARSER_NONE;
            array_init(&(dps)->fields,
                       sizeof(lkit_dpexpr_t *),
                       0,
                       NULL, /* XXX set to NULL manually */
                       (array_finalizer_t)dpexpr_fini_item);
            array_init(&(dps)->names,
                       sizeof(bytes_t *),
                       0,
                       NULL,
                       NULL);
            res = (lkit_dpexpr_t *)dps;
        }
        break;

    default:
        FAIL("lkit_dpexpr_new");
    }

    return res;
}


static int
parse_fielddef(mrklkit_ctx_t *mctx,
               fparser_datum_t *dat,
               lkit_dpstruct_t *dps,
               lkit_struct_t *ts,
               void *udata,
               int seterror)
{
    array_t *form;
    array_iter_t it;
    bytes_t **name0, **name1;
    lkit_type_t **fty;
    lkit_dpexpr_t **fpa;
    fparser_datum_t **node;

    form = (array_t *)dat->body;

    if ((name0 = array_incr(&dps->names)) == NULL) {
        FAIL("array_incr");
    }

    if (lparse_first_word_bytes(form, &it, name0, seterror) != 0) {
        dat->error = seterror;
        TRRET(PARSE_FIELDDEF + 1);
    }

    if ((name1 = array_incr(&ts->names)) == NULL) {
        FAIL("array_incr");
    }
    *name1 = bytes_new_from_bytes(*name0);

    if ((fpa = array_incr(&dps->fields)) == NULL) {
        FAIL("array_incr");
    }

    if ((node = array_next(form, &it)) == NULL) {
        TRRET(PARSE_FIELDDEF + 2);
    }

    if ((*fpa = lkit_dpexpr_parse(mctx, *node, udata, seterror)) == NULL) {
        (*node)->error = seterror;
        TRRET(PARSE_FIELDDEF + 3);
    }

    if ((fty = array_incr(&ts->fields)) == NULL) {
        FAIL("array_incr");
    }
    *fty = LKIT_PARSER_GET_TYPE((*fpa)->ty);

    return 0;
}


lkit_dpexpr_t *
lkit_dpexpr_parse(mrklkit_ctx_t *mctx,
                  fparser_datum_t *dat,
                  void *udata,
                  int seterror)
{
    lkit_dpexpr_t *res;
    fparser_tag_t tag;
    array_t *form;
    array_iter_t it;
    bytes_t *b;
    char *pname;

    res = NULL;
    tag = FPARSER_DATUM_TAG(dat);
    form = NULL;
    b = NULL;

    switch (tag) {
    case FPARSER_WORD:
        b = (bytes_t *)(dat->body);
        break;

    case FPARSER_SEQ:
        form = (array_t *)dat->body;
        if (lparse_first_word_bytes(form, &it, &b, seterror) != 0) {
            TR(LKIT_DPEXPR_PARSE + 1);
            goto err;
        }
        break;

    default:
        TR(LKIT_DPEXPR_PARSE + 2);
        goto err;
    }


    pname = (char *)(b->data);
    if (strcmp(pname, "int") == 0) {
        res = lkit_dpexpr_new(mctx, LKIT_INT);

    } else if (strcmp(pname, "float") == 0) {
        res = lkit_dpexpr_new(mctx, LKIT_FLOAT);

    } else if (strcmp(pname, "str") == 0) {
        /*
         * :parser quoted|opt-quoted
         */
        res = lkit_dpexpr_new(mctx, LKIT_STR);
        if (form != NULL) {
            if (lparse_quals(form,
                             &it,
                             (quals_parser_t)parse_dpstr_quals,
                             res) != 0) {
                TR(LKIT_DPEXPR_PARSE + 10);
                goto err;
            }
        }

    } else if (strcmp(pname, "array") == 0) {
        /*
         * :parser delim|mdelim|smartdelim|none
         * :reserve N
         * fpa
         */

        if (form != NULL) {
            lkit_dparray_t *dpa;
            lkit_dpexpr_t **fpa;
            fparser_datum_t **node;
            lkit_array_t *ta;
            lkit_type_t **fty;

            dpa = (lkit_dparray_t *)lkit_dpexpr_new(mctx, LKIT_ARRAY);
            res = (lkit_dpexpr_t *)dpa;

            if (lparse_quals(form,
                             &it,
                             (quals_parser_t)parse_dparray_quals,
                             dpa) != 0) {
                TR(LKIT_DPEXPR_PARSE + 20);
                goto err;
            }
            if ((fpa = array_incr(&dpa->fields)) == NULL) {
                FAIL("array_incr");
            }
            if ((node = array_next(form, &it)) == NULL) {
                TR(LKIT_DPEXPR_PARSE + 21);
                goto err;
            }
            if ((*fpa = lkit_dpexpr_parse(mctx,
                                          *node,
                                          udata,
                                          seterror)) == NULL) {
                TR(LKIT_DPEXPR_PARSE + 22);
                goto err;
            }

            ta = (lkit_array_t *)lkit_type_get(mctx, LKIT_ARRAY);
            if ((fty = array_incr(&ta->fields)) == NULL) {
                FAIL("array_incr");
            }
            *fty = LKIT_PARSER_GET_TYPE((*fpa)->ty);

            dpa->base.ty = (lkit_parser_t *)lkit_type_get(mctx, LKIT_PARSER);
            dpa->base.ty->ty = lkit_type_finalize(mctx, (lkit_type_t *)ta);


        } else {
            TR(LKIT_DPEXPR_PARSE + 23);
            goto err;
        }

    } else if (strcmp(pname, "dict") == 0) {
        /*
         * :parser delim|smartdelim|delim-opt-quoted|none
         */
        if (form != NULL) {
            lkit_dpdict_t *dpd;
            lkit_dpexpr_t **fpa;
            fparser_datum_t **node;
            lkit_dict_t *td;
            lkit_type_t **fty;

            dpd = (lkit_dpdict_t *)lkit_dpexpr_new(mctx, LKIT_DICT);
            res = (lkit_dpexpr_t *)dpd;

            if (lparse_quals(form,
                             &it,
                             (quals_parser_t)parse_dpdict_quals,
                             dpd) != 0) {
                TR(LKIT_DPEXPR_PARSE + 30);
                goto err;
            }
            if ((fpa = array_incr(&dpd->fields)) == NULL) {
                FAIL("array_incr");
            }
            if ((node = array_next(form, &it)) == NULL) {
                TR(LKIT_DPEXPR_PARSE + 31);
                goto err;
            }
            if ((*fpa = lkit_dpexpr_parse(mctx,
                                          *node,
                                          udata,
                                          seterror)) == NULL) {
                TR(LKIT_DPEXPR_PARSE + 32);
                goto err;
            }

            td = (lkit_dict_t *)lkit_type_get(mctx, LKIT_DICT);
            if ((fty = array_incr(&td->fields)) == NULL) {
                FAIL("array_incr");
            }
            *fty = LKIT_PARSER_GET_TYPE((*fpa)->ty);

            dpd->base.ty = (lkit_parser_t *)lkit_type_get(mctx, LKIT_PARSER);
            dpd->base.ty->ty = lkit_type_finalize(mctx, (lkit_type_t *)td);

        } else {
            TR(LKIT_DPEXPR_PARSE + 33);
            goto err;
        }

    } else if (strcmp(pname, "struct") == 0) {
        /*
         * :parser delim|mdelim|none
         */
        if (form != NULL) {
            lkit_dpstruct_t *dps;
            lkit_struct_t *ts;
            fparser_datum_t **node;
            array_t *custom_fields;

            dps = (lkit_dpstruct_t *)lkit_dpexpr_new(mctx, LKIT_STRUCT);
            res = (lkit_dpexpr_t *)dps;

            if (lparse_quals(form,
                             &it,
                             (quals_parser_t)parse_dpstruct_quals,
                             dps) != 0) {
                TR(LKIT_DPEXPR_PARSE + 40);
                goto err;
            }

            ts = (lkit_struct_t *)lkit_type_get(mctx, LKIT_STRUCT);

            for (node = array_next(form, &it);
                 node != NULL;
                 node = array_next(form, &it)) {

                if (FPARSER_DATUM_TAG(*node) != FPARSER_SEQ) {
                    lkit_type_t *ty;

                    ty = (lkit_type_t *)ts;
                    lkit_type_destroy(&ty);
                    TR(LKIT_DPEXPR_PARSE + 41);
                    goto err;
                }
                if (parse_fielddef(mctx,
                                   *node,
                                   dps,
                                   ts,
                                   udata,
                                   seterror) != 0) {
                    lkit_type_t *ty;

                    ty = (lkit_type_t *)ts;
                    lkit_type_destroy(&ty);
                    TR(LKIT_DPEXPR_PARSE + 42);
                    goto err;
                }
            }

            custom_fields = udata;

            if (custom_fields != NULL && custom_fields->elnum > 0) {
                array_iter_t it0;
                bytes_t **cname;

                for (cname = array_first(custom_fields, &it0);
                     cname != NULL;
                     cname = array_next(custom_fields, &it0)) {

                    array_iter_t it1;
                    bytes_t **fname0;

                    it1 = it0;
                    for (fname0 = array_get_iter(&ts->names, &it1);
                         fname0 != NULL;
                         fname0 = array_next(&ts->names, &it1)) {

                        if (bytes_cmp(*cname, *fname0) == 0) {
                            assert(it0.iter <= it1.iter);

                            if (it0.iter < it1.iter) {
                                bytes_t **fname1, *tmp;
                                lkit_type_t **fty0, **fty1, *ftmp;
                                lkit_dpexpr_t **dpa0, **dpa1, *dpatmp;
                                /*
                                 * swap ts
                                 */
                                fname1 = array_get(&ts->names,
                                                    it0.iter);
                                tmp = *fname1;
                                *fname1 = *fname0;
                                *fname0 = tmp;

                                fty0 = array_get(&ts->fields,
                                                 it1.iter);
                                fty1 = array_get(&ts->fields,
                                                 it0.iter);
                                ftmp = *fty1;
                                *fty1 = *fty0;
                                *fty0 = ftmp;

                                /*
                                 * swap dps
                                 */
                                dpa0 = array_get(&dps->fields,
                                                 it1.iter);
                                dpa1 = array_get(&dps->fields,
                                                 it0.iter);
                                dpatmp = *dpa1;
                                *dpa1 = *dpa0;
                                *dpa0 = dpatmp;
                            }

                            break;
                        }
                    }
                }
            }

            dps->base.ty = (lkit_parser_t *)lkit_type_get(mctx, LKIT_PARSER);
            dps->base.ty->ty = lkit_type_finalize(mctx, (lkit_type_t *)ts);

        } else {
            TR(LKIT_DPEXPR_PARSE + 43);
            goto err;
        }

    } else {
        lkit_parser_t *ty;
        lkit_type_t *pty;
        lkit_dpexpr_t *dpty;

        if ((ty = (lkit_parser_t *)lkit_typedef_get(mctx, b)) == NULL) {
            TRACE("unknown parser: %s", pname);
            goto err;
        }

        if (ty->base.tag != LKIT_PARSER) {
            TRACE("incorrect parser type: %s", pname);
            goto err;
        }

        pty = LKIT_PARSER_GET_TYPE(ty);

        dpty = lkit_dpexpr_new(mctx, pty->tag);
        dpty->ty = ty;
        res = dpty;
    }

    res->ty = (lkit_parser_t *)lkit_type_finalize(mctx, (lkit_type_t *)res->ty);

end:
    return res;

err:
    dat->error = seterror;
    if (res != NULL) {
        lkit_dpexpr_destroy(&res);
    }
    goto end;
}


lkit_dpexpr_t *
lkit_dpstruct_get_field_parser(lkit_dpstruct_t *dps, int idx)
{
    lkit_dpexpr_t **pdpty;

    if ((pdpty = array_get(&dps->fields, idx)) == NULL) {
        FAIL("array_get");
    }
    return *pdpty;
}


lkit_dpexpr_t *
lkit_dpdict_get_element_parser(lkit_dpdict_t *dpd)
{
    lkit_dpexpr_t **pdpty;

    if ((pdpty = array_get(&dpd->fields, 0)) == NULL) {
        FAIL("array_get");
    }
    return *pdpty;
}


lkit_dpexpr_t *
lkit_dparray_get_element_parser(lkit_dparray_t *dpa)
{
    lkit_dpexpr_t **pdpty;

    if ((pdpty = array_get(&dpa->fields, 0)) == NULL) {
        FAIL("array_get");
    }
    return *pdpty;
}


lkit_dpexpr_t *
lkit_dpexpr_find(mrklkit_ctx_t *mctx, bytes_t *name, void *udata)
{
    mrklkit_module_t **mod;
    array_iter_t it;
    lkit_dpexpr_t *res;

    res = NULL;
    for (mod = array_first(&mctx->modules, &it);
         mod != NULL;
         mod = array_next(&mctx->modules, &it)) {

        if ((*mod)->dpexpr_find != NULL) {
            if ((res = (*mod)->dpexpr_find(mctx, name, udata)) != NULL) {
                break;
            }
        }
    }
    return res;
}


void
dpexpr_init(void)
{
}

void
dpexpr_fini(void)
{
}
