/*
 * NEXUS OS - Network Stack Core Header
 * net/core/net.h
 */

#ifndef _NEXUS_NET_H
#define _NEXUS_NET_H

#include "../../kernel/include/kernel.h"
#include "../../kernel/sync/sync.h"

/*===========================================================================*/
/*                         NETWORK CONSTANTS                                 */
/*===========================================================================*/

/* Network Protocol Families */
#define PF_UNSPEC       0
#define PF_INET         2       /* IPv4 */
#define PF_INET6        10      /* IPv6 */
#define PF_PACKET       17      /* Packet socket */
#define PF_NETLINK      16      /* Netlink socket */

/* Socket Types */
#define SOCK_STREAM     1       /* TCP */
#define SOCK_DGRAM      2       /* UDP */
#define SOCK_RAW        3       /* Raw socket */
#define SOCK_SEQPACKET  5       /* Sequenced packet */

/* Socket Options Levels */
#define SOL_SOCKET      1
#define SOL_IP          0
#define SOL_TCP         6
#define SOL_UDP         17

/* Socket Options */
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_LINGER       13
#define SO_TIMESTAMP    29

/* Address Families */
#define AF_UNSPEC       PF_UNSPEC
#define AF_INET         PF_INET
#define AF_INET6        PF_INET6
#define AF_PACKET       PF_PACKET
#define AF_NETLINK      PF_NETLINK

/* IP Protocols */
#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_RAW     255

/* Socket States */
#define SS_FREE         0       /* Not allocated */
#define SS_UNCONNECTED  1       /* Unconnected */
#define SS_CONNECTING   2       /* Connecting */
#define SS_CONNECTED    3       /* Connected */
#define SS_DISCONNECTING 4      /* Disconnecting */

/* Shutdown Modes */
#define SHUT_RD         0       /* Shutdown read */
#define SHUT_WR         1       /* Shutdown write */
#define SHUT_RDWR       2       /* Shutdown both */

/* Message Flags */
#define MSG_OOB         0x0001
#define MSG_PEEK        0x0002
#define MSG_DONTROUTE   0x0004
#define MSG_CTRUNC      0x0008
#define MSG_PROXY       0x0010
#define MSG_TRUNC       0x0020
#define MSG_DONTWAIT    0x0040
#define MSG_EOR         0x0080
#define MSG_MORE        0x8000
#define MSG_WAITALL     0x0100

/* Network Byte Order */
#define htons(x)        __builtin_bswap16(x)
#define ntohs(x)        __builtin_bswap16(x)
#define htonl(x)        __builtin_bswap32(x)
#define ntohl(x)        __builtin_bswap32(x)

/* Maximum Values */
#define MAX_SOCKETS     65536
#define MAX_LISTEN_BACKLOG  128
#define MAX_SKBUFF_ORDER    10
#define NET_MAX_MTU     65535
#define NET_MIN_MTU     68

/* Socket Buffer Sizes */
#define SOCK_MIN_SNDBUF     2048
#define SOCK_MIN_RCVBUF     256
#define SOCK_MAX_SNDBUF     (16 * 1024 * 1024)
#define SOCK_MAX_RCVBUF     (16 * 1024 * 1024)
#define SOCK_DEFAULT_SNDBUF (128 * 1024)
#define SOCK_DEFAULT_RCVBUF (128 * 1024)

/* Network Statistics */
#define NET_STAT_INC(stat)      atomic_inc(&net_stats.stat)
#define NET_STAT_DEC(stat)      atomic_dec(&net_stats.stat)
#define NET_STAT_ADD(stat, n)   atomic_add((n), &net_stats.stat)
#define NET_STAT_SUB(stat, n)   atomic_sub((n), &net_stats.stat)

/*===========================================================================*/
/*                         NETWORK ADDRESS STRUCTURES                        */
/*===========================================================================*/

/**
 * sockaddr - Socket address structure
 */
struct sockaddr {
    u16 sa_family;              /* Address family */
    char sa_data[14];           /* Address data */
} __packed;

/**
 * sockaddr_in - IPv4 socket address structure
 */
struct sockaddr_in {
    u16 sin_family;             /* Address family (AF_INET) */
    u16 sin_port;               /* Port number (network byte order) */
    u32 sin_addr;               /* IPv4 address (network byte order) */
    u8 sin_zero[8];             /* Padding */
} __packed;

