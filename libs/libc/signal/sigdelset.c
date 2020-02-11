#include <errno.h>
#include <signal.h>

int sigdelset(sigset_t *set, int signum) {
    if (signum < 1 || signum > _NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1U << (signum - 1));
    return 0;
}