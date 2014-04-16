#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dict.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/fasthash.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lparse.h>
/* dtors */
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

/*
 * lkit_type_t *, lkit_type_t *
 */
static dict_t types;
/*
 * bytes_t *, lkit_type_t *
 */
static dict_t typedefs;

int
lkit_type_destroy(lkit_type_t **ty)
{
    if (*ty != NULL) {
        (*ty)->hash = 0;
        (*ty)->backend = NULL;

        if (((*ty)->name) != NULL) {
            /* weak ref */
            //free((*ty)->name);
            (*ty)->name = NULL;
        }

        switch((*ty)->tag) {
        case LKIT_ARRAY:
            {
                lkit_array_t *ta;
                ta = (lkit_array_t *)*ty;
                if (ta->delim != NULL) {
                    /* weak ref */
                    //free(ta->delim);
                    ta->delim = NULL;
                }
                array_fini(&ta->fields);
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *td;
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
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *ts;
                ts = (lkit_struct_t *)*ty;
                if (ts->delim != NULL) {
                    /* weak ref */
                    //free(ts->delim);
                    ts->delim = NULL;
                }
                array_fini(&ts->fields);
                array_fini(&ts->names);
            }
            break;

        case LKIT_FUNC:
            {
                lkit_func_t *tf;
                tf = (lkit_func_t *)*ty;
                array_fini(&tf->fields);
            }
            break;

        default:
            /* builtin? */
            assert((*ty)->tag < _LKIT_END_OF_BUILTIN_TYPES);

        }

        free(*ty);
        *ty = NULL;
    }
    return 0;
}

int
lkit_type_fini_dict(lkit_type_t *key, UNUSED lkit_type_t *value)
{
    lkit_type_destroy(&key);
    return 0;
}

