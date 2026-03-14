/*
 * NEXUS OS - Icon Library
 * gui/icons/icon_library.c
 *
 * Comprehensive icon set for system, network, applications
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ICON CATEGORIES                                   */
/*===========================================================================*/

#define ICON_CATEGORY_SYSTEM        1
#define ICON_CATEGORY_NETWORK       2
#define ICON_CATEGORY_SECURITY      3
#define ICON_CATEGORY_APPLICATION   4
#define ICON_CATEGORY_DEVICE        5
#define ICON_CATEGORY_FILE          6
#define ICON_CATEGORY_ACTION        7
#define ICON_CATEGORY_STATUS        8

/*===========================================================================*/
/*                         ICON DEFINITIONS                                  */
/*===========================================================================*/

/* Icon Structure */
typedef struct {
    char name[64];
    u32 category;
    u16 width;
    u16 height;
    u32 format;  /* 0=SVG, 1=PNG, 2=BMP */
    const char *data;
    const char *description;
} icon_t;

/* Icon Library */
typedef struct {
    icon_t *icons;
    u32 icon_count;
    u32 max_icons;
    bool initialized;
} icon_library_t;

static icon_library_t g_icon_library;

/*===========================================================================*/
/*                         NETWORK ICONS (SVG)                               */
/*===========================================================================*/

/* WiFi Icons */
static const char *icon_wifi_full_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M12 3C7.5 3 3.5 4.5 0.5 7L12 21L23.5 7C20.5 4.5 16.5 3 12 3Z'/>"
"</svg>";

static const char *icon_wifi_medium_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M12 8C9 8 6 9 4 11L12 21L20 11C18 9 15 8 12 8Z'/>"
"</svg>";

static const char *icon_wifi_low_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M12 13C10.5 13 9 13.5 8 14.5L12 21L16 14.5C15 13.5 13.5 13 12 13Z'/>"
"</svg>";

static const char *icon_wifi_off_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<path d='M12 3C7.5 3 3.5 4.5 0.5 7L4 11.5C6.5 9.5 9 8.5 12 8.5C15 8.5 17.5 9.5 20 11.5L23.5 7C20.5 4.5 16.5 3 12 3Z'/>"
"<line x1='2' y2='22' x2='22' y2='2' stroke='#E74C3C' stroke-width='2'/>"
"</svg>";

/* Ethernet Icons */
static const char *icon_ethernet_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='2' y='4' width='20' height='16' rx='2' fill='#2ECC71'/>"
"<rect x='5' y='7' width='14' height='8' fill='#1E8449'/>"
"<line x1='6' y1='18' x2='6' y2='21' stroke='#2ECC71' stroke-width='2'/>"
"<line x1='10' y1='18' x2='10' y2='21' stroke='#2ECC71' stroke-width='2'/>"
"<line x1='14' y1='18' x2='14' y2='21' stroke='#2ECC71' stroke-width='2'/>"
"<line x1='18' y1='18' x2='18' y2='21' stroke='#2ECC71' stroke-width='2'/>"
"</svg>";

/* Network Status Icons */
static const char *icon_network_connected_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<circle cx='12' cy='12' r='10' fill='#2ECC71'/>"
"<path d='M7 12L10 15L17 8' stroke='white' stroke-width='2' fill='none'/>"
"</svg>";

static const char *icon_network_disconnected_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<circle cx='12' cy='12' r='10' fill='#E74C3C'/>"
"<line x1='8' y1='8' x2='16' y2='16' stroke='white' stroke-width='2'/>"
"<line x1='16' y1='8' x2='8' y2='16' stroke='white' stroke-width='2'/>"
"</svg>";

static const char *icon_network_error_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<circle cx='12' cy='12' r='10' fill='#F39C12'/>"
"<text x='12' y='16' text-anchor='middle' fill='white' font-size='14'>!</text>"
"</svg>";

