#ifndef SPA_H
#define SPA_H

// (1) Instrumentation Option 1

// thread local storage for the random value + buddy stack
//#define    USE_SPA_BUDDY_STACK_TLS

// thread local storage for the random value + global stack size +  buddy stack
//#define    USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE
// When building firefox, it should be commented.
//#define    NON_PIE_GLOBAL_VAR_FOR_STK_SIZE

// maybe sacrifice compatibility

//#define    USE_SPA_FS_GS_TLS

// direct call / indirect call instrumented
//#define    ENABLE_GS_RSP_CALL_INSTRUMENTED

#define    USE_SPA_GS_RSP


//#define    USE_SHADESMAR_GS

// (2) Instrumentation Option 2
// traditional shadow stack
//#define  USE_SPA_SHADOW_STACK
//#define  USE_SPA_SHADOW_STACK_VIA_REG

// (3) Instrumentation Option 3
// global variable for the random value + shadow stack
//#define    USE_SPA_SHADOW_STACK_PLUS_GLOBAL_RANDVAR




//#define  ENABLE_SPA_RDSTC



#define    SAVE_TOTAL_RAND_CNT

#ifdef __cplusplus
extern "C" {
#endif




#include <time.h>
//#include "buddy_tls.h"

#define SPA_VERSION         "0.01"

#define SPA_CFI_STARTPROC                       "\t.cfi_startproc\n"
#define SPA_CLANG_RETQ                          "\tretq\n"
#define SPA_GCC_RET                             "\tret\n"
#define SPA_CFI_ENDPROC                         "\t.cfi_endproc\n"
// indirect call
#define SPA_CALLQ_STAR                          "\tcallq\t*"
// direct call
#define SPA_CALLQ_NO_STAR                       "\tcallq"

// 	.type	target_thread,@function
#define SPA_TYPE_PREFIX                         "\t.type\t"
#define SPA_AT_FUNCTION_POSTFIX                 ",@function"

// vdivps	%zmm10, %zmm0, %zmm0 {%k1} {z}
#define SPA_XMM_YMM_ZMM_WRITE_MASK_CLANG        "} {z}\n"
#define SPA_XMM_YMM_ZMM_WRITE_MAST_GCC          "}{z}\n"

#define SPA_DEBUG_OUTPUT(x)    do{} while(0)
//#define SPA_DEBUG_OUTPUT(x)    do{printf("%s %d: ", __FILE__, __LINE__);  x ; } while(0)

#define SPA_CXX_GLOBAL_SUB_I_PREFIX             "_GLOBAL__sub_I_"

//#define SPA_STACK_SIZE      BUDDY_CALL_STACK_SIZE


void __libc_csu_init(void);

#define	SPA_MAIN_STACK_END          ((unsigned long)__libc_csu_init)
#define	SPA_OTHER_STACK_END         0

// Once this environment varible, runtime rerandomization is enabled
#define SPA_ENABLE_RANDOMIZATION_ENV      "__SPA_ENABLE_RANDOMIZATION"

#define SPA_CALL_STACK_SIZE_ENV           "__SPA_CALL_STACK_SIZE"

#define SPA_PROTECTED_FUNCS_PATH_ENV      "__SPA_PROTECTED_FUNCS_PATH"

//#define SPA_MAIN_EXE_INITED_ENV           "__SPA_MAIN_EXE_INITED"

// \x43\x53\x45\x40\x55\x4e\x53\x57,  "CSE@UNSW"
#define SPA_GLOBAL_STACK_SIZE_MAGIC_NUM     (0x57534E5540455343L)
#define SPA_MAIN_FUNC_CALLED_FLAG           2021




//#define  SPA_ENABLE_GS_MSR

#if defined(USE_SPA_FS_GS_TLS)

    #if defined(SPA_ENABLE_GS_MSR)

        /*
            400a68:	4c 8b 1c 24          	mov    (%rsp),%r11
            400a6c:	65 4c 03 1c 25 10 00 	add    %gs:0x10,%r11
         */
        #define SPA_PROTECTED_FUNC_MAGIC_NUM        0x1c034c65241c8b4cL
        #define SPA_LENGTH_OF_PROTECTED_PROLOGUE    26

    #else
        /*
            Disable MSR
        0000000000400a50 <f>:
          400a50:	4c 8b 1c 24          	mov    (%rsp),%r11
          400a54:	65 4c 8b 14 25 08 00 	mov    %gs:0x8,%r10
          400a5b:	00 00
          400a5d:	4e 89 1c 14          	mov    %r11,(%rsp,%r10,1)

         */
        #define SPA_PROTECTED_FUNC_MAGIC_NUM        0x148b4c65241c8b4cL
        #define SPA_LENGTH_OF_PROTECTED_PROLOGUE    17
    #endif
#elif defined(USE_SPA_GS_RSP)
    /*
        0000000000400b30 <main>:
          400b30:	4c 8b 1c 24          	mov    (%rsp),%r11
          400b34:	49 ba 00 00 00 00 00 	movabs $0xffff800000000000,%r10
          400b3b:	80 ff ff
          400b3e:	65 4e 89 1c 14       	mov    %r11,%gs:(%rsp,%r10,1)
     */
    #define SPA_PROTECTED_FUNC_MAGIC_NUM        0x0000ba49241c8b4cL
    #define SPA_LENGTH_OF_PROTECTED_PROLOGUE    19
#endif

// Intel(R) Core(TM) i5-6500 CPU @ 3.20GHz
// randomization period in microseconds
#define SPA_RAND_PERIOD_IN_US           10000L
#define SPA_CPU_FREQUENCY               3200000000L
#define CPU_CYCLES_PER_US               (SPA_CPU_FREQUENCY / 1000000)
#define CPU_CYCLES_PER_RANDOMIZATION        (CPU_CYCLES_PER_US * SPA_RAND_PERIOD_IN_US)


#define SPA_RESERVED_REG        "xmm15"


#define SPA_RANDOM_VAL_NO_PIE          ".unsw.randomval"
#define SPA_RANDOM_VAL_PIE             ".unsw.randomval@GOTPCREL(%rip)"
#define SPA_RANDOM_VAL                 SPA_RANDOM_VAL_PIE


#define SPA_ERROR(format, ...)                                      \
    do {                                                            \
        fprintf(stderr, "error: " format "\n", ##__VA_ARGS__);      \
        abort();                                                    \
    } while (0)



#define	 SPA_GET_RBP() ({ \
    unsigned long  p; \
    __asm__ __volatile__ (	\
        "movq\t%%rbp, %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})


#define	 SPA_GET_RSP() ({ \
    unsigned long  p; \
    __asm__ __volatile__ (	\
        "movq\t%%rsp, %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})


#define  SPA_GEN_RANDOM_VAL() ({ \
    unsigned long  p; \
    __asm__ __volatile__ (  \
        "rdrand %0 \r\n"  \
        :"=r"(p) \
        : \
        : \
    );  \
    p; \
})

// FIXME: 4 on 32-bit, 8 on 64-bit
// It should be signed integer, as "-8" might be used in assembly code.
#define SPA_CPU_WORD_LENGTH           ((long)(sizeof(unsigned long)))


#define DYN_SYM_TABLE_FILE  "/dynamic_symbol_table.txt"
//#define BUDDY_STACK_SIZE_LIB   "/rt_lib.so"

// if it is definded in Makefile, then bla bla ...
#ifdef SPA_CUR_WORK_DIR
    #define DEFAULT_SPA_DYNAMIC_SYMBOL_TABLE_PATH  SPA_CUR_WORK_DIR  DYN_SYM_TABLE_FILE
    #define DEFAULT_BUDDY_STACK_SIZE_LIB_PATH  SPA_CUR_WORK_DIR
#else
    #define DEFAULT_SPA_DYNAMIC_SYMBOL_TABLE_PATH    "/home/iron/github/ParallelShadowStacks/FlashStack" DYN_SYM_TABLE_FILE
    #define DEFAULT_BUDDY_STACK_SIZE_LIB_PATH  "/home/iron/github/ParallelShadowStacks/FlashStack"
#endif

//////////////////////////////////////// ACCESS  RESERVED REGISTER ////////////////////////////////////////////



// multiple-thread, shared-object
// @tpoff
// @tlsgd
// @gottpoff
// @GOTPCREL
// @gotntpoff
// %%xmm15
#define	 GET_MT_BARRA_RANVAL() ({ \
    unsigned long  p; \
    __asm__ __volatile__ (	\
        "movq	%%" SPA_RESERVED_REG ", %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})

#define	 SET_MT_BARRA_RANVAL(val) ({ \
    unsigned long  p = (val); \
    __asm__ __volatile__ (	\
        "movq	%0, %%" SPA_RESERVED_REG " \r\n"	\
        : \
        :"r"(p)\
        :	\
    );	\
    p; \
})


#define	 GET_SPA_RANVAL() ({ \
    unsigned long  p; \
    __asm__ __volatile__ (	\
        "movq	%%" SPA_RESERVED_REG ", %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})



//////////////////////////////////////////// BUDDY STACK API //////////////////////////////////////////////////////
// See weak_stack_size.s,  the first 32 bytes are reserved
#define SPA_OFFSET_OF_MAIN_FUNC_CALLED_FLAG          32

//fprintf(outf, "\tmovq\t" ".BUDDY.CALL_STACK_SIZE_MASK@GOTPCREL(%%rip), %s\n", reg_x);
//fprintf(outf, "\tmovq\t" "$%ld, %ld(%s)\n", SPA_GLOBAL_STACK_SIZE_MAGIC_NUM,

#define GET_FLAG_OF_MAIN_FUNC_CALLED() ({ \
    long p; \
    __asm__ __volatile__ (	\
        "movq  .BUDDY.CALL_STACK_SIZE_MASK@GOTPCREL(%%rip), %0 \r\n"	\
        "movq  32(%0), %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})


#define	 GET_BUDDY_CALL_STACK_SIZE_MASK_ADDR() ({ \
    void *p; \
    __asm__ __volatile__ (	\
        "movq  .BUDDY.CALL_STACK_SIZE_MASK@GOTPCREL(%%rip), %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})


#define	 GET_BUDDY_CALL_STACK_SIZE_MASK() ({ \
    long  p; \
    __asm__ __volatile__ (	\
        "movq  .BUDDY.CALL_STACK_SIZE_MASK@GOTPCREL(%%rip), %0 \r\n"	\
        "movq  (%0), %0 \r\n"	\
        :"=r"(p) \
        :	\
        :	\
    );	\
    p; \
})


#define	 SET_BUDDY_CALL_STACK_SIZE_MASK(val) ({ \
    long  p = (val); \
    __asm__ __volatile__ (	\
        "movq  .BUDDY.CALL_STACK_SIZE_MASK@GOTPCREL(%%rip), %%rax \r\n"	\
        "movq  %%rbx, (%%rax) \r\n"	\
        : \
        :"b"(p)\
        :"rax"	\
    );	\
    p; \
})