lkit_type_t *
lkit_type_new(lkit_tag_t tag)
{
    lkit_type_t *ty = NULL;

    switch (tag) {
    case LKIT_UNDEF:
        if ((ty = malloc(sizeof(lkit_type_t))) == NULL) {
            FAIL("malloc");
        }
        ty->tag = tag;
        ty->name = "undef";
        ty->error = 0;
        ty->dtor = NULL;
        break;

    case LKIT_INT:
        {
            lkit_int_t *ti;
            ti = (lkit_int_t *)ty;
            if ((ti = malloc(sizeof(lkit_int_t))) == NULL) {
                FAIL("malloc");
            }
            ti->base.tag = tag;
            ti->base.name = "int";
            LKIT_ERROR(ti) = 0;
            ti->base.dtor = NULL;
            ty = (lkit_type_t *)ti;
        }
        break;

    case LKIT_STR:
        {
            lkit_str_t *tc;
            tc = (lkit_str_t *)ty;
            if ((tc = malloc(sizeof(lkit_str_t))) == NULL) {
                FAIL("malloc");
            }
            tc->base.tag = tag;
            tc->base.name = "str";
            LKIT_ERROR(tc) = 0;
            tc->base.dtor = NULL;
            ty = (lkit_type_t *)tc;
        }
        break;

    case LKIT_FLOAT:
        {
            lkit_float_t *tg;
            tg = (lkit_float_t *)ty;
            if ((tg = malloc(sizeof(lkit_float_t))) == NULL) {
                FAIL("malloc");
            }
            tg->base.tag = tag;
            tg->base.name = "float";
            LKIT_ERROR(tg) = 0;
            tg->base.dtor = NULL;
            ty = (lkit_type_t *)tg;
        }
        break;

    case LKIT_BOOL:
        {
            lkit_bool_t *tb;
            tb = (lkit_bool_t *)ty;
            if ((tb = malloc(sizeof(lkit_bool_t))) == NULL) {
                FAIL("malloc");
            }
            tb->base.tag = tag;
            tb->base.name = "bool";
            LKIT_ERROR(tb) = 0;
            tb->base.dtor = NULL;
            ty = (lkit_type_t *)tb;
        }
        break;

    case LKIT_VARARG:
        {
            lkit_vararg_t *tv;
            tv = (lkit_vararg_t *)ty;
            if ((tv = malloc(sizeof(lkit_vararg_t))) == NULL) {
                FAIL("malloc");
            }
            tv->base.tag = tag;
            tv->base.name = "...";
            LKIT_ERROR(tv) = 0;
            tv->base.dtor = NULL;
            ty = (lkit_type_t *)tv;
        }
        break;

    case LKIT_ARRAY:
        {
            lkit_array_t *ta;
            ta = (lkit_array_t *)ty;
            if ((ta = malloc(sizeof(lkit_array_t))) == NULL) {
                FAIL("malloc");
            }
            ta->base.tag = tag;
            ta->base.name = "array";
            LKIT_ERROR(ta) = 0;
            ta->parser = LKIT_PARSER_NONE;
            ta->delim = NULL;
            array_init(&ta->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ta->base.dtor = mrklkit_rt_array_dtor;
            ty = (lkit_type_t *)ta;
        }
        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            td = (lkit_dict_t *)ty;
            if ((td = malloc(sizeof(lkit_dict_t))) == NULL) {
                FAIL("malloc");
            }
            td->base.tag = tag;
            td->base.name = "dict";
            LKIT_ERROR(td) = 0;
            td->kvdelim = NULL;
            td->fdelim = NULL;
            td->base.dtor = mrklkit_rt_dict_dtor;
            array_init(&td->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ty = (lkit_type_t *)td;
        }
        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            ts = (lkit_struct_t *)ty;
            if ((ts = malloc(sizeof(lkit_struct_t))) == NULL) {
                FAIL("malloc");
            }
            ts->base.tag = tag;
            ts->base.name = "struct";
            LKIT_ERROR(ts) = 0;
            ts->parser = LKIT_PARSER_NONE;
            ts->delim = NULL;
            ts->base.dtor = mrklkit_rt_array_dtor;
            array_init(&ts->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            array_init(&ts->names, sizeof(unsigned char *), 0, NULL, NULL);
            ty = (lkit_type_t *)ts;
        }
        break;

    case LKIT_FUNC:
        {
            lkit_func_t *tf;
            tf = (lkit_func_t *)ty;
            if ((tf = malloc(sizeof(lkit_func_t))) == NULL) {
                FAIL("malloc");
            }
            tf->base.tag = tag;
            tf->base.name = "func";
            tf->base.dtor = NULL;
            LKIT_ERROR(tf) = 0;
            array_init(&tf->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ty = (lkit_type_t *)tf;
        }
        break;

    default:
        FAIL("type_new");

    }

    ty->hash = 0;
    ty->backend = NULL;

    return ty;
}

lkit_type_t *
lkit_array_get_element_type(lkit_array_t *ta)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    if ((ty = array_first(&ta->fields, &it)) == NULL) {
        return NULL;
    }
    return *ty;
}

lkit_type_t *
lkit_dict_get_element_type(lkit_dict_t *td)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    if ((ty = array_first(&td->fields, &it)) == NULL) {
        return NULL;
    }
    return *ty;
}

lkit_type_t *
lkit_struct_get_field_type(lkit_struct_t *ts, bytes_t *name)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    for (ty = array_first(&ts->fields, &it);
         ty != NULL;
         ty = array_next(&ts->fields, &it)) {

        bytes_t **fname;

        if ((fname = array_get(&ts->names, it.iter)) == NULL) {
            FAIL("array_get");
        }
        if (bytes_cmp(name, *fname) == 0) {
            return *ty;
        }
    }

    return *ty;
}

int
lkit_struct_get_field_index(lkit_struct_t *ts, bytes_t *name)
{
    array_iter_t it;
    lkit_type_t **ty = NULL;

    for (ty = array_first(&ts->fields, &it);
         ty != NULL;
         ty = array_next(&ts->fields, &it)) {

        bytes_t **fname;

        if ((fname = array_get(&ts->names, it.iter)) == NULL) {
            FAIL("array_get");
        }
        if (bytes_cmp(name, *fname) == 0) {
            return it.iter;
        }
    }

    return -1;
}


