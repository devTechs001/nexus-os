/*
 * NEXUS OS - Network Device Management
 * net/core/net_device.c
 */

#include "net.h"

/*===========================================================================*/
/*                         NET DEVICE CONFIGURATION                          */
/*===========================================================================*/

#define NETDEV_HASH_BITS        8
#define NETDEV_HASH_SIZE        (1 << NETDEV_HASH_BITS)
#define NETDEV_MAX_NAME_LEN     16
#define NETDEV_WATCHDOG_TIMEOUT 5000    /* 5 seconds */

/* Default MTU Values */
#define DEFAULT_MTU             1500
#define DEFAULT_MIN_MTU         68
#define DEFAULT_MAX_MTU         65535

/* Interface Types */
#define ARPHRD_ETHER            1       /* Ethernet */
#define ARPHRD_LOOPBACK         772     /* Loopback */
#define ARPHRD_PPP              512     /* PPP */
#define ARPHRD_TUNNEL           768     /* IPIP tunnel */
#define ARPHRD_IEEE80211        801     /* IEEE 802.11 */

/*===========================================================================*/
/*                         NET DEVICE GLOBAL DATA                            */
/*===========================================================================*/

/* Network Device List */
static struct net_device *netdev_list = NULL;
static spinlock_t netdev_list_lock = { .lock = SPINLOCK_UNLOCKED };

/* Network Device Hash Table */
static struct net_device *netdev_hash[NETDEV_HASH_SIZE];
static spinlock_t netdev_hash_lock = { .lock = SPINLOCK_UNLOCKED };

/* Interface Index Allocator */
static struct {
    spinlock_t lock;
    u32 next_ifindex;
    u8 ifindex_bitmap[8192 / 8];  /* Support up to 8192 interfaces */
} ifindex_allocator = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .next_ifindex = 1,
    .ifindex_bitmap = {0},
};

/* Network Device Statistics */
static struct {
    spinlock_t lock;
    atomic_t total_devices;
    atomic_t registered_devices;
    atomic_t up_devices;
    atomic64_t total_rx_packets;
    atomic64_t total_tx_packets;
    atomic64_t total_rx_bytes;
    atomic64_t total_tx_bytes;
    atomic64_t total_rx_errors;
    atomic64_t total_tx_errors;
} netdev_stats = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_devices = ATOMIC_INIT(0),
    .registered_devices = ATOMIC_INIT(0),
    .up_devices = ATOMIC_INIT(0),
    .total_rx_packets = ATOMIC64_INIT(0),
    .total_tx_packets = ATOMIC64_INIT(0),
    .total_rx_bytes = ATOMIC64_INIT(0),
    .total_rx_bytes = ATOMIC64_INIT(0),
    .total_tx_bytes = ATOMIC64_INIT(0),
    .total_rx_errors = ATOMIC64_INIT(0),
    .total_tx_errors = ATOMIC64_INIT(0),
};

/* Loopback Device */
static struct net_device *loopback_dev = NULL;

/*===========================================================================*/
/*                         NET DEVICE HASH FUNCTIONS                         */
/*===========================================================================*/

/**
 * netdev_hash_fn - Hash function for network devices
 * @name: Device name
 *
 * Returns: Hash value
 */
static inline u32 netdev_hash_fn(const char *name)
{
    u32 hash = 0;
    u32 i;

    for (i = 0; i < NETDEV_MAX_NAME_LEN && name[i]; i++) {
        hash = hash * 31 + name[i];
    }

    return hash & (NETDEV_HASH_SIZE - 1);
}

/**
 * netdev_hash_add - Add device to hash table
 * @dev: Device to add
 */
static void netdev_hash_add(struct net_device *dev)
{
    u32 hash;

    hash = netdev_hash_fn(dev->name);

    spin_lock(&netdev_hash_lock);

    dev->next = netdev_hash[hash];
    dev->prev = NULL;
    if (netdev_hash[hash]) {
        netdev_hash[hash]->prev = dev;
    }
    netdev_hash[hash] = dev;

    spin_unlock(&netdev_hash_lock);
}

/**
 * netdev_hash_del - Remove device from hash table
 * @dev: Device to remove
 */
