#include <assert.h>
#include <limits.h>
#include <stdlib.h>

//#define TRRET_DEBUG
#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#define DUMPM_INDENT_SIZE 1
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#include "diag.h"
#include "fparser_private.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(fparser);
#endif

#define BLOCKSZ (1024 * 1024)
#define POISON_NREF 0x333333330000

static int fparser_datum_init(fparser_datum_t *, fparser_tag_t);
static int fparser_datum_fini(fparser_datum_t *);


ssize_t
fparser_unescape(char *dst, const char *src, ssize_t sz)
{
    ssize_t res = 0;
    ssize_t i;
    int j;
    char ch;

    for (i = 0, j = 0; i < sz; ++i, ++j) {
        ch = *(src + i);
        if (ch == '\\') {
            ++i;
            ++res;
            if (i < sz) {
                ch = *(src + i);
                switch (ch) {
                case 'a':
                    ch = '\a';
                    break;

                case 'b':
                    ch = '\b';
                    break;

                case 'f':
                    ch = '\f';
                    break;

                case 'n':
                    ch = '\n';
                    break;

                case 'r':
                    ch = '\r';
                    break;

                case 't':
                    ch = '\t';
                    break;

                case 'v':
                    ch = '\v';
                    break;

                default:
                    break;
                }
                *(dst + j) = ch;
            }
        } else {
            *(dst + j) = ch;
        }
    }
    *(dst + j) = '\0';
    return res;
}


void
fparser_escape(char *dst,
               size_t dst_sz,
               const char *src,
               size_t src_sz)
{
    size_t i, j;

    for (i = 0, j = 0;
        (i < src_sz) && (j < dst_sz);
        ++i, ++j) {
        if (src[i] == '"') {
            dst[j++] = '\\';
        }
        dst[j] = src[i];
    }
}


