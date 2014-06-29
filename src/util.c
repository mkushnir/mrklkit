#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mrkcommon/fasthash.h>
#include <mrkcommon/mpool.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/util.h>

#include "diag.h"

static uint64_t newvar_ctr = 0;


bytes_t *
mrklkit_bytes_json_escape(bytes_t *src)
{
    size_t i, j;
    bytes_t *dest;

    /* partial json string support */
    dest = bytes_new(src->sz * 2);
    for (i = 0, j = 0; i < src->sz; ++i, ++j) {
        unsigned char ch;

        ch = src->data[i];
        if (ch == '\\' || ch == '"') {
            dest->data[j++] = '\\';
        } else if (ch == '\b') {
            ch = 'b';
            dest->data[j++] = '\\';
        } else if (ch == '\f') {
            ch = 'f';
            dest->data[j++] = '\\';
        } else if (ch == '\n') {
            ch = 'n';
            dest->data[j++] = '\\';
        } else if (ch == '\r') {
            ch = 'r';
        } else if (ch == '\t') {
            ch = 't';
            dest->data[j++] = '\\';
        }
        dest->data[j] = src->data[i];
    }
    dest->data[j] = '\0';
    dest->sz = j + 1;
    return dest;
}


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
    int64_t diff;

    ha = bytes_hash(a);
    hb = bytes_hash(b);
    diff = (int64_t)(ha - hb);
    if (diff == 0) {
        diff =  (a->sz - b->sz);
        if (diff == 0) {
            return memcmp(a->data, b->data, a->sz);
        }
    }
    return diff > 0 ? 1 : -1;
}


#define MRKLKIT_BYTES_NEW_BODY(malloc_fn) \
    size_t mod, msz; \
    bytes_t *res; \
    assert(sz > 0); \
    msz = sz; \
    mod = sz % 8; \
    if (mod) { \
        msz += (8 - mod); \
    } else { \
        msz += 8; \
    } \
    if ((res = malloc_fn(sizeof(bytes_t) + msz)) == NULL) { \
        FAIL("malloc"); \
    } \
    res->nref = 0; \
    res->sz = sz; \
    res->hash = 0; \
    return res

#define MRKLKIT_BYTES_NEW_FROM_STR_BODY(malloc_fn) \
    bytes_t *res; \
    size_t mod, msz; \
    size_t sz; \
    sz = strlen(s) + 1; \
    msz = sz; \
    mod = sz % 8; \
    if (mod) { \
        msz += (8 - mod); \
    } else { \
        msz += 8; \
    } \
    if ((res = malloc_fn(sizeof(bytes_t) + msz)) == NULL) { \
        FAIL("malloc"); \
    } \
    memcpy(res->data, s, sz); \
    res->nref = 0; \
    res->sz = sz; \
    res->hash = 0; \
    return res


bytes_t *
mrklkit_bytes_new(size_t sz)
{
    MRKLKIT_BYTES_NEW_BODY(malloc);
}


bytes_t *
mrklkit_bytes_new_from_str(const char *s)
{
    MRKLKIT_BYTES_NEW_FROM_STR_BODY(malloc);
}


#define _malloc(sz) mpool_malloc(mpool, (sz))
bytes_t *
mrklkit_bytes_new_mpool(mpool_ctx_t *mpool, size_t sz)
{
    MRKLKIT_BYTES_NEW_BODY(_malloc);
}
bytes_t *
mrklkit_bytes_new_from_str_mpool(mpool_ctx_t *mpool, const char *s)
{
    MRKLKIT_BYTES_NEW_FROM_STR_BODY(_malloc);
}
#undef _malloc


void
mrklkit_bytes_copy(bytes_t *dst, bytes_t *src, size_t off)
{
    assert((off + src->sz) <= dst->sz);
    memcpy(dst->data + off, src->data, src->sz);
}


void
mrklkit_bytes_brushdown(bytes_t *str)
{
    unsigned char *src, *dst;
    unsigned char *end;

    /*
     * expect alpha-numeric words optionally surrounded by spaces
     */
    end = str->data + str->sz;
    for (src = str->data, dst = str->data;
         src < end;
         ++src, ++dst) {
        if (*src == '%' && (src + 2) < end) {
            ++src;
            if (*src >= '0' && *src <= '9') {
                *dst = (*src - '0') << 4;
                //TRACE("*dst='%02hhd'", *dst);
            } else if (*src >= 'A' && *src <= 'F') {
                *dst = (*src - '7') << 4;
                //TRACE("*dst='%02hhd'", *dst);
            } else if (*src >= 'a' && *src <= 'f') {
                *dst = (*src - 'w') << 4;
                //TRACE("*dst='%02hhd'", *dst);
            } else {
                /* ignore invalid sequence */
            }
            ++src;
            if (*src >= '0' && *src <= '9') {
                *dst |= (*src - '0');
                //TRACE("*dst='%02hhd'", *dst);
            } else if (*src >= 'A' && *src <= 'F') {
                *dst |= (*src - '7');
                //TRACE("*dst='%02hhd'", *dst);
            } else if (*src >= 'a' && *src <= 'f') {
                *dst |= (*src - 'w');
                //TRACE("*dst='%02hhd'", *dst);
            } else {
                /* ignore invalid sequence */
            }
        } else {
            *dst = *src;
        }
        if (*dst == ' ') {
            /* discard SPACE */
            --dst;
        }
    }
    *(dst - 1) = '\0';
    str->sz = (intptr_t)(dst - str->data);
    str->hash = 0;
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


void
reset_newvar_counter(void)
{
    newvar_ctr = 0;
}


char *newvar(char *buf, size_t sz, const char *prefix)
{
    static char mybuf[1024];

    if (buf == NULL) {
        buf = mybuf;
        sz = sizeof(mybuf);
    }
    snprintf(buf, sz, "%s.%ld", prefix, ++newvar_ctr);
    return buf;
}

