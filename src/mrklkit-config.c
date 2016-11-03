#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include "config.h"

static int includes = 0;
static int libs = 0;
static int version = 0;
static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
#define MRKLKIT_CONFIG_LOINCLUES 1
    {"includes", no_argument, &includes, 1},
#define MRKLKIT_CONFIG_LOLIBS 2
    {"libs", no_argument, &libs, 1},
#define MRKLKIT_CONFIG_LOVERSION 3
    {"version", no_argument, &version, 1},
    {NULL, 0, NULL, 0},
};


static void
usage(char *p)
{
    printf(
"Usage: %s OPTIONS\n"
"\n"
"Options:\n"
"  --includes       Print include paths.\n"
"  --libs           Print library files.\n"
"  --version        Print package version.\n",
        p);
}


int
main(int argc, char **argv)
{
    char ch;
    int optidx;

    while ((ch = getopt_long(argc, argv, "", longopts, &optidx)) != -1) {
        switch (ch) {
        case 'h':
            usage(argv[0]);
            break;

        case 1:
        case 0:
            break;

        default:
            usage(argv[0]);
            exit(1);
        }
    }

    if (includes) {
        printf(MRKLKIT_CONFIG_INCLUDES);
        printf("\n");
    }
    if (libs) {
        printf(MRKLKIT_CONFIG_LIBS);
        printf("\n");
    }
    if (version) {
        printf(VERSION);
        printf("\n");
    }

    return 0;
}
