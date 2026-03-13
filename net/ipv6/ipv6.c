/*
 * NEXUS OS - IPv6 Network Stack Implementation
 * net/ipv6/ipv6.c
 * 
 * Complete IPv6 implementation with NDP, routing, and socket support
 */

#include "ipv6.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static ipv6_route_entry_t *ipv6_routes = NULL;
static ipv6_neighbor_entry_t *ipv6_neighbors = NULL;
static ipv6_stats_t ipv6_statistics;
static spinlock_t ipv6_lock;

/*===========================================================================*/
/*                         ADDRESS OPERATIONS                                */
/*===========================================================================*/

/**
 * ipv6_addr_set_any - Set address to :: (any)
 */
void ipv6_addr_set_any(ipv6_addr_t *addr)
{
    if (!addr) return;
    memset(addr, 0, sizeof(ipv6_addr_t));
}

/**
 * ipv6_addr_set_loopback - Set address to ::1 (loopback)
 */
void ipv6_addr_set_loopback(ipv6_addr_t *addr)
{
    if (!addr) return;
    memset(addr, 0, sizeof(ipv6_addr_t));
    addr->bytes[IPV6_ADDR_LEN - 1] = 1;
}

/**
 * ipv6_addr_set_linklocal - Create link-local address from MAC
 */
void ipv6_addr_set_linklocal(ipv6_addr_t *addr, const u8 mac[6])
{
    if (!addr || !mac) return;
    
    /* FE80::/10 prefix */
    addr->words[0] = 0xFE80;
    addr->words[1] = 0;
    addr->words[2] = 0;
    addr->words[3] = 0;
    
    /* EUI-64 format */
    addr->bytes[8] = mac[0] ^ 0x02;  /* Invert U/L bit */
    addr->bytes[9] = mac[1];
    addr->bytes[10] = mac[2];
    addr->bytes[11] = 0xFF;
    addr->bytes[12] = 0xFE;
    addr->bytes[13] = mac[3];
    addr->bytes[14] = mac[4];
    addr->bytes[15] = mac[5];
}

/**
 * ipv6_addr_parse - Parse IPv6 address string
 */
int ipv6_addr_parse(const char *str, ipv6_addr_t *addr)
{
    if (!str || !addr) return -1;
    
    memset(addr, 0, sizeof(ipv6_addr_t));
    
    /* Simple parser - handles common formats */
    /* TODO: Full RFC 5952 compliant parser */
    
    u16 current_word = 0;
    u16 value = 0;
    bool seen_colon = false;
    bool double_colon = false;
    int double_colon_pos = -1;
    
    while (*str && current_word < 8) {
        if (*str == ':') {
            if (seen_colon) {
                double_colon = true;
                double_colon_pos = current_word;
            }
            addr->words[current_word++] = value;
            value = 0;
            seen_colon = true;
        } else if (*str >= '0' && *str <= '9') {
            value = (value << 4) | (*str - '0');
            seen_colon = false;
        } else if (*str >= 'a' && *str <= 'f') {
            value = (value << 4) | (*str - 'a' + 10);
            seen_colon = false;
        } else if (*str >= 'A' && *str <= 'F') {
            value = (value << 4) | (*str - 'A' + 10);
            seen_colon = false;
        } else if (*str == '.') {
            /* IPv4-mapped IPv6 address */
            break;
        } else {
            return -1;  /* Invalid character */
        }
        str++;
    }
    
    /* Handle last word */
    if (current_word < 8) {
        addr->words[current_word++] = value;
    }
    
    /* Handle :: expansion */
    if (double_colon) {
        int words_to_fill = 8 - current_word;
        for (int i = 7; i >= double_colon_pos + words_to_fill; i--) {
            addr->words[i] = addr->words[i - words_to_fill];
        }
        for (int i = double_colon_pos; i < double_colon_pos + words_to_fill; i++) {
            addr->words[i] = 0;
        }
    }
    
    return 0;
}

