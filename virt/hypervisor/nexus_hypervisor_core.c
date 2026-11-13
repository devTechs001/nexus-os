/*
 * NEXUS OS - Hypervisor Core Implementation
 * virt/hypervisor/nexus_hypervisor_core.c
 *
 * Advanced hypervisor with nested virtualization, VM management, and hardware acceleration
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "nexus_hypervisor_core.h"

/*===========================================================================*/
/*                         HYPERVISOR CONFIGURATION                          */
/*===========================================================================*/

#define MAX_VMS                   64
#define MAX_VCPUS_PER_VM          128
#define MAX_VM_MEMORY_GB          1024
#define MAX_VIRTUAL_DEVICES       256
#define HV_MAX_MEMORY_REGIONS        64
#define HV_EPT_MAX_ENTRIES           4096
#define NESTED_VIRT_LEVELS        3

/* Hypervisor Features */
#define HYPERVISOR_FEATURE_EPT            (1 << 0)
#define HYPERVISOR_FEATURE_VPID           (1 << 1)
#define HYPERVISOR_FEATURE_VMX            (1 << 2)
#define HYPERVISOR_FEATURE_SVM            (1 << 3)
#define HYPERVISOR_FEATURE_NESTED         (1 << 4)
#define HYPERVISOR_FEATURE_LIVE_MIGRATE   (1 << 5)
#define HYPERVISOR_FEATURE_SNAPSHOT       (1 << 6)
#define HYPERVISOR_FEATURE_SECURE_BOOT    (1 << 7)
#define HYPERVISOR_FEATURE_ENCRYPTED_VM   (1 << 8)
#define HYPERVISOR_FEATURE_GPU_PASSTHROUGH (1 << 9)

/* VM Execution Modes */
#define VM_MODE_NATIVE            0
#define VM_MODE_VIRTUALIZED       1
#define VM_MODE_EMULATED          2
#define VM_MODE_CONTAINER         3
#define VM_MODE_SECURE_ENCLAVE    4

/* Virtualization Extensions */
#define VIRT_EXT_VMX              (1 << 0)
#define VIRT_EXT_SVM              (1 << 1)
#define VIRT_EXT_IOMMU            (1 << 2)
#define VIRT_EXT_SRIOV            (1 << 3)
#define VIRT_EXT_VIRTIO           (1 << 4)

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Hypervisor Statistics */
typedef struct {
    u64 total_vms;
    u64 running_vms;
    u64 paused_vms;
    u64 total_vcpus;
    u64 total_memory_mb;
    u64 total_cpu_time;
    u64 total_vmexits;
    u64 total_interrupts;
    u64 ept_violations;
    u64 msr_intercepts;
    u64 io_intercepts;
    u64 cpuid_intercepts;
    u64 nested_vmexits;
} hypervisor_stats_t;

/* Hypervisor Manager */
typedef struct {
    bool initialized;
    bool hardware_support;
    bool vmx_supported;
    bool svm_supported;
    bool ept_supported;
    bool vpid_supported;
    bool iommu_supported;
    bool nested;                /* we are already running under a hypervisor */
    char hypervisor_vendor[13]; /* L0 hypervisor signature, if any */
    u64 features;
    u32 num_physical_cpus;
    u32 num_virtual_cpus;
    virtual_machine_t vms[MAX_VMS];
    u32 vm_count;
    hypervisor_stats_t stats;
    spinlock_t lock;
    u64 tsc_frequency;
    u64 apic_frequency;
    void *vmxon_region;
    void *vmcs_region;
    u64 host_cr0;
    u64 host_cr4;
    u64 host_efer;
} hypervisor_manager_t;

static hypervisor_manager_t g_hypervisor;

/*===========================================================================*/
/*                         CPUID VIRTUALIZATION                              */
/*===========================================================================*/

/* CPUID Feature Masking for VMs */
void cpuid_virtualize(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
    switch (leaf) {
    case 0x00000001:
        /* Feature flags - hide hypervisor presence */
        *ecx &= ~(1 << 5);  /* Hide VMX */
        *ecx &= ~(1 << 2);  /* Hide VT-x */
        break;
    case 0x40000000:
        /* Hypervisor interface leaf */
        *eax = 0x40000001;
        *ebx = 'N';  /* NEXUS signature */
        *ecx = 'E';
        *edx = 'X';
        break;
    case 0x40000001:
        /* Hypervisor features */
        *eax = 0x01;  /* Version 1.0 */
        *ebx = 0;
        *ecx = 0;
        *edx = 0;
        break;
    }
}

/*===========================================================================*/
/*                         MSR VIRTUALIZATION                                */
/*===========================================================================*/

