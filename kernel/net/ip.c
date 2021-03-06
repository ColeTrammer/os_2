#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include <kernel/hal/output.h>
#include <kernel/mem/page.h>
#include <kernel/mem/vm_allocator.h>
#include <kernel/net/destination_cache.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/icmp.h>
#include <kernel/net/interface.h>
#include <kernel/net/ip.h>
#include <kernel/net/neighbor_cache.h>
#include <kernel/net/network_task.h>
#include <kernel/net/packet.h>
#include <kernel/net/socket.h>
#include <kernel/net/tcp.h>
#include <kernel/net/udp.h>
#include <kernel/time/clock.h>
#include <kernel/time/timer.h>
#include <kernel/util/checksum.h>
#include <kernel/util/hash_map.h>
#include <kernel/util/init.h>

// #define IP_V4_DEBUG
#define IP_V4_FRAGMENT_DEBUG

struct ip_v4_fragment_hole {
    uint16_t first;
    uint16_t last;
    uint16_t next;
};

_Static_assert(sizeof(struct ip_v4_fragment_hole) <= 8);

struct ip_v4_fragment_id {
    struct ip_v4_address source;
    struct ip_v4_address dest;
    uint16_t identification;
    uint8_t protocol;
};

struct ip_v4_fragment_desc {
    struct hash_entry hash;
    struct timespec created;
    struct vm_region *reassembly_buffer;
    struct ip_v4_fragment_id id;
    uint16_t hole_list;
};

#define GET_HOLE(buffer, offset)      ((offset) != 0 ? ((struct ip_v4_fragment_hole *) ((buffer) + (offset))) : NULL)
#define GET_HOLE_NUMBER(buffer, hole) ((uintptr_t)(hole) - (uintptr_t)(buffer))
#define MAKE_HOLE(buffer, new_first, new_last, new_next)                                                             \
    ({                                                                                                               \
        uint16_t __new_hole_first = (new_first);                                                                     \
        struct ip_v4_fragment_hole *__new_hole = GET_HOLE((buffer), sizeof(struct ip_v4_packet) + __new_hole_first); \
        __new_hole->first = __new_hole_first;                                                                        \
        __new_hole->last = (new_last);                                                                               \
        __new_hole->next = (new_next);                                                                               \
        __new_hole;                                                                                                  \
    })

static struct hash_map *fragment_store;
static struct timer *fragment_gc_timer;
static struct timespec fragment_max_lifetime = { .tv_sec = 60, .tv_nsec = 0 };

static unsigned int fragment_desc_hash(void *id, int num_buckets) {
    unsigned int sum = 0;
    for (size_t i = 0; i < sizeof(struct ip_v4_fragment_id); i++) {
        sum += ((uint8_t *) id)[i];
    }
    return sum % num_buckets;
}

static int fragment_desc_equals(void *i1, void *i2) {
    return memcmp(i1, i2, sizeof(struct ip_v4_fragment_id)) == 0;
}

static void *fragment_desc_key(struct hash_entry *m) {
    return &hash_table_entry(m, struct ip_v4_fragment_desc)->id;
}

static void free_ip_v4_fragment_desc(struct ip_v4_fragment_desc *desc) {
    if (desc->reassembly_buffer) {
        vm_free_kernel_region(desc->reassembly_buffer);
    }
    free(desc);
}

static void remove_ip_v4_fragment_desc(struct ip_v4_fragment_desc *desc) {
    hash_del(fragment_store, &desc->id);
    free_ip_v4_fragment_desc(desc);
}

static void gc_fragment(struct hash_entry *entry, void *_now) {
    struct ip_v4_fragment_desc *desc = hash_table_entry(entry, struct ip_v4_fragment_desc);
    struct timespec *now = _now;

    struct timespec delta = time_sub(*now, desc->created);
    if (time_compare(delta, fragment_max_lifetime) >= 0) {
        __hash_del(fragment_store, &desc->id);
        free_ip_v4_fragment_desc(desc);
    }
}

