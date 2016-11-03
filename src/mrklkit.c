#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Linker.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/hash.h>
//#define TRRET_DEBUG_VERBOSE
#define TRRET_DEBUG
#include <mrkcommon/dumpm.h>
#define NO_PROFILE
#include <mrkcommon/profile.h>
#include <mrkcommon/util.h>

#include <mrklkit/fparser.h>
#include <mrklkit/lparse.h>
#include <mrklkit/ltype.h>
#include <mrklkit/ltypegen.h>
#include <mrklkit/lexpr.h>
#include <mrklkit/module.h>
#include <mrklkit/mrklkit.h>

#include <mrklkit/lruntime.h>

#include "diag.h"

#ifdef DO_MEMDEBUG
#include <mrkcommon/memdebug.h>
MEMDEBUG_DECLARE(mrklkit);
#endif

#ifndef LLVM_VERSION_NUM
#error LLVM_VERSION_NUM must be defined to %d%03d, llvm_version_mjr, llvm_version_mnr
#endif

UNUSED static const profile_t *parse_p;
UNUSED static const profile_t *compile_p;
UNUSED static const profile_t *analyze_p;
UNUSED static const profile_t *setup_runtime_p;

const char *mrklkit_meta = "; libc\n"
"(sym malloc (func any int))\n"
"(sym realloc (func any any int))\n"
"(sym free (func int any))\n"
"(sym printf (func int any ...))\n"
"(sym strcmp (func int any any))\n"
"(sym time (func int any))\n"
"\n"
"; mrklkit backend\n"
"(sym mrklkit_rt_debug (func void any))\n"
"\n"
"; (parse )\n"
"(sym dparse_struct_item_ra_int                 (func int undef int))\n"
"(sym dparse_struct_item_ra_int_mpool           (func int undef int))\n"
"(sym dparse_struct_item_ra_float               (func float undef int))\n"
"(sym dparse_struct_item_ra_float_mpool         (func float undef int))\n"
"(sym dparse_struct_item_ra_bool                (func bool undef int))\n"
"(sym dparse_struct_item_ra_bool_mpool          (func bool undef int))\n"
"(sym dparse_struct_item_ra_str                 (func str undef int))\n"
"(sym dparse_struct_item_ra_str_mpool           (func str undef int))\n"
"(sym dparse_struct_item_ra_array               (func any undef int))\n"
"(sym dparse_struct_item_ra_array_mpool         (func any undef int))\n"
"(sym dparse_struct_item_ra_struct              (func any undef int))\n"
"(sym dparse_struct_item_ra_struct_mpool        (func any undef int))\n"
"(sym dparse_struct_item_ra_dict                (func any undef int))\n"
"(sym dparse_struct_item_ra_dict_mpool          (func any undef int))\n"
"\n"
"(sym rt_parser_info_data                       (func str any))\n"
"(sym rt_parser_info_data_mpool                 (func str any))\n"
"(sym rt_parser_info_pos                        (func str any))\n"
"\n"
";(sym dparse_dict_from_bytes                    (func any any str))\n"
"(sym dparse_dict_from_bytes_mpool              (func any any str))\n"
";(sym dparse_array_from_bytes                   (func any any str))\n"
"(sym dparse_array_from_bytes_mpool             (func any any str))\n"
"(sym mrklkit_rt_struct_get_item_int            (func int undef int int))\n"
"(sym mrklkit_rt_struct_get_item_float          (func float undef int float))\n"
"(sym mrklkit_rt_struct_get_item_bool           (func bool undef int bool))\n"
"(sym mrklkit_rt_struct_get_item_str            (func str undef int str))\n"
"(sym mrklkit_rt_struct_get_item_array          (func any undef int any))\n"
"(sym mrklkit_rt_struct_get_item_struct         (func any undef int any))\n"
"(sym mrklkit_rt_struct_get_item_dict           (func any undef int any))\n"
"\n"
"(sym mrklkit_rt_struct_set_item_int            (func void undef int int))\n"
"(sym mrklkit_rt_struct_set_item_int_mpool      (func void undef int int))\n"
"(sym mrklkit_rt_struct_set_item_float          (func void undef int float))\n"
"(sym mrklkit_rt_struct_set_item_float_mpool    (func void undef int float))\n"
"(sym mrklkit_rt_struct_set_item_bool           (func void undef int bool))\n"
"(sym mrklkit_rt_struct_set_item_bool_mpool     (func void undef int bool))\n"
"(sym mrklkit_rt_struct_set_item_str            (func void undef int str))\n"
"(sym mrklkit_rt_struct_set_item_str_mpool      (func void undef int str))\n"
"(sym mrklkit_rt_struct_set_item_array          (func void undef int any))\n"
"(sym mrklkit_rt_struct_set_item_array_mpool    (func void undef int any))\n"
"(sym mrklkit_rt_struct_set_item_dict           (func void undef int any))\n"
"(sym mrklkit_rt_struct_set_item_dict_mpool     (func void undef int any))\n"
"(sym mrklkit_rt_struct_set_item_struct         (func void undef int any))\n"
"(sym mrklkit_rt_struct_set_item_struct_mpool   (func void undef int any))\n"
"\n"
"(sym mrklkit_rt_struct_del_item_int            (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_float          (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_str            (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_str_mpool      (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_array          (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_array_mpool    (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_dict           (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_dict_mpool     (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_struct         (func void undef int))\n"
"(sym mrklkit_rt_struct_del_item_struct_mpool   (func void undef int))\n"
"\n"
"(sym mrklkit_rt_array_get_item_int             (func int (array int) int int))\n"
"(sym mrklkit_rt_array_get_item_float           (func float (array float) int float))\n"
"(sym mrklkit_rt_array_get_item_str             (func str (array str) int str))\n"
"(sym mrklkit_rt_array_len                      (func int any))\n"
"(sym mrklkit_rt_array_traverse                 (func void any any))\n"
"(sym mrklkit_rt_dict_get_item_int              (func int (dict int) str int))\n"
"(sym mrklkit_rt_dict_get_item_float            (func float (dict float) str float))\n"
"(sym mrklkit_rt_dict_get_item_str              (func str (dict str) str str))\n"
"(sym mrklkit_rt_dict_get_item_array            (func any (dict any) str any))\n"
"(sym mrklkit_rt_dict_get_item_struct           (func any (dict any) str any))\n"
"(sym mrklkit_rt_dict_get_item_dict             (func any (dict any) str any))\n"
"\n"
"(sym mrklkit_rt_dict_set_item_int              (func void undef str int))\n"
"(sym mrklkit_rt_dict_set_item_int_mpool        (func void undef str int))\n"
"(sym mrklkit_rt_dict_set_item_float            (func void undef str float))\n"
"(sym mrklkit_rt_dict_set_item_float_mpool      (func void undef str float))\n"
"(sym mrklkit_rt_dict_set_item_bool             (func void undef str bool))\n"
"(sym mrklkit_rt_dict_set_item_bool_mpool       (func void undef str bool))\n"
"(sym mrklkit_rt_dict_set_item_str              (func void undef str str))\n"
"(sym mrklkit_rt_dict_set_item_str_mpool        (func void undef str str))\n"
"(sym mrklkit_rt_dict_set_item_array            (func void undef str any))\n"
"(sym mrklkit_rt_dict_set_item_array_mpool      (func void undef str any))\n"
"(sym mrklkit_rt_dict_set_item_dict             (func void undef str any))\n"
"(sym mrklkit_rt_dict_set_item_dict_mpool       (func void undef str any))\n"
"(sym mrklkit_rt_dict_set_item_struct           (func void undef str any))\n"
"(sym mrklkit_rt_dict_set_item_struct_mpoo      (func void undef str any))\n"
"(sym mrklkit_rt_dict_has_item                  (func bool undef str))\n"
"(sym mrklkit_rt_dict_del_item                  (func bool undef str))\n"
"(sym mrklkit_rt_dict_del_item_mpool            (func bool undef str))\n"
"(sym mrklkit_rt_dict_traverse                  (func void any any))\n"
"\n"
"(sym bytes_new (func str int))\n"
"(sym bytes_new_from_bytes (func str str))\n"
"(sym bytes_copy (func int str str int))\n"
"(sym bytes_copyz (func int str str int))\n"
"(sym bytes_decref (func int any))\n"
";(sym bytes_decref_fast (func int str))\n"
";(sym bytes_incref (func int str))\n"
"(sym bytes_startswith (func bool str str))\n"
"(sym bytes_endswith (func bool str str))\n"
"(sym bytes_cmp (func int str str))\n"
"(sym bytes_cmpv (func int str str))\n"
"(sym bytes_cmpi (func int str str))\n"
"(sym bytes_contains (func int str str))\n"
"(sym bytes_containsi (func int str str))\n"
"\n"
"(sym mrklkit_rt_bytes_new (func str int))\n"
"(sym mrklkit_rt_bytes_new_mpool (func str int))\n"
"(sym mrklkit_rt_bytes_new_from_int (func str int))\n"
"(sym mrklkit_rt_bytes_new_from_int_mpool (func str int))\n"
"(sym mrklkit_rt_bytes_new_from_float (func str float))\n"
"(sym mrklkit_rt_bytes_new_from_float_mpool (func str float))\n"
"(sym mrklkit_rt_bytes_new_from_bool (func str bool))\n"
"(sym mrklkit_rt_bytes_new_from_bool_mpool (func str bool))\n"
"(sym mrklkit_rt_bytes_incref (func void str))\n"
"(sym mrklkit_rt_bytes_decref (func void any))\n"
"(sym mrklkit_rt_bytes_slice (func str str int int))\n"
"(sym mrklkit_rt_bytes_slice_mpool (func str str int int))\n"
"(sym mrklkit_rt_bytes_brushdown (func str str))\n"
"(sym mrklkit_rt_bytes_brushdown_mpool (func str str))\n"
"(sym mrklkit_rt_bytes_urldecode (func str str))\n"
"(sym mrklkit_rt_bytes_urldecode_mpool (func str str))\n"
"\n"
"(sym mrklkit_rt_array_split (func (array str) any str str))\n"
"(sym mrklkit_rt_array_split_mpool (func (array str) any str str))\n"
"(sym mrklkit_rt_array_incref (func void any))\n"
"(sym mrklkit_rt_array_decref (func void any))\n"
"(sym mrklkit_rt_array_new_mpool (func any any))\n"
"(sym mrklkit_rt_array_new (func any any))\n"
"\n"
"(sym mrklkit_rt_dict_new_mpool (func any any))\n"
"(sym mrklkit_rt_dict_new (func any any))\n"
"(sym mrklkit_rt_dict_incref (func void any))\n"
"(sym mrklkit_rt_dict_decref (func void any))\n"
"(sym mrklkit_rt_struct_new_mpool (func any any))\n"
"(sym mrklkit_rt_struct_new (func any any))\n"
"(sym mrklkit_rt_struct_incref (func void any))\n"
"(sym mrklkit_rt_struct_decref (func void any))\n"
"(sym mrklkit_strtoi64 (func int str int))\n"
"(sym mrklkit_strtoi64_loose (func int str))\n"
"(sym mrklkit_strtod (func float str float))\n"
"(sym mrklkit_strtod_loose (func float str))\n"
"(sym mrklkit_strptime (func int str str))\n"
"(sym mrklkit_timef (func float))\n"
"\n"
"; builtin\n"
"(builtin atoty (func undef ty str))\n"
"(builtin new (func undef ty))\n"
"(builtin copy (func undef undef))\n"
"(builtin , (func undef ...))\n"
"(builtin if (func undef bool undef undef))\n"
"(builtin then (func undef ...))\n"
"(builtin else (func undef ...))\n"
"(builtin do (func undef ...))\n"
"(builtin print (func undef undef ...))\n"
"\n"
"(builtin + (func undef undef ...))\n"
"(builtin - (func undef undef ...))\n"
"(builtin * (func undef undef ...))\n"
"(builtin / (func undef undef ...))\n"
"(builtin % (func undef undef ...))\n"
"(builtin min (func undef undef ...))\n"
"(builtin max (func undef undef ...))\n"
"\n"
"(builtin and (func bool bool ...))\n"
"(builtin or (func bool bool ...))\n"
"(builtin not (func bool bool))\n"
"\n"
"(builtin == (func bool undef undef))\n"
"(builtin != (func bool undef undef))\n"
"(builtin < (func bool undef undef))\n"
"(builtin <= (func bool undef undef))\n"
"(builtin > (func bool undef undef))\n"
"(builtin >= (func bool undef undef))\n"
"\n"
"(builtin bswap (func int int))\n"
"(sym llvm.bswap.i64 (func int int))\n"
"(builtin bcnt (func int int))\n"
"(builtin ctpop (func int int))\n"
"(sym llvm.ctpop.i64 (func int int))\n"
"(builtin ffs (func int int))\n"
"(builtin cttz (func int int))\n"
"(sym llvm.cttz.i64 (func int int bool))\n"
"(builtin fls (func int int))\n"
"(builtin ctlz (func int int))\n"
"(sym llvm.ctlz.i64 (func int int bool))\n"
"\n"
"(builtin parse (func undef undef undef))\n"
"(builtin get (func undef undef undef undef))\n"
"(builtin set (func void undef undef undef))\n"
"(builtin del (func void undef undef))\n"
"(builtin has (func bool undef undef))\n"
"(builtin len (func int undef))\n"
"(builtin traverse (func void undef undef))\n"
"\n"
"(builtin itof (func float int))\n"
"(builtin ftoi (func int float))\n"
"(builtin atoi (func int str int))\n"
"(builtin atoi-loose (func int str))\n"
"(builtin atof (func float str float))\n"
"(builtin atof-loose (func float str))\n"
"(builtin itob (func bool int))\n"
"(builtin btoi (func int bool))\n"
"(builtin tostr (func str undef))\n"
"(builtin in (func bool undef ...))\n"
"(builtin substr (func str str int int))\n"
"(builtin brushdown (func str str))\n"
"(builtin urldecode (func str str))\n"
"(builtin strfind (func int str str))\n"
"(builtin split (func (array str) str str))\n"
"(builtin startswith (func bool str str))\n"
"(builtin endswith (func bool str str))\n"
"(builtin dp-info (func undef undef str))\n"
"\n"
"(builtin nowi (func int))\n"
"(builtin nowf (func float))\n"
"(builtin isnull (func bool undef))\n"
"(builtin addrof (func any undef))\n"
"\n"
"(builtin __ir ir)\n"
"\n"
"(pragma user)\n"
"\n"
;

