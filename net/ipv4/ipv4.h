/*
 * NEXUS OS - IPv4 Stack Header
 * net/ipv4/ipv4.h
 */

#ifndef _NEXUS_IPV4_H
#define _NEXUS_IPV4_H

#include "../../kernel/include/kernel.h"
#include "../core/net.h"

/*===========================================================================*/
/*                         IPv4 CONSTANTS                                    */
/*===========================================================================*/

/* IPv4 Version */
#define IP_VERSION_4            4

/* IPv4 Header Length */
#define IP_MIN_HDR_LEN          20      /* Minimum IP header length */
#define IP_MAX_HDR_LEN          60      /* Maximum IP header length (with options) */

/* IPv4 Protocol Numbers */
#define IPPROTO_HOPOPTS         0       /* IPv6 Hop-by-Hop options */
#define IPPROTO_ICMP            1       /* ICMP */
#define IPPROTO_IGMP            2       /* IGMP */
#define IPPROTO_IPIP            4       /* IPIP tunnels */
#define IPPROTO_TCP             6       /* TCP */
#define IPPROTO_EGP             8       /* EGP */
#define IPPROTO_PUP             12      /* PUP */
#define IPPROTO_UDP             17      /* UDP */
#define IPPROTO_IDP             22      /* IDP */
#define IPPROTO_TP              29      /* TP-4 */
#define IPPROTO_DCCP            33      /* DCCP */
#define IPPROTO_IPV6            41      /* IPv6 */
#define IPPROTO_RSVP            46      /* RSVP */
#define IPPROTO_GRE             47      /* GRE */
#define IPPROTO_ESP             50      /* ESP */
#define IPPROTO_AH              51      /* AH */
#define IPPROTO_ICMPV6          58      /* ICMPv6 */
#define IPPROTO_NONE            59      /* No next header */
#define IPPROTO_DSTOPTS         60      /* IPv6 Destination options */
#define IPPROTO_BEETPH          94      /* BEET PH */
#define IPPROTO_MTP             92      /* MTP */
#define IPPROTO_ENCAP           98      /* ENCAP */
#define IPPROTO_PIM             103     /* PIM */
#define IPPROTO_COMP            108     /* COMP */
#define IPPROTO_SCTP            132     /* SCTP */
#define IPPROTO_UDPLITE         136     /* UDPLITE */
#define IPPROTO_RAW             255     /* RAW */

/* IPv4 Type of Service / DSCP */
#define IPTOS_TOS_MASK          0x1E
#define IPTOS_TOS(tos)          ((tos) & IPTOS_TOS_MASK)
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#define IPTOS_LOWCOST           0x02
#define IPTOS_MINCOST           IPTOS_LOWCOST

#define IPTOS_PREC_MASK         0xE0
#define IPTOS_PREC(tos)         ((tos) & IPTOS_PREC_MASK)
#define IPTOS_PREC_NETCONTROL   0xE0
#define IPTOS_PREC_INTERNETCONTROL 0xC0
#define IPTOS_PREC_CRITIC_ECP   0xA0
#define IPTOS_PREC_FLASHOVERRIDE 0x80
#define IPTOS_PREC_FLASH        0x60
#define IPTOS_PREC_IMMEDIATE    0x40
#define IPTOS_PREC_PRIORITY     0x20
#define IPTOS_PREC_ROUTINE      0x00

/* IPv4 Fragment Flags */
#define IP_DF                   0x4000  /* Don't Fragment */
#define IP_MF                   0x2000  /* More Fragments */
#define IP_OFFSET               0x1FFF  /* Fragment Offset Mask */

/* IPv4 Options */
#define IPOPT_END               0       /* End of options */
#define IPOPT_NOP               1       /* No operation */
#define IPOPT_RR                7       /* Record Route */
#define IPOPT_TS                68      /* Timestamp */
#define IPOPT_SECURITY          130     /* Security */
#define IPOPT_LSRR              131     /* Loose Source Route */
#define IPOPT_SATID             136     /* Satellite ID */
#define IPOPT_SSRR              137     /* Strict Source Route */
#define IPOPT_RA                148     /* Router Alert */

