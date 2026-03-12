/*
 * NEXUS OS - ICMP Implementation
 * net/ipv4/icmp.c
 */

#include "ipv4.h"

/*===========================================================================*/
/*                         ICMP CONFIGURATION                                */
/*===========================================================================*/

#define ICMP_RATE_LIMIT         100       /* Messages per second */
#define ICMP_TIMESTAMP_TIMEOUT  1000      /* 1 second */
#define ICMP_ECHO_TIMEOUT       5000      /* 5 seconds */

/* ICMP Types */
#define ICMP_ECHOREPLY          0         /* Echo Reply */
#define ICMP_DEST_UNREACH       3         /* Destination Unreachable */
#define ICMP_SOURCE_QUENCH      4         /* Source Quench */
#define ICMP_REDIRECT           5         /* Redirect */
#define ICMP_ECHO               8         /* Echo Request */
#define ICMP_ROUTER_ADVERT      9         /* Router Advertisement */
#define ICMP_ROUTER_SOLICIT     10        /* Router Solicitation */
#define ICMP_TIME_EXCEEDED      11        /* Time Exceeded */
#define ICMP_PARAMETERPROB      12        /* Parameter Problem */
#define ICMP_TIMESTAMP          13        /* Timestamp Request */
#define ICMP_TIMESTAMPREPLY     14        /* Timestamp Reply */
#define ICMP_INFO_REQUEST       15        /* Information Request */
#define ICMP_INFO_REPLY         16        /* Information Reply */
#define ICMP_ADDRESS            17        /* Address Mask Request */
#define ICMP_ADDRESSREPLY       18        /* Address Mask Reply */

/* ICMP Destination Unreachable Codes */
#define ICMP_NET_UNREACH        0         /* Network Unreachable */
#define ICMP_HOST_UNREACH       1         /* Host Unreachable */
#define ICMP_PROT_UNREACH       2         /* Protocol Unreachable */
#define ICMP_PORT_UNREACH       3         /* Port Unreachable */
#define ICMP_FRAG_NEEDED        4         /* Fragmentation Needed */
#define ICMP_SR_FAILED          5         /* Source Route Failed */
#define ICMP_NET_UNKNOWN        6         /* Network Unknown */
#define ICMP_HOST_UNKNOWN       7         /* Host Unknown */
#define ICMP_HOST_ISOLATED      8         /* Host Isolated */
#define ICMP_NET_ANO            9         /* Network Administratively Prohibited */
#define ICMP_HOST_ANO           10        /* Host Administratively Prohibited */
#define ICMP_NET_UNR_TOS        11        /* Network Unreachable for TOS */
#define ICMP_HOST_UNR_TOS       12        /* Host Unreachable for TOS */
#define ICMP_PKT_FILTERED       13        /* Packet Filtered */
#define ICMP_PREC_VIOLATION     14        /* Precedence Violation */
#define ICMP_PREC_CUTOFF        15        /* Precedence Cutoff */

/* ICMP Time Exceeded Codes */
#define ICMP_EXC_TTL            0         /* TTL Count Exceeded */
#define ICMP_EXC_FRAGTIME       1         /* Fragment Reassembly Time Exceeded */

/* ICMP Redirect Codes */
#define ICMP_REDIR_NET          0         /* Redirect Network */
#define ICMP_REDIR_HOST         1         /* Redirect Host */
#define ICMP_REDIR_NETTOS       2         /* Redirect Network for TOS */
#define ICMP_REDIR_HOSTTOS      3         /* Redirect Host for TOS */

/* ICMP Parameter Problem Codes */
#define ICMP_PTR_INDIC          0         /* Pointer Indicates Error */
#define ICMP_MISSING_OPT        1         /* Missing Required Option */
#define ICMP_BAD_LENGTH         2         /* Bad Length */

/* ICMP Flags */
#define ICMP_FLAG_RATE_LIMITED  0x0001

/*===========================================================================*/
/*                         ICMP STRUCTURES                                   */
/*===========================================================================*/

/**
 * icmphdr - ICMP header structure
 */
