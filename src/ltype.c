#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dict.h>
//#define TRRET_DEBUG_VERBOSE
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
 * lkit_type_t *, lkit_type_t *
 */
static dict_t types;
static array_t builtin_types;
/*
 * bytes_t *, lkit_type_t *
 */
static dict_t typedefs;

/*
 * array of weak refs to lkit_type_t *
 */
int
lkit_type_destroy(lkit_type_t **ty)
{
    if (*ty != NULL) {
        (*ty)->hash = 0;

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
                array_fini(&ta->fields);
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *td;

                td = (lkit_dict_t *)*ty;
                array_fini(&td->fields);
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *ts;

                ts = (lkit_struct_t *)*ty;
                array_fini(&ts->fields);
                array_fini(&ts->names);
                array_fini(&ts->parsers);
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


static int
lkit_array_names_fini(bytes_t **pname)
{
    BYTES_DECREF(pname);
    return 0;
}


static int
lkit_array_parsers_init(lkit_parser_t *parser)
{
    *parser = LKIT_PARSER_NONE;
    return 0;
}


static lkit_type_t *
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

    case LKIT_IR:
        {
            lkit_ir_t *ti;
            if ((ti = malloc(sizeof(lkit_ir_t))) == NULL) {
                FAIL("malloc");
            }
            ti->base.tag = tag;
            ti->base.name = "ir";
            ty = (lkit_type_t *)ti;
        }
        break;

    case LKIT_TY:
        {
            lkit_ty_t *tt;
            if ((tt = malloc(sizeof(lkit_ty_t))) == NULL) {
                FAIL("malloc");
            }
            tt->base.tag = tag;
            tt->base.name = "ty";
            ty = (lkit_type_t *)tt;
        }
        break;

    case LKIT_VOID:
        {
            lkit_void_t *tv;
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
            if ((tc = malloc(sizeof(lkit_str_t))) == NULL) {
                FAIL("malloc");
            }
            tc->base.tag = tag;
            tc->base.name = "str";
            ty = (lkit_type_t *)tc;
        }
        break;

    case LKIT_FLOAT:
    case LKIT_FLOAT_MIN:
    case LKIT_FLOAT_MAX:
        {
            lkit_float_t *tg;
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
            if ((ta = malloc(sizeof(lkit_array_t))) == NULL) {
                FAIL("malloc");
            }
            ta->base.tag = tag;
            ta->base.name = "array";
            //ta->fini = NULL;
            ta->parser = LKIT_PARSER_NONE;
            ta->delim = '\0';
            ta->nreserved = 0;
            array_init(&ta->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ty = (lkit_type_t *)ta;
        }
        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;
            if ((td = malloc(sizeof(lkit_dict_t))) == NULL) {
                FAIL("malloc");
            }
            td->base.tag = tag;
            td->base.name = "dict";
            //td->fini = NULL;
            td->kvdelim = '\0';
            td->fdelim = '\0';
            array_init(&td->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            ty = (lkit_type_t *)td;
        }
        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            if ((ts = malloc(sizeof(lkit_struct_t))) == NULL) {
                FAIL("malloc");
            }
            ts->base.tag = tag;
            ts->base.name = "struct";
            ts->init = NULL;
            ts->fini = NULL;
            ts->parser = LKIT_PARSER_NONE;
            ts->delim = '\0';
            array_init(&ts->fields, sizeof(lkit_type_t *), 0, NULL, NULL);
            array_init(&ts->names,
                       sizeof(bytes_t *),
                       0,
                       NULL,
                       (array_finalizer_t)lkit_array_names_fini);
            array_init(&ts->parsers,
                       sizeof(lkit_parser_t),
                       0,
                       (array_initializer_t)lkit_array_parsers_init,
                       NULL);
            ty = (lkit_type_t *)ts;
        }
        break;

    case LKIT_FUNC:
        {
            lkit_func_t *tf;
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
    ty->compile = NULL;
    ty->compile_setup = NULL;
    ty->compile_cleanup = NULL;

    return ty;
}


lkit_type_t *
lkit_type_get(UNUSED mrklkit_ctx_t *mctx, int tag)
{
    lkit_type_t *ty;

    if (tag < _LKIT_END_OF_BUILTIN_TYPES) {
        lkit_type_t **pty;

        if ((pty = array_get(&builtin_types, tag)) == NULL) {
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
    return (lkit_array_t *)lkit_type_finalize((lkit_type_t *)ty);
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
    return (lkit_dict_t *)lkit_type_finalize((lkit_type_t *)ty);
}


lkit_type_t *
lkit_type_finalize(lkit_type_t *ty)
{
    dict_item_t *probe;

    assert(ty != NULL);

    if ((probe = dict_get_item(&types, ty)) == NULL) {
        /* this is new one */
        dict_set_item(&types, ty, ty);
    } else {
        lkit_type_t *pty;

        pty = probe->value;
        if (pty != ty) {
            /* make ty singleton */
            if (ty->tag >= _LKIT_END_OF_BUILTIN_TYPES) {
                lkit_type_destroy(&ty);
            } else {
                TRACE("a new instance of builtin type??? (%d)", ty->tag);
                lkit_type_dump(ty);
                FAIL("dict_get_item");
            }
            ty = pty;
        }
    }

    return ty;
}


void
lkit_register_typedef(UNUSED mrklkit_ctx_t *mctx, lkit_type_t *ty, bytes_t *typename)
{
    dict_item_t *probe;

    /* have it unique */
    if ((probe = dict_get_item(&typedefs, typename)) != NULL) {
        lkit_type_t *pty;

        pty = probe->value;
        if (lkit_type_cmp(pty, ty) != 0) {

            //lkit_type_dump(pty);
            lkit_type_dump(ty);
            TRACE("pty=%p ty=%p", pty, ty);
            FAIL("lkit_type_cmp");
        }
    } else {
        dict_set_item(&typedefs,
                      bytes_new_from_str((char *)typename->data),
                      ty);
    }
}


lkit_type_t *
lkit_typedef_get(UNUSED mrklkit_ctx_t *mctx, bytes_t *typename)
{
    dict_item_t *di;

    if ((di = dict_get_item(&typedefs, typename)) != NULL) {
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


lkit_type_t *
lkit_func_get_arg_type(lkit_func_t *tf, size_t idx)
{
    ++idx;
    if (idx < tf->fields.elnum) {
         return *ARRAY_GET(lkit_type_t *, &tf->fields, idx);
    }

    return NULL;
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

            ta = (lkit_array_t *)ty;
            if (ta->delim) {
                TRACEC("\"%c\" ", ta->delim);
            }
            _lkit_type_dump(lkit_array_get_element_type(ta), level + 1);
        }
        break;

    case LKIT_DICT:
        {
            lkit_dict_t *td;

            td = (lkit_dict_t *)ty;
            if (td->kvdelim) {
                TRACEC("\"%c\" ", td->kvdelim);
            }
            if (td->fdelim) {
                TRACEC("\"%c\" ", td->fdelim);
            }
            _lkit_type_dump(lkit_dict_get_element_type(td), level + 1);
        }
        break;

    case LKIT_STRUCT:
        {
            lkit_struct_t *ts;
            lkit_type_t **elty;
            array_iter_t it;

            ts = (lkit_struct_t *)ty;
            if (ts->delim) {
                TRACEC(" \"%c\" ", ts->delim);
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
delim_cmp(char a, char b)
{
    return a - b;
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


static int
type_cmp_loose(lkit_type_t **pa, lkit_type_t **pb)
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

                diff = array_cmp(&aa->fields,
                                 &ab->fields,
                                 (array_compar_t)type_cmp_loose,
                                 0);
            }
            break;

        case LKIT_DICT:
            {
                lkit_dict_t *da, *db;
                da = (lkit_dict_t *)a;
                db = (lkit_dict_t *)b;

                diff = array_cmp(&da->fields,
                                 &db->fields,
                                 (array_compar_t)type_cmp_loose,
                                 0);
            }
            break;

        case LKIT_STRUCT:
            {
                lkit_struct_t *sa, *sb;
                sa = (lkit_struct_t *)a;
                sb = (lkit_struct_t *)b;

                diff = array_cmp(&sa->fields,
                                 &sb->fields,
                                 (array_compar_t)type_cmp_loose,
                                 0);
            }
            break;

        case LKIT_FUNC:
            {
                lkit_func_t *fa, *fb;
                fa = (lkit_func_t *)a;
                fb = (lkit_func_t *)b;

                diff = array_cmp(&fa->fields,
                                 &fb->fields,
                                 (array_compar_t)type_cmp_loose,
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


int
lkit_type_cmp_loose(lkit_type_t *a, lkit_type_t *b)
{
    uint64_t ha, hb;
    int64_t diff;

    ha = lkit_type_hash(a);
    hb = lkit_type_hash(b);

    diff = (int64_t)(ha - hb);

    if (diff == 0) {
        diff = type_cmp_loose(&a, &b);
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

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ta->parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "mdelim") == 0) {
                ta->parser = LKIT_PARSER_MDELIM;
                if (lparse_next_char(form, it, &ta->delim, 1) != 0) {
                    /* mdelim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "smartdelim") == 0) {
                ta->parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_char(form, it, &ta->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_ARRAY_QUALS + 5);
                }
            } else if (strcmp((char *) parser, "none") == 0) {
                ta->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                ta->parser = -1;
                TRRET(PARSE_ARRAY_QUALS + 6);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_ARRAY_QUALS + 7);
        }

    } else if (strcmp(s, ":reserve") == 0) {
        if (lparse_next_int(form, it, &ta->nreserved, 1) != 0) {
            TRRET(PARSE_ARRAY_QUALS + 8);
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

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                td->parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &td->kvdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 1);
                }
                if (lparse_next_char(form, it, &td->fdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 2);
                }
            } else if (strcmp((char *) parser, "smartdelim") == 0) {
                td->parser = LKIT_PARSER_SMARTDELIM;
                if (lparse_next_char(form, it, &td->kvdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 3);
                }
                if (lparse_next_char(form, it, &td->fdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 4);
                }
            } else if (strcmp((char *) parser, "delim-opt-quoted") == 0) {
                td->parser = LKIT_PARSER_OPTQSTRDELIM;
                if (lparse_next_char(form, it, &td->kvdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 5);
                }
                if (lparse_next_char(form, it, &td->fdelim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_DICT_QUALS + 6);
                }
            } else if (strcmp((char *) parser, "none") == 0) {
                td->parser = LKIT_PARSER_NONE;
            } else {
                /* unknown parser */
                td->parser = -1;
                TRRET(PARSE_DICT_QUALS + 7);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_DICT_QUALS + 8);
        }
    } else {
        /* unknown dict qualifier */
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

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "delim") == 0) {
                ts->parser = LKIT_PARSER_DELIM;
                if (lparse_next_char(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 1);
                }
            } else if (strcmp((char *) parser, "mdelim") == 0) {
                ts->parser = LKIT_PARSER_MDELIM;
                if (lparse_next_char(form, it, &ts->delim, 1) != 0) {
                    /* delim requires a string argument */
                    TRRET(PARSE_STRUCT_QUALS + 2);
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
        /* unknown struct qualifier */
        TRACE("unknown qual: %s", s);
    }
    return 0;
}

static int
parse_fielddef_quals(array_t *form,
                     array_iter_t *it,
                     char *qual,
                     lkit_parser_t *p)
{
    char *s = (char *)qual;

    if (strcmp(s, ":parser") == 0) {
        char *parser = NULL;

        if (lparse_next_word(form, it, &parser, 1) == 0) {
            if (strcmp((char *) parser, "quoted") == 0) {
                *p = LKIT_PARSER_QSTR;
            } else if (strcmp((char *) parser, "opt-quoted") == 0) {
                *p = LKIT_PARSER_OPTQSTR;
            } else {
                /* unknown parser */
                *p = LKIT_PARSER_NONE;
                TRRET(PARSE_FIELDDEF_QUALS + 2);
            }
        } else {
            /* a WORD expected after :parser */
            TRRET(PARSE_FIELDDEF_QUALS + 3);
        }
    } else {
        /* unknown fielddef qualifier */
        TRACE("unknown qual: %s", s);
    }
    return 0;
}
/**
 * name ::= WORD
 * delim|delim1|delim2 ::= STR
 * simple_type ::= int | str | float | bool
 * fielddef ::= (name quals type)
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
               lkit_type_t *ty,
               int seterror)
{
    array_t *form;
    array_iter_t it;
    bytes_t **name = NULL;
    lkit_parser_t *parser;

#define DO_PARSE_FIELDDEF(var)                                         \
    var = (__typeof(var))ty;                                           \
                                                                       \
    if ((name = array_incr(&var->names)) == NULL) {                    \
        FAIL("array_incr");                                            \
    }                                                                  \
                                                                       \
    if ((parser = array_incr(&var->parsers)) == NULL) {                \
        FAIL("array_incr");                                            \
    }                                                                  \
                                                                       \
    form = (array_t *)dat->body;                                       \
                                                                       \
    if (lparse_first_word_bytes(form, &it, name, 1) == 0) {            \
        fparser_datum_t **node;                                        \
        lkit_type_t **fty;                                             \
        *name = bytes_new_from_bytes(*name);                           \
        if (lparse_quals(form, &it,                                    \
                         (quals_parser_t)parse_fielddef_quals,         \
                         parser) != 0) {                               \
            dat->error = seterror;                                     \
            return 1;                                                  \
        }                                                              \
        if ((node = array_next(form, &it)) == NULL) {                  \
            dat->error = seterror;                                     \
            return 1;                                                  \
        }                                                              \
        if ((fty = array_incr(&var->fields)) == NULL) {                \
            FAIL("array_incr");                                        \
        }                                                              \
        if ((*fty = lkit_type_parse(mctx, *node, seterror)) == NULL) { \
            (*node)->error = seterror;                                 \
            return 1;                                                  \
        }                                                              \
    } else {                                                           \
        dat->error = seterror;                                         \
        return 1;                                                      \
    }                                                                  \


    if (ty->tag == LKIT_STRUCT) {
        lkit_struct_t *ts;
        DO_PARSE_FIELDDEF(ts);
    } else {
        FAIL("parse_fielddef");
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
        } else if (strcmp(typename, "ir") == 0) {
            ty = lkit_type_get(mctx, LKIT_IR);
        } else if (strcmp(typename, "ty") == 0) {
            ty = lkit_type_get(mctx, LKIT_TY);
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
            if ((probe = dict_get_item(&typedefs,
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

        if (lparse_first_word_bytes(form, &it, &first, seterror) == 0) {
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
                        if (parse_fielddef(mctx, *node, ty, 1) != 0) {
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
        } else {
            goto err;
        }
    } else {
        goto end;
    }

    ty = lkit_type_finalize(ty);

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


static int
lkit_type_fini_dict(lkit_type_t *key, UNUSED lkit_type_t *value)
{
    lkit_type_destroy(&key);
    return 0;
}


static int
lkit_typedef_fini_dict(bytes_t *key, UNUSED lkit_type_t *value)
{
    BYTES_DECREF(&key);
    return 0;
}


int
lkit_traverse_types(dict_traverser_t cb, void *udata)
{
    return dict_traverse(&types, cb, udata);
}


static void
mrklkit_init_types(dict_t *types,
                   array_t *builtin_types,
                   dict_t *typedefs)
{
    lkit_type_t *ty, **pty;

    dict_init(types, 101,
             (dict_hashfn_t)lkit_type_hash,
             (dict_item_comparator_t)lkit_type_cmp,
             (dict_item_finalizer_t)lkit_type_fini_dict);

    /* builtin types */

    array_init(builtin_types,
               sizeof(lkit_type_t *),
               _LKIT_END_OF_BUILTIN_TYPES,
               NULL,
               NULL /* weak refs */);

    ty = lkit_type_new(LKIT_UNDEF);
    if ((pty = array_get(builtin_types, LKIT_UNDEF)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_IR);
    if ((pty = array_get(builtin_types, LKIT_IR)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_TY);
    if ((pty = array_get(builtin_types, LKIT_TY)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_VOID);
    if ((pty = array_get(builtin_types, LKIT_VOID)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_INT);
    if ((pty = array_get(builtin_types, LKIT_INT)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_INT_MIN);
    if ((pty = array_get(builtin_types, LKIT_INT_MIN)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_INT_MAX);
    if ((pty = array_get(builtin_types, LKIT_INT_MAX)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_STR);
    if ((pty = array_get(builtin_types, LKIT_STR)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_FLOAT);
    if ((pty = array_get(builtin_types, LKIT_FLOAT)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_FLOAT_MIN);
    if ((pty = array_get(builtin_types, LKIT_FLOAT_MIN)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_FLOAT_MAX);
    if ((pty = array_get(builtin_types, LKIT_FLOAT_MAX)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_BOOL);
    if ((pty = array_get(builtin_types, LKIT_BOOL)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_ANY);
    if ((pty = array_get(builtin_types, LKIT_ANY)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    ty = lkit_type_new(LKIT_VARARG);
    if ((pty = array_get(builtin_types, LKIT_VARARG)) == NULL) {
        FAIL("array_get");
    }
    *pty = lkit_type_finalize(ty);

    dict_init(typedefs, 101,
             (dict_hashfn_t)bytes_hash,
             (dict_item_comparator_t)bytes_cmp,
             (dict_item_finalizer_t)lkit_typedef_fini_dict
             /* strongref key and weakref value */);

}


void
ltype_init(void)
{
    mrklkit_init_types(&types, &builtin_types, &typedefs);
}

void
ltype_fini(void)
{
    dict_fini(&typedefs);
    array_fini(&builtin_types);
    dict_fini(&types);
}
