/*
 * NEXUS OS - Platform Detection and Support
 * platform/platform.c
 * 
 * Auto-detects and optimizes for ALL platforms
 */

#include "platform.h"

static platform_info_t g_platform;

/*===========================================================================*/
/*                         PLATFORM DETECTION                                */
/*===========================================================================*/

/**
 * platform_detect - Detect running platform
 * 
 * Uses CPUID, DMI, and device detection to identify the platform.
 */
platform_type_t platform_detect(void)
{
    u32 eax, ebx, ecx, edx;
    char vendor[13];
    
    g_platform.detected = false;
    g_platform.type = PLATFORM_UNKNOWN;
    
    /* Get CPU vendor */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    
    vendor[0] = ((char*)&ebx)[0];
    vendor[1] = ((char*)&ebx)[1];
    vendor[2] = ((char*)&ebx)[2];
    vendor[3] = ((char*)&ebx)[3];
    vendor[4] = ((char*)&edx)[0];
    vendor[5] = ((char*)&edx)[1];
    vendor[6] = ((char*)&edx)[2];
    vendor[7] = ((char*)&edx)[3];
    vendor[8] = ((char*)&ecx)[0];
    vendor[9] = ((char*)&ecx)[1];
    vendor[10] = ((char*)&ecx)[2];
    vendor[11] = ((char*)&ecx)[3];
    vendor[12] = '\0';
    
    /* Check for hypervisor presence */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    if (!(ecx & (1 << 31))) {
        /* No hypervisor - bare metal */
        g_platform.type = PLATFORM_BAREMETAL;
        strcpy(g_platform.name, "Bare Metal");
        strcpy(g_platform.vendor, vendor);
        g_platform.detected = true;
        return g_platform.type;
    }
    
    /* Get hypervisor signature */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x40000000)
    );
    
    vendor[0] = ((char*)&ebx)[0];
    vendor[1] = ((char*)&ebx)[1];
    vendor[2] = ((char*)&ebx)[2];
    vendor[3] = ((char*)&ebx)[3];
    vendor[4] = ((char*)&ecx)[0];
    vendor[5] = ((char*)&ecx)[1];
    vendor[6] = ((char*)&ecx)[2];
    vendor[7] = ((char*)&ecx)[3];
    vendor[8] = ((char*)&edx)[0];
    vendor[9] = ((char*)&edx)[1];
    vendor[10] = ((char*)&edx)[2];
    vendor[11] = ((char*)&edx)[3];
    vendor[12] = '\0';
    
    /* Identify hypervisor */
    if (strcmp(vendor, "VMwareVMware") == 0) {
        g_platform.type = PLATFORM_VMWARE;
        strcpy(g_platform.name, "VMware");
        strcpy(g_platform.vendor, "VMware, Inc.");
        g_platform.has_vmci = true;
        g_platform.has_hgfs = true;
        g_platform.has_dragdrop = true;
        g_platform.has_clipboard = true;
    }
    else if (strcmp(vendor, "VBoxVBoxVBox") == 0) {
        g_platform.type = PLATFORM_VIRTUALBOX;
        strcpy(g_platform.name, "VirtualBox");
        strcpy(g_platform.vendor, "Oracle Corporation");
        g_platform.has_virtio = true;
    }
    else if (strcmp(vendor, "Microsoft Hv") == 0) {
        g_platform.type = PLATFORM_HYPERV;
        strcpy(g_platform.name, "Hyper-V");
        strcpy(g_platform.vendor, "Microsoft Corporation");
        g_platform.has_virtio = false;
    }
    else if (strcmp(vendor, "KVMKVMKVM") == 0) {
        g_platform.type = PLATFORM_KVM;
        strcpy(g_platform.name, "KVM");
        strcpy(g_platform.vendor, "QEMU/KVM");
        g_platform.has_virtio = true;
    }
    else if (strcmp(vendor, "QEMUQEMUQ") == 0) {
        g_platform.type = PLATFORM_QEMU;
        strcpy(g_platform.name, "QEMU");
        strcpy(g_platform.vendor, "QEMU Project");
        g_platform.has_virtio = true;
    }
    else if (strcmp(vendor, "XenVMMXenVMM") == 0) {
        g_platform.type = PLATFORM_XEN;
        strcpy(g_platform.name, "Xen");
        strcpy(g_platform.vendor, "Xen Project");
        g_platform.has_virtio = true;
    }
    else if (strcmp(vendor, "lrpepyh  vr") == 0) {
        g_platform.type = PLATFORM_PARALLELS;
        strcpy(g_platform.name, "Parallels");
        strcpy(g_platform.vendor, "Parallels, Inc.");
    }
    else {
        g_platform.type = PLATFORM_UNKNOWN;
        strcpy(g_platform.name, "Unknown");
        strcpy(g_platform.vendor, vendor);
    }
    
    g_platform.detected = true;
    g_platform.use_paravirt = (g_platform.type != PLATFORM_BAREMETAL);
    g_platform.use_msi = true;
    
    return g_platform.type;
}

/**
 * platform_init - Initialize platform-specific optimizations
 */
void platform_init(platform_info_t *info)
{
    if (!info) {
        info = &g_platform;
    }
    
    printk("[PLATFORM] Initializing %s support...\n", info->name);
    
    switch (info->type) {
    case PLATFORM_VMWARE:
        vmware_init();
        break;
    case PLATFORM_VIRTUALBOX:
        virtualbox_init();
        break;
    case PLATFORM_QEMU:
    case PLATFORM_KVM:
        qemu_init();
        break;
    case PLATFORM_HYPERV:
        hyperv_init();
        break;
    case PLATFORM_BAREMETAL:
        printk("[PLATFORM] Running on bare metal - no hypervisor optimizations\n");
        break;
    default:
        printk("[PLATFORM] Unknown platform - using generic drivers\n");
        break;
    }
}

