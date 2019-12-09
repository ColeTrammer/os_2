#include <assert.h>
#include <errno.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <kernel/fs/file.h>
#include <kernel/hal/output.h>
#include <kernel/hal/timer.h>
#include <kernel/net/inet_socket.h>
#include <kernel/net/socket.h>
#include <kernel/net/tcp.h>
#include <kernel/net/unix_socket.h>
#include <kernel/proc/task.h>
#include <kernel/sched/task_sched.h>
#include <kernel/util/hash_map.h>
#include <kernel/util/spinlock.h>

static unsigned long socket_id_next = 1;
static spinlock_t id_lock = SPINLOCK_INITIALIZER;

static int socket_file_close(struct file *file);
static ssize_t net_read(struct file *file, void *buf, size_t len);
static ssize_t net_write(struct file *file, const void *buf, size_t len);

static struct file_operations socket_file_ops = { socket_file_close, net_read, net_write, NULL };

static struct hash_map *map;

static int socket_hash(void *i, int num_buckets) {
    return *((unsigned long *) i) % num_buckets;
}

static int socket_equals(void *i1, void *i2) {
    return *((unsigned long *) i1) == *((unsigned long *) i2);
}

static void *socket_key(void *socket) {
    return &((struct socket *) socket)->id;
}

static int socket_file_close(struct file *file) {
    assert(file);

    struct socket_file_data *file_data = file->private_data;
    assert(file_data);

    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    spin_lock(&socket->lock);

    debug_log("Destroying socket: [ %lu ]\n", socket->id);

    int ret = 0;
    switch (socket->domain) {
        case AF_INET:
            ret = net_inet_close(socket);
            break;
        case AF_UNIX:
            ret = net_unix_close(socket);
            break;
        default:
            break;
    }

    struct socket_data *to_remove = socket->data_head;
    while (to_remove != NULL) {
        struct socket_data *next = to_remove->next;
        free(to_remove);
        to_remove = next;
    }

    hash_del(map, &socket->id);

    spin_unlock(&socket->lock);

    free(socket);
    free(file_data);

    return ret;
}

static ssize_t net_read(struct file *file, void *buf, size_t len) {
    return net_recvfrom(file, buf, len, 0, NULL, NULL);
}

static ssize_t net_write(struct file *file, const void *buf, size_t len) {
    return net_sendto(file, buf, len, 0, NULL, 0);
}

struct socket *net_create_socket(int domain, int type, int protocol, int *fd) {
    struct task *current = get_current_task();

    for (int i = 0; i < FOPEN_MAX; i++) {
        if (current->process->files[i] == NULL) {
            current->process->files[i] = calloc(1, sizeof(struct file));
            current->process->files[i]->flags = FS_SOCKET;
            current->process->files[i]->f_op = &socket_file_ops;
            current->process->files[i]->ref_count = 1;

            struct socket_file_data *file_data = malloc(sizeof(struct socket_file_data));
            current->process->files[i]->private_data = file_data;

            spin_lock(&id_lock);
            file_data->socket_id = socket_id_next++;
            spin_unlock(&id_lock);

            struct socket *socket = calloc(1, sizeof(struct socket));
            socket->domain = domain;
            socket->type = type;
            socket->protocol = protocol;
            socket->id = file_data->socket_id;
            socket->timeout = (struct timeval) { 10, 0 };
            init_spinlock(&socket->lock);

            hash_put(map, socket);

            *fd = i;
            return socket;
        }
    }

    *fd = -EMFILE;
    return NULL;
}

