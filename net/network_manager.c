/*
 * NEXUS OS - Network Manager
 * net/network_manager.c
 *
 * Network configuration, connection management, VPN support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         NETWORK MANAGER CONFIGURATION                     */
/*===========================================================================*/

#define NET_MAX_INTERFACES        16
#define NET_MAX_CONNECTIONS       64
#define NET_MAX_ROUTES            128
#define NET_MAX_DNS               8
#define NET_MAX_WIFI_NETWORKS     64

/*===========================================================================*/
/*                         NETWORK CONSTANTS                                 */
/*===========================================================================*/

#define NET_IFACE_ETHERNET        1
#define NET_IFACE_WIFI            2
#define NET_IFACE_LOOPBACK        3
#define NET_IFACE_BRIDGE          4
#define NET_IFACE_VLAN            5
#define NET_IFACE_TUNNEL          6
#define NET_IFACE_VPN             7

#define NET_CONN_STATE_DISCONNECTED  0
#define NET_CONN_STATE_CONNECTING    1
#define NET_CONN_STATE_CONNECTED     2
#define NET_CONN_STATE_DISCONNECTING 3
#define NET_CONN_STATE_ERROR         4

#define NET_WIFI_SEC_NONE         0
#define NET_WIFI_SEC_WEP          1
#define NET_WIFI_SEC_WPA          2
#define NET_WIFI_SEC_WPA2         3
#define NET_WIFI_SEC_WPA3         4
#define NET_WIFI_SEC_WPA2_ENT     5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u8 addr[6];                 /* MAC Address */
} mac_addr_t;

typedef struct {
    union {
        u8 addr6[16];
        u32 addr;
    };
    u32 prefix;
} ip_addr_t;

typedef struct {
    u32 interface_id;
    char name[32];
    u32 type;
    mac_addr_t mac;
    ip_addr_t ip;
    ip_addr_t netmask;
    ip_addr_t gateway;
    ip_addr_t dns[NET_MAX_DNS];
    u32 dns_count;
    u32 mtu;
    u32 speed;
    bool is_up;
    bool is_running;
    bool is_dhcp;
    u64 rx_bytes;
    u64 tx_bytes;
    u64 rx_packets;
    u64 tx_packets;
    u64 rx_errors;
    u64 tx_errors;
} net_interface_t;

typedef struct {
    u32 ssid_len;
    char ssid[33];
    mac_addr_t bssid;
    u32 security;
    s32 signal;                 /* dBm */
    u32 channel;
    u32 frequency;
    bool is_connected;
} wifi_network_t;

typedef struct {
    u32 connection_id;
    char name[64];
    u32 interface_id;
    u32 state;
    ip_addr_t local_ip;
    ip_addr_t remote_ip;
    u32 mtu;
    u64 rx_bytes;
    u64 tx_bytes;
    u64 connect_time;
    void *driver_data;
} net_connection_t;

typedef struct {
    ip_addr_t dest;
    ip_addr_t gateway;
    ip_addr_t netmask;
    u32 metric;
    u32 interface_id;
} net_route_t;

typedef struct {
    bool initialized;
    net_interface_t interfaces[NET_MAX_INTERFACES];
    u32 interface_count;
    net_connection_t connections[NET_MAX_CONNECTIONS];
    u32 connection_count;
    net_route_t routes[NET_MAX_ROUTES];
    u32 route_count;
    u32 default_interface;
    bool dhcp_enabled;
    void (*on_interface_change)(net_interface_t *, int event);
    void (*on_connection_change)(net_connection_t *, int state);
} net_manager_t;

static net_manager_t g_net;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int net_manager_init(void)
{
    printk("[NET] ========================================\n");
    printk("[NET] NEXUS OS Network Manager\n");
    printk("[NET] ========================================\n");
    
    memset(&g_net, 0, sizeof(net_manager_t));
    g_net.initialized = true;
    
    /* Create loopback interface */
    net_interface_t *lo = &g_net.interfaces[g_net.interface_count++];
    lo->interface_id = 1;
    strcpy(lo->name, "lo");
    lo->type = NET_IFACE_LOOPBACK;
    lo->ip.addr = 0x7F000001;  /* 127.0.0.1 */
    lo->netmask.addr = 0xFF000000;
    lo->mtu = 65536;
    lo->is_up = true;
    lo->is_running = true;
    
    printk("[NET] Network Manager initialized\n");
    return 0;
}

