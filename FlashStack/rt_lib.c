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






//#define DUMP_STACK_DEPTH_MAX 4096

//#define ENABLE_SPA_LOGGER_FILE

// TBD

//#define SPA_DWARF_FRAME_REGISTERS  17

//struct _Unwind_Context
//{
//  void *reg[SPA_DWARF_FRAME_REGISTERS+1];
//  void *cfa;
//  void *ra;
//  void *lsda;
//};


//struct RegInfo{
//      union {
//        _Unwind_Word reg;
//        _Unwind_Sword offset;
//        const unsigned char *exp;
//      } loc;
//      enum {
//        REG_UNSAVED,
//        REG_SAVED_OFFSET,
//        REG_SAVED_REG,
//        REG_SAVED_EXP,
//      } how;
//};

//struct ArgInfo{
//    void *(*start_routine) (void *);
//    void *arg;
//};

// (1)
// Do randomization with a specified period

#if  defined(USE_SPA_FS_GS_TLS)

#define DO_RANDOMIZATION()  do{ \
    fsgs_runtime_rerandomize(); \
}while(0)

#elif defined(USE_SPA_GS_RSP)

#define DO_RANDOMIZATION()  do{ \
    gs_rsp_runtime_rerandomize(); \
}while(0)

#else

#define DO_RANDOMIZATION()  do{ \
    unsigned long rsp = SPA_GET_RSP(); \
    struct Buddy_TLS_Info  * tls_info = \
        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(rsp) - 2 * BUDDY_CALL_STACK_SIZE); \
    unsigned long last_clocks = tls_info->cpu_cycles; \
    unsigned long cur_clocks = __rdtsc(); \
    if((cur_clocks - last_clocks) > CPU_CYCLES_PER_RANDOMIZATION){ \
        au_edu_unsw_randomize(tls_info, cur_clocks); \
    } \
}while(0)

#endif
// (2)
// Do randomization every time

//#define DO_RANDOMIZATION()  do{ \
//    unsigned long rsp = SPA_GET_RSP(); \
//    struct Buddy_TLS_Info  * tls_info = \
//        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(rsp) - 2 * BUDDY_CALL_STACK_SIZE); \
//    unsigned long last_clocks = tls_info->cpu_cycles; \
//    unsigned long cur_clocks = __rdtsc(); \
//    if((cur_clocks - last_clocks) > 0){ \
//        au_edu_unsw_randomize(tls_info, cur_clocks); \
//    } \
//}while(0)


//#if defined(ENABLE_SPA_RDSTC)

//#define DO_RANDOMIZATION()  do{ \
//    unsigned long rsp = SPA_GET_RSP(); \
//    struct Buddy_TLS_Info  * tls_info = \
//        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(rsp) - 2 * BUDDY_CALL_STACK_SIZE); \
//    unsigned long last_clocks = tls_info->cpu_cycles; \
//    unsigned long cur_clocks = __rdtsc(); \
//    if((cur_clocks - last_clocks) > CPU_CYCLES_PER_RANDOMIZATION){ \
//        au_edu_unsw_randomize(tls_info, cur_clocks); \
//    } \
//}while(0)

//#else

//#define DO_RANDOMIZATION()  do{ \
//    unsigned long rsp = SPA_GET_RSP(); \
//    struct Buddy_TLS_Info  * tls_info = \
//        (struct Buddy_TLS_Info *) (BUDDY_ALIGN_CALL_STACK_POINTER(rsp) - 2 * BUDDY_CALL_STACK_SIZE); \
//    au_edu_unsw_randomize(tls_info, 0); \
//}while(0)

//#endif














//static void dump_trace() {
//    void *stack_trace[DUMP_STACK_DEPTH_MAX] = {0};
//    char **stack_strings = NULL;
//    int stack_depth = 0;
//    int i = 0;

//    stack_depth = backtrace(stack_trace, DUMP_STACK_DEPTH_MAX);


