/*
 * NEXUS OS - Kernel Configuration
 * kernel/include/config.h
 */

#ifndef _NEXUS_CONFIG_H
#define _NEXUS_CONFIG_H

/* Debug Options */
#define CONFIG_DEBUG            1
#define CONFIG_DEBUG_SPINLOCK   1
#define CONFIG_DEBUG_MUTEX      1
#define CONFIG_DEBUG_MEMORY     1
#define CONFIG_DEBUG_SCHED      1

/* Architecture Options */
#define CONFIG_ARCH_X86_64      1
#define CONFIG_SMP              1
#define CONFIG_MAX_CPUS         256

/* Memory Options */
#define CONFIG_KERNEL_HEAP_SIZE (1024 * 1024 * 1024)  /* 1 GB */
#define CONFIG_KERNEL_STACK_SIZE (64 * 1024)          /* 64 KB */
#define CONFIG_PAGE_SIZE        4096

/* Scheduler Options */
#define CONFIG_SCHEDULER_CFS    1
#define CONFIG_SCHEDULER_RT     1
#define CONFIG_MAX_PROCESSES    65536
#define CONFIG_MAX_THREADS      4096

/* Virtualization Options */
#define CONFIG_VIRTUALIZATION   1
#define CONFIG_HYPERVISOR       1
#define CONFIG_KVM              0

/* Filesystem Options */
#define CONFIG_VFS              1
#define CONFIG_NEXFS            1
#define CONFIG_EXT4             1
#define CONFIG_FAT32            1

/* Network Options */
#define CONFIG_NETWORK          1
#define CONFIG_IPV4             1
#define CONFIG_IPV6             1
#define CONFIG_TCP              1
#define CONFIG_UDP              1

/* Security Options */
#define CONFIG_SECURITY         1
#define CONFIG_CRYPTO           1
#define CONFIG_SANDBOX          1

/* Device Options */
#define CONFIG_PCI              1
#define CONFIG_USB              1
#define CONFIG_NVME             1
#define CONFIG_AHCI             1

/* GUI Options */
#define CONFIG_GUI              1
#define CONFIG_COMPOSITOR       1
#define CONFIG_RENDERER         1

/* AI/ML Options */
#define CONFIG_AI_ML            1
#define CONFIG_NPU              0

/* IoT Options */
#define CONFIG_IOT              1
#define CONFIG_MQTT             1
#define CONFIG_COAP             1

/* Mobile Options */
#define CONFIG_MOBILE           1
#define CONFIG_POWER_MANAGEMENT 1
#define CONFIG_THERMAL          1

/* Logging */
#define CONFIG_LOG_LEVEL        LOG_INFO
#define CONFIG_SERIAL_CONSOLE   1
#define CONFIG_FRAMEBUFFER_CONSOLE 1

#endif /* _NEXUS_CONFIG_H */
