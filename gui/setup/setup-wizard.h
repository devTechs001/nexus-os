/*
 * NEXUS OS - Graphical Setup Wizard Header
 * gui/setup/setup-wizard.h
 *
 * Complete graphical setup wizard for NEXUS OS installation
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _SETUP_WIZARD_H
#define _SETUP_WIZARD_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"
#include "../../fs/vfs/vfs.h"

/*===========================================================================*/
/*                         SETUP WIZARD CONFIGURATION                        */
/*===========================================================================*/

#define SETUP_WIZARD_VERSION        "1.0.0"
#define SETUP_MAX_PAGES             15
#define SETUP_MAX_DISKS             16
#define SETUP_MAX_PARTITIONS        128
#define SETUP_MAX_NETWORK_INTERFACES 16
#define SETUP_MAX_WIFI_NETWORKS     64
#define SETUP_USERNAME_MAX          64
#define SETUP_HOSTNAME_MAX          64
#define SETUP_PASSWORD_MAX          128
#define SETUP_TIMEZONE_MAX          64
#define SETUP_KEYMAP_MAX            16
#define SETUP_LOG_BUFFER_SIZE       131072

/*===========================================================================*/
/*                         DISK AND PARTITION TYPES                          */
/*===========================================================================*/

/* Disk types */
#define DISK_TYPE_UNKNOWN         0
#define DISK_TYPE_IDE             1
#define DISK_TYPE_SATA            2
#define DISK_TYPE_SCSI            3
#define DISK_TYPE_NVME            4
#define DISK_TYPE_VIRTIO          5
#define DISK_TYPE_VMWARE          6
#define DISK_TYPE_VBOX            7

/* Partition table types */
#define PARTITION_TABLE_MBR       0
#define PARTITION_TABLE_GPT       1

/* Filesystem types */
#define FS_UNKNOWN                0
#define FS_NEXUS_FS               1
#define FS_EXT4                   2
#define FS_FAT32                  3
#define FS_NTFS                   4
#define FS_EXFAT                  5
#define FS_BTRFS                  6
#define FS_XFS                    7
#define FS_SWAP                   8

/* Partition flags */
#define PART_FLAG_BOOT            0x01
#define PART_FLAG_ESP             0x02
#define PART_FLAG_HOME            0x04
#define PART_FLAG_SWAP            0x08
#define PART_FLAG_ENCRYPTED       0x10
#define PART_FLAG_ROOT            0x20

/*===========================================================================*/
/*                         NETWORK CONFIGURATION                             */
/*===========================================================================*/

/* Network interface types */
#define NET_TYPE_ETHERNET         0
#define NET_TYPE_WIFI             1
#define NET_TYPE_MODEM            2
#define NET_TYPE_LOOPBACK         3

/* WiFi security types */
#define WIFI_SECURITY_NONE        0
#define WIFI_SECURITY_WEP         1
#define WIFI_SECURITY_WPA         2
#define WIFI_SECURITY_WPA2        3
#define WIFI_SECURITY_WPA3        4
#define WIFI_SECURITY_WPA2_ENT    5

/* IP configuration */
#define IP_CONFIG_DHCP            0
#define IP_CONFIG_STATIC          1
#define IP_CONFIG_AUTO            2

/*===========================================================================*/
/*                         INSTALLATION MODES                                */
/*===========================================================================*/

#define INSTALL_MODE_NEW          0
#define INSTALL_MODE_UPGRADE      1
#define INSTALL_MODE_CUSTOM       2
#define INSTALL_MODE_DUAL_BOOT    3
#define INSTALL_MODE_ERASE_DISK   4
#define INSTALL_MODE_ENCRYPTED    5

/*===========================================================================*/
/*                         INSTALLATION STAGES                               */
/*===========================================================================*/

