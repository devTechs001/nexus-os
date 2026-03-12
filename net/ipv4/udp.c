/*
 * NEXUS OS - UDP Implementation
 * net/ipv4/udp.c
 */

#include "ipv4.h"

/*===========================================================================*/
/*                         UDP CONFIGURATION                                 */
/*===========================================================================*/

#define UDP_HASH_BITS           8
#define UDP_HASH_SIZE           (1 << UDP_HASH_BITS)
#define UDP_MAX_QUEUE_LEN       256
#define UDP_MIN_PORT            1024
#define UDP_MAX_PORT            65535

/* UDP Flags */
#define UDP_FLAG_CSUM_ENABLED   0x0001
#define UDP_FLAG_CSUM_NOXMIT    0x0002
#define UDP_FLAG_NO_CHECK       0x0004

/*===========================================================================*/
/*                         UDP STRUCTURES                                    */
/*===========================================================================*/

/**
 * udphdr - UDP header structure
 */
struct udphdr {
    u16 source;                 /* Source port */
    u16 dest;                   /* Destination port */
    u16 len;                    /* Length (header + data) */
    u16 check;                  /* Checksum */
} __packed;

/**
 * udp_sock - UDP socket
 */
struct udp_sock {
    struct socket *sock;        /* Parent socket */

    /* Addresses */
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
    u16 sport;                  /* Source port */
    u16 dport;                  /* Destination port */

    /* Socket Options */
    u32 flags;                  /* UDP flags */
    u32 recv_saddr;             /* Receive source address */
    u32 recv_daddr;             /* Receive destination address */

    /* Statistics */
    u64 datagrams_in;           /* Datagrams received */
    u64 datagrams_out;          /* Datagrams sent */
    u64 bytes_in;               /* Bytes received */
    u64 bytes_out;              /* Bytes sent */
    u64 errors;                 /* Errors */

    /* Lock */
    spinlock_t lock;            /* Socket lock */
};

/**
 * udp_table - UDP hash table
 */
struct udp_table {
    struct udp_sock *hash[UDP_HASH_SIZE];
    spinlock_t lock;
    u32 entries;
};

/*===========================================================================*/
/*                         UDP GLOBAL DATA                                   */
/*===========================================================================*/

/* UDP Statistics */
static struct {
    spinlock_t lock;
    atomic64_t datagrams_in;
    atomic64_t datagrams_out;
    atomic64_t receive_errors;
    atomic64_t send_errors;
    atomic64_t no_ports;        /* No port available */
} udp_stats = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .datagrams_in = ATOMIC64_INIT(0),
    .datagrams_out = ATOMIC64_INIT(0),
    .receive_errors = ATOMIC64_INIT(0),
    .send_errors = ATOMIC64_INIT(0),
    .no_ports = ATOMIC64_INIT(0),
};

/* UDP Hash Table */
static struct udp_table udp_table = {
    .hash = {NULL},
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .entries = 0,
};

/*===========================================================================*/
/*                         UDP HASH FUNCTIONS                                */
/*===========================================================================*/

/**
 * udp_hash_fn - Hash function for UDP sockets
 * @port: Port number
 *
 * Returns: Hash value
 */
static inline u32 udp_hash_fn(u16 port)
{
    return port & (UDP_HASH_SIZE - 1);
}

/**
 * udp_hash_add - Add UDP socket to hash table
 * @up: UDP socket
 */
static void udp_hash_add(struct udp_sock *up)
{
    u32 hash;

    if (up->sport == 0) {
        return;
    }

    hash = udp_hash_fn(ntohs(up->sport));

    spin_lock(&udp_table.lock);

    up->sock->next = (struct socket *)udp_table.hash[hash];
    up->sock->prev = NULL;
    if (udp_table.hash[hash]) {
        ((struct udp_sock *)udp_table.hash[hash])->sock->prev = up->sock;
    }
    udp_table.hash[hash] = up;
    udp_table.entries++;

    spin_unlock(&udp_table.lock);
}