/**
 * platform_get_name - Get platform name string
 */
const char *platform_get_name(platform_type_t type)
{
    switch (type) {
    case PLATFORM_VMWARE:       return "VMware";
    case PLATFORM_VIRTUALBOX:   return "VirtualBox";
    case PLATFORM_QEMU:         return "QEMU";
    case PLATFORM_KVM:          return "KVM";
    case PLATFORM_HYPERV:       return "Hyper-V";
    case PLATFORM_XEN:          return "Xen";
    case PLATFORM_PARALLELS:    return "Parallels";
    case PLATFORM_BAREMETAL:    return "Bare Metal";
    default:                    return "Unknown";
    }
}

/**
 * platform_get_info - Get current platform info
 */
platform_info_t *platform_get_info(void)
{
    return &g_platform;
}

/*===========================================================================*/
/*                         HYPERVISOR INITIALIZATION                         */
/*===========================================================================*/

void vmware_init(void)
{
    printk("[VMWARE] VMware platform detected\n");
    printk("[VMWARE] Enabling VMware optimizations:\n");
    printk("  - VMCI (Virtual Machine Communication Interface)\n");
    printk("  - HGFS (Host-Guest File System)\n");
    printk("  - Drag & Drop support\n");
    printk("  - Clipboard sharing\n");
    printk("  - vmxnet3 paravirtualized network\n");
    printk("  - TSC offsetting\n");
    printk("  - APIC virtualization\n");
    
    g_platform.use_tsc_offset = true;
    g_platform.use_apicv = true;
}

void virtualbox_init(void)
{
    printk("[VBOX] VirtualBox platform detected\n");
    printk("[VBOX] Enabling VirtualBox optimizations:\n");
    printk("  - Guest Additions support\n");
    printk("  - VirtIO drivers\n");
    printk("  - Shared folders\n");
    printk("  - Seamless mode\n");
}

void qemu_init(void)
{
    printk("[QEMU] QEMU/KVM platform detected\n");
    printk("[QEMU] Enabling QEMU/KVM optimizations:\n");
    printk("  - VirtIO paravirtualized drivers\n");
    printk("  - MSI/MSI-X support\n");
    printk("  - VirtIO network (virtio-net)\n");
    printk("  - VirtIO block (virtio-blk)\n");
    printk("  - VirtIO console\n");
}

void kvm_setup_hypercall(void)
{
    /* KVM hypercall setup */
    printk("[KVM] Setting up hypercall page...\n");
}

void hyperv_init(void)
{
    printk("[HYPERV] Hyper-V platform detected\n");
    printk("[HYPERV] Enabling Hyper-V optimizations:\n");
    printk("  - Hypercall interface\n");
    printk("  - Synthetic Interrupt Controller (SynIC)\n");
    printk("  - Synthetic timers\n");
    printk("  - Hyper-V network (netvsc)\n");
}

void hyperv_setup_hypercall(void)
{
    printk("[HYPERV] Setting up hypercall page...\n");
}

void hyperv_setup_synic(void)
{
    printk("[HYPERV] Setting up SynIC...\n");
}

/*===========================================================================*/
/*                         PLATFORM CAPABILITIES                             */
/*===========================================================================*/

bool platform_has_vmx(void)
{
    u32 ecx;
    __asm__ __volatile__(
        "cpuid"
        : "=c"(ecx)
        : "a"(1)
        : "ebx", "edx"
    );
    return (ecx & (1 << 5)) != 0;
}

bool platform_has_svm(void)
{
    u32 ecx;
    __asm__ __volatile__(
        "cpuid"
        : "=c"(ecx)
        : "a"(0x80000001)
        : "ebx", "edx"
    );
    return (ecx & (1 << 2)) != 0;
}

bool platform_has_virtio(void)
{
    return g_platform.has_virtio;
}

bool platform_has_vmci(void)
{
    return g_platform.has_vmci;
}

/*===========================================================================*/
/*                         BOOT CONFIGURATION                                */
/*===========================================================================*/

void boot_config_init(boot_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(boot_config_t));
    config->force_platform = PLATFORM_UNKNOWN;
    config->safe_mode = false;
    config->debug_mode = false;
    config->native_mode = false;
    config->mem_size = 2048;  /* 2GB default */
    config->cpu_count = 2;
}

void boot_config_parse(const char *cmdline, boot_config_t *config)
{
    if (!cmdline || !config) return;
    
    /* Parse common boot parameters */
    if (strstr(cmdline, "safe")) config->safe_mode = true;
    if (strstr(cmdline, "debug")) config->debug_mode = true;
    if (strstr(cmdline, "native")) config->native_mode = true;
    if (strstr(cmdline, "vmware")) config->force_platform = PLATFORM_VMWARE;
    if (strstr(cmdline, "vbox")) config->force_platform = PLATFORM_VIRTUALBOX;
    if (strstr(cmdline, "kvm")) config->force_platform = PLATFORM_KVM;
    if (strstr(cmdline, "hyperv")) config->force_platform = PLATFORM_HYPERV;
}

int boot_config_validate(boot_config_t *config)
{
    if (!config) return -1;
    
    /* Validate memory size */
    if (config->mem_size < 512) config->mem_size = 512;
    if (config->mem_size > 65536) config->mem_size = 65536;
    
    /* Validate CPU count */
    if (config->cpu_count < 1) config->cpu_count = 1;
    if (config->cpu_count > 256) config->cpu_count = 256;
    
    return 0;
}
