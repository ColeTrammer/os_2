#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/hal/output.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/inet_socket.h>
#include <kernel/net/interface.h>
#include <kernel/net/ip.h>
#include <kernel/net/tcp.h>

ssize_t net_send_tcp(struct network_interface *interface, struct ip_v4_address dest, uint16_t source_port, uint16_t dest_port, uint32_t sequence_number, uint32_t ack_num, union tcp_flags flags, uint16_t len, const void *payload) {
    size_t total_length = sizeof(struct ethernet_packet) + sizeof(struct ip_v4_packet) + sizeof(struct tcp_packet) + len;

    struct ethernet_packet *packet = net_create_ethernet_packet(net_get_mac_from_ip_v4(interface->broadcast)->mac, interface->ops->get_mac_address(interface),
        ETHERNET_TYPE_IPV4, total_length - sizeof(struct ethernet_packet));

    struct ip_v4_packet *ip_packet = (struct ip_v4_packet*) packet->payload;
    net_init_ip_v4_packet(ip_packet, 1, IP_V4_PROTOCOL_TCP, interface->address, dest, total_length - sizeof(struct ethernet_packet) - sizeof(struct ip_v4_packet));

    struct tcp_packet *tcp_packet = (struct tcp_packet*) ip_packet->payload;
    net_init_tcp_packet(tcp_packet, source_port, dest_port, sequence_number, ack_num, flags, 8192, len, payload);

    struct ip_v4_pseudo_header header = {
        interface->address, dest, 0, IP_V4_PROTOCOL_TCP, htons(len + sizeof(struct tcp_packet))
    };

    tcp_packet->check_sum = ntohs(in_compute_checksum_with_start(tcp_packet, sizeof(struct tcp_packet) + len,
        in_compute_checksum(&header, sizeof(struct ip_v4_pseudo_header))));

    debug_log("Sending TCP packet\n");
    net_tcp_log(tcp_packet);

    ssize_t ret = interface->ops->send(interface, packet, total_length);

    free(packet);
    return ret < 0 ? ret : ret - (ssize_t) sizeof(struct ethernet_packet) - (ssize_t) sizeof(struct ip_v4_packet);
}

void net_tcp_recieve(const struct tcp_packet *packet, size_t len) {
    assert(packet);
    if (len < sizeof(struct tcp_packet)) {
        debug_log("TCP Packet to small\n");
        return;
    }

    net_tcp_log(packet);

    const struct ip_v4_packet *ip_packet = ((const struct ip_v4_packet*) packet) - 1;
    struct socket *socket = net_get_tcp_socket_by_ip_v4_and_port((struct ip_v4_and_port) { ntohs(packet->source_port), ip_packet->source });
    if (socket == NULL) {
        debug_log("No socket listening for port and ip: [ %u, %u.%u.%u.%u ]\n", ntohs(packet->source_port),
            ip_packet->source.addr[0], ip_packet->source.addr[1], ip_packet->source.addr[2], ip_packet->source.addr[3]);

        // Tell the destination we recieved there fin (we already sent ours and removed ourselves from the list of sockets)
        if (packet->flags.bits.fin) {
            struct network_interface *interface = net_get_interface_for_ip(ip_packet->source);
            net_send_tcp(interface, ip_packet->destination, ntohs(packet->dest_port), ntohs(packet->source_port),
                ntohl(packet->ack_number), ntohl(packet->sequence_number) + 1, (union tcp_flags) { .bits.ack=1 }, 0, NULL);
        }
        return;
    }

    struct inet_socket_data *data = socket->private_data;
    assert(data);
    assert(data->tcb);

    // expect this to be a SYN ACK
    if (socket->state != CONNECTED) {
        if (packet->flags.bits.syn && packet->flags.bits.ack) {
            if (data->tcb->current_sequence_num != ntohl(packet->ack_number)) {
                debug_log("Recieved incorrect sequence num: [ %u, %u ]\n", data->tcb->current_sequence_num, ntohl(packet->ack_number));
            }

            debug_log("Setting ack num to: [ %u ]\n", ntohl(packet->sequence_number) + 1);
            data->tcb->current_ack_num = ntohl(packet->sequence_number) + 1;
            data->tcb->should_send_ack = true;
            // struct network_interface *interface = net_get_interface_for_ip(data->dest_ip);
            // net_send_tcp(interface, data->dest_ip, data->source_port, data->dest_port,
            //     data->tcb->current_sequence_num, data->tcb->current_ack_num, (union tcp_flags) { .bits.ack=1 }, 0, NULL);

            debug_log("Setting socket state to connected\n");
            socket->state = CONNECTED;
        }

        return;
    }

    assert(socket->state == CONNECTED);

    if (packet->flags.bits.ack) {
        data->tcb->current_sequence_num = ntohl(packet->ack_number);
    }

    if (packet->flags.bits.fin) {
        socket->state = CLOSING;
        data->tcb->current_ack_num++;
        data->tcb->should_send_ack = true;
    }

    size_t message_length = ntohs(ip_packet->length) - sizeof(struct ip_v4_packet) - sizeof(uint32_t) * packet->data_offset;
    char *message = ((char*) packet) + sizeof(uint32_t) * packet->data_offset;

    if (message_length > 0) {
        data->tcb->current_ack_num += message_length;
        data->tcb->should_send_ack = true;
        struct socket_data *socket_data = net_inet_create_socket_data(ip_packet, packet->source_port, message, message_length);
        net_send_to_socket(socket, socket_data);
    }
}

