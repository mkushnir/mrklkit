#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mrkcommon/fasthash.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/util.h>

#include "diag.h"


uint64_t
mrklkit_bytes_hash(bytes_t *bytes)
{
    if (bytes->hash == 0) {
        bytes->hash = fasthash(0, bytes->data, bytes->sz);
    }
    return bytes->hash;
}


int
mrklkit_bytes_cmp(bytes_t *a, bytes_t *b)
{
    uint64_t ha, hb;
    int diff;

    ha = bytes_hash(a);
    hb = bytes_hash(b);
    diff = (int)(ha - hb);
    if (diff == 0) {
        diff = (int) (a->sz - b->sz);
        if (diff == 0) {
            return memcmp(a->data, b->data, a->sz);
        }
        return diff;
    }
    return diff;
}


bytes_t *
mrklkit_bytes_new(size_t sz)
{
    size_t mod, msz;
    bytes_t *res;

    assert(sz > 0);

    msz = sz;
    mod = sz % 8;
    if (mod) {
        msz += (8 - mod);
    } else {
        msz += 8;
    }
    if ((res = malloc(sizeof(bytes_t) + msz)) == NULL) {
        FAIL("malloc");
    }
    res->nref = 0;
    res->sz = sz;
    res->hash = 0;
    //TRACE("%p", res);
    return res;
}


void
mrklkit_bytes_copy(bytes_t *dst, bytes_t *src, size_t off)
{
    assert((off + src->sz) <= dst->sz);
    memcpy(dst->data + off, src->data, src->sz);
}


bytes_t *
mrklkit_bytes_new_from_str(const char *s)
{
    bytes_t *res;
    size_t mod, msz;
    size_t sz;

    sz = strlen(s) + 1;

    msz = sz;
    mod = sz % 8;
    if (mod) {
        msz += (8 - mod);
    } else {
        msz += 8;
    }
    if ((res = malloc(sizeof(bytes_t) + msz)) == NULL) {
        FAIL("malloc");
    }
    memcpy(res->data, s, sz);
    res->nref = 0;
    res->sz = sz;
    res->hash = 0;
    //TRACE("%p sz=%ld data=%s", res, res->sz, res->data);
    return res;
}


void
mrklkit_bytes_decref(bytes_t **value)
{
    BYTES_DECREF(value);
}


void
mrklkit_bytes_decref_fast(bytes_t *value)
{
    BYTES_DECREF_FAST(value);
}


void
mrklkit_bytes_incref(bytes_t *value)
{
    BYTES_INCREF(value);
}


char *newvar(char *buf, size_t sz, const char *prefix)
{
    static uint64_t idx = 0;
    static char mybuf[1024];

    if (buf == NULL) {
        buf = mybuf;
        sz = sizeof(mybuf);
    }
    snprintf(buf, sz, "%s.%ld", prefix, ++idx);
    return buf;
}