/**
 * udp_hash_del - Remove UDP socket from hash table
 * @up: UDP socket
 */
static void udp_hash_del(struct udp_sock *up)
{
    u32 hash;

    if (up->sport == 0) {
        return;
    }

    hash = udp_hash_fn(ntohs(up->sport));

    spin_lock(&udp_table.lock);

    if (up->sock->prev) {
        ((struct udp_sock *)up->sock->prev)->sock->next = up->sock->next;
    } else {
        udp_table.hash[hash] = (struct udp_sock *)up->sock->next;
    }

    if (up->sock->next) {
        ((struct udp_sock *)up->sock->next)->sock->prev = up->sock->prev;
    }

    up->sock->next = NULL;
    up->sock->prev = NULL;
    udp_table.entries--;

    spin_unlock(&udp_table.lock);
}

/**
 * udp_hash_find - Find UDP socket in hash table
 * @port: Port number
 * @addr: Address (0 for any)
 *
 * Returns: UDP socket, or NULL if not found
 */
static struct udp_sock *udp_hash_find(u16 port, u32 addr)
{
    struct udp_sock *up;
    u32 hash;

    hash = udp_hash_fn(port);

    spin_lock(&udp_table.lock);

    up = udp_table.hash[hash];
    while (up) {
        if (up->sport == port) {
            if (addr == 0 || up->saddr == 0 || up->saddr == addr) {
                spin_unlock(&udp_table.lock);
                return up;
            }
        }
        up = (struct udp_sock *)up->sock->next;
    }

    spin_unlock(&udp_table.lock);
    return NULL;
}

/*===========================================================================*/
/*                         UDP CHECKSUM                                      */
/*===========================================================================*/

/**
 * udp_checksum - Compute UDP checksum
 * @iph: IP header
 * @udph: UDP header
 * @len: UDP datagram length
 *
 * Returns: 16-bit checksum
 */
static u16 udp_checksum(struct iphdr *iph, struct udphdr *udph, u32 len)
{
    u32 sum = 0;
    u32 pseudo_header[3];

    /* Pseudo header */
    pseudo_header[0] = iph->saddr;
    pseudo_header[1] = iph->daddr;
    pseudo_header[2] = htonl((IPPROTO_UDP << 16) + len);

    /* Sum pseudo header */
    sum += (pseudo_header[0] >> 16) & 0xFFFF;
    sum += pseudo_header[0] & 0xFFFF;
    sum += (pseudo_header[1] >> 16) & 0xFFFF;
    sum += pseudo_header[1] & 0xFFFF;
    sum += (pseudo_header[2] >> 16) & 0xFFFF;
    sum += pseudo_header[2] & 0xFFFF;

    /* Sum UDP header and data */
    sum = ip_checksum(udph, len, sum);

    /* Convert 0 to 0xFFFF for UDP */
    return sum == 0 ? 0xFFFF : sum;
}

/**
 * udp_send_check - Compute and set UDP checksum
 * @iph: IP header
 * @udph: UDP header
 * @len: UDP datagram length
 */
static void udp_send_check(struct iphdr *iph, struct udphdr *udph, u32 len)
{
    udph->check = 0;
    udph->check = udp_checksum(iph, udph, len);
}

/*===========================================================================*/
/*                         UDP SOCKET ALLOCATION                             */
/*===========================================================================*/

/**
 * udp_alloc_sock - Allocate UDP socket
 *
 * Returns: UDP socket, or NULL on failure
 */
static struct udp_sock *udp_alloc_sock(void)
{
    struct udp_sock *up;

    up = kzalloc(sizeof(struct udp_sock));
    if (!up) {
        return NULL;
    }

    /* Initialize flags */
    up->flags = UDP_FLAG_CSUM_ENABLED;

