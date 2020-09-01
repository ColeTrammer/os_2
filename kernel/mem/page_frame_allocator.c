#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include <kernel/hal/block.h>
#include <kernel/hal/hal.h>
#include <kernel/hal/output.h>
#include <kernel/mem/kernel_vm.h>
#include <kernel/mem/page.h>
#include <kernel/mem/page_frame_allocator.h>
#include <kernel/proc/task.h>
#include <kernel/util/spinlock.h>

// #define PAGE_FRAME_ALLOCATOR_DEBUG

static uintptr_t page_bitmap[PAGE_BITMAP_SIZE / sizeof(uintptr_t)];
static spinlock_t bitmap_lock = SPINLOCK_INITIALIZER;

static bool get_bit(uintptr_t bit_index) {
    return page_bitmap[bit_index / (8 * sizeof(uintptr_t))] & (1UL << (bit_index % (8 * sizeof(uintptr_t))));
}

static void set_bit(uintptr_t bit_index, bool value) {
    if (value) {
        page_bitmap[bit_index / (8 * sizeof(uintptr_t))] |= (1UL << (bit_index % (8 * sizeof(uintptr_t))));
    } else {
        page_bitmap[bit_index / (8 * sizeof(uintptr_t))] &= ~(1UL << (bit_index % (8 * sizeof(uintptr_t))));
    }
}

void mark_used(uintptr_t phys_addr_start, uintptr_t length) {
    uintptr_t num_pages = NUM_PAGES(phys_addr_start, phys_addr_start + length);
    uintptr_t bit_index_base = phys_addr_start / PAGE_SIZE;
    for (uintptr_t i = 0; i < num_pages; i++) {
        set_bit(bit_index_base + i, true);
    }
}

void mark_available(uintptr_t phys_addr_start, uintptr_t length) {
    uintptr_t num_pages = NUM_PAGES(phys_addr_start, phys_addr_start + length);
    uintptr_t bit_index_base = phys_addr_start / PAGE_SIZE;
    for (uintptr_t i = 0; i < num_pages; i++) {
        set_bit(bit_index_base + i, false);
    }
}

static uintptr_t try_get_next_phys_page(struct process *process) {
    spin_lock(&bitmap_lock);
    for (uintptr_t i = 0; i < PAGE_BITMAP_SIZE / sizeof(uintptr_t); i++) {
        if (~page_bitmap[i]) {
            uintptr_t bit_index = i * 8 * sizeof(uintptr_t);
            while (get_bit(bit_index)) {
                bit_index++;
            }
            set_bit(bit_index, true);

            spin_unlock(&bitmap_lock);

            process->resident_memory += PAGE_SIZE;
#ifdef PAGE_FRAME_ALLOCATOR_DEBUG
            debug_log("allocated: [ %#.16lX ]\n", bit_index * PAGE_SIZE);
#endif /* PAGE_FRAME_ALLOCATOR_DEBUG */
            return bit_index * PAGE_SIZE;
        }
    }
    spin_unlock(&bitmap_lock);
    return 0;
}

uintptr_t get_next_phys_page(struct process *process) {
    uintptr_t try_1 = try_get_next_phys_page(process);
    if (try_1) {
        return try_1;
    }

    // The block cache will hopefully have unused pages it can release.
    block_trim_cache();

    uintptr_t try_2 = try_get_next_phys_page(process);
    if (try_2) {
        return try_2;
    }

    debug_log("Out of Physical Memory\n");
    abort();
    return 0;
}

uintptr_t get_contiguous_pages(size_t pages) {
    uintptr_t ret = 0;
    spin_lock(&bitmap_lock);

try_again:
    for (size_t i = 0; i < PAGE_BITMAP_SIZE / sizeof(uintptr_t); i++) {
        if (~page_bitmap[i]) {
            uintptr_t bit_index = i * 8 * sizeof(uintptr_t);
            while (get_bit(bit_index)) {
                bit_index++;
            }

            bit_index++;
            for (size_t consecutive_pages = 1; consecutive_pages < pages; consecutive_pages++) {
                if (get_bit(bit_index++)) {
                    goto try_again;
                }
            }

            for (size_t i = bit_index - pages; i < bit_index; i++) {
                set_bit(i, true);
            }
            ret = (bit_index - pages) * PAGE_SIZE;
            break;
        }
    }

    spin_unlock(&bitmap_lock);
    return ret;
}