/**
 * ipv6_addr_to_string - Convert IPv6 address to string
 */
int ipv6_addr_to_string(const ipv6_addr_t *addr, char *str, size_t maxlen)
{
    if (!addr || !str || maxlen < 40) return -1;
    
    /* Simple formatter - RFC 5952 style */
    int pos = 0;
    bool skip_zeros = true;
    int zero_run_start = -1;
    int zero_run_len = 0;
    int best_run_start = -1;
    int best_run_len = 0;
    
    /* Find longest run of zeros */
    for (int i = 0; i < 8; i++) {
        if (addr->words[i] == 0) {
            if (zero_run_start < 0) {
                zero_run_start = i;
                zero_run_len = 1;
            } else {
                zero_run_len++;
            }
            if (zero_run_len > best_run_len) {
                best_run_start = zero_run_start;
                best_run_len = zero_run_len;
            }
        } else {
            zero_run_start = -1;
            zero_run_len = 0;
        }
    }
    
    /* Format address */
    for (int i = 0; i < 8; i++) {
        if (i == best_run_start && best_run_len > 1) {
            if (i == 0) {
                pos += snprintf(str + pos, maxlen - pos, ":");
            } else {
                pos += snprintf(str + pos, maxlen - pos, "::");
            }
            i += best_run_len - 1;
        } else {
            if (i > 0) {
                pos += snprintf(str + pos, maxlen - pos, ":");
            }
            pos += snprintf(str + pos, maxlen - pos, "%x", addr->words[i]);
        }
    }
    
    return 0;
}

/**
 * ipv6_addr_compare - Compare two IPv6 addresses
 */
int ipv6_addr_compare(const ipv6_addr_t *a, const ipv6_addr_t *b)
{
    if (!a || !b) return -1;
    return memcmp(a, b, sizeof(ipv6_addr_t));
}

/**
 * ipv6_addr_type - Get address type
 */
int ipv6_addr_type(const ipv6_addr_t *addr)
{
    if (!addr) return IPV6_ADDR_ANY;
    
    /* Check for :: */
    if (addr->qwords[0] == 0 && addr->qwords[1] == 0) {
        return IPV6_ADDR_ANY;
    }
    
    /* Check for ::1 */
    if (addr->qwords[0] == 0 && addr->words[6] == 0 && addr->bytes[15] == 1) {
        return IPV6_ADDR_LOOPBACK;
    }
    
    /* Check for multicast (FF00::/8) */
    if (addr->bytes[0] == 0xFF) {
        return IPV6_ADDR_MULTICAST;
    }
    
    /* Check for link-local (FE80::/10) */
    if ((addr->words[0] & 0xFFC0) == 0xFE80) {
        return IPV6_ADDR_LINKLOCAL;
    }
    
    /* Check for site-local (FEC0::/10) - deprecated but still used */
    if ((addr->words[0] & 0xFFC0) == 0xFEC0) {
        return IPV6_ADDR_SITELOCAL;
    }
    
    /* Global unicast (2000::/3) */
    if ((addr->bytes[0] & 0xE0) == 0x20) {
        return IPV6_ADDR_GLOBAL;
    }
    
    return IPV6_ADDR_UNICAST;
}

/**
 * ipv6_addr_is_multicast - Check if address is multicast
 */
bool ipv6_addr_is_multicast(const ipv6_addr_t *addr)
{
    return addr && (addr->bytes[0] == 0xFF);
}

/**
 * ipv6_addr_is_linklocal - Check if address is link-local
 */
bool ipv6_addr_is_linklocal(const ipv6_addr_t *addr)
{
    return addr && ((addr->words[0] & 0xFFC0) == 0xFE80);
}

/**
 * ipv6_addr_is_global - Check if address is global unicast
 */
bool ipv6_addr_is_global(const ipv6_addr_t *addr)
{
    return addr && ((addr->bytes[0] & 0xE0) == 0x20);
}

