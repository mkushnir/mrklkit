#ifndef FPARSER_PRIVATE_H
#define FPARSER_PRIVATE_H

#include <mrklkit/fparser.h>
#include <mrkcommon/bytestream.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _fparser_datum;

struct tokenizer_ctx {
    fparser_tag_t state;
    off_t tokstart;
    int indent;
    struct _fparser_datum *form;
};

#ifdef __cplusplus
}
#endif

#endif