//    stack_strings = (char **)backtrace_symbols(stack_trace, stack_depth);
//    if (NULL == stack_strings) {
//        fprintf(stderr, "Out of memory!\n");
//        return;
//    }
//    for (i = 0; i < stack_depth; ++i) {
//        fprintf(stderr, " [%d] %s \r\n", i, stack_strings[i]);
//    }

//    free(stack_strings);
//    stack_strings = NULL;
//    return;
//}




//// puts
//#define SPA_ENABLE_HOOK_PUTS

//// gets
//#define SPA_ENABLE_HOOK_GETS

//// sendfile
//#define SPA_ENABLE_HOOK_SENDFILE

//// writev
//#define SPA_ENABLE_HOOK_WRITEV

//// getppid
//#define SPA_ENABLE_HOOK_GETPPID

//// getpid
//#define SPA_ENABLE_HOOK_GETPID

//// epoll_wait
//#define SPA_ENABLE_HOOK_EPOLL_WAIT

//// fork
//#define SPA_ENABLE_HOOK_FORK

#if 0
// pthread_self
#define SPA_ENABLE_HOOK_PTHREAD_SELF
#endif

#if 0
// write
#define SPA_ENABLE_HOOK_WRITE

// send
#define SPA_ENABLE_HOOK_SEND

// sendto
#define SPA_ENABLE_HOOK_SENDTO

// sendmsg
#define SPA_ENABLE_HOOK_SENDMSG

// read
#define SPA_ENABLE_HOOK_READ

// recvfrom
#define SPA_ENABLE_HOOK_RECVFROM

// recvmsg
#define SPA_ENABLE_HOOK_RECVMSG

// recv
#define SPA_ENABLE_HOOK_RECV
#endif



/////////////////////////////////////////////////////////////////////////////////////////////////

// write
#if defined(SPA_ENABLE_HOOK_WRITE)

typedef ssize_t (* WRITE_FUNC)(int fd, const void *buf, size_t count);
static WRITE_FUNC _write;

ssize_t write(int fd, const void *buf, size_t count){
    if(!_write){
        _write = (WRITE_FUNC) dlsym(RTLD_NEXT, "write");
    }
    ssize_t n = _write(fd, buf, count);
    DO_RANDOMIZATION();
    return n;
}
#endif




// puts
#if defined(SPA_ENABLE_HOOK_PUTS)

typedef int (* PUTS_FUNC)(const char *s);
static PUTS_FUNC _puts;

int puts(const char *s){

    if(!_puts){
        _puts = (PUTS_FUNC) dlsym(RTLD_NEXT, "puts");
    }
    // FIXME:  calling printf() here might lead to a crash. due to recursion ?
    int n = _puts(s);
    DO_RANDOMIZATION();
    return n;
}
#endif

// gets
#if defined(SPA_ENABLE_HOOK_GETS)

typedef char * (* GETS_FUNC)(char *s);
static GETS_FUNC _gets;

