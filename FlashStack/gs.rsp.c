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
#include <asm/prctl.h>
#include <sys/prctl.h>

#include <dlfcn.h>
#include <link.h>

#include <malloc.h>

#include <sys/stat.h>

#include <fcntl.h>
#include <x86intrin.h>
#include <stdarg.h>
#include <limits.h>

#include <gnu/lib-names.h>

// fast user-space locking
#include <linux/futex.h>
#include <sys/time.h>
#include <assert.h>


#include <mmintrin.h>  // MMX
#include <xmmintrin.h> // SSE (include mmintrin.h)
#include <emmintrin.h> // SSE2 (include xmmintrin.h)
#include <pmmintrin.h> // SSE3 (include emmintrin.h)
#include <tmmintrin.h> // SSSE3 (include pmmintrin.h)
#include <smmintrin.h> // SSE4.1 (include tmmintrin.h)
#include <nmmintrin.h> // SSE4.2 (include smmintrin.h)
#include <wmmintrin.h> // AES (include nmmintrin.h)
#include <immintrin.h> // AVX (include wmmintrin.h)

#include "spa.h"


#define  SPA_MAX_MMAP_ATTEMPTS   20


int arch_prctl(int code, ...);
int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr);






//#define  GET_GS_VALUE_AT_RSP() ({ \
//    unsigned long  p; \
//        __asm__ __volatile__ (  \
//        "movq   %%gs:(%%rsp), %0 \r\n"    \
//        :"=r"(p) \
//        : \
//        : \
//    );  \
//    p; \
//})


// (8 + 2) MB
// 10 MB
#define  REAL_SHADOW_STACK_SIZE   ((DEF_BUDDY_CALL_STACK_SIZE) + (2L << 20))
//
#define  METADATA_OFFSET_ON_SHADOW_STACK    ((DEF_BUDDY_CALL_STACK_SIZE) + (1L << 20) + (1L << 19))
//
#define  INIT_SHADOW_STACK_OFFSET           (DEF_BUDDY_CALL_STACK_SIZE)
//#define  REAL_META_DATA_SIZE      (PAGE_SIZE)

#define  MAX_THREAD_BUF_CNT         1024
#define  MONITOR_INTERVAL_IN_SECS       30
// 10 second
#define  CPU_CYCLES_AFTER_THREAD_EXITING        ((CPU_CYCLES_PER_RANDOMIZATION) * 1000L)

#define  MAX_GS_BASE_ADDR           (0x7FFFFFFFEFFFL)


struct ArgInfo{
    void *(*start_routine) (void *);
    void *arg;
};

struct ThreadRegionInfo{
    long valid;
    void *shadow_stack;
    long shadow_stack_size;
    long expired_time;
    long tid;
};

///////////////////////////////////////////////////////////////////////////////
__thread long gs_rsp_flash_stack_inited = 0;

/// Thread data for the cleanup handler
static pthread_key_t thread_cleanup_key;

// avoid race condition ?
//__thread int gs_is_randomizing = 0;

#if 0
__thread long time_for_gs_random = 0;
__thread long gs_cont_changing_count = 0;
__thread long gs_fast_moving_count = 0;
__thread long gs_avg_rand_time = 0;

#endif

static __thread int relaxing_sandbox_during_init = 0;

long gsrsp_total_rand_cnt = 0;
//long gsrsp_total_cleaning_cnt = 0;
long gsrsp_total_fail_rand_cnt = 0;
long gsrsp_total_cpu_cycles = 0;

long gsrsp_total_attempts[SPA_MAX_MMAP_ATTEMPTS+1];

static __thread long gsrsp_thread_rand_cnt = 0;
//
static pthread_t cleaner_tid;
//
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//
static struct ThreadRegionInfo thread_info[MAX_THREAD_BUF_CNT];

static long  gsrsp_total_crash_cnt;

struct gs_rsp_metadata{
    //struct gs_rsp_metadata *buddy;
    void *shadow_stack;     //
    long is_randomizing;    //
    long cpu_cycles;        //