    /* Initialize lock */
    spin_lock_init_named(&up->lock, "udp_sock");

    return up;
}

/**
 * udp_free_sock - Free UDP socket
 * @up: UDP socket
 */
static void udp_free_sock(struct udp_sock *up)
{
    if (!up) {
        return;
    }

    kfree(up);
}

/*===========================================================================*/
/*                         UDP PORT ALLOCATION                               */
/*===========================================================================*/

/**
 * udp_port_alloc - Allocate UDP port
 *
 * Returns: Port number in network byte order, or 0 on failure
 */
static u16 udp_port_alloc(void)
{
    u16 port;
    u32 attempts = 0;

    do {
        /* Generate random port */
        port = htons(UDP_MIN_PORT + (net_random() % (UDP_MAX_PORT - UDP_MIN_PORT)));

        /* Check if port is available */
        if (!udp_hash_find(port, 0)) {
            return port;
        }

        attempts++;
    } while (attempts < 100);

    atomic64_inc(&udp_stats.no_ports);
    return 0;
}

/*===========================================================================*/
/*                         UDP DATAGRAM CREATION                             */
/*===========================================================================*/

/**
 * udp_build_datagram - Build UDP datagram
 * @up: UDP socket
 * @data: Data buffer
 * @len: Data length
 * @daddr: Destination address
 * @dport: Destination port
 *
 * Returns: Socket buffer, or NULL on failure
 */
static struct sk_buff *udp_build_datagram(struct udp_sock *up, void *data,
                                           u32 len, u32 daddr, u16 dport)
{
    struct sk_buff *skb;
    struct udphdr *uh;
    struct iphdr *iph;
    u32 udp_len;

    /* Calculate total length */
    udp_len = sizeof(struct udphdr) + len;

    /* Allocate buffer */
    skb = alloc_skb(udp_len + sizeof(struct iphdr));
    if (!skb) {
        return NULL;
    }

    /* Copy data */
    if (data && len > 0) {
        memcpy(skb_put(skb, len), data, len);
    }

    /* Build UDP header */
    uh = (struct udphdr *)skb_push(skb, sizeof(struct udphdr));
    uh->source = up->sport;
    uh->dest = dport;
    uh->len = htons(udp_len);

    /* Build IP header */
    iph = (struct iphdr *)skb_push(skb, sizeof(struct iphdr));
    memset(iph, 0, sizeof(struct iphdr));

    iph->version = IP_VERSION_4;
    iph->ihl = sizeof(struct iphdr) / 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + udp_len);
    iph->id = htons(net_random() & 0xFFFF);
    iph->frag_off = 0;
    iph->ttl = IP_DEFAULT_TTL;
    iph->protocol = IPPROTO_UDP;
    iph->saddr = up->saddr;
    iph->daddr = daddr;

    /* Calculate checksum */
    if (up->flags & UDP_FLAG_CSUM_ENABLED) {
        udp_send_check(iph, uh, udp_len);
    } else {
        uh->check = 0;
    }

    /* Calculate IP checksum */
    ip_send_check(iph);

    return skb;
}

/**
 * udp_send_datagram - Send UDP datagram
 * @up: UDP socket
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
static int udp_send_datagram(struct udp_sock *up, struct sk_buff *skb)
{
    if (!up || !skb) {
        return -EINVAL;
    }

    /* Update statistics */
    up->datagrams_out++;
    up->bytes_out += skb->len;
    atomic64_inc(&udp_stats.datagrams_out);

    /* Send packet */
    return ip_send_skb(skb);
}

/*===========================================================================*/
/*                         UDP DATAGRAM PROCESSING                           */
/*===========================================================================*/

