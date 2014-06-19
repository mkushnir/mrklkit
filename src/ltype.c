#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dict.h>
#define TRRET_DEBUG_VERBOSE
#include <mrkcommon/dumpm.h>
#include <mrkcommon/fasthash.h>
#include <mrkcommon/util.h>

#include <mrklkit/mrklkit.h>
#include <mrklkit/fparser.h>
#include <mrklkit/ltype.h>
#include <mrklkit/lparse.h>
#include <mrklkit/lruntime.h>
#include <mrklkit/util.h>

#include "diag.h"

/*
 * array of weak refs to lkit_type_t *
 */
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
            //assert((*ty)->tag < _LKIT_END_OF_BUILTIN_TYPES);
            ;

        }

        free(*ty);
        *ty = NULL;
    }
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
        break;

    case LKIT_VOID:
        {
            lkit_void_t *tv;
            tv = (lkit_void_t *)ty;
            if ((tv = malloc(sizeof(lkit_void_t))) == NULL) {
                FAIL("malloc");
            }
            tv->base.tag = tag;
            tv->base.name = "void";
            ty = (lkit_type_t *)tv;
        }
        break;

    case LKIT_INT:
    case LKIT_INT_MIN:
    case LKIT_INT_MAX:
        {
            lkit_int_t *ti;
            ti = (lkit_int_t *)ty;
            if ((ti = malloc(sizeof(lkit_int_t))) == NULL) {
                FAIL("malloc");
            }
            ti->base.tag = tag;
            ti->base.name = (tag == LKIT_INT_MIN) ? "intm" :
                            (tag == LKIT_INT_MAX) ? "intM" :
                            "int";
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
            tc->deref_backend = NULL;
            ty = (lkit_type_t *)tc;
        }
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        {
            lkit_float_t *tg;
            tg = (lkit_float_t *)ty;
            if ((tg = malloc(sizeof(lkit_float_t))) == NULL) {
                FAIL("malloc");
            }
            tg->base.tag = tag;
            tg->base.name = (tag == LKIT_FLOAT_MIN) ? "floatm" :
                            (tag == LKIT_FLOAT_MAX) ? "floatM" :
                            "float";
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
            ty = (lkit_type_t *)tb;
        }
        break;

    case LKIT_ANY:
        {
            lkit_any_t *tn;
            tn = (lkit_any_t *)ty;
            if ((tn = malloc(sizeof(lkit_any_t))) == NULL) {
                FAIL("malloc");
            }
            tn->base.tag = tag;
            tn->base.name = "any";
            ty = (lkit_type_t *)tn;
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
            //ta->fini = NULL;
            ta->parser = LKIT_PARSER_NONE;
            ta->delim = NULL;
            array_init(&ta->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
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
            //td->fini = NULL;
            td->kvdelim = NULL;
            td->fdelim = NULL;
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
            ts->init = NULL;
            ts->fini = NULL;
            ts->parser = LKIT_PARSER_NONE;
            ts->delim = NULL;
            array_init(&ts->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            array_init(&ts->names, sizeof(bytes_t *), 0, NULL, NULL);
            ts->deref_backend = NULL;
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
            array_init(&tf->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ty = (lkit_type_t *)tf;
        }
        break;

    default:
        FAIL("type_new");

    }

    ty->hash = 0;
    ty->backend = NULL;
    ty->compile = NULL;
    ty->compile_setup = NULL;
    ty->compile_cleanup = NULL;

    return ty;
}


lkit_type_t *
lkit_type_get(mrklkit_ctx_t *mctx, int tag)
{
    lkit_type_t *ty;

    if (tag < _LKIT_END_OF_BUILTIN_TYPES) {

        lkit_type_t **pty;

        if ((pty = array_get(&mctx->builtin_types, tag)) == NULL) {
            FAIL("array_get");
        }
        ty = *pty;

    } else {
        ty = lkit_type_new(tag);
    }

    return ty;
}

lkit_array_t *
lkit_type_get_array(mrklkit_ctx_t *mctx, int ftag)
{
    lkit_array_t *ty;
    lkit_type_t **fty;

    ty = (lkit_array_t *)lkit_type_get(mctx, LKIT_ARRAY);

    if ((fty = array_incr(&ty->fields)) == NULL) {
        FAIL("array_incr");
    }

    *fty = lkit_type_get(mctx, ftag);
    return (lkit_array_t *)lkit_type_finalize(mctx, (lkit_type_t *)ty);
}


lkit_dict_t *
lkit_type_get_dict(mrklkit_ctx_t *mctx, int ftag)
{
    lkit_dict_t *ty;
    lkit_type_t **fty;

    ty = (lkit_dict_t *)lkit_type_get(mctx, LKIT_DICT);

    if ((fty = array_incr(&ty->fields)) == NULL) {
        FAIL("array_incr");
    }

    *fty = lkit_type_get(mctx, ftag);
    return (lkit_dict_t *)lkit_type_finalize(mctx, (lkit_type_t *)ty);
}


lkit_type_t *
lkit_type_finalize(mrklkit_ctx_t *mctx, lkit_type_t *ty)
{
    dict_item_t *probe;

    if ((probe = dict_get_item(&mctx->types, ty)) == NULL) {
        /* this is new one */
        dict_set_item(&mctx->types, ty, ty);
    } else {
        lkit_type_t *pty;

        pty = probe->value;
        if (pty != ty) {
            /* make ty singleton */
            if (ty->tag >= _LKIT_END_OF_BUILTIN_TYPES) {
                lkit_type_destroy(&ty);
            } else {
                TRACE("a new instance of builtin type???");
                lkit_type_dump(ty);
                FAIL("dict_get_item");
            }
            ty = pty;
        }
    }

    return ty;
}


void
lkit_register_typedef(mrklkit_ctx_t *mctx, lkit_type_t *ty, bytes_t *typename)
{
    dict_item_t *probe;

    /* have it unique */
    if ((probe = dict_get_item(&mctx->typedefs, typename)) != NULL) {
        lkit_type_t *pty;

        pty = probe->value;
        if (lkit_type_cmp(pty, ty) != 0) {
            FAIL("lkit_type_cmp");
        }
    } else {
        dict_set_item(&mctx->typedefs, typename, ty);
    }
}


lkit_type_t *
lkit_typedef_get(mrklkit_ctx_t *mctx, bytes_t *typename)
{
    dict_item_t *di;

    if ((di = dict_get_item(&mctx->typedefs, typename)) != NULL) {
        return di->value;
    }
    return NULL;
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

    return NULL;
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
            char *dst;
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
            char *dst;
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
            char *dst;
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
                TRACEC(" \"%s\" ", dst);
                free(dst);
            }

            for (elty = array_first(&ts->fields, &it);
                 elty != NULL;
                 elty = array_next(&ts->fields, &it)) {

                bytes_t **name;

                if ((name = array_get(&ts->names, it.iter)) != NULL) {
                    TRACEC(" (%s ", (*name)->data);
                    _lkit_type_dump(*elty, level + 1);
                    TRACEC(") ");
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
            bytestream_nprintf(bs, strlen(ty->name) + 2, "%s ", ty->name);
        }
        bytestream_cat(bs, 2, ") ");
    }
}

