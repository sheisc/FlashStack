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


#define  SPA_MAX_MMAP_ATTEMPTS   5


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


// (8 + 1) MB
#define  REAL_SHADOW_STACK_SIZE   ((DEF_BUDDY_CALL_STACK_SIZE) + (1L << 20))
#define  REAL_META_DATA_SIZE      (PAGE_SIZE)

#define  MAX_THREAD_BUF_CNT         1024
#define  MONITOR_INTERVAL_IN_SECS       30
// 10 second
#define  CPU_CYCLES_AFTER_THREAD_EXITING        ((CPU_CYCLES_PER_RANDOMIZATION) * 1000L * 10)

struct ArgInfo{
    void *(*start_routine) (void *);
    void *arg;
};

struct ThreadRegionInfo{
    long valid;
    void *metadata;
    long metadata_size;
    void *shadow_stack;
    long shadow_stack_size;
    long expired_time;
    long tid;
};

///////////////////////////////////////////////////////////////////////////////
__thread long unsw_flash_stack_inited = 0;

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

static __thread int relaxing_sandbox = 0;

long fsgs_total_rand_cnt = 0;
__thread long fsgs_thread_rand_cnt = 0;
//
static pthread_t cleaner_tid;
//
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//
static struct ThreadRegionInfo thread_info[MAX_THREAD_BUF_CNT];

static long  total_crash_cnt;
/////////////////////////////////////////////////////////////////////////////////
static int init_shadow_stack(void);

void unsw_inc_asm_js_crash_cnt(void){
    //total_crash_cnt++;
    __sync_fetch_and_add(&total_crash_cnt, 1);

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
//        fprintf(stderr, "..................   Thread clearner: pid = %d, tid = %ld ...................... \n",
//                       getpid(),  syscall(SYS_gettid));
        pthread_mutex_lock(&mutex);
        for(int i = 0; i < MAX_THREAD_BUF_CNT; i++){
            if(thread_info[i].valid &&
                    (__rdtsc() - thread_info[i].expired_time) > CPU_CYCLES_AFTER_THREAD_EXITING){
                // unmap the shadow stack first
                munmap(thread_info[i].shadow_stack, thread_info[i].shadow_stack_size);
                // then the metadata
                munmap(thread_info[i].metadata, thread_info[i].metadata_size);
                thread_info[i].valid = 0;
#if 0
                fprintf(stderr, "Thread clearner: pid = %d, tid = %ld, metadata = %p, shadow_stack = %p: %s, %d \n",
                            getpid(),
                            thread_info[i].tid,
                            thread_info[i].metadata,
                            thread_info[i].shadow_stack, __FILE__, __LINE__);
#endif
            }
        }
        pthread_mutex_unlock(&mutex);
        //
        sleep(MONITOR_INTERVAL_IN_SECS);
    }
}

int spa_is_relaxing_sandbox(void){
//    struct gs_metadata *metadata = (struct gs_metadata *) GET_GS_VALUE_AT_OFFSET(0);
//    return metadata->relaxing;
    return relaxing_sandbox;
}

void spa_set_relaxing_sandbox(int v){
//    struct gs_metadata *metadata = (struct gs_metadata *) GET_GS_VALUE_AT_OFFSET(0);
//    metadata->relaxing = v;
    relaxing_sandbox = v;
}

