/*
 * NEXUS OS - Universal Platform Support
 * platform/platform.h
 * 
 * Support for ALL platforms: VMware, VirtualBox, QEMU, Hyper-V, Bare Metal
 * With complete IPv6 and modern networking
 */

#ifndef _NEXUS_PLATFORM_H
#define _NEXUS_PLATFORM_H

#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         PLATFORM DETECTION                                */
/*===========================================================================*/

typedef enum {
    PLATFORM_UNKNOWN      = 0,
    PLATFORM_VMWARE       = 1,
    PLATFORM_VIRTUALBOX   = 2,
    PLATFORM_QEMU         = 3,
    PLATFORM_KVM          = 4,
    PLATFORM_HYPERV       = 5,
    PLATFORM_BAREMETAL    = 6,
    PLATFORM_XEN          = 7,
    PLATFORM_PARALLELS    = 8,
} platform_type_t;

typedef struct {
    platform_type_t type;
    char name[32];
    char vendor[64];
    u32 version;
    bool detected;
    
    /* Capabilities */
    bool has_virtio;
    bool has_vmci;
    bool has_hgfs;
    bool has_dragdrop;
    bool has_clipboard;
    
    /* Optimization flags */
    bool use_paravirt;
    bool use_msi;
    bool use_tsc_offset;
    bool use_apicv;
} platform_info_t;

/* Platform detection */
platform_type_t platform_detect(void);
void platform_init(platform_info_t *info);
const char *platform_get_name(platform_type_t type);
platform_info_t *platform_get_info(void);

/*===========================================================================*/
/*                         HYPERVISOR-SPECIFIC OPTIMIZATIONS                 */
/*===========================================================================*/

/* VMware optimizations */
void vmware_init(void);
void vmware_setup_vmci(void);
void vmware_setup_hypercall(void);

/* VirtualBox optimizations */
void virtualbox_init(void);
void virtualbox_setup_guest_additions(void);

/* QEMU/KVM optimizations */
void qemu_init(void);
void qemu_setup_virtio(void);
void kvm_setup_hypercall(void);

/* Hyper-V optimizations */
void hyperv_init(void);
void hyperv_setup_hypercall(void);
void hyperv_setup_synic(void);

/*===========================================================================*/
/*                         UNIVERSAL BOOT CONFIGURATION                      */
/*===========================================================================*/

typedef struct {
    char cmdline[512];
    platform_type_t force_platform;
    bool safe_mode;
    bool debug_mode;
    bool native_mode;
    u32 mem_size;
    u32 cpu_count;
} boot_config_t;

void boot_config_init(boot_config_t *config);
void boot_config_parse(const char *cmdline, boot_config_t *config);
int boot_config_validate(boot_config_t *config);

/*===========================================================================*/
/*                         MULTI-PLATFORM SUPPORT                            */
/*===========================================================================*/

/* Platform-specific memory mapping */
phys_addr_t platform_map_memory(phys_addr_t guest, size_t size);
void platform_unmap_memory(phys_addr_t guest, size_t size);

/* Platform-specific interrupts */
int platform_setup_interrupt(u32 irq);
void platform_teardown_interrupt(u32 irq);

/* Platform-specific timers */
u64 platform_get_tsc_frequency(void);
void platform_setup_timer(void);

/* Platform-specific console */
void platform_console_init(void);
void platform_console_write(const char *str, size_t len);

/*===========================================================================*/
/*                         FEATURE DETECTION                                 */
/*===========================================================================*/

/* CPU Features */
bool platform_has_vmx(void);      /* Intel VT-x */
bool platform_has_svm(void);      /* AMD-V */
bool platform_has_ept(void);      /* Extended Page Tables */
bool platform_has_npt(void);      /* Nested Page Tables */
bool platform_has_vtd(void);      /* Intel VT-d */
bool platform_has_iommu(void);    /* AMD IOMMU */

/* Device Features */
bool platform_has_virtio(void);
bool platform_has_vmci(void);
bool platform_has_usb3(void);
bool platform_has_nvme(void);

/* Network Features */
bool platform_has_vmxnet3(void);
bool platform_has_e1000(void);
bool platform_has_virtio_net(void);
bool platform_has_hyperv_net(void);

#endif /* _NEXUS_PLATFORM_H */
