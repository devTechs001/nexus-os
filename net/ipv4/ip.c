/*
 * NEXUS OS - IP Layer Implementation
 * net/ipv4/ip.c
 */

#include "ipv4.h"

/*===========================================================================*/
/*                         IP CONFIGURATION                                  */
/*===========================================================================*/

#define IP_RT_HASH_BITS         8
#define IP_RT_HASH_SIZE         (1 << IP_RT_HASH_BITS)
#define IP_FRAG_HASH_BITS       6
#define IP_FRAG_HASH_SIZE       (1 << IP_FRAG_HASH_BITS)
#define IP_FRAG_TIMEOUT         30000     /* 30 seconds */
#define IP_FRAG_MAX_DIST        5
#define IP_LOCAL_PORT_RANGE_MIN 32768
#define IP_LOCAL_PORT_RANGE_MAX 61000

/*===========================================================================*/
/*                         IP GLOBAL DATA                                    */
/*===========================================================================*/

/* IP Statistics */
struct ip_statistics ip_stats = {
    .in_receives = ATOMIC64_INIT(0),
    .in_hdr_errors = ATOMIC64_INIT(0),
    .in_addr_errors = ATOMIC64_INIT(0),
    .in_unknown_protos = ATOMIC64_INIT(0),
    .in_discards = ATOMIC64_INIT(0),
    .in_delivers = ATOMIC64_INIT(0),
    .out_requests = ATOMIC64_INIT(0),
    .out_discards = ATOMIC64_INIT(0),
    .out_no_routes = ATOMIC64_INIT(0),
    .reasm_reqds = ATOMIC64_INIT(0),
    .reasm_oks = ATOMIC64_INIT(0),
    .reasm_fails = ATOMIC64_INIT(0),
    .reasm_timeout = ATOMIC64_INIT(0),
    .frag_creates = ATOMIC64_INIT(0),
    .frag_oks = ATOMIC64_INIT(0),
    .frag_fails = ATOMIC64_INIT(0),
    .forwards = ATOMIC64_INIT(0),
};

/* Routing Tables */
static struct rt_table ip_rt_table;
static spinlock_t ip_rt_lock = { .lock = SPINLOCK_UNLOCKED };

/* Fragment Queues */
static struct ipq *ip_frag_hash[IP_FRAG_HASH_SIZE];
static spinlock_t ip_frag_lock = { .lock = SPINLOCK_UNLOCKED };
static atomic_t ip_frag_count = ATOMIC_INIT(0);
static atomic_t ip_frag_mem = ATOMIC_INIT(0);

/* Local Port Allocator */
static struct {
    spinlock_t lock;
    u16 next_port;
    u8 port_bitmap[(IP_LOCAL_PORT_RANGE_MAX - IP_LOCAL_PORT_RANGE_MIN) / 8];
} ip_local_ports = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .next_port = IP_LOCAL_PORT_RANGE_MIN,
    .port_bitmap = {0},
};

/* Default TTL */
static u8 ip_default_ttl = IP_DEFAULT_TTL;

/*===========================================================================*/
/*                         IP CHECKSUM                                       */
/*===========================================================================*/

/**
 * ip_checksum - Compute IP checksum
 * @buf: Buffer to checksum
 * @len: Length of buffer
 * @sum: Initial sum
 *
 * Returns: 16-bit checksum
 */
u16 ip_checksum(void *buf, u32 len, u32 sum)
{
    u16 *ptr = (u16 *)buf;
    u32 i;

    /* Sum 16-bit words */
    for (i = 0; i < len / 2; i++) {
        sum += *ptr++;
    }

    /* Add remaining byte if odd length */
    if (len & 1) {
        sum += *(u8 *)ptr;
    }

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (u16)~sum;
}

/**
 * ip_compute_csum - Compute IP header checksum
 * @buf: Buffer to checksum
 * @len: Length of buffer
 *
 * Returns: 16-bit checksum
 */
u16 ip_compute_csum(void *buf, u32 len)
{
    return ip_checksum(buf, len, 0);
}

/**
 * ip_send_check - Compute and set IP header checksum
 * @ip: IP header
 */
void ip_send_check(struct iphdr *ip)
{
    ip->check = 0;
    ip->check = ip_compute_csum(ip, ip_hdrlen(ip));
}

/**
 * ip_check - Verify IP header checksum
 * @ip: IP header
 *
 * Returns: 0 if valid, non-zero if invalid
 */
int ip_check(struct iphdr *ip)
{
    u16 check;

    check = ip_compute_csum(ip, ip_hdrlen(ip));
    return check != 0;
}

/*===========================================================================*/
/*                         IP ADDRESS UTILITIES                              */
/*===========================================================================*/

/**
 * ip_make_addr - Create IP address from bytes
 * @a: First octet
 * @b: Second octet
 * @c: Third octet
 * @d: Fourth octet
 *
 * Returns: IP address in network byte order
 */