    long state;             //
    long diff;
    long relaxing;          // relax the policy of sandbox

};
/////////////////////////////////////////////////////////////////////////////////
static int init_shadow_stack(void);

inline struct gs_rsp_metadata * get_gs_rsp_metadata_by_shadow_stack(long shadow_stack){

    struct gs_rsp_metadata * metadata =
            (struct gs_rsp_metadata *) (shadow_stack + METADATA_OFFSET_ON_SHADOW_STACK);
    return metadata;
}


inline struct gs_rsp_metadata * get_gs_rsp_metadata_by_rsp(){
    long rsp = SPA_GET_RSP();
    long gs_base = 0;
    int r = arch_prctl(ARCH_GET_GS, &gs_base);
    if(r < 0){
        return NULL;
    }
    long ssp = rsp + gs_base - SPA_USER_SPACE_SIZE;
    // get the shadow stack
    long shadow_stack = (ssp & DEF_BUDDY_CALL_STACK_SIZE_MASK);

    return get_gs_rsp_metadata_by_shadow_stack(shadow_stack);
}


void unsw_inc_asm_js_crash_cnt(void){
    //total_crash_cnt++;
    __sync_fetch_and_add(&gsrsp_total_crash_cnt, 1);
    fprintf(stderr, "total_crash_cnt = %ld\n", gsrsp_total_crash_cnt);
}

static int init_metadata_on_shadow_stack(long shadow_stack, long diff){
    struct gs_rsp_metadata * pMetadata = get_gs_rsp_metadata_by_shadow_stack(shadow_stack);

    memset(pMetadata, 0, sizeof(struct gs_rsp_metadata));

    pMetadata->shadow_stack = (void *) shadow_stack;
    pMetadata->is_randomizing = 0;
    pMetadata->cpu_cycles = 0;
    pMetadata->relaxing = 0;
    pMetadata->diff = diff;

    return 0;
}