/* IPv4 Address Classes */
#define IP_CLASS_A              0x00000000
#define IP_CLASS_B              0x80000000
#define IP_CLASS_C              0xC0000000
#define IP_CLASS_D              0xE0000000  /* Multicast */
#define IP_CLASS_E              0xF0000000  /* Reserved */

#define IP_MULTICAST(a)         (((a) & IP_CLASS_D) == IP_CLASS_D)
#define IP_BAD_CLASS(a)         (((a) & IP_CLASS_E) == IP_CLASS_E)

/* Special IPv4 Addresses */
#define IP_ADDR_ANY             0x00000000  /* 0.0.0.0 */
#define IP_ADDR_BROADCAST       0xFFFFFFFF  /* 255.255.255.255 */
#define IP_ADDR_LOOPBACK        0x7F000001  /* 127.0.0.1 */
#define IP_ADDR_NONE            0xFFFFFFFF  /* 255.255.255.255 */

/* Multicast Addresses */
#define IP_MULTICAST_LOOP       0x00000001
#define IP_MULTICAST_TTL        0x00000002
#define IP_MULTICAST_IF         0x00000003
#define IP_ADD_MEMBERSHIP       0x00000004
#define IP_DROP_MEMBERSHIP      0x00000005

/* IP Socket Options */
#define IP_TOS                  1
#define IP_TTL                  2
#define IP_HDRINCL              3
#define IP_OPTIONS              4
#define IP_ROUTER_ALERT         5
#define IP_RECVOPTS             6
#define IP_RETOPTS              7
#define IP_PKTINFO              8
#define IP_PKTOPTIONS           9
#define IP_MTU_DISCOVER         10
#define IP_RECVERR              11
#define IP_RECVTTL              12
#define IP_RECVTOS              13
#define IP_MTU                  14
#define IP_FREEBIND             15
#define IP_IPSEC_POLICY         16
#define IP_XFRM_POLICY          17
#define IP_PASSSEC              18
#define IP_TRANSPARENT          19
#define IP_ORIGDSTADDR          20
#define IP_RECVORIGDSTADDR      IP_ORIGDSTADDR
#define IP_MINTTL               21
#define IP_NODEFRAG             22
#define IP_CHECKSUM             23
#define IP_BIND_ADDRESS_NO_PORT 24
#define IP_RECVFRAGSIZE         25

/* IP MTU Discover */
#define IP_PMTUDISC_DONT        0
#define IP_PMTUDISC_WANT        1
#define IP_PMTUDISC_DO          2
#define IP_PMTUDISC_PROBE       3
#define IP_PMTUDISC_INTERFACE   4
#define IP_PMTUDISC_OMIT        5

/* Router Alert */
#define IP_ROUTER_ALERT_MLD     0
#define IP_ROUTER_ALERT_RSVP    1

/* Maximum Values */
#define IP_MAX_MTU              65535
#define IP_MIN_MTU              68
#define IP_DEFAULT_TTL          64
#define IP_MAX_TTL              255

/* Routing Table */
#define RT_TABLE_MAX            256
#define RT_TABLE_DEFAULT        253
#define RT_TABLE_MAIN           254
#define RT_TABLE_LOCAL          255

/* Routing Flags */
#define RTF_UP                  0x0001  /* Route usable */
#define RTF_GATEWAY             0x0002  /* Destination is gateway */
#define RTF_HOST                0x0004  /* Host entry */
#define RTF_REJECT              0x0008  /* Reject route */
#define RTF_DYNAMIC             0x0010  /* Created dynamically */
#define RTF_MODIFIED            0x0020  /* Modified dynamically */
#define RTF_MTU                 0x0040  /* MTU set */
#define RTF_WINDOW              0x0080  /* Window size set */
#define RTF_IRTT                0x0100  /* Initial RTT set */
#define RTF_REINSTATE           0x0200  /* Reinstate route */
#define RTF_MULTICAST           0x0400  /* Multicast route */
#define RTF_LOCAL               0x8000  /* Local route */