char *gets(char *s){

    if(!_gets){
        _gets = (GETS_FUNC) dlsym(RTLD_NEXT, "gets");
    }
    DO_RANDOMIZATION();
    char *r = _gets(s);
    return r;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////

// getppid
#if defined(SPA_ENABLE_HOOK_GETPPID)

typedef pid_t (*GETPPID_FUNC)(void);
static GETPPID_FUNC _getppid;

pid_t getppid(void){

    if(!_getppid){
        _getppid = (GETPPID_FUNC) dlsym(RTLD_NEXT, "getppid");
    }
    //SPA_DEBUG_OUTPUT(printf("getppid() hooked.\n"));
    DO_RANDOMIZATION();
    return _getppid();
}
#endif




// getpid
#if defined(SPA_ENABLE_HOOK_GETPID)

typedef pid_t (*GETPPID_FUNC)(void);
static GETPPID_FUNC _getpid;

pid_t getpid(void){

    if(!_getpid){
        _getpid = (GETPPID_FUNC) dlsym(RTLD_NEXT, "getpid");

    }
    DO_RANDOMIZATION();
    return _getpid();
}
#endif



// pthread_self
#if defined(SPA_ENABLE_HOOK_PTHREAD_SELF)

typedef pthread_t (* PTHREAD_SELF_FUNC)(void);
static PTHREAD_SELF_FUNC _pthread_self;

pthread_t pthread_self(void){

    if(!_pthread_self){
        _pthread_self = (PTHREAD_SELF_FUNC) dlsym(RTLD_NEXT, "pthread_self");
    }
    DO_RANDOMIZATION();
    pthread_t tid = _pthread_self();
    return tid;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// epoll_wait
#if defined(SPA_ENABLE_HOOK_EPOLL_WAIT)

typedef int (*EPOLL_WAIT_FUNC)(int epfd, struct epoll_event *events, int maxevents, int timeout);
static EPOLL_WAIT_FUNC _epoll_wait;

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout){

    if(!_epoll_wait){
        _epoll_wait = (EPOLL_WAIT_FUNC) dlsym(RTLD_NEXT, "epoll_wait");
    }
    int n = _epoll_wait(epfd, events, maxevents, timeout);
    DO_RANDOMIZATION();
    return n;
}
#endif



// sendto
#if defined(SPA_ENABLE_HOOK_SENDTO)

typedef ssize_t (* SENDTO_FUNC)(int sockfd, const void *buf, size_t len, int flags,
                       const struct sockaddr *dest_addr, socklen_t addrlen);
static SENDTO_FUNC _sendto;

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen){

    if(!_sendto){
        _sendto = (SENDTO_FUNC) dlsym(RTLD_NEXT, "sendto");
    }

    ssize_t n = _sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    DO_RANDOMIZATION();
    return n;
}
#endif

// sendmsg
#if defined(SPA_ENABLE_HOOK_SENDMSG)

typedef ssize_t (*SENDMSG_FUNC)(int sockfd, const struct msghdr *msg, int flags);
static SENDMSG_FUNC _sendmsg;

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags){

    if(!_sendmsg){
        _sendmsg = (SENDMSG_FUNC) dlsym(RTLD_NEXT, "sendmsg");
    }

    ssize_t n = _sendmsg(sockfd, msg, flags);
    DO_RANDOMIZATION();
    return n;
}
#endif


// fork
#if defined(SPA_ENABLE_HOOK_FORK)

typedef pid_t (*FORK_FUNC)(void);
static FORK_FUNC _fork;

pid_t fork(void){

    if(!_fork){
        _fork = (FORK_FUNC) dlsym(RTLD_NEXT, "fork");
    }
    pid_t pid = _fork();
    return pid;
}
#endif

// sendfile
#if defined(SPA_ENABLE_HOOK_SENDFILE)

typedef ssize_t (* SENDFILE_FUNC)(int out_fd, int in_fd, off_t *offset, size_t count);
static SENDFILE_FUNC _sendfile;

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count){

    if(!_sendfile){
        _sendfile = (SENDFILE_FUNC) dlsym(RTLD_NEXT, "sendfile");
    }
    ssize_t n = _sendfile(out_fd, in_fd, offset, count);
    DO_RANDOMIZATION();
    return n;
}
#endif


// send
#if defined(SPA_ENABLE_HOOK_SEND)

typedef ssize_t (* SEND_FUNC)(int sockfd, const void *buf, size_t len, int flags);
static SEND_FUNC _send;

ssize_t send(int sockfd, const void *buf, size_t len, int flags){

    if(!_send){
        _send = (SEND_FUNC) dlsym(RTLD_NEXT, "send");
    }

    ssize_t n = _send(sockfd, buf, len, flags);
    DO_RANDOMIZATION();
    return n;
}
#endif




// writev
#if defined(SPA_ENABLE_HOOK_WRITEV)

typedef ssize_t (*WRITEV_FUNC)(int fd, const struct iovec *iov, int iovcnt);
static WRITEV_FUNC _writev;