void *get_memory_at_random(unsigned long size){
    void *addr =  MAP_FAILED;
    long i = 0;
    while( MAP_FAILED == addr ){
      unsigned long x = SPA_GEN_RANDOM_VAL();



      x &= 0x7FFFFFFFF000;
      if(i > SPA_MAX_MMAP_ATTEMPTS){ // avoid dead looping ?
        x = 0;
      }
      addr = mmap( (void *) x, size,
                            PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      i++;
    }
    return addr;
}

static int set_call_stack_info(struct gs_metadata * pMetadata){
    pthread_attr_t attr;
    int ret;
    size_t stack_size;
    char *stack_low, *stack_high;

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

    // Adjust stack_size
    stack_size = DEF_BUDDY_CALL_STACK_SIZE;

    //FIXME
    if(stack_high < (char *)stack_low + stack_size){
        stack_high = (char *)stack_low + stack_size;
    }

    pMetadata->call_stack_top = stack_high;
    pMetadata->shadow_stack_top = (char *)(pMetadata->shadow_stack) + DEF_BUDDY_CALL_STACK_SIZE;
    // Now pMetadata is ready
    pMetadata->state = FLASH_STACK_INITED;

//    fprintf(stderr, "tid = %ld, call_stack_top = %p, shadow_stack = %p, shadow_stack_top = %p: %s, %d\n",
//           syscall(SYS_gettid), pMetadata->call_stack_top, pMetadata->shadow_stack,
//           pMetadata->shadow_stack_top, __FILE__, __LINE__);
    return 0;
}


static int do_init_shadow_stack(void){
    struct gs_metadata * pMetadata =
            (struct gs_metadata *) get_memory_at_random(REAL_META_DATA_SIZE);

    if(pMetadata == MAP_FAILED){
        SPA_ERROR("mmap().");
    }

    long * shadow_stack =
            (long *) get_memory_at_random(REAL_SHADOW_STACK_SIZE);

    if(shadow_stack == MAP_FAILED){
        SPA_ERROR("mmap().");
    }
    // malloc() might be called be pthread_getattr_np()
#if 0
    char *stack_low;

    pthread_attr_t attr;
    int ret;
    size_t stack_size;

    ret = pthread_getattr_np(pthread_self(), &attr);
    if(ret != 0){
        SPA_ERROR("pthread_getattr_np().");
    }
    ret = pthread_attr_getstack (&attr, (void **) (&stack_low), &stack_size);
    if(ret != 0){
        SPA_ERROR("pthread_attr_getstack().");
    }
    long diff = ((long) shadow_stack) - ((long) stack_low);
#endif

    // int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

    long diff = ((long) shadow_stack) + DEF_BUDDY_CALL_STACK_SIZE - ((long) SPA_GET_RSP());

#if 0
    fprintf(stderr, "tid = %ld, gs_page = %p, shadow_stack = %p %s, %d\n",
                syscall(SYS_gettid), pMetadata, shadow_stack,  __FILE__, __LINE__);
#endif
    memset(pMetadata, 0, sizeof(*pMetadata));
    // %gs:(0)
    pMetadata->self_addr = pMetadata;
    // %gs:(8)
    pMetadata->diff = diff;
    // %gs:(16)
    pMetadata->shadow_stack = shadow_stack;

    pMetadata->cpu_cycles = 0;
    pMetadata->is_randomizing = 0;

    pMetadata->ratio_factor = DEFAULT_CHANGING_MOVING_RATIO;
//    // %gs:(24)  tid
//    gs_page[3] = syscall(SYS_gettid);

    pMetadata->R = SPA_GEN_RANDOM_VAL();

    long x = 0;

    //spa_set_relaxing_sandbox(1);
    arch_prctl(ARCH_SET_GS, pMetadata);
    //spa_set_relaxing_sandbox(0);

    // now we can set the top of the call stack
    set_call_stack_info(pMetadata);



#if 0
    arch_prctl(ARCH_GET_GS, &x);
    fprintf(stderr, "tid = %ld, ARCH_GET_GS = 0x%lx: %s, %d\n",
           syscall(SYS_gettid), x, __FILE__, __LINE__);


//    arch_prctl(ARCH_GET_FS, &x);
//    fprintf(stderr, "tid = %ld, ARCH_GET_FS = 0x%lx: %s, %d\n",
//           syscall(SYS_gettid), x, __FILE__, __LINE__);
#endif
    return 0;
}


// we call it in customized malloc().
// so no call malloc() here to make sure the register gs is ready before real work of malloc().
static int init_shadow_stack(void){
    if(unsw_flash_stack_inited){
        return 0;
    }
    unsw_flash_stack_inited = 1;
    do_init_shadow_stack();
    return 0;
}



static int release_shadow_stack(void){

    // release the shadow stack
    // At the stage, we can use gs:(0) and gs:(8) respectively.

    struct gs_metadata * pMetadata = (struct gs_metadata *) (GET_GS_VALUE_AT_OFFSET(0));
    void *shadow_stack = pMetadata->shadow_stack;

#if 0
    fprintf(stderr, "release_shadow_stack(): tid = %ld, gs_page = %p, shadow_stack = %p: %s, %d \n",
                syscall(SYS_gettid), pMetadata, shadow_stack, __FILE__, __LINE__);
#endif

#if 0
    // unmap the shadow stack first
    munmap(shadow_stack, REAL_SHADOW_STACK_SIZE);
    // then the metadata
    munmap(pMetadata, REAL_META_DATA_SIZE);

#endif
    //
    struct ThreadRegionInfo region_info;
    memset(&region_info, 0, sizeof(region_info));
    region_info.expired_time = __rdtsc();
    region_info.metadata = pMetadata;
    region_info.metadata_size = REAL_META_DATA_SIZE;
    region_info.shadow_stack = shadow_stack;
    region_info.shadow_stack_size = REAL_SHADOW_STACK_SIZE;
    region_info.tid = syscall(SYS_gettid);
    add_memory_region(&region_info);

    return 0;
}

static void * do_start_routine(void *arg){
    // now we are in the new thread context.
    init_shadow_stack();
    // call it without checking
    //do_init_shadow_stack();
#if 0
    fprintf(stderr, "tid = %ld, %s, %s, %d\n",
                syscall(SYS_gettid), __FUNCTION__, __FILE__, __LINE__);
#endif
    // copy to local variables, then release the heap object
    struct ArgInfo * pArg = (struct ArgInfo *)arg;
    struct ArgInfo argInfo = *pArg;
    // for firefox ?
#if 1
    free(pArg);
#endif

//    fprintf(stderr, "tid = %ld: %s, %d \n",
//           syscall(SYS_gettid), __FILE__, __LINE__);
    pthread_setspecific(thread_cleanup_key, (void*) 1);

    void * retVal = (argInfo.start_routine)(argInfo.arg);
    //release_shadow_stack();

    return retVal;
}


#if 1
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
#if 1
    return _pthread_create(thread, attr, &do_start_routine, pArgInfo);
#endif
    //return _pthread_create(thread, attr, start_routine, arg);
}
#endif


