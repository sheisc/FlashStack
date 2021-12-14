#include <mmintrin.h>  // MMX
#include <xmmintrin.h> // SSE (include mmintrin.h)
#include <emmintrin.h> // SSE2 (include xmmintrin.h)
#include <pmmintrin.h> // SSE3 (include emmintrin.h)
#include <tmmintrin.h> // SSSE3 (include pmmintrin.h)
#include <smmintrin.h> // SSE4.1 (include tmmintrin.h)
#include <nmmintrin.h> // SSE4.2 (include smmintrin.h)
#include <wmmintrin.h> // AES (include nmmintrin.h)
#include <immintrin.h> // AVX (include wmmintrin.h)

#include <sys/user.h>
// mprotect
#include <sys/mman.h>
#include <stdio.h>
// gettid()
#include <unistd.h>
#include <sys/syscall.h>

#include <pthread.h>

#include "spa.h"

extern long  total_rand_cnt;

#define IS_BUDDY_STACK_INITED(tls_info) \
        ((tls_info) && ((tls_info)->buddy_info != NULL) && ((tls_info)->buddy_info->buddy_info == (tls_info)))

/*
    Add x to every long integers on the shadow stack [ss_ptr, ss_end).
    Can it be further improved by loop unrolling ?  Or Inline this function ?

    @ss_ptr         the start position, which will be 32-byte aligned in the function
    @ss_end         ss_end should have already been page-aligned, the high bound of the shadow stack
    @x              the random value

    @return         the microseconds we need
 */
__attribute__((always_inline))
static inline void avx2_64_x_4_add(unsigned long * ss_ptr, unsigned long * ss_end, long x){
    ss_ptr = (unsigned long *)(((unsigned long) ss_ptr) & (-32));
    //unsigned long from, to;
    __m256i A, B, C;
    long __attribute__ ((aligned (32))) xs[] = { x, x, x, x};
    B = _mm256_load_si256((__m256i *)xs);
    while(ss_ptr < ss_end){
        A = _mm256_load_si256((__m256i *) ss_ptr);
        C = _mm256_add_epi64(A, B);
        _mm256_store_si256((__m256i *) ss_ptr, C);
        // 4 long integers
        ss_ptr += 4;
    }
    //return 0;
}


static int is_instrumented_main_func_called(void){
    long flag = GET_FLAG_OF_MAIN_FUNC_CALLED();
    return SPA_MAIN_FUNC_CALLED_FLAG == flag;
}

/*
    Use optimized avx2_64_x_4_add() to do the runtime rerandomization
 */

void au_edu_unsw_randomize(struct Buddy_TLS_Info * tls_info, long cycles){

    struct Buddy_TLS_Info  * buddy_info = tls_info->buddy_info;

//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//    if(!is_instrumented_main_func_called()
//            || !IS_BUDDY_STACK_INITED(tls_info) || tls_info->is_randomizing || buddy_info->is_randomizing){
//        return;
//    }
//#else
//    if(!IS_BUDDY_STACK_INITED(tls_info) || tls_info->is_randomizing || buddy_info->is_randomizing){
//        return;
//    }
//#endif

    if(!IS_BUDDY_STACK_INITED(tls_info) || tls_info->is_randomizing || buddy_info->is_randomizing){
        return;
    }

    unsigned long call_stack_size = BUDDY_CALL_STACK_SIZE;

    unsigned long stack_top = (unsigned long) tls_info->stack_top;

    // When MPK is available, the cost of mprotect() can be greatly reduced.

    long curRandOffset = random();

    // start of randomization
    mprotect(tls_info, PAGE_SIZE, PROT_WRITE | PROT_READ);
    tls_info->is_randomizing = 1;
    tls_info->random_val += curRandOffset;
    tls_info->cpu_cycles = cycles;

    // buddy
    mprotect(buddy_info, PAGE_SIZE, PROT_WRITE | PROT_READ);
    buddy_info->is_randomizing = 1;
    buddy_info->random_val += curRandOffset;
    buddy_info->cpu_cycles = cycles;
    buddy_info->is_randomizing = 0;
    mprotect(buddy_info, PAGE_SIZE, PROT_READ);


    // shadow stack
    unsigned long  rsp = SPA_GET_RSP();

    avx2_64_x_4_add(
                    //(unsigned long * )(rbp + SPA_CPU_WORD_LENGTH - call_stack_size),
                    (unsigned long * )(rsp - call_stack_size),
                    (unsigned long * )(stack_top - call_stack_size),
                    curRandOffset
                   );

    // end of randomization
    tls_info->is_randomizing = 0;
    mprotect(tls_info, PAGE_SIZE, PROT_READ);


#if 0
    static long seq = 0;
    printf("seq = %ld %ld\n", seq, syscall(SYS_gettid));
    seq++;
#endif

#if defined(SAVE_TOTAL_RAND_CNT)
    //total_rand_cnt++;

    // lock addq $0x1,(%rax)
    // for httpd
    __sync_fetch_and_add(&total_rand_cnt, 1);
#endif
    // Assert for checking integrity
    // assert(...);
}