static void gc_fragments(struct timer *timer, void *closure __attribute__((unused))) {
    struct timespec now = time_read_clock(CLOCK_MONOTONIC);
    hash_for_each(fragment_store, gc_fragment, &now);
    __time_reset_kernel_callback(timer, &fragment_max_lifetime);
}

static struct hash_entry *create_fragment_desc(void *_id) {
    struct ip_v4_fragment_id *id = _id;
    struct ip_v4_fragment_desc *desc = malloc(sizeof(struct ip_v4_fragment_desc));
    desc->created = time_read_clock(CLOCK_MONOTONIC);
    desc->id = *id;
    desc->reassembly_buffer = vm_allocate_kernel_region(PAGE_SIZE);

    struct ip_v4_fragment_hole *hole = MAKE_HOLE(desc->reassembly_buffer->start, 0, IP_V4_MAX_PACKET_LENGTH, 0);
    desc->hole_list = GET_HOLE_NUMBER(desc->reassembly_buffer->start, hole);
    return &desc->hash;
}

void handle_fragment(struct network_interface *interface, const struct ip_v4_packet *packet) {
    struct ip_v4_fragment_id fragment_id = {
        .source = packet->source,
        .dest = packet->destination,
        .identification = htons(packet->identification),
        .protocol = packet->protocol,
    };
    struct ip_v4_fragment_desc *desc =
        hash_table_entry(hash_put_if_not_present(fragment_store, &fragment_id, create_fragment_desc), struct ip_v4_fragment_desc);

    uint16_t fragment_first = IP_V4_FRAGMENT_OFFSET(packet);
    uint16_t fragment_last = fragment_first + htons(packet->length) - sizeof(struct ip_v4_packet) - 1;
    bool first_fragment = fragment_first == 0;
    bool last_fragment = packet->more_fragments == 0;

#ifdef IP_V4_FRAGMENT_DEBUG
    debug_log("Got fragment: [ %p, %u, %u, %u, %u ]\n", desc, fragment_first, fragment_last, first_fragment, last_fragment);
#endif /* IP_V4_FRAGMENT_DEBUG */

    uint16_t fragment_length = fragment_last - fragment_first + 1;
    if (!last_fragment && fragment_length % 8 != 0) {
        debug_log("Fragment length is not a multiple of 8 octets\n");
        remove_ip_v4_fragment_desc(desc);
        return;
    }

    size_t current_packet_length = fragment_last + 1 + sizeof(struct ip_v4_packet);
    if (current_packet_length > IP_V4_MAX_PACKET_LENGTH) {
        debug_log("Fragment is way too big");
        remove_ip_v4_fragment_desc(desc);
        return;
    }

    // The reassambly buffer needs at least enough space to hold data until the end of the current fragment,
    // plus space to store the hold descriptor at the end of the packet.
    if (current_packet_length + (last_fragment ? 0 : sizeof(struct ip_v4_fragment_hole)) >
        desc->reassembly_buffer->end - desc->reassembly_buffer->start) {
        desc->reassembly_buffer = vm_reallocate_kernel_region(desc->reassembly_buffer, ALIGN_UP(current_packet_length, PAGE_SIZE));
    }

    void *reassembly_buffer = (void *) desc->reassembly_buffer->start;
    struct ip_v4_packet *reassembled_packet = reassembly_buffer;

    // The first fragment fills in all of the header fields except for the length, while the length is only known
    // once the last fragment is recieved.
    if (first_fragment) {
        uint16_t length_save = reassembled_packet->length;
        memcpy(reassembled_packet, packet, sizeof(struct ip_v4_packet));
        reassembled_packet->length = length_save;
        reassembled_packet->more_fragments = 0;
    } else if (last_fragment) {
        reassembled_packet->length = ntohs(current_packet_length);
    }

    struct ip_v4_fragment_hole *prev_hole = (void *) &desc->hole_list - offsetof(struct ip_v4_fragment_hole, next);
    struct ip_v4_fragment_hole *hole = GET_HOLE(reassembly_buffer, prev_hole->next);
    for (; hole; prev_hole = hole, hole = GET_HOLE(reassembly_buffer, hole->next)) {
        uint16_t hole_first = hole->first;
        uint16_t hole_last = hole->last;
        if (fragment_first > hole_last || fragment_last < hole_first) {
            continue;
        }

        // Remove the old hole, since this fragment at least partially fills it in.
        prev_hole->next = hole->next;
        hole = prev_hole;

        // Add a new hole if there's a gap between the hole's left and the fragment's start.
        if (fragment_first > hole_first) {
            struct ip_v4_fragment_hole *new_hole = MAKE_HOLE(reassembly_buffer, hole_first, fragment_first - 1, hole->next);
            hole->next = GET_HOLE_NUMBER(reassembly_buffer, new_hole);
            hole = new_hole;
        }

        // Add a new hole if there's a gap between the fragment's end and the hole's end.
        if (fragment_last < hole_last && !last_fragment) {
            struct ip_v4_fragment_hole *new_hole = MAKE_HOLE(reassembly_buffer, fragment_last + 1, hole_last, hole->next);
            hole->next = GET_HOLE_NUMBER(reassembly_buffer, new_hole);
            hole = new_hole;
        }
    }

    // Fill in the reassembly buffer with the fragment data, now that there cannot be any hole data stored there.
    memcpy(reassembly_buffer + sizeof(struct ip_v4_packet) + fragment_first, packet->payload, fragment_length);

    if (desc->hole_list == 0) {
#ifdef IP_V4_FRAGMENT_DEBUG
        debug_log("Successfully reassembled packet: [ %p ]\n", desc);
#endif /* IP_V4_FRAGMENT_DEBUG */
        struct packet fake_packet = {
            .interface = interface,
            .flags = PKT_DONT_FREE,
            .header_count = 1,
            .total_length = htons(reassembled_packet->length),
        };
        net_init_packet_header(&fake_packet, 0, PH_IP_V4, reassembled_packet, fake_packet.total_length);

        net_ip_v4_recieve(&fake_packet);
        remove_ip_v4_fragment_desc(desc);
    }
}