ssize_t net_generic_recieve_from(struct socket *socket, void *buf, size_t len, struct sockaddr *addr, socklen_t *addrlen) {
    if (socket->state != CONNECTED && socket->type == SOCK_STREAM) {
        return -ENOTCONN;
    }

    struct socket_data *data;

    time_t start_time = get_time();

    for (;;) {
        spin_lock(&socket->lock);
        data = socket->data_head;

        if (data != NULL) {
            break;
        }

        spin_unlock(&socket->lock);

        switch (socket->domain) {
            case AF_UNIX: {
                struct unix_socket_data *d = socket->private_data;
                if (!net_get_socket_by_id(d->connected_id)) {
                    debug_log("Connection terminated: [ %lu ]\n", socket->id);
                    return 0;
                }
            }
            default: {
                break;
            }
        }

        if (socket->type & SOCK_NONBLOCK) {
            return -EAGAIN;
        }

        if (socket->timeout.tv_sec != 0 || socket->timeout.tv_usec != 0) {
            time_t now = get_time();
            time_t ms_seconds_to_wait = socket->timeout.tv_sec * 1000 + socket->timeout.tv_usec / 1000;

            // We timed out
            if (now >= start_time + ms_seconds_to_wait) {
                return -EAGAIN;
            }
        }

        kernel_yield();
    }

    socket->data_head = data->next;
    remque(data);
    if (socket->data_head == NULL) {
        socket->data_tail = NULL;
    }

    spin_unlock(&socket->lock);

    if (socket->protocol == IPPROTO_TCP) {
        struct inet_socket_data *data = socket->private_data;
        assert(data);
        if (data->tcb->should_send_ack) {
            struct network_interface *interface = net_get_interface_for_ip(data->dest_ip);

            net_send_tcp(interface, data->dest_ip, data->source_port, data->dest_port, data->tcb->current_sequence_num,
                         data->tcb->current_ack_num, (union tcp_flags) { .bits.ack = 1, .bits.fin = socket->state == CLOSING }, 0, NULL);
            data->tcb->should_send_ack = false;

            if (socket->state == CLOSING) {
                socket->state = CLOSED;
            }
        }
    }

    size_t to_copy = MIN(len, data->len);
    memcpy(buf, data->data, to_copy);

    if (addr && addrlen) {
        size_t len = MIN(data->from.addrlen, *addrlen);
        memcpy(addr, &data->from.addr, len);
        *addrlen = len;
    }

    debug_log("Received message: [ %lu, %lu ]\n", socket->id, to_copy);

    free(data);
    return (ssize_t) to_copy;
}

int net_get_next_connection(struct socket *socket, struct socket_connection *connection) {
    for (;;) {
        spin_lock(&socket->lock);
        if (socket->pending[0] != NULL) {
            memcpy(connection, socket->pending[0], sizeof(struct socket_connection));

            free(socket->pending[0]);
            memmove(socket->pending, socket->pending + 1, (socket->pending_length - 1) * sizeof(struct socket_connection *));
            socket->pending[--socket->num_pending] = NULL;

            spin_unlock(&socket->lock);
            break;
        }

        spin_unlock(&socket->lock);

        if (socket->type & SOCK_NONBLOCK) {
            return -EAGAIN;
        }

        kernel_yield();
        barrier();
    }

    return 0;
}

struct socket *net_get_socket_by_id(unsigned long id) {
    return hash_get(map, &id);
}

void net_for_each_socket(void (*f)(struct socket *socket, void *data), void *data) {
    hash_for_each(map, (void (*)(void *, void *)) f, data);
}

ssize_t net_send_to_socket(struct socket *to_send, struct socket_data *socket_data) {
    spin_lock(&to_send->lock);
    insque(socket_data, to_send->data_tail);
    if (!to_send->data_head) {
        to_send->data_head = to_send->data_tail = socket_data;
    } else {
        to_send->data_tail = socket_data;
    }

    debug_log("Sent message to: [ %lu ]\n", to_send->id);

    spin_unlock(&to_send->lock);
    return (ssize_t) socket_data->len;
}