static void netdev_hash_del(struct net_device *dev)
{
    u32 hash;

    hash = netdev_hash_fn(dev->name);

    spin_lock(&netdev_hash_lock);

    if (dev->prev) {
        dev->prev->next = dev->next;
    } else {
        netdev_hash[hash] = dev->next;
    }

    if (dev->next) {
        dev->next->prev = dev->prev;
    }

    dev->next = NULL;
    dev->prev = NULL;

    spin_unlock(&netdev_hash_lock);
}

/*===========================================================================*/
/*                         INTERFACE INDEX ALLOCATION                        */
/*===========================================================================*/

/**
 * ifindex_alloc - Allocate an interface index
 *
 * Returns: Interface index, or 0 on failure
 */
static u32 ifindex_alloc(void)
{
    u32 ifindex;
    u32 byte, bit;

    spin_lock(&ifindex_allocator.lock);

    /* Find free ifindex in bitmap */
    for (byte = 0; byte < sizeof(ifindex_allocator.ifindex_bitmap); byte++) {
        if (ifindex_allocator.ifindex_bitmap[byte] != 0xFF) {
            for (bit = 0; bit < 8; bit++) {
                if (!(ifindex_allocator.ifindex_bitmap[byte] & (1 << bit))) {
                    ifindex = byte * 8 + bit + 1;  /* Start from 1 */
                    ifindex_allocator.ifindex_bitmap[byte] |= (1 << bit);
                    spin_unlock(&ifindex_allocator.lock);
                    return ifindex;
                }
            }
        }
    }

    spin_unlock(&ifindex_allocator.lock);
    return 0;
}

/**
 * ifindex_free - Free an interface index
 * @ifindex: Interface index to free
 */
static void ifindex_free(u32 ifindex)
{
    u32 byte, bit;

    if (ifindex == 0 || ifindex > 8192) {
        return;
    }

    spin_lock(&ifindex_allocator.lock);

    byte = (ifindex - 1) / 8;
    bit = (ifindex - 1) % 8;
    ifindex_allocator.ifindex_bitmap[byte] &= ~(1 << bit);

    spin_unlock(&ifindex_allocator.lock);
}

/*===========================================================================*/
/*                         NET DEVICE ALLOCATION                             */
/*===========================================================================*/

/**
 * alloc_netdev - Allocate a new network device
 * @sizeof_priv: Size of private data area
 * @name: Device name pattern
 * @setup: Setup function
 *
 * Returns: Pointer to allocated net_device, or NULL on failure
 */
struct net_device *alloc_netdev(int sizeof_priv, const char *name, void (*setup)(struct net_device *))
{
    struct net_device *dev;
    u32 total_size;
    u32 name_len;

    if (!name) {
        return NULL;
    }

    /* Calculate total size */
    total_size = sizeof(struct net_device) + sizeof_priv;

    /* Allocate device structure */
    dev = kzalloc(total_size);
    if (!dev) {
        return NULL;
    }

    /* Set device name */
    name_len = strlen(name);
    if (name_len >= NETDEV_MAX_NAME_LEN) {
        name_len = NETDEV_MAX_NAME_LEN - 1;
    }
    memcpy(dev->name, name, name_len);
    dev->name[name_len] = '\0';

    /* Set default values */
    dev->ifindex = 0;
    dev->iftype = ARPHRD_ETHER;
    dev->flags = 0;
    dev->features = 0;

    dev->addr_len = 6;  /* Ethernet */
    memset(dev->dev_addr, 0, sizeof(dev->dev_addr));
    memset(dev->broadcast, 0xFF, dev->addr_len);

    dev->mtu = DEFAULT_MTU;
    dev->min_mtu = DEFAULT_MIN_MTU;
    dev->max_mtu = DEFAULT_MAX_MTU;

    dev->num_tx_queues = 1;
    dev->num_rx_queues = 1;
    dev->real_num_tx_queues = 1;
    dev->real_num_rx_queues = 1;

    /* Initialize transmit queue */
    dev->tx_queue.next = NULL;
    dev->tx_queue.prev = NULL;
    dev->tx_queue.last = NULL;
    spin_lock_init_named(&dev->tx_lock, "tx_lock");

    /* Initialize receive queue */
    dev->rx_queue.next = NULL;
    dev->rx_queue.prev = NULL;
    dev->rx_queue.last = NULL;
    spin_lock_init_named(&dev->rx_lock, "rx_lock");

    /* Initialize statistics */
    memset(&dev->stats, 0, sizeof(struct net_device_stats));

