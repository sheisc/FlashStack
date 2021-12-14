#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/user.h>
#include <sys/mman.h>
// gettid()
#include <sys/syscall.h>
#include <sys/types.h>
// pthread_getattr_np - get attributes of created thread
// _GNU_SOURCE has already been set in Makefile
#include <pthread.h>

#include "spa.h"



struct Buddy_TLS_Info *  buddy_get_tls_info(void){
    unsigned long rsp = SPA_GET_RSP();
    return (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(rsp) - 2 * BUDDY_CALL_STACK_SIZE);
}

/*
    (1)
    This function is NOT usable in a pthread.
    It should be only called in the main thread.


    7f5a5159b000-7f5a5359b000 rw-p 00000000 00:00 0                          // the call stack for the pthread
    7f5a5359b000-7f5a5359e000 r-xp 00000000 08:15 12717624                   /lib/x86_64-linux-gnu/libdl-2.27.so
    7f5a5359e000-7f5a5379d000 ---p 00003000 08:15 12717624                   /lib/x86_64-linux-gnu/libdl-2.27.so
    7f5a5379d000-7f5a5379e000 r--p 00002000 08:15 12717624                   /lib/x86_64-linux-gnu/libdl-2.27.so
    7f5a5379e000-7f5a5379f000 rw-p 00003000 08:15 12717624                   /lib/x86_64-linux-gnu/libdl-2.27.so
    7f5a5379f000-7f5a53986000 r-xp 00000000 08:15 12714523                   /lib/x86_64-linux-gnu/libc-2.27.so
    7f5a53986000-7f5a53b86000 ---p 001e7000 08:15 12714523                   /lib/x86_64-linux-gnu/libc-2.27.so
    7f5a53b86000-7f5a53b8a000 r--p 001e7000 08:15 12714523                   /lib/x86_64-linux-gnu/libc-2.27.so
    7f5a53b8a000-7f5a53b8c000 rw-p 001eb000 08:15 12714523                   /lib/x86_64-linux-gnu/libc-2.27.so
    7f5a53b8c000-7f5a53b90000 rw-p 00000000 00:00 0
    7f5a53b90000-7f5a53baa000 r-xp 00000000 08:15 12717639                   /lib/x86_64-linux-gnu/libpthread-2.27.so
    7f5a53baa000-7f5a53da9000 ---p 0001a000 08:15 12717639                   /lib/x86_64-linux-gnu/libpthread-2.27.so
    7f5a53da9000-7f5a53daa000 r--p 00019000 08:15 12717639                   /lib/x86_64-linux-gnu/libpthread-2.27.so
    7f5a53daa000-7f5a53dab000 rw-p 0001a000 08:15 12717639                   /lib/x86_64-linux-gnu/libpthread-2.27.so
    7f5a53dab000-7f5a53daf000 rw-p 00000000 00:00 0
    7f5a53daf000-7f5a53db1000 r-xp 00000000 08:15 17565355                   /home/iron/src/SPA/fork.so
    7f5a53db1000-7f5a53fb0000 ---p 00002000 08:15 17565355                   /home/iron/src/SPA/fork.so
    7f5a53fb0000-7f5a53fb1000 r--p 00001000 08:15 17565355                   /home/iron/src/SPA/fork.so
    7f5a53fb1000-7f5a53fb2000 rw-p 00002000 08:15 17565355                   /home/iron/src/SPA/fork.so
    7f5a53fb2000-7f5a53fd9000 r-xp 00000000 08:15 12714469                   /lib/x86_64-linux-gnu/ld-2.27.so
    7f5a541a9000-7f5a541ab000 rw-p 00000000 00:00 0
    7f5a541d7000-7f5a541d9000 rw-p 00000000 00:00 0
    7f5a541d9000-7f5a541da000 r--p 00027000 08:15 12714469                   /lib/x86_64-linux-gnu/ld-2.27.so
    7f5a541da000-7f5a541db000 rw-p 00028000 08:15 12714469                   /lib/x86_64-linux-gnu/ld-2.27.so
    7f5a541db000-7f5a541dc000 rw-p 00000000 00:00 0
    7ffc2e800000-7ffc302af000 rw-p 00000000 00:00 0                          [stack]
    7ffc302db000-7ffc302de000 r--p 00000000 00:00 0                          [vvar]
    7ffc302de000-7ffc302e0000 r-xp 00000000 00:00 0                          [vdso]
    ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]

    (2) There might be some bugs even in the main thread, when [stack] and [vvar] is adjacent. Shxt happens.

    7fff0231e000-7fff0231f000 r--p 00000000 00:00 0
    7fff0231f000-7fff02b20000 rw-p 00000000 00:00 0                          [stack]
    7fff02b20000-7fff02b23000 r--p 00000000 00:00 0                          [vvar]
    7fff02b23000-7fff02b25000 r-xp 00000000 00:00 0                          [vdso]
    ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]

 */
