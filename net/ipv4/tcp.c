/*
 * NEXUS OS - TCP Implementation
 * net/ipv4/tcp.c
 */

#include "ipv4.h"

/*===========================================================================*/
/*                         TCP CONFIGURATION                                 */
/*===========================================================================*/

#define TCP_HASH_BITS           10
#define TCP_HASH_SIZE           (1 << TCP_HASH_BITS)
#define TCP_TIMEWAIT_TIMEOUT    120000    /* 2 minutes */
#define TCP_RETRANS_TIMEOUT     1000      /* 1 second initial */
#define TCP_MAX_RETRANS         15
#define TCP_KEEPALIVE_TIME      7200000   /* 2 hours */
#define TCP_KEEPALIVE_INTVL     75000     /* 75 seconds */
#define TCP_KEEPALIVE_PROBES    9
#define TCP_MAX_WINDOW          65535
#define TCP_DEFAULT_WINDOW      29200
#define TCP_MSS_DEFAULT         536
#define TCP_MSS_DESIRED         1460

/* TCP Flags */
#define TCP_FLAG_FIN            0x01
#define TCP_FLAG_SYN            0x02
#define TCP_FLAG_RST            0x04
#define TCP_FLAG_PSH            0x08
#define TCP_FLAG_ACK            0x10
#define TCP_FLAG_URG            0x20
#define TCP_FLAG_ECE            0x40
#define TCP_FLAG_CWR            0x80

/* TCP Options */
#define TCP_OPT_END             0
#define TCP_OPT_NOP             1
#define TCP_OPT_MSS             2
#define TCP_OPT_WINDOW          3
#define TCP_OPT_SACK_PERM       4
#define TCP_OPT_SACK            5
#define TCP_OPT_TIMESTAMP       8

/* TCP States */
#define TCP_STATE_CLOSED        0
#define TCP_STATE_LISTEN        1
#define TCP_STATE_SYN_SENT      2
#define TCP_STATE_SYN_RECV      3
#define TCP_STATE_ESTABLISHED   4
#define TCP_STATE_FIN_WAIT1     5
#define TCP_STATE_FIN_WAIT2     6
#define TCP_STATE_CLOSE_WAIT    7
#define TCP_STATE_CLOSING       8
#define TCP_STATE_LAST_ACK      9
#define TCP_STATE_TIME_WAIT     10

/*===========================================================================*/
/*                         TCP STRUCTURES                                    */
/*===========================================================================*/

/**
 * tcphdr - TCP header structure
 */
struct tcphdr {
    u16 source;                 /* Source port */
    u16 dest;                   /* Destination port */
    u32 seq;                    /* Sequence number */
    u32 ack_seq;                /* Acknowledgment number */
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u16 res1:4;                 /* Reserved */
    u16 doff:4;                 /* Data offset */
    u16 fin:1;                  /* FIN flag */
    u16 syn:1;                  /* SYN flag */
    u16 rst:1;                  /* RST flag */
    u16 psh:1;                  /* PSH flag */
    u16 ack:1;                  /* ACK flag */
    u16 urg:1;                  /* URG flag */
    u16 ece:1;                  /* ECE flag */
    u16 cwr:1;                  /* CWR flag */
#elif defined(__BIG_ENDIAN_BITFIELD)
    u16 doff:4;                 /* Data offset */
    u16 res1:4;                 /* Reserved */
    u16 cwr:1;                  /* CWR flag */
    u16 ece:1;                  /* ECE flag */
    u16 urg:1;                  /* URG flag */
    u16 ack:1;                  /* ACK flag */
    u16 psh:1;                  /* PSH flag */
    u16 rst:1;                  /* RST flag */
    u16 syn:1;                  /* SYN flag */
    u16 fin:1;                  /* FIN flag */
#endif
    u16 window;                 /* Window size */
    u16 check;                  /* Checksum */
    u16 urg_ptr;                /* Urgent pointer */
} __packed;

/**
 * tcp_options - TCP options
 */
struct tcp_options {
    u16 mss;                    /* Maximum Segment Size */
    u8 mss_ok:1;                /* MSS option present */
    u8 ws_ok:1;                 /* Window scale present */
    u8 sack_ok:1;               /* SACK permitted */
    u8 ts_ok:1;                 /* Timestamps present */
    u8 wscale;                  /* Window scale */
    u8 tsv;                     /* Timestamp value */
    u8 tscr;                    /* Timestamp echo reply */
};

/**
 * tcp_sock - TCP socket
 */
struct tcp_sock {
    struct socket *sock;        /* Parent socket */

    /* Connection State */
    u8 state;                   /* TCP state */
    u8 pending;                 /* Pending state */

    /* Addresses */
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
    u16 sport;                  /* Source port */
    u16 dport;                  /* Destination port */

