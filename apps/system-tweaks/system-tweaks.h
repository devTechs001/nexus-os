/*
 * NEXUS OS - System Tweaks
 * apps/system-tweaks/system-tweaks.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * System tweaking and fine-tuning:
 * - Kernel parameters
 * - Network tuning
 * - Power management
 * - Performance profiles
 * - UI tweaks
 * - Security hardening
 * - Gaming mode
 * - Development mode
 */

#ifndef _NEXUS_SYSTEM_TWEAKS_H
#define _NEXUS_SYSTEM_TWEAKS_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         TWEAK PROFILES                                    */
/*===========================================================================*/

#define TWEAK_PROFILE_DEFAULT       0
#define TWEAK_PROFILE_PERFORMANCE   1
#define TWEAK_PROFILE_POWERSAVE     2
#define TWEAK_PROFILE_GAMING        3
#define TWEAK_PROFILE_SERVER        4
#define TWEAK_PROFILE_DESKTOP       5
#define TWEAK_PROFILE_DEVELOPMENT   6
#define TWEAK_PROFILE_MEDIA         7
#define TWEAK_PROFILE_VIRTUALIZATION 8

/*===========================================================================*/
/*                         TWEAK CATEGORIES                                  */
/*===========================================================================*/

/* Kernel Tweaks */
#define TWEAK_KERNEL_SWAPPINESS         0x0001
#define TWEAK_KERNEL_DIRTY_RATIO        0x0002
#define TWEAK_KERNEL_DIRTY_BG_RATIO     0x0004
#define TWEAK_KERNEL_VM_OVERCOMMIT      0x0008
#define TWEAK_KERNEL_READ_AHEAD         0x0010
#define TWEAK_KERNEL_MAX_MAP_COUNT      0x0020
#define TWEAK_KERNEL_PID_MAX            0x0040
#define TWEAK_KERNEL_CORE_PATTERN       0x0080

/* Network Tweaks */
#define TWEAK_NET_TCP_MEM               0x0100
#define TWEAK_NET_TCP_RMEM              0x0200
#define TWEAK_NET_TCP_WMEM              0x0400
#define TWEAK_NET_TCP_CONGESTION        0x0800
#define TWEAK_NET_TCP_MAX_SYN_BACKLOG   0x1000
#define TWEAK_NET_TCP_SYNCOOKIES        0x2000
#define TWEAK_NET_IP_LOCAL_PORT_RANGE   0x4000
#define TWEAK_NET_CORE_NETDEV_MAX_BACKLOG 0x8000

/* Power Tweaks */
#define TWEAK_POWER_CPU_GOVERNOR        0x00010000
#define TWEAK_POWER_CPU_FREQ_MIN        0x00020000
#define TWEAK_POWER_CPU_FREQ_MAX        0x00040000
#define TWEAK_POWER_PCIE_ASPM           0x00080000
#define TWEAK_POWER_USB_AUTOSUSPEND     0x00100000
#define TWEAK_POWER_WIFI_POWER          0x00200000
#define TWEAK_POWER_HDD_SPINDOWN        0x00400000
#define TWEAK_POWER_SUSPEND_MODE        0x00800000

/* UI Tweaks */
#define TWEAK_UI_ANIMATIONS             0x01000000
#define TWEAK_UI_COMPOSITOR             0x02000000
#define TWEAK_UI_VSYNC                  0x04000000
#define TWEAK_UI_DPI_SCALE              0x08000000
#define TWEAK_UI_FONT_HINTING           0x10000000
#define TWEAK_UI_CURSOR_THEME           0x20000000
#define TWEAK_UI_ICON_THEME             0x40000000
#define TWEAK_UI_GTK_THEME              0x80000000

/* Security Tweaks */
#define TWEAK_SECURITY_FIREWALL         0x00000001
#define TWEAK_SECURITY_SELINUX          0x00000002
#define TWEAK_SECURITY_ASLR             0x00000004
#define TWEAK_SECURITY_SPECTRE          0x00000008
#define TWEAK_SECURITY_MELTDOWN         0x00000010
#define TWEAK_SECURITY_COREDUMP         0x00000020
#define TWEAK_SECURITY_PTRACE           0x00000040
#define TWEAK_SECURITY_UNPRIV_BPF       0x00000080

/*===========================================================================*/
/*                         TWEAK STRUCTURES                                  */
/*===========================================================================*/

/**
 * tweak_value_t - Tweaks value
 */
typedef struct {
    char name[128];
    char description[256];
    char category[64];
    int type;  /* INT, STRING, BOOL, ENUM */
    
    union {
        int int_value;
        char string_value[256];
        bool bool_value;
        int enum_value;
    } current;
    
    union {
        int int_value;
        char string_value[256];
        bool bool_value;
        int enum_value;
    } default_value;
    
    union {
        int int_value;
        char string_value[256];
        bool bool_value;
        int enum_value;
    } recommended;
    
    int min_value;
    int max_value;
    char **enum_values;
    int enum_count;
    
    bool modified;
    bool requires_reboot;
    char *command;
} tweak_value_t;

/**
 * tweak_profile_t - Tweaks profile
 */
