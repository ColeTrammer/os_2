#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

int setgid(gid_t gid) {
    int ret = (int) syscall(SYS_setgid, gid);
    __SYSCALL_TO_ERRNO(ret);
}