    /* Initialize wait queue */
    init_waitqueue_head_named(&dev->waitq, "netdev_wait");

    /* Initialize reference count */
    atomic_set(&dev->refcnt, 1);

    /* Set state */
    dev->state = NET_DEVICE_STATE_UNINITIALIZED;
    dev->reg_state = NET_DEVICE_STATE_UNINITIALIZED;

    /* Set watchdog timeout */
    dev->watchdog_timeo = NETDEV_WATCHDOG_TIMEOUT;
    dev->last_tx_time = 0;

    /* Call setup function if provided */
    if (setup) {
        setup(dev);
    }

    /* Update statistics */
    atomic_inc(&netdev_stats.total_devices);

    pr_debug("NetDev: Allocated device %s (%p)\n", dev->name, dev);

    return dev;
}

/**
 * free_netdev - Free a network device
 * @dev: Device to free
 */
void free_netdev(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    pr_debug("NetDev: Freeing device %s (%p)\n", dev->name, dev);

    /* Free device structure */
    kfree(dev);
}

/*===========================================================================*/
/*                         NET DEVICE REGISTRATION                           */
/*===========================================================================*/

/**
 * register_netdevice - Register a network device
 * @dev: Device to register
 *
 * Returns: 0 on success, negative error code on failure
 */
int register_netdevice(struct net_device *dev)
{
    int ret;

    if (!dev) {
        return -EINVAL;
    }

    if (dev->reg_state == NET_DEVICE_STATE_REGISTERED) {
        return -EEXIST;
    }

    spin_lock(&netdev_list_lock);

    /* Allocate interface index */
    dev->ifindex = ifindex_alloc();
    if (dev->ifindex == 0) {
        spin_unlock(&netdev_list_lock);
        pr_err("NetDev: Failed to allocate ifindex for %s\n", dev->name);
        return -ENOMEM;
    }

    /* Add to device list */
    dev->next = netdev_list;
    if (netdev_list) {
        netdev_list->prev = dev;
    }
    netdev_list = dev;

    /* Add to hash table */
    netdev_hash_add(dev);

    /* Call device init if provided */
    if (dev->ops && dev->ops->ndo_init) {
        ret = dev->ops->ndo_init(dev);
        if (ret != 0) {
            /* Rollback */
            if (netdev_list == dev) {
                netdev_list = dev->next;
            }
            if (dev->next) {
                dev->next->prev = NULL;
            }
            ifindex_free(dev->ifindex);
            spin_unlock(&netdev_list_lock);
            return ret;
        }
    }

    /* Update state */
    dev->reg_state = NET_DEVICE_STATE_REGISTERED;
    dev->state = NET_DEVICE_STATE_REGISTERED;

    /* Update statistics */
    atomic_inc(&netdev_stats.registered_devices);

    spin_unlock(&netdev_list_lock);

    pr_info("NetDev: Registered device %s (ifindex=%d)\n", dev->name, dev->ifindex);

    return 0;
}

/**
 * unregister_netdevice - Unregister a network device
 * @dev: Device to unregister
 *
 * Returns: 0 on success, negative error code on failure
 */
int unregister_netdevice(struct net_device *dev)
{
    if (!dev) {
        return -EINVAL;
    }

    if (dev->reg_state != NET_DEVICE_STATE_REGISTERED) {
        return -ENODEV;
    }

    spin_lock(&netdev_list_lock);

    /* Update state */
    dev->reg_state = NET_DEVICE_STATE_UNREGISTERING;

    /* Call device uninit if provided */
    if (dev->ops && dev->ops->ndo_uninit) {
        dev->ops->ndo_uninit(dev);
    }

    /* Remove from device list */
    if (dev->prev) {
        dev->prev->next = dev->next;
    } else {
        netdev_list = dev->next;
    }
    if (dev->next) {
        dev->next->prev = dev->prev;
    }

    /* Remove from hash table */
    netdev_hash_del(dev);

    /* Free interface index */
    ifindex_free(dev->ifindex);

    /* Update state */
    dev->reg_state = NET_DEVICE_STATE_RELEASED;

    /* Update statistics */
    atomic_dec(&netdev_stats.registered_devices);

    spin_unlock(&netdev_list_lock);

    pr_info("NetDev: Unregistered device %s (ifindex=%d)\n", dev->name, dev->ifindex);

    return 0;
}

/*===========================================================================*/
/*                         NET DEVICE LOOKUP                                 */
/*===========================================================================*/

