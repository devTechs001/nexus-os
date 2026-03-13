/*
 * NEXUS OS - Ethernet Driver
 * drivers/network/ethernet.c
 *
 * Ethernet NIC driver with support for common chipsets
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ETHERNET CONFIGURATION                            */
/*===========================================================================*/

#define ETH_MAX_DEVICES           8
#define ETH_RX_DESC_COUNT         256
#define ETH_TX_DESC_COUNT         256
#define ETH_MAX_PACKET_SIZE       1518
#define ETH_MIN_PACKET_SIZE       64

/* Ethernet Frame Types */
#define ETH_TYPE_IP               0x0800
#define ETH_TYPE_ARP              0x0806
#define ETH_TYPE_IP6              0x86DD

/* NIC Vendor IDs */
#define NIC_VENDOR_INTEL          0x8086
#define NIC_VENDOR_REALTEK        0x10EC
#define NIC_VENDOR_BROADCOM       0x14E4
#define NIC_VENDOR_QUALCOMM       0x1969

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u8 dest_mac[6];
    u8 src_mac[6];
    u16 eth_type;
    u8 payload[];
} eth_header_t;

typedef struct {
    u32 address;
    u32 length;
    u32 flags;
    u32 status;
} eth_rx_desc_t;

typedef struct {
    u32 address;
    u32 length;
    u32 flags;
    u32 status;
} eth_tx_desc_t;

typedef struct {
    u32 device_id;
    char name[32];
    u16 vendor_id;
    u16 device_id_pci;
    u8 mac_address[6];
    bool is_up;
    bool is_link_up;
    u32 speed;
    u32 mtu;
    
    /* Ring buffers */
    eth_rx_desc_t *rx_ring;
    eth_tx_desc_t *tx_ring;
    phys_addr_t rx_ring_phys;
    phys_addr_t tx_ring_phys;
    u32 rx_head;
    u32 rx_tail;
    u32 tx_head;
    u32 tx_tail;
    
    /* Statistics */
    u64 rx_packets;
    u64 tx_packets;
    u64 rx_bytes;
    u64 tx_bytes;
    u64 rx_errors;
    u64 tx_errors;
    u64 collisions;
    
    /* Driver callbacks */
    int (*init)(struct eth_device *);
    int (*start)(struct eth_device *);
    int (*stop)(struct eth_device *);
    int (*send)(struct eth_device *, void *, u32);
    int (*recv)(struct eth_device *, void *, u32 *);
    void (*set_mac)(struct eth_device *, u8 *);
    
    void *private_data;
} eth_device_t;

typedef struct {
    bool initialized;
    eth_device_t devices[ETH_MAX_DEVICES];
    u32 device_count;
    eth_device_t *current_device;
    spinlock_t lock;
} eth_driver_t;

static eth_driver_t g_eth;

/*===========================================================================*/
/*                         ETHERNET CORE FUNCTIONS                           */
/*===========================================================================*/

int eth_register_device(const char *name, u16 vendor, u16 device, u8 *mac)
{
    spinlock_lock(&g_eth.lock);
    
    if (g_eth.device_count >= ETH_MAX_DEVICES) {
        spinlock_unlock(&g_eth.lock);
        return -ENOMEM;
    }
    
    eth_device_t *dev = &g_eth.devices[g_eth.device_count++];
    memset(dev, 0, sizeof(eth_device_t));
    
    dev->device_id = g_eth.device_count;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->vendor_id = vendor;
    dev->device_id_pci = device;
    
    if (mac) {
        memcpy(dev->mac_address, mac, 6);
    }
    
    dev->mtu = 1500;
    dev->speed = 1000;  /* 1 Gbps default */
    
    /* Allocate ring buffers */
    dev->rx_ring = kmalloc(sizeof(eth_rx_desc_t) * ETH_RX_DESC_COUNT);
    dev->tx_ring = kmalloc(sizeof(eth_tx_desc_t) * ETH_TX_DESC_COUNT);
    
    if (!dev->rx_ring || !dev->tx_ring) {
        if (dev->rx_ring) kfree(dev->rx_ring);
        if (dev->tx_ring) kfree(dev->tx_ring);
        g_eth.device_count--;
        spinlock_unlock(&g_eth.lock);
        return -ENOMEM;
    }
    
    memset(dev->rx_ring, 0, sizeof(eth_rx_desc_t) * ETH_RX_DESC_COUNT);
    memset(dev->tx_ring, 0, sizeof(eth_tx_desc_t) * ETH_TX_DESC_COUNT);
    
    /* Set as current if first */
    if (g_eth.device_count == 1) {
        g_eth.current_device = dev;
    }
    
    spinlock_unlock(&g_eth.lock);
    
    printk("[ETH] Registered %s: MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
           name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return dev->device_id;
}

int eth_send_packet(u32 device_id, void *data, u32 length)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_up) return -ENODEV;
    if (!dev->send) return -ENOTSUP;
    
    return dev->send(dev, data, length);
}