ssize_t writev(int fd, const struct iovec *iov, int iovcnt){

    if(!_writev){
        _writev = (WRITEV_FUNC) dlsym(RTLD_NEXT, "writev");
    }
    ssize_t n = _writev(fd, iov, iovcnt);
    DO_RANDOMIZATION();

    return n;
}
#endif

// read
#if defined(SPA_ENABLE_HOOK_READ)

typedef ssize_t (* READ_FUNC)(int fd, void *buf, size_t count);
static READ_FUNC _read;

ssize_t read(int fd, void *buf, size_t count){

    if(!_read){
        _read = (READ_FUNC) dlsym(RTLD_NEXT, "read");
    }
    DO_RANDOMIZATION();
    ssize_t n = _read(fd, buf, count);
    return n;
}
#endif


// recvfrom
#if defined(SPA_ENABLE_HOOK_RECVFROM)

typedef ssize_t (*RECVFROM_FUNC)(int sockfd, void *buf, size_t len, int flags,
                         struct sockaddr *src_addr, socklen_t *addrlen);
static RECVFROM_FUNC _recvfrom;

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen){

    if(!_recvfrom){
        _recvfrom = (RECVFROM_FUNC) dlsym(RTLD_NEXT, "recvfrom");
    }
    DO_RANDOMIZATION();
    ssize_t n = _recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

    return n;
}
#endif

// recvmsg
#if defined(SPA_ENABLE_HOOK_RECVMSG)

typedef ssize_t (*RECVMSG_FUNC)(int sockfd, struct msghdr *msg, int flags);
static RECVMSG_FUNC _recvmsg;

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){

    if(!_recvmsg){
        _recvmsg = (RECVMSG_FUNC) dlsym(RTLD_NEXT, "recvmsg");
    }
    DO_RANDOMIZATION();
    ssize_t n = _recvmsg(sockfd, msg, flags);

    return n;
}
#endif

// recv
#if defined(SPA_ENABLE_HOOK_RECV)

typedef ssize_t (* RECV_FUNC)(int sockfd, void *buf, size_t len, int flags);
static RECV_FUNC _recv;

ssize_t recv(int sockfd, void *buf, size_t len, int flags){

    if(!_recv){
        _recv = (RECV_FUNC) dlsym(RTLD_NEXT, "recv");
    }
    DO_RANDOMIZATION();
    ssize_t n = _recv(sockfd, buf, len, flags);

    return n;
}

#endif


//// pthread_cond_wait
//#if defined(SPA_ENABLE_HOOK_PTHREAD_COND_WAIT)

//typedef int (* PTHREAD_COND_WAIT_FUNC)(pthread_cond_t *cond, pthread_mutex_t *mutex);
//static PTHREAD_COND_WAIT_FUNC do_pthread_cond_wait;

//int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex){
//    if(! do_pthread_cond_wait){
//        do_pthread_cond_wait = (PTHREAD_COND_WAIT_FUNC) dlsym(RTLD_NEXT, "pthread_cond_wait");
//    }

//    int r = do_pthread_cond_wait(cond, mutex);
//    DO_RANDOMIZATION();
//    return r;
//}

//#endif

//// pthread_cond_timedwait
//#if defined(SPA_ENABLE_HOOK_PTHREAD_COND_TIMEDWAIT)

//typedef int (* PTHREAD_COND_TIMEDWAIT_FUNC)(pthread_cond_t *cond,
//    pthread_mutex_t *mutex, const struct timespec *abstime);
//static PTHREAD_COND_TIMEDWAIT_FUNC do_pthread_cond_timedwait;

//int pthread_cond_timedwait(pthread_cond_t *cond,
//                           pthread_mutex_t *mutex, const struct timespec *abstime){
//    if(! do_pthread_cond_timedwait){
//        do_pthread_cond_timedwait = (PTHREAD_COND_TIMEDWAIT_FUNC) dlsym(RTLD_NEXT, "pthread_cond_timedwait");
//    }