static void
_lkit_type_dump(lkit_type_t *ty, int level)
{
    if (ty == NULL) {
        LTRACEN(0, " <null>");
        return;
    }

    LTRACEN(0, "%s", ty->name);

    switch (ty->tag) {
    case LKIT_ARRAY:
        {
            lkit_array_t *ta;
            unsigned char *dst;
            size_t sz;

            ta = (lkit_array_t *)ty;
            if (ta->delim != NULL) {
                sz = strlen((char *)(ta->delim));
                if ((dst = malloc(sz * 2 + 1)) == NULL) {
                    FAIL("malloc");
                }
                memset(dst, '\0', sz * 2 + 1);
                fparser_escape(dst, sz * 2, ta->delim, sz);
                TRACEC("\"%s\" ", dst);
                free(dst);
            }
            _lkit_type_dump(lkit_array_get_element_type(ta), level + 1);
        }
        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            unsigned char *dst;
            size_t sz;

            td = (lkit_dict_t *)ty;
            if (td->kvdelim != NULL) {
                sz = strlen((char *)(td->kvdelim));
                if ((dst = malloc(sz * 2 + 1)) == NULL) {
                    FAIL("malloc");
                }
                memset(dst, '\0', sz * 2 + 1);
                fparser_escape(dst, sz * 2, td->kvdelim, sz);
                TRACEC("\"%s\" ", dst);
                free(dst);
            }
            if (td->fdelim != NULL) {
                sz = strlen((char *)(td->fdelim));
                if ((dst = malloc(sz * 2 + 1)) == NULL) {
                    FAIL("malloc");
                }
                memset(dst, '\0', sz * 2 + 1);
                fparser_escape(dst, sz * 2, td->fdelim, sz);
                TRACEC("\"%s\" ", dst);
                free(dst);
            }
            _lkit_type_dump(lkit_dict_get_element_type(td), level + 1);
        }
        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            unsigned char *dst;
            size_t sz;
            lkit_type_t **elty;
            array_iter_t it;

            ts = (lkit_struct_t *)ty;
            if (ts->delim != NULL) {
                sz = strlen((char *)(ts->delim));
                if ((dst = malloc(sz * 2 + 1)) == NULL) {
                    FAIL("malloc");
                }
                memset(dst, '\0', sz * 2 + 1);
                fparser_escape(dst, sz * 2, ts->delim, sz);
                TRACEC("\"%s\" ", dst);
                free(dst);
            }

            for (elty = array_first(&ts->fields, &it);
                 elty != NULL;
                 elty = array_next(&ts->fields, &it)) {

                unsigned char **name;

                if ((name = array_get(&ts->names, it.iter)) != NULL) {
                    TRACEC("%s:", *name);
                    _lkit_type_dump(*elty, level + 1);
                } else {
                    _lkit_type_dump(*elty, level + 1);
                }


            }
        }
        break;


    case LKIT_FUNC:
        {
            lkit_type_t **elty;
            array_iter_t it;
            lkit_func_t *tf;

            tf = (lkit_func_t *)ty;
            for (elty = array_first(&tf->fields, &it);
                 elty != NULL;
                 elty = array_next(&tf->fields, &it)) {
                _lkit_type_dump(*elty, level + 1);
            }
        }
        break;


    default:
        /* builtin */
        break;
    }
}

void
lkit_type_dump(lkit_type_t *ty)
{
    _lkit_type_dump(ty, 0);
    TRACEC("\n");
}