struct icmphdr {
    u8 type;                    /* ICMP type */
    u8 code;                    /* ICMP code */
    u16 checksum;               /* Checksum */
    union {
        struct {
            u16 id;             /* Identifier */
            u16 sequence;       /* Sequence number */
        } echo;                 /* Echo Request/Reply */
        struct {
            u32 gateway;        /* Gateway address */
            u32 unused;         /* Unused */
        } redirect;             /* Redirect */
        struct {
            u32 unused;         /* Unused */
        } dest_unreach;         /* Destination Unreachable */
        struct {
            u32 pointer;        /* Pointer to error */
            u32 unused;         /* Unused */
        } parameter;            /* Parameter Problem */
        struct {
            u32 orig_time;      /* Originate timestamp */
            u32 recv_time;      /* Receive timestamp */
            u32 trans_time;     /* Transmit timestamp */
        } timestamp;            /* Timestamp */
        u32 gateway;            /* Gateway for redirects */
    } un;
} __packed;

/**
 * icmp_echo - ICMP Echo structure
 */
struct icmp_echo {
    struct icmphdr hdr;
    u8 data[0];                 /* Variable length data */
} __packed;

/**
 * icmp_bhdr - ICMP bad header structure
 */
struct icmp_bhdr {
    u8 ihl:4;                   /* IP header length */
    u8 version:4;               /* IP version */
    u8 tos;                     /* Type of service */
    u16 tot_len;                /* Total length */
    u16 id;                     /* Identification */
    u16 frag_off;               /* Fragment offset */
    u8 ttl;                     /* Time to live */
    u8 protocol;                /* Protocol */
    u16 check;                  /* Checksum */
    u32 saddr;                  /* Source address */
    u32 daddr;                  /* Destination address */
} __packed;

/**
 * icmp_error - ICMP error message structure
 */
struct icmp_error {
    struct icmphdr hdr;
    struct icmp_bhdr ip;
    u8 data[8];                 /* First 8 bytes of original data */
} __packed;

/**
 * icmp_statistics - ICMP statistics
 */
struct icmp_statistics {
    /* Messages */
    atomic64_t in_msgs;         /* Messages received */
    atomic64_t out_msgs;        /* Messages sent */
    atomic64_t in_errors;       /* Receive errors */
    atomic64_t out_errors;      /* Send errors */

    /* By Type - Received */
    atomic64_t in_echo;         /* Echo requests */
    atomic64_t in_echo_reply;   /* Echo replies */
    atomic64_t in_dest_unreach; /* Destination unreachable */
    atomic64_t in_time_exceeded;/* Time exceeded */
    atomic64_t in_redirect;     /* Redirects */
    atomic64_t in_timestamp;    /* Timestamp requests */
    atomic64_t in_timestamp_reply; /* Timestamp replies */
    atomic64_t in_param_prob;   /* Parameter problems */

    /* By Type - Sent */
    atomic64_t out_echo;        /* Echo requests */
    atomic64_t out_echo_reply;  /* Echo replies */
    atomic64_t out_dest_unreach;/* Destination unreachable */
    atomic64_t out_time_exceeded; /* Time exceeded */
    atomic64_t out_redirect;    /* Redirects */
    atomic64_t out_timestamp;   /* Timestamp requests */
    atomic64_t out_timestamp_reply; /* Timestamp replies */
    atomic64_t out_param_prob;  /* Parameter problems */

    /* Rate Limiting */
    atomic64_t rate_limited;    /* Rate limited messages */
};

/*===========================================================================*/
/*                         ICMP GLOBAL DATA                                  */
/*===========================================================================*/

/* ICMP Statistics */
static struct icmp_statistics icmp_stats = {
    .in_msgs = ATOMIC64_INIT(0),
    .out_msgs = ATOMIC64_INIT(0),
    .in_errors = ATOMIC64_INIT(0),
    .out_errors = ATOMIC64_INIT(0),
    .in_echo = ATOMIC64_INIT(0),
    .in_echo_reply = ATOMIC64_INIT(0),
    .in_dest_unreach = ATOMIC64_INIT(0),
    .in_time_exceeded = ATOMIC64_INIT(0),
    .in_redirect = ATOMIC64_INIT(0),
    .in_timestamp = ATOMIC64_INIT(0),
    .in_timestamp_reply = ATOMIC64_INIT(0),
    .in_param_prob = ATOMIC64_INIT(0),
    .out_echo = ATOMIC64_INIT(0),
    .out_echo_reply = ATOMIC64_INIT(0),
    .out_dest_unreach = ATOMIC64_INIT(0),
    .out_time_exceeded = ATOMIC64_INIT(0),
    .out_redirect = ATOMIC64_INIT(0),
    .out_timestamp = ATOMIC64_INIT(0),
    .out_timestamp_reply = ATOMIC64_INIT(0),
    .out_param_prob = ATOMIC64_INIT(0),
    .rate_limited = ATOMIC64_INIT(0),
};