/* Bluetooth Icons */
static const char *icon_bluetooth_on_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M12 2L7 7L12 12L7 17L12 22L17 17L12 12L17 7L12 2Z' stroke='#3498DB' stroke-width='2' fill='none'/>"
"<line x1='12' y1='2' x2='12' y2='22' stroke='#3498DB' stroke-width='2'/>"
"</svg>";

static const char *icon_bluetooth_off_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#95A5A6'>"
"<path d='M12 2L7 7L12 12L7 17L12 22L17 17L12 12L17 7L12 2Z' stroke='#95A5A6' stroke-width='2' fill='none'/>"
"<line x1='12' y1='2' x2='12' y2='22' stroke='#95A5A6' stroke-width='2'/>"
"<line x1='3' y1='3' x2='21' y2='21' stroke='#E74C3C' stroke-width='2'/>"
"</svg>";

/* Server Icons */
static const char *icon_server_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<rect x='3' y='3' width='18' height='7' rx='1' fill='#9B59B6'/>"
"<rect x='3' y='14' width='18' height='7' rx='1' fill='#9B59B6'/>"
"<circle cx='7' cy='6.5' r='1.5' fill='#2ECC71'/>"
"<circle cx='7' cy='17.5' r='1.5' fill='#2ECC71'/>"
"<line x1='11' y1='6.5' x2='17' y2='6.5' stroke='white' stroke-width='1'/>"
"<line x1='11' y1='17.5' x2='17' y2='17.5' stroke='white' stroke-width='1'/>"
"</svg>";

/* Cloud Icons */
static const char *icon_cloud_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z' fill='#3498DB'/>"
"</svg>";

static const char *icon_cloud_upload_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z' fill='#3498DB'/>"
"<path d='M12 16V8' stroke='white' stroke-width='2'/>"
"<path d='M8 12L12 8L16 12' stroke='white' stroke-width='2' fill='none'/>"
"</svg>";

static const char *icon_cloud_download_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<path d='M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z' fill='#3498DB'/>"
"<path d='M12 8V16' stroke='white' stroke-width='2'/>"
"<path d='M8 12L12 16L16 12' stroke='white' stroke-width='2' fill='none'/>"
"</svg>";

/* Router Icons */
static const char *icon_router_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E67E22'>"
"<rect x='2' y='8' width='20' height='10' rx='2' fill='#E67E22'/>"
"<circle cx='6' cy='13' r='1.5' fill='#2ECC71'/>"
"<circle cx='10' cy='13' r='1.5' fill='#2ECC71'/>"
"<line x1='4' y1='4' x2='8' y2='8' stroke='#E67E22' stroke-width='2'/>"
"<line x1='16' y1='4' x2='20' y2='8' stroke='#E67E22' stroke-width='2'/>"
"</svg>";

/* Firewall Icons */
static const char *icon_firewall_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<path d='M12 2L3 7V12C3 17 7 21 12 22C17 21 21 17 21 12V7L12 2Z' fill='#E74C3C'/>"
"<path d='M8 12L11 15L16 9' stroke='white' stroke-width='2' fill='none'/>"
"</svg>";

/* VPN Icons */
static const char *icon_vpn_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<rect x='3' y='11' width='18' height='10' rx='2' fill='#9B59B6'/>"
"<rect x='7' y='3' width='10' height='8' rx='1' fill='#9B59B6'/>"
"<circle cx='12' cy='16' r='2' fill='#2ECC71'/>"
"</svg>";

/*===========================================================================*/
/*                         SYSTEM ICONS                                      */
/*===========================================================================*/

/* CPU Icon */
static const char *icon_cpu_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='5' y='5' width='14' height='14' rx='2' fill='#3498DB'/>"
"<rect x='8' y='8' width='8' height='8' fill='#2ECC71'/>"
"<line x1='12' y1='2' x2='12' y2='5' stroke='#3498DB' stroke-width='2'/>"
"<line x1='12' y1='19' x2='12' y2='22' stroke='#3498DB' stroke-width='2'/>"
"<line x1='2' y1='12' x2='5' y2='12' stroke='#3498DB' stroke-width='2'/>"
"<line x1='19' y1='12' x2='22' y2='12' stroke='#3498DB' stroke-width='2'/>"
"</svg>";