static __attribute__((__noinline__))  void buddy_get_stack_bound(
        void **low_b, void **high_b){
    //
    char *fptr0 = (char *) SPA_GET_RSP();
    char *fptr = fptr0 - ((uintptr_t)fptr0 % PAGE_SIZE);

    //SPA_DEBUG_OUTPUT(printf("rsp = %p ftpr = %p\n", fptr0, fptr));
    //
    *low_b = fptr;
    fptr += PAGE_SIZE;
    // mincore() will fail with ENOMEM for unmapped pages.  We can therefore
    // linearly scan to the base of the stack.
    // Note in practice this seems to be 1-3 pages at most if called from a
    // constructor.
    unsigned char vec;
    /*
        mincore() returns a vector that indicates
        whether pages of the calling process's virtual memory are resident in core (RAM)
     */
    while (mincore(fptr, PAGE_SIZE, &vec) == 0){
        fptr += PAGE_SIZE;
    }
    if (errno != ENOMEM){
        SPA_ERROR("failed to mincore page: %s", strerror(errno));
    }
    *high_b = fptr;
}


void buddy_print_tls_info(struct Buddy_TLS_Info *p_info){
//    SPA_DEBUG_OUTPUT(printf("SPA_TLS_Info at %p \n", p_info));
//    SPA_DEBUG_OUTPUT(printf("random_val = 0x%lx \n", p_info->random_val));
//    SPA_DEBUG_OUTPUT(printf("stack_top = %p \n", p_info->stack_top));
//    SPA_DEBUG_OUTPUT(printf("buddy_info = %p \n\n", p_info->buddy_info));
//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//    SPA_DEBUG_OUTPUT(printf("BUDDY_CALL_STACK_SIZE = 0x%lx \n\n", BUDDY_CALL_STACK_SIZE));
//#endif
}


void buddy_init_tls_metadata(void *stack_high){

    long random_val;

    srandom (time (0));
    random_val = random();

//    SPA_DEBUG_OUTPUT(printf("\n\n\n--------------------------------  pid = %ld,  buddy_init_tls_metadata() ----------------------------- \n",
//                            syscall(SYS_gettid)));
//    SPA_DEBUG_OUTPUT(printf("pid = %ld stack_high = %p\n", syscall(SYS_gettid), stack_high));
//    SPA_DEBUG_OUTPUT(printf("generated random_val = %lx \n", random_val));

    struct Buddy_TLS_Info *  ptr2 =
        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(stack_high) - 2 * BUDDY_CALL_STACK_SIZE);
    struct Buddy_TLS_Info *  ptr3 =
        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(stack_high) - 3 * BUDDY_CALL_STACK_SIZE);

    //void * tls_addr = (void *)(((char *)stack_high) - BUDDY_STACK_SIZE);

    // FIXME:
    //mprotect(tls_addr, BUDDY_THREAD_LOCAL_STORAGE_SIZE, PROT_WRITE | PROT_READ);

    //SPA_DEBUG_OUTPUT(printf("current random_val = %lx \n", ptr2->random_val));

    mprotect(ptr2, PAGE_SIZE, PROT_WRITE | PROT_READ);
    ptr2->random_val = random_val;
    ptr2->stack_top = stack_high;
    ptr2->buddy_info = ptr3;
    //ptr2->rand_cnt = 0;
    ptr2->cpu_cycles = 0;
    ptr2->is_randomizing = 0;
    buddy_print_tls_info(ptr2);
    //mprotect(ptr2, PAGE_SIZE, PROT_READ);

    mprotect(ptr3, PAGE_SIZE, PROT_WRITE | PROT_READ);
    //SPA_DEBUG_OUTPUT(printf("current random_val = %lx \n", ptr3->random_val));
    ptr3->random_val = random_val;
    ptr3->stack_top = stack_high;
    ptr3->buddy_info = ptr2;
    //ptr3->rand_cnt = 0;
    ptr3->cpu_cycles = 0;
    ptr3->is_randomizing = 0;
    buddy_print_tls_info(ptr3);
    //mprotect(ptr3, PAGE_SIZE, PROT_READ);

    //SPA_DEBUG_OUTPUT(printf("------------------------------------------------------------------------------------\n\n\n"));
}