    /* Sequence Numbers */
    u32 snd_nxt;                /* Next sequence to send */
    u32 snd_una;                /* Oldest unacknowledged sequence */
    u32 snd_wnd;                /* Send window */
    u32 snd_wl1;                /* Sequence for window update */
    u32 snd_wl2;                /* ACK for window update */
    u32 iss;                    /* Initial send sequence */

    u32 rcv_nxt;                /* Next sequence to receive */
    u32 rcv_wnd;                /* Receive window */
    u32 rcv_wup;                /* Window update sequence */
    u32 irs;                    /* Initial receive sequence */

    /* Retransmission */
    u32 rto;                    /* Retransmission timeout */
    u32 rtt;                    /* Round trip time */
    u32 rttvar;                 /* RTT variance */
    u32 srtt;                   /* Smoothed RTT */
    u64 rto_start;              /* RTO timer start */
    u32 retransmits;            /* Retransmit count */

    /* Congestion Control */
    u32 cwnd;                   /* Congestion window */
    u32 ssthresh;               /* Slow start threshold */
    u32 dup_acks;               /* Duplicate ACK count */

    /* Options */
    struct tcp_options opts;
    u16 mss;                    /* Maximum Segment Size */
    u16 advmss;                 /* Advertised MSS */

    /* Timers */
    u64 timer_expire;           /* Timer expiration */
    u32 timer_flags;            /* Timer flags */

    /* Keepalive */
    u32 keepalive_time;         /* Keepalive time */
    u32 keepalive_intvl;        /* Keepalive interval */
    u32 keepalive_probes;       /* Keepalive probes */

    /* Statistics */
    u64 segs_in;                /* Segments received */
    u64 segs_out;               /* Segments sent */
    u64 data_segs_in;           /* Data segments received */
    u64 data_segs_out;          /* Data segments sent */
    u64 bytes_in;               /* Bytes received */
    u64 bytes_out;              /* Bytes sent */
    u64 retrans_segs;           /* Retransmitted segments */

    /* Lock */
    spinlock_t lock;            /* Socket lock */
};

/**
 * tcp_listen_sock - TCP listen socket
 */
struct tcp_listen_sock {
    struct tcp_sock *parent;    /* Parent TCP socket */
    struct sk_buff_head accept_queue;  /* Accept queue */
    u32 max_qlen;               /* Max queue length */
    u32 qlen;                   /* Current queue length */
};

/**
 * tcp_timewait_sock - TCP TIME_WAIT socket
 */
struct tcp_timewait_sock {
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
    u16 sport;                  /* Source port */
    u16 dport;                  /* Destination port */
    u32 rcv_nxt;                /* Next sequence to receive */
    u32 snd_nxt;                /* Next sequence to send */
    u64 expire;                 /* Expiration time */
};

/*===========================================================================*/
/*                         TCP GLOBAL DATA                                   */
/*===========================================================================*/

/* TCP Statistics */
static struct {
    spinlock_t lock;
    atomic64_t segs_in;
    atomic64_t segs_out;
    atomic64_t segs_retrans;
    atomic64_t errors;
    atomic64_t connects;
    atomic64_t accepts;
    atomic64_t closes;
} tcp_stats = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .segs_in = ATOMIC64_INIT(0),
    .segs_out = ATOMIC64_INIT(0),
    .segs_retrans = ATOMIC64_INIT(0),
    .errors = ATOMIC64_INIT(0),
    .connects = ATOMIC64_INIT(0),
    .accepts = ATOMIC64_INIT(0),
    .closes = ATOMIC64_INIT(0),
};

/* TCP Hash Table */
static struct tcp_sock *tcp_hash[TCP_HASH_SIZE];
static spinlock_t tcp_hash_lock = { .lock = SPINLOCK_UNLOCKED };

/* TIME_WAIT Hash Table */
static struct tcp_timewait_sock *tcp_tw_hash[TCP_HASH_SIZE];
static spinlock_t tcp_tw_lock = { .lock = SPINLOCK_UNLOCKED };

/* Default TCP Options */
static struct {
    u16 mss;
    u8 window_scale;
    u8 sack_enabled;
    u8 timestamps_enabled;
} tcp_defaults = {
    .mss = TCP_MSS_DESIRED,
    .window_scale = 7,
    .sack_enabled = 1,
    .timestamps_enabled = 1,
};

/*===========================================================================*/
/*                         TCP HASH FUNCTIONS                                */
/*===========================================================================*/

/**
 * tcp_hash_fn - Hash function for TCP connections
 * @saddr: Source address
 * @daddr: Destination address
 * @sport: Source port
 * @dport: Destination port
 *
 * Returns: Hash value
 */
static inline u32 tcp_hash_fn(u32 saddr, u32 daddr, u16 sport, u16 dport)
{
    u32 hash = saddr;
    hash ^= daddr;
    hash ^= sport;
    hash ^= dport;
    return hash & (TCP_HASH_SIZE - 1);
}

/**
 * tcp_hash_add - Add TCP socket to hash table
 * @tp: TCP socket
 */
