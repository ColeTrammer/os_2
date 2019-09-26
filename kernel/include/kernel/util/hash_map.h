#ifndef _KERNEL_UTIL_HASH_MAP_H
#define _KERNEL_UTIL_HASH_MAP_H 1

#include <kernel/util/spinlock.h>

#define HASH_DEFAULT_NUM_BUCKETS 25

struct hash_entry {
    void *key;
    void *data;
    struct hash_entry *next;
};

struct hash_map {
    int (*hash)(void *ptr, int hash_size);
    int (*equals)(void *ptr, void *id);
    void *(*key)(void *ptr);
    spinlock_t lock;
    struct hash_entry *entries[HASH_DEFAULT_NUM_BUCKETS];
};

struct hash_map *hash_create_hash_map(int (*hash)(void *ptr, int hash_size), int (*equals)(void *ptr, void *id), void *(*key)(void *ptr));
void *hash_get(struct hash_map *map, void *key);
void hash_put(struct hash_map *map, void *ptr);
void hash_set(struct hash_map *map, void *ptr);
void hash_del(struct hash_map *map, void *key);
void hash_free_hash_map(struct hash_map *map);

#endif /* _KERNEL_UTIL_HASH_MAP_H */