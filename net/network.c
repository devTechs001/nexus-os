/*
 * NEXUS OS - Networking Subsystem
 * net/network.c
 */

#include "core/net.h"
#include "ipv4/ipv4.h"
#include "ipv6/ipv6.h"
#include "../include/kernel.h"

/* External initializers from different layers */
extern void skbuff_init(void);
extern void net_core_init(void);
extern void socket_init(void);
extern void net_device_init(void);
extern void ip_init(void);
extern void udp_init(void);
extern void tcp_init(void);
extern void icmp_init(void);
extern void ipv6_init(void);
extern void firewall_init(void);

/**
 * network_init - Initialize the entire networking stack
 */
void network_init(void)
{
    pr_info("Initializing Networking Stack...\n");

    /* 1. Core infrastructure */
    skbuff_init();
    net_core_init();
    net_device_init();
    socket_init();

    /* 2. IPv4 Stack */
    ip_init();
    icmp_init();
    udp_init();
    tcp_init();

    /* 3. IPv6 Stack */
    ipv6_init();

    /* 4. Security */
    firewall_init();

    pr_info("Networking Stack initialized successfully.\n");
}