static void tcp_hash_add(struct tcp_sock *tp)
{
    u32 hash;

    hash = tcp_hash_fn(tp->saddr, tp->daddr, tp->sport, tp->dport);

    spin_lock(&tcp_hash_lock);

    tp->sock->next = (struct socket *)tcp_hash[hash];
    tp->sock->prev = NULL;
    if (tcp_hash[hash]) {
        ((struct tcp_sock *)tcp_hash[hash])->sock->prev = tp->sock;
    }
    tcp_hash[hash] = tp;

    spin_unlock(&tcp_hash_lock);
}

/**
 * tcp_hash_del - Remove TCP socket from hash table
 * @tp: TCP socket
 */
static void tcp_hash_del(struct tcp_sock *tp)
{
    u32 hash;

    hash = tcp_hash_fn(tp->saddr, tp->daddr, tp->sport, tp->dport);

    spin_lock(&tcp_hash_lock);

    if (tp->sock->prev) {
        ((struct tcp_sock *)tp->sock->prev)->sock->next = tp->sock->next;
    } else {
        tcp_hash[hash] = (struct tcp_sock *)tp->sock->next;
    }

    if (tp->sock->next) {
        ((struct tcp_sock *)tp->sock->next)->sock->prev = tp->sock->prev;
    }

    tp->sock->next = NULL;
    tp->sock->prev = NULL;

    spin_unlock(&tcp_hash_lock);
}

/**
 * tcp_hash_find - Find TCP socket in hash table
 * @saddr: Source address
 * @daddr: Destination address
 * @sport: Source port
 * @dport: Destination port
 *
 * Returns: TCP socket, or NULL if not found
 */
static struct tcp_sock *tcp_hash_find(u32 saddr, u32 daddr, u16 sport, u16 dport)
{
    struct tcp_sock *tp;
    u32 hash;

    hash = tcp_hash_fn(saddr, daddr, sport, dport);

    spin_lock(&tcp_hash_lock);

    tp = tcp_hash[hash];
    while (tp) {
        if (tp->saddr == saddr && tp->daddr == daddr &&
            tp->sport == sport && tp->dport == dport) {
            spin_unlock(&tcp_hash_lock);
            return tp;
        }
        tp = (struct tcp_sock *)tp->sock->next;
    }

    spin_unlock(&tcp_hash_lock);
    return NULL;
}

/*===========================================================================*/
/*                         TCP CHECKSUM                                      */
/*===========================================================================*/

/**
 * tcp_checksum - Compute TCP checksum
 * @iph: IP header
 * @tcph: TCP header
 * @len: TCP segment length
 *
 * Returns: 16-bit checksum
 */
static u16 tcp_checksum(struct iphdr *iph, struct tcphdr *tcph, u32 len)
{
    u32 sum = 0;
    u32 pseudo_header[3];

    /* Pseudo header */
    pseudo_header[0] = iph->saddr;
    pseudo_header[1] = iph->daddr;
    pseudo_header[2] = htonl((IPPROTO_TCP << 16) + len);

    /* Sum pseudo header */
    sum += (pseudo_header[0] >> 16) & 0xFFFF;
    sum += pseudo_header[0] & 0xFFFF;
    sum += (pseudo_header[1] >> 16) & 0xFFFF;
    sum += pseudo_header[1] & 0xFFFF;
    sum += (pseudo_header[2] >> 16) & 0xFFFF;
    sum += pseudo_header[2] & 0xFFFF;

    /* Sum TCP header and data */
    sum = ip_checksum(tcph, len, sum);

    return sum;
}

/**
 * tcp_send_check - Compute and set TCP checksum
 * @iph: IP header
 * @tcph: TCP header
 * @len: TCP segment length
 */
static void tcp_send_check(struct iphdr *iph, struct tcphdr *tcph, u32 len)
{
    tcph->check = 0;
    tcph->check = tcp_checksum(iph, tcph, len);
}

/*===========================================================================*/
/*                         TCP SOCKET ALLOCATION                             */
/*===========================================================================*/

/**
 * tcp_alloc_sock - Allocate TCP socket
 *
 * Returns: TCP socket, or NULL on failure
 */
static struct tcp_sock *tcp_alloc_sock(void)
{
    struct tcp_sock *tp;

    tp = kzalloc(sizeof(struct tcp_sock));
    if (!tp) {
        return NULL;
    }

    /* Initialize state */
    tp->state = TCP_STATE_CLOSED;
    tp->pending = 0;

    /* Initialize sequence numbers */
    tp->snd_nxt = 0;
    tp->snd_una = 0;
    tp->snd_wnd = TCP_DEFAULT_WINDOW;
    tp->iss = net_random();

    tp->rcv_nxt = 0;
    tp->rcv_wnd = TCP_DEFAULT_WINDOW;
    tp->irs = 0;

    /* Initialize RTT */
    tp->rto = TCP_RETRANS_TIMEOUT;
    tp->rtt = 0;
    tp->rttvar = 0;
    tp->srtt = 0;
    tp->retransmits = 0;