/**
 * Generic form parser
 *
 * types? vars?
 *
 */

int
mrklkit_parse(mrklkit_ctx_t *ctx, int fd, void *udata, fparser_datum_t **datum_root)
{
    int res = 0;
    array_t *form;
    array_iter_t it;
    fparser_datum_t **fnode;

    if ((*datum_root = fparser_parse(fd, NULL, NULL)) == NULL) {
        FAIL("fparser_parse");
    }

    if (FPARSER_DATUM_TAG(*datum_root) != FPARSER_SEQ) {
        (*datum_root)->error = 1;
        res = MRKLKIT_PARSE + 1;
        goto err;
    }

    form = (array_t *)(*datum_root)->body;
    for (fnode = array_first(form, &it);
         fnode != NULL;
         fnode = array_next(form, &it)) {


        /* here tag can be either SEQ, or one of INT, WORD, FLOAT */
        switch (FPARSER_DATUM_TAG(*fnode)) {
            array_t *nform;
            array_iter_t nit;
            char *first = NULL;

        case FPARSER_SEQ:
            nform = (array_t *)((*fnode)->body);

            if (lparse_first_word(nform, &nit, &first, 1) == 0) {
                /*
                 * control statements:
                 *  - mode
                 *  - pragma
                 */
                if (strcmp((char *)first, "mode") == 0) {
                    TRACE("mode ...");
                } else if (strcmp((char *)first, "pragma") == 0) {
                    char *arg = NULL;

                    if (lparse_next_word(nform, &nit, &arg, 1) == 0) {
                        //TRACE("pragma arg: %s", arg);
                    } else {
                        TRACE("ignoring empty pragma");
                    }

                } else {
                    mrklkit_module_t **mod;
                    array_iter_t it;

                    /*
                     * required:
                     *  - type
                     *  - builtin
                     *  - sym
                     */
                    for (mod = array_first(&ctx->modules, &it);
                         mod != NULL;
                         mod = array_next(&ctx->modules, &it)) {

                        if ((*mod)->parse_expr != NULL) {
                            if ((*mod)->parse_expr(udata,
                                                   (const char *)first,
                                                   nform,
                                                   &nit) != 0) {
                                (*fnode)->error = 1;
                                res = MRKLKIT_PARSE + 2;
                                goto err;
                            }
                        }
                    }
                }
            } else {
                /* ignore ? */
                TRACE("empty top-level sequence");
                res = MRKLKIT_PARSE + 3;
                goto err;
            }

            break;

        default:
            /* ignore ? */
            TRACE("unknown keyword");
            res = MRKLKIT_PARSE + 4;
            goto err;
        }
    }

end:
    TRRET(res);

err:
    fparser_datum_dump_formatted(*datum_root);
    goto end;
}