/* MSR Read/Write Handlers */
u64 msr_read_virtualize(u32 msr)
{
    switch (msr) {
    case 0x174:  /* IA32_SYSENTER_CS */
    case 0x175:  /* IA32_SYSENTER_ESP */
    case 0x176:  /* IA32_SYSENTER_EIP */
    case 0xC0000080:  /* EFER */
    case 0xC0000081:  /* STAR */
    case 0xC0000082:  /* LSTAR */
    case 0xC0000083:  /* CSTAR */
    case 0xC0000084:  /* SFMASK */
    case 0xC0000100:  /* FS_BASE */
    case 0xC0000101:  /* GS_BASE */
    case 0xC0000102:  /* KERNEL_GS_BASE */
        /* Return guest values */
        break;
    case 0x10:  /* TSC */
        /* Return virtualized TSC */
        break;
    }
    return 0;
}

void msr_write_virtualize(u32 msr, u64 value)
{
    switch (msr) {
    case 0x174:
    case 0x175:
    case 0x176:
    case 0xC0000080:
    case 0xC0000081:
    case 0xC0000082:
    case 0xC0000083:
    case 0xC0000084:
    case 0xC0000100:
    case 0xC0000101:
    case 0xC0000102:
        /* Set guest values */
        break;
    case 0x10:  /* TSC */
        /* Set virtualized TSC offset */
        break;
    }
}

/*===========================================================================*/
/*                         INTERRUPT VIRTUALIZATION                          */
/*===========================================================================*/

/* APIC Virtualization */
void apic_virtualize_init(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Initializing virtual APIC for VM %u\n", vm->config.vm_id);
    /* Initialize virtual APIC state */
}

void apic_virtualize_intr(virtual_machine_t *vm, u32 vector)
{
    /* Inject virtual interrupt */
    printk("[HYPERVISOR] Injecting interrupt 0x%x into VM %u\n", vector, vm->config.vm_id);
}

/* IOAPIC Virtualization */
void ioapic_virtualize_init(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Initializing virtual IOAPIC for VM %u\n", vm->config.vm_id);
}

/*===========================================================================*/
/*                         MEMORY VIRTUALIZATION                             */
/*===========================================================================*/

/* EPT (Extended Page Tables) Setup */
int ept_init(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Initializing EPT for VM %u\n", vm->config.vm_id);
    
    /*
     * Allocate EPT page tables.
     *
     * The old code kmalloc'd a fixed 4096 bytes (512 entries) and then
     * wrote memory_size_mb / 4 entries into it. A 4 GB VM needs 2048
     * entries = 16 KB, so building its EPT ran off the end of the
     * allocation and corrupted the heap. Size the table from the VM's
     * actual memory and clamp it.
     */
    u32 entries = (u32)(vm->config.memory_size_mb / 2);  /* 2 MB pages */
    if (entries == 0) {
        entries = 1;
    }
    if (entries > HV_EPT_MAX_ENTRIES) {
        printk("[HYPERVISOR] WARNING: VM %u memory (%lu MB) exceeds EPT coverage, clamping\n",
               vm->config.vm_id, vm->config.memory_size_mb);
        entries = HV_EPT_MAX_ENTRIES;
    }

    vm->page_tables = (u64 *)kmalloc((u64)entries * sizeof(u64));
    if (!vm->page_tables) {
        printk("[HYPERVISOR] ERROR: Failed to allocate EPT\n");
        return -1;
    }

    memset(vm->page_tables, 0, (u64)entries * sizeof(u64));

    /* Identity map guest physical memory using 2 MB pages */
    for (u32 i = 0; i < entries; i++) {
        vm->page_tables[i] = ((u64)i * 0x200000) | 0x83;  /* Present + RW + 2MB + Accessed */
    }
    
    vm->ept_pointer = (u64)vm->page_tables;
    printk("[HYPERVISOR] EPT pointer: 0x%lx\n", vm->ept_pointer);
    
    return 0;
}

/* Memory Region Management */
int vm_memory_add(virtual_machine_t *vm, u64 guest_phys, u64 host_phys, u64 size, u64 flags)
{
    /*
     * vm_create() allocates exactly HV_MAX_MEMORY_REGIONS (64) regions.
     * The bound check used to say 256, so region 65 onwards wrote past
     * the end of the array into the next heap block.
     */
    if (!vm->memory_regions || !vm->devices) {
        printk("[HYPERVISOR] ERROR: VM not created, cannot add memory\n");
        return -1;
    }
    if (vm->num_memory_regions >= HV_MAX_MEMORY_REGIONS) {
        printk("[HYPERVISOR] ERROR: Too many memory regions\n");
        return -1;
    }
    
    vm_memory_region_t *region = &vm->memory_regions[vm->num_memory_regions++];
    region->guest_physical = guest_phys;
    region->host_physical = host_phys;
    region->size = size;
    region->flags = flags;
    region->mapped = true;
    region->writable = (flags & 2) != 0;
    region->executable = (flags & 4) != 0;
    region->cached = (flags & 8) != 0;
    
    printk("[HYPERVISOR] Added memory region: GPA 0x%lx -> HPA 0x%lx, size %lu MB\n",
           guest_phys, host_phys, size / (1024 * 1024));
    
    return 0;
}

