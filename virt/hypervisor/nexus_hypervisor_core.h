/*
 * NEXUS OS - Hypervisor Core Header
 * virt/hypervisor/nexus_hypervisor_core.h
 *
 * Advanced hypervisor interface and API
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef NEXUS_HYPERVISOR_CORE_H
#define NEXUS_HYPERVISOR_CORE_H

#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         HYPERVISOR API                                    */
/*===========================================================================*/

/* Initialization */
extern int hypervisor_init(void);
extern void hypervisor_print_stats(void);

/* VM Management */
extern virtual_machine_t *vm_create(vm_config_t *config);
extern int vm_init(virtual_machine_t *vm);
extern int vm_start(virtual_machine_t *vm);
extern int vm_stop(virtual_machine_t *vm);
extern void vm_destroy(virtual_machine_t *vm);

/* Advanced Features */
extern int vm_live_migrate(virtual_machine_t *vm, const char *dest_host);
extern int vm_snapshot(virtual_machine_t *vm, const char *snapshot_name);
extern int vm_enable_nested(virtual_machine_t *vm, u32 level);
extern int vm_enable_secure_boot(virtual_machine_t *vm);
extern int vm_enable_encryption(virtual_machine_t *vm, const char *key);
extern int vm_gpu_passthrough(virtual_machine_t *vm, u32 gpu_id);

/* Device Management */
extern vm_virtual_device_t *vm_device_create(virtual_machine_t *vm, u32 type);
extern void virtio_device_init(vm_virtual_device_t *dev);
extern int pci_passthrough_attach(virtual_machine_t *vm, u32 bus, u32 dev, u32 func);

/* Memory Management */
extern int vm_memory_add(virtual_machine_t *vm, u64 guest_phys, u64 host_phys, u64 size, u64 flags);
extern int ept_init(virtual_machine_t *vm);

/* CPU Virtualization */
extern void cpuid_virtualize(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx);
extern u64 msr_read_virtualize(u32 msr);
extern void msr_write_virtualize(u32 msr, u64 value);

/* Interrupt Virtualization */
extern void apic_virtualize_init(virtual_machine_t *vm);
extern void apic_virtualize_intr(virtual_machine_t *vm, u32 vector);
extern void ioapic_virtualize_init(virtual_machine_t *vm);

#endif /* NEXUS_HYPERVISOR_CORE_H */