/**
 * in_addr - IPv4 address structure
 */
struct in_addr {
    u32 s_addr;                 /* IPv4 address */
} __packed;

/**
 * in6_addr - IPv6 address structure
 */
struct in6_addr {
    union {
        u8 u6_addr8[16];
        u16 u6_addr16[8];
        u32 u6_addr32[4];
    } u6_addr;
} __packed;

/**
 * sockaddr_in6 - IPv6 socket address structure
 */
struct sockaddr_in6 {
    u16 sin6_family;            /* Address family (AF_INET6) */
    u16 sin6_port;              /* Port number */
    u32 sin6_flowinfo;          /* IPv6 flow information */
    struct in6_addr sin6_addr;  /* IPv6 address */
    u32 sin6_scope_id;          /* Scope ID */
} __packed;

/**
 * sockaddr_storage - Large enough for any address family
 */
struct sockaddr_storage {
    u16 ss_family;              /* Address family */
    u8 __ss_pad[126];           /* Padding to ensure size */
} __aligned(8);

/*===========================================================================*/
/*                         SOCKET BUFFER STRUCTURES                          */
/*===========================================================================*/

/**
 * sk_buff - Socket buffer (packet data)
 *
 * Core data structure for network packet handling. Each packet traversing
 * the network stack is represented by an sk_buff.
 */
struct sk_buff {
    /* Linked List Pointers */
    struct sk_buff *next;       /* Next buffer in list */
    struct sk_buff *prev;       /* Previous buffer in list */
    struct list_head list;      /* Generic list entry */

    /* Socket Association */
    void *sk;                   /* Associated socket */

    /* Timing Information */
    u64 tstamp;                 /* Timestamp */
    u32 tstamp_type;            /* Timestamp type */

    /* Buffer Information */
    u8 *head;                   /* Head of buffer */
    u8 *data;                   /* Start of data */
    u32 len;                    /* Length of data */
    u32 data_len;               /* Length of fragmented data */
    u32 tail;                   /* End of data */
    u32 end;                    /* End of buffer */
    u32 truesize;               /* True size of buffer */

    /* Reference Count */
    atomic_t users;             /* Reference count */
    atomic_t dataref;           /* Data reference count */

    /* Network Layer Information */
    u16 protocol;               /* Protocol type */
    u16 mac_len;                /* MAC header length */
    u16 network_header;         /* Network header offset */
    u16 transport_header;       /* Transport header offset */

    /* Packet Information */
    u32 mark;                   /* Packet mark */
    u32 priority;               /* Packet priority */
    u32 hash;                   /* Packet hash */
    u16 vlan_proto;             /* VLAN protocol */
    u16 vlan_tci;               /* VLAN TCI */

    /* Device Information */
    void *dev;                  /* Network device */
    u32 ifindex;                /* Interface index */

    /* Control Buffer */
    u8 cb[48] __aligned(8);     /* Control buffer for protocol use */

    /* Flags */
    u32 pkt_type:3;             /* Packet type */
    u32 fclone:2;               /* Clone type */
    u32 ip_summed:2;            /* Checksum status */
    u32 ooo_okay:1;             /* Out-of-order okay */
    u32 l4_hash:1;              /* Layer 4 hash */
    u32 sw_hash:1;              /* Software hash */
    u32 wifi_acked_valid:1;     /* WiFi ACK valid */
    u32 wifi_acked:1;           /* WiFi ACKed */
    u32 no_fcs:1;               /* No FCS */
    u32 encapsoding:1;          /* Encapsulation */
    u32 csum_valid:1;           /* Checksum valid */
    u32 csum_complete_sw:1;     /* Checksum complete SW */
    u32 csum_level:2;           /* Checksum level */
    u32 csum_not_inet:1;        /* Checksum not inet */
    u32 dst_pending_confirm:1;  /* DST pending confirm */
    u32 ipvs_property:1;        /* IPVS property */
    u32 peeked:1;               /* Packet peeked */
    u32 nf_trace:1;             /* Netfilter trace */
    u32 nfctinfo:3;             /* Netfilter conntrack info */
    u32 nfct:1;                 /* Netfilter conntrack */
    u32 secmark:1;              /* Security mark */
    u32 offload_fwd_mark:1;     /* Offload forward mark */
    u32 tc_skip_classify:1;     /* TC skip classify */
    u32 tc_at_ingress:1;        /* TC at ingress */
    u32 tc_redirected:1;        /* TC redirected */
    u32 tc_from_ingress:1;      /* TC from ingress */
    u32 slow_gso:1;             /* Slow GSO */
    u32 encapsulation:1;        /* Encapsulation */
    u32 gro_normal:1;           /* GRO normal */

