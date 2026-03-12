/*
 * NEXUS OS - Network Driver
 * drivers/network/network.h
 *
 * Network driver with Ethernet, WiFi, Bluetooth support
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NETWORK_H
#define _NETWORK_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         NETWORK CONFIGURATION                             */
/*===========================================================================*/

#define NET_MAX_INTERFACES          16
#define NET_MAX_DRIVERS             32
#define NET_MAX_ROUTES              256
#define NET_MAX_DNS                 8
#define NET_MAX_SSID_LEN            32
#define NET_MAX_KEY_LEN             64
#define NET_MAC_LEN                 6
#define NET_IP_LEN                  4
#define NET_IPV6_LEN                16

/*===========================================================================*/
/*                         NETWORK INTERFACE TYPES                           */
/*===========================================================================*/

#define NET_IF_ETHERNET             0
#define NET_IF_WIFI                 1
#define NET_IF_BLUETOOTH            2
#define NET_IF_LOOPBACK             3
#define NET_IF_TUNNEL               4
#define NET_IF_BRIDGE               5

/*===========================================================================*/
/*                         NETWORK STATUS                                    */
/*===========================================================================*/

#define NET_STATUS_DOWN             0
#define NET_STATUS_UP               1
#define NET_STATUS_CONNECTING       2
#define NET_STATUS_CONNECTED        3
#define NET_STATUS_DISCONNECTED     4
#define NET_STATUS_ERROR            5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * MAC Address
 */
typedef struct {
    u8 addr[NET_MAC_LEN];
} mac_addr_t;

/**
 * IPv4 Address
 */
typedef struct {
    u8 addr[NET_IP_LEN];
} ipv4_addr_t;

/**
 * IPv6 Address
 */
typedef struct {
    u8 addr[NET_IPV6_LEN];
} ipv6_addr_t;

/**
 * Network Interface
 */
typedef struct {
    u32 iface_id;                   /* Interface ID */
    char name[32];                  /* Interface Name */
    u32 type;                       /* Interface Type */
    u32 status;                     /* Status */
    mac_addr_t mac;                 /* MAC Address */
    ipv4_addr_t ip;                 /* IPv4 Address */
    ipv4_addr_t netmask;            /* Netmask */
    ipv4_addr_t gateway;            /* Gateway */
    ipv4_addr_t dns_primary;        /* Primary DNS */
    ipv4_addr_t dns_secondary;      /* Secondary DNS */
    ipv6_addr_t ipv6;               /* IPv6 Address */
    u32 mtu;                        /* MTU */
    u32 speed;                      /* Link Speed (Mbps) */
    bool is_up;                     /* Is Up */
    bool is_running;                /* Is Running */
    bool is_dhcp;                   /* Is DHCP */
    u64 rx_bytes;                   /* RX Bytes */
    u64 tx_bytes;                   /* TX Bytes */
    u64 rx_packets;                 /* RX Packets */
    u64 tx_packets;                 /* TX Packets */
    u64 rx_errors;                  /* RX Errors */
    u64 tx_errors;                  /* TX Errors */
    void *driver_data;              /* Driver Data */
} net_interface_t;

/**
 * WiFi Network
 */
typedef struct {
    char ssid[NET_MAX_SSID_LEN];    /* SSID */
    mac_addr_t bssid;               /* BSSID */
    u32 channel;                    /* Channel */
    u32 frequency;                  /* Frequency (MHz) */
    s32 signal_strength;            /* Signal Strength (dBm) */
    u32 security;                   /* Security Type */
    bool is_open;                   /* Is Open */
    bool is_connected;              /* Is Connected */
} wifi_network_t;

/**
 * WiFi Configuration
 */
typedef struct {
    char ssid[NET_MAX_SSID_LEN];    /* SSID */
    char password[NET_MAX_KEY_LEN]; /* Password */
    u32 security;                   /* Security Type */
    bool auto_connect;              /* Auto Connect */
    bool hidden;                    /* Hidden Network */
} wifi_config_t;

/**
 * Network Driver
 */
typedef struct {
    char name[64];                  /* Driver Name */
    u32 type;                       /* Driver Type */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id;                  /* Device ID */
    bool (*probe)(net_interface_t *);
    bool (*remove)(net_interface_t *);
    bool (*open)(net_interface_t *);
    bool (*close)(net_interface_t *);
    bool (*send)(net_interface_t *, const void *, u32);
    bool (*recv)(net_interface_t *, void *, u32);
    bool (*set_mac)(net_interface_t *, mac_addr_t *);
    bool (*get_stats)(net_interface_t *, u64 *, u64 *);
    void *driver_data;              /* Driver Data */
} net_driver_t;