int net_manager_shutdown(void)
{
    printk("[NET] Shutting down Network Manager...\n");
    
    /* Bring down all interfaces */
    for (u32 i = 0; i < g_net.interface_count; i++) {
        net_interface_t *iface = &g_net.interfaces[i];
        if (iface->type != NET_IFACE_LOOPBACK) {
            iface->is_up = false;
        }
    }
    
    g_net.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         INTERFACE MANAGEMENT                              */
/*===========================================================================*/

int net_manager_enumerate_interfaces(void)
{
    printk("[NET] Enumerating network interfaces...\n");
    
    /* Mock Ethernet interface */
    net_interface_t *eth = &g_net.interfaces[g_net.interface_count++];
    eth->interface_id = 2;
    strcpy(eth->name, "eth0");
    eth->type = NET_IFACE_ETHERNET;
    eth->mac.addr[0] = 0x52;
    eth->mac.addr[1] = 0x54;
    eth->mac.addr[2] = 0x00;
    eth->mac.addr[3] = 0x12;
    eth->mac.addr[4] = 0x34;
    eth->mac.addr[5] = 0x56;
    eth->mtu = 1500;
    eth->speed = 1000;  /* Mbps */
    eth->is_up = true;
    eth->is_running = true;
    eth->is_dhcp = true;
    
    /* Mock WiFi interface */
    net_interface_t *wlan = &g_net.interfaces[g_net.interface_count++];
    wlan->interface_id = 3;
    strcpy(wlan->name, "wlan0");
    wlan->type = NET_IFACE_WIFI;
    wlan->mac.addr[0] = 0xAA;
    wlan->mac.addr[1] = 0xBB;
    wlan->mac.addr[2] = 0xCC;
    wlan->mac.addr[3] = 0xDD;
    wlan->mac.addr[4] = 0xEE;
    wlan->mac.addr[5] = 0xFF;
    wlan->mtu = 1500;
    wlan->speed = 300;  /* Mbps */
    wlan->is_up = false;
    wlan->is_running = true;
    wlan->is_dhcp = true;
    
    printk("[NET] Found %d interfaces\n", g_net.interface_count);
    return g_net.interface_count;
}

net_interface_t *net_get_interface(u32 id)
{
    for (u32 i = 0; i < g_net.interface_count; i++) {
        if (g_net.interfaces[i].interface_id == id) {
            return &g_net.interfaces[i];
        }
    }
    return NULL;
}

net_interface_t *net_get_interface_by_name(const char *name)
{
    for (u32 i = 0; i < g_net.interface_count; i++) {
        if (strcmp(g_net.interfaces[i].name, name) == 0) {
            return &g_net.interfaces[i];
        }
    }
    return NULL;
}

int net_set_ip(u32 interface_id, ip_addr_t *ip, ip_addr_t *netmask, ip_addr_t *gateway)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    
    if (ip) iface->ip = *ip;
    if (netmask) iface->netmask = *netmask;
    if (gateway) iface->gateway = *gateway;
    iface->is_dhcp = false;
    
    printk("[NET] %s: IP set to %d.%d.%d.%d\n", iface->name,
           (iface->ip.addr >> 0) & 0xFF,
           (iface->ip.addr >> 8) & 0xFF,
           (iface->ip.addr >> 16) & 0xFF,
           (iface->ip.addr >> 24) & 0xFF);
    
    return 0;
}

int net_set_dhcp(u32 interface_id, bool enabled)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    
    iface->is_dhcp = enabled;
    
    if (enabled) {
        printk("[NET] %s: DHCP enabled\n", iface->name);
        /* In real implementation, would start DHCP client */
    }
    
    return 0;
}

int net_bring_up(u32 interface_id)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    
    iface->is_up = true;
    printk("[NET] %s: Interface up\n", iface->name);
    
    if (g_net.on_interface_change) {
        g_net.on_interface_change(iface, 1);  /* UP event */
    }
    
    return 0;
}

int net_bring_down(u32 interface_id)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    
    iface->is_up = false;
    printk("[NET] %s: Interface down\n", iface->name);
    
    if (g_net.on_interface_change) {
        g_net.on_interface_change(iface, 0);  /* DOWN event */
    }
    
    return 0;
}

