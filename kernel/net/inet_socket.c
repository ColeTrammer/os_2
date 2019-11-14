#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/net/ethernet.h>
#include <kernel/net/inet_socket.h>
#include <kernel/net/interface.h>
#include <kernel/net/ip.h>
#include <kernel/net/port.h>
#include <kernel/net/socket.h>
#include <kernel/net/tcp.h>
#include <kernel/net/udp.h>
#include <kernel/sched/process_sched.h>
#include <kernel/util/hash_map.h>

static struct hash_map *map;

static int ip_v4_and_port_hash(void *i, int num_buckets) {
    struct ip_v4_and_port *a = i;
    return (a->ip.addr[0] + a->ip.addr[1] + a->ip.addr[2] + a->ip.addr[2] + a->port) % num_buckets;
}

static int ip_v4_and_port_equals(void *i1, void *i2) {
    return memcmp(i1, i2, sizeof(struct ip_v4_and_port)) == 0;
}

static void *ip_v4_and_port_key(void *m) {
    return &((struct tcp_socket_mapping*) m)->key;
}

static int tcp_data_hash(void *k, int num_buckets) {
    return *((uint32_t*) k) % num_buckets;
}

static int tcp_data_equals(void *i1, void *i2) {
    return *((uint32_t*) i1) == *((uint32_t*) i2);
}

static void *tcp_data_key(void *tcp_data) {
    return &((struct tcp_packet_data*) tcp_data)->sequence_number;
}

static void create_tcp_socket_mapping(struct socket *socket) {
    assert(socket->private_data);

    struct inet_socket_data *data = socket->private_data;
    struct tcp_socket_mapping *mapping = calloc(1, sizeof(struct tcp_socket_mapping));
    mapping->key = (struct ip_v4_and_port) { data->dest_port, data->dest_ip };
    mapping->socket_id = socket->id;

    hash_put(map, mapping);
}

struct socket_data *net_inet_create_socket_data(const struct ip_v4_packet *packet, uint16_t port_network_ordered, const void *buf, size_t len) {
    struct socket_data *data = calloc(1, sizeof(struct socket_data) + len);
    assert(data);

    data->len = len;
    memcpy(data->data, buf, len);
    data->from.addrlen = sizeof(struct sockaddr_in);
    data->from.addr.in.sin_family = AF_INET;
    data->from.addr.in.sin_port = port_network_ordered;
    data->from.addr.in.sin_addr.s_addr = ip_v4_to_uint(packet->source);

    return data;
}

