/*
 * NEXUS OS - PCI Driver
 * drivers/pci/pci.c
 *
 * PCI/PCIe bus enumeration and device management
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         PCI CONFIGURATION                                 */
/*===========================================================================*/

#define PCI_MAX_DEVICES           256
#define PCI_MAX_BARS              6
#define PCI_MAX_IRQ_LINES         32
#define PCI_ANY_ID                0xFFFFFFFF

/* PCI Config Space Offsets */
#define PCI_CONFIG_VENDOR_ID      0x00
#define PCI_CONFIG_DEVICE_ID      0x02
#define PCI_CONFIG_COMMAND        0x04
#define PCI_CONFIG_STATUS         0x06
#define PCI_CONFIG_REVISION       0x08
#define PCI_CONFIG_CLASS          0x09
#define PCI_CONFIG_CACHE_LINE     0x0C
#define PCI_CONFIG_LATENCY        0x0D
#define PCI_CONFIG_HEADER_TYPE    0x0E
#define PCI_CONFIG_BAR0           0x10
#define PCI_CONFIG_INTERRUPT      0x3C

/* PCI Command Register Bits */
#define PCI_CMD_IO                0x0001
#define PCI_CMD_MEMORY            0x0002
#define PCI_CMD_BUS_MASTER        0x0004
#define PCI_CMD_SPECIAL_CYCLE     0x0008
#define PCI_CMD_MEMORY_WRITE_INVALID 0x0010
#define PCI_CMD_VGA_PALETTE       0x0020
#define PCI_CMD_PARITY_ERROR      0x0040
#define PCI_CMD_WAIT_CYCLE        0x0080
#define PCI_CMD_SERR              0x0100
#define PCI_CMD_FAST_BACK         0x0200

/* PCI Header Types */
#define PCI_HEADER_TYPE_NORMAL    0x00
#define PCI_HEADER_TYPE_BRIDGE    0x01
#define PCI_HEADER_TYPE_CARDBUS   0x02

/* PCI Class Codes */
#define PCI_CLASS_NOT_DEFINED     0x00
#define PCI_CLASS_STORAGE         0x01
#define PCI_CLASS_NETWORK         0x02
#define PCI_CLASS_DISPLAY         0x03
#define PCI_CLASS_MULTIMEDIA      0x04
#define PCI_CLASS_MEMORY          0x05
#define PCI_CLASS_BRIDGE          0x06
#define PCI_CLASS_SIMPLE_COMM     0x07
#define PCI_CLASS_BASE_SYSTEM     0x08
#define PCI_CLASS_INPUT           0x09
#define PCI_CLASS_DOCKING         0x0A
#define PCI_CLASS_PROCESSOR       0x0B
#define PCI_CLASS_SERIAL          0x0C
#define PCI_CLASS_WIRELESS        0x0D
#define PCI_CLASS_I2O             0x0E
#define PCI_CLASS_SATELLITE       0x0F
#define PCI_CLASS_ENCRYPTION      0x10
#define PCI_CLASS_SIGNAL          0x11
#define PCI_CLASS_ANY             0xFF

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 bus;
    u32 device;
    u32 function;
    u16 vendor_id;
    u16 device_id;
    u16 command;
    u16 status;
    u8 revision_id;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 header_type;
    u8 irq_line;
    u8 irq_pin;
    u32 bar[PCI_MAX_BARS];
    u32 bar_size[PCI_MAX_BARS];
    bool is_enabled;
    void *driver_data;
} pci_device_t;

typedef struct {
    u16 vendor_id;
    u16 device_id;
    int (*probe)(u16 vendor, u16 device, u32 bar0);
    const char *name;
} pci_driver_t;

typedef struct {
    bool initialized;
    pci_device_t devices[PCI_MAX_DEVICES];
    u32 device_count;
    pci_driver_t drivers[32];
    u32 driver_count;
    spinlock_t lock;
} pci_manager_t;

static pci_manager_t g_pci;