u32 ip_make_addr(u8 a, u8 b, u8 c, u8 d)
{
    return htonl(IP_ADDR(a, b, c, d));
}

/**
 * ip_print_addr - Print IP address to string
 * @addr: IP address
 * @buf: Buffer to store string
 * @len: Buffer length
 */
void ip_print_addr(u32 addr, char *buf, u32 len)
{
    u8 *bytes = (u8 *)&addr;
    snprintf(buf, len, "%u.%u.%u.%u",
             bytes[3], bytes[2], bytes[1], bytes[0]);
}

/**
 * ip_parse_addr - Parse IP address from string
 * @str: String to parse
 * @addr: Pointer to store address
 *
 * Returns: 0 on success, -1 on failure
 */
int ip_parse_addr(const char *str, u32 *addr)
{
    u32 a, b, c, d;
    char *endptr;

    if (!str || !addr) {
        return -1;
    }

    a = strtoul(str, &endptr, 10);
    if (*endptr != '.' || a > 255) return -1;

    b = strtoul(endptr + 1, &endptr, 10);
    if (*endptr != '.' || b > 255) return -1;

    c = strtoul(endptr + 1, &endptr, 10);
    if (*endptr != '.' || c > 255) return -1;

    d = strtoul(endptr + 1, &endptr, 10);
    if (*endptr != '\0' || d > 255) return -1;

    *addr = ip_make_addr(a, b, c, d);
    return 0;
}

/**
 * ip_validate_addr - Validate IP address
 * @addr: IP address to validate
 *
 * Returns: 0 if valid, -1 if invalid
 */
int ip_validate_addr(u32 addr)
{
    /* Check for bad class */
    if (IP_BAD_CLASS(addr)) {
        return -1;
    }

    return 0;
}

/*===========================================================================*/
/*                         LOCAL PORT ALLOCATION                             */
/*===========================================================================*/

/**
 * ip_local_port_alloc - Allocate a local port
 *
 * Returns: Port number in network byte order, or 0 on failure
 */
static u16 ip_local_port_alloc(void)
{
    u16 port;
    u32 offset;
    u8 bit;

    spin_lock(&ip_local_ports.lock);

    /* Find free port */
    for (offset = 0; offset < sizeof(ip_local_ports.port_bitmap); offset++) {
        if (ip_local_ports.port_bitmap[offset] != 0xFF) {
            for (bit = 0; bit < 8; bit++) {
                if (!(ip_local_ports.port_bitmap[offset] & (1 << bit))) {
                    port = IP_LOCAL_PORT_RANGE_MIN + offset * 8 + bit;
                    ip_local_ports.port_bitmap[offset] |= (1 << bit);
                    spin_unlock(&ip_local_ports.lock);
                    return htons(port);
                }
            }
        }
    }

    spin_unlock(&ip_local_ports.lock);
    return 0;
}

/**
 * ip_local_port_free - Free a local port
 * @port: Port number in network byte order
 */
static void ip_local_port_free(u16 port)
{
    u16 p = ntohs(port);
    u32 offset;
    u8 bit;

    if (p < IP_LOCAL_PORT_RANGE_MIN || p > IP_LOCAL_PORT_RANGE_MAX) {
        return;
    }

    offset = (p - IP_LOCAL_PORT_RANGE_MIN) / 8;
    bit = (p - IP_LOCAL_PORT_RANGE_MIN) % 8;

    spin_lock(&ip_local_ports.lock);
    ip_local_ports.port_bitmap[offset] &= ~(1 << bit);
    spin_unlock(&ip_local_ports.lock);
}

/*===========================================================================*/
/*                         ROUTING TABLE                                     */
/*===========================================================================*/

/**
 * ip_rt_hash - Hash function for routing entries
 * @dst: Destination address
 *
 * Returns: Hash value
 */
static inline u32 ip_rt_hash(u32 dst)
{
    return (dst >> 24) & 0xFF;
}

/**
 * ip_rt_init - Initialize routing table
 */
static void ip_rt_init(void)
{
    int i;

    memset(&ip_rt_table, 0, sizeof(struct rt_table));
    ip_rt_table.id = RT_TABLE_MAIN;
    strncpy(ip_rt_table.name, "main", sizeof(ip_rt_table.name) - 1);

    for (i = 0; i < 256; i++) {
        ip_rt_table.hash[i] = NULL;
    }

    spin_lock_init_named(&ip_rt_table.lock, "rt_table");
    ip_rt_table.entries = 0;
    ip_rt_table.lookups = 0;
    ip_rt_table.hits = 0;
}