static int
compile_value(struct tokenizer_ctx *ctx,
              mnbytestream_t *bs,
              int state,
              int (*cb)(const char *,
                        fparser_datum_t *,
                        void *),
              void *udata)
{
    fparser_datum_t *dat = NULL;

    if (state & (LEX_QSTROUT)) {
        ssize_t sz = SPOS(bs) - ctx->tokstart;
        mnbytes_t *value;
        ssize_t escaped;

        if ((dat = malloc(sizeof(fparser_datum_t) +
                          sizeof(mnbytes_t) + sz + 1)) == NULL) {
            FAIL("malloc");
        }
        fparser_datum_init(dat, FPARSER_STR);
        value = (mnbytes_t *)(dat->body);
        value->nref = POISON_NREF;
        value->hash = 0;
        value->sz = (size_t)sz + 1;

        escaped = fparser_unescape((char *)value->data, SDATA(bs, ctx->tokstart), sz);
        value->sz -= escaped;

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            TRRET(COMPILE_VALUE + 1);
        }

        if (cb != NULL && cb(SDATA(bs, ctx->tokstart), dat, udata) != 0) {
            TRRET(COMPILE_VALUE + 2);
        }

        //LTRACE(ctx->indent, " '%s'", dat->body);

    } else if (state & (LEX_TOKOUT)) {
        char ch = SNCHR(bs, ctx->tokstart);
        ssize_t sz = SPOS(bs) - ctx->tokstart;
        char *s = SDATA(bs, ctx->tokstart);
#define _NUMKIND_INT 1
#define _NUMKIND_FLOAT 2
#define _NUMKIND_BOOL 3
#define _NUMKIND_VOID 4
#define _NUMKIND_NULL 5
        int numkind = 0;

        /*
         * +, -, +1, -1, 1, 10, +1.0, -1.0, 1.0, +xxx, -xxx, xxx
         */
        if (sz > 1) {
            if (ch == '+' || ch == '-' || (ch >= '0' && ch <= '9')) {
                unsigned i;
                /* try number */
                numkind = _NUMKIND_INT;
                for (i = 1; i < sz; ++i) {
                    if (s[i] == '.') {
                        numkind = _NUMKIND_FLOAT;
                    } else if (s[i] < '0' || s[i] > '9') {
                        numkind = 0;
                        break;
                    }
                }
            } else if (sz == 2 && ch == '#') {
                ch = SNCHR(bs, ctx->tokstart + 1);
                if (ch == 't') {
                    char *value;

                    numkind = _NUMKIND_BOOL;
                    if ((dat = malloc(sizeof(fparser_datum_t) +
                                      sizeof(char))) == NULL) {
                        FAIL("malloc");
                    }
                    fparser_datum_init(dat, FPARSER_BOOL);
                    value = (char *)(dat->body);
                    *value = 1;
                } else if (ch == 'f') {
                    char *value;

                    numkind = _NUMKIND_BOOL;
                    if ((dat = malloc(sizeof(fparser_datum_t) +
                                      sizeof(char))) == NULL) {
                        FAIL("malloc");
                    }
                    fparser_datum_init(dat, FPARSER_BOOL);
                    value = (char *)(dat->body);
                    *value = 0;
                } else if (ch == 'v') {
                    numkind = _NUMKIND_VOID;
                    if ((dat = malloc(sizeof(fparser_datum_t))) == NULL) {
                        FAIL("malloc");
                    }
                    fparser_datum_init(dat, FPARSER_VOID);
                } else if (ch == 'n') {
                    numkind = _NUMKIND_NULL;
                    if ((dat = malloc(sizeof(fparser_datum_t))) == NULL) {
                        FAIL("malloc");
                    }
                    fparser_datum_init(dat, FPARSER_NULL);
                }
            }
        } else {
            if (ch >= '0' && ch <= '9') {
                numkind = _NUMKIND_INT;
            }
        }

        if (numkind == _NUMKIND_INT) {
            int64_t *value;

            if ((dat = malloc(sizeof(fparser_datum_t) +
                              sizeof(int64_t))) == NULL) {
                FAIL("malloc");
            }
            fparser_datum_init(dat, FPARSER_INT);
            value = (int64_t *)(dat->body);
            *value = (int64_t)strtoll((const char *)SDATA(bs, ctx->tokstart),
                                      NULL,
                                      10);

        } else if (numkind == _NUMKIND_FLOAT) {
            double *value;

            if ((dat = malloc(sizeof(fparser_datum_t) +
                              sizeof(double))) == NULL) {
                FAIL("malloc");
            }
            fparser_datum_init(dat, FPARSER_FLOAT);
            value = (double *)(dat->body);
            *value = strtod((const char *)SDATA(bs, ctx->tokstart), NULL);

        } else if (numkind == _NUMKIND_BOOL ||
                   numkind == _NUMKIND_VOID ||
                   numkind == _NUMKIND_NULL) {
            /* already done */
            ;

        } else {
            mnbytes_t *value;

            if ((dat = malloc(sizeof(fparser_datum_t) +
                              sizeof(mnbytes_t) + sz + 1)) == NULL) {
                FAIL("malloc");
            }
            fparser_datum_init(dat, FPARSER_WORD);
            value = (mnbytes_t *)(dat->body);
            value->nref = POISON_NREF;
            value->hash = 0;
            value->sz = (size_t)sz + 1;

            memcpy(value->data, SDATA(bs, ctx->tokstart), sz);
            *(value->data + sz) = '\0';
        }

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            TRRET(COMPILE_VALUE + 3);
        }

        if (cb != NULL && cb(SDATA(bs, ctx->tokstart), dat, udata) != 0) {
            TRRET(COMPILE_VALUE + 4);
        }

        //LTRACE(ctx->indent, " %s", dat->body);

    } else if (state & LEX_SEQIN) {
        ++(ctx->indent);

        if ((dat = malloc(sizeof(fparser_datum_t) +
                          sizeof(mnarray_t))) == NULL) {
            FAIL("malloc");
        }
        fparser_datum_init(dat, FPARSER_SEQ);

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            TRRET(COMPILE_VALUE + 5);
        }

        ctx->form = dat;

        if (cb != NULL && cb(SDATA(bs, ctx->tokstart), dat, udata) != 0) {
            TRRET(COMPILE_VALUE + 6);
        }


    } else if (state & LEX_SEQOUT) {

        if (ctx->indent <= 0) {
            TRRET(COMPILE_VALUE + 7);
        }
        --(ctx->indent);

        if (ctx->form->parent == NULL) {
            /* root */
            assert(0);
        }
        if (cb != NULL &&
            cb(SDATA(bs, ctx->tokstart), ctx->form, udata) != 0) {
            TRRET(COMPILE_VALUE + 8);
        }
        ctx->form = ctx->form->parent;

    }

    return 0;
}


