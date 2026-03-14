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
/*                         GUI API                                           */
/*===========================================================================*/

/* Initialization */
extern int virt_gui_init(void);
extern void virt_manager_gui_render(void);
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
