#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/dumpm.h>
#include <mrkcommon/array.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lparse.h>

#include "diag.h"

/* types */
static array_t types;

/**
 * type
 *
 */

static int
builtin_type_destroy(lkit_type_t **ty)
{
    if ((*ty) != NULL) {
        free(*ty);
        *ty = NULL;
    }
    return 0;
}


int
lkit_type_destroy(lkit_type_t **ty)
{

    if (*ty != NULL) {
        lkit_tag_t tag;
        tag = (*ty)->tag;
        if (tag < _LKIT_END_OF_BUILTIN_TYPES) {
            LKIT_ERROR(*ty) = 1;
            return 1;
        }

        if (((*ty)->name) != NULL) {
            /* weak ref */
            //free((*ty)->name);
            (*ty)->name = NULL;
        }

        switch((*ty)->tag) {
            lkit_array_t *ta;
            lkit_dict_t *td;
            lkit_struct_t *ts;
            lkit_func_t *tf;

        case LKIT_ARRAY:
            ta = (lkit_array_t *)*ty;
            if (ta->delim != NULL) {
                /* weak ref */
                //free(ta->delim);
                ta->delim = NULL;
            }
            array_fini(&ta->fields);
            break;

        case LKIT_DICT:
            td = (lkit_dict_t *)*ty;
            if (td->kvdelim != NULL) {
                /* weak ref */
                //free(td->kvdelim);
                td->kvdelim = NULL;
            }
            if (td->fdelim != NULL) {
                /* weak ref */
                //free(td->fdelim);
                td->fdelim = NULL;
            }
            array_fini(&td->fields);
            break;

        case LKIT_STRUCT:
            ts = (lkit_struct_t *)*ty;
            if (ts->delim != NULL) {
                /* weak ref */
                //free(ts->delim);
                ts->delim = NULL;
            }
            array_fini(&ts->fields);
            array_fini(&ts->names);
            break;

        case LKIT_FUNC:
            tf = (lkit_func_t *)*ty;
            array_fini(&tf->fields);
            break;

        default:
            FAIL("type_destroy");

        }

        free(*ty);
        *ty = NULL;
    }
    return 0;
}

static lkit_type_t *
type_new(lkit_tag_t tag)
{
    lkit_type_t *ty = NULL;

    if (tag < _LKIT_END_OF_BUILTIN_TYPES) {
        lkit_type_t **pty;

        if ((pty = array_get(&types, tag)) == NULL) {
            FAIL("array_get");
        }
        return *pty;
    }

    switch (tag) {
        lkit_array_t *ta;
        lkit_dict_t *td;
        lkit_struct_t *ts;
        lkit_func_t *tf;

    case LKIT_ARRAY:
        ta = (lkit_array_t *)ty;
        if ((ta = malloc(sizeof(lkit_array_t))) == NULL) {
            FAIL("malloc");
        }
        ta->base.tag = tag;
        ta->base.name = "array";
        LKIT_ERROR(ta) = 0;
        ta->parser = LKIT_PARSER_NONE;
        ta->delim = NULL;
        array_init(&ta->fields, sizeof(lkit_type_t *), 0,
                   NULL, (array_finalizer_t)lkit_type_destroy);
        ty = (lkit_type_t *)ta;
        break;

    case LKIT_DICT:
        td = (lkit_dict_t *)ty;
        if ((td = malloc(sizeof(lkit_dict_t))) == NULL) {
            FAIL("malloc");
        }
        td->base.tag = tag;
        td->base.name = "name";
        LKIT_ERROR(td) = 0;
        td->kvdelim = NULL;
        td->fdelim = NULL;
        array_init(&td->fields, sizeof(lkit_type_t *), 0,
                   NULL, (array_finalizer_t)lkit_type_destroy);
        ty = (lkit_type_t *)td;
        break;

    case LKIT_STRUCT:
        ts = (lkit_struct_t *)ty;
        if ((ts = malloc(sizeof(lkit_struct_t))) == NULL) {
            FAIL("malloc");
        }
        ts->base.tag = tag;
        ts->base.name = "struct";
        LKIT_ERROR(ts) = 0;
        ts->parser = LKIT_PARSER_NONE;
        ts->delim = NULL;
        array_init(&ts->fields, sizeof(lkit_type_t *), 0,
                   NULL, (array_finalizer_t)lkit_type_destroy);
        array_init(&ts->names, sizeof(unsigned char *), 0,
                   NULL, NULL);
        ty = (lkit_type_t *)ts;
        break;

    case LKIT_FUNC:
        tf = (lkit_func_t *)ty;
        if ((tf = malloc(sizeof(lkit_func_t))) == NULL) {
            FAIL("malloc");
        }
        tf->base.tag = tag;
        tf->base.name = "func";
        LKIT_ERROR(tf) = 0;
        array_init(&tf->fields, sizeof(lkit_type_t *), 0,
                   NULL, (array_finalizer_t)lkit_type_destroy);
        ty = (lkit_type_t *)tf;
        break;

    default:
        FAIL("type_new");

    }

    return ty;
}