static int
tokenize(struct tokenizer_ctx *ctx,
         mnbytestream_t *bs,
         int(*cb)(const char *,
                  fparser_datum_t *,
                  void *),
         void *udata)
{
    char ch;

    //D8(buf, buflen);

    for (; SPOS(bs) < SEOD(bs); SINCR(bs)) {
        ch = SPCHR(bs);
        if (ch == '(') {
            if (ctx->state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {
                ctx->state = LEX_SEQIN;

            } else if (ctx->state & LEX_TOK) {
                ctx->state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, bs, ctx->state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 1);
                }
                ctx->state = LEX_SEQIN;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == ')') {
            if (ctx->state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {
                ctx->state = LEX_SEQOUT;

            } else if (ctx->state & LEX_TOK) {
                ctx->state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, bs, ctx->state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 2);
                }
                ctx->state = LEX_SEQOUT;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == ' ' || ch == '\t') {

            if (ctx->state & (LEX_SEQ | LEX_OUT)) {
                ctx->state = LEX_SPACE;

            } else if (ctx->state & LEX_TOK) {
                ctx->state = LEX_TOKOUT;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == '\r' || ch == '\n') {

            if (ctx->state & (LEX_SEQ | LEX_OUT)) {
                ctx->state = LEX_SPACE;

            } else if (ctx->state & LEX_TOK) {
                ctx->state = LEX_TOKOUT;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COM) {
                ctx->state = LEX_COMOUT;

            } else {
                /* noop */
            }

        } else if (ch == '"') {

            if (ctx->state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {
                ctx->state = LEX_QSTRIN;
                ctx->tokstart = SPOS(bs) + 1;

            } else if (ctx->state & LEX_QSTRESC) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & (LEX_QSTRIN | LEX_QSTRMID)) {
                ctx->state = LEX_QSTROUT;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == '\\') {
            if (ctx->state & (LEX_QSTRIN | LEX_QSTRMID)) {
                ctx->state = LEX_QSTRESC;

            } else if (ctx->state & LEX_QSTRESC) {
                ctx->state = LEX_QSTRMID;

            } else {
                /* noop */
            }

        } else if (ch == ';') {

            if (ctx->state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {
                ctx->state = LEX_COMIN;

            } else if (ctx->state & LEX_TOK) {
                ctx->state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, bs, ctx->state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 3);
                }
                ctx->state = LEX_COMIN;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else {
            if (ctx->state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {
                ctx->state = LEX_TOKIN;
                ctx->tokstart = SPOS(bs);

            } else if (ctx->state & LEX_TOKIN) {
                ctx->state = LEX_TOKMID;

            } else if (ctx->state & LEX_QSTR) {
                ctx->state = LEX_QSTRMID;

            } else if (ctx->state & LEX_COMIN) {
                ctx->state = LEX_COMMID;

            } else {
                /* noop */
            }
        }

        if (ctx->state & LEX_FOUNDVAL) {
            if (compile_value(ctx, bs, ctx->state, cb, udata) != 0) {
                TRRET(TOKENIZE + 4);
            }
        }
    }

    return 0;
}

#define FPARSER_PARSE_BODY(tokenize_fn)                                        \
    ssize_t nread;                                                             \
    mnbytestream_t bs;                                                           \
    struct tokenizer_ctx ctx;                                                  \
    fparser_datum_t *root = NULL;                                              \
    ctx.indent = 0;                                                            \
    if ((root = malloc(sizeof(fparser_datum_t) + sizeof(mnarray_t))) == NULL) {  \
        TRRETNULL(FPARSER_PARSE + 1);                                          \
    }                                                                          \
    if (fparser_datum_init(root, FPARSER_SEQ) != 0) {                          \
        TRRETNULL(FPARSER_PARSE + 2);                                          \
    }                                                                          \
    bytestream_init(&bs, BLOCKSZ);                                             \
    ctx.form = root;                                                           \
    ctx.tokstart = SPOS(&bs);                                                  \
    ctx.state = LEX_SPACE;                                                     \
    while (SNEEDMORE(&bs)) {                                                   \
        nread = bytestream_read_more(&bs, fd, BLOCKSZ);                        \
        if (nread <= 0) {                                                      \
            break;                                                             \
        }                                                                      \
        (void)tokenize_fn(&ctx, &bs, cb, udata);                               \
    }                                                                          \
    bytestream_fini(&bs);                                                      \
    return root


fparser_datum_t *
fparser_parse(int fd,
              int (*cb)(const char *,
                        fparser_datum_t *,
                        void *),
              void *udata)
{
    FPARSER_PARSE_BODY(tokenize);
}


static int
fparser_datum_fini(fparser_datum_t *dat)
{
    if (dat->tag == FPARSER_SEQ) {
        mnarray_iter_t it;
        fparser_datum_t **o;

        mnarray_t *form = (mnarray_t *)(&dat->body);

        for (o = array_first(form, &it);
             o != NULL;
             o = array_next(form, &it)) {

            if (*o != NULL) {
                fparser_datum_fini(*o);
                free(*o);
                *o = NULL;
            }
        }
        array_fini(form);
    }
    return 0;
}


int
fparser_datum_dump(fparser_datum_t **dat, void *udata)
{
    fparser_datum_t *rdat = *dat;

    //TRACE("tag=%d", rdat->tag);
    if (rdat->tag == FPARSER_SEQ) {
        mnarray_t *form = (mnarray_t *)(&rdat->body);

        TRACE("SEQ:%ld", form->elnum);

        array_traverse(form, (array_traverser_t)fparser_datum_dump, udata);

    } else if (rdat->tag == FPARSER_VOID) {
        TRACE("VOID");

    } else if (rdat->tag == FPARSER_NULL) {
        TRACE("NULL");

    } else if (rdat->tag == FPARSER_STR) {
        mnbytes_t *v;
        v = (mnbytes_t *)(rdat->body);
        TRACE("STR '%s'", v->data);

    } else if (rdat->tag == FPARSER_WORD) {
        mnbytes_t *v;
        v = (mnbytes_t *)(rdat->body);
        TRACE("WORD '%s'", v->data);

    } else if (rdat->tag == FPARSER_INT) {
        int64_t *v = (int64_t *)rdat->body;
        TRACE("INT '%ld'", *v);

    } else if (rdat->tag == FPARSER_FLOAT) {
        double *v = (double *)rdat->body;
        TRACE("FLOAT '%f'", *v);

    } else if (rdat->tag == FPARSER_BOOL) {
        TRACE("BOOL '#%c'", *((char *)(rdat->body)) ? 't' : 'f');

    } else {
        FAIL("fparser_datum_dump");
    }

    return 0;
}


struct _fparser_datum_dump_info {
    mnarray_t *ar;
    int level;
    mnbytestream_t *bs;
};


static int
checkseq(fparser_datum_t **dat, void *udata)
{
    int *pi = (int *)udata;
    *pi = (*dat)->tag == FPARSER_SEQ;
    return *pi;
}


static int
datum_dump_bytestream(fparser_datum_t **dat, void *udata)
{
    fparser_datum_t *rdat = *dat;
    struct _fparser_datum_dump_info *di = udata;

    if (di->level != 0) {
        if (array_index(di->ar, dat) != 0) {
            bytestream_cat(di->bs, 1, " ");
        }
    }

    if (rdat->tag == FPARSER_SEQ) {
        int hasforms = 0;
        mnarray_t *form = (mnarray_t *)(&rdat->body);

        if (di->level > 0) {
            bytestream_cat(di->bs, 1, "\n");
            if (rdat->error) {
                bytestream_nprintf(di->bs,
                                   di->level * DUMPM_INDENT_SIZE + 10 + 5,
                                   "%*c-->(", di->level * DUMPM_INDENT_SIZE, ' ');
            } else {
                bytestream_nprintf(di->bs,
                                   di->level * DUMPM_INDENT_SIZE + 10 + 2,
                                   "%*c(", di->level * DUMPM_INDENT_SIZE, ' ');
            }
        } else {
            if (rdat->error) {
                bytestream_cat(di->bs, 4, "-->(");
            } else {
                bytestream_cat(di->bs, 1, "(");
            }
        }

        ++(di->level);
        di->ar = form;
        array_traverse(form, (array_traverser_t)datum_dump_bytestream, udata);
        array_traverse(form, (array_traverser_t)checkseq, &hasforms);
        --(di->level);
        if (hasforms) {
            bytestream_cat(di->bs, 1, "\n");
            if (di->level > 0) {
                if (rdat->error) {
                    bytestream_nprintf(di->bs,
                                       di->level * DUMPM_INDENT_SIZE + 10 + 5,
                                       "%*c)<--", di->level * DUMPM_INDENT_SIZE, ' ');
                } else {
                    bytestream_nprintf(di->bs,
                                       di->level * DUMPM_INDENT_SIZE + 10 + 2,
                                       "%*c)", di->level * DUMPM_INDENT_SIZE, ' ');
                }
            } else {
                if (rdat->error) {
                    bytestream_cat(di->bs, 4, ")<--");
                } else {
                    bytestream_cat(di->bs, 1, ")");
                }
            }
        } else {
            if (rdat->error) {
                bytestream_cat(di->bs, 4, ")<--");
            } else {
                bytestream_cat(di->bs, 1, ")");
            }
        }

    } else if (rdat->tag == FPARSER_VOID) {
        if (rdat->error) {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 8, "-->#v<--");
        } else {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 2, "#v");
        }

    } else if (rdat->tag == FPARSER_NULL) {
        if (rdat->error) {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 8, "-->#n<--");
        } else {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 2, "#n");
        }

    } else if (rdat->tag == FPARSER_STR) {
        mnbytes_t *v;
        char *dst;
        size_t sz;
        v = (mnbytes_t *)(rdat->body);
        sz = strlen((char *)(v->data));
        if ((dst = malloc(sz * 2 + 1)) == NULL) {
            FAIL("malloc");
        }
        memset(dst, '\0', sz * 2 + 1);
        fparser_escape(dst, sz * 2, (char *)v->data, sz);
        if (rdat->error) {
            bytestream_nprintf(di->bs, sz * 2 + 10 + 9, "-->\"%s\"<--", dst);
        } else {
            bytestream_nprintf(di->bs, sz * 2 + 10 + 3, "\"%s\"", dst);
        }
        free(dst);

    } else if (rdat->tag == FPARSER_WORD) {
        mnbytes_t *v;
        v = (mnbytes_t *)(rdat->body);
        if (rdat->error) {
            bytestream_nprintf(di->bs, v->sz + 10 + 7, "-->%s<--", v->data);
        } else {
            bytestream_nprintf(di->bs, v->sz + 10 + 1, "%s", v->data);
        }

    } else if (rdat->tag == FPARSER_INT) {
        if (rdat->error) {
            int64_t *v = (int64_t *)(rdat->body);
            bytestream_nprintf(di->bs, 20 + 10 + 7, "-->%ld<--",
                               *v);
        } else {
            int64_t *v = (int64_t *)(rdat->body);
            bytestream_nprintf(di->bs, 20 + 10 + 1, "%ld",
                               *v);
        }

    } else if (rdat->tag == FPARSER_FLOAT) {
        if (rdat->error) {
            double *v = (double *)(rdat->body);
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 7, "-->%f<--",
                               *v);
        } else {
            double *v = (double *)(rdat->body);
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 1, "%f",
                               *v);
        }

    } else if (rdat->tag == FPARSER_BOOL) {
        if (rdat->error) {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 7, "-->#%c<--",
                               *((char *)(rdat->body)) ? 't' : 'f');
        } else {
            bytestream_nprintf(di->bs, 13 * 2 + 10 + 1, "#%c",
                               *((char *)(rdat->body)) ? 't' : 'f');
        }
    }

    return 0;
}