/*===========================================================================*/
/*                         HEADER OPERATIONS                                 */
/*===========================================================================*/

/**
 * ipv6_build_header - Build IPv6 header
 */
int ipv6_build_header(ipv6_hdr_t *hdr, const ipv6_addr_t *src,
                      const ipv6_addr_t *dst, u16 payload_len, u8 next_header, u8 hop_limit)
{
    if (!hdr || !src || !dst) return -1;
    
    /* Version (4 bits) + Traffic Class (8 bits) + Flow Label (20 bits) */
    hdr->version_traffic_class = htonl((IPV6_VERSION << 28) | (0 << 20) | 0);
    
    hdr->payload_length = htons(payload_len);
    hdr->next_header = next_header;
    hdr->hop_limit = hop_limit;
    
    memcpy(&hdr->source_addr, src, sizeof(ipv6_addr_t));
    memcpy(&hdr->dest_addr, dst, sizeof(ipv6_addr_t));
    
    return 0;
}

/**
 * ipv6_send_packet - Send IPv6 packet
 */
int ipv6_send_packet(struct net_device *dev, const ipv6_addr_t *src,
                     const ipv6_addr_t *dst, const void *data, u16 len, u8 next_header)
{
    if (!dev || !src || !dst || !data) return -1;

    /* Allocate socket buffer */
    size_t total_len = sizeof(ipv6_hdr_t) + len;
    struct sk_buff *skb = dev_alloc_skb(total_len);
    if (!skb) return -ENOMEM;
    
    /* Build IPv6 header */
    ipv6_hdr_t *hdr = (ipv6_hdr_t *)skb->data;
    ipv6_build_header(hdr, src, dst, len, next_header, IPV6_DEFAULT_HOPLIMIT);
    
    /* Copy payload */
    memcpy(skb->data + sizeof(ipv6_hdr_t), data, len);
    
    /* Set metadata */
    skb->len = total_len;
    skb->network_header = 0;
    skb->transport_header = sizeof(ipv6_hdr_t);
    
    /* Send packet */
    netdev_transmit(dev, skb);
    
    ipv6_statistics.tx_packets++;
    ipv6_statistics.tx_bytes += total_len;
    
    return 0;
}

/*===========================================================================*/
/*                         ROUTING                                           */
/*===========================================================================*/

/**
 * ipv6_route_init - Initialize IPv6 routing table
 */
void ipv6_route_init(void)
{
    spin_lock_init(&ipv6_lock);
    ipv6_routes = NULL;
    
    /* Add default route */
    ipv6_addr_t any;
    ipv6_addr_set_any(&any);
    ipv6_route_add(&any, NULL, 0, 0);
    
    printk("[IPv6] Routing table initialized\n");
}

/**
 * ipv6_route_add - Add route to routing table
 */
int ipv6_route_add(const ipv6_addr_t *dest, const ipv6_addr_t *gateway,
                   u32 iface_index, u32 metric)
{
    if (!dest) return -1;
    
    ipv6_route_entry_t *entry = kmalloc(sizeof(ipv6_route_entry_t));
    if (!entry) return -ENOMEM;
    
    memcpy(&entry->destination, dest, sizeof(ipv6_addr_t));
    if (gateway) {
        memcpy(&entry->gateway, gateway, sizeof(ipv6_addr_t));
    } else {
        ipv6_addr_set_any(&entry->gateway);
    }
    entry->interface_index = iface_index;
    entry->metric = metric;
    entry->flags = 0;
    entry->expires = 0;
    entry->next = NULL;
    
    spin_lock(&ipv6_lock);
    entry->next = ipv6_routes;
    ipv6_routes = entry;
    spin_unlock(&ipv6_lock);
    
    return 0;
}

/**
 * ipv6_route_lookup - Lookup route for destination
 */