void
lkit_type_str(lkit_type_t *ty, bytestream_t *bs)
{

    if (ty == NULL) {
        bytestream_cat(bs, 6, "<null>");
        return;
    }
    if (ty->tag < _LKIT_END_OF_BUILTIN_TYPES) {
        bytestream_nprintf(bs, strlen(ty->name) + 2, "%s ", ty->name);

    } else {
        bytestream_nprintf(bs, strlen(ty->name) + 3, "(%s ", ty->name);
        switch (ty->tag) {

        case LKIT_ARRAY:
            {
                lkit_array_t *ta;

                ta = (lkit_array_t *)ty;
                lkit_type_str(lkit_array_get_element_type(ta), bs);
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *td;

                td = (lkit_dict_t *)ty;
                lkit_type_str(lkit_dict_get_element_type(td), bs);
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *ts;
                lkit_type_t **elty;
                array_iter_t it;

                ts = (lkit_struct_t *)ty;
                for (elty = array_first(&ts->fields, &it);
                     elty != NULL;
                     elty = array_next(&ts->fields, &it)) {

                    lkit_type_str(*elty, bs);
                }
            }
            break;

        case LKIT_FUNC:
            {
                lkit_func_t *tf;
                lkit_type_t **elty;
                array_iter_t it;

                tf = (lkit_func_t *)ty;
                for (elty = array_first(&tf->fields, &it);
                     elty != NULL;
                     elty = array_next(&tf->fields, &it)) {

                    lkit_type_str(*elty, bs);
                }
            }
            break;


        default:
            FAIL("lkit_type_str");
        }
        bytestream_cat(bs, 1, ")");
    }
}

int
ltype_next_struct(array_t *form, array_iter_t *it, lkit_struct_t **value, int seterror)
{
    lkit_type_t *ty;
    fparser_datum_t **node;

    if ((node = array_next(form, it)) == NULL) {
        TRRET(LTYPE_NEXT_STRUCT + 1);
    }

    if ((ty = lkit_type_parse(*node, seterror)) != NULL) {
        if (ty->tag == LKIT_STRUCT) {
            *value = (lkit_struct_t *)ty;
            TRRET(0);
        }
    }
    array_prev(form, it);
    (*node)->error = seterror;
    TRRET(LTYPE_NEXT_STRUCT + 2);
}

/*
 * Parser.
 *
 */

static int
type_cmp(lkit_type_t **pa, lkit_type_t **pb)
{
    lkit_type_t *a = *pa;
    lkit_type_t *b = *pb;
    int diff;

    if (a == b) {
        TRRET(0);
    }

    diff = a->tag - b->tag;

    if (diff == 0) {
        /* deep compare of custom types by their field content */
        if (a->tag > _LKIT_END_OF_BUILTIN_TYPES) {

            switch (a->tag) {

            case LKIT_ARRAY:
                {
                    lkit_array_t *aa, *ab;
                    aa = (lkit_array_t *)a;
                    ab = (lkit_array_t *)b;

                    return array_cmp(&aa->fields,
                                     &ab->fields,
                                     (array_compar_t)type_cmp,
                                     0);

                }
                break;

            case LKIT_DICT:
                {
                    lkit_dict_t *da, *db;
                    da = (lkit_dict_t *)a;
                    db = (lkit_dict_t *)b;

                    return array_cmp(&da->fields,
                                     &db->fields,
                                     (array_compar_t)type_cmp,
                                     0);

                }
                break;

            case LKIT_STRUCT:
                {
                    lkit_struct_t *sa, *sb;
                    sa = (lkit_struct_t *)a;
                    sb = (lkit_struct_t *)b;

                    return array_cmp(&sa->fields,
                                     &sb->fields,
                                     (array_compar_t)type_cmp,
                                     0);

                }
                break;

            case LKIT_FUNC:
                {
                    lkit_func_t *fa, *fb;
                    fa = (lkit_func_t *)a;
                    fb = (lkit_func_t *)b;

                    return array_cmp(&fa->fields,
                                     &fb->fields,
                                     (array_compar_t)type_cmp,
                                     0);

                }
                break;

            default:
                FAIL("type_cmp");
            }
        }
    }

    TRRET(diff);
}