/* Memory/RAM Icon */
static const char *icon_memory_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='3' y='6' width='18' height='12' rx='1' fill='#2ECC71'/>"
"<rect x='5' y='8' width='4' height='8' fill='#1E8449'/>"
"<rect x='10' y='8' width='4' height='8' fill='#1E8449'/>"
"<rect x='15' y='8' width='4' height='8' fill='#1E8449'/>"
"</svg>";

/* Storage/Disk Icon */
static const char *icon_storage_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<ellipse cx='12' cy='6' rx='8' ry='3' fill='#F39C12'/>"
"<path d='M4 6V18C4 19.5 7.5 21 12 21C16.5 21 20 19.5 20 18V6' stroke='#F39C12' stroke-width='2' fill='none'/>"
"<circle cx='12' cy='18' r='1.5' fill='#E67E22'/>"
"</svg>";

/* GPU Icon */
static const char *icon_gpu_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<rect x='2' y='6' width='20' height='12' rx='2' fill='#9B59B6'/>"
"<circle cx='8' cy='12' r='2.5' fill='#2ECC71'/>"
"<circle cx='16' cy='12' r='2.5' fill='#2ECC71'/>"
"<line x1='6' y1='4' x2='6' y2='6' stroke='#9B59B6' stroke-width='2'/>"
"<line x1='18' y1='4' x2='18' y2='6' stroke='#9B59B6' stroke-width='2'/>"
"</svg>";

/* Battery Icons */
static const char *icon_battery_full_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='2' y='6' width='18' height='12' rx='2' fill='#2ECC71'/>"
"<rect x='20' y='10' width='2' height='4' fill='#2ECC71'/>"
"<rect x='4' y='8' width='14' height='8' fill='white'/>"
"</svg>";

static const char *icon_battery_medium_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<rect x='2' y='6' width='18' height='12' rx='2' fill='#F39C12'/>"
"<rect x='20' y='10' width='2' height='4' fill='#F39C12'/>"
"<rect x='4' y='8' width='7' height='8' fill='white'/>"
"</svg>";

static const char *icon_battery_low_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<rect x='2' y='6' width='18' height='12' rx='2' fill='#E74C3C'/>"
"<rect x='20' y='10' width='2' height='4' fill='#E74C3C'/>"
"<rect x='4' y='8' width='3.5' height='8' fill='white'/>"
"</svg>";

/* Temperature Icon */
static const char *icon_temperature_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<path d='M12 2C9 2 7 4 7 7V14C7 17 9 19 12 19C15 19 17 17 17 14V7C17 4 15 2 12 2Z' fill='#E74C3C'/>"
"<circle cx='12' cy='15' r='2' fill='white'/>"
"</svg>";

/* Fan Icon */
static const char *icon_fan_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<circle cx='12' cy='12' r='2' fill='#3498DB'/>"
"<path d='M12 2C14 2 15 5 15 7C15 9 14 10 12 10C10 10 9 9 9 7C9 5 10 2 12 2Z' fill='#3498DB'/>"
"<path d='M22 12C22 14 19 15 17 15C15 15 14 14 14 12C14 10 15 9 17 9C19 9 22 10 22 12Z' fill='#3498DB'/>"
"<path d='M12 22C10 22 9 19 9 17C9 15 10 14 12 14C14 14 15 15 15 17C15 19 14 22 12 22Z' fill='#3498DB'/>"
"<path d='M2 12C2 10 5 9 7 9C9 9 10 10 10 12C10 14 9 15 7 15C5 15 2 14 2 12Z' fill='#3498DB'/>"
"</svg>";

/*===========================================================================*/
/*                         APPLICATION ICONS                                 */
/*===========================================================================*/

/* Terminal Icon */
static const char *icon_terminal_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2C3E50'>"
"<rect x='2' y='4' width='20' height='16' rx='2' fill='#2C3E50'/>"
"<path d='M6 10L10 14L6 18' stroke='#2ECC71' stroke-width='2' fill='none'/>"
"<line x1='12' y1='18' x2='18' y2='18' stroke='#2ECC71' stroke-width='2'/>"
"</svg>";

