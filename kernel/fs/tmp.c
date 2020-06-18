#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <kernel/fs/cached_dirent.h>
#include <kernel/fs/file.h>
#include <kernel/fs/file_system.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/super_block.h>
#include <kernel/fs/tmp.h>
#include <kernel/fs/vfs.h>
#include <kernel/hal/output.h>
#include <kernel/hal/timer.h>
#include <kernel/mem/inode_vm_object.h>
#include <kernel/mem/page.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/mem/vm_region.h>
#include <kernel/proc/task.h>
#include <kernel/time/clock.h>
#include <kernel/util/spinlock.h>

static struct file_system fs;

static spinlock_t inode_count_lock = SPINLOCK_INITIALIZER;
static ino_t inode_counter = 1;
static dev_t tmp_fs_id = 0x210;

static struct file_system fs = { "tmpfs", 0, &tmp_mount, NULL, NULL };

static struct super_block_operations s_op = { &tmp_rename };

static struct inode_operations tmp_i_op = { .lookup = &tmp_lookup,
                                            .open = &tmp_open,
                                            .unlink = &tmp_unlink,
                                            .chmod = &tmp_chmod,
                                            .chown = &tmp_chown,
                                            .mmap = &tmp_mmap,
                                            .read_all = &tmp_read_all,
                                            .utimes = &tmp_utimes,
                                            .on_inode_destruction = &tmp_on_inode_destruction };

static struct inode_operations tmp_dir_i_op = { .mknod = &tmp_mknod,
                                                .lookup = &tmp_lookup,
                                                .open = &tmp_open,
                                                .mkdir = &tmp_mkdir,
                                                .rmdir = &tmp_rmdir,
                                                .chmod = &tmp_chmod,
                                                .chown = &tmp_chown,
                                                .utimes = &tmp_utimes };

static struct file_operations tmp_f_op = { .read = &tmp_read, .write = &tmp_write };

static struct file_operations tmp_dir_f_op = { 0 };

static ino_t get_next_tmp_index() {
    spin_lock(&inode_count_lock);
    ino_t next = inode_counter++;
    spin_unlock(&inode_count_lock);
    return next;
}

struct inode *tmp_mknod(struct tnode *tparent, const char *name, mode_t mode, dev_t device, int *error) {
    assert(tparent);
    assert(tparent->inode->flags & FS_DIR);
    assert(name);

    debug_log("Tmp create: [ %s ]\n", name);

    struct tmp_data *data = calloc(1, sizeof(struct tmp_data));
    data->owner = get_current_task()->process->pid;

    struct inode *inode = fs_create_inode(tparent->inode->super_block, get_next_tmp_index(), get_current_task()->process->euid,
                                          get_current_task()->process->egid, mode, 0, &tmp_i_op, data);
    if (inode == NULL) {
        *error = ENOMEM;
        return NULL;
    }

    if (inode->flags & FS_DEVICE) {
        fs_bind_device_to_inode(inode, device);
    }

    tparent->inode->modify_time = time_read_clock(CLOCK_REALTIME);
    return inode;
}

struct inode *tmp_lookup(struct inode *inode, const char *name) {
    if (inode == NULL || name == NULL) {
        return NULL;
    }

    return fs_lookup_in_cache(inode->dirent_cache, name);
}

struct file *tmp_open(struct inode *inode, int flags, int *error) {
    *error = 0;
    return fs_create_file(inode, inode->flags, 0, flags, (inode->flags & FS_DIR) ? &tmp_dir_f_op : &tmp_f_op, NULL);
}

ssize_t tmp_read(struct file *file, off_t offset, void *buffer, size_t len) {
    struct inode *inode = fs_file_inode(file);
    assert(inode);

    struct tmp_data *data = inode->private_data;
    assert(data);

    spin_lock(&inode->lock);
    size_t to_read = MIN(len, inode->size - offset);

    if (to_read == 0) {
        spin_unlock(&inode->lock);
        return 0;
    }

    memcpy(buffer, data->contents + offset, to_read);
    offset += to_read;

    inode->access_time = time_read_clock(CLOCK_REALTIME);

    spin_unlock(&inode->lock);
    return (ssize_t) to_read;
}

