#include <stdint.h>

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