uint64_t
lkit_type_hash(lkit_type_t *ty)
{
    if (ty == NULL) {
        return 0;
    }
    if (ty->hash == 0) {
        bytestream_t bs;
        bytestream_init(&bs);
        lkit_type_str(ty, &bs);
        ty->hash = fasthash(0,
            (const unsigned char *)SDATA(&bs, 0), SEOD(&bs));
        bytestream_fini(&bs);
    }
    return ty->hash;
}

int
lkit_type_cmp(lkit_type_t *a, lkit_type_t *b)
{
    uint64_t ha, hb;
    int diff;

    ha = lkit_type_hash(a);
    hb = lkit_type_hash(b);

    diff = (int)(ha - hb);

    if (diff) {
        return type_cmp(&a, &b);
    }
    return diff;
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
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "w3c") == 0) {
                ta->parser = LKIT_PARSER_W3C;
            } else if (strcmp((char *) parser, "none") == 0) {
                ta->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                ta->parser = -1;
                TRRET(PARSE_ARRAY_QUALS + 2);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_ARRAY_QUALS + 3);
        }
    } else {
        /* unknown array qualifier */
        TRACE("unknown qual: %s", s);
    }
    TRRET(0);
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
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "w3c") == 0) {
                ts->parser = LKIT_PARSER_W3C;
            } else if (strcmp((char *) parser, "none") == 0) {
                ts->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                ts->parser = -1;
                TRRET(PARSE_STRUCT_QUALS + 2);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_STRUCT_QUALS + 3);
        }
    } else {
        /* unknown array qualifier */
        TRACE("unknown qual: %s", s);
    }
    TRRET(0);
}
/**
 * name ::= WORD
 * delim|delim1|delim2 ::= STR
 * simple_type ::= int | str | float | bool
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
        fparser_datum_t **node; \
        lkit_type_t **ty; \
        if ((node = array_next(form, &it)) == NULL) { \
            LKIT_ERROR(var) = 1; \
            dat->error = 1; \
            return 1; \
        } \
        if ((ty = array_incr(&var->fields)) == NULL) { \
            FAIL("array_incr"); \
        } \
        if ((*ty = lkit_type_parse(*node, 1)) == NULL) { \
            LKIT_ERROR(var) = 1; \
            (*node)->error = 1; \
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
lkit_type_parse(fparser_datum_t *dat, int seterror)
{
    lkit_type_t *ty = NULL;
    lkit_type_t *probe = NULL;
    fparser_tag_t tag;

    tag = FPARSER_DATUM_TAG(dat);
    if (tag == FPARSER_WORD) {
        bytes_t *b;
        char *typename;

        b = (bytes_t *)(dat->body);
        typename = (char *)(b->data);

        /* simple types */
        if (strcmp(typename, "undef") == 0) {
            ty = lkit_type_new(LKIT_UNDEF);
        } else if (strcmp(typename, "int") == 0) {
            ty = lkit_type_new(LKIT_INT);
        } else if (strcmp(typename, "str") == 0) {
            ty = lkit_type_new(LKIT_STR);
        } else if (strcmp(typename, "float") == 0) {
            ty = lkit_type_new(LKIT_FLOAT);
        } else if (strcmp(typename, "bool") == 0) {
            ty = lkit_type_new(LKIT_BOOL);
        } else if (strcmp(typename, "...") == 0) {
            ty = lkit_type_new(LKIT_VARARG);
        } else {
            /*
             * XXX handle typedefs here, or unknown type ...
             */
            if ((probe = dict_get_item(&typedefs, (bytes_t *)(dat->body))) != NULL) {
                ty = probe;
            }
            /*
             * either already registered type, or NULL, skip type
             * registration.
             */
            goto end;
        }
    } else if (tag == FPARSER_SEQ) {
        array_t *form;
        array_iter_t it;
        bytes_t *first;

        form = (array_t *)dat->body;

        if (lparse_first_word_bytes(form, &it, &first, 1) == 0) {

            if (strcmp((char *)first->data, "array") == 0) {
                lkit_array_t *ta;
                fparser_datum_t **node;
                lkit_type_t **elemtype;

                ty = lkit_type_new(LKIT_ARRAY);
                ta = (lkit_array_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_array_quals,
                                 ta) != 0) {
                    goto err;
                }

                if ((node = array_next(form, &it)) == NULL) {
                    goto err;
                }

                if ((elemtype = array_incr(&ta->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = lkit_type_parse(*node, 1)) == NULL) {
                    goto err;
                }

            } else if (strcmp((char *)first->data, "dict") == 0) {
                lkit_dict_t *td;
                fparser_datum_t **node;
                lkit_type_t **elemtype;

                ty = lkit_type_new(LKIT_DICT);
                td = (lkit_dict_t *)ty;

                if (lparse_next_str(form, &it, &td->kvdelim, 1) != 0) {
                    goto err;
                }

                if (lparse_next_str(form, &it, &td->fdelim, 1) != 0) {
                    goto err;
                }

                if ((node = array_next(form, &it)) == NULL) {
                    goto err;
                }

                if ((elemtype = array_incr(&td->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = lkit_type_parse(*node, 1)) == NULL) {
                    goto err;
                }

            } else if (strcmp((char *)first->data, "struct") == 0) {
                lkit_struct_t *ts;
                fparser_datum_t **node;

                ty = lkit_type_new(LKIT_STRUCT);
                ts = (lkit_struct_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_struct_quals,
                                 ts) != 0) {
                    goto err;
                }

                /* fields */
                for (node = array_next(form, &it);
                     node != NULL;
                     node = array_next(form, &it)) {


                    if (FPARSER_DATUM_TAG(*node) == FPARSER_SEQ) {
                        //if (parse_fielddef_struct(*node, ts) != 0) {
                        if (parse_fielddef(*node, ty) != 0) {
                            goto err;
                        }
                    } else {
                        goto err;
                    }
                }

            } else if (strcmp((char *)first->data, "func") == 0) {
                lkit_func_t *tf;
                fparser_datum_t **node;
                lkit_type_t **paramtype;


                ty = lkit_type_new(LKIT_FUNC);
                tf = (lkit_func_t *)ty;

                /* retval and optional params are stroed in tf->fields */
                for (node = array_next(form, &it);
                     node != NULL;
                     node = array_next(form, &it)) {

                    if ((paramtype = array_incr(&tf->fields)) == NULL) {
                        FAIL("array_incr");
                    }

                    if ((*paramtype = lkit_type_parse(*node, 1)) == NULL) {
                        goto err;
                    }
                    /* no function params or return values */
                    if ((*paramtype)->tag == LKIT_FUNC) {
                        (*node)->error = 1;
                        goto err;
                    }
                }

                /* no void functions */
                if (tf->fields.elnum < 1) {
                    goto err;
                }

            } else {
                /* unknown */
                goto err;
            }
        }
    } else {
        goto end;
    }

    if ((probe = dict_get_item(&types, ty)) == NULL) {
        dict_set_item(&types, ty, ty);
    } else {
        lkit_type_destroy(&ty);
        ty = probe;
    }