/**
 * dev_get_by_name - Get network device by name
 * @name: Device name
 *
 * Returns: Pointer to device, or NULL if not found
 */
struct net_device *dev_get_by_name(const char *name)
{
    struct net_device *dev;
    u32 hash;

    if (!name) {
        return NULL;
    }

    hash = netdev_hash_fn(name);

    spin_lock(&netdev_hash_lock);

    dev = netdev_hash[hash];
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            dev_hold(dev);
            spin_unlock(&netdev_hash_lock);
            return dev;
        }
        dev = dev->next;
    }

    spin_unlock(&netdev_hash_lock);
    return NULL;
}

/**
 * dev_get_by_index - Get network device by interface index
 * @ifindex: Interface index
 *
 * Returns: Pointer to device, or NULL if not found
 */
struct net_device *dev_get_by_index(u32 ifindex)
{
    struct net_device *dev;

    if (ifindex == 0) {
        return NULL;
    }

    spin_lock(&netdev_list_lock);

    dev = netdev_list;
    while (dev) {
        if (dev->ifindex == ifindex) {
            dev_hold(dev);
            spin_unlock(&netdev_list_lock);
            return dev;
        }
        dev = dev->next;
    }

    spin_unlock(&netdev_list_lock);
    return NULL;
}

/**
 * dev_get_first - Get first network device
 *
 * Returns: Pointer to first device, or NULL if no devices
 */
struct net_device *dev_get_first(void)
{
    struct net_device *dev;

    spin_lock(&netdev_list_lock);
    dev = netdev_list;
    if (dev) {
        dev_hold(dev);
    }
    spin_unlock(&netdev_list_lock);

    return dev;
}

/**
 * dev_get_next - Get next network device
 * @dev: Current device
 *
 * Returns: Pointer to next device, or NULL if no more devices
 */
struct net_device *dev_get_next(struct net_device *dev)
{
    struct net_device *next;

    if (!dev) {
        return NULL;
    }

    spin_lock(&netdev_list_lock);
    next = dev->next;
    if (next) {
        dev_hold(next);
    }
    spin_unlock(&netdev_list_lock);

    return next;
}

/*===========================================================================*/
/*                         NET DEVICE OPERATIONS                             */
/*===========================================================================*/

/**
 * dev_open - Open a network device
 * @dev: Device to open
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_open(struct net_device *dev)
{
    int ret = 0;

    if (!dev) {
        return -EINVAL;
    }

    spin_lock(&dev->tx_lock);

    if (dev->flags & IFF_UP) {
        spin_unlock(&dev->tx_lock);
        return 0;  /* Already up */
    }

    /* Call device open if provided */
    if (dev->ops && dev->ops->ndo_open) {
        ret = dev->ops->ndo_open(dev);
        if (ret != 0) {
            spin_unlock(&dev->tx_lock);
            return ret;
        }
    }

    /* Update flags */
    dev->flags |= IFF_UP;
    dev->flags |= IFF_RUNNING;

    /* Update statistics */
    atomic_inc(&netdev_stats.up_devices);

    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Opened device %s\n", dev->name);

    return 0;
}

/**
 * dev_close - Close a network device
 * @dev: Device to close
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_close(struct net_device *dev)
{
    int ret = 0;

    if (!dev) {
        return -EINVAL;
    }

    spin_lock(&dev->tx_lock);

    if (!(dev->flags & IFF_UP)) {
        spin_unlock(&dev->tx_lock);
        return 0;  /* Already down */
    }

    /* Call device stop if provided */
    if (dev->ops && dev->ops->ndo_stop) {
        ret = dev->ops->ndo_stop(dev);
    }

    /* Update flags */
    dev->flags &= ~IFF_UP;
    dev->flags &= ~IFF_RUNNING;

    /* Update statistics */
    atomic_dec(&netdev_stats.up_devices);

    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Closed device %s\n", dev->name);

    return ret;
}

/**
 * dev_change_mtu - Change device MTU
 * @dev: Device
 * @new_mtu: New MTU value
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_change_mtu(struct net_device *dev, int new_mtu)
{
    int ret = 0;

    if (!dev) {
        return -EINVAL;
    }

    if (new_mtu < dev->min_mtu || new_mtu > dev->max_mtu) {
        return -EINVAL;
    }

    spin_lock(&dev->tx_lock);

    /* Call device MTU change if provided */
    if (dev->ops && dev->ops->ndo_change_mtu) {
        ret = dev->ops->ndo_change_mtu(dev, new_mtu);
        if (ret != 0) {
            spin_unlock(&dev->tx_lock);
            return ret;
        }
    }

    dev->mtu = new_mtu;

    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Changed MTU of %s to %d\n", dev->name, new_mtu);

    return 0;
}