/**
 * udp_v4_rcv - Receive UDP datagram
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_rcv(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct udphdr *uh;
    struct udp_sock *up;
    struct socket *sock;
    u32 saddr, daddr;
    u16 sport, dport;
    u32 udp_len;
    u32 data_len;

    if (!skb) {
        return -EINVAL;
    }

    atomic64_inc(&udp_stats.datagrams_in);

    /* Get IP header */
    iph = (struct iphdr *)skb->data;

    /* Get UDP header */
    uh = (struct udphdr *)(skb->data + ip_hdrlen(iph));
    udp_len = ntohs(uh->len);

    /* Validate UDP length */
    if (udp_len < sizeof(struct udphdr)) {
        atomic64_inc(&udp_stats.receive_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate checksum */
    if (uh->check != 0) {
        if (udp_checksum(iph, uh, udp_len) != 0) {
            atomic64_inc(&udp_stats.receive_errors);
            free_skb(skb);
            return -EINVAL;
        }
    }

    /* Extract addresses and ports */
    saddr = iph->saddr;
    daddr = iph->daddr;
    sport = uh->source;
    dport = uh->dest;

    /* Find socket */
    up = udp_hash_find(dport, daddr);
    if (!up) {
        /* Try to find socket bound to any address */
        up = udp_hash_find(dport, 0);
    }

    if (!up) {
        /* No socket found */
        atomic64_inc(&udp_stats.receive_errors);
        free_skb(skb);
        return -ENOENT;
    }

    /* Update statistics */
    up->datagrams_in++;
    up->bytes_in += skb->len;

    /* Trim to data */
    data_len = udp_len - sizeof(struct udphdr);
    if (data_len > 0) {
        skb_pull(skb, sizeof(struct udphdr));
        skb_trim(skb, data_len);
    } else {
        skb_pull(skb, sizeof(struct udphdr));
    }

    /* Store source info for recvfrom */
    up->recv_saddr = saddr;
    up->recv_daddr = dport;

    /* Queue to socket */
    sock = up->sock;
    if (sock_queue_rcv_skb(sock, skb) != 0) {
        atomic64_inc(&udp_stats.receive_errors);
        free_skb(skb);
        return -ENOMEM;
    }

    return 0;
}

/*===========================================================================*/
/*                         UDP SOCKET OPERATIONS                             */
/*===========================================================================*/

/**
 * udp_create - Create UDP socket
 * @sock: Socket structure
 * @protocol: Protocol number
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_create(struct socket *sock, int protocol)
{
    struct udp_sock *up;

    if (!sock) {
        return -EINVAL;
    }

    /* Allocate UDP socket */
    up = udp_alloc_sock();
    if (!up) {
        return -ENOMEM;
    }

    up->sock = sock;
    sock->sk_protinfo = up;

    pr_debug("UDP: Created socket %p\n", up);

    return 0;
}

/**
 * udp_release - Release UDP socket
 * @sock: Socket structure
 *
 * Returns: 0 on success
 */
int udp_release(struct socket *sock)
{
    struct udp_sock *up;

    if (!sock) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return 0;
    }

    /* Remove from hash table */
    udp_hash_del(up);

    /* Free local port */
    if (up->sport != 0) {
        ip_local_port_free(up->sport);
    }

    udp_free_sock(up);
    sock->sk_protinfo = NULL;

    return 0;
}

/**
 * udp_bind - Bind UDP socket
 * @sock: Socket structure
 * @addr: Address to bind to
 * @addr_len: Address length
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
    struct udp_sock *up;
    struct sockaddr_in *addr_in;

    if (!sock || !addr) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    addr_in = (struct sockaddr_in *)addr;

    spin_lock(&up->lock);

    /* Check if already bound */
    if (up->sport != 0) {
        spin_unlock(&up->lock);
        return -EINVAL;
    }

    /* Set address */
    up->saddr = addr_in->sin_addr;
    up->sport = addr_in->sin_port;

    /* Allocate port if not specified */
    if (up->sport == 0) {
        up->sport = udp_port_alloc();
        if (up->sport == 0) {
            spin_unlock(&up->lock);
            return -EADDRNOTAVAIL;
        }
    }

    /* Add to hash table */
    udp_hash_add(up);

    spin_unlock(&up->lock);

    return 0;
}