void
fparser_datum_dump_bytestream(fparser_datum_t *dat, mnbytestream_t *bs)
{
    struct _fparser_datum_dump_info di;

    di.level = 0;
    di.ar = NULL;
    di.bs = bs;
    datum_dump_bytestream(&dat, &di);
    //bytestream_cat(bs, 1, "\n");
}


void
fparser_datum_dump_formatted(fparser_datum_t *dat)
{
    mnbytestream_t bs;

    bytestream_init(&bs, 4096);
    fparser_datum_dump_bytestream(dat, &bs);
    bytestream_write(&bs, 2, bs.eod);
    bytestream_fini(&bs);
}


void
fparser_datum_destroy(fparser_datum_t **dat)
{
    if (*dat != NULL) {
        fparser_datum_fini(*dat);
        free(*dat);
        *dat = NULL;
    }
}


static int
fparser_datum_init(fparser_datum_t *dat, fparser_tag_t tag)
{
    dat->tag = tag;
    dat->parent = NULL;
    dat->error = 0;

    if (tag == FPARSER_SEQ) {
        mnarray_t *form = (mnarray_t *)(&dat->body);

        if (array_init(form,
                       sizeof(fparser_datum_t *), 0,
                       NULL, NULL) != 0) {
            FAIL("array_init");
        }

    }

    return 0;
}