/* Settings Icon */
static const char *icon_settings_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#7F8C8D'>"
"<circle cx='12' cy='12' r='3' fill='#7F8C8D'/>"
"<path d='M12 2V4M12 20V22M4 12H2M22 12H20M5.6 5.6L7 7M17 17L18.4 18.4M5.6 18.4L7 17M17 7L18.4 5.6' stroke='#7F8C8D' stroke-width='2'/>"
"</svg>";

/* File Manager Icon */
static const char *icon_files_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<path d='M3 7V17C3 18.5 4.5 20 6 20H18C19.5 20 21 18.5 21 17V9C21 7.5 19.5 6 18 6H12L10 4H6C4.5 4 3 5.5 3 7Z' fill='#F39C12'/>"
"</svg>";

/* Browser Icon */
static const char *icon_browser_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<circle cx='12' cy='12' r='10' fill='#3498DB'/>"
"<circle cx='12' cy='12' r='8' fill='white'/>"
"<circle cx='12' cy='12' r='3' fill='#3498DB'/>"
"</svg>";

/* VM Manager Icon */
static const char *icon_vm_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<rect x='2' y='4' width='20' height='14' rx='2' fill='#9B59B6'/>"
"<rect x='5' y='7' width='14' height='8' fill='#2ECC71'/>"
"<rect x='9' y='20' width='6' height='2' fill='#9B59B6'/>"
"</svg>";

/* Container Icon */
static const char *icon_container_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='4' y='4' width='16' height='16' rx='2' fill='#3498DB'/>"
"<path d='M8 8H16M8 12H16M8 16H14' stroke='white' stroke-width='2'/>"
"</svg>";

/* Update Icon */
static const char *icon_update_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<path d='M12 2V6M12 18V22M6 12H2M22 12H18' stroke='#2ECC71' stroke-width='2'/>"
"<circle cx='12' cy='12' r='4' fill='#2ECC71'/>"
"</svg>";

/* Security Icon */
static const char *icon_security_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<path d='M12 2L3 7V12C3 17 7 21 12 22C17 21 21 17 21 12V7L12 2Z' fill='#E74C3C'/>"
"<path d='M9 12L11 14L15 10' stroke='white' stroke-width='2' fill='none'/>"
"</svg>";

/*===========================================================================*/
/*                         ICON LIBRARY FUNCTIONS                            */
/*===========================================================================*/

/* Initialize Icon Library */
int icon_library_init(void)
{
    printk("[ICONS] ========================================\n");
    printk("[ICONS] NEXUS OS Icon Library\n");
    printk("[ICONS] ========================================\n");
    
    memset(&g_icon_library, 0, sizeof(icon_library_t));
    
    /* Icons would be loaded from files in production */
    /* For now, we define them in code */
    
    g_icon_library.initialized = true;
    
    printk("[ICONS] Icon library initialized\n");
    printk("[ICONS] Categories:\n");
    printk("[ICONS]   - Network (WiFi, Ethernet, Bluetooth, Server, Cloud, Router, Firewall, VPN)\n");
    printk("[ICONS]   - System (CPU, Memory, Storage, GPU, Battery, Temperature, Fan)\n");
    printk("[ICONS]   - Applications (Terminal, Settings, Files, Browser, VM, Container, Update, Security)\n");
    printk("[ICONS] ========================================\n");
    
    return 0;
}