/**
 * ip_rt_add - Add route to routing table
 * @dst: Destination address
 * @mask: Netmask
 * @gw: Gateway address
 * @dev: Output device
 * @flags: Route flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_rt_add(u32 dst, u32 mask, u32 gw, void *dev, u32 flags)
{
    struct rt_entry *rt;
    u32 hash;

    /* Allocate routing entry */
    rt = kzalloc(sizeof(struct rt_entry));
    if (!rt) {
        return -ENOMEM;
    }

    rt->dst = dst;
    rt->mask = mask;
    rt->gw = gw;
    rt->dev = dev;
    rt->flags = flags;
    rt->mtu = 1500;  /* Default MTU */
    rt->refcnt = 1;
    rt->lastuse = get_ticks();

    spin_lock_init_named(&rt->lock, "rt_entry");

    /* Add to hash table */
    hash = ip_rt_hash(dst);

    spin_lock(&ip_rt_table.lock);

    rt->next = ip_rt_table.hash[hash];
    rt->prev = NULL;
    if (ip_rt_table.hash[hash]) {
        ip_rt_table.hash[hash]->prev = rt;
    }
    ip_rt_table.hash[hash] = rt;

    ip_rt_table.entries++;

    spin_unlock(&ip_rt_table.lock);

    pr_debug("IP: Added route " IP_FMT "/" IP_FMT " via " IP_FMT "\n",
             IP_ARG(dst), IP_ARG(mask), IP_ARG(gw));

    return 0;
}

/**
 * ip_rt_del - Delete route from routing table
 * @dst: Destination address
 * @mask: Netmask
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_rt_del(u32 dst, u32 mask)
{
    struct rt_entry *rt;
    u32 hash;

    hash = ip_rt_hash(dst);

    spin_lock(&ip_rt_table.lock);

    rt = ip_rt_table.hash[hash];
    while (rt) {
        if (rt->dst == dst && rt->mask == mask) {
            /* Remove from hash table */
            if (rt->prev) {
                rt->prev->next = rt->next;
            } else {
                ip_rt_table.hash[hash] = rt->next;
            }
            if (rt->next) {
                rt->next->prev = rt->prev;
            }

            ip_rt_table.entries--;

            spin_unlock(&ip_rt_table.lock);

            kfree(rt);

            pr_debug("IP: Deleted route " IP_FMT "/" IP_FMT "\n",
                     IP_ARG(dst), IP_ARG(mask));

            return 0;
        }
        rt = rt->next;
    }

    spin_unlock(&ip_rt_table.lock);
    return -ENOENT;
}

/**
 * ip_rt_flush - Flush routing table
 */
void ip_rt_flush(void)
{
    struct rt_entry *rt;
    struct rt_entry *next;
    int i;

    spin_lock(&ip_rt_table.lock);

    for (i = 0; i < 256; i++) {
        rt = ip_rt_table.hash[i];
        while (rt) {
            next = rt->next;
            kfree(rt);
            rt = next;
        }
        ip_rt_table.hash[i] = NULL;
    }

    ip_rt_table.entries = 0;

    spin_unlock(&ip_rt_table.lock);

    pr_debug("IP: Flushed routing table\n");
}

/**
 * ip_route_output - Lookup route for output
 * @daddr: Destination address
 * @saddr: Source address
 * @tos: Type of service
 *
 * Returns: Routing entry, or NULL if not found
 */
struct rt_entry *ip_route_output(u32 daddr, u32 saddr, u8 tos)
{
    struct rt_entry *rt;
    u32 hash;
    u32 best_match = 0;
    struct rt_entry *best_rt = NULL;

    hash = ip_rt_hash(daddr);

    spin_lock(&ip_rt_table.lock);

    ip_rt_table.lookups++;

    rt = ip_rt_table.hash[hash];
    while (rt) {
        /* Check if destination matches */
        if ((daddr & rt->mask) == rt->dst) {
            /* Check for better match */
            u32 match_bits = 32 - __builtin_clz(rt->mask);
            if (match_bits > best_match) {
                best_match = match_bits;
                best_rt = rt;
            }
        }
        rt = rt->next;
    }

    if (best_rt) {
        best_rt->refcnt++;
        best_rt->lastuse = get_ticks();
        ip_rt_table.hits++;
    }

    spin_unlock(&ip_rt_table.lock);

    return best_rt;
}

/**
 * ip_route_input - Determine route for input packet
 * @skb: Socket buffer
 *
 * Returns: Routing entry, or NULL if not found
 */
struct rt_entry *ip_route_input(struct sk_buff *skb)
{
    struct iphdr *ip;
    struct rt_entry *rt;

    if (!skb) {
        return NULL;
    }

    ip = (struct iphdr *)skb->data;

    /* Check if destination is local */
    /* In a full implementation, this would check local addresses */

    /* For now, return NULL for non-local destinations */
    return NULL;
}

/**
 * ip_rt_put - Release routing entry
 * @rt: Routing entry
 */