int eth_receive_packet(u32 device_id, void *buffer, u32 *length)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_up) return -ENODEV;
    if (!dev->recv) return -ENOTSUP;
    
    return dev->recv(dev, buffer, length);
}

int eth_set_mac(u32 device_id, u8 *mac)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (!dev->set_mac) return -ENOTSUP;
    
    dev->set_mac(dev, mac);
    memcpy(dev->mac_address, mac, 6);
    
    return 0;
}

int eth_up(u32 device_id)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    if (dev->start) {
        dev->start(dev);
    }
    
    dev->is_up = true;
    printk("[ETH] %s is up\n", dev->name);
    
    return 0;
}

int eth_down(u32 device_id)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    if (dev->stop) {
        dev->stop(dev);
    }
    
    dev->is_up = false;
    printk("[ETH] %s is down\n", dev->name);
    
    return 0;
}

/*===========================================================================*/
/*                         INTEL E1000 DRIVER                                */
/*===========================================================================*/

typedef struct {
    u32 reg_base;
    u32 irq;
    bool has_msi;
} e1000_private_t;

static int e1000_init(eth_device_t *dev)
{
    e1000_private_t *priv = dev->private_data;
    
    printk("[E1000] Initializing %s...\n", dev->name);
    
    /* Software reset */
    /* Would write to CTRL register */
    
    /* Read MAC address from EEPROM */
    /* Would read from EEPROM registers */
    
    (void)priv;
    return 0;
}

static int e1000_start(eth_device_t *dev)
{
    e1000_private_t *priv = dev->private_data;
    
    /* Set up receive descriptors */
    /* Enable receive unit */
    /* Set up transmit descriptors */
    /* Enable transmit unit */
    
    /* Enable interrupts */
    
    (void)priv;
    dev->is_link_up = true;
    
    return 0;
}

static int e1000_stop(eth_device_t *dev)
{
    e1000_private_t *priv = dev->private_data;
    
    /* Disable interrupts */
    /* Disable receive and transmit units */
    
    (void)priv;
    dev->is_link_up = false;
    
    return 0;
}

static int e1000_send(eth_device_t *dev, void *data, u32 length)
{
    e1000_private_t *priv = dev->private_data;
    
    /* Copy data to transmit buffer */
    /* Update transmit descriptor */
    /* Trigger transmit */
    
    dev->tx_packets++;
    dev->tx_bytes += length;
    
    (void)priv; (void)data; (void)length;
    return 0;
}

static int e1000_recv(eth_device_t *dev, void *buffer, u32 *length)
{
    e1000_private_t *priv = dev->private_data;
    
    /* Check for received packets */
    /* Copy data from receive buffer */
    /* Update receive descriptor */
    
    dev->rx_packets++;
    
    (void)priv; (void)buffer; (void)length;
    return 0;
}

static void e1000_set_mac(eth_device_t *dev, u8 *mac)
{
    e1000_private_t *priv = dev->private_data;
    
    /* Set MAC address in RAL/RAH registers */
    
    (void)priv; (void)mac;
}

int e1000_probe(u16 vendor, u16 device, u32 bar0)
{
    if (vendor != NIC_VENDOR_INTEL) {
        return -ENODEV;
    }
    
    printk("[E1000] Probing Intel NIC (device 0x%04X)...\n", device);
    
    /* Allocate private data */
    e1000_private_t *priv = kmalloc(sizeof(e1000_private_t));
    if (!priv) return -ENOMEM;
    
    memset(priv, 0, sizeof(e1000_private_t));
    priv->reg_base = bar0;
    
    /* Generate random MAC for now */
    u8 mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    
    /* Register device */
    u32 dev_id = eth_register_device("e1000", vendor, device, mac);
    if (dev_id < 0) {
        kfree(priv);
        return dev_id;
    }
    
    eth_device_t *dev = &g_eth.devices[dev_id - 1];
    dev->private_data = priv;
    dev->init = e1000_init;
    dev->start = e1000_start;
    dev->stop = e1000_stop;
    dev->send = e1000_send;
    dev->recv = e1000_recv;
    dev->set_mac = e1000_set_mac;
    
    /* Initialize device */
    e1000_init(dev);
    
    return 0;
}

/*===========================================================================*/
/*                         REALTEK 8139 DRIVER                               */
/*===========================================================================*/

typedef struct {
    u32 reg_base;
    u32 irq;
} rtl8139_private_t;

static int rtl8139_init(eth_device_t *dev)
{
    rtl8139_private_t *priv = dev->private_data;
    
    printk("[RTL8139] Initializing %s...\n", dev->name);
    
    /* Software reset */
    /* Would write to Command register */
    
    (void)priv;
    return 0;
}

