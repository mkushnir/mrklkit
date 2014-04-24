#ifndef DSOURCE_H_DEFINED
#define DSOURCE_H_DEFINED

#include <mrklkit/ltype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dsource {
    int timestamp_index;
    int date_index;
    int time_index;
    int duration_index;
    int error:1;
    /* weak ref*/
    unsigned char *logtype;
    lkit_struct_t *_struct;
    char rdelim[2];
    char fdelim;
    uint64_t parse_flags;
} dsource_t;

int dsource_parse(array_t *, array_iter_t *);
dsource_t *dsource_get(const char *);

void dsource_init_module(void);
void dsource_fini_module(void);

#ifdef __cplusplus
}
#endif
#endif /* DSOURCE_H_DEFINED */