    /* Destructor */
    void (*destructor)(struct sk_buff *skb);
};

/**
 * sk_buff_head - Socket buffer queue head
 */
struct sk_buff_head {
    struct sk_buff *next;       /* Next buffer */
    struct sk_buff *prev;       /* Previous buffer */
    struct sk_buff *last;       /* Last buffer */
    spinlock_t lock;            /* Queue lock */
    atomic_t qlen;              /* Queue length */
    u32 flags;                  /* Queue flags */
};

/*===========================================================================*/
/*                         SOCKET STRUCTURES                                 */
/*===========================================================================*/

/**
 * socket - Generic socket structure
 */
struct socket {
    struct socket *next;        /* Next socket in list */
    struct socket *prev;        /* Previous socket in list */

    /* Socket State */
    u32 state;                  /* Socket state */
    u32 type;                   /* Socket type */
    u32 flags;                  /* Socket flags */
    u32 mark;                   /* Socket mark */

    /* Protocol Information */
    u16 family;                 /* Address family */
    u16 protocol;               /* Protocol */

    /* File Descriptor */
    int fd;                     /* File descriptor */

    /* Socket Operations */
    struct socket_ops *ops;     /* Socket operations */

    /* Socket Buffer */
    struct sk_buff_head receive_queue;  /* Receive queue */
    struct sk_buff_head write_queue;    /* Write queue */
    struct sk_buff_head error_queue;    /* Error queue */

    /* Socket Buffers */
    atomic_t sk_wmem_alloc;     /* Write memory allocated */
    atomic_t sk_rmem_alloc;     /* Read memory allocated */
    u32 sk_sndbuf;              /* Send buffer size */
    u32 sk_rcvbuf;              /* Receive buffer size */
    u32 sk_rcvlowat;            /* Receive low watermark */
    u32 sk_sndlowat;            /* Send low watermark */
    u32 sk_rcvtimeo;            /* Receive timeout */
    u32 sk_sndtimeo;            /* Send timeout */

    /* Socket Lock */
    spinlock_t lock;            /* Socket lock */

    /* Wait Queues */
    wait_queue_head_t wait;     /* Wait queue */
    wait_queue_head_t sk_wait;  /* Socket wait queue */

    /* Socket Errors */
    atomic_t sk_err;            /* Socket error */
    atomic_t sk_err_soft;       /* Soft socket error */
    u32 sk_ack_backlog;         /* ACK backlog */

    /* Socket Address */
    struct sockaddr_in local_addr;      /* Local address */
    struct sockaddr_in remote_addr;     /* Remote address */

    /* Socket Statistics */
    u64 packets_in;             /* Packets received */
    u64 packets_out;            /* Packets sent */
    u64 bytes_in;               /* Bytes received */
    u64 bytes_out;              /* Bytes sent */
    u64 errors_in;              /* Receive errors */
    u64 errors_out;             /* Send errors */

    /* Private Data */
    void *sk_protinfo;          /* Protocol private data */
};

/**
 * socket_ops - Socket operations structure
 */
struct socket_ops {
    u16 family;                 /* Address family */
    u16 type;                   /* Socket type */
    u16 protocol;               /* Protocol */
    const char *name;           /* Operations name */

    /* Socket Lifecycle */
    int (*create)(struct socket *sock, int protocol);
    int (*release)(struct socket *sock);
    int (*bind)(struct socket *sock, struct sockaddr *addr, int addr_len);
    int (*connect)(struct socket *sock, struct sockaddr *addr, int addr_len, int flags);
    int (*listen)(struct socket *sock, int backlog);
    int (*accept)(struct socket *sock, struct socket *newsock, int flags);

    /* Socket I/O */
    int (*sendmsg)(struct socket *sock, void *msg, size_t len, int flags);
    int (*recvmsg)(struct socket *sock, void *msg, size_t len, int flags);
    int (*sendpage)(struct socket *sock, void *page, int offset, size_t size, int flags);

