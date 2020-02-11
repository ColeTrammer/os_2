#ifndef _KERNEL_TIME_CLOCK_H
#define _KERNEL_TIME_CLOCK_H 1

#include <sys/time.h>
#include <time.h>

struct clock {
    clockid_t id;
    struct timespec resolution;
    struct timespec time;
};

struct clock *time_create_clock(clockid_t id);
void time_destroy_clock(struct clock *clock);

struct clock *time_get_clock(clockid_t id);
struct timespec time_read_clock(clockid_t id);

void init_clocks();

static inline __attribute__((always_inline)) void time_inc_clock(struct clock *clock, long nanoseconds) {
    clock->time.tv_nsec += nanoseconds;
    if (clock->time.tv_nsec >= 1000000000L) {
        clock->time.tv_nsec %= 1000000000L;
        clock->time.tv_sec++;
    }
}

static inline __attribute__((always_inline)) long time_compare(struct timespec t1, struct timespec t2) {
    if (t1.tv_sec == t2.tv_sec) {
        return t1.tv_nsec - t2.tv_nsec;
    }

    return t1.tv_sec - t2.tv_sec;
}

static inline __attribute__((always_inline)) struct timespec time_add(struct timespec t1, struct timespec t2) {
    t1.tv_sec += t2.tv_sec;
    t1.tv_nsec += t2.tv_nsec;
    if (t1.tv_nsec >= 1000000000L) {
        t1.tv_nsec %= 1000000000L;
        t1.tv_sec++;
    }

    return t1;
}

static inline __attribute__((always_inline)) struct timespec time_sub(struct timespec t1, struct timespec t2) {
    t1.tv_sec -= t2.tv_sec;
    t1.tv_nsec -= t2.tv_nsec;
    if (t1.tv_nsec < 0) {
        t1.tv_nsec += 1000000000L;
        t1.tv_sec--;
    }

    return t1;
}

static inline __attribute__((always_inline)) struct timespec time_from_timeval(struct timeval v) {
    return (struct timespec) { .tv_sec = v.tv_sec, .tv_nsec = v.tv_usec * 1000 };
}

extern struct clock global_monotonic_clock;
extern struct clock global_realtime_clock;

#endif /* _KERNEL_TIME_CLOCK_H */