uint8_t net_packet_header_to_ip_v4_type(enum packet_header_type type) {
    switch (type) {
        case PH_ICMP:
            return IP_V4_PROTOCOL_ICMP;
        case PH_UDP:
            return IP_V4_PROTOCOL_UDP;
        case PH_TCP:
            return IP_V4_PROTOCOL_TCP;
        default:
            return 0;
    }
}

enum packet_header_type net_inet_protocol_to_packet_header_type(uint8_t protocol) {
    switch (protocol) {
        case IPPROTO_ICMP:
            return PH_ICMP;
        case IPPROTO_UDP:
            return PH_UDP;
        case IPPROTO_TCP:
            return PH_TCP;
        default:
            return PH_RAW_DATA;
    }
}

static int net_interface_send_ip_v4_fragmented(struct network_interface *self, struct link_layer_address ll_dest,
                                               struct packet *net_packet) {
    int ret = 0;
    struct packet packet = *net_packet;
    packet.flags |= PKT_DONT_FREE;

    struct packet_header *outer_header = net_packet_outer_header(&packet);

    struct ip_v4_packet ip_packet;
    net_init_ip_v4_packet(&ip_packet, packet.destination->next_packet_id++, net_packet_header_to_ip_v4_type(outer_header->type),
                          self->address, packet.destination->destination_path.dest_ip_address, NULL, packet.total_length);
    net_init_packet_header(&packet, net_packet_header_index(&packet, outer_header) - 1, PH_IP_V4, &ip_packet, sizeof(struct ip_v4_packet));

    uint16_t total_length = packet.total_length - sizeof(struct ip_v4_packet);
    uint16_t offset = 0;
    uint16_t data_mtu = ALIGN_DOWN(self->mtu - sizeof(struct ip_v4_packet), 8);
    uint16_t header_index = net_packet_header_index(&packet, outer_header);
    for (;;) {
        uint16_t old_header_count = packet.header_count;
        uint16_t old_header_index = header_index;
        uint16_t old_header_length = 0;
        uint16_t data_advanced = 0;

        // Figure out the last header that will be part of the message.
        for (; header_index < packet.header_count; header_index++) {
            struct packet_header *header = &packet.headers[header_index];
            if (data_advanced + header->length > data_mtu) {
                // This header needs to be fragmented. Adjust its length so that it will be partially sent,
                // and modify the packet so that any later headers won't be sent.
                old_header_length = header->length;
                header->length = data_mtu - data_advanced;
                packet.header_count = header_index + 1;
                data_advanced = data_mtu;
                break;
            }
            data_advanced += header->length;
        }

        // Adjust the ip header to match the fragment.
        ip_packet.more_fragments = data_advanced + offset < total_length;
        ip_packet.length = htons(sizeof(struct ip_v4_packet) + data_advanced);
        SET_IP_V4_FRAGMENT_OFFSET(&ip_packet, offset);
        ip_packet.checksum = 0;
        ip_packet.checksum = htons(compute_internet_checksum(&ip_packet, sizeof(struct ip_v4_packet)));

        struct packet copy = packet;
        if ((ret = self->ops->send(self, ll_dest, &copy))) {
            break;
        }

        offset += data_advanced;
        if (offset >= total_length) {
            break;
        }

        // Slice the fragmented header so that its raw data is advanced by the amount sent, and
        // its length is appropriately updated.
        struct packet_header *header_to_fragment = &packet.headers[header_index];
        header_to_fragment->raw_header += header_to_fragment->length;
        header_to_fragment->length = old_header_length - header_to_fragment->length;

        // Remove any header's that were completely sent in the last fragment.
        if (old_header_index != header_index) {
            memmove(&packet.headers[old_header_index], &packet.headers[header_index],
                    (old_header_count - header_index + 1) * sizeof(struct packet_header));
            packet.header_count = old_header_count - (header_index - old_header_index);
            header_index = old_header_index;
        }
    }

    net_free_packet(net_packet);
    return ret;
}