    /* Socket Control */
    int (*shutdown)(struct socket *sock, int how);
    int (*setsockopt)(struct socket *sock, int level, int optname, void *optval, int optlen);
    int (*getsockopt)(struct socket *sock, int level, int optname, void *optval, int *optlen);
    int (*ioctl)(struct socket *sock, int cmd, unsigned long arg);
    int (*poll)(struct socket *sock, wait_queue_head_t *wait);

    /* Socket Address */
    int (*getname)(struct socket *sock, struct sockaddr *addr, int *addr_len, int peer);
};

/*===========================================================================*/
/*                         NETWORK DEVICE STRUCTURES                         */
/*===========================================================================*/

/**
 * net_device_stats - Network device statistics
 */
struct net_device_stats {
    u64 rx_packets;             /* Received packets */
    u64 tx_packets;             /* Transmitted packets */
    u64 rx_bytes;               /* Received bytes */
    u64 tx_bytes;               /* Transmitted bytes */
    u64 rx_errors;              /* Receive errors */
    u64 tx_errors;              /* Transmit errors */
    u64 rx_dropped;             /* Receive dropped */
    u64 tx_dropped;             /* Transmit dropped */
    u64 multicast;              /* Multicast packets */
    u64 collisions;             /* Collisions */
    u64 rx_length_errors;       /* Receive length errors */
    u64 rx_over_errors;         /* Receive over errors */
    u64 rx_crc_errors;          /* Receive CRC errors */
    u64 rx_frame_errors;        /* Receive frame errors */
    u64 rx_fifo_errors;         /* Receive FIFO errors */
    u64 rx_missed_errors;       /* Receive missed errors */
    u64 tx_aborted_errors;      /* Transmit aborted errors */
    u64 tx_carrier_errors;      /* Transmit carrier errors */
    u64 tx_fifo_errors;         /* Transmit FIFO errors */
    u64 tx_heartbeat_errors;    /* Transmit heartbeat errors */
    u64 tx_window_errors;       /* Transmit window errors */
};

/**
 * net_device_ops - Network device operations
 */
struct net_device_ops {
    int (*ndo_init)(void *dev);
    void (*ndo_uninit)(void *dev);
    int (*ndo_open)(void *dev);
    int (*ndo_stop)(void *dev);

    /* Packet Transmission */
    int (*ndo_start_xmit)(struct sk_buff *skb, void *dev);

    /* Configuration */
    int (*ndo_change_mtu)(void *dev, int new_mtu);
    int (*ndo_change_addr)(void *dev, u8 *addr);

    /* Statistics */
    struct net_device_stats *(*ndo_get_stats)(void *dev);

    /* VLAN */
    int (*ndo_vlan_rx_add_vid)(void *dev, u16 vid);
    int (*ndo_vlan_rx_kill_vid)(void *dev, u16 vid);

    /* Timeout */
    void (*ndo_tx_timeout)(void *dev);

    /* Features */
    u32 (*ndo_fix_features)(void *dev, u32 features);
    int (*ndo_set_features)(void *dev, u32 features);
};

/**
 * net_device - Network device structure
 */
struct net_device {
    struct net_device *next;    /* Next device in list */
    struct net_device *prev;    /* Previous device in list */

    /* Device Information */
    char name[16];              /* Device name */
    u32 ifindex;                /* Interface index */
    u32 iftype;                 /* Interface type */
    u32 flags;                  /* Device flags */
    u32 features;               /* Device features */

    /* Hardware Address */
    u16 addr_len;               /* Address length */
    u8 dev_addr[32];            /* Device MAC address */
    u8 broadcast[32];           /* Broadcast address */

    /* MTU */
    u32 mtu;                    /* Maximum Transmission Unit */
    u32 min_mtu;                /* Minimum MTU */
    u32 max_mtu;                /* Maximum MTU */

    /* Queue Information */
    u16 num_tx_queues;          /* Number of TX queues */
    u16 num_rx_queues;          /* Number of RX queues */
    u16 real_num_tx_queues;     /* Real TX queues */
    u16 real_num_rx_queues;     /* Real RX queues */

    /* Transmit Queue */
    struct sk_buff_head tx_queue;       /* Transmit queue */
    spinlock_t tx_lock;         /* Transmit lock */

    /* Receive Queue */
    struct sk_buff_head rx_queue;       /* Receive queue */
    spinlock_t rx_lock;         /* Receive lock */