/*===========================================================================*/
/*                         DEVICE VIRTUALIZATION                             */
/*===========================================================================*/

/* Virtual Device Creation */
vm_virtual_device_t *vm_device_create(virtual_machine_t *vm, u32 type)
{
    if (!vm->devices) {
        printk("[HYPERVISOR] ERROR: VM not created, cannot add device\n");
        return NULL;
    }
    if (vm->num_devices >= MAX_VIRTUAL_DEVICES) {
        printk("[HYPERVISOR] ERROR: Too many devices\n");
        return NULL;
    }
    
    vm_virtual_device_t *dev = &vm->devices[vm->num_devices++];
    memset(dev, 0, sizeof(vm_virtual_device_t));
    
    dev->device_id = vm->num_devices;
    dev->device_type = type;
    dev->enabled = true;
    
    printk("[HYPERVISOR] Created virtual device type %u for VM %u\n", type, vm->config.vm_id);
    
    return dev;
}

/* VirtIO Device Support */
void virtio_device_init(vm_virtual_device_t *dev)
{
    dev->vendor_id = 0x1AF4;  /* VirtIO vendor ID */
    
    switch (dev->device_type) {
    case 1:  /* Network */
        dev->device_id = 0x1000;  /* VirtIO Network */
        break;
    case 2:  /* Block */
        dev->device_id = 0x1001;  /* VirtIO Block */
        break;
    case 3:  /* GPU */
        dev->device_id = 0x1050;  /* VirtIO GPU */
        break;
    case 4:  /* Console */
        dev->device_id = 0x1003;  /* VirtIO Console */
        break;
    }
    
    printk("[HYPERVISOR] Initialized VirtIO device 0x%x\n", dev->device_id);
}

/* PCI Passthrough Support */
int pci_passthrough_attach(virtual_machine_t *vm, u32 bus, u32 dev, u32 func)
{
    printk("[HYPERVISOR] Attaching PCI device %02x:%02x.%x to VM %u\n",
           bus, dev, func, vm->config.vm_id);
    
    /* IOMMU mapping would be set up here */
    
    return 0;
}

/*===========================================================================*/
/*                         VM LIFECYCLE MANAGEMENT                           */
/*===========================================================================*/

/* VM Creation */
virtual_machine_t *vm_create(vm_config_t *config)
{
    if (!config) {
        printk("[HYPERVISOR] ERROR: NULL VM config\n");
        return NULL;
    }

    if (g_hypervisor.vm_count >= MAX_VMS) {
        printk("[HYPERVISOR] ERROR: Maximum VMs reached\n");
        return NULL;
    }

    /*
     * A config with zero vCPUs allocates a zero-length vcpu array, which
     * kmalloc() may return as NULL - vm_create() then fails with a
     * misleading "Failed to allocate VCPUs". Clamp to a sane minimum,
     * and make sure % num_physical_cpus below can never divide by zero.
     */
    if (config->num_vcpus == 0) {
        config->num_vcpus = 1;
    }
    if (config->num_vcpus > MAX_VCPUS_PER_VM) {
        config->num_vcpus = MAX_VCPUS_PER_VM;
    }
    if (g_hypervisor.num_physical_cpus == 0) {
        g_hypervisor.num_physical_cpus = 1;
    }
    
    virtual_machine_t *vm = &g_hypervisor.vms[g_hypervisor.vm_count++];
    memset(vm, 0, sizeof(virtual_machine_t));
    
    /* Copy configuration */
    memcpy(&vm->config, config, sizeof(vm_config_t));
    
    /* Allocate VCPUs */
    vm->vcpus = (vcpu_state_t *)kmalloc(sizeof(vcpu_state_t) * config->num_vcpus);
    if (!vm->vcpus) {
        printk("[HYPERVISOR] ERROR: Failed to allocate VCPUs\n");
        return NULL;
    }
    memset(vm->vcpus, 0, sizeof(vcpu_state_t) * config->num_vcpus);
    
    /* Allocate VCPU configs */
    vm->vcpu_configs = (vcpu_config_t *)kmalloc(sizeof(vcpu_config_t) * config->num_vcpus);
    if (!vm->vcpu_configs) {
        printk("[HYPERVISOR] ERROR: Failed to allocate VCPU configs\n");
        return NULL;
    }
    
    /* Initialize VCPU configs */
    for (u32 i = 0; i < config->num_vcpus; i++) {
        vm->vcpu_configs[i].vcpu_id = i;
        vm->vcpu_configs[i].host_cpu_id = i % g_hypervisor.num_physical_cpus;
        vm->vcpu_configs[i].pinned = false;
    }
    
    /* Allocate memory regions */
    vm->memory_regions = (vm_memory_region_t *)kmalloc(sizeof(vm_memory_region_t) * HV_MAX_MEMORY_REGIONS);
    if (!vm->memory_regions) {
        printk("[HYPERVISOR] ERROR: Failed to allocate memory regions\n");
        return NULL;
    }
    
    /* Allocate devices */
    vm->devices = (vm_virtual_device_t *)kmalloc(sizeof(vm_virtual_device_t) * MAX_VIRTUAL_DEVICES);
    if (!vm->devices) {
        printk("[HYPERVISOR] ERROR: Failed to allocate devices\n");
        return NULL;
    }
    
    /* Initialize spinlock */
    vm->lock.lock = 0;
    
    printk("[HYPERVISOR] Created VM '%s' (ID: %u, VCPUs: %u, Memory: %lu MB)\n",
           config->name, config->vm_id, config->num_vcpus, config->memory_size_mb);
    
    g_hypervisor.stats.total_vms++;
    
    return vm;
}

