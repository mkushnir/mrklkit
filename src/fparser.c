#include <assert.h>
#include <limits.h>
#include <stdlib.h>

//#define TRRET_DEBUG
#include "mrkcommon/array.h"
#define DUMPM_INDENT_SIZE 1
#include "mrkcommon/dumpm.h"
#include "mrkcommon/util.h"

#include "diag.h"
#include "fparser_private.h"

#define BLOCKSZ (4096 * 8)
#define NEEDMORE (-1)

static int fparser_datum_init(fparser_datum_t *, fparser_tag_t);
static int fparser_datum_fini(fparser_datum_t *);
static int fparser_datum_form_add(fparser_datum_t *, fparser_datum_t *);


static void
_unescape(unsigned char *dst, const unsigned char *src, ssize_t sz)
{
    ssize_t i;
    int j;
    char ch;

    for (i = 0, j = 0; i < sz; ++i, ++j) {
        ch = *(src + i);
        if (ch == '\\') {
            ++i;
            if (i < sz) {
                *(dst + j) = *(src + i);
            }
        } else {
            *(dst + j) = ch;
        }
    }
    *(dst + j) = '\0';
}

void
fparser_escape(unsigned char *dst,
               size_t dst_sz,
               const unsigned char *src,
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
              const unsigned char *buf,
              ssize_t idx,
              int state,
              int (*cb)(const unsigned char *,
                        fparser_datum_t *,
                        void *),
              void *udata)
{
    fparser_datum_t *dat;

    if (state == LEX_TOKIN) {
        ctx->tokstart = buf + idx;

    } else if (state == LEX_QSTRIN) {
        ctx->tokstart = buf + idx + 1;

    } else if (state & (LEX_QSTROUT)) {
        ssize_t sz = buf + idx - ctx->tokstart;
        bytes_t *value;

        if ((dat = malloc(sizeof(fparser_datum_t) + sizeof(bytes_t) + sz + 1)) == NULL) {
            perror("malloc");
            return 1;
        }
        if (fparser_datum_init(dat, FPARSER_STR) != 0) {
            perror("fparser_datum_init");
            return 1;
        }
        value = (bytes_t *)(dat->body);
        value->sz = (size_t)sz;

        _unescape(value->data, ctx->tokstart, sz);

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            perror("fparser_datum_form_add");
            return 1;
        }

        if (cb != NULL && cb(ctx->tokstart, dat, udata) != 0) {
            return 1;
        }

        //LTRACE(ctx->indent, " '%s'", dat->body);

    } else if (state & (LEX_TOKOUT)) {
        unsigned char ch = ctx->tokstart[0];
        ssize_t sz = buf + idx - ctx->tokstart;
#define _NUMKIND_INT 1
#define _NUMKIND_FLOAT 2
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
                    if (ctx->tokstart[i] == '.') {
                        numkind = _NUMKIND_FLOAT;
                    } else if (ctx->tokstart[i] < '0' || ctx->tokstart[i] > '9') {
                        numkind = 0;
                        break;
                    }
                }
            }
        } else {
            if (ch >= '0' && ch <= '9') {
                numkind = _NUMKIND_INT;
            }
        }

        if (numkind == _NUMKIND_INT) {
            int64_t *value;

            if ((dat = malloc(sizeof(fparser_datum_t) + sizeof(int64_t))) == NULL) {
                perror("malloc");
                return 1;
            }
            if (fparser_datum_init(dat, FPARSER_INT) != 0) {
                perror("fparser_datum_init");
                return 1;
            }
            value = (int64_t *)(dat->body);
            *value = (int64_t)strtoll((const char *)ctx->tokstart, NULL, 10);
        } else if (numkind == _NUMKIND_FLOAT) {
            double *value;

            if ((dat = malloc(sizeof(fparser_datum_t) + sizeof(double))) == NULL) {
                perror("malloc");
                return 1;
            }
            if (fparser_datum_init(dat, FPARSER_FLOAT) != 0) {
                perror("fparser_datum_init");
                return 1;
            }
            value = (double *)(dat->body);
            *value = strtod((const char *)ctx->tokstart, NULL);
        } else {
            bytes_t *value;

            if ((dat = malloc(sizeof(fparser_datum_t) + sizeof(bytes_t) + sz + 1)) == NULL) {
                perror("malloc");
                return 1;
            }
            if (fparser_datum_init(dat, FPARSER_WORD) != 0) {
                perror("fparser_datum_init");
                return 1;
            }
            value = (bytes_t *)(dat->body);
            value->sz = (size_t)sz;

            memcpy(value->data, ctx->tokstart, sz);
            *(value->data + sz) = '\0';
        }

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            perror("fparser_datum_form_add");
            return 1;
        }

        if (cb != NULL && cb(ctx->tokstart, dat, udata) != 0) {
            return 1;
        }

        //LTRACE(ctx->indent, " %s", dat->body);

    } else if (state & LEX_SEQIN) {
        ++(ctx->indent);

        if ((dat = malloc(sizeof(fparser_datum_t) +
                          sizeof(array_t))) == NULL) {
            perror("malloc");
            return 1;
        }
        if (fparser_datum_init(dat, FPARSER_SEQ) != 0) {
            perror("fparser_datum_init");
            return 1;
        }

        if (fparser_datum_form_add(ctx->form, dat) != 0) {
            perror("fparser_datum_form_add");
            return 1;
        }

        ctx->form = dat;

        ctx->form->seqout = 0;
        if (cb != NULL && cb(ctx->tokstart, dat, udata) != 0) {
            return 1;
        }


    } else if (state & LEX_SEQOUT) {

        if (ctx->indent <= 0) {
            return 1;
        }
        --(ctx->indent);

        if (ctx->form->parent == NULL) {
            /* root */
            assert(0);
        }
        ctx->form->seqout = 1;
        if (cb != NULL && cb(ctx->tokstart, ctx->form, udata) != 0) {
            return 1;
        }
        ctx->form = ctx->form->parent;

    }

    return 0;
}