/**
 * dev_change_addr - Change device hardware address
 * @dev: Device
 * @addr: New hardware address
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_change_addr(struct net_device *dev, u8 *addr)
{
    int ret = 0;

    if (!dev || !addr) {
        return -EINVAL;
    }

    spin_lock(&dev->tx_lock);

    /* Call device address change if provided */
    if (dev->ops && dev->ops->ndo_change_addr) {
        ret = dev->ops->ndo_change_addr(dev, addr);
        if (ret != 0) {
            spin_unlock(&dev->tx_lock);
            return ret;
        }
    }

    memcpy(dev->dev_addr, addr, dev->addr_len);

    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Changed address of %s\n", dev->name);

    return 0;
}

/*===========================================================================*/
/*                         PACKET TRANSMISSION                               */
/*===========================================================================*/

/**
 * dev_hard_start_xmit - Start packet transmission on device
 * @skb: Socket buffer to transmit
 * @dev: Device to transmit on
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    int ret;

    if (!skb || !dev) {
        return -EINVAL;
    }

    if (!(dev->flags & IFF_UP)) {
        return -ENETDOWN;
    }

    /* Call device transmit if provided */
    if (dev->ops && dev->ops->ndo_start_xmit) {
        ret = dev->ops->ndo_start_xmit(skb, dev);
    } else {
        /* Default: queue the packet */
        spin_lock(&dev->tx_lock);
        skb_queue_tail(&dev->tx_queue, skb);
        spin_unlock(&dev->tx_lock);
        ret = 0;
    }

    /* Update statistics */
    if (ret == 0) {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += skb->len;
        dev->last_tx_time = get_ticks();

        NET_STAT_ADD(total_tx_packets, 1);
        NET_STAT_ADD(total_tx_bytes, skb->len);
    } else {
        dev->stats.tx_errors++;
        NET_STAT_ADD(total_tx_errors, 1);
    }

    return ret;
}

/**
 * dev_queue_xmit - Queue packet for transmission
 * @skb: Socket buffer to transmit
 *
 * Returns: 0 on success, negative error code on failure
 */
int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev;
    int ret;

    if (!skb) {
        return -EINVAL;
    }

    dev = (struct net_device *)skb->dev;
    if (!dev) {
        /* Use default route device */
        dev = dev_get_first();
        if (!dev) {
            free_skb(skb);
            return -ENODEV;
        }
        skb->dev = dev;
    }

    /* Check if device is up */
    if (!(dev->flags & IFF_UP)) {
        free_skb(skb);
        return -ENETDOWN;
    }

    /* Transmit the packet */
    ret = dev_hard_start_xmit(skb, dev);

    dev_put(dev);

    return ret;
}

/**
 * dev_transmit_complete - Notify transmission completion
 * @dev: Device
 * @skb: Socket buffer that was transmitted
 * @status: Transmission status (0 = success)
 */
void dev_transmit_complete(struct net_device *dev, struct sk_buff *skb, int status)
{
    if (!dev || !skb) {
        return;
    }

    /* Free the transmitted buffer */
    free_skb(skb);

    /* Wake up transmit queue */
    netif_wake_queue(dev);
}

/*===========================================================================*/
/*                         PACKET RECEPTION                                  */
/*===========================================================================*/

/**
 * netif_receive_skb - Receive a socket buffer
 * @skb: Socket buffer to receive
 *
 * Returns: 0 on success, negative error code on failure
 */
int netif_receive_skb(struct sk_buff *skb)
{
    struct net_device *dev;

    if (!skb) {
        return -EINVAL;
    }

    dev = (struct net_device *)skb->dev;

    /* Update statistics */
    if (dev) {
        dev->stats.rx_packets++;
        dev->stats.rx_bytes += skb->len;

        NET_STAT_ADD(total_rx_packets, 1);
        NET_STAT_ADD(total_rx_bytes, skb->len);
    }

    /* In a full implementation, this would pass the packet up the stack */
    /* For now, just free it */
    free_skb(skb);

    return 0;
}