    /* Initialize congestion control */
    tp->cwnd = 1;  /* Start with 1 MSS */
    tp->ssthresh = 65535;
    tp->dup_acks = 0;

    /* Set MSS */
    tp->mss = tcp_defaults.mss;
    tp->advmss = tcp_defaults.mss;

    /* Initialize timers */
    tp->timer_expire = 0;
    tp->timer_flags = 0;

    /* Initialize keepalive */
    tp->keepalive_time = TCP_KEEPALIVE_TIME;
    tp->keepalive_intvl = TCP_KEEPALIVE_INTVL;
    tp->keepalive_probes = TCP_KEEPALIVE_PROBES;

    /* Initialize lock */
    spin_lock_init_named(&tp->lock, "tcp_sock");

    return tp;
}

/**
 * tcp_free_sock - Free TCP socket
 * @tp: TCP socket
 */
static void tcp_free_sock(struct tcp_sock *tp)
{
    if (!tp) {
        return;
    }

    kfree(tp);
}

/*===========================================================================*/
/*                         TCP CONNECTION MANAGEMENT                         */
/*===========================================================================*/

/**
 * tcp_set_state - Set TCP state
 * @tp: TCP socket
 * @state: New state
 */
static void tcp_set_state(struct tcp_sock *tp, u8 state)
{
    u8 old_state = tp->state;
    tp->state = state;

    pr_debug("TCP: State change %u -> %u\n", old_state, state);
}

/**
 * tcp_connect_init - Initialize TCP connection
 * @tp: TCP socket
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_connect_init(struct tcp_sock *tp)
{
    if (!tp) {
        return -EINVAL;
    }

    /* Reset sequence numbers */
    tp->snd_nxt = tp->iss;
    tp->snd_una = tp->iss;
    tp->rcv_nxt = 0;

    /* Reset congestion control */
    tp->cwnd = 1;
    tp->ssthresh = 65535;
    tp->dup_acks = 0;

    /* Reset retransmission */
    tp->retransmits = 0;
    tp->rto = TCP_RETRANS_TIMEOUT;

    return 0;
}

/*===========================================================================*/
/*                         TCP SEGMENT CREATION                              */
/*===========================================================================*/

/**
 * tcp_alloc_skb - Allocate TCP socket buffer
 * @tp: TCP socket
 * @size: Data size
 *
 * Returns: Socket buffer, or NULL on failure
 */
static struct sk_buff *tcp_alloc_skb(struct tcp_sock *tp, u32 size)
{
    struct sk_buff *skb;
    u32 total_size;

    total_size = size + sizeof(struct tcphdr) + sizeof(struct iphdr);

    skb = alloc_skb(total_size);
    if (!skb) {
        return NULL;
    }

    /* Reserve space for headers */
    skb_reserve(skb, sizeof(struct iphdr) + sizeof(struct tcphdr));

    return skb;
}

/**
 * tcp_build_segment - Build TCP segment
 * @tp: TCP socket
 * @flags: TCP flags
 * @seq: Sequence number
 * @ack_seq: Acknowledgment sequence
 * @data: Data buffer
 * @data_len: Data length
 *
 * Returns: Socket buffer, or NULL on failure
 */
static struct sk_buff *tcp_build_segment(struct tcp_sock *tp, u8 flags,
                                          u32 seq, u32 ack_seq,
                                          void *data, u32 data_len)
{
    struct sk_buff *skb;
    struct tcphdr *th;
    struct iphdr *iph;
    u32 tcp_len;

    /* Allocate buffer */
    skb = tcp_alloc_skb(tp, data_len);
    if (!skb) {
        return NULL;
    }

    /* Copy data */
    if (data && data_len > 0) {
        memcpy(skb_put(skb, data_len), data, data_len);
    }

    /* Build TCP header */
    th = (struct tcphdr *)skb_push(skb, sizeof(struct tcphdr));
    memset(th, 0, sizeof(struct tcphdr));

    th->source = tp->sport;
    th->dest = tp->dport;
    th->seq = htonl(seq);
    th->ack_seq = htonl(ack_seq);
    th->doff = sizeof(struct tcphdr) / 4;
    th->window = htons(tp->rcv_wnd);

    /* Set flags */
    if (flags & TCP_FLAG_FIN) th->fin = 1;
    if (flags & TCP_FLAG_SYN) th->syn = 1;
    if (flags & TCP_FLAG_RST) th->rst = 1;
    if (flags & TCP_FLAG_PSH) th->psh = 1;
    if (flags & TCP_FLAG_ACK) th->ack = 1;
    if (flags & TCP_FLAG_URG) th->urg = 1;

    /* Build IP header */
    iph = (struct iphdr *)skb_push(skb, sizeof(struct iphdr));
    memset(iph, 0, sizeof(struct iphdr));

    iph->version = IP_VERSION_4;
    iph->ihl = sizeof(struct iphdr) / 4;
    iph->tos = 0;
    iph->tot_len = htons(skb->len);
    iph->id = htons(net_random() & 0xFFFF);
    iph->frag_off = 0;
    iph->ttl = IP_DEFAULT_TTL;
    iph->protocol = IPPROTO_TCP;
    iph->saddr = tp->saddr;
    iph->daddr = tp->daddr;

    /* Calculate checksums */
    tcp_send_check(iph, th, sizeof(struct tcphdr) + data_len);
    ip_send_check(iph);

    return skb;
}

