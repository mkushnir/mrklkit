
#ifndef DSOURCE_H_DEFINED
#define DSOURCE_H_DEFINED

#include "ltype.h"

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
    lkit_struct_t *fields;
} dsource_t;

int dsource_parse(array_t *, array_iter_t *);

void dsource_init_module(void);
void dsource_fini_module(void);

#ifdef __cplusplus
}
#endif
#endif /* DSOURCE_H_DEFINED */