ssize_t tmp_write(struct file *file, off_t offset, const void *buffer, size_t len) {
    if (len == 0) {
        return len;
    }

    struct inode *inode = fs_file_inode(file);
    assert(inode);

    struct tmp_data *data = inode->private_data;
    assert(data);

    spin_lock(&inode->lock);
    if (data->contents == NULL) {
        data->max = ((offset + len + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        data->contents = aligned_alloc(PAGE_SIZE, data->max);
        assert(data->contents);
        assert(((uintptr_t) data->contents) % PAGE_SIZE == 0);
    }

    size_t old_size = inode->size;
    inode->size = MAX(inode->size, offset + len);
    if (inode->size > data->max) {
        data->max = ((inode->size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        assert(data->max >= inode->size);
        char *save = data->contents;
        data->contents = aligned_alloc(PAGE_SIZE, data->max);
        assert(((uintptr_t) data->contents) % PAGE_SIZE == 0);
        memcpy(data->contents, save, old_size);
        free(save);
    }

    memcpy(data->contents + offset, buffer, len);

    inode->modify_time = time_read_clock(CLOCK_REALTIME);

    spin_unlock(&inode->lock);
    return (ssize_t) len;
}

struct inode *tmp_mkdir(struct tnode *tparent, const char *name, mode_t mode, int *error) {
    assert(name);

    struct inode *inode = fs_create_inode(tparent->inode->super_block, get_next_tmp_index(), get_current_task()->process->euid,
                                          get_current_task()->process->egid, mode, 0, &tmp_dir_i_op, NULL);
    tparent->inode->modify_time = time_read_clock(CLOCK_REALTIME);

    *error = 0;
    return inode;
}

int tmp_unlink(struct tnode *tnode) {
    (void) tnode;
    return 0;
}

int tmp_rmdir(struct tnode *tnode) {
    (void) tnode;
    return 0;
}

int tmp_chmod(struct inode *inode, mode_t mode) {
    inode->mode = mode;
    inode->modify_time = inode->access_time = time_read_clock(CLOCK_REALTIME);
    return 0;
}

int tmp_chown(struct inode *inode, uid_t uid, gid_t gid) {
    inode->uid = uid;
    inode->gid = gid;
    inode->modify_time = inode->access_time = time_read_clock(CLOCK_REALTIME);
    return 0;
}

int tmp_utimes(struct inode *inode, const struct timespec *times) {
    if (times[0].tv_nsec != UTIME_OMIT) {
        inode->access_time = times[0];
    }

    if (times[1].tv_nsec != UTIME_OMIT) {
        inode->modify_time = times[1];
    }
    return 0;
}

int tmp_rename(struct tnode *tnode, struct tnode *new_parent, const char *new_name) {
    (void) tnode;
    (void) new_name;

    new_parent->inode->modify_time = time_read_clock(CLOCK_REALTIME);

    return 0;
}

intptr_t tmp_mmap(void *addr, size_t len, int prot, int flags, struct inode *inode, off_t offset) {
    if (offset != 0 || !(flags & MAP_SHARED) || len > inode->size || len == 0) {
        return -EINVAL;
    }

    struct tmp_data *data = inode->private_data;
    if (!inode->vm_object) {
        inode->vm_object = vm_create_direct_inode_object(inode, data->contents);
    } else {
        bump_vm_object(inode->vm_object);
    }

    struct vm_region *region = map_region(addr, len, prot, VM_DEVICE_MEMORY_MAP_DONT_FREE_PHYS_PAGES);
    region->vm_object = inode->vm_object;
    region->vm_object_offset = 0;
    region->flags |= VM_SHARED;

    int ret = vm_map_region_with_object(region);
    if (ret < 0) {
        return (intptr_t) ret;
    }

    return (intptr_t) region->start;
}

int tmp_read_all(struct inode *inode, void *buffer) {
    struct tmp_data *data = inode->private_data;

    spin_lock(&inode->lock);
    memcpy(buffer, data->contents, inode->size);
    inode->access_time = time_read_clock(CLOCK_REALTIME);
    spin_unlock(&inode->lock);

    return 0;
}

void tmp_on_inode_destruction(struct inode *inode) {
    struct tmp_data *data = inode->private_data;
    if (data) {
        free(data->contents);
        free(data);
    }
}

struct inode *tmp_mount(struct file_system *current_fs, struct device *device) {
    assert(current_fs != NULL);
    assert(!device);

    struct super_block *sb = calloc(1, sizeof(struct super_block));
    sb->block_size = PAGE_SIZE;
    sb->fsid = tmp_fs_id++;
    sb->op = &s_op;
    sb->private_data = NULL;
    init_spinlock(&sb->super_block_lock);

    struct inode *root = fs_create_inode(sb, get_next_tmp_index(), 0, 0, S_IFDIR | 0777, 0, &tmp_dir_i_op, NULL);

    sb->root = root;
    current_fs->super_block = sb;

    return root;
}

void init_tmpfs() {
    load_fs(&fs);
}