#define INSTALL_STAGE_NONE        0
#define INSTALL_STAGE_PREPARE     1
#define INSTALL_STAGE_PARTITION   2
#define INSTALL_STAGE_FORMAT      3
#define INSTALL_STAGE_COPY        4
#define INSTALL_STAGE_BOOTLOADER  5
#define INSTALL_STAGE_CONFIGURE   6
#define INSTALL_STAGE_FINALIZE    7
#define INSTALL_STAGE_COMPLETE    8

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Disk Information
 */
typedef struct disk_info {
    u32 disk_id;
    char device_name[64];       /* e.g., /dev/sda, /dev/nvme0n1 */
    char model[128];            /* Disk model */
    char serial[64];            /* Serial number */
    char vendor[64];            /* Vendor name */
    u32 type;                   /* Disk type */
    u64 size_bytes;             /* Total size */
    u64 size_sectors;           /* Total sectors */
    u32 sector_size;            /* Sector size */
    u32 cylinders;
    u32 heads;
    u32 sectors_per_track;
    u32 partition_table;        /* MBR or GPT */
    bool is_removable;
    bool is_ssd;
    bool has_os;
    char existing_os[64];
    bool is_selected;
    s32 error_code;
} disk_info_t;

/**
 * Partition Information
 */
typedef struct partition_info {
    u32 partition_id;
    u32 disk_id;
    u32 partition_number;
    char device_name[64];       /* e.g., /dev/sda1 */
    char part_label[64];        /* Partition label */
    u64 start_sector;
    u64 end_sector;
    u64 size_bytes;
    u32 filesystem;
    char mount_point[128];
    u32 flags;
    bool is_primary;
    bool is_logical;
    bool is_active;
    bool is_encrypted;
    bool is_formatted;
    char uuid[64];
    char fs_uuid[64];
    bool is_selected;
    bool is_new;
    s32 error_code;
} partition_info_t;

/**
 * WiFi Network Information
 */
typedef struct wifi_network {
    char ssid[64];
    u32 security;
    s32 signal_strength;
    u32 channel;
    char bssid[32];
    bool is_saved;
} wifi_network_t;

/**
 * Network Interface Information
 */
typedef struct network_interface {
    u32 iface_id;
    char name[32];              /* e.g., eth0, wlan0 */
    u32 type;
    char mac_address[32];
    bool is_connected;
    bool is_wireless;
    bool is_active;
    
    /* IP Configuration */
    u32 ip_config;
    u32 ip_address;
    u32 netmask;
    u32 gateway;
    u32 dns_primary;
    u32 dns_secondary;
    u32 dns_tertiary;
    
    /* WiFi specific */
    char ssid[64];
    u32 wifi_security;
    s32 signal_strength;
    char wifi_password[128];
    wifi_network_t available_networks[SETUP_MAX_WIFI_NETWORKS];
    u32 network_count;
    
    /* Connection status */
    bool has_internet;
    u32 link_speed;
    char status[64];
} network_interface_t;

/**
 * User Account Information
 */
typedef struct user_account {
    char username[SETUP_USERNAME_MAX];
    char password[SETUP_PASSWORD_MAX];
    char password_hint[128];
    char full_name[128];
    u32 user_id;
    u32 group_id;
    bool is_admin;
    bool auto_login;
    bool require_password;
    char icon_path[256];
    char home_dir[256];
    char shell[64];
} user_account_t;

/**
 * System Configuration
 */
typedef struct system_config {
    char hostname[SETUP_HOSTNAME_MAX];
    char timezone[SETUP_TIMEZONE_MAX];
    char keymap[SETUP_KEYMAP_MAX];
    char language[32];
    char locale[32];
    char keyboard_variant[32];
    u32 keyboard_model;
    bool enable_ssh;
    bool enable_remote_desktop;
    bool enable_encryption;
    bool enable_secure_boot;
    bool enable_auto_updates;
    bool enable_telemetry;
    char ntp_server[64];
} system_config_t;

/**
 * Installation Options
 */
