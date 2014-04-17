#ifndef TESTRT_H_DEFINED
#define TESTRT_H_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

int testrt_parse(array_t *, array_iter_t *);
int testrt_compile(LLVMModuleRef);

void testrt_init(void);
void testrt_fini(void);

extern mrklkit_module_t testrt_module;

#ifdef __cplusplus
}
#endif
#endif /* TESTRT_H_DEFINED */