/*===========================================================================*/
/*                         PCI LOW-LEVEL OPERATIONS                          */
/*===========================================================================*/

static inline void pci_write_config_byte(u32 bus, u32 dev, u32 func, 
                                          u32 offset, u8 value)
{
    u32 address = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | offset;
    vga_outb(0xCF8, address & 0xFF);
    vga_outb(0xCF9, (address >> 8) & 0xFF);
    vga_outb(0xCFA, (address >> 16) & 0xFF);
    vga_outb(0xCFB, (address >> 24) & 0xFF);
    vga_outb(0xCFC, value);
}

static inline u8 pci_read_config_byte(u32 bus, u32 dev, u32 func, u32 offset)
{
    u32 address = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | offset;
    vga_outb(0xCF8, address & 0xFF);
    vga_outb(0xCF9, (address >> 8) & 0xFF);
    vga_outb(0xCFA, (address >> 16) & 0xFF);
    vga_outb(0xCFB, (address >> 24) & 0xFF);
    return vga_inb(0xCFC);
}

static inline u16 pci_read_config_word(u32 bus, u32 dev, u32 func, u32 offset)
{
    u32 address = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | offset;
    vga_outb(0xCF8, address & 0xFF);
    vga_outb(0xCF9, (address >> 8) & 0xFF);
    vga_outb(0xCFA, (address >> 16) & 0xFF);
    vga_outb(0xCFB, (address >> 24) & 0xFF);
    return vga_inw(0xCFC);
}

static inline u32 pci_read_config_dword(u32 bus, u32 dev, u32 func, u32 offset)
{
    u32 address = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | offset;
    vga_outb(0xCF8, address & 0xFF);
    vga_outb(0xCF9, (address >> 8) & 0xFF);
    vga_outb(0xCFA, (address >> 16) & 0xFF);
    vga_outb(0xCFB, (address >> 24) & 0xFF);
    return vga_inl(0xCFC);
}

/*===========================================================================*/
/*                         PCI DEVICE OPERATIONS                             */
/*===========================================================================*/

static void pci_scan_bus(u32 bus);

static void pci_scan_device(u32 bus, u32 dev, u32 func)
{
    if (g_pci.device_count >= PCI_MAX_DEVICES) return;

    u16 vendor_id = pci_read_config_word(bus, dev, func, PCI_CONFIG_VENDOR_ID);
    if (vendor_id == 0xFFFF) return;  /* Device not present */
    
    pci_device_t *pci_dev = &g_pci.devices[g_pci.device_count++];
    memset(pci_dev, 0, sizeof(pci_device_t));
    
    pci_dev->bus = bus;
    pci_dev->device = dev;
    pci_dev->function = func;
    pci_dev->vendor_id = vendor_id;
    pci_dev->device_id = pci_read_config_word(bus, dev, func, PCI_CONFIG_DEVICE_ID);
    pci_dev->command = pci_read_config_word(bus, dev, func, PCI_CONFIG_COMMAND);
    pci_dev->status = pci_read_config_word(bus, dev, func, PCI_CONFIG_STATUS);
    pci_dev->revision_id = pci_read_config_byte(bus, dev, func, PCI_CONFIG_REVISION);
    pci_dev->class_code = pci_read_config_byte(bus, dev, func, PCI_CONFIG_CLASS);
    pci_dev->subclass = pci_read_config_byte(bus, dev, func, PCI_CONFIG_CLASS + 1);
    pci_dev->prog_if = pci_read_config_byte(bus, dev, func, PCI_CONFIG_CLASS + 2);
    pci_dev->header_type = pci_read_config_byte(bus, dev, func, PCI_CONFIG_HEADER_TYPE);
    pci_dev->irq_line = pci_read_config_byte(bus, dev, func, PCI_CONFIG_INTERRUPT);
    pci_dev->irq_pin = pci_read_config_byte(bus, dev, func, PCI_CONFIG_INTERRUPT + 1);
    
    /* Read BARs */
    for (int i = 0; i < PCI_MAX_BARS; i++) {
        u32 bar_offset = PCI_CONFIG_BAR0 + (i * 4);
        u32 bar_value = pci_read_config_dword(bus, dev, func, bar_offset);
        
        /* Write all 1s to determine size */
        pci_write_config_byte(bus, dev, func, bar_offset, 0xFF);
        u32 bar_size = pci_read_config_dword(bus, dev, func, bar_offset);
        pci_write_config_dword(bus, dev, func, bar_offset, bar_value);
        
        pci_dev->bar[i] = bar_value;
        pci_dev->bar_size[i] = ~(bar_size & ~0xF) + 1;
    }
    
    printk("[PCI] Found device %04X:%04X (class %02X) at %02X:%02X.%d\n",
           vendor_id, pci_dev->device_id, pci_dev->class_code,
           bus, dev, func);
    
    /* Check if it's a bridge */
    if (pci_dev->header_type == PCI_HEADER_TYPE_BRIDGE) {
        /* Scan secondary bus */
        u8 secondary_bus = pci_read_config_byte(bus, dev, func, 0x19);
        if (secondary_bus > bus) {
            pci_scan_bus(secondary_bus);
        }
    }
}

