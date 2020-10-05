#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/fs/dev.h>
#include <kernel/fs/disk_sync.h>
#include <kernel/fs/vfs.h>
#include <kernel/hal/hal.h>
#include <kernel/hal/output.h>
#include <kernel/irqs/handlers.h>
#include <kernel/mem/page_frame_allocator.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/net/interface.h>
#include <kernel/proc/elf64.h>
#include <kernel/proc/task.h>
#include <kernel/proc/task_finalizer.h>
#include <kernel/sched/task_sched.h>
#include <kernel/util/init.h>
#include <kernel/util/list.h>

void kernel_main(uint32_t *multiboot_info) {
    init_hal();
    init_irq_handlers();
    init_page_frame_allocator(multiboot_info);
    init_kernel_process();
    init_vm_allocator();
    init_cpus();
    INIT_DO_LEVEL(fs);
    INIT_DO_LEVEL(driver);
    INIT_DO_LEVEL(time);

    /* Mount INITRD as root */
    int error = fs_mount(NULL, "/", "initrd");
    assert(error == 0);

    // FIXME: make procfs_register_process work even when the procfs isn't mounted, so that this can be mounted later (and in userspace).
    error = fs_mount(NULL, "/proc", "procfs");
    assert(error == 0);

    init_task_sched();
    init_task_finalizer();
    INIT_DO_LEVEL(net);
    init_disk_sync_task();

    // Mount tmpfs at /tmp
    error = fs_mount(NULL, "/tmp", "tmpfs");
    assert(error == 0);

    /* Mount sda1 at / */
    struct device *sda1 = dev_get_device(0x00501);
    assert(sda1);
    error = 0;
    error = fs_mount(sda1, "/", "ext2");
    assert(error == 0);
    dev_drop_device(sda1);

    // Mount tmpfs at /dev/shm
    error = fs_mount(NULL, "/dev/shm", "tmpfs");
    assert(error == 0);

    // NOTE: if we put these symbols on the initrd instead of in /boot/os_2.o, thse symbols
    //       could be loaded sooner
    init_kernel_symbols();

    init_userland();

    init_smp();
    sched_run_next();

    while (1)
        ;
}