// Macros starting with "DEF_*" are used as constants in afl-as.c
#define DEF_BUDDY_CALL_STACK_SIZE                   (8L << 20)
#define DEF_BUDDY_CALL_STACK_SIZE_MASK              (-DEF_BUDDY_CALL_STACK_SIZE)

#define DEF_BUDDY_SHADOW_STACK_SIZE                     DEF_BUDDY_CALL_STACK_SIZE
#define DEF_BUDDY_FUNCTION_LOCAL_STORAGE_SIZE           (DEF_BUDDY_CALL_STACK_SIZE + DEF_BUDDY_SHADOW_STACK_SIZE)
#define DEF_BUDDY_THREAD_LOCAL_STORAGE_SIZE             DEF_BUDDY_FUNCTION_LOCAL_STORAGE_SIZE
#define DEF_BUDDY_STACK_SIZE                        (4 * DEF_BUDDY_CALL_STACK_SIZE)

#define DEF_SPA_SS_OFFSET       DEF_BUDDY_CALL_STACK_SIZE

//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//#define BUDDY_CALL_STACK_SIZE                       (-GET_BUDDY_CALL_STACK_SIZE_MASK())
//#else
//#define BUDDY_CALL_STACK_SIZE                       DEF_BUDDY_CALL_STACK_SIZE
//#endif //