end:
    return ty;

err:
    dat->error = seterror;
    lkit_type_destroy(&ty);
    goto end;
}

lkit_type_t *
lkit_type_find(lkit_type_t *sample)
{
    return dict_get_item(&types, sample);
}

int
lkit_parse_typedef(array_t *form, array_iter_t *it)
{
    int res = 0;
    fparser_datum_t **node;
    bytes_t *typename = NULL;
    lkit_type_t *type, *probe;

    /* (type WORD int|float|str|bool|undef|(array )|(dict )|(struct )|(func )) */
    if (lparse_next_word_bytes(form, it, &typename, 1) != 0) {
        goto err;
    }

    if ((node = array_next(form, it)) == NULL) {
        goto err;
    }

    if ((type = lkit_type_parse(*node, 1)) == NULL) {
        (*node)->error = 1;
        goto err;
    }

    /* have it unique */
    if ((probe = dict_get_item(&typedefs, typename)) != NULL) {
        if (lkit_type_cmp(probe, type) != 0) {
            (*node)->error = 1;
            goto err;
        }
    } else {
        dict_set_item(&typedefs, typename, type);
    }

end:
    return res;

err:
    res = 1;
    goto end;
}

int
lkit_type_traverse(lkit_type_t *ty, lkit_type_traverser_t cb, void *udata)
{
    array_iter_t it;
    lkit_type_t **pty;

    /* depth-first */
    switch (ty->tag) {

    case LKIT_ARRAY:
        {
            lkit_array_t *ta;
            ta = (lkit_array_t *)ty;

            for (pty = array_first(&ta->fields, &it);
                 pty != NULL;
                 pty = array_next(&ta->fields, &it)) {
                if (cb(*pty, udata) != 0) {
                    TRRET(LKIT_TYPE_TRAVERSE + 1);
                }
            }
        }

        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            td = (lkit_dict_t *)ty;

            for (pty = array_first(&td->fields, &it);
                 pty != NULL;
                 pty = array_next(&td->fields, &it)) {
                if (cb(*pty, udata) != 0) {
                    TRRET(LKIT_TYPE_TRAVERSE + 2);
                }
            }
        }

        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            ts = (lkit_struct_t *)ty;

            for (pty = array_first(&ts->fields, &it);
                 pty != NULL;
                 pty = array_next(&ts->fields, &it)) {
                if (cb(*pty, udata) != 0) {
                    TRRET(LKIT_TYPE_TRAVERSE + 3);
                }
            }
        }

        break;

    case LKIT_FUNC:
        {
            lkit_func_t *tf;
            tf = (lkit_func_t *)ty;

            for (pty = array_first(&tf->fields, &it);
                 pty != NULL;
                 pty = array_next(&tf->fields, &it)) {
                if (cb(*pty, udata) != 0) {
                    TRRET(LKIT_TYPE_TRAVERSE + 4);
                }
            }
        }

        break;

    default:
        FAIL("ltype_traverse");

    }

    if (cb(ty, udata) != 0) {
        TRRET(LKIT_TYPE_TRAVERSE + 5);
    }

    return 0;
}

