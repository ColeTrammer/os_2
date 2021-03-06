#ifndef _KERNEL_MEM_KERNEL_VM_H
#define _KERNEL_MEM_KERNEL_VM_H 1

#include <stdint.h>

#include <kernel/mem/page.h>

extern void KERNEL_VMA();

extern void _text_start();
extern void _text_end();
extern void _rod_start();
extern void _rod_end();
extern void _data_start();
extern void _data_end();
extern void _bss_start();
extern void _bss_end();
extern void _kernel_phys_start();
extern void _kernel_phys_end();
extern void _kernel_ap_trampoline_start();
extern void _kernel_ap_trampoline_end();

extern void KERNEL_VM_STACK_START();

#define KERNEL_VM_START ((uintptr_t) &KERNEL_VMA)

#define KERNEL_TEXT_START          ((uintptr_t) &_text_start)
#define KERNEL_TEXT_END            ((uintptr_t) &_text_end)
#define KERNEL_ROD_START           ((uintptr_t) &_rod_start)
#define KERNEL_ROD_END             ((uintptr_t) &_rod_end)
#define KERNEL_DATA_START          ((uintptr_t) &_data_start)
#define KERNEL_DATA_END            ((uintptr_t) &_data_end)
#define KERNEL_BSS_START           ((uintptr_t) &_bss_start)
#define KERNEL_BSS_END             ((uintptr_t) &_bss_end)
#define KERNEL_VM_END              KERNEL_BSS_END
#define KERNEL_PHYS_START          ((uintptr_t) &_kernel_phys_start)
#define KERNEL_PHYS_END            ((uintptr_t) &_kernel_phys_end)
#define KERNEL_AP_TRAMPOLINE_START ((uintptr_t) &_kernel_ap_trampoline_start)
#define KERNEL_AP_TRAMPOLINE_END   ((uintptr_t) &_kernel_ap_trampoline_end)

#define __KERNEL_VM_STACK_START ((uint64_t) &KERNEL_VM_STACK_START)

#endif /* _KERNEL_MEM_KERNEL_VM_H */