static void
do_analysis(LLVMModuleRef module)
{
    //LLVMModuleRef *paux;
    LLVMPassManagerBuilderRef pmb;
    LLVMPassManagerRef fpm;
    LLVMPassManagerRef pm;
    UNUSED int res = 0;
    LLVMValueRef fn = NULL;

    pmb = LLVMPassManagerBuilderCreate();
    LLVMPassManagerBuilderSetOptLevel(pmb, 3);

    /*
     * module
     */
    if ((pm = LLVMCreatePassManager()) == NULL) {
        FAIL("LLVMCreatePassManager");
    }
    LLVMPassManagerBuilderPopulateModulePassManager(pmb, pm);

    /*
     * function
     */
    if ((fpm = LLVMCreateFunctionPassManagerForModule(module)) == NULL) {
        FAIL("LLVMCreateFunctionPassManagerForModule");
    }
    LLVMPassManagerBuilderPopulateFunctionPassManager(pmb, fpm);

    if (LLVMInitializeFunctionPassManager(fpm)) {
        TRACE("LLVMInitializeFunctionPassManager ...");
    }

    for (fn = LLVMGetFirstFunction(module);
         fn != NULL;
         fn = LLVMGetNextFunction(fn)) {

        //TRACE("Passing %s", LLVMGetValueName(fn));
        //LLVMDumpValue(fn);
        if (LLVMRunFunctionPassManager(fpm, fn)) {
            //TRACE("%s was modified:", LLVMGetValueName(fn));
            //LLVMDumpValue(fn);
        }
    }

    res = LLVMRunPassManager(pm, module);

    //TRACE("res=%d", res);
    //if (res != 0) {
    //    TRACE("module was modified");
    //}

    LLVMPassManagerBuilderDispose(pmb);
    LLVMDisposePassManager(pm);
    LLVMDisposePassManager(fpm);
}


