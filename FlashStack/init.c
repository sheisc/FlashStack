/*****************************************************************
            SPA runtime library
   Once we have instrumented the target program during building,
   it is time to hijact during runtime.

   LD_PRELOAD=/path/to/rt_lib.so   program

   We use au_edu_unsw_SPA_init() to initialize the thread-local storage
   needed for our runtime rerandomization.

******************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/user.h>
// mprotect
#include <sys/mman.h>
#include <dlfcn.h>
#include <pthread.h>
// gettid()
#include <sys/syscall.h>
// gettimeofday
#include <sys/time.h>
#include <time.h>
// backtrace()
#include <execinfo.h>
// _Unwind_Word
#include <unwind.h>
// epoll_wait
#include <sys/epoll.h>
// recvmsg
#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <signal.h>
#include <time.h>

#include <x86intrin.h>





#include "spa.h"






struct ArgInfo{
    void *(*start_routine) (void *);
    void *arg;
};


#if defined(SAVE_TOTAL_RAND_CNT)
// TDB: per-thread
// This value can be watched by using gdb as follows,
// where 7395 is the procee id of a nginx worker.
// iron@CSE:~$ sudo gdb -p 7395
// (gdb) p (long) nginx_rand_cnt
long  total_rand_cnt = 0;
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////


static void * do_start_routine(void *arg){
    // copy to local variables, then release the heap object
    struct ArgInfo * pArg = (struct ArgInfo *)arg;
    struct ArgInfo argInfo = *pArg;
    free(pArg);

    buddy_init_other_thread_tls();

    void * retVal = (argInfo.start_routine)(argInfo.arg);
    return retVal;
}




int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg){
    typedef int (* PTHREAD_CREATE_FUNC)(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start_routine) (void *), void *arg);
    size_t stacksize = 0;
    static PTHREAD_CREATE_FUNC _pthread_create;

    if(!_pthread_create){
        _pthread_create = (PTHREAD_CREATE_FUNC) dlsym(RTLD_NEXT, "pthread_create");
    }

    pthread_attr_t threadAttr;
    if(pthread_attr_init(&threadAttr) == -1){
        fprintf(stderr, "error in pthread_attr_init()\n");
    }

//    if(attr){
//        pthread_attr_getstacksize(attr, &stacksize);
//        // FIXME: ignore the user-specified stacksize now.
//    }

    stacksize = BUDDY_STACK_SIZE;


//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)
//    char *p_stk_size = getenv(SPA_CALL_STACK_SIZE_ENV);
//    if(p_stk_size){
//        // FIXME: more checks needed;  hardcoded
//        stacksize = (atol(p_stk_size) * 1024) * 4;
//    }
//#endif

    if(!attr){
        attr = &threadAttr;
    }
    if(pthread_attr_setstacksize(( pthread_attr_t *)attr, stacksize) != 0){
        fprintf(stderr, "error in pthread_attr_setstacksize()\n");
    }
    //pthread_attr_getstacksize(attr, &stacksize);
//    SPA_DEBUG_OUTPUT(printf("pthread_create() is hijacked. BUDDY_STACK_SIZE = 0x%lx, pid = %ld \n",
//                            stacksize, syscall(SYS_gettid)));


    struct ArgInfo * pArgInfo = (struct ArgInfo *) malloc(sizeof(struct ArgInfo));
    pArgInfo->start_routine= start_routine;
    pArgInfo->arg = arg;

    return _pthread_create(thread, attr, &do_start_routine, pArgInfo);
}







static void set_stk_size_global_var(){
    //printf("pid = %d, default call stack size = 0x%lx\n", getpid(), -GET_BUDDY_CALL_STACK_SIZE_MASK());
    char *p_stk_size = getenv(SPA_CALL_STACK_SIZE_ENV);
    if(p_stk_size){
        // FIXME: more checks needed        
        long stacksize = atol(p_stk_size);
        stacksize *= 1024;
        // reset the call stack size

        void *addr = GET_BUDDY_CALL_STACK_SIZE_MASK_ADDR();
        mprotect(addr, PAGE_SIZE, PROT_READ | PROT_WRITE);

        // reset the call stack size
        SET_BUDDY_CALL_STACK_SIZE_MASK(-stacksize);

        mprotect(addr, PAGE_SIZE, PROT_READ);
    }
    //printf("After reset, pid = %d, call stack size = 0x%lx\n\n", getpid(), -GET_BUDDY_CALL_STACK_SIZE_MASK());
}




static __attribute__((constructor(101))) void au_edu_unsw_SPA_init(){
    static int init = 0;
    if(init){
        return;
    }
    init = 1;

    //fprintf(stderr, "%s in %s, %d\n", __FUNCTION__,  __FILE__, __LINE__);

    buddy_init_rt_lib_hooker();

//#if defined(USE_SPA_BUDDY_STACK_TLS_WITH_STK_SIZE)

    set_stk_size_global_var();

//#endif



    buddy_init_main_thread_tls();
    setenv(SPA_ENABLE_RANDOMIZATION_ENV, "1", 1);

    srandom (time (0));

}


