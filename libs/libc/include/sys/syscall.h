#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <assert.h>

#define ENUMERATE_SYSCALLS                                                     \
    __ENUMERATE_SYSCALL(EXIT, exit, 1)                                         \
    __ENUMERATE_SYSCALL(SBRK, sbrk, 1)                                         \
    __ENUMERATE_SYSCALL(FORK, fork, 0)                                         \
    __ENUMERATE_SYSCALL(OPEN, open, 3)                                         \
    __ENUMERATE_SYSCALL(READ, read, 3)                                         \
    __ENUMERATE_SYSCALL(WRITE, write, 3)                                       \
    __ENUMERATE_SYSCALL(CLOSE, close, 1)                                       \
    __ENUMERATE_SYSCALL(EXECVE, execve, 3)                                     \
    __ENUMERATE_SYSCALL(WAITPID, waitpid, 3)                                   \
    __ENUMERATE_SYSCALL(GETPID, getpid, 0)                                     \
    __ENUMERATE_SYSCALL(GETCWD, getcwd, 2)                                     \
    __ENUMERATE_SYSCALL(CHDIR, chdir, 1)                                       \
    __ENUMERATE_SYSCALL(STAT, stat, 2)                                         \
    __ENUMERATE_SYSCALL(LSEEK, lseek, 3)                                       \
    __ENUMERATE_SYSCALL(IOCTL, ioctl, 3)                                       \
    __ENUMERATE_SYSCALL(FTRUNCATE, ftruncate, 2)                               \
    __ENUMERATE_SYSCALL(GETTIMEOFDAY, gettimeofday, 3)                         \
    __ENUMERATE_SYSCALL(MKDIR, mkdir, 2)                                       \
    __ENUMERATE_SYSCALL(DUP2, dup2, 2)                                         \
    __ENUMERATE_SYSCALL(PIPE, pipe, 1)                                         \
    __ENUMERATE_SYSCALL(UNLINK, unlink, 1)                                     \
    __ENUMERATE_SYSCALL(RMDIR, rmdir, 1)                                       \
    __ENUMERATE_SYSCALL(CHMOD, chmod, 1)                                       \
    __ENUMERATE_SYSCALL(KILL, kill, 2)                                         \
    __ENUMERATE_SYSCALL(SETPGID, setpgid, 2)                                   \
    __ENUMERATE_SYSCALL(SIGACTION, sigaction, 3)                               \
    __ENUMERATE_SYSCALL(SIGRETURN, sigreturn, 0)                               \
    __ENUMERATE_SYSCALL(SIGPROCMASK, sigprocmask, 3)                           \
    __ENUMERATE_SYSCALL(DUP, dup, 1)                                           \
    __ENUMERATE_SYSCALL(GETPGID, getpgid, 1)                                   \
    __ENUMERATE_SYSCALL(SLEEP, sleep, 1)                                       \
    __ENUMERATE_SYSCALL(ACCESS, access, 2)                                     \
    __ENUMERATE_SYSCALL(ACCEPT4, accept4, 4)                                   \
    __ENUMERATE_SYSCALL(BIND, bind, 3)                                         \
    __ENUMERATE_SYSCALL(CONNECT, connect, 3)                                   \
    __ENUMERATE_SYSCALL(LISTEN, listen, 2)                                     \
    __ENUMERATE_SYSCALL(SOCKET, socket, 3)                                     \
    __ENUMERATE_SYSCALL(SHUTDOWN, shutdown, 2)                                 \
    __ENUMERATE_SYSCALL(GETSOCKOPT, getsockopt, 5)                             \
    __ENUMERATE_SYSCALL(SETSOCKOPT, setsockopt, 5)                             \
    __ENUMERATE_SYSCALL(GETPEERNAME, getpeername, 3)                           \
    __ENUMERATE_SYSCALL(GETSOCKNAME, getsockname, 3)                           \
    __ENUMERATE_SYSCALL(SENDTO, sendto, 6)                                     \
    __ENUMERATE_SYSCALL(RECVFROM, recvfrom, 6)                                 \
    __ENUMERATE_SYSCALL(MMAP, mmap, 6)                                         \
    __ENUMERATE_SYSCALL(MUNMAP, munmap, 2)                                     \
    __ENUMERATE_SYSCALL(RENAME, rename, 2)                                     \
    __ENUMERATE_SYSCALL(FCNTL, fcntl, 3)                                       \
    __ENUMERATE_SYSCALL(FSTAT, fstat, 2)                                       \
    __ENUMERATE_SYSCALL(ALARM, alarm, 1)                                       \
    __ENUMERATE_SYSCALL(FCHMOD, fchmod, 2)                                     \
    __ENUMERATE_SYSCALL(GETPPID, getppid, 0)                                   \
    __ENUMERATE_SYSCALL(SIGSUSPEND, sigsuspend, 1)                             \
    __ENUMERATE_SYSCALL(TIMES, times, 1)                                       \
    __ENUMERATE_SYSCALL(CREATE_TASK, create_task, 1)                           \
    __ENUMERATE_SYSCALL(EXIT_TASK, exit_task, 0)                               \
    __ENUMERATE_SYSCALL(OS_MUTEX, os_mutex, 6)                                 \
    __ENUMERATE_SYSCALL(TGKILL, tgkill, 3)                                     \
    __ENUMERATE_SYSCALL(GET_INITIAL_PROCESS_INFO, get_initial_process_info, 1) \
    __ENUMERATE_SYSCALL(SET_THREAD_SELF_POINTER, set_thread_self_pointer, 2)   \
    __ENUMERATE_SYSCALL(MPROTECT, mprotect, 3)                                 \
    __ENUMERATE_SYSCALL(SIGPENDING, sigpending, 1)                             \
    __ENUMERATE_SYSCALL(SIGALTSTACK, sigaltstack, 2)                           \
    __ENUMERATE_SYSCALL(PSELECT, pselect, 6)                                   \
    __ENUMERATE_SYSCALL(SETUID, setuid, 1)                                     \
    __ENUMERATE_SYSCALL(SETEUID, seteuid, 1)                                   \
    __ENUMERATE_SYSCALL(SETGID, setgid, 1)                                     \
    __ENUMERATE_SYSCALL(SETEGID, setegid, 1)                                   \
    __ENUMERATE_SYSCALL(GETUID, getuid, 0)                                     \
    __ENUMERATE_SYSCALL(GETEUID, geteuid, 0)                                   \
    __ENUMERATE_SYSCALL(GETGID, getgid, 0)                                     \
    __ENUMERATE_SYSCALL(GETEGID, getegid, 0)                                   \
    __ENUMERATE_SYSCALL(UMASK, umask, 1)                                       \
    __ENUMERATE_SYSCALL(UNAME, uname, 1)                                       \
    __ENUMERATE_SYSCALL(GETSID, getsid, 1)                                     \
    __ENUMERATE_SYSCALL(SETSID, setsid, 0)                                     \
    __ENUMERATE_SYSCALL(READLINK, readlink, 3)                                 \
    __ENUMERATE_SYSCALL(LSTAT, lstat, 2)                                       \
    __ENUMERATE_SYSCALL(SYMLINK, symlink, 2)                                   \
    __ENUMERATE_SYSCALL(LINK, link, 2)                                         \
    __ENUMERATE_SYSCALL(CHOWN, chown, 3)                                       \
    __ENUMERATE_SYSCALL(UTIMES, utimes, 2)                                     \
    __ENUMERATE_SYSCALL(PREAD, pread, 4)                                       \
    __ENUMERATE_SYSCALL(PWRITE, pwrite, 4)                                     \
    __ENUMERATE_SYSCALL(READV, readv, 3)                                       \
    __ENUMERATE_SYSCALL(WRITEV, writev, 3)