//  Macros without "DEF_*" are used as variables
#define BUDDY_CALL_STACK_SIZE                       (-GET_BUDDY_CALL_STACK_SIZE_MASK())
#define BUDDY_CALL_STACK_SIZE_MASK                  (~(unsigned long)(BUDDY_CALL_STACK_SIZE - 1))

#define BUDDY_SHADOW_STACK_SIZE                     BUDDY_CALL_STACK_SIZE
#define BUDDY_FUNCTION_LOCAL_STORAGE_SIZE           (BUDDY_CALL_STACK_SIZE + BUDDY_SHADOW_STACK_SIZE)
#define BUDDY_THREAD_LOCAL_STORAGE_SIZE             BUDDY_FUNCTION_LOCAL_STORAGE_SIZE
#define BUDDY_STACK_SIZE                            (BUDDY_FUNCTION_LOCAL_STORAGE_SIZE + BUDDY_THREAD_LOCAL_STORAGE_SIZE)



#define BUDDY_ALIGN_CALL_STACK_POINTER(x)           (BUDDY_CALL_STACK_SIZE_MASK & (unsigned long)(x))



// ATI
#define BUDDY_VIR_REG_0         0
#define BUDDY_VIR_REG_1         8
#define BUDDY_VIR_REG_2         16
#define BUDDY_VIR_REG_3         24
// ...