void ip_rt_put(struct rt_entry *rt)
{
    if (!rt) {
        return;
    }

    spin_lock(&rt->lock);
    if (rt->refcnt > 0) {
        rt->refcnt--;
    }
    spin_unlock(&rt->lock);
}

/*===========================================================================*/
/*                         FRAGMENTATION                                     */
/*===========================================================================*/

/**
 * ip_frag_hash_fn - Hash function for fragment queues
 * @id: Fragment ID
 * @saddr: Source address
 * @daddr: Destination address
 * @protocol: Protocol
 *
 * Returns: Hash value
 */
static inline u32 ip_frag_hash_fn(u32 id, u32 saddr, u32 daddr, u8 protocol)
{
    u32 hash = id;
    hash ^= saddr;
    hash ^= daddr;
    hash ^= protocol;
    return hash & (IP_FRAG_HASH_SIZE - 1);
}

/**
 * ip_frag_alloc - Allocate fragment queue
 * @id: Fragment ID
 * @saddr: Source address
 * @daddr: Destination address
 * @protocol: Protocol
 *
 * Returns: Fragment queue, or NULL on failure
 */
static struct ipq *ip_frag_alloc(u32 id, u32 saddr, u32 daddr, u8 protocol)
{
    struct ipq *q;

    q = kzalloc(sizeof(struct ipq));
    if (!q) {
        return NULL;
    }

    q->id = id;
    q->saddr = saddr;
    q->daddr = daddr;
    q->protocol = protocol;
    q->expires = get_time_ms() + IP_FRAG_TIMEOUT;
    q->fragments = NULL;

    spin_lock_init_named(&q->lock, "ipq");

    atomic_inc(&ip_frag_count);

    return q;
}

/**
 * ip_frag_free - Free fragment queue
 * @q: Fragment queue
 */
static void ip_frag_free(struct ipq *q)
{
    struct ip_frag *frag;
    struct ip_frag *next;

    if (!q) {
        return;
    }

    /* Free all fragments */
    frag = q->fragments;
    while (frag) {
        next = frag->next;
        if (frag->skb) {
            free_skb(frag->skb);
        }
        kfree(frag);
        frag = next;
    }

    kfree(q);
    atomic_dec(&ip_frag_count);
}

/**
 * ip_frag_find - Find fragment queue
 * @id: Fragment ID
 * @saddr: Source address
 * @daddr: Destination address
 * @protocol: Protocol
 *
 * Returns: Fragment queue, or NULL if not found
 */
static struct ipq *ip_frag_find(u32 id, u32 saddr, u32 daddr, u8 protocol)
{
    struct ipq *q;
    u32 hash;

    hash = ip_frag_hash_fn(id, saddr, daddr, protocol);

    spin_lock(&ip_frag_lock);

    q = ip_frag_hash[hash];
    while (q) {
        if (q->id == id && q->saddr == saddr &&
            q->daddr == daddr && q->protocol == protocol) {
            spin_unlock(&ip_frag_lock);
            return q;
        }
        q = q->next;
    }

    spin_unlock(&ip_frag_lock);
    return NULL;
}

/**
 * ip_frag_queue - Queue a fragment
 * @q: Fragment queue
 * @skb: Socket buffer containing fragment
 * @offset: Fragment offset
 * @more: More fragments flag
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ip_frag_queue(struct ipq *q, struct sk_buff *skb, u32 offset, int more)
{
    struct ip_frag *frag;
    struct ip_frag **fp;

    if (!q || !skb) {
        return -EINVAL;
    }

    /* Allocate fragment */
    frag = kzalloc(sizeof(struct ip_frag));
    if (!frag) {
        return -ENOMEM;
    }

    frag->offset = offset;
    frag->len = skb->len;
    frag->end = offset + skb->len;
    frag->skb = skb_get(skb);
    frag->next = NULL;

    spin_lock(&q->lock);

    /* Insert in order */
    fp = &q->fragments;
    while (*fp && (*fp)->offset < offset) {
        fp = &(*fp)->next;
    }

    frag->next = *fp;
    *fp = frag;

    q->received += skb->len;

    /* Check if complete */
    if (!more && q->first_frag_offset == 0) {
        q->len = frag->end;
    }

    spin_unlock(&q->lock);

    atomic_add(skb->truesize, &ip_frag_mem);

    return 0;
}

/**
 * ip_frag_reasm - Reassemble fragments
 * @q: Fragment queue
 *
 * Returns: Reassembled socket buffer, or NULL on failure
 */
static struct sk_buff *ip_frag_reasm(struct ipq *q)
{
    struct sk_buff *skb;
    struct sk_buff *skb2;
    struct ip_frag *frag;
    u32 offset;
    u32 total_len;

    if (!q) {
        return NULL;
    }

    /* Calculate total length */
    total_len = q->len;