void net_init_tcp_packet(struct tcp_packet *packet, uint16_t source_port, uint16_t dest_port, uint32_t sequence, uint32_t ack_num, union tcp_flags flags, uint16_t win_size, uint16_t payload_length, const void *payload) {
    packet->source_port = htons(source_port);
    packet->dest_port = htons(dest_port);
    packet->sequence_number = htonl(sequence);
    packet->ack_number = htonl(ack_num);
    packet->ns = 0;
    packet->zero = 0;
    packet->data_offset = sizeof(struct tcp_packet) / sizeof(uint32_t);
    packet->flags = flags;
    packet->window_size = htons(win_size);
    packet->check_sum = 0;
    packet->urg_pointer = 0;

    if (payload_length > 0 && payload != NULL) {
        memcpy(packet->options_and_payload, payload, payload_length);
    }
}

void net_tcp_log(const struct tcp_packet *packet) {
    const struct ip_v4_packet *ip_packet = ((const struct ip_v4_packet*) packet) - 1;

    debug_log("TCP Packet:\n"
              "               Source Port  [ %15u ]   Dest Port [ %15u ]\n"
              "               Sequence     [ %15u ]   Ack Num   [ %15u ]\n"
              "               Win Size     [ %15u ]   Urg Point [ %15u ]\n"
              "               Source IP    [ %03u.%03u.%03u.%03u ]   Dest IP   [ %03u.%03u.%03u.%03u ]\n"   
              "               Data Len     [ %15lu ]   Data off  [ %15u ]\n"
              "               Flags        [ FIN=%u SYN=%u RST=%u PSH=%u ACK=%u URG=%u ECE=%u CWR=%u ]\n"
              "               Data         [\n%s\n]\n",
              ntohs(packet->source_port), ntohs(packet->dest_port), ntohl(packet->sequence_number), ntohl(packet->ack_number),
              ntohs(packet->window_size), ntohs(packet->urg_pointer),
              ip_packet->source.addr[0], ip_packet->source.addr[1], ip_packet->source.addr[2], ip_packet->source.addr[3],
              ip_packet->destination.addr[0], ip_packet->destination.addr[1], ip_packet->destination.addr[2], ip_packet->destination.addr[3],
              ntohs(ip_packet->length) - sizeof(struct ip_v4_packet) - sizeof(uint32_t) * packet->data_offset, packet->data_offset,
              packet->flags.bits.fin, packet->flags.bits.syn, packet->flags.bits.rst, packet->flags.bits.psh,
              packet->flags.bits.ack, packet->flags.bits.urg, packet->flags.bits.ece, packet->flags.bits.cwr,
              ntohs(ip_packet->length) > sizeof(uint32_t) * packet->data_offset + sizeof(struct ip_v4_packet) ? ((char*) packet) + sizeof(uint32_t) * packet->data_offset : "None"
    );
}