typedef struct {
    char name[64];
    char description[512];
    int id;
    
    /* Tweaks */
    tweak_value_t *tweaks;
    int tweak_count;
    
    /* Categories enabled */
    u32 kernel_tweaks;
    u32 network_tweaks;
    u32 power_tweaks;
    u32 ui_tweaks;
    u32 security_tweaks;
    
    /* Performance impact */
    int performance_score;
    int power_score;
    int security_score;
    int stability_score;
    
    /* Compatibility */
    bool compatible_vmware;
    bool compatible_virtualbox;
    bool compatible_qemu;
    bool compatible_native;
    
    /* Requirements */
    int min_ram_mb;
    int min_cpu_cores;
    char **required_packages;
    int package_count;
} tweak_profile_t;

/**
 * tweak_preset_t - Quick preset
 */
typedef struct {
    char name[64];
    char description[256];
    int category;
    int impact;  /* 0=none, 1=low, 2=medium, 3=high */
    bool enabled;
    void (*apply)(void);
    void (*revert)(void);
} tweak_preset_t;

/*===========================================================================*/
/*                         SYSTEM TWEAKS API                                 */
/*===========================================================================*/

/* Initialization */
int system_tweaks_init(void);
int system_tweaks_shutdown(void);

/* Profile management */
int system_tweaks_load_profile(int profile_id);
int system_tweaks_save_profile(int profile_id);
int system_tweaks_get_profile(int profile_id, tweak_profile_t *profile);
int system_tweaks_list_profiles(void);
int system_tweaks_create_profile(const char *name, const char *description);
int system_tweaks_delete_profile(int profile_id);

/* Apply tweaks */
int system_tweaks_apply(int profile_id);
int system_tweaks_apply_tweak(const char *name, const char *value);
int system_tweaks_apply_all(void);
int system_tweaks_revert_all(void);
int system_tweaks_revert_tweak(const char *name);

/* Kernel tweaks */
int tweak_kernel_set_swappiness(int value);
int tweak_kernel_get_swappiness(void);
int tweak_kernel_set_dirty_ratio(int value);
int tweak_kernel_set_dirty_background_ratio(int value);
int tweak_kernel_set_vm_overcommit(int mode);
int tweak_kernel_set_read_ahead_kb(int value);
int tweak_kernel_set_max_map_count(int value);

/* Network tweaks */
int tweak_network_set_tcp_mem(const char *value);
int tweak_network_set_tcp_rmem(const char *value);
int tweak_network_set_tcp_wmem(const char *value);
int tweak_network_set_tcp_congestion(const char *algorithm);
int tweak_network_enable_tcp_syncookies(bool enable);
int tweak_network_set_port_range(const char *range);
int tweak_network_optimize_for_latency(void);
int tweak_network_optimize_for_throughput(void);

/* Power tweaks */
int tweak_power_set_cpu_governor(const char *governor);
int tweak_power_set_cpu_freq_min(int freq_mhz);
int tweak_power_set_cpu_freq_max(int freq_mhz);
int tweak_power_enable_pcie_aspm(bool enable);
int tweak_power_enable_usb_autosuspend(bool enable);
int tweak_power_set_wifi_power_save(bool enable);
int tweak_power_set_hdd_spindown(int minutes);

/* UI tweaks */
int tweak_ui_set_animations(bool enable);
int tweak_ui_set_compositor(bool enable);
int tweak_ui_set_vsync(bool enable);
int tweak_ui_set_dpi_scale(int scale);
int tweak_ui_set_font_hinting(const char *hinting);
int tweak_ui_set_cursor_theme(const char *theme);
int tweak_ui_set_icon_theme(const char *theme);

/* Security tweaks */
int tweak_security_enable_firewall(bool enable);
int tweak_security_enable_selinux(bool enable);
int tweak_security_enable_aslr(bool enable);
int tweak_security_enable_spectre_mitigation(bool enable);
int tweak_security_enable meltdown_mitigation(bool enable);
int tweak_security_set_ptrace_scope(int scope);

/* Gaming mode */
int tweak_gaming_mode_enable(void);
int tweak_gaming_mode_disable(void);
int tweak_gaming_mode_set_priority(int priority);
int tweak_gaming_mode_disable_compositor(void);
int tweak_gaming_mode_optimize_network(void);

/* Development mode */
int tweak_development_mode_enable(void);
int tweak_development_mode_disable(void);
int tweak_development_mode_install_tools(void);
int tweak_development_mode_optimize_compile(void);

/* Server mode */
int tweak_server_mode_enable(void);
int tweak_server_mode_disable(void);
int tweak_server_mode_optimize_network(void);
int tweak_server_mode_optimize_io(void);

/* Presets */
int tweak_preset_list(tweak_preset_t **presets, int *count);
int tweak_preset_apply(const char *name);
int tweak_preset_revert(const char *name);

/* Analysis */
int system_tweaks_analyze(void);
int system_tweaks_get_recommendations(char ***recommendations, int *count);
int system_tweaks_get_current_profile(void);
int system_tweaks_get_impact_summary(int profile_id, char *summary, int len);

/* Backup and restore */
int system_tweaks_backup(const char *path);
int system_tweaks_restore(const char *path);
int system_tweaks_backup_current_settings(void);

/* Validation */
int system_tweaks_validate(int profile_id);
int system_tweaks_check_compatibility(int profile_id);
int system_tweaks_check_conflicts(int profile_id);

/* Export */
int system_tweaks_export_config(const char *path);
int system_tweaks_import_config(const char *path);
int system_tweaks_generate_script(int profile_id, const char *path);

/* GUI */
int system_tweaks_gui_init(void);
int system_tweaks_gui_show(void);
int system_tweaks_gui_show_profile(int profile_id);
int system_tweaks_gui_show_tweak(const char *name);

#endif /* _NEXUS_SYSTEM_TWEAKS_H */
