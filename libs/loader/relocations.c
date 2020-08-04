#include <elf.h>

#include "dynamic_elf_object.h"
#include "relocations.h"
#include "symbols.h"

uintptr_t do_got_resolve(const struct dynamic_elf_object *obj, size_t plt_offset) {
    const Elf64_Rela *relocation = plt_relocation_at(obj, plt_offset);
    const char *to_lookup = symbol_name(obj, ELF64_R_SYM(relocation->r_info));
    struct symbol_lookup_result result = do_symbol_lookup(to_lookup, obj, 0);
    if (!result.symbol) {
        loader_log("Cannot resolve `%s' for `%s'", to_lookup, object_name(obj));
        _exit(96);
    }
    uint64_t *addr = (uint64_t *) (obj->relocation_offset + relocation->r_offset);
    uintptr_t resolved_value = result.symbol->st_value + result.object->relocation_offset;
    *addr = resolved_value;

#ifdef LOADER_SYMBOL_DEBUG
    loader_log("Resolved `%s' in `%s' to %#.16lX", to_lookup, object_name(obj), resolved_value);
#endif /* LOADER_SYMBOL_DEBUG */
    return resolved_value;
}

int process_relocations(const struct dynamic_elf_object *self, bool bind_now) {
    for (struct dynamic_elf_object *obj = dynamic_object_tail; obj; obj = obj->prev) {
        if (do_process_relocations(obj, bind_now)) {
            return -1;
        }

        if (obj == self) {
            break;
        }
    }

    return 0;
}
LOADER_HIDDEN_EXPORT(process_relocations, __loader_process_relocations);