int
fparser_datum_form_add(fparser_datum_t *parent, fparser_datum_t *dat)
{
    assert(parent->tag == FPARSER_SEQ);

    //TRACE("Adding %d to %d", dat->tag, parent->tag);

    mnarray_t *form = (mnarray_t *)parent->body;
    fparser_datum_t **entry;

    if ((entry = array_incr(form)) == NULL) {
        TRRET(FPARSER_DATUM_FORM_ADD + 1);
    }
    *entry = dat;
    dat->parent = parent;
    return 0;
}


#define FPARSER_DATUM_BUILD_TY_BODY0(ty, tag, malloc_fn)       \
    fparser_datum_t *dat;                                      \
    if ((dat = malloc_fn(sizeof(fparser_datum_t) +             \
                         sizeof(ty))) == NULL) {               \
        FAIL("malloc");                                        \
    }                                                          \
    fparser_datum_init(dat, tag);                              \
    return dat;                                                \


#define FPARSER_DATUM_BUILD_TY_BODY1(ty, tag, malloc_fn)       \
    fparser_datum_t *dat;                                      \
    ty *value;                                                 \
    if ((dat = malloc_fn(sizeof(fparser_datum_t) +             \
                         sizeof(ty))) == NULL) {               \
        FAIL("malloc");                                        \
    }                                                          \
    fparser_datum_init(dat, tag);                              \
    value = (ty *)(dat->body);                                 \
    *value = val;                                              \
    return dat;                                                \