/*===========================================================================*/
/*                         WIFI MANAGEMENT                                   */
/*===========================================================================*/

/* WiFi functions now implemented in drivers/network/wifi.c */
/*
int wifi_scan(u32 interface_id, wifi_network_t *networks, u32 *count)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface || iface->type != NET_IFACE_WIFI) {
        return -EINVAL;
    }

    if (!iface->is_up) {
        return -ENETDOWN;
    }

    printk("[WIFI] Scanning for networks...\n");

    // Mock discovered networks
    u32 found = 0;

    wifi_network_t *net = &networks[found++];
    net->ssid_len = 10;
    strcpy(net->ssid, "HomeWiFi");
    net->bssid.addr[0] = 0x00;
    net->bssid.addr[1] = 0x11;
    net->bssid.addr[2] = 0x22;
    net->bssid.addr[3] = 0x33;
    net->bssid.addr[4] = 0x44;
    net->bssid.addr[5] = 0x55;
    net->security = NET_WIFI_SEC_WPA2;
    net->signal = -45;
    net->channel = 6;
    net->frequency = 2437;
    net->is_connected = false;

    net = &networks[found++];
    net->ssid_len = 8;
    strcpy(net->ssid, "Office5G");
    net->security = NET_WIFI_SEC_WPA3;
    net->signal = -62;
    net->channel = 36;
    net->frequency = 5180;
    net->is_connected = false;

    net = &networks[found++];
    net->ssid_len = 6;
    strcpy(net->ssid, "Cafe");
    net->security = NET_WIFI_SEC_NONE;
    net->signal = -78;
    net->channel = 11;
    net->frequency = 2462;
    net->is_connected = false;

    *count = found;
    printk("[WIFI] Found %d networks\n", found);

    return found;
}

int wifi_connect(u32 interface_id, const char *ssid, const char *password, u32 security)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface || iface->type != NET_IFACE_WIFI) {
        return -EINVAL;
    }

    printk("[WIFI] Connecting to '%s'...\n", ssid);

    // In real implementation, would use wpa_supplicant
    // Mock connection
    iface->is_up = true;
    iface->ip.addr = 0xC0A80164;  // 192.168.1.100
    iface->netmask.addr = 0xFFFFFF00;
    iface->gateway.addr = 0xC0A80101;  // 192.168.1.1

    printk("[WIFI] Connected to '%s'\n", ssid);
    return 0;
}

int wifi_disconnect(u32 interface_id)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface || iface->type != NET_IFACE_WIFI) {
        return -EINVAL;
    }

    printk("[WIFI] Disconnecting...\n");
    iface->is_up = false;

    return 0;
}
*/

/*===========================================================================*/
/*                         ROUTE MANAGEMENT                                  */
/*===========================================================================*/

int net_add_route(ip_addr_t *dest, ip_addr_t *gateway, ip_addr_t *netmask, u32 metric)
{
    if (g_net.route_count >= NET_MAX_ROUTES) {
        return -ENOMEM;
    }
    
    net_route_t *route = &g_net.routes[g_net.route_count++];
    route->dest = *dest;
    route->gateway = *gateway;
    route->netmask = *netmask;
    route->metric = metric;
    route->interface_id = g_net.default_interface;
    
    printk("[NET] Route added: %d.%d.%d.%d/%d via %d.%d.%d.%d\n",
           (dest->addr >> 0) & 0xFF, (dest->addr >> 8) & 0xFF,
           (dest->addr >> 16) & 0xFF, (dest->addr >> 24) & 0xFF,
           netmask->prefix,
           (gateway->addr >> 0) & 0xFF, (gateway->addr >> 8) & 0xFF,
           (gateway->addr >> 16) & 0xFF, (gateway->addr >> 24) & 0xFF);
    
    return 0;
}

int net_add_default_route(u32 interface_id, ip_addr_t *gateway)
{
    ip_addr_t dest = {0};
    ip_addr_t netmask = {0};
    netmask.prefix = 0;
    
    g_net.default_interface = interface_id;
    return net_add_route(&dest, gateway, &netmask, 0);
}

/*===========================================================================*/
/*                         DNS CONFIGURATION                                 */
/*===========================================================================*/