typedef struct installation_options {
    u32 install_mode;
    u32 target_disk;
    u32 target_partition;
    bool format_target;
    bool install_grub;
    bool enable_trim;
    bool enable_compression;
    bool enable_lvm;
    bool enable_swap;
    u32 swap_size_mb;
    u32 selected_packages;
    char encryption_password[SETUP_PASSWORD_MAX];
    u32 encryption_algorithm;
    u32 luks_version;
    char grub_device[64];
    bool create_home_partition;
    u32 root_size_mb;
    u32 home_size_mb;
    u32 efi_size_mb;
} installation_options_t;

/**
 * Partition Plan Entry
 */
typedef struct partition_plan {
    char mount_point[128];
    u32 filesystem;
    u64 size_bytes;
    u32 flags;
    bool format;
    char label[64];
} partition_plan_t;

/**
 * Setup Wizard Page
 */
typedef struct setup_page {
    u32 page_id;
    char title[128];
    char description[512];
    char icon[64];
    widget_t *page_widget;
    widget_t *content_widget;
    bool (*on_enter)(struct setup_page *);
    bool (*on_leave)(struct setup_page *);
    bool (*on_next)(struct setup_page *);
    bool (*on_back)(struct setup_page *);
    bool (*validate)(struct setup_page *);
    void (*on_resize)(struct setup_page *, s32, s32);
    void *page_data;
    bool is_complete;
    bool is_valid;
} setup_page_t;

/**
 * Language Information
 */
typedef struct language_info {
    char code[16];
    char name[64];
    char native_name[64];
    char locale[32];
    char keymap[32];
    char timezone[64];
} language_info_t;

/**
 * Timezone Information
 */
typedef struct timezone_info {
    char name[64];
    char region[64];
    char city[64];
    s32 utc_offset;
    bool dst;
    char dst_name[16];
} timezone_info_t;

/**
 * Keyboard Layout Information
 */
typedef struct keyboard_layout {
    char code[16];
    char name[64];
    char variant[32];
} keyboard_layout_t;

/**
 * Setup Wizard State
 */
typedef struct setup_wizard {
    bool initialized;
    bool running;
    bool completed;
    bool cancelled;
    bool has_error;
    
    /* Window references */
    window_t *main_window;
    widget_t *main_widget;
    widget_t *sidebar;
    widget_t *content_area;
    widget_t *header_label;
    widget_t *description_label;
    widget_t *progress_bar;
    widget_t *progress_label;
    widget_t *back_button;
    widget_t *next_button;
    widget_t *cancel_button;
    widget_t *help_button;
    widget_t *status_bar;
    
    /* State */
    u32 current_page;
    u32 total_pages;
    setup_page_t pages[SETUP_MAX_PAGES];
    
    /* Progress */
    u32 installation_progress;
    u32 current_stage;
    char status_message[256];
    char error_message[512];
    u64 start_time;
    u64 stage_start_time;
    u64 elapsed_time;
    u64 estimated_remaining;
    u32 current_file;
    u32 total_files;
    u64 bytes_copied;
    u64 total_bytes;
    
    /* Configuration data */
    disk_info_t disks[SETUP_MAX_DISKS];
    u32 disk_count;
    
    partition_info_t partitions[SETUP_MAX_PARTITIONS];
    u32 partition_count;
    
    partition_plan_t partition_plan[16];
    u32 plan_count;
    
    network_interface_t networks[SETUP_MAX_NETWORK_INTERFACES];
    u32 network_count;
    
    language_info_t languages[64];
    u32 language_count;
    
    timezone_info_t timezones[256];
    u32 timezone_count;
    
    keyboard_layout_t keyboards[128];
    u32 keyboard_count;
    
    user_account_t user;
    system_config_t system;
    installation_options_t options;
    
    /* Selected indices */
    u32 selected_language;
    u32 selected_keyboard;
    u32 selected_timezone;
    u32 selected_disk;
    u32 selected_network;
    u32 selected_wifi;
    
    /* Installation log */
    char log_buffer[SETUP_LOG_BUFFER_SIZE];
    u32 log_offset;
    u32 log_lines;
    
    /* Error handling */
    s32 last_error;
    s32 error_code;
    
    /* Temporary storage */
    char temp_buffer[4096];
    void *temp_data;
    u32 temp_data_size;
    
    /* Threading */
    bool installation_thread_running;
    bool installation_thread_done;
    s32 installation_thread_result;
    
    /* VFS handles */
    void *target_mount;
    void *source_mount;
    int target_fd;
    
} setup_wizard_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Wizard initialization and lifecycle */
int setup_wizard_init(setup_wizard_t *wizard);
int setup_wizard_run(setup_wizard_t *wizard);
int setup_wizard_shutdown(setup_wizard_t *wizard);
bool setup_wizard_is_running(setup_wizard_t *wizard);
int setup_wizard_cancel(setup_wizard_t *wizard);
int setup_wizard_set_error(setup_wizard_t *wizard, s32 error, const char *msg);

