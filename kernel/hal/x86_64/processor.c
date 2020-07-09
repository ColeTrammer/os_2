#include <kernel/arch/x86_64/asm_utils.h>
#include <kernel/hal/processor.h>
#include <kernel/hal/x86_64/drivers/local_apic.h>
#include <kernel/mem/page.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/proc/task.h>

// #define PROCESSOR_IPI_DEBUG

static bool s_bsp_enabled;

bool bsp_enabled(void) {
    return s_bsp_enabled;
}

void arch_init_processor(struct processor *processor) {
    set_msr(MSR_GS_BASE, 0);
    set_msr(MSR_KERNEL_GS_BASE, (uintptr_t) processor);
    swapgs();
    init_local_apic();
    init_gdt(processor);
    init_idle_task(processor);
}

void init_bsp(struct processor *processor) {
    processor->kernel_stack = vm_allocate_kernel_region(PAGE_SIZE);
    arch_init_processor(processor);

    processor->enabled = true;
    s_bsp_enabled = true;
}

void arch_broadcast_panic(void) {
    local_apic_broadcast_ipi(LOCAL_APIC_PANIC_IRQ);
}

void arch_broadcast_ipi(void) {
    local_apic_broadcast_ipi(LOCAL_APIC_IPI_IRQ);
}

void handle_processor_messages(void) {
    struct processor *processor = get_current_processor();
    assert(processor);

    spin_lock(&processor->ipi_messages_lock);
    struct processor_ipi_message *message = processor->ipi_messages_head;
    while (message) {
        switch (message->type) {
            case PROCESSOR_IPI_FLUSH_TLB:
#ifdef PROCESSOR_IPI_DEBUG
                if (processor->id == 0) {
                    debug_log("Flushing TLB: [ %d, %#.16lX, %lu ]\n", processor->id, message->flush_tlb.base, message->flush_tlb.pages);
                }
#endif /* PROCESSOR_IPI_DEBUG */
                for (size_t i = 0; i < message->flush_tlb.pages; i++) {
                    invlpg(message->flush_tlb.base + i * PAGE_SIZE);
                }
                break;
            default:
                assert(false);
        }

        struct processor_ipi_message *next = message->next;
        drop_processor_ipi_message(message);
        message = next;
    }
    processor->ipi_messages_head = processor->ipi_messages_tail = NULL;
    spin_unlock(&processor->ipi_messages_lock);
}