/*
 * Parser.
 *
 */

UNUSED static int
rt_array_bytes_fini(bytes_t **value, UNUSED void *udata)
{
    BYTES_DECREF(value);
    return 0;
}


UNUSED static int
rt_dict_fini_keyonly(bytes_t *key, UNUSED void *val)
{
    //TRACE("%ld %s", key != NULL ? key->nref : -2, key != NULL ? key->data : NULL);
    if (key != NULL) {
        BYTES_DECREF(&key);
    }
    return 0;
}

UNUSED static int
rt_dict_fini_keyval(bytes_t *key, void *val)
{
    if (key != NULL) {
        BYTES_DECREF(&key);
    }
    if (val != NULL) {
        void **v;
        v = &val;
        BYTES_DECREF((bytes_t **)v);
    }
    return 0;
}


static int
struct_field_name_cmp(bytes_t **a, bytes_t **b)
{
    return bytes_cmp(*a, *b);
}

static int
delim_cmp(const char *a, const char *b)
{
    if (a == NULL) {
        if (b != NULL) {
            return -1;
        }
    } else {
        if (b == NULL) {
            return 1;
        }
        return a[0] - b[0];
    }
    return 0;
}

static int
type_cmp(lkit_type_t **pa, lkit_type_t **pb)
{
    lkit_type_t *a = *pa;
    lkit_type_t *b = *pb;
    int64_t diff;

    if (a == b) {
        return 0;
    }

    diff = (int64_t)(a->tag - b->tag);

    if (diff == 0) {
        /* deep compare of custom types by their field content */
        switch (a->tag) {

        case LKIT_ARRAY:
            {
                lkit_array_t *aa, *ab;
                aa = (lkit_array_t *)a;
                ab = (lkit_array_t *)b;

                diff = delim_cmp(aa->delim, ab->delim);

                if (diff == 0) {
                    diff = array_cmp(&aa->fields,
                                     &ab->fields,
                                     (array_compar_t)type_cmp,
                                     0);
                }
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *da, *db;
                da = (lkit_dict_t *)a;
                db = (lkit_dict_t *)b;

                diff = delim_cmp(da->kvdelim, db->kvdelim);
                if (diff == 0) {
                    diff = delim_cmp(da->fdelim, db->fdelim);
                }
                if (diff == 0) {
                    diff = array_cmp(&da->fields,
                                     &db->fields,
                                     (array_compar_t)type_cmp,
                                     0);
                }
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *sa, *sb;
                sa = (lkit_struct_t *)a;
                sb = (lkit_struct_t *)b;

                diff = delim_cmp(sa->delim, sb->delim);

                if (diff == 0) {
                    diff = array_cmp(&sa->fields,
                                     &sb->fields,
                                     (array_compar_t)type_cmp,
                                     0);
                    if (diff == 0) {
                        diff = array_cmp(&sa->names,
                                         &sb->names,
                                         (array_compar_t)struct_field_name_cmp,
                                         0);
                    }
                }
            }
            break;

        case LKIT_FUNC:
            {
                lkit_func_t *fa, *fb;
                fa = (lkit_func_t *)a;
                fb = (lkit_func_t *)b;

                diff = array_cmp(&fa->fields,
                                 &fb->fields,
                                 (array_compar_t)type_cmp,
                                 0);
            }
            break;

        default:
            break;
        }
    }

    return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}


