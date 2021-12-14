// gettimeofday
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

unsigned long spa_get_cur_time_us(void) {
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    return (tv.tv_sec * 1000000UL) + tv.tv_usec;
}


//unsigned long spa_get_cur_time_us(void) {
//    struct timespec t_spec;

//    clock_gettime(CLOCK_MONOTONIC, &t_spec);

//    return (t_spec.tv_sec * 1000000UL) + (t_spec.tv_nsec / 1000);
//}