/* Page navigation */
int setup_wizard_goto_page(setup_wizard_t *wizard, u32 page_id);
int setup_wizard_next_page(setup_wizard_t *wizard);
int setup_wizard_previous_page(setup_wizard_t *wizard);
setup_page_t *setup_wizard_get_current_page(setup_wizard_t *wizard);
int setup_wizard_add_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_wizard_insert_page(setup_wizard_t *wizard, setup_page_t *page, u32 index);

/* UI creation */
int setup_create_ui(setup_wizard_t *wizard);
int setup_create_sidebar(setup_wizard_t *wizard);
int setup_create_content_area(setup_wizard_t *wizard);
int setup_create_buttons(setup_wizard_t *wizard);
int setup_create_status_bar(setup_wizard_t *wizard);
int setup_update_ui(setup_wizard_t *wizard);

/* Page content creation */
int setup_create_welcome_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_language_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_keyboard_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_license_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_install_type_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_disk_select_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_partition_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_network_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_user_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_timezone_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_confirm_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_install_page(setup_wizard_t *wizard, setup_page_t *page);
int setup_create_complete_page(setup_wizard_t *wizard, setup_page_t *page);

/* Disk operations */
int setup_detect_disks(setup_wizard_t *wizard);
int setup_get_disk_info(u32 disk_id, disk_info_t *info);
int setup_list_partitions(u32 disk_id, partition_info_t *partitions, u32 *count);
int setup_create_partition(setup_wizard_t *wizard, disk_info_t *disk, partition_info_t *partition);
int setup_delete_partition(setup_wizard_t *wizard, partition_info_t *partition);
int setup_format_partition(setup_wizard_t *wizard, partition_info_t *partition, u32 filesystem, const char *label);
int setup_wipe_disk(setup_wizard_t *wizard, disk_info_t *disk);
int setup_create_partition_table(setup_wizard_t *wizard, disk_info_t *disk, u32 table_type);
int setup_commit_partitions(setup_wizard_t *wizard, disk_info_t *disk);

/* Partition planning */
int setup_create_default_partitions(setup_wizard_t *wizard, disk_info_t *disk);
int setup_create_efi_partition(setup_wizard_t *wizard, u64 size_mb);
int setup_create_root_partition(setup_wizard_t *wizard, u64 size_mb);
int setup_create_home_partition(setup_wizard_t *wizard, u64 size_mb);
int setup_create_swap_partition(setup_wizard_t *wizard, u64 size_mb);
int setup_calculate_partition_sizes(setup_wizard_t *wizard, disk_info_t *disk);

/* Network operations */
int setup_detect_network(setup_wizard_t *wizard);
int setup_scan_wifi(setup_wizard_t *wizard, network_interface_t *iface);
int setup_connect_wifi(setup_wizard_t *wizard, network_interface_t *iface, const char *ssid, const char *password, u32 security);
int setup_configure_ip(setup_wizard_t *wizard, network_interface_t *iface, u32 config, u32 ip, u32 mask, u32 gateway, u32 dns1, u32 dns2);
int setup_test_connection(setup_wizard_t *wizard);
bool setup_has_internet(setup_wizard_t *wizard);
int setup_wait_for_network(setup_wizard_t *wizard, u32 timeout_ms);

