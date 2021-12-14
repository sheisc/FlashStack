/*****************************************************************
            SPA library for building
    We just want to make the Build process happy.
    When a thread is created, its stack size is set.
    So the tests during building can pass.
******************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <pthread.h>

#include <time.h>

#include "spa.h"




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

    stacksize = BUDDY_STACK_SIZE;


    if(!attr){
        attr = &threadAttr;
    }
    if(pthread_attr_setstacksize(( pthread_attr_t *)attr, stacksize) != 0){
        fprintf(stderr, "error in pthread_attr_setstacksize()\n");
    }
#if 0
    SPA_DEBUG_OUTPUT(printf("pthread_create() is hijacked. \n"));
#endif
    //fprintf(stderr, "%s, %s, %d\n", __FUNCTION__, __FILE__, __LINE__);

    return _pthread_create(thread, attr, start_routine, arg);
}




