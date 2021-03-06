#include <kernel/mem/page.h>

// Physical addresses are already mapped in at VIRT_ADDR(MAX_PML4_ENTRIES - 3)
void *create_phys_addr_mapping(uintptr_t phys_addr) {
    return (void *) (phys_addr + PHYS_ID_START);
}

void *create_phys_addr_mapping_from_virt_addr(void *virt_addr) {
    if (virt_addr == NULL) {
        return NULL;
    }

    return create_phys_addr_mapping(get_phys_addr((uintptr_t) virt_addr));
}