int net_interface_send_ip_v4(struct network_interface *self, struct link_layer_address ll_dest, struct packet *packet) {
    if (sizeof(struct ip_v4_packet) + packet->total_length > self->mtu) {
        return net_interface_send_ip_v4_fragmented(self, ll_dest, packet);
    }

    struct destination_cache_entry *destination = packet->destination;
    struct packet_header *outer_header = net_packet_outer_header(packet);

    struct ip_v4_packet ip_packet;
    net_init_ip_v4_packet(&ip_packet, destination->next_packet_id++, net_packet_header_to_ip_v4_type(outer_header->type), self->address,
                          destination->destination_path.dest_ip_address, NULL, packet->total_length);
    net_init_packet_header(packet, net_packet_header_index(packet, outer_header) - 1, PH_IP_V4, &ip_packet, sizeof(struct ip_v4_packet));

    return self->ops->send(self, ll_dest, packet);
}

int net_interface_route_ip_v4(struct network_interface *self, struct packet *packet) {
    (void) self;
    return net_queue_packet_for_neighbor(packet->destination->next_hop, packet);
}

int net_send_ip_v4(struct socket *socket, struct network_interface *interface, uint8_t protocol, struct ip_v4_address dest, const void *buf,
                   size_t len) {
    struct destination_cache_entry *destination = net_lookup_destination(interface, dest);

    struct ip_v4_address d = dest;
    debug_log("Sending raw IPV4 to: [ %u.%u.%u.%u ]\n", d.addr[0], d.addr[1], d.addr[2], d.addr[3]);

    struct packet *packet = net_create_packet(interface, socket, destination, len);
    packet->header_count = interface->link_layer_overhead + 2;

    net_drop_destination_cache_entry(destination);

    struct packet_header *raw_data = net_init_packet_header(packet, interface->link_layer_overhead + 1,
                                                            net_inet_protocol_to_packet_header_type(protocol), packet->inline_data, len);
    memcpy(raw_data->raw_header, buf, len);

    return interface->ops->route_ip_v4(interface, packet);
}