    /* Allocate reassembled buffer */
    skb = alloc_skb(total_len);
    if (!skb) {
        return NULL;
    }

    /* Copy fragments */
    offset = 0;
    frag = q->fragments;
    while (frag && offset < total_len) {
        memcpy(skb_put(skb, frag->len), frag->skb->data, frag->len);
        offset += frag->len;
        frag = frag->next;
    }

    return skb;
}

/**
 * ip_frag_expire - Expire old fragment queues
 */
static void ip_frag_expire(void)
{
    struct ipq *q;
    struct ipq *next;
    u64 now;
    int i;

    now = get_time_ms();

    spin_lock(&ip_frag_lock);

    for (i = 0; i < IP_FRAG_HASH_SIZE; i++) {
        q = ip_frag_hash[i];
        while (q) {
            next = q->next;

            if (now > q->expires) {
                /* Remove from hash table */
                if (q->prev) {
                    q->prev->next = q->next;
                } else {
                    ip_frag_hash[i] = q->next;
                }
                if (q->next) {
                    q->next->prev = q->prev;
                }

                atomic64_inc(&ip_stats.reasm_timeout);
                ip_frag_free(q);
            }

            q = next;
        }
    }

    spin_unlock(&ip_frag_lock);
}

/**
 * ip_defrag - Defragment IP packet
 * @skb: Socket buffer containing fragment
 *
 * Returns: Reassembled socket buffer, or NULL if still waiting
 */
struct sk_buff *ip_defrag(struct sk_buff *skb)
{
    struct iphdr *ip;
    struct ipq *q;
    u32 id, saddr, daddr;
    u8 protocol;
    u16 frag_off;
    u32 offset;
    int more;

    if (!skb) {
        return NULL;
    }

    ip = (struct iphdr *)skb->data;

    /* Check if fragmentation is needed */
    frag_off = ntohs(ip->frag_off);
    if (!(frag_off & (IP_MF | IP_OFFSET))) {
        return skb;  /* Not fragmented */
    }

    /* Extract fragment info */
    id = ntohs(ip->id);
    saddr = ip->saddr;
    daddr = ip->daddr;
    protocol = ip->protocol;
    offset = (frag_off & IP_OFFSET) * 8;
    more = frag_off & IP_MF;

    /* Update statistics */
    atomic64_inc(&ip_stats.reasm_reqds);

    /* Find or create fragment queue */
    q = ip_frag_find(id, saddr, daddr, protocol);
    if (!q) {
        q = ip_frag_alloc(id, saddr, daddr, protocol);
        if (!q) {
            atomic64_inc(&ip_stats.reasm_fails);
            free_skb(skb);
            return NULL;
        }

        /* Add to hash table */
        u32 hash = ip_frag_hash_fn(id, saddr, daddr, protocol);
        spin_lock(&ip_frag_lock);
        q->next = ip_frag_hash[hash];
        if (ip_frag_hash[hash]) {
            ip_frag_hash[hash]->prev = q;
        }
        ip_frag_hash[hash] = q;
        spin_unlock(&ip_frag_lock);
    }

    /* Queue fragment */
    if (ip_frag_queue(q, skb, offset, more) != 0) {
        atomic64_inc(&ip_stats.reasm_fails);
        return NULL;
    }

    /* Check if complete */
    if (!more && q->received >= q->len) {
        struct sk_buff *reassembled;

        reassembled = ip_frag_reasm(q);

        /* Remove from hash table */
        spin_lock(&ip_frag_lock);
        if (q->prev) {
            q->prev->next = q->next;
        } else {
            u32 hash = ip_frag_hash_fn(id, saddr, daddr, protocol);
            ip_frag_hash[hash] = q->next;
        }
        if (q->next) {
            q->next->prev = q->prev;
        }
        spin_unlock(&ip_frag_lock);

        ip_frag_free(q);

        atomic64_inc(&ip_stats.reasm_oks);

        return reassembled;
    }

    /* Still waiting for more fragments */
    return NULL;
}

/**
 * ip_fragment - Fragment IP packet
 * @skb: Socket buffer to fragment
 * @frags: Array to store fragments
 * @mtu: Maximum transmission unit
 *
 * Returns: Number of fragments, or negative error code on failure
 */