/**
 * netif_rx - Receive a socket buffer (interrupt context)
 * @skb: Socket buffer to receive
 *
 * Returns: 0 on success, negative error code on failure
 */
int netif_rx(struct sk_buff *skb)
{
    return netif_receive_skb(skb);
}

/**
 * netif_rx_ni - Receive a socket buffer (non-interrupt context)
 * @skb: Socket buffer to receive
 *
 * Returns: 0 on success, negative error code on failure
 */
int netif_rx_ni(struct sk_buff *skb)
{
    return netif_receive_skb(skb);
}

/*===========================================================================*/
/*                         DEVICE STATISTICS                                 */
/*===========================================================================*/

/**
 * dev_get_stats - Get device statistics
 * @dev: Device
 *
 * Returns: Pointer to statistics structure
 */
struct net_device_stats *dev_get_stats(struct net_device *dev)
{
    if (!dev) {
        return NULL;
    }

    /* Call device get_stats if provided */
    if (dev->ops && dev->ops->ndo_get_stats) {
        return dev->ops->ndo_get_stats(dev);
    }

    return &dev->stats;
}

/**
 * dev_update_stats - Update device statistics
 * @dev: Device
 */
void dev_update_stats(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    /* In a full implementation, this would query hardware counters */
}

/*===========================================================================*/
/*                         DEVICE STATE                                      */
/*===========================================================================*/

/**
 * netif_running - Check if device is running
 * @dev: Device to check
 *
 * Returns: true if running, false otherwise
 */
bool netif_running(const struct net_device *dev)
{
    if (!dev) {
        return false;
    }

    return dev->flags & IFF_UP;
}

/**
 * netif_carrier_ok - Check if device carrier is OK
 * @dev: Device to check
 *
 * Returns: true if carrier OK, false otherwise
 */
bool netif_carrier_ok(const struct net_device *dev)
{
    if (!dev) {
        return false;
    }

    return dev->flags & IFF_LOWER_UP;
}

/**
 * netif_carrier_on - Set carrier on
 * @dev: Device
 */
void netif_carrier_on(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    spin_lock(&dev->tx_lock);
    dev->flags |= IFF_LOWER_UP;
    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Carrier on for %s\n", dev->name);
}

/**
 * netif_carrier_off - Set carrier off
 * @dev: Device
 */
void netif_carrier_off(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    spin_lock(&dev->tx_lock);
    dev->flags &= ~IFF_LOWER_UP;
    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Carrier off for %s\n", dev->name);
}

/**
 * netif_start_queue - Start transmit queue
 * @dev: Device
 */
void netif_start_queue(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    dev->flags |= IFF_UP;
    wake_up(&dev->waitq);
}

/**
 * netif_stop_queue - Stop transmit queue
 * @dev: Device
 */
void netif_stop_queue(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    dev->flags &= ~IFF_UP;
}

/**
 * netif_wake_queue - Wake transmit queue
 * @dev: Device
 */
void netif_wake_queue(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    wake_up(&dev->waitq);
}

/*===========================================================================*/
/*                         DEVICE FEATURES                                   */
/*===========================================================================*/

/**
 * netif_get_features - Get device features
 * @dev: Device
 *
 * Returns: Feature flags
 */
u32 netif_get_features(struct net_device *dev)
{
    if (!dev) {
        return 0;
    }

    return dev->features;
}

/**
 * netif_set_features - Set device features
 * @dev: Device
 * @features: New feature flags
 *
 * Returns: 0 on success, negative error code on failure
 */
int netif_set_features(struct net_device *dev, u32 features)
{
    u32 old_features;
    int ret = 0;

    if (!dev) {
        return -EINVAL;
    }

    spin_lock(&dev->tx_lock);

    old_features = dev->features;

    /* Call device set_features if provided */
    if (dev->ops && dev->ops->ndo_set_features) {
        ret = dev->ops->ndo_set_features(dev, features);
        if (ret != 0) {
            spin_unlock(&dev->tx_lock);
            return ret;
        }
    }

    dev->features = features;

    spin_unlock(&dev->tx_lock);

    pr_debug("NetDev: Changed features of %s from 0x%x to 0x%x\n",
             dev->name, old_features, features);

    return 0;
}

/*===========================================================================*/
/*                         DEVICE REFERENCE COUNTING                         */
/*===========================================================================*/

