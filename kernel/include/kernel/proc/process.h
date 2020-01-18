#ifndef _KERNEL_PROC_PROCESS_H
#define _KERNEL_PROC_PROCESS_H 1

#include <signal.h>
#include <stdio.h>
#include <sys/times.h>
#include <sys/types.h>

#include <kernel/proc/user_mutex.h>
#include <kernel/util/spinlock.h>

// clang-format off
#include <kernel/arch/arch.h>
#include ARCH_SPECIFIC(proc/process.h)
// clang-format on

struct file;

struct file_descriptor {
    struct file *file;
    int fd_flags;
};

struct tnode;

struct process {
    struct tnode *cwd;
    struct file_descriptor files[FOPEN_MAX];

    struct vm_region *process_memory;

    struct user_mutex *used_user_mutexes;

    pid_t pid;
    pid_t pgid;
    pid_t ppid;
    pid_t sid;

    uid_t uid;
    uid_t euid;
    gid_t gid;
    gid_t egid;

    mode_t umask;

    int tty;
    int ref_count;

    struct arch_process arch_process;

    ino_t inode_id;
    dev_t inode_dev;

    struct tms times;

    // TLS info
    void *tls_master_copy_start;
    size_t tls_master_copy_size;
    size_t tls_master_copy_alignment;

    struct sigaction sig_state[_NSIG];
    stack_t alt_stack;
    spinlock_t lock;
};

void proc_drop_process_unlocked(struct process *process, bool free_paging_structure);
void proc_drop_process(struct process *process, bool free_paging_structure);
void proc_add_process(struct process *process);
void proc_bump_process(struct process *process);
struct process *find_by_pid(pid_t pid);
void proc_set_sig_pending(struct process *process, int n);
void init_processes();

#endif /* _KERNEL_PROC_PROCESS_H */