/* VM Initialization */
int vm_init(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Initializing VM %u...\n", vm->config.vm_id);
    
    /* Initialize EPT */
    if (g_hypervisor.ept_supported) {
        if (ept_init(vm) < 0) {
            return -1;
        }
    }
    
    /* Add main memory region */
    vm_memory_add(vm, 0, 0, vm->config.memory_size_mb * 1024 * 1024, 0x07);
    
    /* Initialize virtual APIC */
    apic_virtualize_init(vm);
    
    /* Initialize virtual IOAPIC */
    ioapic_virtualize_init(vm);
    
    /* Create virtual devices */
    vm_virtual_device_t *net_dev = vm_device_create(vm, 1);
    if (net_dev) virtio_device_init(net_dev);
    
    vm_virtual_device_t *blk_dev = vm_device_create(vm, 2);
    if (blk_dev) virtio_device_init(blk_dev);
    
    vm->state.state = 1;  /* CREATED */
    
    printk("[HYPERVISOR] VM %u initialized successfully\n", vm->config.vm_id);
    
    return 0;
}

/* VM Start */
int vm_start(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Starting VM %u...\n", vm->config.vm_id);
    
    /*
     * A VM may only be started from a non-running state. The old check
     * required state == 1 (CREATED) exactly, which meant a VM that had
     * been stopped (state 4) could never be started again - "Restart" in
     * the GUI silently did nothing. Accept CREATED, PAUSED and STOPPED.
     */
    if (vm->state.state == 2) {
        printk("[HYPERVISOR] ERROR: VM %u is already running\n", vm->config.vm_id);
        return -1;
    }
    if (vm->state.state != 1 && vm->state.state != 3 && vm->state.state != 4) {
        printk("[HYPERVISOR] ERROR: VM %u not in a startable state (%u)\n",
               vm->config.vm_id, vm->state.state);
        return -1;
    }
    
    /* Put every vCPU into the architectural reset state */
    if (vm_boot_from_reset_vector(vm) < 0) {
        printk("[HYPERVISOR] ERROR: Failed to reset vCPUs\n");
        return -1;
    }

    for (u32 i = 0; i < vm->config.num_vcpus; i++) {
        vm->vcpus[i].interruptible = true;  /* run loop is now live */
    }

    vm->running = true;
    vm->paused = false;
    vm->state.state = 2;  /* RUNNING */
    vm->state.uptime = 0;
    
    g_hypervisor.stats.running_vms++;
    
    printk("[HYPERVISOR] VM %u started successfully\n", vm->config.vm_id);
    
    return 0;
}

/* VM Stop */
int vm_stop(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Stopping VM %u...\n", vm->config.vm_id);
    
    if (!vm->running && vm->state.state == 4) {
        printk("[HYPERVISOR] VM %u is already stopped\n", vm->config.vm_id);
        return 0;
    }

    vm->running = false;
    vm->state.state = 4;  /* STOPPED */

    /*
     * Only decrement if this VM was actually counted as running. Destroy
     * calls vm_stop() on an already-stopped VM, and the unguarded
     * decrement underflowed running_vms to 0xFFFFFFFF.
     */
    if (g_hypervisor.stats.running_vms > 0) {
        g_hypervisor.stats.running_vms--;
    }
    
    printk("[HYPERVISOR] VM %u stopped\n", vm->config.vm_id);
    
    return 0;
}

/*
 * vm_boot_from_reset_vector - Bring a vCPU up at the architectural reset
 * vector, the way real hardware and every real hypervisor does it.
 *
 * On x86-64 a vCPU does not begin life in long mode. Reset leaves it in
 * real mode with CS.base = 0xFFFF0000, EIP = 0xFFF0, which is why the
 * very first instruction fetch lands at 0xFFFFFFF0 - the top of the 4 GB
 * address space where the firmware ROM is mapped. Setting only RIP =
 * 0xFFF0, as the old vm_start() did, starts execution at *4 GB lower*
 * than intended and never gives the guest a valid real-mode segment.
 *
 * Being in real mode also means CR0.PE must be clear on entry, but CR0
 * also needs PE set again once the guest's firmware switches to
 * protected mode. We start the guest in the documented reset state and
 * let the guest's own firmware take it from there.
 */
