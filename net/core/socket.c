/*
 * NEXUS OS - Socket Layer Implementation
 * net/core/socket.c
 */

#include "net.h"

/*===========================================================================*/
/*                         SOCKET CONFIGURATION                              */
/*===========================================================================*/

#define SOCKET_HASH_BITS        10
#define SOCKET_HASH_SIZE        (1 << SOCKET_HASH_BITS)
#define SOCKET_MAX_LISTEN       128
#define SOCKET_BACKLOG_LIMIT    4096

/*===========================================================================*/
/*                         SOCKET GLOBAL DATA                                */
/*===========================================================================*/

/* Socket Hash Table */
static struct socket *socket_hash[SOCKET_HASH_SIZE];
static spinlock_t socket_hash_lock = { .lock = SPINLOCK_UNLOCKED };

/* Socket List */
static struct socket *socket_list = NULL;
static spinlock_t socket_list_lock = { .lock = SPINLOCK_UNLOCKED };

/* Socket Statistics */
static struct {
    spinlock_t lock;
    atomic_t total_created;
    atomic_t total_destroyed;
    atomic_t current_sockets;
    atomic_t bind_calls;
    atomic_t connect_calls;
    atomic_t listen_calls;
    atomic_t accept_calls;
    atomic_t send_calls;
    atomic_t recv_calls;
} socket_stats = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_created = ATOMIC_INIT(0),
    .total_destroyed = ATOMIC_INIT(0),
    .current_sockets = ATOMIC_INIT(0),
    .bind_calls = ATOMIC_INIT(0),
    .connect_calls = ATOMIC_INIT(0),
    .listen_calls = ATOMIC_INIT(0),
    .accept_calls = ATOMIC_INIT(0),
    .send_calls = ATOMIC_INIT(0),
    .recv_calls = ATOMIC_INIT(0),
};

/* Socket File Descriptor Allocator */
static struct {
    spinlock_t lock;
    u8 fd_bitmap[MAX_SOCKETS / 8];
    atomic_t next_fd;
} fd_allocator = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .fd_bitmap = {0},
    .next_fd = ATOMIC_INIT(3),  /* Start after stdin, stdout, stderr */
};

/*===========================================================================*/
/*                         SOCKET HASH FUNCTIONS                             */
/*===========================================================================*/

/**
 * socket_hash_fn - Hash function for sockets
 * @family: Address family
 * @type: Socket type
 * @protocol: Protocol number
 *
 * Returns: Hash value
 */
static inline u32 socket_hash_fn(u16 family, u16 type, u16 protocol)
{
    u32 hash = family;
    hash = hash * 31 + type;
    hash = hash * 31 + protocol;
    return hash & (SOCKET_HASH_SIZE - 1);
}

/**
 * socket_hash_add - Add socket to hash table
 * @sock: Socket to add
 */
static void socket_hash_add(struct socket *sock)
{
    u32 hash;

    hash = socket_hash_fn(sock->family, sock->type, sock->protocol);

    spin_lock(&socket_hash_lock);

    sock->next = socket_hash[hash];
    sock->prev = NULL;
    if (socket_hash[hash]) {
        socket_hash[hash]->prev = sock;
    }
    socket_hash[hash] = sock;

    spin_unlock(&socket_hash_lock);
}

/**
 * socket_hash_del - Remove socket from hash table
 * @sock: Socket to remove
 */
static void socket_hash_del(struct socket *sock)
{
    u32 hash;

    hash = socket_hash_fn(sock->family, sock->type, sock->protocol);

    spin_lock(&socket_hash_lock);

    if (sock->prev) {
        sock->prev->next = sock->next;
    } else {
        socket_hash[hash] = sock->next;
    }

    if (sock->next) {
        sock->next->prev = sock->prev;
    }

    sock->next = NULL;
    sock->prev = NULL;

    spin_unlock(&socket_hash_lock);
}

/**
 * socket_hash_find - Find socket in hash table
 * @family: Address family
 * @type: Socket type
 * @protocol: Protocol number
 *
 * Returns: Pointer to socket, or NULL if not found
 */
static struct socket *socket_hash_find(u16 family, u16 type, u16 protocol)
{
    struct socket *sock;
    u32 hash;

    hash = socket_hash_fn(family, type, protocol);

    spin_lock(&socket_hash_lock);

    sock = socket_hash[hash];
    while (sock) {
        if (sock->family == family && sock->type == type &&
            sock->protocol == protocol) {
            spin_unlock(&socket_hash_lock);
            return sock;
        }
        sock = sock->next;
    }

    spin_unlock(&socket_hash_lock);
    return NULL;
}

/*===========================================================================*/
/*                         FILE DESCRIPTOR ALLOCATION                        */
/*===========================================================================*/

/**
 * fd_alloc - Allocate a file descriptor
 *
 * Returns: File descriptor number, or -1 on error
 */