fparser_datum_t *
fparser_datum_build_void(void)
{
    FPARSER_DATUM_BUILD_TY_BODY0(int64_t, FPARSER_VOID, malloc);
}


fparser_datum_t *
fparser_datum_build_null(void)
{
    FPARSER_DATUM_BUILD_TY_BODY0(int64_t, FPARSER_NULL, malloc);
}


fparser_datum_t *
fparser_datum_build_int(int64_t val)
{
    FPARSER_DATUM_BUILD_TY_BODY1(int64_t, FPARSER_INT, malloc);
}


fparser_datum_t *
fparser_datum_build_float(double val)
{
    FPARSER_DATUM_BUILD_TY_BODY1(double, FPARSER_FLOAT, malloc);
}


fparser_datum_t *
fparser_datum_build_bool(char val)
{
    FPARSER_DATUM_BUILD_TY_BODY1(char, FPARSER_BOOL, malloc);
}


#define FPARSER_DATUM_BUILD_STR_BODY(malloc_fn, tag)           \
    fparser_datum_t *dat;                                      \
    mnbytes_t *value;                                            \
    size_t sz;                                                 \
    ssize_t escaped;                                           \
    sz = strlen(str);                                          \
    if ((dat = malloc_fn(sizeof(fparser_datum_t) +             \
                         sizeof(mnbytes_t) +                     \
                         sz +                                  \
                         1)) == NULL) {                        \
        FAIL("malloc");                                        \
    }                                                          \
    fparser_datum_init(dat, tag);                              \
    value = (mnbytes_t *)(dat->body);                            \
    value->nref = POISON_NREF;                                           \
    value->hash = 0;                                           \
    value->sz = sz + 1;                                        \
    escaped = fparser_unescape((char *)value->data, str, sz);  \
    value->sz -= escaped;                                      \
    return dat;