//    int r = do_pthread_cond_timedwait(cond, mutex, abstime);
//    //DO_RANDOMIZATION();
//    return r;
//}
//#endif

void buddy_init_rt_lib_hooker(void){
    // write
#if defined(SPA_ENABLE_HOOK_WRITE)

    if(!_write){
        _write = (WRITE_FUNC) dlsym(RTLD_NEXT, "write");
    }

#endif
    // puts
#if defined(SPA_ENABLE_HOOK_PUTS)

    if(!_puts){
        _puts = (PUTS_FUNC) dlsym(RTLD_NEXT, "puts");
    }

#endif
    // gets
#if defined(SPA_ENABLE_HOOK_GETS)

    if(!_gets){
        _gets = (GETS_FUNC) dlsym(RTLD_NEXT, "gets");
    }

#endif
    // getppid
#if defined(SPA_ENABLE_HOOK_GETPPID)

    if(!_getppid){
        _getppid = (GETPPID_FUNC) dlsym(RTLD_NEXT, "getppid");
    }

#endif
    // getpid
#if defined(SPA_ENABLE_HOOK_GETPID)

    if(!_getpid){

        _getpid = (GETPPID_FUNC) dlsym(RTLD_NEXT, "getpid");

    }

#endif
    // pthread_self
#if defined(SPA_ENABLE_HOOK_PTHREAD_SELF)

    if(!_pthread_self){
        _pthread_self = (PTHREAD_SELF_FUNC) dlsym(RTLD_NEXT, "pthread_self");
    }

#endif
    // epoll_wait
#if defined(SPA_ENABLE_HOOK_EPOLL_WAIT)

    if(!_epoll_wait){
        _epoll_wait = (EPOLL_WAIT_FUNC) dlsym(RTLD_NEXT, "epoll_wait");
    }

#endif
    // sendto
#if defined(SPA_ENABLE_HOOK_SENDTO)
    if(!_sendto){
        _sendto = (SENDTO_FUNC) dlsym(RTLD_NEXT, "sendto");
    }

#endif
    // sendmsg
#if defined(SPA_ENABLE_HOOK_SENDMSG)

    if(!_sendmsg){
        _sendmsg = (SENDMSG_FUNC) dlsym(RTLD_NEXT, "sendmsg");
    }

#endif
    // fork
#if defined(SPA_ENABLE_HOOK_FORK)

    if(!_fork){
        _fork = (FORK_FUNC) dlsym(RTLD_NEXT, "fork");
    }

#endif
    // sendfile
#if defined(SPA_ENABLE_HOOK_SENDFILE)

    if(!_sendfile){
        _sendfile = (SENDFILE_FUNC) dlsym(RTLD_NEXT, "sendfile");
    }

#endif
    // send
#if defined(SPA_ENABLE_HOOK_SEND)

    if(!_send){
        _send = (SEND_FUNC) dlsym(RTLD_NEXT, "send");
    }

#endif
    // writev
#if defined(SPA_ENABLE_HOOK_WRITEV)

    if(!_writev){
        _writev = (WRITEV_FUNC) dlsym(RTLD_NEXT, "writev");
    }

#endif
    // read
#if defined(SPA_ENABLE_HOOK_READ)

    if(!_read){
        _read = (READ_FUNC) dlsym(RTLD_NEXT, "read");
    }

#endif
    // recvfrom
#if defined(SPA_ENABLE_HOOK_RECVFROM)

    if(!_recvfrom){
        _recvfrom = (RECVFROM_FUNC) dlsym(RTLD_NEXT, "recvfrom");
    }

#endif
    // recvmsg
#if defined(SPA_ENABLE_HOOK_RECVMSG)

    if(!_recvmsg){
        _recvmsg = (RECVMSG_FUNC) dlsym(RTLD_NEXT, "recvmsg");
    }

#endif
    // recv
#if defined(SPA_ENABLE_HOOK_RECV)

    if(!_recv){
        _recv = (RECV_FUNC) dlsym(RTLD_NEXT, "recv");
    }

#endif


}