static int fd_alloc(void)
{
    int fd;
    int byte, bit;

    spin_lock(&fd_allocator.lock);

    /* Try to find a free FD in bitmap */
    for (byte = 0; byte < MAX_SOCKETS / 8; byte++) {
        if (fd_allocator.fd_bitmap[byte] != 0xFF) {
            for (bit = 0; bit < 8; bit++) {
                if (!(fd_allocator.fd_bitmap[byte] & (1 << bit))) {
                    fd = byte * 8 + bit;
                    fd_allocator.fd_bitmap[byte] |= (1 << bit);
                    spin_unlock(&fd_allocator.lock);
                    return fd;
                }
            }
        }
    }

    spin_unlock(&fd_allocator.lock);
    return -1;
}

/**
 * fd_free - Free a file descriptor
 * @fd: File descriptor to free
 */
static void fd_free(int fd)
{
    int byte, bit;

    if (fd < 0 || fd >= MAX_SOCKETS) {
        return;
    }

    spin_lock(&fd_allocator.lock);

    byte = fd / 8;
    bit = fd % 8;
    fd_allocator.fd_bitmap[byte] &= ~(1 << bit);

    spin_unlock(&fd_allocator.lock);
}

/*===========================================================================*/
/*                         SOCKET INITIALIZATION                             */
/*===========================================================================*/

/**
 * socket_init_internal - Initialize socket structure
 * @sock: Socket to initialize
 */
static void socket_init_internal(struct socket *sock)
{
    memset(sock, 0, sizeof(struct socket));

    sock->state = SS_FREE;
    sock->fd = -1;

    /* Initialize receive queue */
    sock->receive_queue.next = NULL;
    sock->receive_queue.prev = NULL;
    sock->receive_queue.last = NULL;
    spin_lock_init(&sock->receive_queue.lock);
    atomic_set(&sock->receive_queue.qlen, 0);
    sock->receive_queue.flags = 0;

    /* Initialize write queue */
    sock->write_queue.next = NULL;
    sock->write_queue.prev = NULL;
    sock->write_queue.last = NULL;
    spin_lock_init(&sock->write_queue.lock);
    atomic_set(&sock->write_queue.qlen, 0);
    sock->write_queue.flags = 0;

    /* Initialize error queue */
    sock->error_queue.next = NULL;
    sock->error_queue.prev = NULL;
    sock->error_queue.last = NULL;
    spin_lock_init(&sock->error_queue.lock);
    atomic_set(&sock->error_queue.qlen, 0);
    sock->error_queue.flags = 0;

    /* Initialize socket lock */
    spin_lock_init_named(&sock->lock, "socket");

    /* Initialize wait queues */
    init_waitqueue_head_named(&sock->wait, "socket_wait");
    init_waitqueue_head_named(&sock->sk_wait, "socket_sk_wait");

    /* Set default buffer sizes */
    sock->sk_sndbuf = SOCK_DEFAULT_SNDBUF;
    sock->sk_rcvbuf = SOCK_DEFAULT_RCVBUF;
    sock->sk_rcvlowat = 1;
    sock->sk_sndlowat = 1;
    sock->sk_rcvtimeo = 0;  /* Infinite */
    sock->sk_sndtimeo = 0;  /* Infinite */

    /* Initialize atomic counters */
    atomic_set(&sock->sk_wmem_alloc, 0);
    atomic_set(&sock->sk_rmem_alloc, 0);
    atomic_set(&sock->sk_err, 0);
    atomic_set(&sock->sk_err_soft, 0);
    atomic_set(&sock->sk_ack_backlog, 0);

    /* Initialize statistics */
    sock->packets_in = 0;
    sock->packets_out = 0;
    sock->bytes_in = 0;
    sock->bytes_out = 0;
    sock->errors_in = 0;
    sock->errors_out = 0;
}

/*===========================================================================*/
/*                         SOCKET CREATION                                   */
/*===========================================================================*/

/**
 * sock_create - Create a new socket
 * @family: Address family (AF_INET, AF_INET6, etc.)
 * @type: Socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
 * @protocol: Protocol number (IPPROTO_TCP, IPPROTO_UDP, etc.)
 * @res: Pointer to store created socket
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_create(int family, int type, int protocol, struct socket **res)
{
    struct socket *sock;
    int fd;
    int ret;

    if (!res) {
        return -EINVAL;
    }

    /* Validate parameters */
    if (family < 0 || family > AF_NETLINK) {
        return -EINVAL;
    }

    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_RAW &&
        type != SOCK_SEQPACKET) {
        return -EINVAL;
    }

    /* Allocate socket structure */
    sock = kzalloc(sizeof(struct socket));
    if (!sock) {
        pr_err("Socket: Failed to allocate socket structure\n");
        return -ENOMEM;
    }

    /* Allocate file descriptor */
    fd = fd_alloc();
    if (fd < 0) {
        pr_err("Socket: No free file descriptors\n");
        kfree(sock);
        return -EMFILE;
    }

    /* Initialize socket */
    socket_init_internal(sock);

    sock->family = family;
    sock->type = type;
    sock->protocol = protocol ? protocol : 0;
    sock->fd = fd;
    sock->state = SS_UNCONNECTED;

    /* Add to hash table and list */
    socket_hash_add(sock);

    spin_lock(&socket_list_lock);
    sock->next = socket_list;
    if (socket_list) {
        socket_list->prev = sock;
    }
    socket_list = sock;
    spin_unlock(&socket_list_lock);

    /* Update statistics */
    atomic_inc(&socket_stats.total_created);
    atomic_inc(&socket_stats.current_sockets);

    *res = sock;

    pr_debug("Socket: Created socket %p (fd=%d, family=%d, type=%d, protocol=%d)\n",
             sock, fd, family, type, protocol);

    return 0;
}