/* User account operations */
int setup_validate_username(setup_wizard_t *wizard, const char *username);
int setup_validate_password(setup_wizard_t *wizard, const char *password);
int setup_create_user(setup_wizard_t *wizard, user_account_t *user);
int setup_set_hostname(setup_wizard_t *wizard, const char *hostname);
int setup_generate_hostname(setup_wizard_t *wizard);

/* System configuration */
int setup_set_timezone(setup_wizard_t *wizard, const char *timezone);
int setup_set_keyboard(setup_wizard_t *wizard, const char *keymap);
int setup_set_language(setup_wizard_t *wizard, const char *language);
int setup_set_locale(setup_wizard_t *wizard, const char *locale);

/* Installation operations */
int setup_prepare_installation(setup_wizard_t *wizard);
int setup_start_installation(setup_wizard_t *wizard);
int setup_copy_files(setup_wizard_t *wizard);
int setup_install_bootloader(setup_wizard_t *wizard);
int setup_configure_system(setup_wizard_t *wizard);
int setup_finalize_installation(setup_wizard_t *wizard);
int setup_install_grub(setup_wizard_t *wizard, const char *device);
int setup_create_fstab(setup_wizard_t *wizard);
int setup_initramfs(setup_wizard_t *wizard);

/* File operations */
int setup_mount_source(setup_wizard_t *wizard);
int setup_mount_target(setup_wizard_t *wizard);
int setup_unmount_source(setup_wizard_t *wizard);
int setup_unmount_target(setup_wizard_t *wizard);
int setup_copy_file(const char *src, const char *dst);
int setup_copy_directory(const char *src, const char *dst);
int setup_extract_archive(const char *archive, const char *dst);

/* Utility functions */
const char *setup_get_filesystem_name(u32 fs_type);
const char *setup_get_disk_type_name(u32 disk_type);
const char *setup_get_network_type_name(u32 net_type);
const char *setup_get_security_name(u32 security);
u64 setup_format_size(u64 size_bytes, char *buffer, u32 buffer_size);
bool setup_validate_hostname(const char *hostname);
bool setup_validate_ip(u32 ip);
u32 setup_string_to_ip(const char *str);
void setup_ip_to_string(u32 ip, char *buffer, u32 buffer_size);
char *setup_trim_string(char *str);
int setup_string_to_lower(char *str);
u32 setup_calculate_checksum(const void *data, u32 size);

/* Logging */
int setup_log(setup_wizard_t *wizard, const char *fmt, ...);
int setup_log_error(setup_wizard_t *wizard, s32 error, const char *fmt, ...);
int setup_log_info(setup_wizard_t *wizard, const char *fmt, ...);
int setup_log_warning(setup_wizard_t *wizard, const char *fmt, ...);
const char *setup_get_log(setup_wizard_t *wizard);
int setup_clear_log(setup_wizard_t *wizard);
int setup_save_log(setup_wizard_t *wizard, const char *path);

/* Data initialization */
int setup_init_languages(setup_wizard_t *wizard);
int setup_init_timezones(setup_wizard_t *wizard);
int setup_init_keyboards(setup_wizard_t *wizard);

/* Global wizard instance */
setup_wizard_t *setup_wizard_get_instance(void);

/* Inline helpers */
static inline bool setup_is_valid_disk(disk_info_t *disk) {
    return disk && disk->size_bytes > 0;
}

static inline bool setup_is_valid_partition(partition_info_t *part) {
    return part && part->size_bytes > 0;
}

static inline u64 setup_sectors_to_bytes(u64 sectors, u32 sector_size) {
    return sectors * sector_size;
}

static inline u64 setup_bytes_to_sectors(u64 bytes, u32 sector_size) {
    return (bytes + sector_size - 1) / sector_size;
}

#endif /* _SETUP_WIZARD_H */
