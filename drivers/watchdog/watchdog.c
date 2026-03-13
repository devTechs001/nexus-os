/*
 * NEXUS OS - Watchdog Timer Driver
 * drivers/watchdog/watchdog.c
 *
 * Hardware watchdog timer support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         WATCHDOG CONFIGURATION                            */
/*===========================================================================*/

#define WDT_MAX_DEVICES           4
#define WDT_DEFAULT_TIMEOUT       60  /* seconds */
#define WDT_MIN_TIMEOUT           1
#define WDT_MAX_TIMEOUT           300

/* Watchdog Commands */
#define WDT_CMD_START             1
#define WDT_CMD_STOP              2
#define WDT_CMD_KEEPALIVE         3
#define WDT_CMD_SET_TIMEOUT       4
#define WDT_CMD_GET_TIMEOUT       5
#define WDT_CMD_GET_TIMELEFT      6

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct wdt_device {
    u32 device_id;
    char name[32];
    u16 vendor_id;
    u16 device_id_pci;
    u32 base_addr;
    u32 timeout;
    u32 timeleft;
    bool is_running;
    bool is_enabled;
    bool nowayout;
    void (*start)(struct wdt_device *);
    void (*stop)(struct wdt_device *);
    void (*kick)(struct wdt_device *);
    u32 (*get_timeleft)(struct wdt_device *);
    void *private_data;
} wdt_device_t;

typedef struct {
    bool initialized;
    wdt_device_t devices[WDT_MAX_DEVICES];
    u32 device_count;
    wdt_device_t *current_device;
    spinlock_t lock;
} wdt_driver_t;

static wdt_driver_t g_wdt;

/*===========================================================================*/
/*                         WATCHDOG CORE FUNCTIONS                           */
/*===========================================================================*/

int wdt_register_device(const char *name, u16 vendor, u16 device, u32 base)
{
    spinlock_lock(&g_wdt.lock);
    
    if (g_wdt.device_count >= WDT_MAX_DEVICES) {
        spinlock_unlock(&g_wdt.lock);
        return -ENOMEM;
    }
    
    wdt_device_t *dev = &g_wdt.devices[g_wdt.device_count++];
    memset(dev, 0, sizeof(wdt_device_t));
    
    dev->device_id = g_wdt.device_count;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->vendor_id = vendor;
    dev->device_id_pci = device;
    dev->base_addr = base;
    dev->timeout = WDT_DEFAULT_TIMEOUT;
    dev->nowayout = false;
    
    if (g_wdt.device_count == 1) {
        g_wdt.current_device = dev;
    }
    
    spinlock_unlock(&g_wdt.lock);
    
    printk("[WDT] Registered %s at 0x%08X\n", name, base);
    return dev->device_id;
}

int wdt_start(u32 device_id)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    if (dev->start) {
        dev->start(dev);
    }
    
    dev->is_running = true;
    printk("[WDT] %s started (timeout: %ds)\n", dev->name, dev->timeout);
    
    return 0;
}

int wdt_stop(u32 device_id)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (dev->nowayout) return -EPERM;
    
    if (dev->stop) {
        dev->stop(dev);
    }
    
    dev->is_running = false;
    printk("[WDT] %s stopped\n", dev->name);
    
    return 0;
}

int wdt_kick(u32 device_id)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_running) return -ENODEV;
    
    if (dev->kick) {
        dev->kick(dev);
    }
    
    return 0;
}

int wdt_set_timeout(u32 device_id, u32 timeout)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (timeout < WDT_MIN_TIMEOUT || timeout > WDT_MAX_TIMEOUT) {
        return -EINVAL;
    }
    
    dev->timeout = timeout;
    printk("[WDT] %s timeout set to %ds\n", dev->name, timeout);
    
    return 0;
}

int wdt_get_timeout(u32 device_id, u32 *timeout)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev || !timeout) return -EINVAL;
    
    *timeout = dev->timeout;
    return 0;
}

int wdt_get_timeleft(u32 device_id, u32 *timeleft)
{
    wdt_device_t *dev = NULL;
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].device_id == device_id) {
            dev = &g_wdt.devices[i];
            break;
        }
    }
    
    if (!dev || !timeleft) return -EINVAL;
    
    if (dev->get_timeleft) {
        *timeleft = dev->get_timeleft(dev);
    } else {
        *timeleft = dev->timeout;
    }
    
    return 0;
}

/*===========================================================================*/
/*                         I801 WATCHDOG (Intel)                             */
/*===========================================================================*/

#define I801_WDT_BASE               0x1800
#define I801_WDT_CONTROL            0x00
#define I801_WDT_TIMEOUT            0x04
#define I801_WDT_KICK               0x08

typedef struct {
    u32 magic;
} i801_private_t;

static void i801_start(wdt_device_t *dev)
{
    /* Enable watchdog */
    // vga_outl(dev->base_addr + I801_WDT_CONTROL, 0x01);
    printk("[I801-WDT] Started\n");
}

static void i801_stop(wdt_device_t *dev)
{
    /* Disable watchdog */
    // vga_outl(dev->base_addr + I801_WDT_CONTROL, 0x00);
    printk("[I801-WDT] Stopped\n");
}

static void i801_kick(wdt_device_t *dev)
{
    /* Kick watchdog */
    // vga_outl(dev->base_addr + I801_WDT_KICK, 0x01);
    (void)dev;
}

static u32 i801_get_timeleft(wdt_device_t *dev)
{
    /* Read remaining time */
    // return vga_inl(dev->base_addr + I801_WDT_TIMEOUT);
    (void)dev;
    return dev->timeout;
}

int i801_wdt_probe(u16 vendor, u16 device, u32 base)
{
    if (vendor != 0x8086) return -ENODEV;
    
    printk("[I801-WDT] Probing Intel watchdog...\n");
    
    i801_private_t *priv = kmalloc(sizeof(i801_private_t));
    if (!priv) return -ENOMEM;
    
    priv->magic = 0x12345678;
    
    u32 dev_id = wdt_register_device("i801_wdt", vendor, device, I801_WDT_BASE);
    if (dev_id < 0) {
        kfree(priv);
        return dev_id;
    }
    
    wdt_device_t *dev = &g_wdt.devices[dev_id - 1];
    dev->private_data = priv;
    dev->start = i801_start;
    dev->stop = i801_stop;
    dev->kick = i801_kick;
    dev->get_timeleft = i801_get_timeleft;
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int watchdog_init(void)
{
    printk("[WDT] ========================================\n");
    printk("[WDT] NEXUS OS Watchdog Driver\n");
    printk("[WDT] ========================================\n");
    
    memset(&g_wdt, 0, sizeof(wdt_driver_t));
    spinlock_init(&g_wdt.lock);
    
    /* Probe for watchdog hardware */
    /* Would enumerate hardware here */
    
    printk("[WDT] Watchdog driver initialized\n");
    return 0;
}

int watchdog_shutdown(void)
{
    printk("[WDT] Shutting down watchdog driver...\n");
    
    /* Stop all running watchdogs */
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        wdt_stop(g_wdt.devices[i].device_id);
        
        if (g_wdt.devices[i].private_data) {
            kfree(g_wdt.devices[i].private_data);
        }
    }
    
    g_wdt.device_count = 0;
    g_wdt.initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

wdt_driver_t *watchdog_get_driver(void)
{
    return &g_wdt;
}

/* Kernel panic handler - kick watchdog */
void watchdog_panic_kick(void)
{
    for (u32 i = 0; i < g_wdt.device_count; i++) {
        if (g_wdt.devices[i].is_running) {
            wdt_kick(g_wdt.devices[i].device_id);
        }
    }
}