/**
 * sock_create_kern - Create a kernel socket
 * @family: Address family
 * @type: Socket type
 * @protocol: Protocol number
 * @res: Pointer to store created socket
 *
 * Kernel sockets are not associated with file descriptors.
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_create_kern(int family, int type, int protocol, struct socket **res)
{
    struct socket *sock;
    int ret;

    if (!res) {
        return -EINVAL;
    }

    /* Allocate socket structure */
    sock = kzalloc(sizeof(struct socket));
    if (!sock) {
        pr_err("Socket: Failed to allocate kernel socket structure\n");
        return -ENOMEM;
    }

    /* Initialize socket */
    socket_init_internal(sock);

    sock->family = family;
    sock->type = type;
    sock->protocol = protocol ? protocol : 0;
    sock->fd = -1;  /* No FD for kernel sockets */
    sock->state = SS_UNCONNECTED;

    /* Add to hash table */
    socket_hash_add(sock);

    /* Update statistics */
    atomic_inc(&socket_stats.total_created);
    atomic_inc(&socket_stats.current_sockets);

    *res = sock;

    pr_debug("Socket: Created kernel socket %p (family=%d, type=%d, protocol=%d)\n",
             sock, family, type, protocol);

    return 0;
}

/*===========================================================================*/
/*                         SOCKET RELEASE                                    */
/*===========================================================================*/

/**
 * sock_release - Release a socket
 * @sock: Socket to release
 *
 * Frees all resources associated with the socket.
 */
void sock_release(struct socket *sock)
{
    struct sk_buff *skb;

    if (!sock) {
        return;
    }

    pr_debug("Socket: Releasing socket %p (fd=%d)\n", sock, sock->fd);

    /* Remove from hash table */
    socket_hash_del(sock);

    /* Remove from socket list */
    spin_lock(&socket_list_lock);
    if (sock->prev) {
        sock->prev->next = sock->next;
    } else {
        socket_list = sock->next;
    }
    if (sock->next) {
        sock->next->prev = sock->prev;
    }
    spin_unlock(&socket_list_lock);

    /* Free all queued packets */
    spin_lock(&sock->receive_queue.lock);
    while ((skb = skb_dequeue(&sock->receive_queue)) != NULL) {
        free_skb(skb);
    }
    spin_unlock(&sock->receive_queue.lock);

    spin_lock(&sock->write_queue.lock);
    while ((skb = skb_dequeue(&sock->write_queue)) != NULL) {
        free_skb(skb);
    }
    spin_unlock(&sock->write_queue.lock);

    spin_lock(&sock->error_queue.lock);
    while ((skb = skb_dequeue(&sock->error_queue)) != NULL) {
        free_skb(skb);
    }
    spin_unlock(&sock->error_queue.lock);

    /* Free file descriptor */
    if (sock->fd >= 0) {
        fd_free(sock->fd);
    }

    /* Update statistics */
    atomic_dec(&socket_stats.current_sockets);
    atomic_inc(&socket_stats.total_destroyed);

    /* Free socket structure */
    kfree(sock);
}

/*===========================================================================*/
/*                         SOCKET BIND                                       */
/*===========================================================================*/

/**
 * sock_bind - Bind a socket to a local address
 * @sock: Socket to bind
 * @addr: Local address to bind to
 * @addr_len: Length of address structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
    struct sockaddr_in *addr_in;

    if (!sock || !addr) {
        return -EINVAL;
    }

    if (addr_len < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    if (sock->family != AF_INET) {
        return -EAFNOSUPPORT;
    }

    spin_lock(&sock->lock);

    if (sock->state != SS_UNCONNECTED) {
        spin_unlock(&sock->lock);
        return -EINVAL;
    }

    addr_in = (struct sockaddr_in *)addr;

    /* Copy address to socket */
    sock->local_addr.sin_family = addr_in->sin_family;
    sock->local_addr.sin_port = addr_in->sin_port;
    sock->local_addr.sin_addr = addr_in->sin_addr;

    sock->state = SS_UNCONNECTED;

    spin_unlock(&sock->lock);

    /* Update statistics */
    atomic_inc(&socket_stats.bind_calls);

    pr_debug("Socket: Bound socket %p to %x:%d\n",
             sock, ntohl(addr_in->sin_addr), ntohs(addr_in->sin_port));

    return 0;
}