/**
 * dev_hold - Get reference to device
 * @dev: Device
 *
 * Returns: Pointer to device
 */
struct net_device *dev_hold(struct net_device *dev)
{
    if (dev) {
        atomic_inc(&dev->refcnt);
    }
    return dev;
}

/**
 * dev_put - Put reference to device
 * @dev: Device
 */
void dev_put(struct net_device *dev)
{
    if (!dev) {
        return;
    }

    if (atomic_dec_and_test(&dev->refcnt)) {
        /* Device can be freed */
        pr_debug("NetDev: Device %s refcount reached zero\n", dev->name);
    }
}

/*===========================================================================*/
/*                         LOOPBACK DEVICE                                   */
/*===========================================================================*/

/**
 * loopback_init - Initialize loopback device
 *
 * Returns: 0 on success, negative error code on failure
 */
static int loopback_init(void)
{
    struct net_device *dev;

    /* Allocate loopback device */
    dev = alloc_netdev(0, "lo", NULL);
    if (!dev) {
        return -ENOMEM;
    }

    /* Set loopback properties */
    dev->iftype = ARPHRD_LOOPBACK;
    dev->flags = IFF_UP | IFF_RUNNING | IFF_LOOPBACK;
    dev->mtu = 65536;
    dev->min_mtu = 68;
    dev->max_mtu = 65535;

    /* Set loopback address */
    dev->dev_addr[0] = 0x7F;
    dev->dev_addr[1] = 0x00;
    dev->dev_addr[2] = 0x00;
    dev->dev_addr[3] = 0x01;

    /* Register device */
    register_netdevice(dev);

    loopback_dev = dev;

    pr_info("NetDev: Loopback device initialized\n");

    return 0;
}

/**
 * loopback_exit - Shutdown loopback device
 */
static void loopback_exit(void)
{
    if (loopback_dev) {
        unregister_netdevice(loopback_dev);
        free_netdev(loopback_dev);
        loopback_dev = NULL;
    }
}

/*===========================================================================*/
/*                         NET DEVICE STATISTICS                             */
/*===========================================================================*/

/**
 * netdev_stats_print - Print network device statistics
 */