int
mrklkit_compile(mrklkit_ctx_t *ctx, int fd, uint64_t flags, void *udata)
{
    char *error_msg = NULL;
    int res = 0;
    LLVMTargetRef tr;
    LLVMTargetMachineRef tmr;
    LLVMMemoryBufferRef mb = NULL;
    mrklkit_module_t **mod;
    array_iter_t it;
    mrklkit_modaux_t *modaux;

    PROFILE_START(parse_p);
    /* parse */
    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->parse != NULL) {
            if ((*mod)->parse(ctx, fd, udata)) {
                PROFILE_STOP(parse_p);
                TRRET(MRKLKIT_COMPILE + 1);
            }
        }
    }

    /* post parse */
    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->post_parse != NULL) {
            if ((*mod)->post_parse(udata)) {
                PROFILE_STOP(parse_p);
                TRRET(MRKLKIT_COMPILE + 2);
            }
        }
    }
    PROFILE_STOP(parse_p);

    /* compile */

    PROFILE_START(compile_p);
    reset_newvar_counter();

    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->compile_type != NULL) {
            if ((*mod)->compile_type(udata, ctx->module)) {
                PROFILE_STOP(compile_p);
                TRRET(MRKLKIT_COMPILE + 3);
            }
        }
    }

    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->compile != NULL) {
            if ((*mod)->compile(udata, ctx->module)) {
                TRRET(MRKLKIT_COMPILE + 4);
            }
        }
    }
    PROFILE_STOP(compile_p);

    if ((flags & MRKLKIT_COMPILE_DUMP0) == MRKLKIT_COMPILE_DUMP0) {
        for (modaux = array_first(&ctx->modaux, &it);
             modaux != NULL;
             modaux = array_next(&ctx->modaux, &it)) {
            LLVMDumpModule(modaux->module);
        }
        LLVMDumpModule(ctx->module);
    }

    PROFILE_START(analyze_p);
    /* LLVM analysis */
    for (modaux = array_first(&ctx->modaux, &it);
         modaux != NULL;
         modaux = array_next(&ctx->modaux, &it)) {
        do_analysis(modaux->module);
    }
    do_analysis(ctx->module);
    PROFILE_STOP(analyze_p);

    if ((flags & MRKLKIT_COMPILE_DUMP1) == MRKLKIT_COMPILE_DUMP1) {
        for (modaux = array_first(&ctx->modaux, &it);
             modaux != NULL;
             modaux = array_next(&ctx->modaux, &it)) {
            TRACEC("-----------------------------------------------\n");
            LLVMDumpModule(modaux->module);
        }
        TRACEC("-----------------------------------------------\n");
        LLVMDumpModule(ctx->module);
    }

    if ((flags & MRKLKIT_COMPILE_DUMP2) == MRKLKIT_COMPILE_DUMP2) {
        TRACEC("-----------------------------------------------\n");
        tr = LLVMGetFirstTarget();
        TRACE("target name=%s descr=%s jit=%d tm=%d asm=%d triple=%s",
              LLVMGetTargetName(tr),
              LLVMGetTargetDescription(tr),
              LLVMTargetHasJIT(tr),
              LLVMTargetHasTargetMachine(tr),
              LLVMTargetHasAsmBackend(tr),
              LLVMGetTarget(ctx->module));

        TRACEC("-----------------------------------------------\n");
        tmr = LLVMCreateTargetMachine(tr,
                                      (char *)LLVMGetTarget(ctx->module),
                                      "x86-64",
                                      "",
                                      LLVMCodeGenLevelDefault,
                                      LLVMRelocDefault,
                                      LLVMCodeModelDefault);

        res = LLVMTargetMachineEmitToMemoryBuffer(tmr,
                                                  ctx->module,
                                                  LLVMAssemblyFile,
                                                  &error_msg,
                                                  &mb);
        if (res != 0) {
            TRACE("res=%d %s", res, error_msg);
            TRRET(MRKLKIT_COMPILE + 5);
        }
        TRACEC("%s", LLVMGetBufferStart(mb));
        LLVMDisposeMemoryBuffer(mb);
        LLVMDisposeTargetMachine(tmr);
    }

    return 0;
}