int net_accept(struct file *file, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    if (socket->state != LISTENING) {
        return -EINVAL;
    }

    if ((socket->type & SOCK_TYPE_MASK) != SOCK_STREAM) {
        return -EOPNOTSUPP;
    }

    switch (socket->domain) {
        case AF_INET:
            return net_inet_accept(socket, (struct sockaddr_in *) addr, addrlen, flags);
        case AF_UNIX:
            return net_unix_accept(socket, (struct sockaddr_un *) addr, addrlen, flags);
        default:
            return -EAFNOSUPPORT;
    }
}

int net_bind(struct file *file, const struct sockaddr *addr, socklen_t addrlen) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    switch (socket->domain) {
        case AF_INET:
            return net_inet_bind(socket, (const struct sockaddr_in *) addr, addrlen);
        case AF_UNIX:
            return net_unix_bind(socket, (const struct sockaddr_un *) addr, addrlen);
        default:
            return -EAFNOSUPPORT;
    }
}

int net_connect(struct file *file, const struct sockaddr *addr, socklen_t addrlen) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    switch (socket->domain) {
        case AF_INET:
            return net_inet_connect(socket, (const struct sockaddr_in *) addr, addrlen);
        case AF_UNIX:
            return net_unix_connect(socket, (const struct sockaddr_un *) addr, addrlen);
        default:
            return -EAFNOSUPPORT;
    }
}

int net_listen(struct file *file, int backlog) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    if (backlog <= 0 || socket->state != BOUND) {
        return -EINVAL;
    }

    switch (socket->domain) {
        case AF_INET: {
            int ret = net_inet_listen(socket);
            if (ret < 0) {
                return ret;
            }
            break;
        }
        case AF_UNIX:
            break;
        default:
            return -EAFNOSUPPORT;
    }

    socket->pending = calloc(backlog, sizeof(struct socket_connection *));
    socket->pending_length = backlog;
    socket->num_pending = 0;

    debug_log("Set socket to listening: [ %lu, %d ]\n", socket->id, socket->pending_length);

    socket->state = LISTENING;
    return 0;
}

int net_setsockopt(struct file *file, int level, int optname, const void *optval, socklen_t optlen) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    if (level != SOL_SOCKET) {
        return -ENOPROTOOPT;
    }

    if (optname != SO_RCVTIMEO) {
        return -ENOPROTOOPT;
    }

    if (optlen != sizeof(struct timeval)) {
        return -EINVAL;
    }

    socket->timeout = *((const struct timeval *) optval);

    return 0;
}

int net_socket(int domain, int type, int protocol) {
    switch (domain) {
        case AF_UNIX:
            return net_unix_socket(domain, type, protocol);
        case AF_INET:
            return net_inet_socket(domain, type, protocol);
        default:
            return -EAFNOSUPPORT;
    }
}

ssize_t net_sendto(struct file *file, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    switch (socket->domain) {
        case AF_UNIX:
            return net_unix_sendto(socket, buf, len, flags, (const struct sockaddr_un *) dest, addrlen);
        case AF_INET:
            return net_inet_sendto(socket, buf, len, flags, (const struct sockaddr_in *) dest, addrlen);
        default:
            return -EAFNOSUPPORT;
    }
}

ssize_t net_recvfrom(struct file *file, void *buf, size_t len, int flags, struct sockaddr *source, socklen_t *addrlen) {
    assert(file);
    assert(file->private_data);

    struct socket_file_data *file_data = file->private_data;
    struct socket *socket = hash_get(map, &file_data->socket_id);
    assert(socket);

    switch (socket->domain) {
        case AF_UNIX:
            return net_unix_recvfrom(socket, buf, len, flags, (struct sockaddr_un *) source, addrlen);
        case AF_INET:
            return net_inet_recvfrom(socket, buf, len, flags, (struct sockaddr_in *) source, addrlen);
        default:
            return -EAFNOSUPPORT;
    }
}

void init_net_sockets() {
    map = hash_create_hash_map(socket_hash, socket_equals, socket_key);
    init_inet_sockets();
}