void netdev_stats_print(void)
{
    spin_lock(&netdev_stats.lock);

    printk("\n=== Network Device Statistics ===\n");
    printk("Total Devices: %d\n", atomic_read(&netdev_stats.total_devices));
    printk("Registered Devices: %d\n", atomic_read(&netdev_stats.registered_devices));
    printk("Up Devices: %d\n", atomic_read(&netdev_stats.up_devices));
    printk("Total RX Packets: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_rx_packets));
    printk("Total TX Packets: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_tx_packets));
    printk("Total RX Bytes: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_rx_bytes));
    printk("Total TX Bytes: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_tx_bytes));
    printk("Total RX Errors: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_rx_errors));
    printk("Total TX Errors: %llu\n", (unsigned long long)atomic64_read(&netdev_stats.total_tx_errors));

    spin_unlock(&netdev_stats.lock);
}

/**
 * netdev_stats_reset - Reset network device statistics
 */
void netdev_stats_reset(void)
{
    spin_lock(&netdev_stats.lock);

    atomic64_set(&netdev_stats.total_rx_packets, 0);
    atomic64_set(&netdev_stats.total_tx_packets, 0);
    atomic64_set(&netdev_stats.total_rx_bytes, 0);
    atomic64_set(&netdev_stats.total_tx_bytes, 0);
    atomic64_set(&netdev_stats.total_rx_errors, 0);
    atomic64_set(&netdev_stats.total_tx_errors, 0);

    spin_unlock(&netdev_stats.lock);
}

/*===========================================================================*/
/*                         NET DEVICE DEBUGGING                              */
/*===========================================================================*/

/**
 * net_debug_device - Debug network device information
 * @dev: Device to debug
 */
void net_debug_device(struct net_device *dev)
{
    if (!dev) {
        printk("NetDev: NULL\n");
        return;
    }

    printk("\n=== NetDev Debug Info ===\n");
    printk("Device: %p\n", dev);
    printk("Name: %s\n", dev->name);
    printk("IfIndex: %u\n", dev->ifindex);
    printk("IfType: %u\n", dev->iftype);
    printk("Flags: 0x%08x\n", dev->flags);
    printk("Features: 0x%08x\n", dev->features);
    printk("MTU: %u (%u - %u)\n", dev->mtu, dev->min_mtu, dev->max_mtu);
    printk("Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
           dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
    printk("Broadcast: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->broadcast[0], dev->broadcast[1], dev->broadcast[2],
           dev->broadcast[3], dev->broadcast[4], dev->broadcast[5]);
    printk("TX Queues: %u/%u\n", dev->real_num_tx_queues, dev->num_tx_queues);
    printk("RX Queues: %u/%u\n", dev->real_num_rx_queues, dev->num_rx_queues);
    printk("TX Queue Length: %u\n", skb_queue_len(&dev->tx_queue));
    printk("RX Queue Length: %u\n", skb_queue_len(&dev->rx_queue));
    printk("State: %u\n", dev->state);
    printk("Reg State: %u\n", dev->reg_state);
    printk("RefCnt: %d\n", atomic_read(&dev->refcnt));
    printk("\nStatistics:\n");
    printk("  RX Packets: %llu\n", (unsigned long long)dev->stats.rx_packets);
    printk("  TX Packets: %llu\n", (unsigned long long)dev->stats.tx_packets);
    printk("  RX Bytes: %llu\n", (unsigned long long)dev->stats.rx_bytes);
    printk("  TX Bytes: %llu\n", (unsigned long long)dev->stats.tx_bytes);
    printk("  RX Errors: %llu\n", (unsigned long long)dev->stats.rx_errors);
    printk("  TX Errors: %llu\n", (unsigned long long)dev->stats.tx_errors);
}

/**
 * netdev_list_all - List all network devices
 */
void netdev_list_all(void)
{
    struct net_device *dev;

    printk("\n=== Network Devices ===\n");

    spin_lock(&netdev_list_lock);

    dev = netdev_list;
    while (dev) {
        printk("  %s (if%d): %s %s\n",
               dev->name,
               dev->ifindex,
               (dev->flags & IFF_UP) ? "UP" : "DOWN",
               (dev->flags & IFF_RUNNING) ? "RUNNING" : "");
        dev = dev->next;
    }

    spin_unlock(&netdev_list_lock);
}

/*===========================================================================*/
/*                         NET DEVICE INITIALIZATION                         */
/*===========================================================================*/

/**
 * net_device_init - Initialize network device subsystem
 */
void net_device_init(void)
{
    int i;
    int ret;

    pr_info("Initializing Network Device Subsystem...\n");

    /* Initialize device list */
    netdev_list = NULL;
    spin_lock_init_named(&netdev_list_lock, "netdev_list");

    /* Initialize hash table */
    for (i = 0; i < NETDEV_HASH_SIZE; i++) {
        netdev_hash[i] = NULL;
    }
    spin_lock_init_named(&netdev_hash_lock, "netdev_hash");

    /* Initialize ifindex allocator */
    spin_lock_init_named(&ifindex_allocator.lock, "ifindex_allocator");
    ifindex_allocator.next_ifindex = 1;
    memset(ifindex_allocator.ifindex_bitmap, 0, sizeof(ifindex_allocator.ifindex_bitmap));

    /* Initialize statistics */
    spin_lock_init_named(&netdev_stats.lock, "netdev_stats");
    atomic_set(&netdev_stats.total_devices, 0);
    atomic_set(&netdev_stats.registered_devices, 0);
    atomic_set(&netdev_stats.up_devices, 0);

    /* Initialize loopback device */
    ret = loopback_init();
    if (ret != 0) {
        pr_err("NetDev: Failed to initialize loopback device\n");
    }

    pr_info("  Hash table: %u buckets\n", NETDEV_HASH_SIZE);
    pr_info("  Max interfaces: 8192\n");
    pr_info("Network Device Subsystem initialized.\n");
}

/**
 * net_device_exit - Shutdown network device subsystem
 */
void net_device_exit(void)
{
    struct net_device *dev;
    struct net_device *next;

    pr_info("Shutting down Network Device Subsystem...\n");

    /* Shutdown loopback device */
    loopback_exit();

    /* Unregister all devices */
    spin_lock(&netdev_list_lock);
    dev = netdev_list;
    while (dev) {
        next = dev->next;
        unregister_netdevice(dev);
        free_netdev(dev);
        dev = next;
    }
    spin_unlock(&netdev_list_lock);

    pr_info("Network Device Subsystem shutdown complete.\n");
}
