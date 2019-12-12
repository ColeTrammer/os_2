#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <sys/os_2.h>
#include <sys/syscall.h>

int pthread_cond_broadcast(pthread_cond_t *cond) {
    return syscall(SC_OS_MUTEX, &cond->__lock, MUTEX_WAKE_AND_SET, 0, 0, INT_MAX, NULL);
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    if (cond == NULL || cond->__lock == -1) {
        return EINVAL;
    }

    cond->__lock = -1;
    return 0;
}

int pthread_cond_init(pthread_cond_t *__restrict cond, const pthread_condattr_t *__restrict attr) {
    if (cond == NULL) {
        return EINVAL;
    }

    cond->__lock = 0;
    if (attr != NULL) {
        memcpy(&cond->__attr, attr, sizeof(pthread_condattr_t));
    } else {
        pthread_condattr_init(&cond->__attr);
    }

    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    return syscall(SC_OS_MUTEX, &cond->__lock, MUTEX_WAKE_AND_SET, 0, 0, 1, NULL);
}

static void cond_wait_cleanup(pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

int pthread_cond_wait(pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex) {
    pthread_cleanup_push((void (*)(void *)) cond_wait_cleanup, mutex);
    syscall(SC_OS_MUTEX, &mutex->__lock, MUTEX_RELEASE_AND_WAIT, 0, 0, 1, &cond->__lock);
    pthread_cleanup_pop(1);
    return 0;
}