/* Rate Limiting */
static struct {
    spinlock_t lock;
    u64 last_time;
    u32 count;
} icmp_rate_limit = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .last_time = 0,
    .count = 0,
};

/* Echo Tracking */
static struct {
    spinlock_t lock;
    u16 id;
    u16 sequence;
} icmp_echo_tracker = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .id = 0,
    .sequence = 0,
};

/*===========================================================================*/
/*                         ICMP CHECKSUM                                     */
/*===========================================================================*/

/**
 * icmp_checksum - Compute ICMP checksum
 * @icmph: ICMP header
 * @len: ICMP message length
 *
 * Returns: 16-bit checksum
 */
static u16 icmp_checksum(struct icmphdr *icmph, u32 len)
{
    return ip_compute_csum(icmph, len);
}

/**
 * icmp_send_check - Compute and set ICMP checksum
 * @icmph: ICMP header
 * @len: ICMP message length
 */
static void icmp_send_check(struct icmphdr *icmph, u32 len)
{
    icmph->checksum = 0;
    icmph->checksum = icmp_checksum(icmph, len);
}

/*===========================================================================*/
/*                         ICMP RATE LIMITING                                */
/*===========================================================================*/

/**
 * icmp_rate_limit_check - Check rate limit
 *
 * Returns: true if allowed, false if rate limited
 */
static bool icmp_rate_limit_check(void)
{
    u64 now;
    bool allowed;

    spin_lock(&icmp_rate_limit.lock);

    now = get_time_ms();

    /* Reset counter every second */
    if (now - icmp_rate_limit.last_time >= 1000) {
        icmp_rate_limit.count = 0;
        icmp_rate_limit.last_time = now;
    }

    /* Check limit */
    if (icmp_rate_limit.count >= ICMP_RATE_LIMIT) {
        allowed = false;
        atomic64_inc(&icmp_stats.rate_limited);
    } else {
        icmp_rate_limit.count++;
        allowed = true;
    }

    spin_unlock(&icmp_rate_limit.lock);

    return allowed;
}

/*===========================================================================*/
/*                         ICMP MESSAGE CREATION                             */
/*===========================================================================*/

/**
 * icmp_build_message - Build ICMP message
 * @type: ICMP type
 * @code: ICMP code
 * @data: Data buffer
 * @len: Data length
 *
 * Returns: Socket buffer, or NULL on failure
 */
static struct sk_buff *icmp_build_message(u8 type, u8 code, void *data, u32 len)
{
    struct sk_buff *skb;
    struct icmphdr *icmph;

    /* Allocate buffer */
    skb = alloc_skb(sizeof(struct icmphdr) + len);
    if (!skb) {
        return NULL;
    }

    /* Copy data */
    if (data && len > 0) {
        memcpy(skb_put(skb, len), data, len);
    }

    /* Build ICMP header */
    icmph = (struct icmphdr *)skb_push(skb, sizeof(struct icmphdr));
    memset(icmph, 0, sizeof(struct icmphdr));

    icmph->type = type;
    icmph->code = code;

    return skb;
}

/**
 * icmp_build_error - Build ICMP error message
 * @type: ICMP type
 * @code: ICMP code
 * @iph: Original IP header
 * @data: Original data (first 8 bytes)
 *
 * Returns: Socket buffer, or NULL on failure
 */
static struct sk_buff *icmp_build_error(u8 type, u8 code,
                                         struct iphdr *iph, u8 *data)
{
    struct sk_buff *skb;
    struct icmp_error *err;
    u32 ip_hdr_len;

    /* Get original IP header length */
    ip_hdr_len = iph->ihl * 4;

    /* Allocate buffer */
    skb = alloc_skb(sizeof(struct icmp_error));
    if (!skb) {
        return NULL;
    }

    /* Build ICMP error message */
    err = (struct icmp_error *)skb_put(skb, sizeof(struct icmp_error));
    memset(err, 0, sizeof(struct icmp_error));

    err->hdr.type = type;
    err->hdr.code = code;

    /* Copy original IP header */
    memcpy(&err->ip, iph, sizeof(struct icmp_bhdr));

    /* Copy first 8 bytes of original data */
    memcpy(err->data, data, 8);