/**
 * tcp_send_segment - Send TCP segment
 * @tp: TCP socket
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_segment(struct tcp_sock *tp, struct sk_buff *skb)
{
    if (!tp || !skb) {
        return -EINVAL;
    }

    /* Update statistics */
    tp->segs_out++;
    tp->bytes_out += skb->len;
    atomic64_inc(&tcp_stats.segs_out);

    /* Send packet */
    return ip_send_skb(skb);
}

/**
 * tcp_send_ack - Send TCP ACK
 * @tp: TCP socket
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_ack(struct tcp_sock *tp)
{
    struct sk_buff *skb;

    skb = tcp_build_segment(tp, TCP_FLAG_ACK,
                            tp->snd_nxt, tp->rcv_nxt,
                            NULL, 0);
    if (!skb) {
        return -ENOMEM;
    }

    return tcp_send_segment(tp, skb);
}

/**
 * tcp_send_syn - Send TCP SYN
 * @tp: TCP socket
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_syn(struct tcp_sock *tp)
{
    struct sk_buff *skb;

    skb = tcp_build_segment(tp, TCP_FLAG_SYN,
                            tp->snd_nxt, 0,
                            NULL, 0);
    if (!skb) {
        return -ENOMEM;
    }

    tp->snd_nxt++;  /* SYN consumes one sequence number */

    return tcp_send_segment(tp, skb);
}

/**
 * tcp_send_synack - Send TCP SYN-ACK
 * @tp: TCP socket
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_synack(struct tcp_sock *tp)
{
    struct sk_buff *skb;

    skb = tcp_build_segment(tp, TCP_FLAG_SYN | TCP_FLAG_ACK,
                            tp->snd_nxt, tp->rcv_nxt,
                            NULL, 0);
    if (!skb) {
        return -ENOMEM;
    }

    tp->snd_nxt++;  /* SYN consumes one sequence number */

    return tcp_send_segment(tp, skb);
}

/**
 * tcp_send_fin - Send TCP FIN
 * @tp: TCP socket
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_fin(struct tcp_sock *tp)
{
    struct sk_buff *skb;

    skb = tcp_build_segment(tp, TCP_FLAG_FIN | TCP_FLAG_ACK,
                            tp->snd_nxt, tp->rcv_nxt,
                            NULL, 0);
    if (!skb) {
        return -ENOMEM;
    }

    tp->snd_nxt++;  /* FIN consumes one sequence number */

    return tcp_send_segment(tp, skb);
}

/**
 * tcp_send_rst - Send TCP RST
 * @tp: TCP socket
 * @seq: Sequence number
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_send_rst(struct tcp_sock *tp, u32 seq)
{
    struct sk_buff *skb;

    skb = tcp_build_segment(tp, TCP_FLAG_RST | TCP_FLAG_ACK,
                            seq, 0,
                            NULL, 0);
    if (!skb) {
        return -ENOMEM;
    }

    return tcp_send_segment(tp, skb);
}

/**
 * tcp_send_data - Send TCP data
 * @tp: TCP socket
 * @data: Data buffer
 * @len: Data length
 *
 * Returns: Number of bytes sent, or negative error code on failure
 */
static int tcp_send_data(struct tcp_sock *tp, void *data, u32 len)
{
    struct sk_buff *skb;
    u32 sent = 0;
    u32 window;

    if (!tp || !data) {
        return -EINVAL;
    }

    if (tp->state != TCP_STATE_ESTABLISHED) {
        return -ENOTCONN;
    }

    while (sent < len) {
        u32 chunk_len;

        /* Calculate window */
        window = MIN(tp->snd_wnd, tp->cwnd * tp->mss);
        if (tp->snd_nxt - tp->snd_una >= window) {
            /* Window is full */
            break;
        }

        /* Calculate chunk size */
        chunk_len = MIN(len - sent, tp->mss);
        chunk_len = MIN(chunk_len, window - (tp->snd_nxt - tp->snd_una));

        /* Build and send segment */
        skb = tcp_build_segment(tp, TCP_FLAG_ACK | TCP_FLAG_PSH,
                                tp->snd_nxt, tp->rcv_nxt,
                                (u8 *)data + sent, chunk_len);
        if (!skb) {
            return sent > 0 ? sent : -ENOMEM;
        }

        tcp_send_segment(tp, skb);

        tp->snd_nxt += chunk_len;
        sent += chunk_len;
        tp->data_segs_out++;
    }

    return sent;
}

