#ifndef FPARSER_H
#define FPARSER_H

/**
 * Lisp-like form parser.
 */
#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>

#include <mrklkit/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEX_SPACE    0x00000001
#define LEX_SEQIN    0x00000002
#define LEX_SEQOUT   0x00000004
#define LEX_TOKIN    0x00000008
#define LEX_TOKMID   0x00000010
#define LEX_TOKOUT   0x00000020
#define LEX_COMIN    0x00000040
#define LEX_COMMID   0x00000080
#define LEX_COMOUT   0x00000100
#define LEX_QSTRIN   0x00000200
#define LEX_QSTRMID  0x00000400
#define LEX_QSTROUT  0x00000800
#define LEX_QSTRESC  0x00001000

#define LEX_SEQ      (LEX_SEQIN | LEX_SEQOUT)
#define LEX_OUT      (LEX_TOKOUT | LEX_QSTROUT | LEX_COMOUT)
#define LEX_TOK      (LEX_TOKIN | LEX_TOKMID)
#define LEX_QSTR     (LEX_QSTRIN | LEX_QSTRMID)
#define LEX_COM      (LEX_COMIN | LEX_COMMID)

#define LEX_FOUNDVAL   (LEX_SEQ | LEX_TOKIN | LEX_TOKOUT | LEX_QSTRIN | LEX_QSTROUT)

#define LEXSTR(s) ( \
    (s) == LEX_SPACE ? "SPACE" : \
    (s) == LEX_SEQIN ? "SEQIN" : \
    (s) == LEX_SEQOUT ? "SEQOUT" : \
    (s) == LEX_TOKIN ? "TOKIN" : \
    (s) == LEX_TOKMID ? "TOKMID" : \
    (s) == LEX_TOKOUT ? "TOKOUT" : \
    (s) == LEX_COMIN ? "COMIN" : \
    (s) == LEX_COMMID ? "COMMID" : \
    (s) == LEX_COMOUT ? "COMOUT" : \
    (s) == LEX_QSTRIN ? "QSTRIN" : \
    (s) == LEX_QSTRMID ? "QSTRMID" : \
    (s) == LEX_QSTROUT ? "QSTROUT" : \
    (s) == LEX_QSTRESC ? "QSTRESC" : \
    "<unknown>" \
)

typedef enum {
    FPARSER_STR,
    FPARSER_WORD,
    FPARSER_INT,
    FPARSER_FLOAT,
    FPARSER_BOOL,
    FPARSER_SEQ,
} fparser_tag_t;

#define FPARSER_TAG_STR(tag) ( \
    (tag) == FPARSER_STR ? "STR" : \
    (tag) == FPARSER_WORD ? "WORD" : \
    (tag) == FPARSER_INT ? "INT" : \
    (tag) == FPARSER_FLOAT ? "FLOAT" : \
    (tag) == FPARSER_BOOL ? "BOOL" : \
    (tag) == FPARSER_SEQ ? "SEQ" : \
    "<unknown>" \
)

typedef struct _fparser_datum {
    fparser_tag_t tag;
    struct _fparser_datum *parent;
    int seqout:1;
    int error:1;
    unsigned char body[];
} fparser_datum_t;

#define FPARSER_DATUM_TAG(dat) ((dat)->tag)

void fparser_escape(unsigned char *, size_t, const unsigned char *, size_t);

int fparser_datum_dump(fparser_datum_t **, void *);
void fparser_datum_dump_formatted(fparser_datum_t *);
void fparser_datum_dump_bytestream(fparser_datum_t *, bytestream_t *);


void fparser_datum_destroy(fparser_datum_t **);

fparser_datum_t *fparser_parse(int fd,
                               int (*cb)(const unsigned char *,
                                         fparser_datum_t *,
                                         void *),
                               void *udata);

#ifdef __cplusplus
}
#endif

#endif