/*===========================================================================*/
/*                         IPv4 HEADER STRUCTURES                            */
/*===========================================================================*/

/**
 * iphdr - IPv4 header structure
 *
 * The IPv4 header is the first part of every IPv4 packet. It contains
 * routing and delivery information.
 */
struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u8 ihl:4;                 /* Internet Header Length (in 32-bit words) */
    u8 version:4;             /* IP version (4 for IPv4) */
#elif defined(__BIG_ENDIAN_BITFIELD)
    u8 version:4;             /* IP version (4 for IPv4) */
    u8 ihl:4;                 /* Internet Header Length (in 32-bit words) */
#else
#error "Please fix <asm/byteorder.h>"
#endif
    u8 tos;                     /* Type of Service / DSCP */
    u16 tot_len;                /* Total length (header + data) */
    u16 id;                     /* Identification */
    u16 frag_off;               /* Fragment offset and flags */
    u8 ttl;                     /* Time to Live */
    u8 protocol;                /* Protocol (TCP, UDP, etc.) */
    u16 check;                  /* Header checksum */
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
    /* Options may follow */
} __packed;

/**
 * ip_options - IPv4 options structure
 */
struct ip_options {
    u8 *opt;                    /* Pointer to options */
    u8 optlen;                  /* Options length */
    u8 srr;                     /* Strict/Loose Source Route */
    u8 is_strictroute:1;        /* Is strict source route */
    u8 is_setbyuser:1;          /* Set by user */
    u8 is_expired:1;            /* Options expired */
    u8 rr;                      /* Record Route */
    u8 ts;                      /* Timestamp */
    u8 router_alert:1;          /* Router alert */
    u8 cipso;                   /* CIPSO */
    u8 __pad2;                  /* Padding */
    u8 __pad3;                  /* Padding */
};

/**
 * ip_mreq - IP multicast request
 */
struct ip_mreq {
    u32 imr_multiaddr;          /* Multicast address */
    u32 imr_interface;          /* Interface address */
} __packed;

/**
 * ip_mreqn - IP multicast request with interface index
 */
struct ip_mreqn {
    u32 imr_multiaddr;          /* Multicast address */
    u32 imr_address;            /* Interface address */
    u32 imr_ifindex;            /* Interface index */
} __packed;

/**
 * ip_pktinfo - IP packet info
 */
struct ip_pktinfo {
    u32 ipipi_ifindex;          /* Interface index */
    u32 ipipi_spec_dst;         /* Local address */
    u32 ipipi_addr;             /* Header destination address */
} __packed;

/**
 * ip_mtu_discover - IP MTU discover
 */
struct ip_mtu_discover {
    u32 imd_addr;               /* Address */
    u32 imd_mtu;                /* MTU */
} __packed;

/*===========================================================================*/
/*                         ROUTING TABLE STRUCTURES                          */
/*===========================================================================*/

/**
 * rt_entry - Routing table entry
 */
struct rt_entry {
    struct rt_entry *next;      /* Next entry in hash bucket */
    struct rt_entry *prev;      /* Previous entry in hash bucket */

    u32 dst;                    /* Destination address */
    u32 mask;                   /* Netmask */
    u32 gw;                     /* Gateway address */
    u32 src;                    /* Preferred source address */

    void *dev;                  /* Output device */
    u32 ifindex;                /* Interface index */

    u32 flags;                  /* Route flags */
    u32 mtu;                    /* Path MTU */
    u32 window;                 /* Window size */
    u32 irtt;                   /* Initial RTT */

    u32 refcnt;                 /* Reference count */
    u64 lastuse;                /* Last use time */
    u64 expires;                /* Expiration time */

    u32 error;                  /* Error code for reject routes */

    spinlock_t lock;            /* Entry lock */
};