/*===========================================================================*/
/*                         TCP SEGMENT PROCESSING                            */
/*===========================================================================*/

/**
 * tcp_rcv_state_process - Process TCP segment in current state
 * @tp: TCP socket
 * @th: TCP header
 * @iph: IP header
 *
 * Returns: 0 on success, negative error code on failure
 */
static int tcp_rcv_state_process(struct tcp_sock *tp, struct tcphdr *th,
                                  struct iphdr *iph)
{
    u32 seq = ntohl(th->seq);
    u32 ack_seq = ntohl(th->ack_seq);
    u16 window = ntohs(th->window);

    /* Update receive window */
    tp->snd_wnd = window;
    tp->snd_wl1 = seq;
    tp->snd_wl2 = ack_seq;

    switch (tp->state) {
        case TCP_STATE_LISTEN:
            if (th->syn) {
                /* Received SYN, move to SYN_RECV */
                tp->irs = seq;
                tp->rcv_nxt = seq + 1;
                tp->snd_nxt = tp->iss;
                tcp_set_state(tp, TCP_STATE_SYN_RECV);
                tcp_send_synack(tp);
            } else {
                /* Unexpected segment */
                tcp_send_rst(tp, ack_seq);
            }
            break;

        case TCP_STATE_SYN_SENT:
            if (th->syn && th->ack) {
                /* Received SYN-ACK */
                if (ack_seq == tp->iss + 1) {
                    tp->irs = seq;
                    tp->rcv_nxt = seq + 1;
                    tp->snd_nxt = tp->iss + 1;
                    tcp_set_state(tp, TCP_STATE_ESTABLISHED);
                    tcp_send_ack(tp);
                }
            } else if (th->syn) {
                /* Simultaneous open */
                tp->irs = seq;
                tp->rcv_nxt = seq + 1;
                tcp_set_state(tp, TCP_STATE_SYN_RECV);
                tcp_send_synack(tp);
            } else {
                tcp_send_rst(tp, ack_seq);
            }
            break;

        case TCP_STATE_SYN_RECV:
            if (th->ack && ack_seq == tp->snd_nxt) {
                /* Received ACK, connection established */
                tcp_set_state(tp, TCP_STATE_ESTABLISHED);
            }
            break;

        case TCP_STATE_ESTABLISHED:
            if (th->rst) {
                /* Connection reset */
                tcp_set_state(tp, TCP_STATE_CLOSED);
            } else if (th->fin) {
                /* Received FIN */
                tp->rcv_nxt = seq + 1;
                tcp_set_state(tp, TCP_STATE_CLOSE_WAIT);
                tcp_send_ack(tp);
            } else if (seq == tp->rcv_nxt) {
                /* Normal data */
                u32 data_len = ntohs(iph->tot_len) - ip_hdrlen(iph) - sizeof(struct tcphdr);
                tp->rcv_nxt += data_len;
                if (th->ack) {
                    tp->snd_una = ack_seq;
                }
                tcp_send_ack(tp);
            }
            break;

        case TCP_STATE_FIN_WAIT1:
            if (th->fin && th->ack) {
                /* Simultaneous close */
                tp->rcv_nxt = seq + 1;
                tp->snd_una = ack_seq;
                tcp_set_state(tp, TCP_STATE_TIME_WAIT);
                tcp_send_ack(tp);
            } else if (th->ack) {
                tp->snd_una = ack_seq;
                tcp_set_state(tp, TCP_STATE_FIN_WAIT2);
            } else if (th->fin) {
                tp->rcv_nxt = seq + 1;
                tcp_set_state(tp, TCP_STATE_CLOSING);
                tcp_send_ack(tp);
            }
            break;

        case TCP_STATE_FIN_WAIT2:
            if (th->fin) {
                tp->rcv_nxt = seq + 1;
                tcp_set_state(tp, TCP_STATE_TIME_WAIT);
                tcp_send_ack(tp);
            }
            break;

        case TCP_STATE_CLOSE_WAIT:
            /* Application should close */
            break;

        case TCP_STATE_CLOSING:
            if (th->ack && ack_seq == tp->snd_nxt) {
                tcp_set_state(tp, TCP_STATE_TIME_WAIT);
            }
            break;

        case TCP_STATE_LAST_ACK:
            if (th->ack && ack_seq == tp->snd_nxt) {
                tcp_set_state(tp, TCP_STATE_CLOSED);
            }
            break;

        case TCP_STATE_TIME_WAIT:
            /* Discard segment */
            tcp_send_ack(tp);
            break;

        case TCP_STATE_CLOSED:
            tcp_send_rst(tp, ack_seq);
            break;
    }

    return 0;
}