static lkit_type_t *
lkit_array_get_element_type(lkit_array_t *ta)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    if ((ty = array_first(&ta->fields, &it)) == NULL) {
        return NULL;
    }
    return *ty;
}

UNUSED static lkit_type_t *
lkit_dict_get_element_type(lkit_dict_t *td)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    if ((ty = array_first(&td->fields, &it)) == NULL) {
        return NULL;
    }
    return *ty;
}


void
lkit_type_dump(lkit_type_t *ty)
{
    if (ty == NULL) {
        LTRACEN(0, " <null>");
        return;
    }
    if (ty->tag < _LKIT_END_OF_BUILTIN_TYPES) {
        LTRACEN(0, "%s", ty->name);
    } else {
        //LTRACEN(0, " (%s", ty->name);
        if (LKIT_ERROR(ty)) {
            TRACEC("-->(%s", ty->name);
        } else {
            TRACEC("(%s", ty->name);
        }
        switch (ty->tag) {
            lkit_type_t **elty;
            array_iter_t it;
            lkit_array_t *ta;
            //lkit_dict_t *td;
            lkit_struct_t *ts;
            //lkit_func_t *tf;
            unsigned char *dst;
            size_t sz;

        case LKIT_ARRAY:
            ta = (lkit_array_t *)ty;
            sz = strlen((char *)(ta->delim));
            if ((dst = malloc(sz * 2 + 1)) == NULL) {
                FAIL("malloc");
            }
            memset(dst, '\0', sz * 2 + 1);
            fparser_escape(dst, sz * 2, ta->delim, sz);
            TRACEC(" \"%s\"", dst);
            free(dst);
            lkit_type_dump(lkit_array_get_element_type(ta));
            break;

        case LKIT_STRUCT:
            ts = (lkit_struct_t *)ty;
            sz = strlen((char *)(ts->delim));
            if ((dst = malloc(sz * 2 + 1)) == NULL) {
                FAIL("malloc");
            }
            memset(dst, '\0', sz * 2 + 1);
            fparser_escape(dst, sz * 2, ts->delim, sz);
            TRACEC(" \"%s\"", dst);
            free(dst);

            for (elty = array_first(&ts->fields, &it);
                 elty != NULL;
                 elty = array_next(&ts->fields, &it)) {

                unsigned char **name;

                if ((name = array_get(&ts->names, it.iter)) != NULL) {
                    TRACEC(" (%s ", *name);
                    lkit_type_dump(*elty);
                    TRACEC(")");
                } else {
                    lkit_type_dump(*elty);
                }


            }
            break;


        default:
            FAIL("lkit_type_dump");
        }
        if (LKIT_ERROR(ty)) {
            TRACEC(")<--");
        } else {
            TRACEC(")");
        }
    }
}

int
ltype_next_struct(array_t *form, array_iter_t *it, lkit_struct_t **value, int seterror)
{
    lkit_type_t *ty;
    fparser_datum_t **tok;

    if ((tok = array_next(form, it)) == NULL) {
        return 1;
    }

    if ((ty = ltype_parse_type(*tok)) != NULL) {
        if (ty->tag == LKIT_STRUCT) {
            *value = (lkit_struct_t *)ty;
            return 0;
        }
    }
    array_prev(form, it);
    (*tok)->error = seterror;
    return 1;
}

/*
 * Parser.
 *
 */