int ip_fragment(struct sk_buff *skb, struct sk_buff **frags, u32 mtu)
{
    struct iphdr *ip;
    u32 max_frag_len;
    u32 offset;
    u32 frag_id;
    int frag_count = 0;

    if (!skb || !frags) {
        return -EINVAL;
    }

    ip = (struct iphdr *)skb->data;

    /* Calculate maximum fragment length */
    max_frag_len = (mtu - IP_MIN_HDR_LEN) & ~7;  /* Align to 8 bytes */

    if (max_frag_len < 8) {
        return -EINVAL;
    }

    /* Generate fragment ID */
    frag_id = net_random();

    /* Create fragments */
    offset = 0;
    while (offset < skb->len) {
        struct sk_buff *frag_skb;
        struct iphdr *frag_ip;
        u32 len;
        u16 frag_off;

        len = MIN(max_frag_len, skb->len - offset);

        frag_skb = alloc_skb(len + IP_MIN_HDR_LEN);
        if (!frag_skb) {
            /* Free already created fragments */
            int i;
            for (i = 0; i < frag_count; i++) {
                free_skb(frags[i]);
            }
            return -ENOMEM;
        }

        /* Reserve space for IP header */
        skb_reserve(frag_skb, IP_MIN_HDR_LEN);

        /* Copy data */
        memcpy(skb_put(frag_skb, len), skb->data + offset, len);

        /* Create IP header */
        frag_ip = (struct iphdr *)skb_push(frag_skb, IP_MIN_HDR_LEN);
        memcpy(frag_ip, ip, IP_MIN_HDR_LEN);

        /* Set fragment fields */
        frag_ip->id = htons(frag_id);
        frag_ip->tot_len = htons(IP_MIN_HDR_LEN + len);

        frag_off = offset / 8;
        if (offset + len < skb->len) {
            frag_off |= IP_MF;
        }
        frag_ip->frag_off = htons(frag_off);

        /* Update checksum */
        ip_send_check(frag_ip);

        frags[frag_count++] = frag_skb;
        offset += len;
    }

    atomic64_add(frag_count, &ip_stats.frag_creates);

    return frag_count;
}

/**
 * ip_frag_init - Initialize fragmentation subsystem
 */
void ip_frag_init(void)
{
    int i;

    for (i = 0; i < IP_FRAG_HASH_SIZE; i++) {
        ip_frag_hash[i] = NULL;
    }

    spin_lock_init_named(&ip_frag_lock, "ip_frag");
    atomic_set(&ip_frag_count, 0);
    atomic_set(&ip_frag_mem, 0);

    /* Initialize local port allocator */
    spin_lock_init_named(&ip_local_ports.lock, "ip_local_ports");
    memset(ip_local_ports.port_bitmap, 0, sizeof(ip_local_ports.port_bitmap));
    ip_local_ports.next_port = IP_LOCAL_PORT_RANGE_MIN;

    pr_debug("IP: Fragmentation subsystem initialized\n");
}

/**
 * ip_frag_exit - Shutdown fragmentation subsystem
 */
void ip_frag_exit(void)
{
    struct ipq *q;
    struct ipq *next;
    int i;

    spin_lock(&ip_frag_lock);

    for (i = 0; i < IP_FRAG_HASH_SIZE; i++) {
        q = ip_frag_hash[i];
        while (q) {
            next = q->next;
            ip_frag_free(q);
            q = next;
        }
        ip_frag_hash[i] = NULL;
    }

    spin_unlock(&ip_frag_lock);

    pr_debug("IP: Fragmentation subsystem shutdown\n");
}

/*===========================================================================*/
/*                         IP PACKET PROCESSING                              */
/*===========================================================================*/