void net_ip_v4_recieve(struct packet *packet) {
    struct packet_header *header = net_packet_inner_header(packet);
    if (header->length < sizeof(struct ip_v4_packet)) {
        debug_log("IP V4 packet too small\n");
        return;
    }

    struct ip_v4_packet *ip_packet = header->raw_header;

#ifdef IP_V4_DEBUG
    net_ip_v4_log(ip_packet);
#endif /* IP_V4_DEBUG */

    if (ip_packet->more_fragments || IP_V4_FRAGMENT_OFFSET(ip_packet) > 0) {
        handle_fragment(packet->interface, ip_packet);
        return;
    }

    struct packet_header *next_header = net_packet_add_header(packet, sizeof(struct ip_v4_packet));
    switch (ip_packet->protocol) {
        case IP_V4_PROTOCOL_ICMP: {
            next_header->type = PH_ICMP;
            net_icmp_recieve(packet);
            return;
        }
        case IP_V4_PROTOCOL_UDP: {
            next_header->type = PH_UDP;
            net_udp_recieve(packet);
            return;
        }
        case IP_V4_PROTOCOL_TCP: {
            next_header->type = PH_TCP;
            net_tcp_recieve(packet);
            return;
        }
        default: {
            break;
        }
    }

    debug_log("Ignored packet\n");
}

void net_init_ip_v4_packet(struct ip_v4_packet *packet, uint16_t ident, uint8_t protocol, struct ip_v4_address source,
                           struct ip_v4_address dest, const void *payload, uint16_t payload_length) {
    packet->version = IP_V4_VERSION;
    packet->ihl = IP_V4_BYTES_TO_WORDS(sizeof(struct ip_v4_packet));
    packet->dscp = 0;
    packet->ecn = 0;
    packet->length = htons(sizeof(struct ip_v4_packet) + payload_length);
    packet->identification = htons(ident);
    packet->reserved_flag = 0;
    packet->dont_fragment = 0;
    packet->more_fragments = 0;
    packet->fragment_offset_low = 0;
    packet->fragment_offset_high = 0;
    packet->ttl = 64;
    packet->protocol = protocol;
    packet->source = source;
    packet->destination = dest;
    packet->checksum = 0;
    packet->checksum = htons(compute_internet_checksum(packet, sizeof(struct ip_v4_packet)));
    if (payload) {
        memcpy(packet->payload, payload, payload_length);
    }
}

void net_ip_v4_log(const struct ip_v4_packet *ip_packet) {
    debug_log("IP v4 Packet:\n"
              "               Header Len   [ %15u ]   Version   [ %15u ]\n"
              "               DSCP         [ %15u ]   ECN       [ %15u ]\n"
              "               Length       [ %15u ]   ID        [ %15u ]\n"
              "               Flags        [ RSZ=%u DF=%u MF=%u ]   Frag Off  [ %15u ]\n"
              "               TTL          [ %15u ]   Protocol  [ %15u ]\n"
              "               Source IP    [ %03u.%03u.%03u.%03u ]   Dest IP   [ %03u.%03u.%03u.%03u ]\n"
              "               Data Len     [ %15u ]   Data off  [ %15u ]\n",
              ip_packet->ihl, ip_packet->version, ip_packet->dscp, ip_packet->ecn, ntohs(ip_packet->length),
              ntohs(ip_packet->identification), ip_packet->reserved_flag, ip_packet->dont_fragment, ip_packet->more_fragments,
              IP_V4_FRAGMENT_OFFSET(ip_packet), ip_packet->ttl, ip_packet->protocol, ip_packet->source.addr[0], ip_packet->source.addr[1],
              ip_packet->source.addr[2], ip_packet->source.addr[3], ip_packet->destination.addr[0], ip_packet->destination.addr[1],
              ip_packet->destination.addr[2], ip_packet->destination.addr[3],
              ntohs(ip_packet->length) - ip_packet->ihl * (uint32_t) sizeof(uint32_t), ip_packet->ihl * (uint32_t) sizeof(uint32_t));
}

static void init_ip_v4(void) {
    fragment_store = hash_create_hash_map(fragment_desc_hash, fragment_desc_equals, fragment_desc_key);
    fragment_gc_timer = time_register_kernel_callback(&fragment_max_lifetime, gc_fragments, NULL);
}
INIT_FUNCTION(init_ip_v4, net);
