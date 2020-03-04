#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/fs/file.h>
#include <kernel/fs/file_system.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/inode_store.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/super_block.h>
#include <kernel/fs/vfs.h>
#include <kernel/hal/output.h>
#include <kernel/hal/timer.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/mem/vm_region.h>
#include <kernel/proc/task.h>
#include <kernel/time/clock.h>
#include <kernel/util/spinlock.h>

static struct file_system fs;
static struct super_block super_block;

static __attribute__((unused)) spinlock_t inode_counter_lock = SPINLOCK_INITIALIZER;
static ino_t inode_counter = 1;

static struct file_system fs = { "procfs", 0, &procfs_mount, NULL, NULL };

static __attribute__((unused)) struct inode_operations procfs_i_op = { NULL, NULL, &procfs_open, NULL, NULL, NULL, NULL, NULL,
                                                                       NULL, NULL, NULL,         NULL, NULL, NULL, NULL, NULL };

static struct inode_operations procfs_dir_i_op = { NULL, &procfs_lookup, &procfs_open, NULL, NULL, NULL, NULL, NULL,
                                                   NULL, NULL,           NULL,         NULL, NULL, NULL, NULL, NULL };

static struct file_operations procfs_f_op = { NULL, &procfs_read, NULL, NULL };

static struct file_operations procfs_dir_f_op = { NULL, NULL, NULL, NULL };

struct tnode *procfs_lookup(struct inode *inode, const char *name) {
    if (inode == NULL || inode->tnode_list == NULL || name == NULL) {
        return NULL;
    }

    struct tnode_list *list = inode->tnode_list;
    while (list != NULL) {
        if (strcmp(list->tnode->name, name) == 0) {
            return list->tnode;
        }
        list = list->next;
    }

    return NULL;
}

struct file *procfs_open(struct inode *inode, int flags, int *error) {
    assert(!(flags & O_RDWR));

    struct file *file = calloc(1, sizeof(struct file));
    file->device = inode->device;
    file->f_op = (inode->flags & FS_DIR) ? &procfs_dir_f_op : &procfs_f_op;
    file->flags = inode->flags;
    file->inode_idenifier = inode->index;
    file->length = inode->size;
    file->position = 0;
    file->start = 0;
    file->abilities = 0;
    file->ref_count = 0;

    *error = 0;
    return file;
}

ssize_t procfs_read(struct file *file, off_t offset, void *buffer, size_t _len) {
    (void) file;
    (void) offset;
    (void) buffer;
    (void) _len;
    return -EOPNOTSUPP;
}

struct tnode *procfs_mount(struct file_system *current_fs, char *device_path) {
    assert(current_fs != NULL);
    assert(strlen(device_path) == 0);

    struct inode *root = calloc(1, sizeof(struct inode));
    struct tnode *t_root = create_root_tnode(root);

    root->device = 4;
    root->flags = FS_DIR;
    root->i_op = &procfs_dir_i_op;
    root->index = inode_counter++;
    init_spinlock(&root->lock);
    root->mode = S_IFDIR | 0777;
    root->mounts = NULL;
    root->private_data = NULL;
    root->size = 0;
    root->super_block = &super_block;
    root->tnode_list = NULL;
    root->ref_count = 1;
    root->readable = true;
    root->writeable = true;
    root->access_time = root->change_time = root->modify_time = time_read_clock(CLOCK_REALTIME);

    super_block.device = root->device;
    super_block.op = NULL;
    super_block.root = t_root;
    super_block.block_size = PAGE_SIZE;

    current_fs->super_block = &super_block;

    return t_root;
}

void init_procfs() {
    load_fs(&fs);
}