/*
    Init metadata for a thread created by pthread_create().
 */
void buddy_init_other_thread_tls(void){
#if 0
    void *stack_low, *stack_high;
    pthread_attr_t attr;
    int ret;
    size_t stack_size;

    /* getting a new address */
    // FIXME:
    ret = pthread_getattr_np(pthread_self(), &attr);
    if(ret != 0){
        SPA_ERROR("pthread_getattr_np().");
    }
    ret = pthread_attr_getstack (&attr, &stack_low, &stack_size);
    if(ret != 0){
        SPA_ERROR("pthread_attr_getstack().");
    }
    //FIXME
    //stack_high = (char *)stack_low + stack_size;
    stack_high = (char *)stack_low + BUDDY_STACK_SIZE;

//    SPA_DEBUG_OUTPUT(printf("stack_low = %p, stack_high = %p, stack_size = %lx \n",
//                                stack_low, stack_high, stack_size));

    buddy_init_tls_metadata(stack_high);
#endif

    buddy_init_main_thread_tls();
}



static unsigned long tmp;
/*
    Init metadata for the main thread.
 */
void buddy_init_main_thread_tls(void){
    char *stack_low, *stack_high;
#if 0
    buddy_get_stack_bound((void **)&stack_low, (void **)&stack_high);

    //printf("stack_low = %p, stack_high = %p \n", stack_low, stack_high);
#endif
    pthread_attr_t attr;
    int ret;
    size_t stack_size;

    /* getting a new address */
    // FIXME:
    ret = pthread_getattr_np(pthread_self(), &attr);
    if(ret != 0){
        SPA_ERROR("pthread_getattr_np().");
    }
    ret = pthread_attr_getstack (&attr, (void **) (&stack_low), &stack_size);
    if(ret != 0){
        SPA_ERROR("pthread_attr_getstack().");
    }

    stack_high = (char *)stack_low + stack_size;

//    printf("pid = %d: buddy_stack_low = %p, buddy_stack_high = %p, buddy_stack_size = 0x%lx \n",
//           getpid(), stack_low, stack_high, stack_size);

    // Adjust stack_size
    stack_size = BUDDY_STACK_SIZE;
//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//    char *p_stk_size = getenv(SPA_CALL_STACK_SIZE_ENV);
//    if(p_stk_size){
//        // FIXME: more checks needed;  hardcoded
//        stack_size = (atol(p_stk_size) * 1024) * 4;
//    }
//#endif
    //FIXME
    if(stack_high < (char *)stack_low + stack_size){
        stack_high = (char *)stack_low + stack_size;
    }

//    printf("Adjusted: pid = %d: buddy_stack_low = %p, buddy_stack_high = %p, buddy_stack_size = 0x%lx \n\n",
//           getpid(), stack_high - stack_size, stack_high, stack_size);

#if 0
//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//    // check the stack is accessible.
//    unsigned long * low = (unsigned long *)(stack_high - BUDDY_STACK_SIZE);
//    unsigned long * ptr = (unsigned long *)SPA_GET_RSP();
//    // TBD: volatile
//    unsigned long sum = 0;
//    while(ptr > low){
//        unsigned x = ptr[0];
//        //ptr[0] = x;
//        sum += x;
//        // 1 page down
//        ptr -= PAGE_SIZE/sizeof(unsigned long);
//    }
//    tmp = sum;
//#endif
#endif

    // a guard page between call stack and shadow stack
    mprotect(stack_high - BUDDY_CALL_STACK_SIZE, PAGE_SIZE, PROT_READ);

    buddy_init_tls_metadata(stack_high);
}