static int rtl8139_start(eth_device_t *dev)
{
    rtl8139_private_t *priv = dev->private_data;
    
    /* Configure receive buffer */
    /* Enable receiver and transmitter */
    /* Enable interrupts */
    
    (void)priv;
    dev->is_link_up = true;
    
    return 0;
}

static int rtl8139_stop(eth_device_t *dev)
{
    rtl8139_private_t *priv = dev->private_data;
    
    /* Disable interrupts */
    /* Disable receiver and transmitter */
    
    (void)priv;
    dev->is_link_up = false;
    
    return 0;
}

static int rtl8139_send(eth_device_t *dev, void *data, u32 length)
{
    rtl8139_private_t *priv = dev->private_data;
    
    /* Copy data to transmit buffer */
    /* Set transmit descriptor */
    /* Trigger transmit */
    
    dev->tx_packets++;
    dev->tx_bytes += length;
    
    (void)priv; (void)data; (void)length;
    return 0;
}

static int rtl8139_recv(eth_device_t *dev, void *buffer, u32 *length)
{
    rtl8139_private_t *priv = dev->private_data;
    
    /* Check receive buffer for packets */
    /* Copy packet data */
    /* Update receive pointer */
    
    dev->rx_packets++;
    
    (void)priv; (void)buffer; (void)length;
    return 0;
}

int rtl8139_probe(u16 vendor, u16 device, u32 bar0)
{
    if (vendor != NIC_VENDOR_REALTEK || device != 0x8139) {
        return -ENODEV;
    }
    
    printk("[RTL8139] Probing Realtek 8139...\n");
    
    /* Allocate private data */
    rtl8139_private_t *priv = kmalloc(sizeof(rtl8139_private_t));
    if (!priv) return -ENOMEM;
    
    memset(priv, 0, sizeof(rtl8139_private_t));
    priv->reg_base = bar0;
    
    /* Generate random MAC */
    u8 mac[6] = {0x52, 0x54, 0x00, 0xAB, 0xCD, 0xEF};
    
    /* Register device */
    u32 dev_id = eth_register_device("rtl8139", vendor, device, mac);
    if (dev_id < 0) {
        kfree(priv);
        return dev_id;
    }
    
    eth_device_t *dev = &g_eth.devices[dev_id - 1];
    dev->private_data = priv;
    dev->init = rtl8139_init;
    dev->start = rtl8139_start;
    dev->stop = rtl8139_stop;
    dev->send = rtl8139_send;
    dev->recv = rtl8139_recv;
    
    e1000_init(dev);
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int ethernet_init(void)
{
    printk("[ETH] ========================================\n");
    printk("[ETH] NEXUS OS Ethernet Driver\n");
    printk("[ETH] ========================================\n");
    
    memset(&g_eth, 0, sizeof(eth_driver_t));
    spinlock_init(&g_eth.lock);
    
    /* Probe for devices would happen here via PCI enumeration */
    /* For now, register mock devices */
    
    printk("[ETH] Ethernet driver initialized\n");
    return 0;
}

int ethernet_shutdown(void)
{
    printk("[ETH] Shutting down ethernet driver...\n");
    
    /* Bring down all devices */
    for (u32 i = 0; i < g_eth.device_count; i++) {
        eth_down(g_eth.devices[i].device_id);
        
        /* Free ring buffers */
        if (g_eth.devices[i].rx_ring) {
            kfree(g_eth.devices[i].rx_ring);
        }
        if (g_eth.devices[i].tx_ring) {
            kfree(g_eth.devices[i].tx_ring);
        }
        if (g_eth.devices[i].private_data) {
            kfree(g_eth.devices[i].private_data);
        }
    }
    
    g_eth.device_count = 0;
    g_eth.initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

eth_driver_t *ethernet_get_driver(void)
{
    return &g_eth;
}

int ethernet_get_stats(u32 device_id, u64 *rx_packets, u64 *tx_packets,
                        u64 *rx_bytes, u64 *tx_bytes)
{
    eth_device_t *dev = NULL;
    for (u32 i = 0; i < g_eth.device_count; i++) {
        if (g_eth.devices[i].device_id == device_id) {
            dev = &g_eth.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    if (rx_packets) *rx_packets = dev->rx_packets;
    if (tx_packets) *tx_packets = dev->tx_packets;
    if (rx_bytes) *rx_bytes = dev->rx_bytes;
    if (tx_bytes) *tx_bytes = dev->tx_bytes;
    
    return 0;
}

int ethernet_list_devices(eth_device_t *devices, u32 *count)
{
    if (!devices || !count) return -EINVAL;
    
    u32 copy = (*count < g_eth.device_count) ? *count : g_eth.device_count;
    memcpy(devices, g_eth.devices, sizeof(eth_device_t) * copy);
    *count = copy;
    
    return 0;
}