uint64_t
lkit_type_hash(lkit_type_t *ty)
{
    if (ty == NULL) {
        return 0;
    }
    if (ty->hash == 0) {
        bytestream_t bs;
        bytestream_init(&bs, 4096);
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
    int64_t diff;

    ha = lkit_type_hash(a);
    hb = lkit_type_hash(b);

    diff = (int64_t)(ha - hb);

    if (diff == 0) {
        diff = type_cmp(&a, &b);
    }
    return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}


static int
parse_array_quals(array_t *form,
                   array_iter_t *it,
                   char *qual,
                   lkit_array_t *ta)
{
    char *s = (char *)qual;

    ta->parser = LKIT_PARSER_NONE;
    ta->delim = NULL;

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ta->parser = LKIT_PARSER_DELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "mdelim") == 0) {
                ta->parser = LKIT_PARSER_MDELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    /* mdelim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "sdelim") == 0) {
                ta->parser = LKIT_PARSER_SDELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 3);
                }
            } else if (strcmp((char *) parser, "smdelim") == 0) {
                ta->parser = LKIT_PARSER_SMDELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 4);
                }
            } else if (strcmp((char *) parser, "smartdelim") == 0) {
                ta->parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_str(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 4);
                }
            } else if (strcmp((char *) parser, "none") == 0) {
                ta->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                ta->parser = -1;
                TRRET(PARSE_ARRAY_QUALS + 5);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_ARRAY_QUALS + 6);
        }
    } else {
        /* unknown array qualifier */
        TRACE("unknown qual: %s", s);
    }
    return 0;
}


static int
parse_dict_quals(array_t *form,
                 array_iter_t *it,
                 char *qual,
                 lkit_dict_t *td)
{
    char *s = (char *)qual;

    td->parser = LKIT_PARSER_NONE;
    td->kvdelim = NULL;
    td->fdelim = NULL;

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                td->parser = LKIT_PARSER_DELIM;
                if (lparse_next_str(form, it, &td->kvdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 1);
                }
                if (lparse_next_str(form, it, &td->fdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "smartdelim") == 0) {
                td->parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_str(form, it, &td->kvdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 1);
                }
                if (lparse_next_str(form, it, &td->fdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "none") == 0) {
                td->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                td->parser = -1;
                TRRET(PARSE_DICT_QUALS + 3);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_DICT_QUALS + 4);
        }
    } else {
        /* unknown array qualifier */
        TRACE("unknown qual: %s", s);
    }
    return 0;
}