/**
 * udp_connect - Connect UDP socket
 * @sock: Socket structure
 * @addr: Address to connect to
 * @addr_len: Address length
 * @flags: Connection flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_connect(struct socket *sock, struct sockaddr *addr, int addr_len, int flags)
{
    struct udp_sock *up;
    struct sockaddr_in *addr_in;

    if (!sock || !addr) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    addr_in = (struct sockaddr_in *)addr;

    spin_lock(&up->lock);

    up->daddr = addr_in->sin_addr;
    up->dport = addr_in->sin_port;

    spin_unlock(&up->lock);

    return 0;
}

/**
 * udp_sendmsg - Send message
 * @sock: Socket structure
 * @msg: Message data
 * @len: Message length
 * @flags: Send flags
 *
 * Returns: Number of bytes sent, or negative error code on failure
 */
int udp_sendmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct udp_sock *up;
    struct sk_buff *skb;
    u32 daddr;
    u16 dport;

    if (!sock || !msg) {
        return -EINVAL;
    }

    if (len > 65535 - sizeof(struct udphdr) - sizeof(struct iphdr)) {
        return -EMSGSIZE;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    spin_lock(&up->lock);

    /* Get destination */
    daddr = up->daddr;
    dport = up->dport;

    /* Check if connected */
    if (daddr == 0) {
        spin_unlock(&up->lock);
        return -ENOTCONN;
    }

    /* Build datagram */
    skb = udp_build_datagram(up, msg, len, daddr, dport);
    if (!skb) {
        spin_unlock(&up->lock);
        return -ENOMEM;
    }

    /* Send datagram */
    udp_send_datagram(up, skb);

    spin_unlock(&up->lock);

    return len;
}

/**
 * udp_recvmsg - Receive message
 * @sock: Socket structure
 * @msg: Buffer for message
 * @len: Buffer length
 * @flags: Receive flags
 *
 * Returns: Number of bytes received, or negative error code on failure
 */
int udp_recvmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct udp_sock *up;
    struct sk_buff *skb;
    size_t copy_len;

    if (!sock || !msg) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    /* Dequeue packet */
    skb = sock_dequeue_rcv_skb(sock);
    if (!skb) {
        if (flags & MSG_DONTWAIT) {
            return -EAGAIN;
        }
        /* In a full implementation, we would wait here */
        return -EAGAIN;
    }

    /* Copy data */
    copy_len = MIN(len, skb->len);
    memcpy(msg, skb->data, copy_len);

    free_skb(skb);

    return copy_len;
}

/**
 * udp_sendpage - Send page
 * @sock: Socket structure
 * @page: Page to send
 * @offset: Offset in page
 * @size: Size to send
 * @flags: Send flags
 *
 * Returns: Number of bytes sent, or negative error code on failure
 */
int udp_sendpage(struct socket *sock, void *page, int offset, size_t size, int flags)
{
    /* In a full implementation, this would send pages directly */
    return -EOPNOTSUPP;
}

/**
 * udp_setsockopt - Set socket option
 * @sock: Socket structure
 * @level: Option level
 * @optname: Option name
 * @optval: Option value
 * @optlen: Option length
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_setsockopt(struct socket *sock, int level, int optname, void *optval, int optlen)
{
    struct udp_sock *up;
    u32 val;

    if (!sock || !optval) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    if (optlen < sizeof(u32)) {
        return -EINVAL;
    }

    val = *(u32 *)optval;

    spin_lock(&up->lock);

    switch (level) {
        case SOL_SOCKET:
            /* Generic socket options */
            break;

        case SOL_UDP:
            switch (optname) {
                case 1:  /* UDP_CORK */
                    /* Not implemented */
                    break;

                default:
                    spin_unlock(&up->lock);
                    return -ENOPROTOOPT;
            }
            break;

        default:
            spin_unlock(&up->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&up->lock);

    return 0;
}