    return skb;
}

/**
 * icmp_send - Send ICMP message
 * @daddr: Destination address
 * @skb: Socket buffer containing ICMP message
 *
 * Returns: 0 on success, negative error code on failure
 */
static int icmp_send(u32 daddr, struct sk_buff *skb)
{
    struct icmphdr *icmph;
    struct iphdr *iph;
    u32 icmp_len;

    if (!skb) {
        return -EINVAL;
    }

    /* Get ICMP header */
    icmph = (struct icmphdr *)skb->data;

    /* Calculate checksum */
    icmp_len = skb->len;
    icmp_send_check(icmph, icmp_len);

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
    iph->protocol = IPPROTO_ICMP;
    iph->saddr = 0;  /* Will be set by IP layer */
    iph->daddr = daddr;

    /* Calculate IP checksum */
    ip_send_check(iph);

    /* Update statistics */
    atomic64_inc(&icmp_stats.out_msgs);

    /* Send packet */
    return ip_send_skb(skb);
}

/*===========================================================================*/
/*                         ICMP ERROR MESSAGES                               */
/*===========================================================================*/

/**
 * icmp_send_dest_unreach - Send Destination Unreachable
 * @iph: Original IP header
 * @code: Unreachable code
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_send_dest_unreach(struct iphdr *iph, u8 code)
{
    struct sk_buff *skb;
    u8 *data;

    /* Check rate limit */
    if (!icmp_rate_limit_check()) {
        return -EBUSY;
    }

    /* Get original data */
    data = (u8 *)iph + (iph->ihl * 4);

    /* Build error message */
    skb = icmp_build_error(ICMP_DEST_UNREACH, code, iph, data);
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Send to original source */
    atomic64_inc(&icmp_stats.out_dest_unreach);
    return icmp_send(iph->saddr, skb);
}

/**
 * icmp_send_time_exceeded - Send Time Exceeded
 * @iph: Original IP header
 * @code: Time exceeded code
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_send_time_exceeded(struct iphdr *iph, u8 code)
{
    struct sk_buff *skb;
    u8 *data;

    /* Check rate limit */
    if (!icmp_rate_limit_check()) {
        return -EBUSY;
    }

    /* Get original data */
    data = (u8 *)iph + (iph->ihl * 4);

    /* Build error message */
    skb = icmp_build_error(ICMP_TIME_EXCEEDED, code, iph, data);
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Send to original source */
    atomic64_inc(&icmp_stats.out_time_exceeded);
    return icmp_send(iph->saddr, skb);
}

/**
 * icmp_send_param_prob - Send Parameter Problem
 * @iph: Original IP header
 * @code: Parameter problem code
 * @pointer: Pointer to error location
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_send_param_prob(struct iphdr *iph, u8 code, u8 pointer)
{
    struct sk_buff *skb;
    struct icmp_error *err;
    u8 *data;

    /* Check rate limit */
    if (!icmp_rate_limit_check()) {
        return -EBUSY;
    }

    /* Get original data */
    data = (u8 *)iph + (iph->ihl * 4);

    /* Build error message */
    skb = icmp_build_error(ICMP_PARAMETERPROB, code, iph, data);
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Set pointer */
    err = (struct icmp_error *)skb->data;
    err->hdr.un.parameter.pointer = pointer;

    /* Send to original source */
    atomic64_inc(&icmp_stats.out_param_prob);
    return icmp_send(iph->saddr, skb);
}

/**
 * icmp_send_redirect - Send Redirect
 * @iph: Original IP header
 * @code: Redirect code
 * @gateway: Gateway address
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_send_redirect(struct iphdr *iph, u8 code, u32 gateway)
{
    struct sk_buff *skb;
    struct icmphdr *icmph;

    /* Check rate limit */
    if (!icmp_rate_limit_check()) {
        return -EBUSY;
    }

    /* Build message */
    skb = icmp_build_message(ICMP_REDIRECT, code, NULL, 0);
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Set gateway */
    icmph = (struct icmphdr *)skb->data;
    icmph->un.gateway = gateway;

    /* Send to original source */
    atomic64_inc(&icmp_stats.out_redirect);
    return icmp_send(iph->saddr, skb);
}

/*===========================================================================*/
/*                         ICMP ECHO                                         */
/*===========================================================================*/