typedef __attribute__((__noreturn__)) void (* PTHREAD_EXIT_FUNC)(void *retval);
static PTHREAD_EXIT_FUNC _pthread_exit;

void pthread_exit(void *retval){
    if(_pthread_exit == NULL){
        _pthread_exit = (PTHREAD_EXIT_FUNC)dlsym(RTLD_NEXT, "pthread_exit");
    }
    //release_shadow_stack();
    _pthread_exit(retval);

}

#if 0
// Note: There is no glibc wrapper for this system call; see NOTES.
// So this hook is useless?
typedef int (*FUTEX_FUNC)(int *uaddr, int futex_op, int val,
                  const struct timespec *timeout,   /* or: uint32_t val2 */
                  int *uaddr2, int val3);
static FUTEX_FUNC _futex;
int futex(int *uaddr, int futex_op, int val,
          const struct timespec *timeout,   /* or: uint32_t val2 */
          int *uaddr2, int val3){
    if(_futex == NULL){
        _futex = (FUTEX_FUNC) dlsym(RTLD_NEXT, "futex");
    }
    fprintf(stderr, "............... tid = %ld, futex(): %s, %d ................... \n",
           syscall(SYS_gettid),  __FILE__, __LINE__);
    return _futex(uaddr, futex_op, val, timeout, uaddr2, val3);
}
#endif

int init_main_shadow_stack(void){
#if 0
    long x = 0;
    arch_prctl(ARCH_GET_GS, &x);

    fprintf(stderr, "tid = %ld, ARCH_GET_GS = 0x%lx: %s, %d, stack_inited = %d\n",
           syscall(SYS_gettid), x, __FILE__, __LINE__, stack_inited);
#endif
/*
    static int fsgs_init;
    if(fsgs_init){
        return 0;
    }
    fsgs_init = 1;
 */


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

#if 0
    arch_prctl(ARCH_GET_GS, &x);
    fprintf(stderr, "tid = %ld, ARCH_GET_GS = 0x%lx: %s, %d, stack_inited = %d\n",
           syscall(SYS_gettid), x, __FILE__, __LINE__, stack_inited);
#endif
    //int x;
    //scanf("%d", &x);
    return 0;
}


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


