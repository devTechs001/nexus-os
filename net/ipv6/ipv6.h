/*
 * NEXUS OS - IPv6 Network Stack
 * net/ipv6/ipv6.h
 * 
 * Complete IPv6 implementation with modern networking features
 */

#ifndef _NEXUS_IPV6_H
#define _NEXUS_IPV6_H

#include "../core/net.h"

/*===========================================================================*/
/*                         IPv6 CONSTANTS                                    */
/*===========================================================================*/

#define IPV6_VERSION            6
#define IPV6_HDR_LEN            40
#define IPV6_MIN_MTU            1280
#define IPV6_DEFAULT_HOPLIMIT   64

/* IPv6 Address Length */
#define IPV6_ADDR_LEN           16
#define IPV6_ADDR_WORDS         8

/* IPv6 Address Types */
#define IPV6_ADDR_ANY           0x00
#define IPV6_ADDR_UNICAST       0x01
#define IPV6_ADDR_MULTICAST     0x02
#define IPV6_ADDR_ANYCAST       0x03
#define IPV6_ADDR_LOOPBACK      0x04
#define IPV6_ADDR_LINKLOCAL     0x05
#define IPV6_ADDR_SITELOCAL     0x06
#define IPV6_ADDR_GLOBAL        0x07

/* IPv6 Extension Headers */
#define IPV6_NEXTHDR_HOPBYHOP   0
#define IPV6_NEXTHDR_TCP        6
#define IPV6_NEXTHDR_UDP        17
#define IPV6_NEXTHDR_ROUTING    43
#define IPV6_NEXTHDR_FRAGMENT   44
#define IPV6_NEXTHDR_AH         51
#define IPV6_NEXTHDR_ICMPV6     58
#define IPV6_NEXTHDR_NONE       59
#define IPV6_NEXTHDR_DESTOPT    60

/* IPv6 Flags */
#define IPV6_FLAG_FRAGMENT      0x0001
#define IPV6_FLAG_ROUTING       0x0002
#define IPV6_FLAG_HOPBYHOP      0x0004

/*===========================================================================*/
/*                         IPv6 STRUCTURES                                   */
/*===========================================================================*/

/**
 * ipv6_addr_t - IPv6 Address
 */
typedef struct {
    union {
        u8 bytes[IPV6_ADDR_LEN];
        u16 words[IPV6_ADDR_WORDS];
        u32 dwords[IPV6_ADDR_LEN / 4];
        u64 qwords[IPV6_ADDR_LEN / 8];
    };
} ipv6_addr_t;

/**
 * ipv6_hdr_t - IPv6 Header
 */
typedef struct {
    u32 version_traffic_class;    /* 4 bits version, 8 bits TC, 20 bits flow label */
    u16 payload_length;
    u8 next_header;
    u8 hop_limit;
    ipv6_addr_t source_addr;
    ipv6_addr_t dest_addr;
} __packed ipv6_hdr_t;

/**
 * ipv6_fragment_hdr_t - IPv6 Fragment Header
 */
typedef struct {
    u8 next_header;
    u8 reserved;
    u16 fragment_offset;          /* 13 bits offset, 2 bits flags, 1 bit reserved */
    u32 identification;
} __packed ipv6_fragment_hdr_t;

/**
 * ipv6_routing_hdr_t - IPv6 Routing Header
 */
typedef struct {
    u8 next_header;
    u8 hdr_ext_len;
    u8 routing_type;
    u8 segments_left;
    u8 type_specific_data[4];
    /* Followed by addresses */
} __packed ipv6_routing_hdr_t;

/**
 * ipv6_hopbyhop_hdr_t - IPv6 Hop-by-Hop Options Header
 */
typedef struct {
    u8 next_header;
    u8 hdr_ext_len;
    /* Followed by options */
} __packed ipv6_hopbyhop_hdr_t;

/**
 * ipv6_pseudo_hdr_t - IPv6 Pseudo-header (for checksum)
 */
typedef struct {
    ipv6_addr_t source_addr;
    ipv6_addr_t dest_addr;
    u32 upper_layer_length;
    u8 zero[3];
    u8 next_header;
} __packed ipv6_pseudo_hdr_t;

/**
 * ipv6_route_entry_t - IPv6 Routing Table Entry
 */
typedef struct ipv6_route_entry {
    ipv6_addr_t destination;
    ipv6_addr_t netmask;
    ipv6_addr_t gateway;
    u32 interface_index;
    u32 metric;
    u32 flags;
    u64 expires;
    struct ipv6_route_entry *next;
} ipv6_route_entry_t;

/**
 * ipv6_neighbor_entry_t - IPv6 Neighbor Cache Entry (NDP)
 */