/**
 * rt_table - Routing table
 */
struct rt_table {
    u32 id;                     /* Table ID */
    char name[32];              /* Table name */

    struct rt_entry *hash[256]; /* Hash buckets */
    spinlock_t lock;            /* Table lock */

    u32 entries;                /* Number of entries */
    u64 lookups;                /* Number of lookups */
    u64 hits;                   /* Number of hits */
};

/*===========================================================================*/
/*                         FRAGMENTATION STRUCTURES                          */
/*===========================================================================*/

/**
 * ip_frag - IP fragment
 */
struct ip_frag {
    struct ip_frag *next;       /* Next fragment */
    u32 offset;                 /* Fragment offset */
    u32 len;                    /* Fragment length */
    u32 end;                    /* End offset */
    struct sk_buff *skb;        /* Fragment data */
};

/**
 * ipq - IP fragment queue
 */
struct ipq {
    struct ipq *next;           /* Next queue in hash table */
    struct ipq *prev;           /* Previous queue in hash table */

    u32 id;                     /* Fragment ID */
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
    u8 protocol;                /* Protocol */
    u8 flags;                   /* Fragment flags */

    u32 len;                    /* Total length */
    u32 received;               /* Bytes received */
    u32 first_frag_offset;      /* First fragment offset */

    u64 expires;                /* Expiration time */

    struct ip_frag *fragments;  /* Fragment list */
    spinlock_t lock;            /* Queue lock */
};

/* Fragment Queue Flags */
#define IPQ_COMPLETE            0x0001  /* All fragments received */

/*===========================================================================*/
/*                         IP STATISTICS                                     */
/*===========================================================================*/

/**
 * ip_statistics - IP layer statistics
 */
struct ip_statistics {
    /* Received */
    atomic64_t in_receives;         /* Total IP packets received */
    atomic64_t in_hdr_errors;       /* Header errors */
    atomic64_t in_addr_errors;      /* Address errors */
    atomic64_t in_unknown_protos;   /* Unknown protocols */
    atomic64_t in_discards;         /* Discarded packets */
    atomic64_t in_delivers;         /* Delivered packets */

    /* Transmitted */
    atomic64_t out_requests;        /* Outgoing requests */
    atomic64_t out_discards;        /* Discarded packets */
    atomic64_t out_no_routes;       /* No route available */

    /* Fragmentation */
    atomic64_t reasm_reqds;         /* Reassembly required */
    atomic64_t reasm_oks;           /* Reassembly successful */
    atomic64_t reasm_fails;         /* Reassembly failed */
    atomic64_t reasm_timeout;       /* Reassembly timeout */
    atomic64_t frag_creates;        /* Fragments created */
    atomic64_t frag_oks;            /* Fragments sent successfully */
    atomic64_t frag_fails;          /* Fragmentation failed */

    /* Forwarding */
    atomic64_t forwards;            /* Packets forwarded */
};

/* Global IP statistics */
extern struct ip_statistics ip_stats;

/*===========================================================================*/
/*                         IP ADDRESS UTILITIES                              */
/*===========================================================================*/

/* Address Conversion Macros */
#define IP_ADDR(a, b, c, d)     (((u32)(a) << 24) | ((u32)(b) << 16) | \
                                 ((u32)(c) << 8) | (u32)(d))

#define IP_ADDR_BYTES(addr)     ((u8 *)(addr))
#define IP_ADDR_BYTE1(addr)     (((u8 *)(addr))[0])
#define IP_ADDR_BYTE2(addr)     (((u8 *)(addr))[1])
#define IP_ADDR_BYTE3(addr)     (((u8 *)(addr))[2])
#define IP_ADDR_BYTE4(addr)     (((u8 *)(addr))[3])