int
mrklkit_compile_incomplete(mrklkit_ctx_t *ctx,
                           int fd,
                           UNUSED uint64_t flags,
                           void *udata)
{
    mrklkit_module_t **mod;
    array_iter_t it;

    PROFILE_START(parse_p);
    /* parse */
    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->parse != NULL) {
            if ((*mod)->parse(ctx, fd, udata)) {
                PROFILE_STOP(parse_p);
                TRRET(MRKLKIT_COMPILE + 1);
            }
        }
    }

    /* post parse */
    for (mod = array_first(&ctx->modules, &it);
         mod != NULL;
         mod = array_next(&ctx->modules, &it)) {

        if ((*mod)->post_parse != NULL) {
            if ((*mod)->post_parse(udata)) {
                PROFILE_STOP(parse_p);
                TRRET(MRKLKIT_COMPILE + 2);
            }
        }
    }
    PROFILE_STOP(parse_p);

    return 0;
}


int
mrklkit_call_void(mrklkit_ctx_t *ctx, const char *name)
{
    int res;
    LLVMValueRef fn;
    LLVMGenericValueRef rv;

    res = LLVMFindFunction(ctx->ee, name, &fn);
    //TRACE("res=%d", res);
    if (res == 0) {
        rv = LLVMRunFunction(ctx->ee, fn, 0, NULL);
        //TRACE("rv=%p", rv);
        //TRACE("rv=%llu", LLVMGenericValueToInt(rv, 0));
        LLVMDisposeGenericValue(rv);
    }
    return res;
}