static int
tokenize(struct tokenizer_ctx *ctx,
         const unsigned char *buf, ssize_t buflen,
         int(*cb)(const unsigned char *, fparser_datum_t *, void *),
         void *udata)
{
    unsigned char ch;
    ssize_t i;
    int state = LEX_SPACE;

    //D8(buf, buflen);

    for (i = 0; i < buflen; ++i) {
        ch = *(buf + i);
        if (ch == '(') {
            if (state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {

                state = LEX_SEQIN;

            } else if (state & LEX_TOK) {

                state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, buf, i, state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 1);
                }
                state = LEX_SEQIN;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == ')') {
            if (state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {

                state = LEX_SEQOUT;

            } else if (state & LEX_TOK) {

                state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, buf, i, state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 2);
                }
                state = LEX_SEQOUT;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == ' ' || ch == '\t') {

            if (state & (LEX_SEQ | LEX_OUT)) {

                state = LEX_SPACE;

            } else if (state & LEX_TOK) {

                state = LEX_TOKOUT;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == '\r' || ch == '\n') {

            if (state & (LEX_SEQ | LEX_OUT)) {

                state = LEX_SPACE;

            } else if (state & LEX_TOK) {

                state = LEX_TOKOUT;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COM) {

                state = LEX_COMOUT;

            } else {
                /* noop */
            }

        } else if (ch == '"') {

            if (state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {

                state = LEX_QSTRIN;

            } else if (state & LEX_QSTR) {

                state = LEX_QSTROUT;

            } else if (state & LEX_QSTRESC) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else if (ch == '\\') {
            if (state & LEX_QSTR) {

                state = LEX_QSTRESC;

            } else if (state & LEX_QSTRESC) {

                state = LEX_QSTRMID;

            } else {
                /* noop */
            }

        } else if (ch == ';') {

            if (state & (LEX_SEQ | LEX_OUT | LEX_SPACE)) {

                state = LEX_COMIN;

            } else if (state & LEX_TOK) {

                state = LEX_TOKOUT;
                /* extra call back */
                if (compile_value(ctx, buf, i, state, cb, udata) != 0) {
                    TRRET(TOKENIZE + 3);
                }
                state = LEX_COMIN;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }

        } else {
            if (state & (LEX_SEQ | LEX_SPACE | LEX_OUT)) {

                state = LEX_TOKIN;

            } else if (state & LEX_TOKIN) {

                state = LEX_TOKMID;

            } else if (state & (LEX_QSTRIN | LEX_QSTRESC)) {

                state = LEX_QSTRMID;

            } else if (state & LEX_COMIN) {

                state = LEX_COMMID;

            } else {
                /* noop */
            }
        }

        if (state & LEX_FOUNDVAL) {
            if (compile_value(ctx, buf, i, state, cb, udata) != 0) {
                TRRET(TOKENIZE + 4);
            }
        }
    }

    return NEEDMORE;
}


static int
fparser_datum_fini(fparser_datum_t *dat)
{
    if (dat->tag == FPARSER_SEQ) {
        array_iter_t it;
        fparser_datum_t **o;

        array_t *form = (array_t *)(&dat->body);

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

        array_t *form = (array_t *)(&rdat->body);

        TRACE("%s form len %ld", rdat->seqout? "<<<" : ">>>", form->elnum);

        array_traverse(form, (array_traverser_t)fparser_datum_dump, udata);

    } else if (rdat->tag == FPARSER_STR) {
        bytes_t *v;
        v = (bytes_t *)(rdat->body);
        TRACE("STR '%s'", v->data);

    } else if (rdat->tag == FPARSER_WORD) {
        bytes_t *v;
        v = (bytes_t *)(rdat->body);
        TRACE("WORD '%s'", v->data);

    } else if (rdat->tag == FPARSER_INT) {
        TRACE("INT '%ld'", *((int64_t *)(rdat->body)));

    } else if (rdat->tag == FPARSER_FLOAT) {
        TRACE("FLOAT '%f'", *((double *)(rdat->body)));
    }

    return 0;
}

struct _fparser_datum_dump_info {
    array_t *ar;
    int level;
};

static int
checkseq(fparser_datum_t **dat, void *udata)
{
    int *pi = (int *)udata;
    *pi = (*dat)->tag == FPARSER_SEQ;
    return *pi;
}

static int
fparser_datum_dump2(fparser_datum_t **dat, void *udata)
{
    fparser_datum_t *rdat = *dat;
    struct _fparser_datum_dump_info *di = udata;

    if (di->level != 0) {
        if (array_index(di->ar, dat) != 0) {
            TRACEC(" ");
        }
    }

    //TRACE("tag=%d", rdat->tag);
    if (rdat->tag == FPARSER_SEQ) {
        int hasforms = 0;
        array_t *form = (array_t *)(&rdat->body);

        if (di->level > 0) {
            TRACEC("\n");
            if (rdat->error) {
                LTRACEN(di->level, "-->(");
            } else {
                LTRACEN(di->level, "(");
            }
        } else {
            if (rdat->error) {
                TRACEC("(");
            } else {
                TRACEC("(");
            }
        }

        ++(di->level);
        di->ar = form;
        array_traverse(form, (array_traverser_t)fparser_datum_dump2, udata);
        array_traverse(form, (array_traverser_t)checkseq, &hasforms);
        --(di->level);
        if (hasforms) {
            TRACEC("\n");
            if (di->level > 0) {
                if (rdat->error) {
                    LTRACEN(di->level, ")<--");
                } else {
                    LTRACEN(di->level, ")");
                }
            } else {
                if (rdat->error) {
                    TRACEC(")<--");
                } else {
                    TRACEC(")");
                }
            }
        } else {
            if (rdat->error) {
                TRACEC(")<--");
            } else {
                TRACEC(")");
            }
        }

    } else if (rdat->tag == FPARSER_STR) {
        bytes_t *v;
        unsigned char *dst;
        size_t sz;
        v = (bytes_t *)(rdat->body);
        sz = strlen((char *)(v->data));
        if ((dst = malloc(sz * 2 + 1)) == NULL) {
            FAIL("malloc");
        }
        memset(dst, '\0', sz * 2 + 1);
        fparser_escape(dst, sz * 2, v->data, sz);
        if (rdat->error) {
            TRACEC("-->\"%s\"<--", dst);
        } else {
            TRACEC("\"%s\"", dst);
        }
        free(dst);

    } else if (rdat->tag == FPARSER_WORD) {
        bytes_t *v;
        v = (bytes_t *)(rdat->body);
        if (rdat->error) {
            TRACEC("-->%s<--", v->data);
        } else {
            TRACEC("%s", v->data);
        }

    } else if (rdat->tag == FPARSER_INT) {
        if (rdat->error) {
            TRACEC("-->%ld<--", *((int64_t *)(rdat->body)));
        } else {
            TRACEC("%ld", *((int64_t *)(rdat->body)));
        }

    } else if (rdat->tag == FPARSER_FLOAT) {
        if (rdat->error) {
            TRACEC("-->%f<--", *((double *)(rdat->body)));
        } else {
            TRACEC("%f", *((double *)(rdat->body)));
        }
    }

    return 0;
}

void
fparser_datum_dump_formatted(fparser_datum_t *dat)
{
    struct _fparser_datum_dump_info di;

    di.level = 0;
    di.ar = NULL;
    fparser_datum_dump2(&dat, &di);
    TRACEC("\n");
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
    dat->seqout = 0;
    dat->error = 0;

    if (tag == FPARSER_SEQ) {

        array_t *form = (array_t *)(&dat->body);

        if (array_init(form,
                       sizeof(fparser_datum_t *), 0,
                       NULL, NULL) != 0) {

            TRRET(FPARSER_DATUM_INIT + 1);
        }

    }

    return 0;
}

static int
fparser_datum_form_add(fparser_datum_t *parent, fparser_datum_t *dat)
{
    assert(parent->tag == FPARSER_SEQ);

    //TRACE("Adding %d to %d", dat->tag, parent->tag);

    array_t *form = (array_t *)parent->body;
    fparser_datum_t **entry;

    if ((entry = array_incr(form)) == NULL) {
        TRRET(FPARSER_DATUM_FORM_ADD + 1);
    }
    *entry = dat;
    dat->parent = parent;
    return 0;
}

fparser_datum_t *
fparser_parse(int fd,
              int (*cb)(const unsigned char *,
                        fparser_datum_t *,
                        void *),
              void *udata)
{
    int res;
    ssize_t nread;
    unsigned char buf[BLOCKSZ];
    struct tokenizer_ctx ctx;
    fparser_datum_t *root = NULL;

    ctx.indent = 0;
    res = 0;

    if ((root = malloc(sizeof(fparser_datum_t) + sizeof(array_t))) == NULL) {
        TRRETNULL(FPARSER_PARSE + 1);
    }

    if (fparser_datum_init(root, FPARSER_SEQ) != 0) {
        TRRETNULL(FPARSER_PARSE + 2);
    }

    ctx.form = root;

    while (1) {
        if ((nread = read(fd, buf, BLOCKSZ)) <= 0) {

            if (nread < 0) {
                TRRETNULL(FPARSER_PARSE + 3);

            } else {
                break;
            }

        }

        if ((res = tokenize(&ctx, buf, nread, cb, udata)) == NEEDMORE) {
            continue;
        }
    }

    return root;
}
/*
 * vim:softtabstop=4
 */
