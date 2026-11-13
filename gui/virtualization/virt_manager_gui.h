/*
 * NEXUS OS - Virtualization Manager GUI Header
 * gui/virtualization/virt_manager_gui.h
 *
 * Enterprise-level VM management interface API
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef VIRT_MANAGER_GUI_H
#define VIRT_MANAGER_GUI_H

#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                     CONFIGURATION TYPES                                   */
/*===========================================================================*/

typedef struct {
    bool host_only;
    bool bridged;
    bool nat;
    bool nat_network;
    bool internal;
    char bridge_interface[32];
    char nat_network_name[64];
    char internal_name[64];
    bool dhcp_enabled;
    char static_ip[32];
    char static_gateway[32];
    char static_netmask[32];
    bool port_forwarding;
    u32 port_forward_count;
} network_config_t;

typedef struct {
    char name[128];
    char host_path[256];
    char guest_path[256];
    bool writable;
    bool auto_mount;
    bool enabled;
} shared_folder_t;

typedef struct {
    bool auto_optimize;
    u32 cpu_overcommit_ratio;
    u32 memory_overcommit_ratio;
    bool dynamic_memory;
    u64 min_memory_mb;
    u64 max_memory_mb;
    bool balloon_driver;
    bool memory_sharing;
    bool transparent_hugepages;
    bool numa_balancing;
    bool cpu_hotplug;
    bool memory_hotplug;
} resource_optimization_t;

typedef struct {
    bool vmware_tools;
    bool drag_drop;
    bool copy_paste;
    bool shared_clipboard;
    bool unity_mode;
    bool auto_fit_guest;
    bool auto_fit_host;
    u32 display_scale;
    bool accelerate_3d;
    u32 video_memory_mb;
    bool monitor_multiple;
} vmware_settings_t;

/*===========================================================================*/
/*                         GUI API                                           */
/*===========================================================================*/

/* Initialization */
extern int virt_gui_init(void);
extern void virt_manager_gui_render(void);

/* Pointer input. The action buttons are registered by virt_manager_add_action()
 * but nothing dispatched events to them; this drains the input queue, tracks
 * the cursor, and activates the button under it. Returns 0 if an event was
 * consumed. */
extern int virt_manager_poll_input(void);
extern int virt_manager_button_at(u32 x, u32 y);
extern void virt_manager_handle_button_hover(u32 x, u32 y);
extern int virt_manager_handle_button_press(u32 x, u32 y);
extern void virt_manager_update_status(void);
extern void virt_manager_refresh_vm_list(void);

/* VM Management */
extern int virt_manager_create_vm(const char *name, const char *template);
extern int virt_manager_start_vm(u32 vm_id);
extern int virt_manager_stop_vm(u32 vm_id);
extern int virt_manager_pause_vm(u32 vm_id);
extern int virt_manager_resume_vm(u32 vm_id);
extern int virt_manager_create_snapshot(u32 vm_id, const char *name);
extern int virt_manager_restore_snapshot(u32 vm_id, u32 snapshot_id);
extern int virt_manager_clone_vm(u32 vm_id, const char *new_name);
extern int virt_manager_delete_vm(u32 vm_id);

/* Network Configuration */
extern int virt_manager_configure_network(u32 vm_id, network_config_t *config);
extern int virt_manager_add_port_forward(u32 vm_id, u32 host_port, u32 guest_port, const char *protocol);
extern int virt_manager_setup_host_only(const char *interface, const char *subnet);
extern int virt_manager_setup_nat_network(const char *network_name, const char *subnet);
extern int virt_manager_setup_bridged(const char *host_interface);

/* Shared Folders */
extern int virt_manager_add_shared_folder(u32 vm_id, shared_folder_t *folder);
extern int virt_manager_remove_shared_folder(u32 vm_id, u32 folder_id);

/* Resource Optimization */
extern int virt_manager_enable_auto_opt(resource_optimization_t *opt);
extern int virt_manager_configure_vmware(vmware_settings_t *settings);
extern int virt_manager_enable_resource_sharing(bool auto_optimize);

/* Container Management */
extern int virt_manager_create_container(const char *name, const char *image);
extern int virt_manager_start_container(u32 container_id);
extern int virt_manager_stop_container(u32 container_id);
extern int virt_manager_deploy_container(u32 template_id, const char *name);
extern int virt_manager_scale_containers(u32 service_id, u32 replica_count);

/* Event Handling */
extern void virt_manager_handle_button_click(u32 button_id);
extern void virt_manager_handle_vm_select(u32 vm_id);
extern void virt_manager_handle_search(const char *query);
extern void virt_manager_handle_category_change(u32 category_id);

/* Monitoring */
extern void virt_manager_update_graphs(u32 vm_id);

#endif /* VIRT_MANAGER_GUI_H */