/*===========================================================================*/
/*                         SOCKET CONNECT                                    */
/*===========================================================================*/

/**
 * sock_connect - Connect a socket to a remote address
 * @sock: Socket to connect
 * @addr: Remote address to connect to
 * @addr_len: Length of address structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_connect(struct socket *sock, struct sockaddr *addr, int addr_len)
{
    struct sockaddr_in *addr_in;
    int ret = 0;

    if (!sock || !addr) {
        return -EINVAL;
    }

    if (addr_len < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    if (sock->family != AF_INET) {
        return -EAFNOSUPPORT;
    }

    spin_lock(&sock->lock);

    if (sock->type != SOCK_STREAM) {
        /* For UDP, just store the remote address */
        addr_in = (struct sockaddr_in *)addr;
        sock->remote_addr.sin_family = addr_in->sin_family;
        sock->remote_addr.sin_port = addr_in->sin_port;
        sock->remote_addr.sin_addr = addr_in->sin_addr;
        spin_unlock(&sock->lock);
        return 0;
    }

    /* TCP connection logic would go here */
    /* For now, just store the address */
    addr_in = (struct sockaddr_in *)addr;
    sock->remote_addr.sin_family = addr_in->sin_family;
    sock->remote_addr.sin_port = addr_in->sin_port;
    sock->remote_addr.sin_addr = addr_in->sin_addr;

    sock->state = SS_CONNECTED;

    spin_unlock(&sock->lock);

    /* Update statistics */
    atomic_inc(&socket_stats.connect_calls);

    pr_debug("Socket: Connected socket %p to %x:%d\n",
             sock, ntohl(addr_in->sin_addr), ntohs(addr_in->sin_port));

    return ret;
}

/*===========================================================================*/
/*                         SOCKET LISTEN                                     */
/*===========================================================================*/

/**
 * sock_listen - Mark a socket as listening for connections
 * @sock: Socket to mark as listening
 * @backlog: Maximum pending connection queue length
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_listen(struct socket *sock, int backlog)
{
    if (!sock) {
        return -EINVAL;
    }

    if (sock->type != SOCK_STREAM) {
        return -EOPNOTSUPP;
    }

    spin_lock(&sock->lock);

    if (sock->state != SS_UNCONNECTED) {
        spin_unlock(&sock->lock);
        return -EINVAL;
    }

    if (backlog <= 0) {
        backlog = 1;
    }
    if (backlog > SOCKET_MAX_LISTEN) {
        backlog = SOCKET_MAX_LISTEN;
    }

    sock->sk_ack_backlog = backlog;
    sock->state = SS_UNCONNECTED;  /* Still unconnected, but listening */

    spin_unlock(&sock->lock);

    /* Update statistics */
    atomic_inc(&socket_stats.listen_calls);

    pr_debug("Socket: Socket %p listening with backlog %d\n", sock, backlog);

    return 0;
}

/*===========================================================================*/
/*                         SOCKET ACCEPT                                     */
/*===========================================================================*/

/**
 * sock_accept - Accept a connection on a socket
 * @sock: Listening socket
 * @newsock: Pointer to store accepted socket
 * @flags: Accept flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_accept(struct socket *sock, struct socket *newsock, int flags)
{
    if (!sock || !newsock) {
        return -EINVAL;
    }

    if (sock->type != SOCK_STREAM) {
        return -EOPNOTSUPP;
    }

    spin_lock(&sock->lock);

    if (sock->state != SS_UNCONNECTED || sock->sk_ack_backlog == 0) {
        spin_unlock(&sock->lock);
        return -EINVAL;
    }

    /* In a full implementation, we would wait for incoming connections */
    /* and create a new socket for each accepted connection */

    spin_unlock(&sock->lock);

    /* Update statistics */
    atomic_inc(&socket_stats.accept_calls);

    return -EAGAIN;  /* No connections pending */
}

/*===========================================================================*/
/*                         SOCKET SEND/RECV                                  */
/*===========================================================================*/

/**
 * sock_sendmsg - Send a message on a socket
 * @sock: Socket to send on
 * @msg: Message data
 * @len: Message length
 * @flags: Send flags
 *
 * Returns: Number of bytes sent, or negative error code on failure
 */
