#ifndef _KERNEL_NET_NEIGHBOR_CACHE_H
#define _KERNEL_NET_NEIGHBOR_CACHE_H 1

#include <kernel/net/ip_address.h>
#include <kernel/net/link_layer_address.h>
#include <kernel/util/hash_map.h>
#include <kernel/util/list.h>
#include <kernel/util/spinlock.h>

struct network_interface;
struct packet;
struct timer;

enum neighbor_status {
    NS_INCOMPLETE,
    NS_REACHABLE,
    NS_UNREACHABLE,
};

struct neighbor_cache_key {
    struct network_interface *interface;
    struct ip_v4_address ip_v4_address;
};

struct neighbor_cache_entry {
    struct list_node queued_packets;
    struct hash_entry hash;
    struct timer *update_timer;
    struct neighbor_cache_key key;
    int ref_count;
    spinlock_t lock;
    enum neighbor_status state;
    struct link_layer_address link_layer_address;
};

struct neighbor_cache_entry *net_bump_neighbor_cache_entry(struct neighbor_cache_entry *neighbor);
void net_drop_neighbor_cache_entry(struct neighbor_cache_entry *neighbor);

int net_queue_packet_for_neighbor(struct neighbor_cache_entry *neighbor, struct packet *packet);
struct neighbor_cache_entry *net_lookup_neighbor(struct network_interface *interface, struct ip_v4_address address);
void net_update_neighbor(struct neighbor_cache_entry *neighbor, struct link_layer_address confirmed_address);
void net_remove_neighbor(struct neighbor_cache_entry *neighbor);

struct hash_map *net_neighbor_cache(void);

#endif /* _KERNEL_NET_NEIGHBOR_CACHE_H */
