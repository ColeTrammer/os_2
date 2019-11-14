#ifndef _TIME_H
#define _TIME_H 1

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

time_t time(time_t *tloc);
clock_t clock(void);
double difftime(time_t t1, time_t t2);
time_t mktime(struct tm *tm);
char *asctime(const struct tm *tm);
char *ctime(const time_t *t);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
size_t strftime(char *__restrict s, size_t n, const char *__restrict format, const struct tm *__restrict tm);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _TIME_H */