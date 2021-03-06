#include <errno.h>
#include <sys/syscall.h>
#include <time.h>

int clock_gettime(clockid_t id, struct timespec *tp) {
    int ret = (int) syscall(SYS_clock_gettime, id, tp);
    __SYSCALL_TO_ERRNO(ret);
}
