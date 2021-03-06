#include <assert.h>
#include <stdlib.h>

#include <kernel/fs/dev.h>
#include <kernel/fs/vfs.h>
#include <kernel/hal/processor.h>
#include <kernel/mem/page.h>
#include <kernel/mem/phys_vm_object.h>
#include <kernel/mem/vm_region.h>
#include <kernel/proc/task.h>

static int phys_map(struct vm_object *self, struct vm_region *region) {
    struct phys_vm_object_data *data = self->private_data;
    assert(((data->size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE >= region->end - region->start);

    struct process *process = get_current_task()->process;
    bool should_broadcast_flush_tlb = atomic_load(&process->ref_count) > 1;
    for (uintptr_t i = region->start; i < region->end; i += PAGE_SIZE) {
        do_map_phys_page(region->vm_object_offset + (i - region->start) + data->phys_start, i, region->flags, should_broadcast_flush_tlb,
                         process);
    }

    return 0;
}

static int phys_kill(struct vm_object *self) {
    struct phys_vm_object_data *data = self->private_data;
    assert(data);

    if (data->on_kill) {
        return data->on_kill(data->closure);
    }

    free(data);
    return 0;
}

static struct vm_object_operations phys_ops = { .map = &phys_map, .kill = &phys_kill };

struct vm_object *vm_create_phys_object(uintptr_t phys_start, size_t size, int (*on_kill)(void *closure), void *closure) {
    struct phys_vm_object_data *data = malloc(sizeof(struct phys_vm_object_data));
    assert(data);

    data->phys_start = phys_start;
    data->size = size;
    data->on_kill = on_kill;
    data->closure = closure;

    return vm_create_object(VM_PHYS, &phys_ops, data);
}

int inode_on_kill(void *_inode) {
    struct inode *inode = _inode;
    drop_inode_reference(inode);
    inode->vm_object = NULL;
    return 0;
}

int device_on_kill(void *_device) {
    struct fs_device *device = _device;
    device->vm_object = NULL;
    return 0;
}