void free_phys_page(uintptr_t phys_addr, struct process *process) {
#ifdef PAGE_FRAME_ALLOCATOR_DEBUG
    debug_log("freed: [ %#.16lX ]\n", phys_addr);
#endif /* PAGE_FRAME_ALLOCATOR_DEBUG */

    spin_lock(&bitmap_lock);

    set_bit(phys_addr / PAGE_SIZE, false);
    if (process) {
        process->resident_memory -= PAGE_SIZE;
    }

    spin_unlock(&bitmap_lock);
}

static unsigned long phys_memory_total = 0;
static unsigned long phys_memory_max = 0;

unsigned long get_total_phys_memory(void) {
    return phys_memory_total;
}

unsigned long get_max_phys_memory(void) {
    return phys_memory_max;
}

uintptr_t initrd_phys_start;
uintptr_t initrd_phys_end;

void init_page_frame_allocator(uint32_t *multiboot_info) {
    // Everything starts off allocated (reserved). Only usable segments (according to the bootloader) are made available.
    memset(page_bitmap, 0xFF, sizeof(page_bitmap));

    debug_log("multiboot_info: [ %p ]\n", multiboot_info);
    assert((uintptr_t) multiboot_info < 0x400000ULL);

    uint32_t *data = multiboot_info + 2;
    while (data < multiboot_info + multiboot_info[0] / sizeof(uint32_t)) {
        if (data[0] == 1) {
            char *cmd_line = (char *) &data[2];
            debug_log("kernel command line: [ %s ]\n", cmd_line);
            if (strcmp(cmd_line, "graphics=0") == 0) {
                kernel_disable_graphics();
            }
        }

        if (data[0] == 6) {
            uintptr_t *mem = (uintptr_t *) (data + 4);
            while ((uint32_t *) mem < data + data[1] / sizeof(uint32_t)) {
                debug_log("Physical memory range: [ %#.16lX, %#.16lX, %u ]\n", mem[0] & ~0xFFF, mem[1], (uint32_t) mem[2]);
                if ((uint32_t) mem[2] == 1) {
                    mark_available(mem[0] & ~0xFFF, mem[1]);
                    phys_memory_total += mem[1] - (mem[0] & ~0xFFF);
                }
                phys_memory_max = MAX((mem[0] & ~0xFFF) + mem[1], phys_memory_max);
                mem += data[2] / sizeof(uintptr_t);
            }
        }

        if (data[0] == 3) {
            initrd_phys_start = data[2];
            initrd_phys_end = data[3];
            debug_log("kernel module: [ %s ]\n", (char *) &data[4]);
        }

        data = (uint32_t *) ((uintptr_t) data + data[1]);
        if ((uintptr_t) data % 8 != 0) {
            data = (uint32_t *) (((uintptr_t) data & ~0x7) + 8);
        }
    }

    mark_used(0, 0x100000); // assume none of this area is available for general purpose allocations, as device drivers might need it.
    mark_used(KERNEL_PHYS_START, KERNEL_PHYS_END - KERNEL_PHYS_START);
    mark_used(initrd_phys_start, initrd_phys_end - initrd_phys_start);

    debug_log("Finished Initializing Page Frame Allocator\n");
    debug_log("Max phys memory: [ %#lX ]\n", phys_memory_max);
    debug_log("Total available memory: [ %#lX ]\n", phys_memory_total);
    debug_log("Kernel physical memory: [ %#.16lX, %#.16lX, %#.16lX ]\n", KERNEL_PHYS_START, KERNEL_PHYS_END,
              KERNEL_PHYS_END - KERNEL_PHYS_START);
    debug_log("Initrd physical memory: [ %#.16lX, %#.16lX, %#.16lX ]\n", initrd_phys_start, initrd_phys_end,
              initrd_phys_end - initrd_phys_start);
}