static void pci_scan_bus(u32 bus)
{
    for (u32 dev = 0; dev < 32; dev++) {
        for (u32 func = 0; func < 8; func++) {
            pci_scan_device(bus, dev, func);
        }
    }
}

/*===========================================================================*/
/*                         PCI DRIVER REGISTRATION                           */
/*===========================================================================*/

int pci_register_driver(u16 vendor, u16 device, int (*probe)(u16, u16, u32),
                         const char *name)
{
    if (g_pci.driver_count >= 32) return -ENOMEM;
    
    pci_driver_t *drv = &g_pci.drivers[g_pci.driver_count++];
    drv->vendor_id = vendor;
    drv->device_id = device;
    drv->probe = probe;
    drv->name = name;
    
    printk("[PCI] Registered driver: %s (%04X:%04X)\n", name, vendor, device);
    return 0;
}

static void pci_match_drivers(void)
{
    for (u32 i = 0; i < g_pci.device_count; i++) {
        pci_device_t *dev = &g_pci.devices[i];
        
        for (u32 j = 0; j < g_pci.driver_count; j++) {
            pci_driver_t *drv = &g_pci.drivers[j];
            
            if ((drv->vendor_id == PCI_ANY_ID || drv->vendor_id == dev->vendor_id) &&
                (drv->device_id == PCI_ANY_ID || drv->device_id == dev->device_id)) {
                
                printk("[PCI] Matching driver %s to device %04X:%04X\n",
                       drv->name, dev->vendor_id, dev->device_id);
                
                if (drv->probe) {
                    drv->probe(dev->vendor_id, dev->device_id, dev->bar[0]);
                }
                
                dev->driver_data = drv;
                break;
            }
        }
    }
}

/*===========================================================================*/
/*                         PCI DEVICE ENABLE/DISABLE                         */
/*===========================================================================*/

int pci_enable_device(pci_device_t *dev)
{
    if (!dev || dev->is_enabled) return 0;
    
    /* Enable memory and I/O space */
    u16 cmd = pci_read_config_word(dev->bus, dev->device, dev->function, 
                                    PCI_CONFIG_COMMAND);
    cmd |= PCI_CMD_MEMORY | PCI_CMD_IO | PCI_CMD_BUS_MASTER;
    pci_write_config_byte(dev->bus, dev->device, dev->function,
                          PCI_CONFIG_COMMAND, cmd & 0xFF);
    pci_write_config_byte(dev->bus, dev->device, dev->function,
                          PCI_CONFIG_COMMAND + 1, (cmd >> 8) & 0xFF);
    
    dev->is_enabled = true;
    printk("[PCI] Enabled device %04X:%04X\n", dev->vendor_id, dev->device_id);
    
    return 0;
}

