#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <kernel/hal/output.h>
#include <kernel/hal/x86_64/drivers/bga.h>
#include <kernel/fs/dev.h>
#include <kernel/mem/page_frame_allocator.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/mem/vm_region.h>
#include <kernel/proc/process.h>

struct bga_data {
    uintptr_t frame_buffer;
    uint16_t x_res;
    uint16_t y_res;
};

static int bga_ioctl(struct device *device, unsigned long request, void *argp);
static intptr_t bga_mmap(struct device *device, void *addr, size_t len, int prot, int flags, off_t offset);

static struct bga_data data = { 0 };

struct device_ops bga_ops = {
    NULL, NULL, NULL, NULL, NULL, NULL, bga_ioctl, NULL, bga_mmap
};

struct device bga_device = {
    0x4200, S_IFCHR, "fb0", false, &bga_ops, NULL, &data
};

static int bga_ioctl(struct device *device, unsigned long request, void *argp) {
    assert(device);

    switch (request) {
        case SSRES: {
            struct screen_res *res = argp;
            bga_write(BGA_INDEX_ENABLE, BGA_VBE_DISABLED);
            bga_write(BGA_INDEX_X_RES, res->x);
            bga_write(BGA_INDEX_Y_RES, res->y);
            bga_write(BGA_INDEX_VIRT_WIDTH, res->x);
            bga_write(BGA_INDEX_VIRT_HEIGHT, res->y);
            bga_write(BGA_INDEX_X_OFFSET, 0);
            bga_write(BGA_INDEX_Y_OFFSET, 0);
            bga_write(BGA_INDEX_BPP, BGA_BPP_32);
            bga_write(BGA_INDEX_BANK, 0);
            bga_write(BGA_INDEX_ENABLE, BGA_VBE_ENABLED | BGA_LFB_ENABLED);
            data.x_res = res->x;
            data.y_res = res->y;
            return 0;
        }
        default:
            return -ENOTTY;
    }
}

static intptr_t bga_mmap(struct device *device, void *addr, size_t len, int prot, int flags, off_t offset) {
    if (offset != 0 || len != sizeof(uint32_t) * data.x_res * data.y_res || !(flags & MAP_SHARED)) {
        return -ENODEV;
    }

    (void) device;

    size_t total_size = sizeof(uint32_t) * (size_t) data.x_res * (size_t) data.y_res;
    debug_log("Framebuffer total size: [ %lu ]\n", total_size);

    struct process *process = get_current_process();
    struct vm_region *region = calloc(1, sizeof(struct vm_region));
    region->start = addr ? (uintptr_t) addr : 0x100000000LL;
    region->end = region->start + NUM_PAGES(region->start, region->start + total_size) * PAGE_SIZE;
    debug_log("Region End: [ %#.16lX ]\n", region->end);
    region->type = VM_DEVICE_MEMORY_MAP_DONT_FREE_PHYS_PAGES;
    region->flags = (prot & PROT_WRITE ? VM_WRITE : 0) | VM_USER;
    process->process_memory = add_vm_region(process->process_memory, region);
    for (uintptr_t i = region->start; i < region->end; i += PAGE_SIZE) {
        mark_used(data.frame_buffer + (i - region->start), PAGE_SIZE);
        map_phys_page(data.frame_buffer + (i - region->start), i, region->flags);
    }

    return (intptr_t) region->start;
}

void init_bga(struct pci_configuration *config) {
    debug_log("Detected bga device: [ %#.8X ]\n", config->bar[0] & ~0xF);

    data.frame_buffer = config->bar[0] & ~0xF;
    dev_add(&bga_device, bga_device.name);
}