UNUSED static int
type_cmp(lkit_type_t **pa, lkit_type_t **pb)
{
    lkit_type_t *a = *pa;
    lkit_type_t *b = *pb;

    if (a == b) {
        return 0;
    }
    if (a->tag < _LKIT_END_OF_BUILTIN_TYPES) {
        if (a->tag == b->tag) {
            return 0;
        }
    } else {
        if (a->tag == b->tag) {
            switch (a->tag) {
                lkit_array_t *aa, *ab;
                lkit_dict_t *da, *db;
                lkit_struct_t *sa, *sb;
                lkit_func_t *fa, *fb;

            case LKIT_ARRAY:
                aa = (lkit_array_t *)a;
                ab = (lkit_array_t *)b;

                return array_cmp(&aa->fields,
                                 &ab->fields,
                                 (array_compar_t)type_cmp,
                                 0);

                break;

            case LKIT_DICT:
                da = (lkit_dict_t *)a;
                db = (lkit_dict_t *)b;

                return array_cmp(&da->fields,
                                 &db->fields,
                                 (array_compar_t)type_cmp,
                                 0);

                break;

            case LKIT_STRUCT:
                sa = (lkit_struct_t *)a;
                sb = (lkit_struct_t *)b;

                return array_cmp(&sa->fields,
                                 &sb->fields,
                                 (array_compar_t)type_cmp,
                                 0);

                break;

            case LKIT_FUNC:
                fa = (lkit_func_t *)a;
                fb = (lkit_func_t *)b;

                return array_cmp(&fa->fields,
                                 &fb->fields,
                                 (array_compar_t)type_cmp,
                                 0);

                break;

            default:
                FAIL("type_eq");
            }
        }
    }
    return 1;
}

static int
parse_array_quals(array_t *form,
                   array_iter_t *it,
                   unsigned char *qual,
                   lkit_array_t *ta)
{
    char *s = (char *)qual;

    ta->parser = LKIT_PARSER_NONE;
    ta->delim = NULL;

    if (strcmp(s, ":parser") == 0) {
        unsigned char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ta->parser = LKIT_PARSER_DELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    return 1;
                }
            } else if (strcmp((char *) parser, "w3c") == 0) {
                ta->parser = LKIT_PARSER_W3C;
            } else if (strcmp((char *) parser, "none") == 0) {
                ta->parser = LKIT_PARSER_NONE;
            } else {
                ta->parser = -1;
                return 1;
            }
        } else {
            return 1;
        }
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

static int
parse_struct_quals(array_t *form,
                   array_iter_t *it,
                   unsigned char *qual,
                   lkit_struct_t *ts)
{
    char *s = (char *)qual;

    ts->parser = LKIT_PARSER_NONE;
    ts->delim = NULL;

    if (strcmp(s, ":parser") == 0) {
        unsigned char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ts->parser = LKIT_PARSER_DELIM;
                if (lparse_next_str(form, it, &ts->delim, 1) != 0) {
                    return 1;
                }
            } else if (strcmp((char *) parser, "w3c") == 0) {
                ts->parser = LKIT_PARSER_W3C;
            } else if (strcmp((char *) parser, "none") == 0) {
                ts->parser = LKIT_PARSER_NONE;
            } else {
                ts->parser = -1;
                return 1;
            }
        } else {
            return 1;
        }
    } else {
        TRACE("unknown qual: %s", s);
    }
    return 0;
}
/**
 * name ::= WORD
 * delim|delim1|delim2 ::= STR
 * simple_type ::= int | str | float
 * fielddef ::= (name type)
 * complex_type ::= (array delim type) |
 *                  (dict delim1 delim2 type) |
 *                  (struct delim *fielddef) |
 *                  (func fielddef *fielddef)
 *
 * type ::= simple_type | complex_type
 *
 */