int pci_disable_device(pci_device_t *dev)
{
    if (!dev || !dev->is_enabled) return 0;
    
    /* Disable memory and I/O space */
    u16 cmd = pci_read_config_word(dev->bus, dev->device, dev->function,
                                    PCI_CONFIG_COMMAND);
    cmd &= ~(PCI_CMD_MEMORY | PCI_CMD_IO | PCI_CMD_BUS_MASTER);
    pci_write_config_byte(dev->bus, dev->device, dev->function,
                          PCI_CONFIG_COMMAND, cmd & 0xFF);
    pci_write_config_byte(dev->bus, dev->device, dev->function,
                          PCI_CONFIG_COMMAND + 1, (cmd >> 8) & 0xFF);
    
    dev->is_enabled = false;
    printk("[PCI] Disabled device %04X:%04X\n", dev->vendor_id, dev->device_id);
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int pci_init(void)
{
    printk("[PCI] ========================================\n");
    printk("[PCI] NEXUS OS PCI Manager\n");
    printk("[PCI] ========================================\n");
    
    memset(&g_pci, 0, sizeof(pci_manager_t));
    spinlock_init(&g_pci.lock);
    
    /* Scan all PCI buses */
    printk("[PCI] Scanning PCI buses...\n");
    
    /* Scan bus 0 */
    pci_scan_bus(0);
    
    printk("[PCI] Found %d PCI devices\n", g_pci.device_count);
    
    /* Match drivers to devices */
    pci_match_drivers();
    
    g_pci.initialized = true;
    
    return 0;
}

int pci_shutdown(void)
{
    printk("[PCI] Shutting down PCI manager...\n");
    
    /* Disable all devices */
    for (u32 i = 0; i < g_pci.device_count; i++) {
        pci_disable_device(&g_pci.devices[i]);
    }
    
    g_pci.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

pci_device_t *pci_get_device(u32 index)
{
    if (index >= g_pci.device_count) return NULL;
    return &g_pci.devices[index];
}

pci_device_t *pci_find_device(u16 vendor, u16 device, u32 index)
{
    u32 found = 0;
    
    for (u32 i = 0; i < g_pci.device_count; i++) {
        if (g_pci.devices[i].vendor_id == vendor &&
            g_pci.devices[i].device_id == device) {
            if (found == index) {
                return &g_pci.devices[i];
            }
            found++;
        }
    }
    
    return NULL;
}

pci_device_t *pci_find_class(u8 class, u8 subclass, u32 index)
{
    u32 found = 0;
    
    for (u32 i = 0; i < g_pci.device_count; i++) {
        if (g_pci.devices[i].class_code == class &&
            g_pci.devices[i].subclass == subclass) {
            if (found == index) {
                return &g_pci.devices[i];
            }
            found++;
        }
    }
    
    return NULL;
}

const char *pci_get_class_name(u8 class)
{
    switch (class) {
        case PCI_CLASS_NOT_DEFINED:  return "Undefined";
        case PCI_CLASS_STORAGE:      return "Storage";
        case PCI_CLASS_NETWORK:      return "Network";
        case PCI_CLASS_DISPLAY:      return "Display";
        case PCI_CLASS_MULTIMEDIA:   return "Multimedia";
        case PCI_CLASS_MEMORY:       return "Memory";
        case PCI_CLASS_BRIDGE:       return "Bridge";
        case PCI_CLASS_SIMPLE_COMM:  return "Communication";
        case PCI_CLASS_BASE_SYSTEM:  return "Base System";
        case PCI_CLASS_INPUT:        return "Input Device";
        case PCI_CLASS_PROCESSOR:    return "Processor";
        case PCI_CLASS_SERIAL:       return "Serial Bus";
        case PCI_CLASS_WIRELESS:     return "Wireless";
        default:                     return "Unknown";
    }
}

pci_manager_t *pci_get_manager(void)
{
    return &g_pci;
}
