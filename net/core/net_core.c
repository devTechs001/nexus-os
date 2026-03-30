/*
 * NEXUS OS - Network Core
 * net/core/net_core.c
 */

#include "net.h"

/* Global network statistics */
struct net_statistics net_stats;

/**
 * net_core_init - Initialize network core infrastructure
 */
void net_core_init(void)
{
    pr_info("Initializing Network Core...\n");

    /* Initialize statistics */
    memset(&net_stats, 0, sizeof(struct net_statistics));
    
    atomic_set(&net_stats.sockets_allocated, 0);
    atomic_set(&net_stats.sockets_in_use, 0);
    atomic_set(&net_stats.sockets_bound, 0);
    atomic_set(&net_stats.sockets_connected, 0);
    atomic_set(&net_stats.sockets_listening, 0);

    atomic64_set(&net_stats.packets_in, 0);
    atomic64_set(&net_stats.packets_out, 0);
    atomic64_set(&net_stats.bytes_in, 0);
    atomic64_set(&net_stats.bytes_out, 0);

    atomic64_set(&net_stats.errors_in, 0);
    atomic64_set(&net_stats.errors_out, 0);
    atomic64_set(&net_stats.drops_in, 0);
    atomic64_set(&net_stats.drops_out, 0);

    atomic_set(&net_stats.skbuffs_allocated, 0);
    atomic_set(&net_stats.skbuffs_in_use, 0);
    atomic_set(&net_stats.skbuffs_cloned, 0);
    atomic64_set(&net_stats.skbuff_memory, 0);

    atomic64_set(&net_stats.tcp_segments_in, 0);
    atomic64_set(&net_stats.tcp_segments_out, 0);
    atomic64_set(&net_stats.udp_datagrams_in, 0);
    atomic64_set(&net_stats.udp_datagrams_out, 0);
    atomic64_set(&net_stats.icmp_in, 0);
    atomic64_set(&net_stats.icmp_out, 0);

    atomic64_set(&net_stats.ip_packets_in, 0);
    atomic64_set(&net_stats.ip_packets_out, 0);
    atomic64_set(&net_stats.ip_fragments_in, 0);
    atomic64_set(&net_stats.ip_fragments_out, 0);
    atomic64_set(&net_stats.ip_reassembled, 0);

    pr_info("Network Core initialized.\n");
}

/**
 * net_stats_print - Print global network statistics
 */
void net_stats_print(void)
{
    printk("\n=== Global Network Statistics ===\n");
    printk("Packets In:  %llu\n", (unsigned long long)atomic64_read(&net_stats.packets_in));
    printk("Packets Out: %llu\n", (unsigned long long)atomic64_read(&net_stats.packets_out));
    printk("Bytes In:    %llu\n", (unsigned long long)atomic64_read(&net_stats.bytes_in));
    printk("Bytes Out:   %llu\n", (unsigned long long)atomic64_read(&net_stats.bytes_out));
    printk("Errors In:   %llu\n", (unsigned long long)atomic64_read(&net_stats.errors_in));
    printk("Errors Out:  %llu\n", (unsigned long long)atomic64_read(&net_stats.errors_out));
    printk("Drops In:    %llu\n", (unsigned long long)atomic64_read(&net_stats.drops_in));
    printk("Drops Out:   %llu\n", (unsigned long long)atomic64_read(&net_stats.drops_out));
    printk("Sockets In Use: %d\n", atomic_read(&net_stats.sockets_in_use));
    printk("Skbuffs In Use: %d\n", atomic_read(&net_stats.skbuffs_in_use));
}

/**
 * net_random - Generate a random number for network use
 */
u32 net_random(void)
{
    /* Simple PRNG for now - should use kernel random */
    static u32 seed = 123456789;
    seed = seed * 1103515245 + 12345;
    return (u32)(seed / 65536) % 32768;
}