/* Address Comparison */
#define IP_ADDR_EQ(a, b)        ((a) == (b))
#define IP_ADDR_NE(a, b)        ((a) != (b))
#define IP_ADDR_ZERO(addr)      ((addr) == 0)
#define IP_ADDR_BCAST(addr)     ((addr) == IP_ADDR_BROADCAST)
#define IP_ADDR_LOOP(addr)      (((addr) & IP_ADDR(255, 0, 0, 0)) == IP_ADDR(127, 0, 0, 0))
#define IP_ADDR_MULTICAST(addr) (IP_MULTICAST(addr))
#define IP_ADDR_LOCAL(addr)     (IP_ADDR_LOOP(addr) || IP_ADDR_BCAST(addr))

/* Network/Host Conversion */
#define htonl(x)                __builtin_bswap32(x)
#define ntohl(x)                __builtin_bswap32(x)
#define htons(x)                __builtin_bswap16(x)
#define ntohs(x)                __builtin_bswap16(x)

/* Address Formatting */
#define IP_FMT                  "%u.%u.%u.%u"
#define IP_ARG(addr)            IP_ADDR_BYTE1(&(addr)), IP_ADDR_BYTE2(&(addr)), \
                                IP_ADDR_BYTE3(&(addr)), IP_ADDR_BYTE4(&(addr))

/*===========================================================================*/
/*                         IP HEADER MANIPULATION                            */
/*===========================================================================*/

/* Header Length */
#define ip_hdrlen(ip)           ((ip)->ihl * 4)
#define ip_optlen(ip)           (ip_hdrlen(ip) - IP_MIN_HDR_LEN)

/* Total Length */
#define ip_totlen(ip)           ntohs((ip)->tot_len)
#define ip_set_totlen(ip, len)  ((ip)->tot_len = htons(len))

/* Fragment Offset */
#define ip_frag_off(ip)         (ntohs((ip)->frag_off) & IP_OFFSET)
#define ip_frag_more(ip)        (ntohs((ip)->frag_off) & IP_MF)
#define ip_frag_dont(ip)        (ntohs((ip)->frag_off) & IP_DF)

/* Address Access */
#define ip_saddr(ip)            ((ip)->saddr)
#define ip_daddr(ip)            ((ip)->daddr)

/*===========================================================================*/
/*                         IP API                                            */
/*===========================================================================*/

/* Initialization */
void ip_init(void);
void ip_exit(void);

/* Packet Processing */
int ip_rcv(struct sk_buff *skb, void *dev);
int ip_local_deliver(struct sk_buff *skb);
int ip_forward(struct sk_buff *skb);

/* Output */
int ip_output(struct sk_buff *skb);
int ip_queue_xmit(struct sk_buff *skb, void *dst);
int ip_send_skb(struct sk_buff *skb);

/* Routing */
struct rt_entry *ip_route_output(u32 daddr, u32 saddr, u8 tos);
struct rt_entry *ip_route_input(struct sk_buff *skb);
void ip_rt_put(struct rt_entry *rt);
int ip_rt_add(u32 dst, u32 mask, u32 gw, void *dev, u32 flags);
int ip_rt_del(u32 dst, u32 mask);
void ip_rt_flush(void);

/* Fragmentation */
int ip_fragment(struct sk_buff *skb, struct sk_buff **frags, u32 mtu);
struct sk_buff *ip_defrag(struct sk_buff *skb);
void ip_frag_init(void);
void ip_frag_exit(void);

/* Address Utilities */
u32 ip_make_addr(u8 a, u8 b, u8 c, u8 d);
void ip_print_addr(u32 addr, char *buf, u32 len);
int ip_parse_addr(const char *str, u32 *addr);
int ip_validate_addr(u32 addr);

/* Checksum */
u16 ip_checksum(void *buf, u32 len, u32 sum);
u16 ip_compute_csum(void *buf, u32 len);
void ip_send_check(struct iphdr *ip);
int ip_check(struct iphdr *ip);

/* Statistics */
void ip_stats_print(void);
void ip_stats_reset(void);

/* Debugging */
void ip_debug_hdr(struct iphdr *ip);
void ip_debug_skb(struct sk_buff *skb);

#endif /* _NEXUS_IPV4_H */