typedef struct ipv6_neighbor_entry {
    ipv6_addr_t ipv6_addr;
    u8 mac_addr[6];
    u8 state;
    u8 is_router;
    u32 interface_index;
    u64 last_seen;
    u64 expires;
    struct ipv6_neighbor_entry *next;
} ipv6_neighbor_entry_t;

/* Neighbor Discovery Protocol States */
#define NDP_STATE_INCOMPLETE    0
#define NDP_STATE_REACHABLE     1
#define NDP_STATE_STALE         2
#define NDP_STATE_DELAY         3
#define NDP_STATE_PROBE         4

/*===========================================================================*/
/*                         IPv6 API                                          */
/*===========================================================================*/

/* Address Operations */
void ipv6_addr_set_any(ipv6_addr_t *addr);
void ipv6_addr_set_loopback(ipv6_addr_t *addr);
void ipv6_addr_set_linklocal(ipv6_addr_t *addr, const u8 mac[6]);
int ipv6_addr_parse(const char *str, ipv6_addr_t *addr);
int ipv6_addr_to_string(const ipv6_addr_t *addr, char *str, size_t maxlen);
int ipv6_addr_compare(const ipv6_addr_t *a, const ipv6_addr_t *b);
int ipv6_addr_type(const ipv6_addr_t *addr);
bool ipv6_addr_is_multicast(const ipv6_addr_t *addr);
bool ipv6_addr_is_linklocal(const ipv6_addr_t *addr);
bool ipv6_addr_is_global(const ipv6_addr_t *addr);

/* Header Operations */
int ipv6_build_header(ipv6_hdr_t *hdr, const ipv6_addr_t *src, 
                      const ipv6_addr_t *dst, u16 payload_len, u8 next_header, u8 hop_limit);
int ipv6_send_packet(struct net_device *dev, const ipv6_addr_t *src,
                     const ipv6_addr_t *dst, const void *data, u16 len, u8 next_header);

/* Routing */
int ipv6_route_add(const ipv6_addr_t *dest, const ipv6_addr_t *gateway,
                   u32 iface_index, u32 metric);
int ipv6_route_remove(const ipv6_addr_t *dest);
ipv6_route_entry_t *ipv6_route_lookup(const ipv6_addr_t *dest);
void ipv6_route_init(void);
void ipv6_route_dump(void);

/* Neighbor Discovery (NDP) */
int ipv6_ndp_solicit(const ipv6_addr_t *target, u32 iface_index);
int ipv6_ndp_advertise(const ipv6_addr_t *target, u32 iface_index);
ipv6_neighbor_entry_t *ipv6_neighbor_lookup(const ipv6_addr_t *addr);
int ipv6_neighbor_add(const ipv6_addr_t *addr, const u8 mac[6], u32 iface_index);
void ipv6_neighbor_init(void);

/* ICMPv6 */
int icmpv6_echo_request(const ipv6_addr_t *src, const ipv6_addr_t *dst,
                        u16 id, u16 seq, const void *data, u16 len);
int icmpv6_echo_reply(const ipv6_addr_t *src, const ipv6_addr_t *dst,
                      u16 id, u16 seq, const void *data, u16 len);
void icmpv6_init(void);

/* TCP over IPv6 */
int tcp6_connect(const ipv6_addr_t *src, const ipv6_addr_t *dst,
                 u16 sport, u16 dport);
int tcp6_listen(u16 port);
int tcp6_send(struct socket *sock, const void *data, size_t len);

/* UDP over IPv6 */
int udp6_send(const ipv6_addr_t *src, const ipv6_addr_t *dst,
              u16 sport, u16 dport, const void *data, size_t len);
int udp6_recv(struct socket *sock, void *buf, size_t len);

/* Socket API */
int ipv6_socket_create(int type, int protocol);
int ipv6_socket_bind(int sockfd, const ipv6_addr_t *addr, u16 port);
int ipv6_socket_connect(int sockfd, const ipv6_addr_t *addr, u16 port);
int ipv6_socket_send(int sockfd, const void *buf, size_t len);
int ipv6_socket_recv(int sockfd, void *buf, size_t len);

/* Autoconfiguration */
int ipv6_autoconfigure(u32 iface_index);
int ipv6_generate_eui64(ipv6_addr_t *addr, const u8 mac[6]);

/* Statistics */
typedef struct {
    u64 tx_packets;
    u64 tx_bytes;
    u64 rx_packets;
    u64 rx_bytes;
    u64 tx_errors;
    u64 rx_errors;
    u64 fragments_created;
    u64 fragments_reassembled;
    u64 neighbor_solicitations;
    u64 neighbor_advertisements;
} ipv6_stats_t;

void ipv6_get_stats(ipv6_stats_t *stats);

#endif /* _NEXUS_IPV6_H */