/* Get Icon by Name */
const char *icon_get(const char *name)
{
    if (!name || !g_icon_library.initialized) {
        return NULL;
    }
    
    /* Network Icons */
    if (strcmp(name, "wifi-full") == 0) return icon_wifi_full_svg;
    if (strcmp(name, "wifi-medium") == 0) return icon_wifi_medium_svg;
    if (strcmp(name, "wifi-low") == 0) return icon_wifi_low_svg;
    if (strcmp(name, "wifi-off") == 0) return icon_wifi_off_svg;
    if (strcmp(name, "ethernet") == 0) return icon_ethernet_svg;
    if (strcmp(name, "network-connected") == 0) return icon_network_connected_svg;
    if (strcmp(name, "network-disconnected") == 0) return icon_network_disconnected_svg;
    if (strcmp(name, "network-error") == 0) return icon_network_error_svg;
    if (strcmp(name, "bluetooth-on") == 0) return icon_bluetooth_on_svg;
    if (strcmp(name, "bluetooth-off") == 0) return icon_bluetooth_off_svg;
    if (strcmp(name, "server") == 0) return icon_server_svg;
    if (strcmp(name, "cloud") == 0) return icon_cloud_svg;
    if (strcmp(name, "cloud-upload") == 0) return icon_cloud_upload_svg;
    if (strcmp(name, "cloud-download") == 0) return icon_cloud_download_svg;
    if (strcmp(name, "router") == 0) return icon_router_svg;
    if (strcmp(name, "firewall") == 0) return icon_firewall_svg;
    if (strcmp(name, "vpn") == 0) return icon_vpn_svg;
    
    /* System Icons */
    if (strcmp(name, "cpu") == 0) return icon_cpu_svg;
    if (strcmp(name, "memory") == 0) return icon_memory_svg;
    if (strcmp(name, "storage") == 0) return icon_storage_svg;
    if (strcmp(name, "gpu") == 0) return icon_gpu_svg;
    if (strcmp(name, "battery-full") == 0) return icon_battery_full_svg;
    if (strcmp(name, "battery-medium") == 0) return icon_battery_medium_svg;
    if (strcmp(name, "battery-low") == 0) return icon_battery_low_svg;
    if (strcmp(name, "temperature") == 0) return icon_temperature_svg;
    if (strcmp(name, "fan") == 0) return icon_fan_svg;
    
    /* Application Icons */
    if (strcmp(name, "terminal") == 0) return icon_terminal_svg;
    if (strcmp(name, "settings") == 0) return icon_settings_svg;
    if (strcmp(name, "files") == 0) return icon_files_svg;
    if (strcmp(name, "browser") == 0) return icon_browser_svg;
    if (strcmp(name, "vm") == 0) return icon_vm_svg;
    if (strcmp(name, "container") == 0) return icon_container_svg;
    if (strcmp(name, "update") == 0) return icon_update_svg;
    if (strcmp(name, "security") == 0) return icon_security_svg;
    
    return NULL;
}

/* List Available Icons */
void icon_list_all(void)
{
    printk("\n[ICONS] ====== AVAILABLE ICONS ======\n");
    
    printk("[ICONS] Network:\n");
    printk("[ICONS]   wifi-full, wifi-medium, wifi-low, wifi-off\n");
    printk("[ICONS]   ethernet, network-connected, network-disconnected, network-error\n");
    printk("[ICONS]   bluetooth-on, bluetooth-off\n");
    printk("[ICONS]   server, cloud, cloud-upload, cloud-download\n");
    printk("[ICONS]   router, firewall, vpn\n");
    
    printk("[ICONS] System:\n");
    printk("[ICONS]   cpu, memory, storage, gpu\n");
    printk("[ICONS]   battery-full, battery-medium, battery-low\n");
    printk("[ICONS]   temperature, fan\n");
    
    printk("[ICONS] Applications:\n");
    printk("[ICONS]   terminal, settings, files, browser\n");
    printk("[ICONS]   vm, container, update, security\n");
    
    printk("[ICONS] ================================\n\n");
}

/* Render Icon */
int icon_render(const char *name, u32 x, u32 y, u32 size)
{
    const char *svg = icon_get(name);
    
    if (!svg) {
        printk("[ICONS] Icon not found: %s\n", name);
        return -ENOENT;
    }
    
    /* SVG would be rendered to framebuffer here */
    printk("[ICONS] Rendering '%s' at (%u,%u) size %u\n", name, x, y, size);
    
    return 0;
}