int sock_sendmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct sk_buff *skb;
    int ret;

    if (!sock || !msg) {
        return -EINVAL;
    }

    if (len == 0) {
        return 0;
    }

    spin_lock(&sock->lock);

    /* Check socket state */
    if (sock->state != SS_CONNECTED && sock->type == SOCK_STREAM) {
        spin_unlock(&sock->lock);
        return -ENOTCONN;
    }

    /* Check if socket is writeable */
    if (!sock_writeable(sock)) {
        spin_unlock(&sock->lock);
        return -EAGAIN;
    }

    spin_unlock(&sock->lock);

    /* Allocate socket buffer */
    skb = alloc_skb(len);
    if (!skb) {
        return -ENOMEM;
    }

    /* Copy data to buffer */
    memcpy(skb_put(skb, len), msg, len);

    /* Update socket statistics */
    sock->packets_out++;
    sock->bytes_out += len;

    /* Update global statistics */
    NET_STAT_ADD(packets_out, 1);
    NET_STAT_ADD(bytes_out, len);

    /* Update statistics */
    atomic_inc(&socket_stats.send_calls);

    pr_debug("Socket: Sent %zu bytes on socket %p\n", len, sock);

    /* In a full implementation, we would pass the skb to the protocol layer */
    free_skb(skb);

    return len;
}

/**
 * sock_recvmsg - Receive a message from a socket
 * @sock: Socket to receive from
 * @msg: Buffer to store received data
 * @len: Buffer length
 * @flags: Receive flags
 *
 * Returns: Number of bytes received, or negative error code on failure
 */
int sock_recvmsg(struct socket *sock, void *msg, size_t len, int flags)
{
    struct sk_buff *skb;
    size_t copy_len;
    int ret;

    if (!sock || !msg) {
        return -EINVAL;
    }

    if (len == 0) {
        return 0;
    }

    spin_lock(&sock->lock);

    /* Check if socket is readable */
    if (!sock_readable(sock)) {
        spin_unlock(&sock->lock);

        /* Block if not non-blocking */
        if (!(flags & MSG_DONTWAIT)) {
            /* In a full implementation, we would wait here */
            return -EAGAIN;
        }
        return -EAGAIN;
    }

    spin_unlock(&sock->lock);

    /* Dequeue packet from receive queue */
    spin_lock(&sock->receive_queue.lock);
    skb = skb_dequeue(&sock->receive_queue);
    spin_unlock(&sock->receive_queue.lock);

    if (!skb) {
        return -EAGAIN;
    }

    /* Copy data to user buffer */
    copy_len = MIN(len, skb->len);
    memcpy(msg, skb->data, copy_len);

    /* Update socket statistics */
    sock->packets_in++;
    sock->bytes_in += copy_len;

    /* Update global statistics */
    NET_STAT_ADD(packets_in, 1);
    NET_STAT_ADD(bytes_in, copy_len);

    /* Update statistics */
    atomic_inc(&socket_stats.recv_calls);

    pr_debug("Socket: Received %zu bytes on socket %p\n", copy_len, sock);

    /* Free the skb */
    free_skb(skb);

    return copy_len;
}

/*===========================================================================*/
/*                         SOCKET SHUTDOWN                                   */
/*===========================================================================*/

/**
 * sock_shutdown - Shutdown part of a socket connection
 * @sock: Socket to shutdown
 * @how: Which part to shutdown (SHUT_RD, SHUT_WR, SHUT_RDWR)
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_shutdown(struct socket *sock, int how)
{
    if (!sock) {
        return -EINVAL;
    }

    spin_lock(&sock->lock);

    if (sock->state != SS_CONNECTED) {
        spin_unlock(&sock->lock);
        return -ENOTCONN;
    }

    /* In a full implementation, we would signal the protocol layer */
    /* to shutdown the appropriate direction of the connection */

    switch (how) {
        case SHUT_RD:
            /* Shutdown receive side */
            break;
        case SHUT_WR:
            /* Shutdown transmit side */
            break;
        case SHUT_RDWR:
            /* Shutdown both sides */
            sock->state = SS_DISCONNECTING;
            break;
        default:
            spin_unlock(&sock->lock);
            return -EINVAL;
    }

    spin_unlock(&sock->lock);

    pr_debug("Socket: Shutdown socket %p (how=%d)\n", sock, how);

    return 0;
}

/*===========================================================================*/
/*                         SOCKET OPTIONS                                    */
/*===========================================================================*/