/**
 * udp_getsockopt - Get socket option
 * @sock: Socket structure
 * @level: Option level
 * @optname: Option name
 * @optval: Pointer to store option value
 * @optlen: Length of option value buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int udp_getsockopt(struct socket *sock, int level, int optname, void *optval, int *optlen)
{
    struct udp_sock *up;

    if (!sock || !optval || !optlen) {
        return -EINVAL;
    }

    up = (struct udp_sock *)sock->sk_protinfo;
    if (!up) {
        return -EINVAL;
    }

    if (*optlen < sizeof(u32)) {
        return -EINVAL;
    }

    spin_lock(&up->lock);

    switch (level) {
        case SOL_SOCKET:
            switch (optname) {
                case SO_TYPE:
                    *(u32 *)optval = SOCK_DGRAM;
                    break;

                default:
                    spin_unlock(&up->lock);
                    return -ENOPROTOOPT;
            }
            break;

        default:
            spin_unlock(&up->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&up->lock);
    *optlen = sizeof(u32);

    return 0;
}

/*===========================================================================*/
/*                         UDP STATISTICS                                    */
/*===========================================================================*/

/**
 * udp_stats_print - Print UDP statistics
 */
void udp_stats_print(void)
{
    spin_lock(&udp_stats.lock);

    printk("\n=== UDP Statistics ===\n");
    printk("Datagrams In: %llu\n", (unsigned long long)atomic64_read(&udp_stats.datagrams_in));
    printk("Datagrams Out: %llu\n", (unsigned long long)atomic64_read(&udp_stats.datagrams_out));
    printk("Receive Errors: %llu\n", (unsigned long long)atomic64_read(&udp_stats.receive_errors));
    printk("Send Errors: %llu\n", (unsigned long long)atomic64_read(&udp_stats.send_errors));
    printk("No Ports: %llu\n", (unsigned long long)atomic64_read(&udp_stats.no_ports));
    printk("Hash Table Entries: %u\n", udp_table.entries);

    spin_unlock(&udp_stats.lock);
}

/**
 * udp_stats_reset - Reset UDP statistics
 */
void udp_stats_reset(void)
{
    spin_lock(&udp_stats.lock);

    atomic64_set(&udp_stats.datagrams_in, 0);
    atomic64_set(&udp_stats.datagrams_out, 0);
    atomic64_set(&udp_stats.receive_errors, 0);
    atomic64_set(&udp_stats.send_errors, 0);
    atomic64_set(&udp_stats.no_ports, 0);

    spin_unlock(&udp_stats.lock);
}

/*===========================================================================*/
/*                         UDP INITIALIZATION                                */
/*===========================================================================*/

/**
 * udp_init - Initialize UDP layer
 */
void udp_init(void)
{
    int i;

    pr_info("Initializing UDP Layer...\n");

    /* Initialize hash table */
    for (i = 0; i < UDP_HASH_SIZE; i++) {
        udp_table.hash[i] = NULL;
    }

    spin_lock_init_named(&udp_table.lock, "udp_table");
    udp_table.entries = 0;

    /* Initialize statistics */
    spin_lock_init_named(&udp_stats.lock, "udp_stats");

    pr_info("  Hash table: %u buckets\n", UDP_HASH_SIZE);
    pr_info("  Port range: %u-%u\n", UDP_MIN_PORT, UDP_MAX_PORT);
    pr_info("UDP Layer initialized.\n");
}

/**
 * udp_exit - Shutdown UDP layer
 */
void udp_exit(void)
{
    pr_info("Shutting down UDP Layer...\n");

    /* Clean up all sockets */
    /* In a full implementation, this would close all sockets */

    pr_info("UDP Layer shutdown complete.\n");
}
