#ifndef _KERNEL_FS_SUPER_BLOCK_H
#define _KERNEL_FS_SUPER_BLOCK_H 1

#include <stdint.h>
#include <sys/types.h>

#include <kernel/fs/file.h>
#include <kernel/fs/tnode.h>
#include <kernel/util/spinlock.h>

struct super_block_operations {

};

struct super_block {
    dev_t device;
    struct tnode *root;
    struct super_block_operations *op;
    blksize_t block_size;

    struct file *dev_file;
    spinlock_t super_block_lock;

    void *private_data;
};

#endif /* _KERNEL_FS_SUPER_BLOCK_H */