    /* Device Operations */
    struct net_device_ops *ops; /* Device operations */
    void *priv;                 /* Private data */

    /* Statistics */
    struct net_device_stats stats;      /* Device statistics */
    struct net_device_stats *pcpu_stats;  /* Per-CPU statistics */

    /* Wait Queue */
    wait_queue_head_t waitq;    /* Wait queue */

    /* Reference Count */
    atomic_t refcnt;            /* Reference count */

    /* State */
    u32 state;                  /* Device state */
    u32 reg_state;              /* Registration state */

    /* Watchdog */
    u32 watchdog_timeo;         /* Watchdog timeout */
    u64 last_tx_time;           /* Last transmit time */

    /* Private Data */
    u8 priv_data[0] __aligned(32);      /* Private data area */
};

/* Network Device Flags */
#define IFF_UP          0x00000001  /* Interface is up */
#define IFF_BROADCAST   0x00000002  /* Broadcast address valid */
#define IFF_DEBUG       0x00000004  /* Turn on debugging */
#define IFF_LOOPBACK    0x00000008  /* Is a loopback net */
#define IFF_POINTOPOINT 0x00000010  /* Is a point-to-point link */
#define IFF_NOTRAILERS  0x00000020  /* Avoid use of trailers */
#define IFF_RUNNING     0x00000040  /* Resources allocated */
#define IFF_NOARP       0x00000080  /* No ARP protocol */
#define IFF_PROMISC     0x00000100  /* Receive all packets */
#define IFF_ALLMULTI    0x00000200  /* Receive all multicast */
#define IFF_MASTER      0x00000400  /* Master of a load balancer */
#define IFF_SLAVE       0x00000800  /* Slave of a load balancer */
#define IFF_MULTICAST   0x00001000  /* Supports multicast */
#define IFF_PORTSEL     0x00002000  /* Can set media type */
#define IFF_AUTOMEDIA   0x00004000  /* Auto media select */
#define IFF_DYNAMIC     0x00008000  /* Dynamic address */
#define IFF_LOWER_UP    0x00010000  /* Driver signals L1 up */
#define IFF_DORMANT     0x00020000  /* Driver signals dormant */
#define IFF_ECHO        0x00040000  /* Echo send packets */

/* Network Device Features */
#define NETIF_F_SG              0x00000001  /* Scatter/Gather IO */
#define NETIF_F_IP_CSUM         0x00000002  /* IP checksum */
#define NETIF_F_NO_CSUM         0x00000004  /* Device checksums OK */
#define NETIF_F_HW_CSUM         0x00000008  /* Device does all checksums */
#define NETIF_F_TSO             0x00000010  /* TCP segmentation offload */
#define NETIF_F_UFO             0x00000020  /* UDP fragmentation offload */
#define NETIF_F_GSO             0x00000040  /* Generic segmentation offload */
#define NETIF_F_GRO             0x00000080  /* Generic receive offload */
#define NETIF_F_VLAN_CHALLENGED 0x00000100  /* VLAN challenged */
#define NETIF_F_HIGHDMA         0x00000200  /* High DMA */
#define NETIF_F_FRAGLIST        0x00000400  /* Fraglist support */
#define NETIF_F_SCTP_CSUM       0x00000800  /* SCTP checksum */

/* Network Device States */
#define NET_DEVICE_STATE_UNINITIALIZED  0
#define NET_DEVICE_STATE_REGISTERED     1
#define NET_DEVICE_STATE_UNREGISTERING  2
#define NET_DEVICE_STATE_RELEASED       3

/*===========================================================================*/
/*                         NETWORK STATISTICS                                */
/*===========================================================================*/

/**
 * net_statistics - Global network statistics
 */
struct net_statistics {
    /* Socket Statistics */
    atomic_t sockets_allocated;   /* Allocated sockets */
    atomic_t sockets_in_use;      /* Sockets in use */
    atomic_t sockets_bound;       /* Bound sockets */
    atomic_t sockets_connected;   /* Connected sockets */
    atomic_t sockets_listening;   /* Listening sockets */

    /* Packet Statistics */
    atomic64_t packets_in;        /* Packets received */
    atomic64_t packets_out;       /* Packets transmitted */
    atomic64_t bytes_in;          /* Bytes received */
    atomic64_t bytes_out;         /* Bytes transmitted */

