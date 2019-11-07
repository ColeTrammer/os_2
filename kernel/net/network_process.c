#include <arpa/inet.h>
#include <assert.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/hal/output.h>
#include <kernel/net/arp.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ip.h>
#include <kernel/net/network_process.h>
#include <kernel/sched/process_sched.h>
#include <kernel/util/spinlock.h>

static struct network_data *head = NULL;
static struct network_data *tail = NULL;
static spinlock_t lock = SPINLOCK_INITIALIZER;

static struct network_data *consume() {
    spin_lock(&lock);

    if (head == NULL) {
        spin_unlock(&lock);
        return NULL;
    }

    struct network_data *data = head;
    head = head->next;
    remque((void*) data);
    if (head == NULL) {
        tail = NULL;
    }

    spin_unlock(&lock);
    return data;
}

void net_on_incoming_packet(void *buf, size_t len) {
    struct network_data *new_data = malloc(sizeof(struct network_data));
    assert(new_data);
    new_data->buf = buf;
    new_data->len = len;

    spin_lock(&lock);

    insque(new_data, tail);
    if (head == NULL) {
        head = tail = new_data;
    } else {
        tail = new_data;
    }

    spin_unlock(&lock);
}

void net_network_process_start() {
    for (;;) {
        struct network_data *data = consume();
        if (data == NULL) {
            yield();
            continue;
        }

        debug_log("Responding to packet\n");

        if (data->len <= sizeof(struct ethernet_packet)) {
            debug_log("Packet was to small\n");
            continue;
        }

        struct ethernet_packet *packet = data->buf;
        switch (ntohs(packet->ether_type)) {
            case ETHERNET_TYPE_ARP:
                net_arp_recieve((struct arp_packet*) packet->payload, data->len - sizeof(struct ethernet_packet));
                break;
            case ETHERNET_TYPE_IPV4:
                net_ip_v4_recieve((struct ip_v4_packet*) packet->payload, data->len - sizeof(struct ethernet_packet));
                break;
            default:
                debug_log("Recived unknown packet: [ %#4X ]\n", ntohs(packet->ether_type));
        }

        free(data);
    }
}