/**
 * sock_setsockopt - Set a socket option
 * @sock: Socket
 * @level: Option level (SOL_SOCKET, SOL_TCP, etc.)
 * @optname: Option name
 * @optval: Option value
 * @optlen: Length of option value
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_setsockopt(struct socket *sock, int level, int optname, void *optval, int optlen)
{
    u32 val;

    if (!sock || !optval) {
        return -EINVAL;
    }

    if (optlen < sizeof(u32)) {
        return -EINVAL;
    }

    val = *(u32 *)optval;

    spin_lock(&sock->lock);

    switch (level) {
        case SOL_SOCKET:
            switch (optname) {
                case SO_SNDBUF:
                    if (val < SOCK_MIN_SNDBUF) {
                        val = SOCK_MIN_SNDBUF;
                    }
                    if (val > SOCK_MAX_SNDBUF) {
                        val = SOCK_MAX_SNDBUF;
                    }
                    sock->sk_sndbuf = val;
                    break;

                case SO_RCVBUF:
                    if (val < SOCK_MIN_RCVBUF) {
                        val = SOCK_MIN_RCVBUF;
                    }
                    if (val > SOCK_MAX_RCVBUF) {
                        val = SOCK_MAX_RCVBUF;
                    }
                    sock->sk_rcvbuf = val;
                    break;

                case SO_RCVTIMEO:
                    sock->sk_rcvtimeo = val;
                    break;

                case SO_SNDTIMEO:
                    sock->sk_sndtimeo = val;
                    break;

                case SO_KEEPALIVE:
                    /* Enable/disable keepalive */
                    break;

                case SO_REUSEADDR:
                    /* Enable/disable address reuse */
                    break;

                case SO_BROADCAST:
                    /* Enable/disable broadcast */
                    break;

                default:
                    spin_unlock(&sock->lock);
                    return -ENOPROTOOPT;
            }
            break;

        case SOL_IP:
            /* IP-level options */
            break;

        case SOL_TCP:
            /* TCP-level options */
            break;

        default:
            spin_unlock(&sock->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&sock->lock);

    pr_debug("Socket: Set option %d on socket %p\n", optname, sock);

    return 0;
}

/**
 * sock_getsockopt - Get a socket option
 * @sock: Socket
 * @level: Option level
 * @optname: Option name
 * @optval: Pointer to store option value
 * @optlen: Length of option value buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_getsockopt(struct socket *sock, int level, int optname, void *optval, int *optlen)
{
    u32 val;

    if (!sock || !optval || !optlen) {
        return -EINVAL;
    }

    if (*optlen < sizeof(u32)) {
        return -EINVAL;
    }

    spin_lock(&sock->lock);

    switch (level) {
        case SOL_SOCKET:
            switch (optname) {
                case SO_SNDBUF:
                    val = sock->sk_sndbuf;
                    break;

                case SO_RCVBUF:
                    val = sock->sk_rcvbuf;
                    break;

                case SO_RCVTIMEO:
                    val = sock->sk_rcvtimeo;
                    break;

                case SO_SNDTIMEO:
                    val = sock->sk_sndtimeo;
                    break;

                case SO_TYPE:
                    val = sock->type;
                    break;

                case SO_ERROR:
                    val = atomic_read(&sock->sk_err);
                    atomic_set(&sock->sk_err, 0);
                    break;

                default:
                    spin_unlock(&sock->lock);
                    return -ENOPROTOOPT;
            }
            break;

        default:
            spin_unlock(&sock->lock);
            return -ENOPROTOOPT;
    }

    spin_unlock(&sock->lock);

    *(u32 *)optval = val;
    *optlen = sizeof(u32);

    return 0;
}

/*===========================================================================*/
/*                         SOCKET QUEUE OPERATIONS                           */
/*===========================================================================*/

/**
 * sock_queue_rcv_skb - Queue a socket buffer for reception
 * @sock: Socket
 * @skb: Socket buffer to queue
 *
 * Returns: 0 on success, negative error code on failure
 */
int sock_queue_rcv_skb(struct socket *sock, struct sk_buff *skb)
{
    u32 qlen;

    if (!sock || !skb) {
        return -EINVAL;
    }

    spin_lock(&sock->receive_queue.lock);

    /* Check receive buffer limit */
    qlen = skb_queue_len(&sock->receive_queue);
    if (qlen * skb->len >= sock->sk_rcvbuf) {
        spin_unlock(&sock->receive_queue.lock);
        return -ENOMEM;
    }

    /* Queue the packet */
    skb_queue_tail(&sock->receive_queue, skb);

    spin_unlock(&sock->receive_queue.lock);

    /* Wake up any waiters */
    wake_up(&sock->wait);

    return 0;
}

/**
 * sock_dequeue_rcv_skb - Dequeue a socket buffer from receive queue
 * @sock: Socket
 *
 * Returns: Socket buffer, or NULL if queue is empty
 */
struct sk_buff *sock_dequeue_rcv_skb(struct socket *sock)
{
    struct sk_buff *skb;

    if (!sock) {
        return NULL;
    }

    spin_lock(&sock->receive_queue.lock);
    skb = skb_dequeue(&sock->receive_queue);
    spin_unlock(&sock->receive_queue.lock);

    return skb;
}

/**
 * sock_writeable - Check if socket is writeable
 * @sock: Socket to check
 *
 * Returns: true if writeable, false otherwise
 */
bool sock_writeable(const struct socket *sock)
{
    u32 wmem;

    if (!sock) {
        return false;
    }

    wmem = atomic_read(&sock->sk_wmem_alloc);
    return wmem < sock->sk_sndbuf;
}

/**
 * sock_readable - Check if socket is readable
 * @sock: Socket to check
 *
 * Returns: true if readable, false otherwise
 */
bool sock_readable(const struct socket *sock)
{
    if (!sock) {
        return false;
    }

    return !skb_queue_empty(&sock->receive_queue);
}