    /* Error Statistics */
    atomic64_t errors_in;         /* Receive errors */
    atomic64_t errors_out;        /* Transmit errors */
    atomic64_t drops_in;          /* Receive drops */
    atomic64_t drops_out;         /* Transmit drops */

    /* Socket Buffer Statistics */
    atomic_t skbuffs_allocated;   /* Allocated skbuffs */
    atomic_t skbuffs_in_use;      /* Skbuffs in use */
    atomic_t skbuffs_cloned;      /* Cloned skbuffs */
    atomic64_t skbuff_memory;     /* Skbuff memory usage */

    /* Protocol Statistics */
    atomic64_t tcp_segments_in;   /* TCP segments received */
    atomic64_t tcp_segments_out;  /* TCP segments sent */
    atomic64_t udp_datagrams_in;  /* UDP datagrams received */
    atomic64_t udp_datagrams_out; /* UDP datagrams sent */
    atomic64_t icmp_in;           /* ICMP messages received */
    atomic64_t icmp_out;          /* ICMP messages sent */

    /* IP Statistics */
    atomic64_t ip_packets_in;     /* IP packets received */
    atomic64_t ip_packets_out;    /* IP packets sent */
    atomic64_t ip_fragments_in;   /* IP fragments received */
    atomic64_t ip_fragments_out;  /* IP fragments sent */
    atomic64_t ip_reassembled;    /* IP packets reassembled */
};

/* Global network statistics */
extern struct net_statistics net_stats;

/*===========================================================================*/
/*                         SOCKET BUFFER API                                 */
/*===========================================================================*/

/* Initialization */
void skbuff_init(void);
void skbuff_exit(void);

/* Allocation */
struct sk_buff *alloc_skb(u32 size);
struct sk_buff *alloc_skb_fclone(u32 size);
struct sk_buff *dev_alloc_skb(u32 size);
void free_skb(struct sk_buff *skb);

/* Reference Counting */
struct sk_buff *skb_get(struct sk_buff *skb);
bool skb_cloned(const struct sk_buff *skb);
struct sk_buff *skb_clone(struct sk_buff *skb);
struct sk_buff *skb_copy(const struct sk_buff *skb);

/* Data Manipulation */
u8 *skb_push(struct sk_buff *skb, u32 len);
u8 *skb_pull(struct sk_buff *skb, u32 len);
u8 *skb_put(struct sk_buff *skb, u32 len);
u8 *skb_reserve(struct sk_buff *skb, u32 len);
void skb_reset(struct sk_buff *skb);
void skb_trim(struct sk_buff *skb, u32 len);

/* Queue Operations */
void skb_queue_head(struct sk_buff_head *list, struct sk_buff *skb);
void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *skb);
struct sk_buff *skb_dequeue(struct sk_buff_head *list);
struct sk_buff *skb_peek(struct sk_buff_head *list);
struct sk_buff *skb_dequeue_tail(struct sk_buff_head *list);
u32 skb_queue_len(const struct sk_buff_head *list);
bool skb_queue_empty(const struct sk_buff_head *list);

/* Checksum */
u32 skb_checksum(const struct sk_buff *skb, int offset, int len, u32 sum);
u32 skb_copy_and_csum_bits(const struct sk_buff *skb, int offset, u8 *to, int len, u32 sum);
void skb_copy_checksum(struct sk_buff *to, const struct sk_buff *from, int offset, int len);

/* Fragmentation */
int skb_split(struct sk_buff *skb, struct sk_buff *skb1, u32 len);
int skb_grow(struct sk_buff *skb, u32 new_len);
int skb_linearize(struct sk_buff *skb);

/* Miscellaneous */
u32 skb_tailroom(const struct sk_buff *skb);
u32 skb_headroom(const struct sk_buff *skb);
u32 skb_availroom(const struct sk_buff *skb);
void skb_orphan(struct sk_buff *skb);
void skb_dst_set(struct sk_buff *skb, void *dst);
void *skb_dst(const struct sk_buff *skb);

/*===========================================================================*/
/*                         SOCKET API                                        */
/*===========================================================================*/

/* Initialization */
void socket_init(void);
void socket_exit(void);

/* Socket Creation */
int sock_create(int family, int type, int protocol, struct socket **res);
int sock_create_kern(int family, int type, int protocol, struct socket **res);
void sock_release(struct socket *sock);

