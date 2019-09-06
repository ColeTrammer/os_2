#include <kernel/hal/timer.h>
#include <kernel/hal/x86_64/drivers/pit.h>
#include <kernel/arch/x86_64/proc/process.h>

void register_callback(void (*callback)(), unsigned int ms) {
    pit_register_callback(callback, ms);
}

void set_sched_callback(void (*callback)(struct process_state*), unsigned int ms) {
    pit_set_sched_callback(callback, ms);
}