fparser_datum_t *
fparser_datum_build_word(const char *str)
{
    FPARSER_DATUM_BUILD_STR_BODY(malloc, FPARSER_WORD);
}


fparser_datum_t *
fparser_datum_build_str(const char *str)
{
    FPARSER_DATUM_BUILD_STR_BODY(malloc, FPARSER_STR);
}

#define FPARSER_DATUM_BUILD_STR_BUF_BODY(malloc_fn)            \
    fparser_datum_t *dat;                                      \
    mnbytes_t *value;                                            \
    ssize_t escaped;                                           \
    if ((dat = malloc_fn(sizeof(fparser_datum_t) +             \
                         sizeof(mnbytes_t) +                     \
                         sz)) == NULL) {                       \
        FAIL("malloc");                                        \
    }                                                          \
    fparser_datum_init(dat, FPARSER_STR);                      \
    value = (mnbytes_t *)(dat->body);                            \
    value->nref = POISON_NREF;                                           \
    value->hash = 0;                                           \
    value->sz = sz;                                            \
    escaped = fparser_unescape((char *)value->data, str, sz);  \
    value->sz -= escaped;                                      \
    return dat;


fparser_datum_t *
fparser_datum_build_str_buf(const char *str, size_t sz)
{
    FPARSER_DATUM_BUILD_STR_BUF_BODY(malloc);
}

#define FPARSER_DATUM_BUILD_SEQ_BODY(malloc_fn)        \
    fparser_datum_t *dat;                              \
    if ((dat = malloc_fn(sizeof(fparser_datum_t) +     \
                         sizeof(mnarray_t))) == NULL) {  \
        FAIL("malloc");                                \
    }                                                  \
    fparser_datum_init(dat, FPARSER_SEQ);              \
    return dat;


fparser_datum_t *
fparser_datum_build_seq(void)
{
    FPARSER_DATUM_BUILD_SEQ_BODY(malloc);
}

/*
 * vim:softtabstop=4
 */