/**
 * Network Route
 */
typedef struct {
    u32 route_id;                   /* Route ID */
    ipv4_addr_t destination;        /* Destination */
    ipv4_addr_t netmask;            /* Netmask */
    ipv4_addr_t gateway;            /* Gateway */
    u32 iface_id;                   /* Interface ID */
    u32 metric;                     /* Metric */
    bool is_default;                /* Is Default Route */
} net_route_t;

/**
 * Network Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    net_interface_t interfaces[NET_MAX_INTERFACES]; /* Interfaces */
    u32 interface_count;            /* Interface Count */
    net_driver_t drivers[NET_MAX_DRIVERS]; /* Drivers */
    u32 driver_count;               /* Driver Count */
    net_route_t routes[NET_MAX_ROUTES]; /* Routes */
    u32 route_count;                /* Route Count */
    ipv4_addr_t dns_servers[NET_MAX_DNS]; /* DNS Servers */
    u32 dns_count;                  /* DNS Count */
    wifi_network_t wifi_networks[64]; /* WiFi Networks */
    u32 wifi_network_count;         /* WiFi Network Count */
    wifi_config_t wifi_config;      /* WiFi Configuration */
    u32 active_wifi;                /* Active WiFi */
} net_driver_state_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int net_init(void);
int net_shutdown(void);
bool net_is_initialized(void);

/* Interface management */
int net_enumerate_interfaces(void);
net_interface_t *net_get_interface(u32 iface_id);
net_interface_t *net_get_interface_by_name(const char *name);
int net_get_interface_count(void);
int net_list_interfaces(net_interface_t *ifaces, u32 *count);

/* Interface control */
int net_interface_up(net_interface_t *iface);
int net_interface_down(net_interface_t *iface);
int net_interface_set_mac(net_interface_t *iface, mac_addr_t *mac);
int net_interface_get_mac(net_interface_t *iface, mac_addr_t *mac);
int net_interface_set_ip(net_interface_t *iface, ipv4_addr_t *ip, ipv4_addr_t *netmask);
int net_interface_set_gateway(net_interface_t *iface, ipv4_addr_t *gateway);
int net_interface_set_dns(net_interface_t *iface, ipv4_addr_t *primary, ipv4_addr_t *secondary);
int net_interface_dhcp(net_interface_t *iface, bool enable);

/* WiFi operations */
int net_wifi_scan(net_interface_t *iface, wifi_network_t *networks, u32 *count);
int net_wifi_connect(net_interface_t *iface, wifi_config_t *config);
int net_wifi_disconnect(net_interface_t *iface);
int net_wifi_get_status(net_interface_t *iface, wifi_network_t *network);
int net_wifi_save_config(wifi_config_t *config);
int net_wifi_load_config(const char *ssid, wifi_config_t *config);
int net_wifi_delete_config(const char *ssid);

/* Routing */
int net_add_route(net_route_t *route);
int net_remove_route(u32 route_id);
int net_get_default_route(net_route_t *route);
int net_list_routes(net_route_t *routes, u32 *count);

/* DNS */
int net_set_dns(ipv4_addr_t *servers, u32 count);
int net_get_dns(ipv4_addr_t *servers, u32 *count);
int net_add_dns(ipv4_addr_t *server);
int net_clear_dns(void);

/* Statistics */
int net_get_stats(net_interface_t *iface, u64 *rx_bytes, u64 *tx_bytes);
int net_reset_stats(net_interface_t *iface);

/* Packet I/O */
int net_send(net_interface_t *iface, const void *data, u32 length);
int net_recv(net_interface_t *iface, void *buffer, u32 length);

/* Driver registration */
int net_register_driver(net_driver_t *driver);
int net_unregister_driver(const char *name);
net_driver_t *net_get_driver(const char *name);

/* Utility functions */
const char *net_get_type_name(u32 type);
const char *net_get_status_name(u32 status);
const char *net_get_security_name(u32 security);
int net_mac_to_string(mac_addr_t *mac, char *str, u32 size);
int net_string_to_mac(const char *str, mac_addr_t *mac);
int net_ip_to_string(ipv4_addr_t *ip, char *str, u32 size);
int net_string_to_ip(const char *str, ipv4_addr_t *ip);

/* Global instance */
net_driver_state_t *net_get_driver_state(void);

#endif /* _NETWORK_H */