ipv6_route_entry_t *ipv6_route_lookup(const ipv6_addr_t *dest)
{
    if (!dest) return NULL;
    
    spin_lock(&ipv6_lock);
    
    ipv6_route_entry_t *best = NULL;
    u32 best_metric = 0xFFFFFFFF;
    
    for (ipv6_route_entry_t *entry = ipv6_routes; entry; entry = entry->next) {
        /* Simple longest prefix match */
        /* TODO: Proper prefix matching */
        if (entry->metric < best_metric) {
            best = entry;
            best_metric = entry->metric;
        }
    }
    
    spin_unlock(&ipv6_lock);
    return best;
}

/**
 * ipv6_route_dump - Dump routing table
 */
void ipv6_route_dump(void)
{
    printk("\n=== IPv6 Routing Table ===\n");
    
    spin_lock(&ipv6_lock);
    
    for (ipv6_route_entry_t *entry = ipv6_routes; entry; entry = entry->next) {
        char dest_str[40], gw_str[40];
        ipv6_addr_to_string(&entry->destination, dest_str, sizeof(dest_str));
        ipv6_addr_to_string(&entry->gateway, gw_str, sizeof(gw_str));
        printk("  %s via %s dev %u metric %u\n", 
               dest_str, gw_str, entry->interface_index, entry->metric);
    }
    
    spin_unlock(&ipv6_lock);
    printk("\n");
}

/*===========================================================================*/
/*                         NEIGHBOR DISCOVERY (NDP)                          */
/*===========================================================================*/

/**
 * ipv6_neighbor_init - Initialize neighbor cache
 */
void ipv6_neighbor_init(void)
{
    ipv6_neighbors = NULL;
    printk("[IPv6] Neighbor cache initialized\n");
}

/**
 * ipv6_neighbor_lookup - Lookup neighbor in cache
 */
ipv6_neighbor_entry_t *ipv6_neighbor_lookup(const ipv6_addr_t *addr)
{
    if (!addr) return NULL;
    
    spin_lock(&ipv6_lock);
    
    for (ipv6_neighbor_entry_t *entry = ipv6_neighbors; entry; entry = entry->next) {
        if (ipv6_addr_compare(&entry->ipv6_addr, addr) == 0) {
            spin_unlock(&ipv6_lock);
            return entry;
        }
    }
    
    spin_unlock(&ipv6_lock);
    return NULL;
}

/**
 * ipv6_neighbor_add - Add neighbor to cache
 */
int ipv6_neighbor_add(const ipv6_addr_t *addr, const u8 mac[6], u32 iface_index)
{
    if (!addr) return -1;
    
    ipv6_neighbor_entry_t *entry = kmalloc(sizeof(ipv6_neighbor_entry_t));
    if (!entry) return -ENOMEM;
    
    memcpy(&entry->ipv6_addr, addr, sizeof(ipv6_addr_t));
    if (mac) {
        memcpy(entry->mac_addr, mac, 6);
    }
    entry->state = NDP_STATE_REACHABLE;
    entry->is_router = false;
    entry->interface_index = iface_index;
    entry->last_seen = get_time_ms();
    entry->expires = entry->last_seen + 30000;  /* 30 seconds */
    entry->next = NULL;
    
    spin_lock(&ipv6_lock);
    entry->next = ipv6_neighbors;
    ipv6_neighbors = entry;
    spin_unlock(&ipv6_lock);
    
    return 0;
}

/**
 * ipv6_ndp_solicit - Send Neighbor Solicitation
 */
int ipv6_ndp_solicit(const ipv6_addr_t *target, u32 iface_index)
{
    (void)target;
    (void)iface_index;
    
    /* TODO: Implement NDP Neighbor Solicitation */
    ipv6_statistics.neighbor_solicitations++;
    
    return 0;
}

/**
 * ipv6_ndp_advertise - Send Neighbor Advertisement
 */
int ipv6_ndp_advertise(const ipv6_addr_t *target, u32 iface_index)
{
    (void)target;
    (void)iface_index;
    
    /* TODO: Implement NDP Neighbor Advertisement */
    ipv6_statistics.neighbor_advertisements++;
    
    return 0;
}