static int
modaux_init(mrklkit_modaux_t *modaux)
{
    modaux->lctx = NULL;
    modaux->buf = NULL;
    modaux->module = NULL;
    modaux->ee = NULL;
    return 0;
}


static int
modaux_fini(mrklkit_modaux_t *modaux)
{
    if (modaux->ee != NULL) {
        LLVMRunStaticDestructors(modaux->ee);
        LLVMDisposeExecutionEngine(modaux->ee);
        modaux->ee = NULL;
    }
    if (modaux->buf != NULL) {
        /*
         * don't dispose the buffer, since it is owned by module
         */
        //LLVMDisposeMemoryBuffer(modaux->buf);
        modaux->buf = NULL;
    }
    /*
     * don't dispose the module, since it is owned by ee
     */
    modaux->module = NULL;
    if (modaux->lctx != NULL) {
        LLVMContextDispose(modaux->lctx);
        modaux->lctx = NULL;
    }
    return 0;
}


static int
hash_item_backend_fini(UNUSED lkit_type_t *key, UNUSED mrklkit_backend_t *value)
{
    if (value != NULL) {
        free(value);
    }
    return 0;
}


void
mrklkit_ctx_init(mrklkit_ctx_t *ctx,
                 const char *name,
                 void *udata,
                 mrklkit_module_t *mod[],
                 size_t modsz)
{
    array_iter_t it;
    mrklkit_module_t **pm;
    size_t i;

    mrklkit_ctx_init_types(ctx);

    array_init(&ctx->modules, sizeof(mrklkit_module_t *), 0, NULL, NULL);
    if (mod != NULL) {
        for (i = 0; i < modsz; ++i) {
            pm = array_incr(&ctx->modules);
            *pm = mod[i];
        }
    }
    hash_init(&ctx->backends,
              101,
              (hash_hashfn_t)lkit_type_hash,
              (hash_item_comparator_t)lkit_type_cmp,
              (hash_item_finalizer_t)hash_item_backend_fini);
    ctx->lctx = LLVMContextCreate();
    if ((ctx->module = LLVMModuleCreateWithNameInContext(name,
            ctx->lctx)) == NULL) {
        FAIL("LLVMModuleCreateWithNameInContext");
    }
    LLVMSetTarget(ctx->module, "x86_64-unknown-unknown");

    array_init(&ctx->modaux,
               sizeof(mrklkit_modaux_t),
               0,
               (array_initializer_t)modaux_init,
               (array_finalizer_t)modaux_fini);

    for (pm = array_first(&ctx->modules, &it);
         pm != NULL;
         pm = array_next(&ctx->modules, &it)) {
        if ((*pm)->init != NULL) {
            (*pm)->init(udata);
        }
    }
    ctx->ee = NULL;
    ctx->mark_referenced = 0;
    ctx->dparse_mpolicy = MRKLKIT_DEFAULT_MPOLICY;
}