/*===========================================================================*/
/*                         SOCKET BUFFER MANAGEMENT                          */
/*===========================================================================*/

/**
 * sock_set_sndbuf - Set socket send buffer size
 * @sock: Socket
 * @size: New buffer size
 */
void sock_set_sndbuf(struct socket *sock, u32 size)
{
    if (!sock) {
        return;
    }

    if (size < SOCK_MIN_SNDBUF) {
        size = SOCK_MIN_SNDBUF;
    }
    if (size > SOCK_MAX_SNDBUF) {
        size = SOCK_MAX_SNDBUF;
    }

    spin_lock(&sock->lock);
    sock->sk_sndbuf = size;
    spin_unlock(&sock->lock);
}

/**
 * sock_set_rcvbuf - Set socket receive buffer size
 * @sock: Socket
 * @size: New buffer size
 */
void sock_set_rcvbuf(struct socket *sock, u32 size)
{
    if (!sock) {
        return;
    }

    if (size < SOCK_MIN_RCVBUF) {
        size = SOCK_MIN_RCVBUF;
    }
    if (size > SOCK_MAX_RCVBUF) {
        size = SOCK_MAX_RCVBUF;
    }

    spin_lock(&sock->lock);
    sock->sk_rcvbuf = size;
    spin_unlock(&sock->lock);
}

/**
 * sock_set_timeout - Set socket timeouts
 * @sock: Socket
 * @rcvtimeo: Receive timeout in milliseconds
 * @sndtimeo: Send timeout in milliseconds
 */
void sock_set_timeout(struct socket *sock, u32 rcvtimeo, u32 sndtimeo)
{
    if (!sock) {
        return;
    }

    spin_lock(&sock->lock);
    sock->sk_rcvtimeo = rcvtimeo;
    sock->sk_sndtimeo = sndtimeo;
    spin_unlock(&sock->lock);
}

/*===========================================================================*/
/*                         SOCKET UTILITIES                                  */
/*===========================================================================*/

/**
 * sock_error - Get and clear socket error
 * @sock: Socket
 *
 * Returns: Socket error code, or 0 if no error
 */
int sock_error(struct socket *sock)
{
    int err;

    if (!sock) {
        return 0;
    }

    err = atomic_read(&sock->sk_err);
    atomic_set(&sock->sk_err, 0);

    return err;
}

/**
 * sock_clear_error - Clear socket error
 * @sock: Socket
 */
void sock_clear_error(struct socket *sock)
{
    if (!sock) {
        return;
    }

    atomic_set(&sock->sk_err, 0);
}

/**
 * sock_wspace - Get available write space
 * @sock: Socket
 *
 * Returns: Available write space in bytes
 */
u32 sock_wspace(struct socket *sock)
{
    u32 wmem;

    if (!sock) {
        return 0;
    }

    wmem = atomic_read(&sock->sk_wmem_alloc);
    if (wmem >= sock->sk_sndbuf) {
        return 0;
    }

    return sock->sk_sndbuf - wmem;
}

/**
 * sock_rspace - Get available read space
 * @sock: Socket
 *
 * Returns: Available read space in bytes
 */
u32 sock_rspace(struct socket *sock)
{
    u32 rmem;

    if (!sock) {
        return 0;
    }

    rmem = atomic_read(&sock->sk_rmem_alloc);
    if (rmem >= sock->sk_rcvbuf) {
        return 0;
    }

    return sock->sk_rcvbuf - rmem;
}

/*===========================================================================*/
/*                         SOCKET POLL                                       */
/*===========================================================================*/

/**
 * sock_poll - Poll a socket for events
 * @sock: Socket to poll
 * @wait: Wait queue to use
 *
 * Returns: Poll events mask
 */
int sock_poll(struct socket *sock, wait_queue_head_t *wait)
{
    int mask = 0;

    if (!sock) {
        return 0;
    }

    spin_lock(&sock->lock);

    /* Check for readable */
    if (sock_readable(sock)) {
        mask |= 0x0001;  /* POLLIN */
    }

    /* Check for writeable */
    if (sock_writeable(sock)) {
        mask |= 0x0004;  /* POLLOUT */
    }

    /* Check for errors */
    if (atomic_read(&sock->sk_err)) {
        mask |= 0x0008;  /* POLLERR */
    }

    spin_unlock(&sock->lock);

    return mask;
}

/*===========================================================================*/
/*                         SOCKET STATISTICS                                 */
/*===========================================================================*/

/**
 * socket_stats_print - Print socket statistics
 */