/**
 * tcp_v4_rcv - Receive TCP segment
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_v4_rcv(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr *th;
    struct tcp_sock *tp;
    u32 saddr, daddr;
    u16 sport, dport;
    u32 tcp_len;

    if (!skb) {
        return -EINVAL;
    }

    atomic64_inc(&tcp_stats.segs_in);

    /* Get IP header */
    iph = (struct iphdr *)skb->data;

    /* Get TCP header */
    th = (struct tcphdr *)(skb->data + ip_hdrlen(iph));
    tcp_len = ntohs(iph->tot_len) - ip_hdrlen(iph);

    /* Validate TCP length */
    if (tcp_len < sizeof(struct tcphdr)) {
        atomic64_inc(&tcp_stats.errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate checksum */
    if (tcp_checksum(iph, th, tcp_len) != 0) {
        atomic64_inc(&tcp_stats.errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Extract addresses and ports */
    saddr = iph->saddr;
    daddr = iph->daddr;
    sport = th->source;
    dport = th->dest;

    /* Find socket */
    tp = tcp_hash_find(saddr, daddr, sport, dport);

    if (!tp) {
        /* No socket found, send RST */
        /* In a full implementation, we would create a temporary socket */
        free_skb(skb);
        return -ENOENT;
    }

    /* Update statistics */
    tp->segs_in++;
    tp->bytes_in += skb->len;
    tp->data_segs_in++;

    /* Process segment */
    spin_lock(&tp->lock);
    tcp_rcv_state_process(tp, th, iph);
    spin_unlock(&tp->lock);

    free_skb(skb);

    return 0;
}

/*===========================================================================*/
/*                         TCP SOCKET OPERATIONS                             */
/*===========================================================================*/

/**
 * tcp_create - Create TCP socket
 * @sock: Socket structure
 * @protocol: Protocol number
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_create(struct socket *sock, int protocol)
{
    struct tcp_sock *tp;

    if (!sock) {
        return -EINVAL;
    }

    /* Allocate TCP socket */
    tp = tcp_alloc_sock();
    if (!tp) {
        return -ENOMEM;
    }

    tp->sock = sock;
    sock->sk_protinfo = tp;

    pr_debug("TCP: Created socket %p\n", tp);

    return 0;
}

/**
 * tcp_release - Release TCP socket
 * @sock: Socket structure
 *
 * Returns: 0 on success
 */
int tcp_release(struct socket *sock)
{
    struct tcp_sock *tp;

    if (!sock) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return 0;
    }

    /* Remove from hash table */
    if (tp->state != TCP_STATE_CLOSED) {
        tcp_hash_del(tp);
    }

    /* Send FIN if connected */
    if (tp->state == TCP_STATE_ESTABLISHED) {
        tcp_send_fin(tp);
        tcp_set_state(tp, TCP_STATE_FIN_WAIT1);
    }

    atomic64_inc(&tcp_stats.closes);

    tcp_free_sock(tp);
    sock->sk_protinfo = NULL;

    return 0;
}

/**
 * tcp_bind - Bind TCP socket
 * @sock: Socket structure
 * @addr: Address to bind to
 * @addr_len: Address length
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
    struct tcp_sock *tp;
    struct sockaddr_in *addr_in;

    if (!sock || !addr) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    addr_in = (struct sockaddr_in *)addr;

    spin_lock(&tp->lock);

    tp->saddr = addr_in->sin_addr;
    tp->sport = addr_in->sin_port;

    spin_unlock(&tp->lock);

    return 0;
}

/**
 * tcp_connect - Connect TCP socket
 * @sock: Socket structure
 * @addr: Address to connect to
 * @addr_len: Address length
 * @flags: Connection flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_connect(struct socket *sock, struct sockaddr *addr, int addr_len, int flags)
{
    struct tcp_sock *tp;
    struct sockaddr_in *addr_in;
    int ret;

    if (!sock || !addr) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    addr_in = (struct sockaddr_in *)addr;

    spin_lock(&tp->lock);

    /* Set remote address */
    tp->daddr = addr_in->sin_addr;
    tp->dport = addr_in->sin_port;

    /* Allocate local port if needed */
    if (tp->sport == 0) {
        tp->sport = ip_local_port_alloc();
        if (tp->sport == 0) {
            spin_unlock(&tp->lock);
            return -EADDRNOTAVAIL;
        }
    }

    /* Initialize connection */
    tcp_connect_init(tp);

    /* Add to hash table */
    tcp_hash_add(tp);

    /* Change state */
    tcp_set_state(tp, TCP_STATE_SYN_SENT);

    /* Send SYN */
    ret = tcp_send_syn(tp);

    atomic64_inc(&tcp_stats.connects);

    spin_unlock(&tp->lock);

    return ret;
}

/**
 * tcp_listen - Listen for connections
 * @sock: Socket structure
 * @backlog: Maximum pending connections
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_listen(struct socket *sock, int backlog)
{
    struct tcp_sock *tp;

    if (!sock) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    spin_lock(&tp->lock);

    if (tp->state != TCP_STATE_CLOSED) {
        spin_unlock(&tp->lock);
        return -EINVAL;
    }

    tcp_set_state(tp, TCP_STATE_LISTEN);

    spin_unlock(&tp->lock);

    return 0;
}

/**
 * tcp_accept - Accept connection
 * @sock: Listening socket
 * @newsock: New socket for connection
 * @flags: Accept flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_accept(struct socket *sock, struct socket *newsock, int flags)
{
    /* In a full implementation, this would accept connections */
    return -EAGAIN;
}

