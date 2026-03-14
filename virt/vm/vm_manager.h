/*
 * NEXUS OS - Virtual Machine Manager Header
 * virt/vm/vm_manager.h
 *
 * VM management API and interfaces
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef NEXUS_VM_MANAGER_H
#define NEXUS_VM_MANAGER_H

#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         VM MANAGEMENT API                                 */
/*===========================================================================*/

/* Initialization */
extern int vm_manager_init(void);
extern void vm_manager_print_stats(void);

/* Resource Management */
extern int vm_set_resource_limits(u32 vm_id, vm_resource_limits_t *limits);
extern int vm_set_qos(u32 vm_id, vm_qos_t *qos);
extern int vm_set_affinity(u32 vm_id, vm_affinity_t *affinity);

/* Group Management */
extern int vm_group_create(const char *name, u32 *group_id);
extern int vm_group_add_vm(u32 group_id, u32 vm_id);
extern int vm_group_set_isolation(u32 group_id, bool isolated);

/* Template Management */
extern int vm_template_create(const char *name, vm_config_t *config, u32 *template_id);
extern int vm_deploy_from_template(u32 template_id, const char *vm_name, u32 *vm_id);

/* Checkpoint Management */
extern int vm_checkpoint_create(u32 vm_id, const char *name, u32 *checkpoint_id);
extern int vm_checkpoint_restore(u32 checkpoint_id);
extern int vm_checkpoint_delete(u32 checkpoint_id);

/* Monitoring */
extern int vm_get_metrics(u32 vm_id, vm_metrics_t *metrics);
extern int vm_register_event_callback(u32 vm_id, u32 event_type, vm_event_callback_t callback);

/* Scheduler */
extern void vm_scheduler_run(void);

#endif /* NEXUS_VM_MANAGER_H */