void fsgs_runtime_rerandomize(void){
    if(!unsw_flash_stack_inited){
        return;
    }
    struct gs_metadata *old_metadata = (struct gs_metadata *) GET_GS_VALUE_AT_OFFSET(0);

    if(old_metadata->state != FLASH_STACK_INITED){
        return;
    }

    if(old_metadata->is_randomizing){
        return;
    }
    mprotect(old_metadata, REAL_META_DATA_SIZE, PROT_WRITE | PROT_READ);
    old_metadata->is_randomizing = 1;
    //spa_set_relaxing_sandbox(1);

#if 1
    unsigned long last_clocks = old_metadata->cpu_cycles;
    unsigned long cur_clocks = __rdtsc();
    if((cur_clocks - last_clocks) < CPU_CYCLES_PER_RANDOMIZATION){
        old_metadata->is_randomizing = 0;
        return;
    }
#endif
    //long from = spa_get_cur_time_us();

    // continuously-changing


    old_metadata->cpu_cycles = cur_clocks;

#if defined(SPA_ENABLE_GS_MSR)
    long delta = SPA_GEN_RANDOM_VAL();
    old_metadata->R += delta;
    long ss_ptr =  (long) old_metadata->shadow_stack_top
                    - (((long) old_metadata->call_stack_top) - SPA_GET_RSP());

//    fprintf(stderr, "tid = %ld, call stack size = %ld\n",
//            syscall(SYS_gettid), (((long) old_metadata->call_stack_top) - SPA_GET_RSP()));

    avx2_64_x_4_add((unsigned long *) ss_ptr,
                    (unsigned long *)old_metadata->shadow_stack_top,
                    delta);
#endif
    //gs_cont_changing_count++;

    old_metadata->ratio_factor -= 1;
#if 1
    if(old_metadata->ratio_factor <= 0){ // fast-moving,  10 ms
        //gs_fast_moving_count++;
        old_metadata->ratio_factor = DEFAULT_CHANGING_MOVING_RATIO;
        // FIXME: we can provide a random value as the first argument.
        struct gs_metadata *new_metadata =
                (struct gs_metadata *) get_memory_at_random(REAL_META_DATA_SIZE);

        void * new_shadow_stack =
                (long *) get_memory_at_random(REAL_SHADOW_STACK_SIZE);

        if((new_metadata != MAP_FAILED) && (new_shadow_stack != MAP_FAILED)){
            // copy() is faster than mremap() for metatdata?
            *new_metadata = *old_metadata;
            // adjust the offset
            new_metadata->diff = old_metadata->diff + (((long) new_shadow_stack) - (long) (old_metadata->shadow_stack));
            //
            new_metadata->self_addr = new_metadata;
            //new_metadata->cpu_cycles = __rdtsc();

            void * old_shadow_stack = old_metadata->shadow_stack;
            // To be optimized, old size can be shrinked ?
            void *shadow_stack = mremap(old_shadow_stack,
                                     REAL_SHADOW_STACK_SIZE,
                                     REAL_SHADOW_STACK_SIZE,
                                     MREMAP_MAYMOVE | MREMAP_FIXED,
                                     new_shadow_stack);
            new_metadata->shadow_stack = shadow_stack;
            new_metadata->shadow_stack_top = shadow_stack + DEF_BUDDY_CALL_STACK_SIZE;
            arch_prctl(ARCH_SET_GS, new_metadata);

//            fprintf(stderr, "tid = %ld, old_shadow_stack = %p, new_stack_stack = %p, shadow_stack = %p\n",
//                    syscall(SYS_gettid), old_shadow_stack, new_shadow_stack, shadow_stack);
//            fprintf(stderr, "tid = %ld, old_metadata = %p, new_metadata = %p\n",
//                    syscall(SYS_gettid), old_metadata, new_metadata);

            munmap(old_metadata, REAL_META_DATA_SIZE);
            // sanity check

        }else{ // FIXME: it should not get here.
            fprintf(stderr, "tid = %ld: while(1) %s, %d \n",
                   syscall(SYS_gettid),  __FILE__, __LINE__);
            while(1);
        }
        old_metadata = new_metadata;
    }
#endif
    old_metadata->is_randomizing = 0;
    //spa_set_relaxing_sandbox(0);
    mprotect(old_metadata, REAL_META_DATA_SIZE, PROT_READ);
#if 0
    long to = spa_get_cur_time_us();
    time_for_gs_random += to - from;    
    gs_avg_rand_time = time_for_gs_random / gs_cont_changing_count;

    if(gs_cont_changing_count % 100 == 0){
        fprintf(stderr, "tid = %ld, time = %ld us, changing_count = %ld, GET_GS_VALUE_AT_OFFSET(0) = 0x%ld "
                        "moving_count = %ld, gs_avg_rand_time = %ld\n",
                syscall(SYS_gettid), to - from,
                gs_cont_changing_count,
                GET_GS_VALUE_AT_OFFSET(0),
                gs_fast_moving_count,
                gs_avg_rand_time);
    }
#endif

#if 0
    static long last_crash_cnt;
    if(total_crash_cnt > last_crash_cnt){
        last_crash_cnt = total_crash_cnt;
        fprintf(stderr, "pid = %d, tid = %ld, total_crash_cnt = %ld\n",
                getpid(), syscall(SYS_gettid), total_crash_cnt);
    }
#endif

#if defined(SAVE_TOTAL_RAND_CNT)
    //total_rand_cnt++;

    // lock addq $0x1,(%rax)
    // for httpd
    //__sync_fetch_and_add(&fsgs_total_rand_cnt, 1);
//    fsgs_thread_rand_cnt++;
//    fprintf(stderr, "###SPA_RAND###: tid = %ld,  pid = %d, thread_rand_cnt = %ld, total_rand_cnt = %ld, is randomizing .... \n",
//                        syscall(SYS_gettid), getpid(), fsgs_thread_rand_cnt, fsgs_total_rand_cnt);
#endif
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



static int __attribute__((constructor(101))) do_init_main_shadow_stack(void){

//    fprintf(stderr, "tid = %ld, do_init_main_shadow_stack():  %s, %d\n",
//                syscall(SYS_gettid), __FILE__, __LINE__);

    pthread_key_create(&thread_cleanup_key, thread_cleanup_handler);

    init_main_shadow_stack();
    buddy_init_rt_lib_hooker();
    return 0;
}





