void socket_stats_print(void)
{
    spin_lock(&socket_stats.lock);

    printk("\n=== Socket Statistics ===\n");
    printk("Total Created: %d\n", atomic_read(&socket_stats.total_created));
    printk("Total Destroyed: %d\n", atomic_read(&socket_stats.total_destroyed));
    printk("Current Sockets: %d\n", atomic_read(&socket_stats.current_sockets));
    printk("Bind Calls: %d\n", atomic_read(&socket_stats.bind_calls));
    printk("Connect Calls: %d\n", atomic_read(&socket_stats.connect_calls));
    printk("Listen Calls: %d\n", atomic_read(&socket_stats.listen_calls));
    printk("Accept Calls: %d\n", atomic_read(&socket_stats.accept_calls));
    printk("Send Calls: %d\n", atomic_read(&socket_stats.send_calls));
    printk("Recv Calls: %d\n", atomic_read(&socket_stats.recv_calls));

    spin_unlock(&socket_stats.lock);
}

/**
 * socket_stats_reset - Reset socket statistics
 */
void socket_stats_reset(void)
{
    spin_lock(&socket_stats.lock);

    atomic_set(&socket_stats.total_created, 0);
    atomic_set(&socket_stats.total_destroyed, 0);
    atomic_set(&socket_stats.current_sockets, 0);
    atomic_set(&socket_stats.bind_calls, 0);
    atomic_set(&socket_stats.connect_calls, 0);
    atomic_set(&socket_stats.listen_calls, 0);
    atomic_set(&socket_stats.accept_calls, 0);
    atomic_set(&socket_stats.send_calls, 0);
    atomic_set(&socket_stats.recv_calls, 0);

    spin_unlock(&socket_stats.lock);
}

/*===========================================================================*/
/*                         SOCKET DEBUGGING                                  */
/*===========================================================================*/

/**
 * net_debug_socket - Debug socket information
 * @sock: Socket to debug
 */
void net_debug_socket(struct socket *sock)
{
    if (!sock) {
        printk("Socket: NULL\n");
        return;
    }

    printk("\n=== Socket Debug Info ===\n");
    printk("Socket: %p\n", sock);
    printk("FD: %d\n", sock->fd);
    printk("State: %u\n", sock->state);
    printk("Family: %u\n", sock->family);
    printk("Type: %u\n", sock->type);
    printk("Protocol: %u\n", sock->protocol);
    printk("Send Buffer: %u/%u\n",
           atomic_read(&sock->sk_wmem_alloc), sock->sk_sndbuf);
    printk("Recv Buffer: %u/%u\n",
           atomic_read(&sock->sk_rmem_alloc), sock->sk_rcvbuf);
    printk("Receive Queue Length: %u\n",
           skb_queue_len(&sock->receive_queue));
    printk("Write Queue Length: %u\n",
           skb_queue_len(&sock->write_queue));
    printk("Packets In: %llu\n", (unsigned long long)sock->packets_in);
    printk("Packets Out: %llu\n", (unsigned long long)sock->packets_out);
    printk("Bytes In: %llu\n", (unsigned long long)sock->bytes_in);
    printk("Bytes Out: %llu\n", (unsigned long long)sock->bytes_out);
}

/*===========================================================================*/
/*                         SOCKET INITIALIZATION                             */
/*===========================================================================*/

/**
 * socket_init - Initialize socket subsystem
 */
void socket_init(void)
{
    int i;

    pr_info("Initializing Socket Subsystem...\n");

    /* Initialize hash table */
    for (i = 0; i < SOCKET_HASH_SIZE; i++) {
        socket_hash[i] = NULL;
    }
    spin_lock_init_named(&socket_hash_lock, "socket_hash");

    /* Initialize socket list */
    socket_list = NULL;
    spin_lock_init_named(&socket_list_lock, "socket_list");

    /* Initialize statistics */
    spin_lock_init_named(&socket_stats.lock, "socket_stats");
    atomic_set(&socket_stats.total_created, 0);
    atomic_set(&socket_stats.total_destroyed, 0);
    atomic_set(&socket_stats.current_sockets, 0);

    /* Initialize FD allocator */
    spin_lock_init_named(&fd_allocator.lock, "fd_allocator");
    memset(fd_allocator.fd_bitmap, 0, sizeof(fd_allocator.fd_bitmap));
    atomic_set(&fd_allocator.next_fd, 3);

    /* Reserve standard file descriptors */
    fd_allocator.fd_bitmap[0] = 0x07;  /* Mark 0, 1, 2 as used */

    pr_info("  Socket hash table: %u buckets\n", SOCKET_HASH_SIZE);
    pr_info("  Max sockets: %u\n", MAX_SOCKETS);
    pr_info("Socket Subsystem initialized.\n");
}

/**
 * socket_exit - Shutdown socket subsystem
 */
void socket_exit(void)
{
    struct socket *sock;
    struct socket *next;

    pr_info("Shutting down Socket Subsystem...\n");

    /* Release all sockets */
    spin_lock(&socket_list_lock);
    sock = socket_list;
    while (sock) {
        next = sock->next;
        sock_release(sock);
        sock = next;
    }
    spin_unlock(&socket_list_lock);

    pr_info("Socket Subsystem shutdown complete.\n");
}
