#include <stdint.h>
#include <string.h>

#include <kernel/hal/hal.h>
#include <kernel/hal/output.h>
#include <kernel/hal/processor.h>
#include <kernel/hal/x86_64/gdt.h>
#include <kernel/mem/kernel_vm.h>
#include <kernel/proc/task.h>

extern void syscall_sysret_entry(void);

static void syscall_compat(void) {
    // FIXME: properly kill the process, or actually support 32 bit processes.
    assert(false);
}

void init_gdt(struct processor *processor) {
    memset(processor->arch_processor.gdt, 0, GDT_ENTRIES * sizeof(struct gdt_entry));
    processor->arch_processor.gdt[CS_OFFSET].type = 0b10011010;
    processor->arch_processor.gdt[CS_OFFSET].flags = 0b0010;

    processor->arch_processor.gdt[DATA_OFFSET].type = 0b10010010;

    processor->arch_processor.gdt[USER_CODE32_OFFSET].type = 0b11111010;

    processor->arch_processor.gdt[USER_CODE_OFFSET].type = 0b11111010;
    processor->arch_processor.gdt[USER_CODE_OFFSET].flags = 0b0010; /* Long mode bit */

    processor->arch_processor.gdt[USER_DATA_OFFSET].type = 0b11110010;

    struct gdt_tss_entry *tss_entry = (struct gdt_tss_entry *) (processor->arch_processor.gdt + GDT_TSS_OFFSET);
    tss_entry->type = TSS_TYPE;
    tss_entry->limit_low = sizeof(struct tss);
    tss_entry->base_low = ((uintptr_t) &processor->arch_processor.tss) & 0x000000000000FFFF;
    tss_entry->base_low_mid = (((uintptr_t) &processor->arch_processor.tss) & 0x0000000000FF0000) >> 16;
    tss_entry->base_high_mid = (((uintptr_t) &processor->arch_processor.tss) & 0x00000000FF000000) >> 24;
    tss_entry->base_high = (((uintptr_t) &processor->arch_processor.tss) & 0xFFFFFFFF00000000) >> 32;

    processor->arch_processor.gdt_descriptor.limit = GDT_ENTRIES * sizeof(struct gdt_entry) - 1;
    processor->arch_processor.gdt_descriptor.gdt = processor->arch_processor.gdt;
    load_gdt(processor->arch_processor.gdt_descriptor);

    memset(&processor->arch_processor.tss, 0, sizeof(struct tss));
    processor->arch_processor.tss.ist[0] = processor->kernel_stack->end;
    processor->arch_processor.tss.io_map_base = sizeof(struct tss);
    load_tr(TSS_SELECTOR);

    // Setup MSR's to support syscall/sysret
    set_msr(MSR_STAR, (USER_CODE32_SELECTOR << 48UL) | (CS_SELECTOR << 32UL));
    set_msr(MSR_LSTAR, (uintptr_t) &syscall_sysret_entry);
    set_msr(MSR_CSTAR, (uintptr_t) &syscall_compat);
    set_msr(MSR_SFMASK, INTERRUPTS_ENABLED_FLAG);
}

/* Must be called from unpremptable context */
void set_tss_stack_pointer(uintptr_t rsp) {
    get_current_processor()->arch_processor.tss.rsp0 = rsp;
}