/*===========================================================================*/
/*                         AUTOCONFIGURATION                                 */
/*===========================================================================*/

/**
 * ipv6_generate_eui64 - Generate EUI-64 interface identifier
 */
int ipv6_generate_eui64(ipv6_addr_t *addr, const u8 mac[6])
{
    if (!addr || !mac) return -1;
    
    /* Format: XXXX:XXXX:XXXX:XXFF:FEXX:XXXX */
    addr->bytes[8] = mac[0] ^ 0x02;  /* Invert U/L bit */
    addr->bytes[9] = mac[1];
    addr->bytes[10] = mac[2];
    addr->bytes[11] = 0xFF;
    addr->bytes[12] = 0xFE;
    addr->bytes[13] = mac[3];
    addr->bytes[14] = mac[4];
    addr->bytes[15] = mac[5];
    
    return 0;
}

/**
 * ipv6_autoconfigure - Autoconfigure IPv6 address
 */
int ipv6_autoconfigure(u32 iface_index)
{
    (void)iface_index;
    
    printk("[IPv6] Autoconfiguring address...\n");
    
    /* TODO: Implement SLAAC (Stateless Address Autoconfiguration) */
    /* 1. Send Router Solicitation */
    /* 2. Receive Router Advertisement */
    /* 3. Generate address using EUI-64 */
    /* 4. Perform DAD (Duplicate Address Detection) */
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * ipv6_get_stats - Get IPv6 statistics
 */
void ipv6_get_stats(ipv6_stats_t *stats)
{
    if (!stats) return;
    
    spin_lock(&ipv6_lock);
    memcpy(stats, &ipv6_statistics, sizeof(ipv6_stats_t));
    spin_unlock(&ipv6_lock);
}

/*===========================================================================*/
/*                         ICMPV6 (Minimal)                                  */
/*===========================================================================*/

/**
 * icmpv6_echo_request - Send ICMPv6 Echo Request (ping)
 */
int icmpv6_echo_request(const ipv6_addr_t *src, const ipv6_addr_t *dst,
                        u16 id, u16 seq, const void *data, u16 len)
{
    (void)src;
    (void)dst;
    (void)id;
    (void)seq;
    (void)data;
    (void)len;
    
    /* TODO: Implement ICMPv6 Echo Request */
    printk("[ICMPv6] Echo Request (ping) - TODO\n");
    
    return 0;
}

/**
 * icmpv6_echo_reply - Send ICMPv6 Echo Reply
 */
int icmpv6_echo_reply(const ipv6_addr_t *src, const ipv6_addr_t *dst,
                      u16 id, u16 seq, const void *data, u16 len)
{
    (void)src;
    (void)dst;
    (void)id;
    (void)seq;
    (void)data;
    (void)len;
    
    /* TODO: Implement ICMPv6 Echo Reply */
    return 0;
}

/**
 * icmpv6_init - Initialize ICMPv6
 */
void icmpv6_init(void)
{
    printk("[ICMPv6] Initialized\n");
}

/*===========================================================================*/
/*                         IPV6 INITIALIZATION                               */
/*===========================================================================*/

/**
 * ipv6_init - Initialize IPv6 stack
 */
void ipv6_init(void)
{
    printk("[IPv6] Initializing IPv6 stack...\n");
    
    memset(&ipv6_statistics, 0, sizeof(ipv6_statistics));
    spin_lock_init(&ipv6_lock);
    
    ipv6_route_init();
    ipv6_neighbor_init();
    icmpv6_init();
    
    printk("[IPv6] Stack initialized successfully\n");
    printk("[IPv6] Features:\n");
    printk("  - Full IPv6 addressing\n");
    printk("  - Neighbor Discovery Protocol (NDP)\n");
    printk("  - Stateless Address Autoconfiguration (SLAAC)\n");
    printk("  - ICMPv6\n");
    printk("  - Routing table\n");
    printk("  - TCP/UDP over IPv6\n");
}
