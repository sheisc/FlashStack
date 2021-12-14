
#define AFL_MAIN

#include "config.h"
#include "types.h"
#include "debug.h"
#include "alloc-inl.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "spa.h"





//#define MAX_SPA_PATH_LEN    4096

//static u8*  as_path;                /* Path to the AFL 'as' wrapper      */
static u8** cc_params;              /* Parameters passed to the real CC  */
static u32  cc_par_cnt = 1;         /* Param count, including argv0      */
//static u8   be_quiet,               /* Quiet mode                        */
//clang_mode;             /* Invoked as afl-clang*?            */





/* Copy argv to cc_params, making the necessary edits. */

static void edit_params(u32 argc, char** argv) {
    //u8 *spa_path;
//    u8 *name;
//    int is_so = 0;
    cc_params = ck_alloc((argc + 128) * sizeof(u8*));

//    name = strrchr(argv[0], '/');
//    if (!name) name = argv[0]; else name++;

    u8* alt_cc = getenv("AFL_RUSTC");
    cc_params[0] = alt_cc ? alt_cc : (u8*)"/usr/bin/rustc";

    while (--argc) {
        u8* cur = *(++argv);
        cc_params[cc_par_cnt++] = cur;
    }

    cc_params[cc_par_cnt++] = "-C";
    cc_params[cc_par_cnt++] = "no-integrated-as";


#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
    cc_params[cc_par_cnt++] = "-L="DEFAULT_BUDDY_STACK_SIZE_LIB_PATH;
    cc_params[cc_par_cnt++] = "-lbustk";
    cc_params[cc_par_cnt++] = "-C";
    cc_params[cc_par_cnt++] = "link-args=-Wl,-rpath=" DEFAULT_BUDDY_STACK_SIZE_LIB_PATH;
#endif

#if 0
    // https://github.com/rust-lang/rust/pull/48786
    cc_params[cc_par_cnt++] = "-C";
    cc_params[cc_par_cnt++] = "force-frame-pointers=yes";
#endif

    cc_params[cc_par_cnt] = NULL;
}


/* Main entry point */

int main(int argc, char** argv) {
//    SAYF(cCYA "SPA-rustc " cBRI SPA_VERSION cRST "\n");

    if (argc < 2) {
        SAYF("\n"
             "It serves as a drop-in replacement for rustc, \n"
             "letting you recompile third-party code with the required\n"
             "runtime instrumentation.\n\n"
            );
        exit(1);

    }

    //find_rustc(argv[0]);

    edit_params(argc, argv);

    execvp(cc_params[0], (char**)cc_params);

    FATAL("Oops, failed to execute '%s' - check your PATH", cc_params[0]);

    return 0;

}