/**
 * tcp_sendmsg - Send message
 * @sock: Socket structure
 * @msg: Message data
 * @len: Message length
 * @flags: Send flags
 *
 * Returns: Number of bytes sent, or negative error code on failure
 */
int tcp_sendmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct tcp_sock *tp;

    if (!sock || !msg) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    return tcp_send_data(tp, msg, len);
}

/**
 * tcp_recvmsg - Receive message
 * @sock: Socket structure
 * @msg: Buffer for message
 * @len: Buffer length
 * @flags: Receive flags
 *
 * Returns: Number of bytes received, or negative error code on failure
 */
int tcp_recvmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct tcp_sock *tp;
    struct sk_buff *skb;

    if (!sock || !msg) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    /* Dequeue packet */
    skb = sock_dequeue_rcv_skb(sock);
    if (!skb) {
        return -EAGAIN;
    }

    /* Copy data */
    len = MIN(len, skb->len);
    memcpy(msg, skb->data, len);

    free_skb(skb);

    return len;
}

/**
 * tcp_shutdown - Shutdown TCP connection
 * @sock: Socket structure
 * @how: Shutdown mode
 *
 * Returns: 0 on success, negative error code on failure
 */
int tcp_shutdown(struct socket *sock, int how)
{
    struct tcp_sock *tp;

    if (!sock) {
        return -EINVAL;
    }

    tp = (struct tcp_sock *)sock->sk_protinfo;
    if (!tp) {
        return -EINVAL;
    }

    if (how == SHUT_WR || how == SHUT_RDWR) {
        if (tp->state == TCP_STATE_ESTABLISHED) {
            tcp_send_fin(tp);
            tcp_set_state(tp, TCP_STATE_FIN_WAIT1);
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         TCP STATISTICS                                    */
/*===========================================================================*/

/**
 * tcp_stats_print - Print TCP statistics
 */
void tcp_stats_print(void)
{
    spin_lock(&tcp_stats.lock);

    printk("\n=== TCP Statistics ===\n");
    printk("Segments In: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.segs_in));
    printk("Segments Out: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.segs_out));
    printk("Segments Retrans: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.segs_retrans));
    printk("Errors: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.errors));
    printk("Connects: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.connects));
    printk("Accepts: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.accepts));
    printk("Closes: %llu\n", (unsigned long long)atomic64_read(&tcp_stats.closes));

    spin_unlock(&tcp_stats.lock);
}

/**
 * tcp_stats_reset - Reset TCP statistics
 */
void tcp_stats_reset(void)
{
    spin_lock(&tcp_stats.lock);

    atomic64_set(&tcp_stats.segs_in, 0);
    atomic64_set(&tcp_stats.segs_out, 0);
    atomic64_set(&tcp_stats.segs_retrans, 0);
    atomic64_set(&tcp_stats.errors, 0);
    atomic64_set(&tcp_stats.connects, 0);
    atomic64_set(&tcp_stats.accepts, 0);
    atomic64_set(&tcp_stats.closes, 0);

    spin_unlock(&tcp_stats.lock);
}

/*===========================================================================*/
/*                         TCP INITIALIZATION                                */
/*===========================================================================*/

/**
 * tcp_init - Initialize TCP layer
 */
void tcp_init(void)
{
    int i;

    pr_info("Initializing TCP Layer...\n");

    /* Initialize hash table */
    for (i = 0; i < TCP_HASH_SIZE; i++) {
        tcp_hash[i] = NULL;
        tcp_tw_hash[i] = NULL;
    }

    spin_lock_init_named(&tcp_hash_lock, "tcp_hash");
    spin_lock_init_named(&tcp_tw_lock, "tcp_tw");

    /* Initialize statistics */
    spin_lock_init_named(&tcp_stats.lock, "tcp_stats");

    pr_info("  Hash table: %u buckets\n", TCP_HASH_SIZE);
    pr_info("  Default MSS: %u\n", tcp_defaults.mss);
    pr_info("  Window scale: %u\n", tcp_defaults.window_scale);
    pr_info("  SACK: %s\n", tcp_defaults.sack_enabled ? "enabled" : "disabled");
    pr_info("  Timestamps: %s\n", tcp_defaults.timestamps_enabled ? "enabled" : "disabled");
    pr_info("TCP Layer initialized.\n");
}

/**
 * tcp_exit - Shutdown TCP layer
 */
void tcp_exit(void)
{
    pr_info("Shutting down TCP Layer...\n");

    /* Clean up all sockets */
    /* In a full implementation, this would close all connections */

    pr_info("TCP Layer shutdown complete.\n");
}