/* Socket Operations */
int sock_bind(struct socket *sock, struct sockaddr *addr, int addr_len);
int sock_connect(struct socket *sock, struct sockaddr *addr, int addr_len);
int sock_listen(struct socket *sock, int backlog);
int sock_accept(struct socket *sock, struct socket *newsock, int flags);
int sock_shutdown(struct socket *sock, int how);

/* Socket I/O */
int sock_sendmsg(struct socket *sock, void *msg, size_t len, int flags);
int sock_recvmsg(struct socket *sock, void *msg, size_t len, int flags);

/* Socket Control */
int sock_setsockopt(struct socket *sock, int level, int optname, void *optval, int optlen);
int sock_getsockopt(struct socket *sock, int level, int optname, void *optval, int *optlen);
int sock_ioctl(struct socket *sock, int cmd, unsigned long arg);
int sock_poll(struct socket *sock, wait_queue_head_t *wait);

/* Socket Address */
int sock_getname(struct socket *sock, struct sockaddr *addr, int *addr_len, int peer);

/* Socket Buffer Management */
int sock_queue_rcv_skb(struct socket *sock, struct sk_buff *skb);
struct sk_buff *sock_dequeue_rcv_skb(struct socket *sock);
bool sock_writeable(const struct socket *sock);
bool sock_readable(const struct socket *sock);

/* Socket Memory */
void sock_set_sndbuf(struct socket *sock, u32 size);
void sock_set_rcvbuf(struct socket *sock, u32 size);
void sock_set_timeout(struct socket *sock, u32 rcvtimeo, u32 sndtimeo);

/* Socket Utilities */
int sock_error(struct socket *sock);
void sock_clear_error(struct socket *sock);
u32 sock_wspace(struct socket *sock);
u32 sock_rspace(struct socket *sock);

/*===========================================================================*/
/*                         NETWORK DEVICE API                                */
/*===========================================================================*/

/* Initialization */
void net_device_init(void);
void net_device_exit(void);

/* Device Registration */
int register_netdevice(struct net_device *dev);
int unregister_netdevice(struct net_device *dev);
void free_netdev(struct net_device *dev);
struct net_device *alloc_netdev(int sizeof_priv, const char *name, void (*setup)(struct net_device *));

/* Device Lookup */
struct net_device *dev_get_by_name(const char *name);
struct net_device *dev_get_by_index(u32 ifindex);
struct net_device *dev_get_first(void);
struct net_device *dev_get_next(struct net_device *dev);

/* Device Operations */
int dev_open(struct net_device *dev);
int dev_close(struct net_device *dev);
int dev_change_mtu(struct net_device *dev, int new_mtu);
int dev_change_addr(struct net_device *dev, u8 *addr);

/* Packet Transmission */
int dev_queue_xmit(struct sk_buff *skb);
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
void dev_transmit_complete(struct net_device *dev, struct sk_buff *skb, int status);

/* Packet Reception */
int netif_receive_skb(struct sk_buff *skb);
int netif_rx(struct sk_buff *skb);
int netif_rx_ni(struct sk_buff *skb);

/* Device Statistics */
struct net_device_stats *dev_get_stats(struct net_device *dev);
void dev_update_stats(struct net_device *dev);

/* Device State */
bool netif_running(const struct net_device *dev);
bool netif_carrier_ok(const struct net_device *dev);
void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);
void netif_start_queue(struct net_device *dev);
void netif_stop_queue(struct net_device *dev);
void netif_wake_queue(struct net_device *dev);

/* Device Features */
u32 netif_get_features(struct net_device *dev);
int netif_set_features(struct net_device *dev, u32 features);

/* Device Reference Counting */
struct net_device *dev_hold(struct net_device *dev);
void dev_put(struct net_device *dev);

/*===========================================================================*/
/*                         NETWORK CORE API                                  */
/*===========================================================================*/

/* Initialization */
void net_core_init(void);
void net_core_exit(void);

/* Network Stack State */
bool net_is_initialized(void);
void net_synchronize(void);

/* Network Statistics */
void net_stats_print(void);
void net_stats_reset(void);

/* Network Utilities */
u32 net_random(void);
void net_srandom(u32 seed);

/* Debugging */
void net_debug_skb(struct sk_buff *skb);
void net_debug_socket(struct socket *sock);
void net_debug_device(struct net_device *dev);

#endif /* _NEXUS_NET_H */