int net_inet_bind(struct socket *socket, const struct sockaddr_in *addr, socklen_t addrlen) {
    assert(socket);

    if (addr->sin_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    struct inet_socket_data *data = calloc(1, sizeof(struct inet_socket_data));
    socket->private_data = data;

    if (addr->sin_port == 0) {
        int ret = net_bind_to_ephemeral_port(socket->id, &data->source_port);
        if (ret < 0) {
            return ret;
        }
    } else {
        // TODO: bind to a specified port
        return -EADDRINUSE;
    }

    socket->state = BOUND;
    return 0;
}

static void __kill_tcp_data(void *a, void *i) { 
    (void) i; 
    free(a);
}

int net_inet_close(struct socket *socket) {
    assert(socket);

    struct inet_socket_data *data = socket->private_data;
    if (data) {
        net_unbind_port(data->source_port);
        if (socket->protocol == IPPROTO_TCP) {
            struct ip_v4_and_port key = { data->dest_port, data->dest_ip };
            free(hash_del(map, &key));

            assert(data->tcb);
            assert(data->tcb->sent_packets);

            if (socket->state != CLOSED) {
                struct network_interface *interface = net_get_interface_for_ip(data->dest_ip);
                net_send_tcp(interface, data->dest_ip, data->source_port, data->dest_port,
                    data->tcb->current_sequence_num, data->tcb->current_ack_num, 
                    (union tcp_flags) { .bits.fin=1, .bits.ack=data->tcb->should_send_ack }, 0, NULL);
            }

            hash_for_each(data->tcb->sent_packets, __kill_tcp_data, NULL);
            hash_free_hash_map(data->tcb->sent_packets);
            free(data->tcb);
        }

        free(data);
    }

    return 0;
}

int net_inet_connect(struct socket *socket, const struct sockaddr_in *addr, socklen_t addrlen) {
    assert(socket);

    if (socket->type != SOCK_STREAM || socket->protocol != IPPROTO_TCP) {
        return -EINVAL;
    }

    if (addr->sin_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
        return -EAFNOSUPPORT;
    }

    if (socket->state != BOUND) {
        struct sockaddr_in to_bind = { AF_INET, 0, { 0 }, { 0 } };
        int ret = net_inet_bind(socket, &to_bind, sizeof(struct sockaddr_in));
        if (ret < 0) {
            return ret;
        }
    }

    struct inet_socket_data *data = socket->private_data;
    struct ip_v4_address dest_ip = ip_v4_from_uint(addr->sin_addr.s_addr);
    struct network_interface *interface = net_get_interface_for_ip(dest_ip);

    data->dest_ip = dest_ip;
    data->dest_port = ntohs(addr->sin_port);

    create_tcp_socket_mapping(socket);
    assert(net_get_tcp_socket_by_ip_v4_and_port((struct ip_v4_and_port) { data->dest_port, data->dest_ip }) != NULL);

    data->tcb = calloc(1, sizeof(struct tcp_control_block));
    assert(data->tcb);

    data->tcb->current_sequence_num = 0;
    data->tcb->current_ack_num = 0;

    data->tcb->sent_packets = hash_create_hash_map(tcp_data_hash, tcp_data_equals, tcp_data_key);
    assert(data->tcb->sent_packets);

    net_send_tcp(interface, dest_ip, data->source_port, data->dest_port, data->tcb->current_sequence_num++, data->tcb->current_ack_num, (union tcp_flags) { .raw_flags=TCP_FLAGS_SYN }, 0, NULL);

    time_t start = get_time();
    for (;;) {
        if (socket->state == CONNECTED) {
            debug_log("Successfully connected socket: [ %lu ]\n", socket->id);
            return 0;
        }

        // It took too long to get the SYN ACK back
        if (get_time() - start >= 5000) {
            break;
        }

        yield();
        barrier();
    }

    return -ETIMEDOUT;
}

int net_inet_socket(int domain, int type, int protocol) {
    assert(domain == AF_INET);

    if (protocol == 0 && type == SOCK_DGRAM) {
        protocol = IPPROTO_UDP;
    }

    if (protocol == 0 && type == SOCK_STREAM) {
        protocol = IPPROTO_TCP;
    }

    if (protocol != IPPROTO_ICMP && protocol != IPPROTO_UDP && protocol != IPPROTO_TCP) {
        return -EPROTONOSUPPORT;
    }

    int fd;
    struct socket *socket = net_create_socket(domain, type, protocol, &fd);
    (void) socket;

    return fd;
}

ssize_t net_inet_sendto(struct socket *socket, const void *buf, size_t len, int flags, const struct sockaddr_in *dest, socklen_t addrlen) {
    (void) flags;

    assert(socket);
    assert((socket->type & SOCK_TYPE_MASK) == SOCK_RAW || (socket->type & SOCK_TYPE_MASK) == SOCK_DGRAM || (socket->type & SOCK_TYPE_MASK) == SOCK_STREAM);

    if (socket->protocol == IPPROTO_TCP) {
        if (dest) {
            return -EINVAL;
        }

        assert(socket->state == CONNECTED);
        assert(socket->private_data);

        struct inet_socket_data *data = socket->private_data;
        struct network_interface *interface = net_get_interface_for_ip(data->dest_ip);

        net_send_tcp(interface, data->dest_ip, data->source_port, data->dest_port,
            data->tcb->current_sequence_num, data->tcb->current_ack_num, (union tcp_flags) { .bits.ack=data->tcb->should_send_ack }, len, buf);

        data->tcb->should_send_ack = false;
        return (ssize_t) len;
    }

    assert(dest);
    if (dest->sin_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    struct network_interface *interface = net_get_interface_for_ip(ip_v4_from_uint(dest->sin_addr.s_addr));
    assert(interface);

    if ((socket->type & SOCK_TYPE_MASK) == SOCK_RAW) {
        return net_send_ip_v4(interface, socket->protocol, ip_v4_from_uint(dest->sin_addr.s_addr), buf, len);
    }

    assert(socket->type == SOCK_DGRAM && socket->protocol == IPPROTO_UDP);
    if (socket->state != BOUND) {
        struct sockaddr_in to_bind = { AF_INET, 0, { 0 }, { 0 } };
        int ret = net_inet_bind(socket, &to_bind, sizeof(struct sockaddr_in));
        if (ret < 0) {
            return ret;
        }
    }

    if (dest == NULL) {
        return -EINVAL;
    }

    struct inet_socket_data *data = socket->private_data;
    struct ip_v4_address dest_ip = ip_v4_from_uint(dest->sin_addr.s_addr);
    return net_send_udp(interface, dest_ip, data->source_port, ntohs(dest->sin_port), len, buf);
}

ssize_t net_inet_recvfrom(struct socket *socket, void *buf, size_t len, int flags, struct sockaddr_in *source, socklen_t *addrlen) {
    (void) flags;

    return net_generic_recieve_from(socket, buf, len, (struct sockaddr*) source, addrlen);
}

struct socket *net_get_tcp_socket_by_ip_v4_and_port(struct ip_v4_and_port tuple) {
    struct tcp_socket_mapping *mapping = hash_get(map, &tuple);
    if (mapping == NULL) {
        return NULL;
    }

    return net_get_socket_by_id(mapping->socket_id);
}

void init_inet_sockets() {
    map = hash_create_hash_map(ip_v4_and_port_hash, ip_v4_and_port_equals, ip_v4_and_port_key);
    assert(map);
}