void
mrklkit_ctx_setup_runtime(mrklkit_ctx_t *ctx,
                          void *udata)
{
    array_iter_t it;
    mrklkit_module_t **mod;
    UNUSED struct LLVMMCJITCompilerOptions opts;
    mrklkit_modaux_t *modaux;
    char *error_msg = NULL;

    PROFILE_START(setup_runtime_p);

    LLVMLinkInMCJIT();
    LLVMInitializeMCJITCompilerOptions(&opts, sizeof(opts));
    opts.EnableFastISel = 1;
    opts.OptLevel = 3;

    for (modaux = array_first(&ctx->modaux, &it);
         modaux != NULL;
         modaux = array_next(&ctx->modaux, &it)) {
        if (LLVMLinkModules2(ctx->module,
                             modaux->module) != 0) {
            FAIL("LLVMLinkModules");
        }
    }
    if (LLVMCreateMCJITCompilerForModule(&ctx->ee,
                                         ctx->module,
                                         &opts,
                                         sizeof(opts),
                                         &error_msg) != 0) {
        TRACE("%s", error_msg);
        FAIL("LLVMCreateExecutionEngineForModule");
    }

    LLVMRunStaticConstructors(ctx->ee);

    for (mod = array_last(&ctx->modules, &it);
         mod != NULL;
         mod = array_prev(&ctx->modules, &it)) {
        if ((*mod)->link != NULL) {
            (*mod)->link(udata, ctx->ee, ctx->module);
        }
    }

    PROFILE_STOP(setup_runtime_p);
}


