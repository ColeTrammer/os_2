#ifndef __pthread_attr_t_defined
#define __pthread_attr_t_defined 1

#include <bits/__sched_param.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    int __flags;
    void *__stack_start;
    unsigned long __stack_len;
    unsigned long __guard_size;
    struct __sched_param __sched_param;
} pthread_attr_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __pthread_attr_t_defined */