static int add_memory_region(struct ThreadRegionInfo * region_info){
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < MAX_THREAD_BUF_CNT; i++){
        if(!thread_info[i].valid){
            thread_info[i] = *region_info;
            thread_info[i].valid = 1;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

static void * release_memory_region(void *arg){
    // FIXME:  compact the array after releasing entries

    while(1){
#if 0
        fprintf(stderr, "..................   Thread clearner: pid = %d, tid = %ld ...................... \n",
                               getpid(),  syscall(SYS_gettid));
#endif
        pthread_mutex_lock(&mutex);
        for(int i = 0; i < MAX_THREAD_BUF_CNT; i++){
            if(thread_info[i].valid &&
                    (__rdtsc() - thread_info[i].expired_time) > CPU_CYCLES_AFTER_THREAD_EXITING){
                // unmap the shadow stack
                munmap(thread_info[i].shadow_stack, thread_info[i].shadow_stack_size);
                thread_info[i].valid = 0;
                //__sync_fetch_and_add(&gsrsp_total_cleaning_cnt, 1);
#if 0
                fprintf(stderr, "Thread clearner: pid = %d, tid = %ld, shadow_stack = %p: %s, %d \n",
                            getpid(),
                            thread_info[i].tid,
                            thread_info[i].shadow_stack, __FILE__, __LINE__);
#endif
            }
        }
        pthread_mutex_unlock(&mutex);
        //
        sleep(MONITOR_INTERVAL_IN_SECS);
    }
}

//int spa_is_relaxing_sandbox(void){

//    return relaxing_sandbox_during_init;
//}

//void spa_set_relaxing_sandbox(int v){

//    relaxing_sandbox_during_init = v;
//}

static void *get_memory_at_random(unsigned long size, int init){
    void *addr =  MAP_FAILED;
    long i = 0;
    unsigned long rsp = SPA_GET_RSP();
    unsigned long adjusted_rsp = rsp - 2 * REAL_SHADOW_STACK_SIZE;
    while( MAP_FAILED == addr ){
      unsigned long x = SPA_GEN_RANDOM_VAL();     
      // below 0x7f0000000000
      x %= ((unsigned long) SPA_MAX_SHADOW_STACK_ADDR);

      // below rsp ?
      if(adjusted_rsp  > 0){
          x %= adjusted_rsp;
      }
      //
      x &= SPA_SHADOW_STACK_8MB_RAND_ADDR_MASK;
      if(i > SPA_MAX_MMAP_ATTEMPTS){ // avoid dead looping ?
        //x = 0;
          break;
      }
      addr = mmap( (void *) x, size,
                            PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

      if(addr != MAP_FAILED){
          unsigned long allocated = (unsigned long) addr;
          // If not 8MB-aligned or larger than adjusted_rsp
          if((allocated & SPA_8MB_MASK) || (allocated > adjusted_rsp)){
              munmap(addr, size);
              addr = MAP_FAILED;
              //continue;
          }
      }
      i++;
    }
    // give a warning ?
//    if(i > 5){
//        fprintf(stderr, "tid = %ld: get_memory_at_random() %s, %d , i = %ld \n",
//                   syscall(SYS_gettid),  __FILE__, __LINE__, i);
//    }
    if(i < SPA_MAX_MMAP_ATTEMPTS){
        if(!init){
            __sync_fetch_and_add(&gsrsp_total_attempts[i], 1);
        }
    }else{
        // FIXME:  check the size of mapped memory regions
         __sync_fetch_and_add(&gsrsp_total_attempts[SPA_MAX_MMAP_ATTEMPTS], 1);
    }
    return addr;
}

static inline int set_call_stack_info(struct gs_rsp_metadata * pMetadata){
    // Now pMetadata is ready
    pMetadata->state = FLASH_STACK_INITED;
    return 0;
}


static int do_init_shadow_stack(void){
    long rsp = (long) SPA_GET_RSP();
    if(rsp < 0x7F0000000000L){
//        fprintf(stderr, "pid = %d, tid = %ld, rsp = 0x%lx < 0x7F0000000000L:  %s, %d\n",
//                getpid(), syscall(SYS_gettid), rsp, __FILE__, __LINE__);
    }
    //assert(rsp > SPA_MAX_SHADOW_STACK_ADDR && "rsp > SPA_MAX_SHADOW_STACK_ADDR");
    long * shadow_stack = MAP_FAILED;

    while(shadow_stack == MAP_FAILED){ // Do it until it succeeds
        shadow_stack = (long *) get_memory_at_random(REAL_SHADOW_STACK_SIZE, 1);
//        if(shadow_stack == MAP_FAILED){
//            fprintf(stderr, "tid = %ld, get_memory_at_random():  %s, %d\n",
//                    syscall(SYS_gettid), __FILE__, __LINE__);
//        }
    }


    // malloc() might be called be pthread_getattr_np()

    long diff = ((long) shadow_stack) + INIT_SHADOW_STACK_OFFSET + SPA_USER_SPACE_SIZE - rsp;

    // init double metadata
    init_metadata_on_shadow_stack((long) shadow_stack, diff);

    //relaxing_sandbox_during_init = 1;
    arch_prctl(ARCH_SET_GS, diff);
    //relaxing_sandbox_during_init = 0;

    // now we can set the top of the call stack
    struct gs_rsp_metadata * metadata = get_gs_rsp_metadata_by_shadow_stack((long) shadow_stack);
    set_call_stack_info(metadata);

    return 0;
}


// we call it in customized malloc().
// so no call malloc() here to make sure the register gs is ready before real work of malloc().
static int init_shadow_stack(void){
    if(gs_rsp_flash_stack_inited){
        return 0;
    }
    // FIXME
    gs_rsp_flash_stack_inited = 1;
    do_init_shadow_stack();
    //gs_rsp_flash_stack_inited = 1;
    return 0;
}



static int release_shadow_stack(void){
    // FIXME
#if 1
    // release the shadow stack

    struct gs_rsp_metadata * pMetadata = get_gs_rsp_metadata_by_rsp();
    if(pMetadata == NULL){
        return -1;
    }
    void *shadow_stack = pMetadata->shadow_stack;

    //
    struct ThreadRegionInfo region_info;
    memset(&region_info, 0, sizeof(region_info));
    region_info.valid = 0;
    region_info.expired_time = __rdtsc();
    region_info.shadow_stack = shadow_stack;
    region_info.shadow_stack_size = REAL_SHADOW_STACK_SIZE;
    region_info.tid = syscall(SYS_gettid);

    add_memory_region(&region_info);

//    fprintf(stderr, "..................   release_shadow_stack(): pid = %d, tid = %ld ...................... \n",
//                               getpid(),  syscall(SYS_gettid));

#endif
    return 0;
}

static void * do_start_routine(void *arg){
    // now we are in the new thread context.
    init_shadow_stack();

    // copy to local variables, then release the heap object
    struct ArgInfo * pArg = (struct ArgInfo *)arg;
    struct ArgInfo argInfo = *pArg;

    free(pArg);

    pthread_setspecific(thread_cleanup_key, (void*) 1);

    void * retVal = (argInfo.start_routine)(argInfo.arg);

    return retVal;
}


/*
    Protected free() is also called when a thread is created.
    [Switching to Thread 0x7ffff57ff700 (LWP 18960)]
    0x00005555555662a4 in free ()
    (gdb) bt
    #0  0x00005555555662a4 in free ()
    #1  0x00007ffff6c195e2 in __libc_thread_freeres () at thread-freeres.c:29
    #2  0x00007ffff79b8700 in start_thread (arg=0x7ffff57ff700) at pthread_create.c:476
    #3  0x00007ffff6b9e71f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95
 */
typedef int (* PTHREAD_CREATE_FUNC)(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);
static PTHREAD_CREATE_FUNC _pthread_create;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg){
    size_t stacksize = 0;
    // FIXME: race condition
    if(!_pthread_create){
        _pthread_create = (PTHREAD_CREATE_FUNC) dlsym(RTLD_NEXT, "pthread_create");

        // create a thread for
        _pthread_create(&cleaner_tid, NULL, release_memory_region, NULL);
    }
    pthread_attr_t threadAttr;
    if(pthread_attr_init(&threadAttr) == -1){
        fprintf(stderr, "error in pthread_attr_init()\n");
    }

    stacksize = DEF_BUDDY_CALL_STACK_SIZE;

    if(!attr){
        attr = &threadAttr;
    }
    if(pthread_attr_setstacksize(( pthread_attr_t *)attr, stacksize) != 0){
        fprintf(stderr, "error in pthread_attr_setstacksize()\n");
    }

    struct ArgInfo * pArgInfo = (struct ArgInfo *) malloc(sizeof(struct ArgInfo));
    pArgInfo->start_routine= start_routine;
    pArgInfo->arg = arg;

    return _pthread_create(thread, attr, &do_start_routine, pArgInfo);
}


#if 0
typedef __attribute__((__noreturn__)) void (* PTHREAD_EXIT_FUNC)(void *retval);
static PTHREAD_EXIT_FUNC _pthread_exit;

void pthread_exit(void *retval){
    if(_pthread_exit == NULL){
        _pthread_exit = (PTHREAD_EXIT_FUNC)dlsym(RTLD_NEXT, "pthread_exit");
    }
    //release_shadow_stack();
    _pthread_exit(retval);

}
#endif

int init_main_shadow_stack(void){
    /*
        We should not call dlsym() here.
        (gdb) bt
        #0  0x00005555555661c4 in calloc ()
        #1  0x00007ffff77ae7f5 in _dlerror_run (operate=operate@entry=0x7ffff77ae0d0 <dlsym_doit>, args=args@entry=0x7fffffffd9d0) at dlerror.c:140
        #2  0x00007ffff77ae166 in __dlsym (handle=<optimised out>, name=0x7ffff7bd0e58 "pthread_create") at dlsym.c:70
        #3  0x00007ffff7bd0df5 in init_main_shadow_stack () at fsgs.c:234
        #4  0x0000555555566156 in malloc ()
        #5  0x0000000000011c00 in ?? ()
        #6  0x00007ffff74b3426 in ?? () from /usr/lib/x86_64-linux-gnu/libstdc++.so.6
        #7  0x00007ffff7de38d3 in call_init (env=0x7fffffffdab8, argv=0x7fffffffdaa8, argc=1, l=<optimised out>) at dl-init.c:72
        #8  _dl_init (main_map=0x7ffff7ffe170, argc=1, argv=0x7fffffffdaa8, env=0x7fffffffdab8) at dl-init.c:119
        #9  0x00007ffff7dd40ca in _dl_start_user () from /lib64/ld-linux-x86-64.so.2
        #10 0x0000000000000001 in ?? ()
        #11 0x00007fffffffde96 in ?? ()
        #12 0x0000000000000000 in ?? ()
     */
//    if(!_pthread_create){
//        _pthread_create = (PTHREAD_CREATE_FUNC) dlsym(RTLD_NEXT, "pthread_create");
//    }

//    if(_pthread_exit == NULL){
//        _pthread_exit = (PTHREAD_EXIT_FUNC)dlsym(RTLD_NEXT, "pthread_exit");
//    }

    init_shadow_stack();

    return 0;
}

/*
    iron@CSE:firefox79.0.asm$ c++filt _ZNK7mozilla17SandboxPolicyBase15EvaluateSyscallEi
    mozilla::SandboxPolicyBase::EvaluateSyscall(int) const
 */
long gs_rsp_allow_system_call_during_shuffling(void){
    if(relaxing_sandbox_during_init){
        return 1;
    }else{
        struct gs_rsp_metadata *pMetadata = get_gs_rsp_metadata_by_rsp();

        return pMetadata->is_randomizing;
    }
}



void gs_rsp_runtime_rerandomize(void){
    unsigned long from = __rdtsc();

    if(!gs_rsp_flash_stack_inited){
        return;
    }

    struct gs_rsp_metadata *pMetadata = get_gs_rsp_metadata_by_rsp();
    if(pMetadata == NULL){
        return;
    }

    if(pMetadata->state != FLASH_STACK_INITED){
        return;
    }

    if(pMetadata->is_randomizing){
        return;
    }

    pMetadata->is_randomizing = 1;

    unsigned long last_clocks = pMetadata->cpu_cycles;
    unsigned long cur_clocks = __rdtsc();
    if((cur_clocks - last_clocks) < CPU_CYCLES_PER_RANDOMIZATION){
        pMetadata->is_randomizing = 0;
        return;
    }

    pMetadata->cpu_cycles = cur_clocks;

    void * new_shadow_stack =
                (long *) get_memory_at_random(REAL_SHADOW_STACK_SIZE, 0);


    if(new_shadow_stack != MAP_FAILED){
#if 1
        long delta = ((long) new_shadow_stack) - ((long)pMetadata->shadow_stack);
        long diff = pMetadata->diff + delta;

        if(diff < 0 || diff > MAX_GS_BASE_ADDR){
            munmap(new_shadow_stack, REAL_SHADOW_STACK_SIZE);
            //return;
            goto rand_exit;
        }

        void * old_shadow_stack = pMetadata->shadow_stack;
        //
        /*
            struct gs_rsp_metadata{
                void *shadow_stack;     // Y
                long is_randomizing;    // ...
                long cpu_cycles;        // Y

                long state;             //
                long diff;              // Y
                long relaxing;          //
            };
         */
        pMetadata->shadow_stack = new_shadow_stack;

        pMetadata->diff = diff;



        // To be optimized, old size can be shrinked ?
        void *shadow_stack = mremap(old_shadow_stack,
                                     REAL_SHADOW_STACK_SIZE,
                                     REAL_SHADOW_STACK_SIZE,
                                     MREMAP_MAYMOVE | MREMAP_FIXED,
                                     new_shadow_stack);

        if(shadow_stack == MAP_FAILED){
            //fprintf(stderr, "\n ..........  tid = %ld:  mremap() failed ...... \n\n", syscall(SYS_gettid));
            __sync_fetch_and_add(&gsrsp_total_fail_rand_cnt, 1);
            pMetadata->shadow_stack = old_shadow_stack;
            pMetadata->diff -= delta;
            munmap(new_shadow_stack, REAL_SHADOW_STACK_SIZE);
            //return;
            goto rand_exit;
        }

        int r = arch_prctl(ARCH_SET_GS, diff);
        if(r < 0){
            //fprintf(stderr, "\n ..........  tid = %ld:  mremap() failed ...... \n\n", syscall(SYS_gettid));
            __sync_fetch_and_add(&gsrsp_total_fail_rand_cnt, 1);
            pMetadata->shadow_stack = old_shadow_stack;
            pMetadata->diff -= delta;
            munmap(new_shadow_stack, REAL_SHADOW_STACK_SIZE);
            //return;
            goto rand_exit;
        }

//        fprintf(stderr, "tid = %ld, old_shadow_stack = %p, new_stack_stack = %p, shadow_stack = %p\n",
//                syscall(SYS_gettid), old_shadow_stack, new_shadow_stack, shadow_stack);

        //pMetadata = (struct gs_rsp_metadata *) (((long) pMetadata) + delta);

        pMetadata = get_gs_rsp_metadata_by_shadow_stack((long) shadow_stack);

#endif
    }
    else{
        __sync_fetch_and_add(&gsrsp_total_fail_rand_cnt, 1);
    }
//    else{ // FIXME: it should not get here.
////        fprintf(stderr, "tid = %ld: while(1) %s, %d \n",
////                   syscall(SYS_gettid),  __FILE__, __LINE__);
////        while(1);
//        SPA_ERROR("get_memory_at_random().");
//    }
rand_exit:
    pMetadata->is_randomizing = 0;

    //spa_set_relaxing_sandbox(0);



#if defined(SAVE_TOTAL_RAND_CNT)
    //total_rand_cnt++;

    // lock addq $0x1,(%rax)
    // for httpd
    __sync_fetch_and_add(&gsrsp_total_rand_cnt, 1);
//    fsgs_thread_rand_cnt++;
//    fprintf(stderr, "###SPA_RAND###: tid = %ld,  pid = %d, thread_rand_cnt = %ld, total_rand_cnt = %ld, is randomizing .... \n",
//                        syscall(SYS_gettid), getpid(), fsgs_thread_rand_cnt, fsgs_total_rand_cnt);
#endif
    unsigned long to = __rdtsc();
    __sync_fetch_and_add(&gsrsp_total_cpu_cycles, (to - from));
}


/// Thread-specific data destructor
static void thread_cleanup_handler(void* _iter) {
  // We want to free the unsafe stack only after all other destructors
  // have already run. We force this function to be called multiple times.
  // User destructors that might run more then PTHREAD_DESTRUCTOR_ITERATIONS-1
  // times might still end up executing after the unsafe stack is deallocated,
  // so such descructors must have __attribute__((no_safe_stack)).
  size_t iter = (size_t) _iter;
  if (iter < PTHREAD_DESTRUCTOR_ITERATIONS) {
    pthread_setspecific(thread_cleanup_key, (void*) (iter + 1));
  } else {
    // This is the last iteration
    release_shadow_stack();
  }
}



//static int __attribute__((constructor(101))) do_gs_rsp_init_main_shadow_stack(void){
static int __attribute__((constructor(101))) do_init_main_shadow_stack(void){

//    fprintf(stderr, "tid = %ld, do_init_main_shadow_stack():  %s, %d\n",
//                syscall(SYS_gettid), __FILE__, __LINE__);

    pthread_key_create(&thread_cleanup_key, thread_cleanup_handler);

    init_main_shadow_stack();
    buddy_init_rt_lib_hooker();
    return 0;
}