LLVMTypeRef
mrklkit_ctx_get_type_backend(mrklkit_ctx_t *mctx, lkit_type_t *ty)
{
    hash_item_t *dit;
    mrklkit_backend_t *backend;

    if ((dit = hash_get_item(&mctx->backends, ty)) == NULL) {
        FAIL("mrklkit_ctx_get_type_backend");
    }
    backend = dit->value;
    return backend->ty;
}


void
mrklkit_ctx_cleanup_runtime(mrklkit_ctx_t *ctx, void *udata)
{
    array_iter_t it;
    mrklkit_module_t **mod;

    for (mod = array_last(&ctx->modules, &it);
         mod != NULL;
         mod = array_prev(&ctx->modules, &it)) {
        if ((*mod)->unlink != NULL) {
            (*mod)->unlink(udata);
        }
        if ((*mod)->fini != NULL) {
            (*mod)->fini(udata);
        }
    }

    if (ctx->ee != NULL) {
        LLVMRunStaticDestructors(ctx->ee);
        LLVMDisposeExecutionEngine(ctx->ee);
        ctx->ee = NULL;
    }

    if (ctx->module != NULL) {
        //LLVMDisposeModule(ctx->module);
        ctx->module = NULL;
    }

    array_fini(&ctx->modaux);
    array_init(&ctx->modaux,
               sizeof(mrklkit_modaux_t),
               0,
               (array_initializer_t)modaux_init,
               (array_finalizer_t)modaux_fini);

    hash_cleanup(&ctx->backends);
}


void
mrklkit_ctx_cleanup_runtime_dirty(mrklkit_ctx_t *ctx, void *udata)
{
    array_iter_t it;
    mrklkit_module_t **mod;

    /*
     * no module unlink, fini only
     */
    for (mod = array_last(&ctx->modules, &it);
         mod != NULL;
         mod = array_prev(&ctx->modules, &it)) {
        if ((*mod)->fini != NULL) {
            (*mod)->fini(udata);
        }
    }

    if (ctx->ee != NULL) {
        LLVMRunStaticDestructors(ctx->ee);
        LLVMDisposeExecutionEngine(ctx->ee);
        ctx->ee = NULL;
    }

    if (ctx->module != NULL) {
        //LLVMDisposeModule(ctx->module);
        ctx->module = NULL;
    }

    array_fini(&ctx->modaux);
    array_init(&ctx->modaux,
               sizeof(mrklkit_modaux_t),
               0,
               (array_initializer_t)modaux_init,
               (array_finalizer_t)modaux_fini);

    hash_cleanup(&ctx->backends);
}


void
mrklkit_ctx_fini(mrklkit_ctx_t *ctx)
{
    array_fini(&ctx->modules);

    array_fini(&ctx->modaux);
    LLVMContextDispose(ctx->lctx);
    ctx->lctx = NULL;
    hash_fini(&ctx->backends);
    mrklkit_ctx_fini_types(ctx);
}



/*
 * Module
 *
 */

static void
llvm_init(void)
{
    LLVMPassRegistryRef pr;

    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeTarget();
    if ((pr = LLVMGetGlobalPassRegistry()) == NULL) {
        FAIL("LLVMGetGlobalRegistry");
    }
    LLVMInitializeCore(pr);

    LLVMInitializeTransformUtils(pr);
    LLVMInitializeScalarOpts(pr);
    LLVMInitializeObjCARCOpts(pr);

    LLVMInitializeVectorization(pr);

    LLVMInitializeInstCombine(pr);

    LLVMInitializeIPO(pr);
    LLVMInitializeInstrumentation(pr);

    LLVMInitializeAnalysis(pr);
    LLVMInitializeIPA(pr);
    LLVMInitializeCodeGen(pr);

    LLVMInitializeTarget(pr);

}

static void
llvm_fini(void)
{
    //LLVMContextDispose(LLVMGetGlobalContext());
}

void
mrklkit_init(void)
{
    profile_init_module();
    parse_p = PROFILE_REGISTER("parse");
    compile_p = PROFILE_REGISTER("compile");
    analyze_p = PROFILE_REGISTER("analyze");
    setup_runtime_p = PROFILE_REGISTER("setup_runtime");
    lexpr_init();
    llvm_init();
    lruntime_init();
}


void
mrklkit_fini(void)
{
    lruntime_fini();
    llvm_fini();
    lexpr_fini();
    profile_fini_module();
}

