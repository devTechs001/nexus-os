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
/*                         PUBLIC TYPES                                      */
/*===========================================================================*/

/* Virtual CPU State */
typedef struct {
    u64 rip;
    u64 rsp;
    u64 rbp;
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    u64 rflags;
    u64 cr0, cr2, cr3, cr4;
    u64 dr0, dr1, dr2, dr3, dr6, dr7;
    u64 gdtr_base, gdtr_limit;
    u64 idtr_base, idtr_limit;
    u16 cs, ds, es, fs, gs, ss;
    u16 tr, ldt;
    u64 efer;
    u64 apic_base;
    u64 tsc_offset;
    u64 vmcs_pointer;
    bool interruptible;
    bool pending_nmi;
    bool pending_smi;
} vcpu_state_t;

/* Virtual CPU Configuration */
typedef struct {
    u32 vcpu_id;
    u32 host_cpu_id;
    bool pinned;
    u64 tsc_frequency;
    u64 apic_frequency;
    bool x2apic;
    bool xsave;
    bool fpu;
    bool vmx;
    bool svm;
} vcpu_config_t;

/* Virtual Memory Region */
typedef struct {
    u64 guest_physical;
    u64 host_physical;
    u64 size;
    u64 flags;
    bool mapped;
    bool writable;
    bool executable;
    bool cached;
} vm_memory_region_t;

/* Virtual Device */
typedef struct {
    u32 device_id;
    u32 vendor_id;
    u32 device_type;
    u32 irq;
    u64 mmio_base;
    u64 mmio_size;
    u64 pio_base;
    u64 pio_size;
    bool enabled;
    bool passthrough;
    bool msix;
    void *private_data;
} vm_virtual_device_t;

/* VM Configuration */
typedef struct {
    char name[128];
    char uuid[64];
    u32 vm_id;
    u32 vm_type;
    u32 execution_mode;
    u32 num_vcpus;
    u64 memory_size_mb;
    u64 max_memory_mb;
    bool memory_hotplug;
    bool nested_virt;
    u32 nested_level;
    u64 features;
    bool secure_boot;
    bool encrypted;
    bool live_migration;
    bool snapshot_support;
    bool gpu_passthrough;
    u32 gpu_count;
    char boot_image[256];
    char kernel_cmdline[512];
    u32 num_network;
    u32 num_storage;
    u32 num_usb;
    u32 num_pci;
} vm_config_t;

/* VM State */
typedef struct {
    u32 state;
    u64 uptime;
    u64 cpu_time;
    u64 memory_used;
    u64 disk_read;
    u64 disk_write;
    u64 network_rx;
    u64 network_tx;
    u32 exit_count;
    u32 interrupt_count;
    u32 exception_count;
    u32 vmexit_reason;
    u64 last_exit_timestamp;
} vm_state_t;

/* Virtual Machine */
typedef struct {
    vm_config_t config;
    vm_state_t state;
    vcpu_state_t *vcpus;
    vcpu_config_t *vcpu_configs;
    vm_memory_region_t *memory_regions;
    u32 num_memory_regions;
    vm_virtual_device_t *devices;
    u32 num_devices;
    u64 *page_tables;
    u64 ept_pointer;
    u32 vpid;
    void *vmcs;
    bool running;
    bool paused;
    bool stopped;
    spinlock_t lock;
} virtual_machine_t;

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
extern int vm_pause(virtual_machine_t *vm);
extern int vm_resume(virtual_machine_t *vm);
extern void vm_destroy(virtual_machine_t *vm);
extern virtual_machine_t *vm_find(u32 vm_id);

/* VM lifecycle states (vm->state.state) */
#define VM_STATE_NONE       0
#define VM_STATE_CREATED    1
#define VM_STATE_RUNNING    2
#define VM_STATE_PAUSED     3
#define VM_STATE_STOPPED    4

/* Boot a VM's vCPU from the architectural reset vector without starting
 * the full run loop; returns 0 on success. */
extern int vm_boot_from_reset_vector(virtual_machine_t *vm);

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