static int
parse_struct_quals(array_t *form,
                   array_iter_t *it,
                   char *qual,
                   lkit_struct_t *ts)
{
    char *s = (char *)qual;

    ts->parser = LKIT_PARSER_NONE;
    ts->delim = NULL;

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ts->parser = LKIT_PARSER_DELIM;
                if (lparse_next_str(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "mdelim") == 0) {
                ts->parser = LKIT_PARSER_MDELIM;
                if (lparse_next_str(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "sdelim") == 0) {
                ts->parser = LKIT_PARSER_SDELIM;
                if (lparse_next_str(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 3);
                }
            } else if (strcmp((char *) parser, "smdelim") == 0) {
                ts->parser = LKIT_PARSER_SMDELIM;
                if (lparse_next_str(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 4);
                }
            } else if (strcmp((char *) parser, "none") == 0) {
                ts->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                ts->parser = -1;
                TRRET(PARSE_STRUCT_QUALS + 5);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_STRUCT_QUALS + 6);
        }
    } else {
        /* unknown array qualifier */
        TRACE("unknown qual: %s", s);
    }
    return 0;
}
/**
 * name ::= WORD
 * delim|delim1|delim2 ::= STR
 * simple_type ::= int | str | float | bool
 * fielddef ::= (name type)
 * complex_type ::= (array quals type) |
 *                  (dict kvdelim fdelim type) |
 *                  (struct quals *fielddef) |
 *                  (func fielddef *fielddef)
 *
 * type ::= simple_type | complex_type
 *
 */
static int
parse_fielddef(mrklkit_ctx_t *mctx,
               fparser_datum_t *dat,
               lkit_type_t *ty)
{
    array_t *form;
    array_iter_t it;
    bytes_t **name = NULL;

#define DO_PARSE_FIELDDEF(var, type) \
    type *var; \
    var = (type *)ty; \
    if ((name = array_incr(&var->names)) == NULL) { \
        FAIL("array_incr"); \
    } \
    form = (array_t *)dat->body; \
    if (lparse_first_word_bytes(form, &it, name, 1) == 0) { \
        fparser_datum_t **node; \
        lkit_type_t **fty; \
        if ((node = array_next(form, &it)) == NULL) { \
            dat->error = 1; \
            return 1; \
        } \
        if ((fty = array_incr(&var->fields)) == NULL) { \
            FAIL("array_incr"); \
        } \
        if ((*fty = lkit_type_parse(mctx, *node, 1)) == NULL) { \
            (*node)->error = 1; \
            return 1; \
        } \
    } else { \
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
lkit_type_parse(mrklkit_ctx_t *mctx,
                fparser_datum_t *dat,
                int seterror)
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
        if (strcmp(typename, "undef") == 0) {
            ty = lkit_type_get(mctx, LKIT_UNDEF);
        } else if (strcmp(typename, "void") == 0) {
            ty = lkit_type_get(mctx, LKIT_VOID);
        } else if (strcmp(typename, "int") == 0) {
            ty = lkit_type_get(mctx, LKIT_INT);
        } else if (strcmp(typename, "str") == 0) {
            ty = lkit_type_get(mctx, LKIT_STR);
        } else if (strcmp(typename, "float") == 0) {
            ty = lkit_type_get(mctx, LKIT_FLOAT);
        } else if (strcmp(typename, "bool") == 0) {
            ty = lkit_type_get(mctx, LKIT_BOOL);
        } else if (strcmp(typename, "any") == 0) {
            ty = lkit_type_get(mctx, LKIT_ANY);
        } else if (strcmp(typename, "...") == 0) {
            ty = lkit_type_get(mctx, LKIT_VARARG);
        } else {
            dict_item_t *probe = NULL;

            /*
             * XXX handle typedefs here, or unknown type ...
             */
            if ((probe = dict_get_item(&mctx->typedefs,
                                       (bytes_t *)(dat->body))) != NULL) {
                lkit_type_t *pty;

                pty = probe->value;
                ty = pty;

            } else {
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

                ty = lkit_type_get(mctx, LKIT_ARRAY);
                ta = (lkit_array_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_array_quals,
                                 ta) != 0) {
                    TR(LKIT_TYPE_PARSE + 1);
                    goto err;
                }

                if ((node = array_next(form, &it)) == NULL) {
                    TR(LKIT_TYPE_PARSE + 2);
                    goto err;
                }

                if ((elemtype = array_incr(&ta->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = lkit_type_parse(mctx,
                                                 *node,
                                                 1)) == NULL) {
                    TR(LKIT_TYPE_PARSE + 3);
                    goto err;
                }

                switch ((*elemtype)->tag) {
                case LKIT_STR:
                    //ta->fini = (array_finalizer_t)rt_array_bytes_fini;
                    break;

                case LKIT_INT:
                case LKIT_FLOAT:
                    //ta->fini = NULL;
                    break;

                default:
                    TR(LKIT_TYPE_PARSE + 4);
                }

            } else if (strcmp((char *)first->data, "dict") == 0) {
                lkit_dict_t *td;
                fparser_datum_t **node;
                lkit_type_t **elemtype;

                ty = lkit_type_get(mctx, LKIT_DICT);
                td = (lkit_dict_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_dict_quals,
                                 td) != 0) {
                    TR(LKIT_TYPE_PARSE + 5);
                    goto err;
                }

                if ((node = array_next(form, &it)) == NULL) {
                    TR(LKIT_TYPE_PARSE + 6);
                    goto err;
                }

                if ((elemtype = array_incr(&td->fields)) == NULL) {
                    FAIL("array_incr");
                }

                if ((*elemtype = lkit_type_parse(mctx,
                                                 *node,
                                                 1)) == NULL) {
                    TR(LKIT_TYPE_PARSE + 7);
                    goto err;
                }

                switch ((*elemtype)->tag) {
                case LKIT_STR:
                    //td->fini = (dict_item_finalizer_t)rt_dict_fini_keyval;
                    break;

                case LKIT_INT:
                case LKIT_FLOAT:
                    //td->fini = (dict_item_finalizer_t)rt_dict_fini_keyonly;
                    break;

                default:
                    TR(LKIT_TYPE_PARSE + 8);
                }

            } else if (strcmp((char *)first->data, "struct") == 0) {
                lkit_struct_t *ts;
                fparser_datum_t **node;

                ty = lkit_type_get(mctx, LKIT_STRUCT);
                ts = (lkit_struct_t *)ty;

                /* quals */
                if (lparse_quals(form, &it,
                                 (quals_parser_t)parse_struct_quals,
                                 ts) != 0) {
                    TR(LKIT_TYPE_PARSE + 9);
                    goto err;
                }

                /* fields */
                for (node = array_next(form, &it);
                     node != NULL;
                     node = array_next(form, &it)) {


                    if (FPARSER_DATUM_TAG(*node) == FPARSER_SEQ) {
                        if (parse_fielddef(mctx, *node, ty) != 0) {
                            TR(LKIT_TYPE_PARSE + 10);
                            goto err;
                        }
                    } else {
                        TR(LKIT_TYPE_PARSE + 11);
                        goto err;
                    }
                }

                /*
                 * ctor and dtor to be compiled, see ltypegen.c
                 */

            } else if (strcmp((char *)first->data, "func") == 0) {
                lkit_func_t *tf;
                fparser_datum_t **node;
                lkit_type_t **paramtype;


                ty = lkit_type_get(mctx, LKIT_FUNC);
                tf = (lkit_func_t *)ty;

                /* retval and optional params are stroed in tf->fields */
                for (node = array_next(form, &it);
                     node != NULL;
                     node = array_next(form, &it)) {

                    if ((paramtype = array_incr(&tf->fields)) == NULL) {
                        FAIL("array_incr");
                    }

                    if ((*paramtype = lkit_type_parse(mctx,
                                                      *node,
                                                      1)) == NULL) {
                        TR(LKIT_TYPE_PARSE + 12);
                        //fparser_datum_dump(node, NULL);
                        goto err;
                    }
                    /* no function params or return values */
                    if ((*paramtype)->tag == LKIT_FUNC) {
                        (*node)->error = 1;
                        TR(LKIT_TYPE_PARSE + 13);
                        goto err;
                    }
                }

                /* no void functions */
                if (tf->fields.elnum < 1) {
                    TR(LKIT_TYPE_PARSE + 14);
                    goto err;
                }

            } else {
                /* unknown */
                //TR(LKIT_TYPE_PARSE + 15);
                goto err;
            }
        }
    } else {
        goto end;
    }

    ty = lkit_type_finalize(mctx, ty);