/**
 * icmp_send_echo - Send Echo Request
 * @daddr: Destination address
 * @data: Data buffer
 * @len: Data length
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_send_echo(u32 daddr, void *data, u32 len)
{
    struct sk_buff *skb;
    struct icmp_echo *echo;
    u16 id, seq;

    /* Allocate sequence number */
    spin_lock(&icmp_echo_tracker.lock);
    if (icmp_echo_tracker.id == 0) {
        icmp_echo_tracker.id = net_random() & 0xFFFF;
    }
    id = icmp_echo_tracker.id;
    seq = icmp_echo_tracker.sequence++;
    spin_unlock(&icmp_echo_tracker.lock);

    /* Build message */
    skb = icmp_build_message(ICMP_ECHO, 0, data, len);
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Set echo header */
    echo = (struct icmp_echo *)skb->data;
    echo->hdr.un.echo.id = htons(id);
    echo->hdr.un.echo.sequence = htons(seq);

    /* Send */
    atomic64_inc(&icmp_stats.out_echo);
    return icmp_send(daddr, skb);
}

/**
 * icmp_send_echo_reply - Send Echo Reply
 * @iph: Original IP header
 * @echo: Original echo request
 * @len: Total length
 *
 * Returns: 0 on success, negative error code on failure
 */
static int icmp_send_echo_reply(struct iphdr *iph, struct icmp_echo *echo, u32 len)
{
    struct sk_buff *skb;
    struct icmp_echo *reply;

    /* Build reply */
    skb = icmp_build_message(ICMP_ECHOREPLY, 0, echo->data, len - sizeof(struct icmphdr));
    if (!skb) {
        atomic64_inc(&icmp_stats.out_errors);
        return -ENOMEM;
    }

    /* Copy echo header and set reply type */
    reply = (struct icmp_echo *)skb->data;
    reply->hdr.un.echo.id = echo->hdr.un.echo.id;
    reply->hdr.un.echo.sequence = echo->hdr.un.echo.sequence;

    /* Send to original source */
    atomic64_inc(&icmp_stats.out_echo_reply);
    return icmp_send(iph->saddr, skb);
}

/*===========================================================================*/
/*                         ICMP MESSAGE PROCESSING                           */
/*===========================================================================*/