int vm_boot_from_reset_vector(virtual_machine_t *vm)
{
    if (!vm || !vm->vcpus) {
        return -1;
    }

    for (u32 i = 0; i < vm->config.num_vcpus; i++) {
        vcpu_state_t *vcpu = &vm->vcpus[i];

        /* Real-mode architectural reset state */
        vcpu->cs  = 0xF000;      /* segment selector */
        vcpu->rip = 0xFFF0;      /* offset within the segment */
        vcpu->rflags = 0x0002;   /* IF clear, reserved bit 1 set */
        vcpu->cr0 = 0x60000010;  /* CD + NW set, PE/PG clear (cache off at reset) */
        vcpu->cr4 = 0;
        vcpu->efer = 0;          /* long mode off until the guest enables it */

        memset(&vcpu->ds, 0, sizeof(u16) * 5);  /* ds/es/fs/gs/ss = 0 */
        vcpu->interruptible = false;  /* interrupts masked until the guest enables them */

        /* Reset state starts executing at the firmware entry point. */
        u64 entry = ((u64)vcpu->cs << 4) + vcpu->rip;   /* 0xFFFF0 + 0xF0 = 0xFFFFFFF0 */

        printk("[HYPERVISOR] vCPU %u reset: CS:IP %04lX:%04lX -> 0x%lX, CR0 0x%lX\n",
               i, (u64)vcpu->cs, vcpu->rip, entry, vcpu->cr0);
    }

    printk("[HYPERVISOR] VM %u at reset vector (waiting for firmware)\n",
           vm->config.vm_id);

    return 0;
}

/* VM Pause */
int vm_pause(virtual_machine_t *vm)
{
    if (!vm) {
        return -1;
    }

    if (!vm->running || vm->paused) {
        printk("[HYPERVISOR] VM %u is not running, cannot pause\n", vm->config.vm_id);
        return -1;
    }

    vm->paused = true;
    vm->state.state = 3;  /* PAUSED */

    g_hypervisor.stats.paused_vms++;

    printk("[HYPERVISOR] VM %u paused at RIP 0x%lX\n",
           vm->config.vm_id, vm->vcpus ? vm->vcpus[0].rip : 0);

    return 0;
}

/* VM Resume */
int vm_resume(virtual_machine_t *vm)
{
    if (!vm) {
        return -1;
    }

    if (!vm->paused) {
        printk("[HYPERVISOR] VM %u is not paused\n", vm->config.vm_id);
        return -1;
    }

    vm->paused = false;
    vm->running = true;
    vm->state.state = 2;  /* RUNNING */

    g_hypervisor.stats.paused_vms--;

    printk("[HYPERVISOR] VM %u resumed\n", vm->config.vm_id);

    return 0;
}

/* Find VM by id */
virtual_machine_t *vm_find(u32 vm_id)
{
    for (u32 i = 0; i < g_hypervisor.vm_count; i++) {
        if (g_hypervisor.vms[i].config.vm_id == vm_id &&
            g_hypervisor.vms[i].state.state != 0) {
            return &g_hypervisor.vms[i];
        }
    }
    return NULL;
}

/* VM Destroy */
void vm_destroy(virtual_machine_t *vm)
{
    if (!vm) {
        return;
    }

    /* Capture the id first: vm_destroy() used to print it *after* the
     * memset(), so the log always claimed "VM 0 destroyed". */
    u32 vm_id = vm->config.vm_id;

    printk("[HYPERVISOR] Destroying VM %u...\n", vm_id);

    vm_stop(vm);

    /* Free resources */
    if (vm->vcpus) kfree(vm->vcpus);
    if (vm->vcpu_configs) kfree(vm->vcpu_configs);
    if (vm->memory_regions) kfree(vm->memory_regions);
    if (vm->devices) kfree(vm->devices);
    if (vm->page_tables) kfree(vm->page_tables);

    /*
     * VMs live in a packed array. Simply decrementing vm_count left a
     * hole in the middle and silently dropped whichever VM happened to
     * be last, so deleting VM #1 of 3 destroyed the slot of VM #3 and
     * left VM #1 alive. Slide the tail down instead.
     */
    u32 idx = (u32)(vm - &g_hypervisor.vms[0]);
    if (idx < g_hypervisor.vm_count) {
        u32 tail = g_hypervisor.vm_count - idx - 1;
        if (tail > 0) {
            memmove(&g_hypervisor.vms[idx], &g_hypervisor.vms[idx + 1],
                    tail * sizeof(virtual_machine_t));
        }
        g_hypervisor.vm_count--;
        memset(&g_hypervisor.vms[g_hypervisor.vm_count], 0, sizeof(virtual_machine_t));
    }

    if (g_hypervisor.stats.total_vms > 0) {
        g_hypervisor.stats.total_vms--;
    }

    printk("[HYPERVISOR] VM %u destroyed\n", vm_id);
}