static int
parse_fielddef(fparser_datum_t *dat, lkit_type_t *ty)
{
    array_t *form;
    array_iter_t it;
    unsigned char **name = NULL;

#define DO_PARSE_FIELDDEF(var, type) \
    type *var; \
    var = (type *)ty; \
    if ((name = array_incr(&var->names)) == NULL) { \
        FAIL("array_incr"); \
    } \
    form = (array_t *)dat->body; \
    if (lparse_first_word(form, &it, name, 1) == 0) { \
        fparser_datum_t **tok; \
        lkit_type_t **ty; \
        if ((tok = array_next(form, &it)) == NULL) { \
            LKIT_ERROR(var) = 1; \
            dat->error = 1; \
            return 1; \
        } \
        if ((ty = array_incr(&var->fields)) == NULL) { \
            FAIL("array_incr"); \
        } \
        if ((*ty = ltype_parse_type(*tok)) == NULL) { \
            LKIT_ERROR(var) = 1; \
            (*tok)->error = 1; \
            return 1; \
        } \
    } else { \
        LKIT_ERROR(var) = 1; \
        dat->error = 1; \
        return 1; \
    }

    if (ty->tag == LKIT_STRUCT) {
        DO_PARSE_FIELDDEF(ts, lkit_struct_t);
    } else {
        FAIL("parse_field");
    }

    return 0;
}

lkit_type_t *
ltype_parse_type(fparser_datum_t *dat)
{
    lkit_type_t *ty = NULL;
    fparser_tag_t tag;

    tag = FPARSER_DATUM_TAG(dat);
    if (tag == FPARSER_WORD) {
        bytes_t *b;
        char *typename;

        b = (bytes_t *)(dat->body);
        typename = (char *)(b->data);

        /* simple types */
        if (strcmp(typename, "int") == 0) {
            ty = type_new(LKIT_INT);
        } else if (strcmp(typename, "timestamp") == 0) {
            ty = type_new(LKIT_TIMESTAMP);
        } else if (strcmp(typename, "str") == 0) {
            ty = type_new(LKIT_STR);
        } else if (strcmp(typename, "float") == 0) {
            ty = type_new(LKIT_FLOAT);
        //} else if (strcmp(typename, "bool") == 0) {
        } else {
            goto err;
        }
    } else if (tag == FPARSER_SEQ) {
        array_t *form;
        array_iter_t it;
        unsigned char *first;

        form = (array_t *)dat->body;

        if (lparse_first_word(form, &it, &first, 1) == 0) {

            if (strcmp((char *)first, "array") == 0) {
                lkit_array_t *ta;
                fparser_datum_t **tok;
                lkit_type_t **elemtype;

                ty = type_new(LKIT_ARRAY);
                ta = (lkit_array_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_array_quals,
                                 ta) != 0) {
                    goto err;
                }

                if ((tok = array_next(form, &it)) == NULL) {
                    goto err;
                }

                if ((elemtype = array_incr(&ta->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = ltype_parse_type(*tok)) == NULL) {
                    goto err;
                }

            } else if (strcmp((char *)first, "dict") == 0) {
                lkit_dict_t *td;
                fparser_datum_t **tok;
                lkit_type_t **elemtype;

                ty = type_new(LKIT_DICT);
                td = (lkit_dict_t *)ty;

                if (lparse_next_str(form, &it, &td->kvdelim, 1) != 0) {
                    goto err;
                }

                if (lparse_next_str(form, &it, &td->fdelim, 1) != 0) {
                    goto err;
                }

                if ((tok = array_next(form, &it)) == NULL) {
                    goto err;
                }

                if ((elemtype = array_incr(&td->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = ltype_parse_type(*tok)) == NULL) {
                    goto err;
                }

            } else if (strcmp((char *)first, "struct") == 0) {
                lkit_struct_t *ts;
                fparser_datum_t **tok;

                ty = type_new(LKIT_STRUCT);
                ts = (lkit_struct_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_struct_quals,
                                 ts) != 0) {
                    goto err;
                }

                /* fields */
                for (tok = array_next(form, &it);
                     tok != NULL;
                     tok = array_next(form, &it)) {


                    if (FPARSER_DATUM_TAG(*tok) == FPARSER_SEQ) {
                        //if (parse_fielddef_struct(*tok, ts) != 0) {
                        if (parse_fielddef(*tok, ty) != 0) {
                            goto err;
                        }
                    } else {
                        goto err;
                    }
                }

            } else if (strcmp((char *)first, "func") == 0) {
                lkit_func_t *tf;
                fparser_datum_t **tok;

                ty = type_new(LKIT_FUNC);
                tf = (lkit_func_t *)ty;

                /* retval and optional params are stroed in tf->fields */
                for (tok = array_next(form, &it);
                     tok != NULL;
                     tok = array_next(form, &it)) {

                    lkit_type_t **paramtype;

                    if ((paramtype = array_incr(&tf->fields)) == NULL) {
                        FAIL("array_incr");
                    }

                    if ((*paramtype = ltype_parse_type(*tok)) == NULL) {
                        goto err;
                    }
                }

            } else {
            }
        }
    }

end:
    return ty;

err:
    dat->error = 1;
    if (ty != NULL) {
        lkit_type_destroy(&ty);
    }
    goto end;
}

