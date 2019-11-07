#include <kernel/hal/output.h>
#include <kernel/net/interface.h>
#include <kernel/net/arp.h>
#include <kernel/net/mac.h>
#include <kernel/net/net.h>
#include <kernel/net/network_process.h>
#include <kernel/net/socket.h>
#include <kernel/proc/process.h>
#include <kernel/sched/process_sched.h>

static void init_ip_v4_mappings(struct network_interface *interface) {
    debug_log("Initializing interface: [ %s ]\n", interface->name);

    net_create_ip_v4_to_mac_mapping(interface->address, interface->ops->get_mac_address(interface));
    net_create_ip_v4_to_mac_mapping(interface->broadcast, MAC_BROADCAST); // NOTE: This will be updated later by an arp requtest

    net_send_arp_request(interface, interface->broadcast);
}

void init_net() {
    init_net_sockets();
    init_mac();

    struct process *network_process = load_kernel_process((uintptr_t) net_network_process_start);
    assert(network_process);

    sched_add_process(network_process);

    net_for_each_interface(init_ip_v4_mappings);
}