int
ltype_transform(dict_traverser_t cb, void *udata)
{
    return dict_traverse(&types, cb, udata);
}

void
ltype_init(void)
{
    lkit_type_t *ty;

    dict_init(&types, 101,
             (dict_hashfn_t)lkit_type_hash,
             (dict_item_comparator_t)lkit_type_cmp,
             (dict_item_finalizer_t)lkit_type_fini_dict);

    ty = lkit_type_new(LKIT_UNDEF);
    dict_set_item(&types, ty, ty);
    ty = lkit_type_new(LKIT_INT);
    dict_set_item(&types, ty, ty);
    ty = lkit_type_new(LKIT_STR);
    dict_set_item(&types, ty, ty);
    ty = lkit_type_new(LKIT_FLOAT);
    dict_set_item(&types, ty, ty);
    ty = lkit_type_new(LKIT_BOOL);
    dict_set_item(&types, ty, ty);
    ty = lkit_type_new(LKIT_VARARG);
    dict_set_item(&types, ty, ty);

    dict_init(&typedefs, 101,
             (dict_hashfn_t)bytes_hash,
             (dict_item_comparator_t)bytes_cmp,
             NULL /* key and value weakrefs */);

}

UNUSED static int
dump_type(lkit_type_t *key, UNUSED lkit_type_t *value)
{
    bytestream_t bs;
    bytestream_init(&bs);
    lkit_type_str(key, &bs);
    bytestream_cat(&bs, 1, "\0");
    TRACE("%s", SDATA(&bs, 0));
    bytestream_fini(&bs);
    return 0;
}

void
ltype_fini(void)
{
    //dict_traverse(&types, (dict_traverser_t)dump_type, NULL);
    dict_fini(&types);
}
