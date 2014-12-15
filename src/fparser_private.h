#ifndef FPARSER_PRIVATE_H
#define FPARSER_PRIVATE_H

#include <mrklkit/fparser.h>
#include <mrkcommon/bytestream.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _fparser_datum;

struct tokenizer_ctx {
    struct _fparser_datum *form;
    off_t tokstart;
    int indent;
    int state;
#define FPARSER_QSTRMODE_S (0)
#define FPARSER_QSTRMODE_D (-1)
    int qstrmode:1;
};

#ifdef __cplusplus
}
#endif

#endif