int
lkit_type_traverse(lkit_type_t *ty, lkit_type_traverser_t cb, void *udata)
{
    if (cb(ty, udata) != 0) {
        return 1;
    }
    switch (ty->tag) {
        array_iter_t it;
        lkit_type_t **pty;
        lkit_array_t *ta;
        lkit_dict_t *td;
        lkit_struct_t *ts;
        lkit_func_t *tf;

    case LKIT_ARRAY:
        ta = (lkit_array_t *)ty;

        for (pty = array_first(&ta->fields, &it);
             pty != NULL;
             pty = array_next(&ta->fields, &it)) {
            if (cb(*pty, udata) != 0) {
                return 1;
            }
        }

        break;

    case LKIT_DICT:
        td = (lkit_dict_t *)ty;

        for (pty = array_first(&td->fields, &it);
             pty != NULL;
             pty = array_next(&td->fields, &it)) {
            if (cb(*pty, udata) != 0) {
                return 1;
            }
        }

        break;

    case LKIT_STRUCT:
        ts = (lkit_struct_t *)ty;

        for (pty = array_first(&ts->fields, &it);
             pty != NULL;
             pty = array_next(&ts->fields, &it)) {
            if (cb(*pty, udata) != 0) {
                return 1;
            }
        }

        break;

    case LKIT_FUNC:
        tf = (lkit_func_t *)ty;

        for (pty = array_first(&tf->fields, &it);
             pty != NULL;
             pty = array_next(&tf->fields, &it)) {
            if (cb(*pty, udata) != 0) {
                return 1;
            }
        }

        break;

    default:
        FAIL("ltype_traverse");

    }
    return 0;
}

void
ltype_init(void)
{
    lkit_type_t **pty;
    lkit_int_t *ti;
    lkit_timestamp_t *tt;
    lkit_float_t *tf;
    lkit_str_t *ts;

    array_init(&types, sizeof(lkit_type_t *), 0,
               NULL, (array_finalizer_t)builtin_type_destroy);

    if ((ti = malloc(sizeof(lkit_int_t))) == NULL) {
        FAIL("malloc");
    }

    ti->base.tag = LKIT_INT;
    ti->base.name = "int";
    LKIT_ERROR(ti) = 0;
    if ((pty = array_incr(&types)) == NULL) {
        FAIL("array_incr");
    }
    *pty = (lkit_type_t *)ti;

    if ((tt = malloc(sizeof(lkit_timestamp_t))) == NULL) {
        FAIL("malloc");
    }

    tt->base.tag = LKIT_TIMESTAMP;
    tt->base.name = "timestamp";
    LKIT_ERROR(tt) = 0;
    if ((pty = array_incr(&types)) == NULL) {
        FAIL("array_incr");
    }
    *pty = (lkit_type_t *)tt;

    if ((ts = malloc(sizeof(lkit_str_t))) == NULL) {
        FAIL("malloc");
    }

    ts->base.tag = LKIT_STR;
    ts->base.name = "str";
    LKIT_ERROR(ts) = 0;
    if ((pty = array_incr(&types)) == NULL) {
        FAIL("array_incr");
    }
    *pty = (lkit_type_t *)ts;

    if ((tf = malloc(sizeof(lkit_float_t))) == NULL) {
        FAIL("malloc");
    }

    tf->base.tag = LKIT_FLOAT;
    tf->base.name = "float";
    LKIT_ERROR(tf) = 0;
    if ((pty = array_incr(&types)) == NULL) {
        FAIL("array_incr");
    }
    *pty = (lkit_type_t *)tf;

}

void
ltype_fini(void)
{
    array_fini(&types);
}