/**
 * ip_rcv - Receive IP packet
 * @skb: Socket buffer containing packet
 * @dev: Network device
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_rcv(struct sk_buff *skb, void *dev)
{
    struct iphdr *ip;
    u32 len;

    if (!skb) {
        return -EINVAL;
    }

    atomic64_inc(&ip_stats.in_receives);

    /* Get IP header */
    ip = (struct iphdr *)skb->data;

    /* Validate header length */
    if (ip_hdrlen(ip) < IP_MIN_HDR_LEN) {
        atomic64_inc(&ip_stats.in_hdr_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate version */
    if (ip->version != IP_VERSION_4) {
        atomic64_inc(&ip_stats.in_hdr_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate total length */
    len = ntohs(ip->tot_len);
    if (len < ip_hdrlen(ip) || len > skb->len) {
        atomic64_inc(&ip_stats.in_hdr_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate checksum */
    if (ip_check(ip)) {
        atomic64_inc(&ip_stats.in_hdr_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate destination address */
    if (ip_validate_addr(ip->daddr) != 0) {
        atomic64_inc(&ip_stats.in_addr_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Trim packet to correct length */
    if (len < skb->len) {
        skb_trim(skb, len);
    }

    /* Handle fragmentation */
    if (ntohs(ip->frag_off) & (IP_MF | IP_OFFSET)) {
        skb = ip_defrag(skb);
        if (!skb) {
            return 0;  /* Still waiting for fragments */
        }
        ip = (struct iphdr *)skb->data;
    }

    /* Deliver to upper layer */
    return ip_local_deliver(skb);
}

/**
 * ip_local_deliver - Deliver IP packet to local upper layer
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_local_deliver(struct sk_buff *skb)
{
    struct iphdr *ip;
    u8 protocol;

    if (!skb) {
        return -EINVAL;
    }

    ip = (struct iphdr *)skb->data;
    protocol = ip->protocol;

    atomic64_inc(&ip_stats.in_delivers);

    /* Deliver based on protocol */
    switch (protocol) {
        case IPPROTO_ICMP:
            /* icmp_rcv(skb); */
            break;

        case IPPROTO_TCP:
            /* tcp_v4_rcv(skb); */
            break;

        case IPPROTO_UDP:
            /* udp_rcv(skb); */
            break;

        default:
            atomic64_inc(&ip_stats.in_unknown_protos);
            free_skb(skb);
            return -EPROTONOSUPPORT;
    }

    return 0;
}

/**
 * ip_forward - Forward IP packet
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_forward(struct sk_buff *skb)
{
    struct iphdr *ip;
    struct rt_entry *rt;

    if (!skb) {
        return -EINVAL;
    }

    ip = (struct iphdr *)skb->data;

    /* Check TTL */
    if (ip->ttl <= 1) {
        /* TTL expired, send ICMP Time Exceeded */
        free_skb(skb);
        return -ETIMEDOUT;
    }

    /* Decrement TTL */
    ip->ttl--;

    /* Update checksum */
    ip_send_check(ip);

    /* Find route */
    rt = ip_route_output(ip->daddr, ip->saddr, ip->tos);
    if (!rt) {
        atomic64_inc(&ip_stats.out_no_routes);
        free_skb(skb);
        return -ENETUNREACH;
    }

    atomic64_inc(&ip_stats.forwards);

    /* Transmit packet */
    return ip_output(skb);
}

/*===========================================================================*/
/*                         IP OUTPUT                                         */
/*===========================================================================*/

/**
 * ip_output - Output IP packet
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_output(struct sk_buff *skb)
{
    struct iphdr *ip;
    struct rt_entry *rt;
    u32 mtu;

    if (!skb) {
        return -EINVAL;
    }

    ip = (struct iphdr *)skb->data;

    /* Find route */
    rt = ip_route_output(ip->daddr, ip->saddr, ip->tos);
    if (!rt) {
        atomic64_inc(&ip_stats.out_no_routes);
        free_skb(skb);
        return -ENETUNREACH;
    }

    /* Get MTU */
    mtu = rt->mtu;

    /* Check if fragmentation is needed */
    if (ntohs(ip->tot_len) > mtu) {
        if (ip_frag_dont(ip)) {
            /* Don't fragment flag set */
            ip_rt_put(rt);
            atomic64_inc(&ip_stats.frag_fails);
            free_skb(skb);
            return -EMSGSIZE;
        }

        /* Fragment packet */
        /* In a full implementation, this would fragment */
    }

    /* Update statistics */
    atomic64_inc(&ip_stats.out_requests);

    /* Send to device */
    skb->dev = rt->dev;
    dev_queue_xmit(skb);

    ip_rt_put(rt);

    return 0;
}

/**
 * ip_queue_xmit - Queue packet for transmission
 * @skb: Socket buffer
 * @dst: Destination
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_queue_xmit(struct sk_buff *skb, void *dst)
{
    return ip_output(skb);
}

/**
 * ip_send_skb - Send socket buffer
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int ip_send_skb(struct sk_buff *skb)
{
    return ip_output(skb);
}

/*===========================================================================*/
/*                         IP STATISTICS                                     */
/*===========================================================================*/

/**
 * ip_stats_print - Print IP statistics
 */
void ip_stats_print(void)
{
    printk("\n=== IP Statistics ===\n");
    printk("InReceives: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_receives));
    printk("InHdrErrors: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_hdr_errors));
    printk("InAddrErrors: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_addr_errors));
    printk("InUnknownProtos: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_unknown_protos));
    printk("InDiscards: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_discards));
    printk("InDelivers: %llu\n", (unsigned long long)atomic64_read(&ip_stats.in_delivers));
    printk("OutRequests: %llu\n", (unsigned long long)atomic64_read(&ip_stats.out_requests));
    printk("OutDiscards: %llu\n", (unsigned long long)atomic64_read(&ip_stats.out_discards));
    printk("OutNoRoutes: %llu\n", (unsigned long long)atomic64_read(&ip_stats.out_no_routes));
    printk("ReasmReqds: %llu\n", (unsigned long long)atomic64_read(&ip_stats.reasm_reqds));
    printk("ReasmOKs: %llu\n", (unsigned long long)atomic64_read(&ip_stats.reasm_oks));
    printk("ReasmFails: %llu\n", (unsigned long long)atomic64_read(&ip_stats.reasm_fails));
    printk("ReasmTimeout: %llu\n", (unsigned long long)atomic64_read(&ip_stats.reasm_timeout));
    printk("FragCreates: %llu\n", (unsigned long long)atomic64_read(&ip_stats.frag_creates));
    printk("FragOKs: %llu\n", (unsigned long long)atomic64_read(&ip_stats.frag_oks));
    printk("FragFails: %llu\n", (unsigned long long)atomic64_read(&ip_stats.frag_fails));
    printk("Forwards: %llu\n", (unsigned long long)atomic64_read(&ip_stats.forwards));
}

/**
 * ip_stats_reset - Reset IP statistics
 */
void ip_stats_reset(void)
{
    atomic64_set(&ip_stats.in_receives, 0);
    atomic64_set(&ip_stats.in_hdr_errors, 0);
    atomic64_set(&ip_stats.in_addr_errors, 0);
    atomic64_set(&ip_stats.in_unknown_protos, 0);
    atomic64_set(&ip_stats.in_discards, 0);
    atomic64_set(&ip_stats.in_delivers, 0);
    atomic64_set(&ip_stats.out_requests, 0);
    atomic64_set(&ip_stats.out_discards, 0);
    atomic64_set(&ip_stats.out_no_routes, 0);
    atomic64_set(&ip_stats.reasm_reqds, 0);
    atomic64_set(&ip_stats.reasm_oks, 0);
    atomic64_set(&ip_stats.reasm_fails, 0);
    atomic64_set(&ip_stats.reasm_timeout, 0);
    atomic64_set(&ip_stats.frag_creates, 0);
    atomic64_set(&ip_stats.frag_oks, 0);
    atomic64_set(&ip_stats.frag_fails, 0);
    atomic64_set(&ip_stats.forwards, 0);
}

/*===========================================================================*/
/*                         IP DEBUGGING                                      */
/*===========================================================================*/

/**
 * ip_debug_hdr - Debug IP header
 * @ip: IP header
 */
void ip_debug_hdr(struct iphdr *ip)
{
    if (!ip) {
        printk("IP: NULL header\n");
        return;
    }

    printk("\n=== IP Header Debug Info ===\n");
    printk("Version: %u\n", ip->version);
    printk("IHL: %u (%u bytes)\n", ip->ihl, ip_hdrlen(ip));
    printk("TOS: 0x%02x\n", ip->tos);
    printk("Total Length: %u\n", ntohs(ip->tot_len));
    printk("ID: 0x%04x\n", ntohs(ip->id));
    printk("Fragment Offset: %u\n", ip_frag_off(ip));
    printk("Flags: %s%s\n",
           ip_frag_dont(ip) ? "DF " : "",
           ip_frag_more(ip) ? "MF" : "");
    printk("TTL: %u\n", ip->ttl);
    printk("Protocol: %u\n", ip->protocol);
    printk("Checksum: 0x%04x (%s)\n",
           ntohs(ip->check),
           ip_check(ip) ? "INVALID" : "OK");
    printk("Source: " IP_FMT "\n", IP_ARG(ip->saddr));
    printk("Destination: " IP_FMT "\n", IP_ARG(ip->daddr));
}

/**
 * ip_debug_skb - Debug IP socket buffer
 * @skb: Socket buffer
 */
void ip_debug_skb(struct sk_buff *skb)
{
    struct iphdr *ip;

    if (!skb) {
        printk("IP: NULL skb\n");
        return;
    }

    ip = (struct iphdr *)skb->data;
    ip_debug_hdr(ip);

    printk("SKB Length: %u\n", skb->len);
    printk("SKB Data Length: %u\n", skb->data_len);
}

/*===========================================================================*/
/*                         IP INITIALIZATION                                 */
/*===========================================================================*/

/**
 * ip_init - Initialize IP layer
 */
void ip_init(void)
{
    pr_info("Initializing IP Layer...\n");

    /* Initialize routing table */
    ip_rt_init();

    /* Initialize fragmentation */
    ip_frag_init();

    /* Add default route */
    ip_rt_add(0, 0, 0, NULL, RTF_UP);

    /* Add loopback route */
    ip_rt_add(IP_ADDR(127, 0, 0, 0), IP_ADDR(255, 0, 0, 0), 0, NULL, RTF_UP | RTF_LOCAL);

    pr_info("  Default TTL: %u\n", ip_default_ttl);
    pr_info("  Fragment timeout: %u ms\n", IP_FRAG_TIMEOUT);
    pr_info("  Local port range: %u-%u\n",
            IP_LOCAL_PORT_RANGE_MIN, IP_LOCAL_PORT_RANGE_MAX);
    pr_info("IP Layer initialized.\n");
}

/**
 * ip_exit - Shutdown IP layer
 */
void ip_exit(void)
{
    pr_info("Shutting down IP Layer...\n");

    /* Flush routing table */
    ip_rt_flush();

    /* Shutdown fragmentation */
    ip_frag_exit();

    pr_info("IP Layer shutdown complete.\n");
}