#ifdef __ASSEMBLER__
#define SC_SIGRETURN 27
#else

enum sc_number {
#define __ENUMERATE_SYSCALL(x, y, v) SC_##x,
    SC_START,
    ENUMERATE_SYSCALLS SC_NUM
};

long __do_syscall(int sc, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5, unsigned long a6);

#define __syscall0(n)             __do_syscall(n, 0, 0, 0, 0, 0, 0)
#define __syscall1(n, a)          __do_syscall(n, (unsigned long) (a), 0, 0, 0, 0, 0)
#define __syscall2(n, a, b)       __do_syscall(n, (unsigned long) (a), (unsigned long) (b), 0, 0, 0, 0)
#define __syscall3(n, a, b, c)    __do_syscall(n, (unsigned long) (a), (unsigned long) (b), (unsigned long) (c), 0, 0, 0)
#define __syscall4(n, a, b, c, d) __do_syscall(n, (unsigned long) (a), (unsigned long) (b), (unsigned long) (c), (unsigned long) (d), 0, 0)
#define __syscall5(n, a, b, c, d, e) \
    __do_syscall(n, (unsigned long) (a), (unsigned long) (b), (unsigned long) (c), (unsigned long) (d), (unsigned long) (e), 0)
#define __syscall6(n, a, b, c, d, e, f)                                                                                      \
    __do_syscall(n, (unsigned long) (a), (unsigned long) (b), (unsigned long) (c), (unsigned long) (d), (unsigned long) (e), \
                 (unsigned long) (f))

#define __COUNT_ARGS(...)                               __COUNT_ARGS_(, ##__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0)
#define __COUNT_ARGS_(z, a, b, c, d, e, f, g, cnt, ...) cnt

#define __SYSCALL_WITH_ARG_COUNT(...)  __SYSCALL_WITH_ARG_COUNT_(__VA_ARGS__)
#define __SYSCALL_WITH_ARG_COUNT_(...) __syscall##__VA_ARGS__

#undef __ENUMERATE_SYSCALL
#define __ENUMERATE_SYSCALL(x, y, v) __SC_##x##_ARG_COUNT = v,
enum __sc_arg_count { ENUMERATE_SYSCALLS __INVALID };

#ifdef __is_libc
#define syscall(n, ...)                                                                                              \
    ({                                                                                                               \
        _Static_assert(__COUNT_ARGS(__VA_ARGS__) == __##n##_ARG_COUNT, "Incorrect number of arguments for " #n "."); \
        __SYSCALL_WITH_ARG_COUNT(__COUNT_ARGS(__VA_ARGS__))(n, ##__VA_ARGS__);                                       \
    })
#else
#define syscall(n, ...) __SYSCALL_WITH_ARG_COUNT(__COUNT_ARGS(__VA_ARGS__))(n, ##__VA_ARGS__)
#endif /* __is_libc */

char *syscall_to_string(enum sc_number sc);

__attribute__((__noreturn__)) void __sigreturn(void);

#endif /*__ASSEMBLER */

#endif /* _SYS_SYSCALL_H */