int net_set_dns(u32 interface_id, ip_addr_t *dns, u32 count)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    if (count > NET_MAX_DNS) count = NET_MAX_DNS;
    
    for (u32 i = 0; i < count; i++) {
        iface->dns[i] = dns[i];
    }
    iface->dns_count = count;
    
    printk("[NET] %s: DNS servers set (%d)\n", iface->name, count);
    return 0;
}

/*===========================================================================*/
/*                         VPN SUPPORT                                       */
/*===========================================================================*/

typedef struct {
    char name[64];
    char server[64];
    u32 port;
    u32 protocol;  /* 0=OpenVPN, 1=WireGuard, 2=IPSec */
    char username[64];
    char password[64];
    bool auto_connect;
} vpn_config_t;

int vpn_connect(vpn_config_t *config)
{
    if (!config) return -EINVAL;
    
    printk("[VPN] Connecting to %s:%d (%s)...\n", 
           config->server, config->port,
           config->protocol == 0 ? "OpenVPN" : 
           config->protocol == 1 ? "WireGuard" : "IPSec");
    
    /* Create VPN connection */
    if (g_net.connection_count >= NET_MAX_CONNECTIONS) {
        return -ENOMEM;
    }
    
    net_connection_t *conn = &g_net.connections[g_net.connection_count++];
    conn->connection_id = g_net.connection_count;
    strcpy(conn->name, config->name);
    conn->state = NET_CONN_STATE_CONNECTED;
    conn->local_ip.addr = 0x0A080001;  /* 10.8.0.1 */
    conn->remote_ip.addr = 0x0A0800FE;  /* 10.8.0.254 */
    conn->mtu = 1500;
    conn->connect_time = ktime_get_sec();
    
    printk("[VPN] Connected: %s\n", config->name);
    return 0;
}

int vpn_disconnect(u32 connection_id)
{
    for (u32 i = 0; i < g_net.connection_count; i++) {
        if (g_net.connections[i].connection_id == connection_id) {
            g_net.connections[i].state = NET_CONN_STATE_DISCONNECTED;
            printk("[VPN] Disconnected connection %d\n", connection_id);
            return 0;
        }
    }
    return -ENOENT;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int net_list_interfaces(net_interface_t *ifaces, u32 *count)
{
    if (!ifaces || !count) return -EINVAL;
    
    u32 copy = (*count < g_net.interface_count) ? *count : g_net.interface_count;
    memcpy(ifaces, g_net.interfaces, sizeof(net_interface_t) * copy);
    *count = copy;
    
    return 0;
}

int net_get_stats(u32 interface_id, u64 *rx_bytes, u64 *tx_bytes,
                  u64 *rx_packets, u64 *tx_packets)
{
    net_interface_t *iface = net_get_interface(interface_id);
    if (!iface) return -ENOENT;
    
    if (rx_bytes) *rx_bytes = iface->rx_bytes;
    if (tx_bytes) *tx_bytes = iface->tx_bytes;
    if (rx_packets) *rx_packets = iface->rx_packets;
    if (tx_packets) *tx_packets = iface->tx_packets;
    
    return 0;
}

const char *net_get_type_name(u32 type)
{
    switch (type) {
        case NET_IFACE_ETHERNET:  return "Ethernet";
        case NET_IFACE_WIFI:      return "WiFi";
        case NET_IFACE_LOOPBACK:  return "Loopback";
        case NET_IFACE_BRIDGE:    return "Bridge";
        case NET_IFACE_VLAN:      return "VLAN";
        case NET_IFACE_TUNNEL:    return "Tunnel";
        case NET_IFACE_VPN:       return "VPN";
        default:                  return "Unknown";
    }
}

const char *net_get_state_name(u32 state)
{
    switch (state) {
        case NET_CONN_STATE_DISCONNECTED:  return "Disconnected";
        case NET_CONN_STATE_CONNECTING:    return "Connecting";
        case NET_CONN_STATE_CONNECTED:     return "Connected";
        case NET_CONN_STATE_DISCONNECTING: return "Disconnecting";
        case NET_CONN_STATE_ERROR:         return "Error";
        default:                           return "Unknown";
    }
}

net_manager_t *net_get_manager(void)
{
    return &g_net;
}