/**
 * icmp_rcv - Receive ICMP message
 * @skb: Socket buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int icmp_rcv(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct icmphdr *icmph;
    u32 icmp_len;
    u16 checksum;

    if (!skb) {
        return -EINVAL;
    }

    atomic64_inc(&icmp_stats.in_msgs);

    /* Get IP header */
    iph = (struct iphdr *)skb->data;

    /* Get ICMP header */
    icmph = (struct icmphdr *)(skb->data + ip_hdrlen(iph));
    icmp_len = ntohs(iph->tot_len) - ip_hdrlen(iph);

    /* Validate length */
    if (icmp_len < sizeof(struct icmphdr)) {
        atomic64_inc(&icmp_stats.in_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Validate checksum */
    checksum = icmp_checksum(icmph, icmp_len);
    if (checksum != 0) {
        atomic64_inc(&icmp_stats.in_errors);
        free_skb(skb);
        return -EINVAL;
    }

    /* Process by type */
    switch (icmph->type) {
        case ICMP_ECHO:
            atomic64_inc(&icmp_stats.in_echo);
            /* Send echo reply */
            icmp_send_echo_reply(iph, (struct icmp_echo *)icmph, icmp_len);
            break;

        case ICMP_ECHOREPLY:
            atomic64_inc(&icmp_stats.in_echo_reply);
            /* In a full implementation, wake up waiting process */
            break;

        case ICMP_DEST_UNREACH:
            atomic64_inc(&icmp_stats.in_dest_unreach);
            /* In a full implementation, notify upper layer */
            break;

        case ICMP_TIME_EXCEEDED:
            atomic64_inc(&icmp_stats.in_time_exceeded);
            /* In a full implementation, notify upper layer */
            break;

        case ICMP_REDIRECT:
            atomic64_inc(&icmp_stats.in_redirect);
            /* In a full implementation, update routing table */
            break;

        case ICMP_TIMESTAMP:
            atomic64_inc(&icmp_stats.in_timestamp);
            /* Send timestamp reply */
            /* In a full implementation */
            break;

        case ICMP_TIMESTAMPREPLY:
            atomic64_inc(&icmp_stats.in_timestamp_reply);
            /* In a full implementation, wake up waiting process */
            break;

        case ICMP_PARAMETERPROB:
            atomic64_inc(&icmp_stats.in_param_prob);
            /* In a full implementation, notify upper layer */
            break;

        default:
            /* Unknown type, silently discard */
            break;
    }

    free_skb(skb);

    return 0;
}

/*===========================================================================*/
/*                         ICMP STATISTICS                                   */
/*===========================================================================*/

/**
 * icmp_stats_print - Print ICMP statistics
 */
void icmp_stats_print(void)
{
    printk("\n=== ICMP Statistics ===\n");
    printk("InMsgs: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_msgs));
    printk("OutMsgs: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_msgs));
    printk("InErrors: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_errors));
    printk("OutErrors: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_errors));
    printk("RateLimited: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.rate_limited));
    printk("\nReceived:\n");
    printk("  InEcho: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_echo));
    printk("  InEchoReply: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_echo_reply));
    printk("  InDestUnreach: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_dest_unreach));
    printk("  InTimeExceeded: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_time_exceeded));
    printk("  InRedirect: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_redirect));
    printk("  InTimestamp: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_timestamp));
    printk("  InTimestampReply: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_timestamp_reply));
    printk("  InParamProb: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.in_param_prob));
    printk("\nSent:\n");
    printk("  OutEcho: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_echo));
    printk("  OutEchoReply: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_echo_reply));
    printk("  OutDestUnreach: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_dest_unreach));
    printk("  OutTimeExceeded: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_time_exceeded));
    printk("  OutRedirect: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_redirect));
    printk("  OutTimestamp: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_timestamp));
    printk("  OutTimestampReply: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_timestamp_reply));
    printk("  OutParamProb: %llu\n", (unsigned long long)atomic64_read(&icmp_stats.out_param_prob));
}

/**
 * icmp_stats_reset - Reset ICMP statistics
 */
void icmp_stats_reset(void)
{
    atomic64_set(&icmp_stats.in_msgs, 0);
    atomic64_set(&icmp_stats.out_msgs, 0);
    atomic64_set(&icmp_stats.in_errors, 0);
    atomic64_set(&icmp_stats.out_errors, 0);
    atomic64_set(&icmp_stats.in_echo, 0);
    atomic64_set(&icmp_stats.in_echo_reply, 0);
    atomic64_set(&icmp_stats.in_dest_unreach, 0);
    atomic64_set(&icmp_stats.in_time_exceeded, 0);
    atomic64_set(&icmp_stats.in_redirect, 0);
    atomic64_set(&icmp_stats.in_timestamp, 0);
    atomic64_set(&icmp_stats.in_timestamp_reply, 0);
    atomic64_set(&icmp_stats.in_param_prob, 0);
    atomic64_set(&icmp_stats.out_echo, 0);
    atomic64_set(&icmp_stats.out_echo_reply, 0);
    atomic64_set(&icmp_stats.out_dest_unreach, 0);
    atomic64_set(&icmp_stats.out_time_exceeded, 0);
    atomic64_set(&icmp_stats.out_redirect, 0);
    atomic64_set(&icmp_stats.out_timestamp, 0);
    atomic64_set(&icmp_stats.out_timestamp_reply, 0);
    atomic64_set(&icmp_stats.out_param_prob, 0);
    atomic64_set(&icmp_stats.rate_limited, 0);
}

/*===========================================================================*/
/*                         ICMP INITIALIZATION                               */
/*===========================================================================*/

/**
 * icmp_init - Initialize ICMP layer
 */
void icmp_init(void)
{
    pr_info("Initializing ICMP Layer...\n");

    /* Initialize rate limiting */
    spin_lock_init_named(&icmp_rate_limit.lock, "icmp_rate_limit");
    icmp_rate_limit.last_time = get_time_ms();
    icmp_rate_limit.count = 0;

    /* Initialize echo tracker */
    spin_lock_init_named(&icmp_echo_tracker.lock, "icmp_echo_tracker");
    icmp_echo_tracker.id = 0;
    icmp_echo_tracker.sequence = 0;

    pr_info("  Rate limit: %u messages/second\n", ICMP_RATE_LIMIT);
    pr_info("ICMP Layer initialized.\n");
}

/**
 * icmp_exit - Shutdown ICMP layer
 */
void icmp_exit(void)
{
    pr_info("Shutting down ICMP Layer...\n");
    pr_info("ICMP Layer shutdown complete.\n");
}
