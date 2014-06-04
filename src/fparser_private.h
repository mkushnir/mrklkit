#ifndef FPARSER_PRIVATE_H
#define FPARSER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

struct _fparser_datum;

struct tokenizer_ctx {
    const char *tokstart;
    int indent;
    struct _fparser_datum *form;
};

#ifdef __cplusplus
}
#endif

#include <mrklkit/fparser.h>
#endif
