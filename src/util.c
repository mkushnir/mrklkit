#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mrkcommon/util.h>
#include "diag.h"

static uint64_t newvar_ctr = 0;

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

