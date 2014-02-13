#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mrkcommon/fasthash.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/util.h>

uint64_t
bytes_hash(bytes_t *bytes)
{
    if (bytes->hash == 0) {
        bytes->hash = fasthash(0, bytes->data, bytes->sz);
    }
    return bytes->hash;
}

int
bytes_cmp(bytes_t *a, bytes_t *b)
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

