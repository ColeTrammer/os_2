#include <sys/syscall.h>
#include <unistd.h>

gid_t getgid(void) {
    return (gid_t) syscall(SYS_getgid);
}
