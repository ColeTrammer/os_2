#include <sys/syscall.h>
#include <unistd.h>

uid_t getuid(void) {
    return (uid_t) syscall(SYS_getuid);
}