end:
    return ty;

err:
    dat->error = seterror;
    if (ty != NULL) {
        if (ty->tag >= _LKIT_END_OF_BUILTIN_TYPES) {
            lkit_type_destroy(&ty);
        } else {
            TRACE("not destroying");
            lkit_type_dump(ty);
        }
    }
    goto end;
}


int
lkit_parse_typedef(mrklkit_ctx_t *mctx,
                   array_t *form,
                   array_iter_t *it)
{
    int res = 0;
    fparser_datum_t **node;
    bytes_t *typename = NULL;
    lkit_type_t *type;

    /* (type WORD int|float|str|bool|undef|(array )|(dict )|(struct )|(func )) */
    if (lparse_next_word_bytes(form, it, &typename, 1) != 0) {
        TR(LKIT_PARSE_TYPEDEF + 1);
        goto err;
    }

    if ((node = array_next(form, it)) == NULL) {
        TR(LKIT_PARSE_TYPEDEF + 2);
        goto err;
    }

    if ((type = lkit_type_parse(mctx, *node, 1)) == NULL) {
        (*node)->error = 1;
        TR(LKIT_PARSE_TYPEDEF + 3);
        goto err;
    }

    lkit_register_typedef(mctx, type, typename);


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


void
ltype_init(void)
{
}

void
ltype_fini(void)
{
}
