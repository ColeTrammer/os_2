#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

int main() {
    pthread_t id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(
        &id, &attr,
        [](void*) -> void* {
            write(1, "ABC\n", 4);
            return nullptr;
        },
        nullptr);

    assert(pthread_join(id, nullptr) == EINVAL);

    sleep(2);
    return 0;
}
