#include <stdint.h>
#include <stdio.h>

#include <mrkcommon/fasthash.h>
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

char *newvar(char *buf, size_t sz, const char *prefix)
{
    static uint64_t idx = 0;
    static char mybuf[1024];

    if (buf == NULL) {
        buf = mybuf;
        sz = sizeof(mybuf);
    }
    snprintf(buf, sz, "%s%ld", prefix, ++idx);
    return buf;
}