#define BUDDY_VIR_REG_RANDOM_VAL        BUDDY_VIR_REG_0
#define BUDDY_VIR_REG_STACK_TOP         BUDDY_VIR_REG_1
#define BUDDY_VIR_REG_BUDDY_INFO        BUDDY_VIR_REG_2
//#define BUDDY_VIR_REG_STACK_SIZE        BUDDY_VIR_REG_3




struct Buddy_TLS_Info{
    long random_val;            // random value for runtime rerandomization
    void *stack_top;			// page aligned
    struct Buddy_TLS_Info *buddy_info;	// pointer to our buddy
    long is_randomizing;
    //long rand_cnt;
    long cpu_cycles;
    //unsigned long stack_size;   // stack size, aligned to power(2,n)
    //void *data;                 //
};


#define  GET_GS_VALUE_AT_OFFSET(offset) ({ \
    unsigned long  p; \
    unsigned long x = (offset); \
    __asm__ __volatile__ (  \
        "movq   %%gs:(%%rax), %0 \r\n"    \
        :"=r"(p) \
        :"a"(x) \
        :   \
    );  \
    p; \
})

#define FLASH_STACK_INITED  1

#define DEFAULT_CHANGING_MOVING_RATIO   1

struct gs_metadata{
    //
    void *self_addr;        //
    long diff;              //
    long R;                 // random value
    long is_randomizing;    //
    //
    void *shadow_stack;     //
    long cpu_cycles;        //
    long state;             //
    void *shadow_stack_top; // shadow_stack + CALL_STACK_SIZE
    void *call_stack_top;   // the top of call stack
    long ratio_factor;      // freq of changing / freq of moving
    long saved_diff;        //
    long relaxing;          // relax the policy of sandbox
};

// field can be diff, shadow_stack, ... in struct gs_metadata
#define  GET_GS_METADATA_FIELD_OFFSET(field)  ((long) (&(((struct gs_metadata *) 0)->field)))

//
struct Buddy_TLS_Info *  buddy_get_tls_info(void);
//
void buddy_print_tls_info(struct Buddy_TLS_Info *p_info);
//
void buddy_init_main_thread_tls(void);
//
void buddy_init_other_thread_tls(void);
//
void buddy_init_rt_lib_hooker(void);

//////////////////////////////////////////////////////////////////////////////////////

//
unsigned long spa_get_cur_time_us(void);

//
//unsigned long avx2_64_x_4_add(unsigned long * ss_ptr, unsigned long * ss_end, long x);
//
//unsigned long do_au_edu_unsw_randomize(
//                                 struct Buddy_TLS_Info * tls_info,
//                                 struct Buddy_TLS_Info * buddy_info,
//                                 unsigned long call_stack_size,
//                                 long cnt,
//                                 long cycles);

//void au_edu_unsw_randomize(const char *func_name, unsigned long rsp, long cycles);

//void avx2_64_x_4_add(unsigned long * ss_ptr, unsigned long * ss_end, long x);

void au_edu_unsw_randomize(struct Buddy_TLS_Info * tls_info, long cycles);

void fsgs_runtime_rerandomize(void);

void gs_rsp_runtime_rerandomize(void);

int spa_is_relaxing_sandbox(void);



#define  SPA_USER_SPACE_SIZE      0x800000000000L
// leave some space (8 * DEF_BUDDY_CALL_STACK_SIZE) for shadow stack
#define  SPA_MAX_SHADOW_STACK_ADDR  (0x7F0000000000L - 8 * DEF_BUDDY_CALL_STACK_SIZE)

#define  SPA_SHADOW_STACK_8MB_RAND_ADDR_MASK    (0x7FFFFF800000L)

#define  SPA_8MB_MASK               0x7FFFFFL

#ifdef __cplusplus
}
#endif

#endif // SPA_H
