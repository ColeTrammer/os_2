#include <kernel/hal/hal.h>
#include <kernel/hal/output.h>
#include <kernel/hal/processor.h>
#include <kernel/proc/task.h>
#include <kernel/util/spinlock.h>

// #define SPINLOCK_DEBUG

bool g_should_log = true;

#define __spinlock_log(s, ...)         \
    do {                               \
        if (g_should_log) {            \
            debug_log(s, __VA_ARGS__); \
        }                              \
    } while (0)

void init_spinlock_internal(spinlock_t *lock, const char *func __attribute__((unused))) {
#ifdef SPINLOCK_DEBUG
    __spinlock_log("~initalizing spinlock: [ %p, %s ]\n", lock, func);
#endif /* SPINLOCK_DEBUG */

    lock->counter = 0;
    lock->interrupts = 0;
}

void spin_lock_internal(spinlock_t *lock, const char *func __attribute__((unused)), bool handle_messages) {
#ifdef SPINLOCK_DEBUG
    __spinlock_log("~locking spinlock: [ %p, %s ]\n", lock, func);
#endif /* SPINLOCK_DEBUG */

    while (1) {
        unsigned long interrupts = disable_interrupts_save();
        if (!xchg_32(&lock->counter, 1)) {
            lock->interrupts = interrupts;
            return;
        }

#ifdef SPINLOCK_DEBUG
        __spinlock_log("faild to aquire lock: [ %p, %s ]\n", lock, func);
#endif /* SPINLOCK_DEBUG */

        while (lock->counter) {
            if (handle_messages) {
                handle_processor_messages();
            }
            cpu_relax();
        }
    }
}

bool spin_trylock(spinlock_t *lock) {
    unsigned long interrupts = disable_interrupts_save();
    if (!xchg_32(&lock->counter, 1)) {
        lock->interrupts = interrupts;
        return true;
    }

    interrupts_restore(interrupts);
    return false;
}

void spin_unlock_internal(spinlock_t *lock, const char *func __attribute__((unused)), bool no_irq_restore) {
#ifdef SPINLOCK_DEBUG
    __spinlock_log("~unlocking spinlock: [ %p, %s ]\n", lock, func);
#endif /* SPINLOCK_DEBUG */

    unsigned long interrupts = lock->interrupts;
    barrier();
    lock->counter = 0;
    if (!no_irq_restore) {
        interrupts_restore(interrupts);
    }
}