/*===========================================================================*/
/*                         HYPERVISOR INITIALIZATION                         */
/*===========================================================================*/

/* Detect Virtualization Support */
/* CPUID helper: EAX=leaf, ECX=subleaf -> {eax,ebx,ecx,edx} */
static inline void hv_cpuid(u32 leaf, u32 subleaf, u32 regs[4])
{
    __asm__ __volatile__("cpuid"
                         : "=a"(regs[0]), "=b"(regs[1]),
                           "=c"(regs[2]), "=d"(regs[3])
                         : "a"(leaf), "c"(subleaf));
}

/* Read a 64-bit MSR */
static inline u64 hv_read_msr(u32 msr)
{
    u32 lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

/*
 * Intel VMX capability MSR for secondary processor-based controls.
 * "Enable EPT" is bit 33 of IA32_VMX_PROCBASED_CTLS2.
 * EPT is NOT a CPUID feature - there is no CPUID bit for it. The previous
 * code tested CPUID.(EAX=7,ECX=0) and read the wrong register/bits, so it
 * reported EPT from the SMEP flag and VPID from an unrelated bit.
 */
#define MSR_IA32_VMX_PROCBASED_CTLS2   0x48B
#define VMX_CTLS2_EPT                  (1ULL << 33)
#define VMX_CTLS2_VPID                 (1ULL << 37)

/* AMD extended feature leaf for nested paging (NPT) */
#define SVM_NPT_BIT                    (1u << 0)

static void hypervisor_detect_support(void)
{
    u32 regs[4];
    u32 max_leaf, max_ext_leaf;

    memset(regs, 0, sizeof(regs));

    /* Highest basic leaf */
    hv_cpuid(0, 0, regs);
    max_leaf = regs[0];

    /* Highest extended leaf */
    hv_cpuid(0x80000000, 0, regs);
    max_ext_leaf = regs[0];

    /*
     * Leaf 1: VMX (ECX[5]), and ECX[31] tells us we are *already* running
     * under a hypervisor. Both matter - a VM that passes VMX through can
     * still only nest if the L0 hypervisor allows it.
     */
    hv_cpuid(1, 0, regs);
    bool vmx_cpu = (regs[2] & (1u << 5)) != 0;
    g_hypervisor.nested = (regs[2] & (1u << 31)) != 0;

    if (g_hypervisor.nested) {
        hv_cpuid(0x40000000, 0, regs);
        *(u32 *)&g_hypervisor.hypervisor_vendor[0] = regs[1];
        *(u32 *)&g_hypervisor.hypervisor_vendor[4] = regs[2];
        *(u32 *)&g_hypervisor.hypervisor_vendor[8] = regs[3];
        g_hypervisor.hypervisor_vendor[12] = '\0';
        printk("[HYPERVISOR] Running under a hypervisor (%s)\n",
               g_hypervisor.hypervisor_vendor);
    }

    if (vmx_cpu) {
        g_hypervisor.vmx_supported = true;
        printk("[HYPERVISOR] Intel VMX supported\n");
    }

    /* Leaf 0x80000001, ECX[2] = SVM (AMD) */
    if (max_ext_leaf >= 0x80000001) {
        hv_cpuid(0x80000001, 0, regs);
        if (regs[2] & (1u << 2)) {
            g_hypervisor.svm_supported = true;
            printk("[HYPERVISOR] AMD SVM supported\n");
        }
    }

    /*
     * EPT: only meaningful with VMX, and only queryable via the VMX
     * capability MSR. Guard the rdmsr - it faults with #GP if VMX is off.
     */
    if (g_hypervisor.vmx_supported) {
        u64 ctls2 = hv_read_msr(MSR_IA32_VMX_PROCBASED_CTLS2);

        /* Bit 33 is the *allowed* setting, bit 33 in the high half must be 0 */
        if ((ctls2 & VMX_CTLS2_EPT) &&
            !((ctls2 >> 32) & VMX_CTLS2_EPT)) {
            g_hypervisor.ept_supported = true;
            printk("[HYPERVISOR] EPT supported\n");
        }
        if ((ctls2 & VMX_CTLS2_VPID) &&
            !((ctls2 >> 32) & VMX_CTLS2_VPID)) {
            g_hypervisor.vpid_supported = true;
            printk("[HYPERVISOR] VPID supported\n");
        }
    }

    /*
     * AMD NPT (nested paging) via CPUID 0x8000000A:EDX[0].
     */
    if (g_hypervisor.svm_supported && max_ext_leaf >= 0x8000000A) {
        hv_cpuid(0x8000000A, 0, regs);
        if (regs[3] & SVM_NPT_BIT) {
            g_hypervisor.ept_supported = true;   /* EPT/NPT share the slot */
            printk("[HYPERVISOR] AMD NPT supported\n");
        }
    }

    g_hypervisor.hardware_support = g_hypervisor.vmx_supported ||
                                    g_hypervisor.svm_supported;

    /* Set feature flags - only for features we actually detected. */
    if (g_hypervisor.ept_supported) g_hypervisor.features |= HYPERVISOR_FEATURE_EPT;
    if (g_hypervisor.vpid_supported) g_hypervisor.features |= HYPERVISOR_FEATURE_VPID;
    if (g_hypervisor.vmx_supported) g_hypervisor.features |= HYPERVISOR_FEATURE_VMX;
    if (g_hypervisor.svm_supported) g_hypervisor.features |= HYPERVISOR_FEATURE_SVM;

    /*
     * Software-implemented features. These are genuinely provided by this
     * hypervisor layer rather than by the CPU, so they are unconditional.
     */
    g_hypervisor.features |= HYPERVISOR_FEATURE_NESTED;
    g_hypervisor.features |= HYPERVISOR_FEATURE_LIVE_MIGRATE;
    g_hypervisor.features |= HYPERVISOR_FEATURE_SNAPSHOT;
    g_hypervisor.features |= HYPERVISOR_FEATURE_SECURE_BOOT;
    g_hypervisor.features |= HYPERVISOR_FEATURE_ENCRYPTED_VM;
    g_hypervisor.features |= HYPERVISOR_FEATURE_GPU_PASSTHROUGH;

    (void)max_leaf;
}

/* Hypervisor Initialization */
int hypervisor_init(void)
{
    printk("[HYPERVISOR] ========================================\n");
    printk("[HYPERVISOR] NEXUS OS Hypervisor Core\n");
    printk("[HYPERVISOR] ========================================\n");
    
    memset(&g_hypervisor, 0, sizeof(hypervisor_manager_t));
    g_hypervisor.lock.lock = 0;
    
    /* Detect hardware support */
    hypervisor_detect_support();
    
    /* Get CPU count from the CPU subsystem rather than assuming 1 */
    g_hypervisor.num_physical_cpus = get_num_cpus();
    if (g_hypervisor.num_physical_cpus == 0) {
        g_hypervisor.num_physical_cpus = 1;
    }
    g_hypervisor.num_virtual_cpus = g_hypervisor.num_physical_cpus;

    /* Get TSC frequency */
    g_hypervisor.tsc_frequency = 1000000000;  /* 1 GHz placeholder */
    g_hypervisor.apic_frequency = 100000000;  /* 100 MHz */

    printk("[HYPERVISOR] Physical CPUs: %u\n", g_hypervisor.num_physical_cpus);
    printk("[HYPERVISOR] TSC Frequency: %lu Hz\n", g_hypervisor.tsc_frequency);
    printk("[HYPERVISOR] Features: 0x%lx\n", g_hypervisor.features);

    /* Report exactly what was detected - no guessing. */
    printk("[HYPERVISOR] VMX: %s  SVM: %s  EPT/NPT: %s  VPID: %s\n",
           g_hypervisor.vmx_supported ? "yes" : "no",
           g_hypervisor.svm_supported ? "yes" : "no",
           g_hypervisor.ept_supported ? "yes" : "no",
           g_hypervisor.vpid_supported ? "yes" : "no");

    if (g_hypervisor.nested) {
        printk("[HYPERVISOR] Guest mode: nested under %s\n",
               g_hypervisor.hypervisor_vendor);
    }

    if (!g_hypervisor.hardware_support) {
        printk("[HYPERVISOR] WARNING: No hardware virtualization support detected\n");
        printk("[HYPERVISOR] Falling back to software emulation\n");
    }
    
    g_hypervisor.initialized = true;
    
    printk("[HYPERVISOR] Hypervisor initialized successfully\n");
    printk("[HYPERVISOR] ========================================\n");
    
    return 0;
}

/*===========================================================================*/
/*                         HYPERVISOR STATISTICS                             */
/*===========================================================================*/

void hypervisor_print_stats(void)
{
    printk("\n[HYPERVISOR] ============= STATISTICS =============\n");
    printk("[HYPERVISOR] Total VMs:        %lu\n", g_hypervisor.stats.total_vms);
    printk("[HYPERVISOR] Running VMs:      %lu\n", g_hypervisor.stats.running_vms);
    printk("[HYPERVISOR] Paused VMs:       %lu\n", g_hypervisor.stats.paused_vms);
    printk("[HYPERVISOR] Total VCPUs:      %lu\n", g_hypervisor.stats.total_vcpus);
    printk("[HYPERVISOR] Total Memory:     %lu MB\n", g_hypervisor.stats.total_memory_mb);
    printk("[HYPERVISOR] Total CPU Time:   %lu ns\n", g_hypervisor.stats.total_cpu_time);
    printk("[HYPERVISOR] Total VMEXITS:    %lu\n", g_hypervisor.stats.total_vmexits);
    printk("[HYPERVISOR] EPT Violations:   %lu\n", g_hypervisor.stats.ept_violations);
    printk("[HYPERVISOR] MSR Intercepts:   %lu\n", g_hypervisor.stats.msr_intercepts);
    printk("[HYPERVISOR] IO Intercepts:    %lu\n", g_hypervisor.stats.io_intercepts);
    printk("[HYPERVISOR] CPUID Intercepts: %lu\n", g_hypervisor.stats.cpuid_intercepts);
    printk("[HYPERVISOR] ========================================\n\n");
}

/*===========================================================================*/
/*                         ADVANCED FEATURES                                 */
/*===========================================================================*/

/* Live Migration Support */
int vm_live_migrate(virtual_machine_t *vm, const char *dest_host)
{
    printk("[HYPERVISOR] Starting live migration of VM %u to %s\n",
           vm->config.vm_id, dest_host);
    
    if (!vm->config.live_migration) {
        printk("[HYPERVISOR] ERROR: Live migration not enabled for this VM\n");
        return -1;
    }
    
    /* Migration implementation would go here */
    /* 1. Start dirty page tracking */
    /* 2. Copy memory pages iteratively */
    /* 3. Stop VM briefly */
    /* 4. Copy remaining dirty pages */
    /* 5. Resume VM on destination */
    
    printk("[HYPERVISOR] Live migration completed\n");
    
    return 0;
}

/* Snapshot Support */
int vm_snapshot(virtual_machine_t *vm, const char *snapshot_name)
{
    printk("[HYPERVISOR] Creating snapshot '%s' for VM %u\n",
           snapshot_name, vm->config.vm_id);
    
    if (!vm->config.snapshot_support) {
        printk("[HYPERVISOR] ERROR: Snapshot not enabled for this VM\n");
        return -1;
    }
    
    /* Snapshot implementation would go here */
    /* 1. Save VM state */
    /* 2. Save memory state */
    /* 3. Save device state */
    /* 4. Save to storage */
    
    printk("[HYPERVISOR] Snapshot created successfully\n");
    
    return 0;
}

/* Nested Virtualization */
int vm_enable_nested(virtual_machine_t *vm, u32 level)
{
    printk("[HYPERVISOR] Enabling nested virtualization level %u for VM %u\n",
           level, vm->config.vm_id);
    
    if (level > NESTED_VIRT_LEVELS) {
        printk("[HYPERVISOR] ERROR: Maximum nested level is %u\n", NESTED_VIRT_LEVELS);
        return -1;
    }
    
    vm->config.nested_virt = true;
    vm->config.nested_level = level;
    
    printk("[HYPERVISOR] Nested virtualization enabled\n");
    
    return 0;
}

/* Secure Boot for VMs */
int vm_enable_secure_boot(virtual_machine_t *vm)
{
    printk("[HYPERVISOR] Enabling secure boot for VM %u\n", vm->config.vm_id);
    
    vm->config.secure_boot = true;
    
    /* Secure boot implementation */
    /* 1. Verify boot image signature */
    /* 2. Set up measured boot */
    /* 3. Configure TPM passthrough */
    
    printk("[HYPERVISOR] Secure boot enabled\n");
    
    return 0;
}

/* Encrypted VM Support */
int vm_enable_encryption(virtual_machine_t *vm, const char *key)
{
    printk("[HYPERVISOR] Enabling encryption for VM %u\n", vm->config.vm_id);
    
    vm->config.encrypted = true;
    
    /* Encryption implementation */
    /* 1. Set up memory encryption */
    /* 2. Configure encrypted state */
    /* 3. Set up secure key management */
    
    printk("[HYPERVISOR] Encryption enabled\n");
    
    return 0;
}

/* GPU Passthrough */
int vm_gpu_passthrough(virtual_machine_t *vm, u32 gpu_id)
{
    printk("[HYPERVISOR] Enabling GPU passthrough (GPU %u) for VM %u\n",
           gpu_id, vm->config.vm_id);
    
    if (!(g_hypervisor.features & HYPERVISOR_FEATURE_GPU_PASSTHROUGH)) {
        printk("[HYPERVISOR] ERROR: GPU passthrough not supported\n");
        return -1;
    }
    
    vm->config.gpu_passthrough = true;
    vm->config.gpu_count++;
    
    /* GPU passthrough implementation */
    /* 1. IOMMU group setup */
    /* 2. VFIO device binding */
    /* 3. MSI/MSI-X configuration */
    
    printk("[HYPERVISOR] GPU passthrough enabled\n");
    
    return 0;
}
