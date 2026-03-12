/*
 * NEXUS OS - AHCI/SATA Storage Driver
 * drivers/storage/ahci.h
 *
 * Complete AHCI 1.3.1 compliant driver with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _AHCI_H
#define _AHCI_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         AHCI SPECIFICATION CONSTANTS                      */
/*===========================================================================*/

/* AHCI Specification Version */
#define AHCI_VERSION_MAJOR          1
#define AHCI_VERSION_MINOR          3
#define AHCI_VERSION                ((AHCI_VERSION_MAJOR << 16) | AHCI_VERSION_MINOR)

/* PCI IDs */
#define AHCI_PCI_CLASS              0x010601  /* AHCI SATA */
#define AHCI_PCI_CLASS_MASK         0xFFFFFF

/* AHCI Register Offsets */
#define AHCI_REG_CAP                0x00    /* Host Capabilities */
#define AHCI_REG_GHC                0x04    /* Global Host Control */
#define AHCI_REG_IS                 0x08    /* Interrupt Status */
#define AHCI_REG_PI                 0x0C    /* Ports Implemented */
#define AHCI_REG_VS                 0x10    /* AHCI Version */
#define AHCI_REG_CCC_CTL            0x14    /* Command Completion Coalescing Control */
#define AHCI_REG_CCC_PORTS          0x18    /* Command Completion Coalescing Ports */
#define AHCI_REG_EM_LOC             0x1C    /* Enclosure Management Location */
#define AHCI_REG_EM_CTL             0x20    /* Enclosure Management Control */
#define AHCI_REG_CAP2               0x24    /* Host Capabilities Extended */
#define AHCI_REG_BOHC               0x28    /* BIOS/OS Handoff Control and Status */
#define AHCI_REG_OFFSET             0x100   /* Port Register Offset */

/* Port Register Offsets */
#define AHCI_PX_REG_CLB             0x00    /* Command List Base Address */
#define AHCI_PX_REG_FB              0x08    /* FIS Base Address */
#define AHCI_PX_REG_IS              0x10    /* Interrupt Status */
#define AHCI_PX_REG_IE              0x14    /* Interrupt Enable */
#define AHCI_PX_REG_CMD             0x18    /* Command and Status */
#define AHCI_PX_REG_RESERVED        0x1C    /* Reserved */
#define AHCI_PX_REG_TFD             0x20    /* Task File Data */
#define AHCI_PX_REG_SIG             0x24    /* Signature */
#define AHCI_PX_REG_SSTS            0x28    /* SATA Status */
#define AHCI_PX_REG_SCTL            0x2C    /* SATA Control */
#define AHCI_PX_REG_SERR            0x30    /* SATA Error */
#define AHCI_PX_REG_SACT            0x34    /* SATA Active */
#define AHCI_PX_REG_CI              0x38    /* Command Issue */
#define AHCI_PX_REG_SNTF            0x3C    /* SATA Notification */
#define AHCI_PX_REG_FBS             0x40    /* FIS-Based Switching */
#define AHCI_PX_REG_DEVSLP          0x44    /* Device Sleep */
#define AHCI_PX_REG_RESERVED2       0x48    /* Reserved */
#define AHCI_PX_REG_VENDOR          0xA0    /* Vendor Specific */

/* Host Capabilities */
#define AHCI_CAP_S64A               (1 << 31) /* 64-bit Addressing */
#define AHCI_CAP_SNCQ               (1 << 30) /* Native Command Queuing */
#define AHCI_CAP_SSNTF              (1 << 29) /* SNotification Register */
#define AHCI_CAP_SMPS               (1 << 28) /* Mechanical Presence Switch */
#define AHCI_CAP_SFBP               (1 << 27) /* FIS-Based Switching */
#define AHCI_CAP_PIO_MULTI          (1 << 26) /* PIO Multiple DRQ Block */
#define AHCI_CAP_FBS                (1 << 24) /* FIS-Based Switching */
#define AHCI_CAP_PSC                (1 << 23) /* Port Speed Configuration */
#define AHCI_CAP_MPS                (1 << 22) /* Mechanical Presence Switch Attached */
#define AHCI_CAP_SPM                (1 << 21) /* Slumber Partial Mechanical */
#define AHCI_CAP_AL                 (1 << 20) /* Activity LED */
#define AHCI_CAP_SCLO               (1 << 19) /* Command List Override */
#define AHCI_CAP_SALP               (1 << 18) /* Aggressive Link Power Management */
#define AHCI_CAP_SAL                (1 << 17) /* Staggered Spin-up Allowed */
#define AHCI_CAP_SKC                (1 << 16) /* Staggered Spin-up Capable */
#define AHCI_CAP_SSC                (1 << 14) /* Slumber Capable */
#define AHCI_CAP_PMD                (1 << 13) /* Port Multiplier Device */
#define AHCI_CAP_SSCC               (1 << 12) /* Command Completion Coalescing */
#define AHCI_CAP_EMS                (1 << 11) /* Enclosure Management */
#define AHCI_CAP_SXS                (1 << 10) /* External SATA */
#define AHCI_CAP_NP_MASK            0x1F      /* Number of Ports */

/* Global Host Control */
#define AHCI_GHC_HR                 (1 << 31) /* HBA Reset */
#define AHCI_GHC_IE                 (1 << 1)  /* Interrupt Enable */
#define AHCI_GHC_AE                 (1 << 0)  /* AHCI Enable */

/* Port Command and Status */
#define AHCI_PX_CMD_ST              (1 << 0)  /* Start HBA */
#define AHCI_PX_CMD_SUD             (1 << 1)  /* Spin-Up Device */
#define AHCI_PX_CMD_POD             (1 << 2)  /* Power On Device */
#define AHCI_PX_CMD_CLO             (1 << 3)  /* Command List Override */
#define AHCI_PX_CMD_FRE             (1 << 4)  /* FIS Receive Enable */
#define AHCI_PX_CMD_MPSS            (1 << 13) /* Mechanical Presence State */
#define AHCI_PX_CMD_FR              (1 << 14) /* FIS Receive Running */
#define AHCI_PX_CMD_CR              (1 << 15) /* Command List Running */
#define AHCI_PX_CMD_CPS             (1 << 16) /* Command Protection State */
#define AHCI_PX_CMD_PMA             (1 << 17) /* Port Multiplier Attached */
#define AHCI_PX_CMD_HPCP            (1 << 18) /* Hot Plug Capable Port */
#define AHCI_PX_CMD_MPS             (1 << 19) /* Mechanical Presence State */
#define AHCI_PX_CMD_CPD             (1 << 20) /* Cold Presence Detection */
#define AHCI_PX_CMD_ESP             (1 << 21) /* External SATA Port */
#define AHCI_PX_CMD_FBSCP           (1 << 22) /* FIS-Based Switching Capable Port */
#define AHCI_PX_CMD_TFSS            (1 << 23) /* Task File Set Status */
#define AHCI_PX_CMD_ALPE            (1 << 24) /* Activity LED Port Enable */
#define AHCI_PX_CMD_ASP             (1 << 25) /* Aggressive Slumber/Partial */
#define AHCI_PX_CMD_DLAE            (1 << 26) /* Drive LED on ATAPI Enable */
#define AHCI_PX_CMD_ATAPI           (1 << 27) /* Device is ATAPI */
#define AHCI_PX_CMD_LA             (1 << 28) /* LED Aggressive */
#define AHCI_PX_CMD_CP             (1 << 29) /* Cold Presence Detection */
#define AHCI_PX_CMD_CCS_MASK        0x1F000000 /* Current Command Slot */
#define AHCI_PX_CMD_ICC_MASK        0xF0000000 /* Interface Control Code */

/* SATA Status */
#define AHCI_PX_SSTS_DET_MASK       0x0F      /* Device Detection */
#define AHCI_PX_SSTS_DET_NONE       0x00      /* No device detected */
#define AHCI_PX_SSTS_DET_PRESENT    0x03      /* Device present */
#define AHCI_PX_SSTS_SPD_MASK       0xF0      /* Interface Speed */
#define AHCI_PX_SSTS_SPD_GEN1       0x10      /* Gen1 (1.5 Gbps) */
#define AHCI_PX_SSTS_SPD_GEN2       0x20      /* Gen2 (3.0 Gbps) */
#define AHCI_PX_SSTS_SPD_GEN3       0x30      /* Gen3 (6.0 Gbps) */
#define AHCI_PX_SSTS_IPM_MASK       0xF00     /* Interface Power Management */
#define AHCI_PX_SSTS_IPM_ACTIVE     0x100     /* Active */
#define AHCI_PX_SSTS_IPM_PARTIAL    0x200     /* Partial */
#define AHCI_PX_SSTS_IPM_SLUMBER    0x600     /* Slumber */

/* SATA Control */
#define AHCI_PX_SCTL_DET_MASK       0x0F      /* Device Detection */
#define AHCI_PX_SCTL_DET_NONE       0x00      /* No detection */
#define AHCI_PX_SCTL_DET_RESET      0x01      /* Reset device */
#define AHCI_PX_SCTL_DET_DISABLE    0x04      /* Disable detection */
#define AHCI_PX_SCTL_SPD_MASK       0xF0      /* Interface Speed */
#define AHCI_PX_SCTL_IPM_MASK       0xF00     /* Interface Power Management */

/* SATA Error */
#define AHCI_PX_SERR_M              (1 << 0)  /* Data Transfer Error */
#define AHCI_PX_SERR_T              (1 << 1)  /* Transient Data Transfer Error */
#define AHCI_PX_SERR_C              (1 << 2)  /* CRC Error */
#define AHCI_PX_SERR_H              (1 << 3)  /* Handshake Error */
#define AHCI_PX_SERR_I              (1 << 4)  /* Internal Error */
#define AHCI_PX_SERR_P              (1 << 5)  /* Protocol Error */
#define AHCI_PX_SERR_E              (1 << 6)  /* External Status */
#define AHCI_PX_SERR_S              (1 << 7)  /* Sequence Error */
#define AHCI_PX_SERR_A              (1 << 8)  /* Host Asserted Error */
#define AHCI_PX_SERR_D              (1 << 9)  /* Device Asserted Error */
#define AHCI_PX_SERR_TR             (1 << 10) /* Transport State Transition */
#define AHCI_PX_SERR_UN             (1 << 11) /* Unrecognized FIS */
#define AHCI_PX_SERR_W              (1 << 12) /* Wakeup Error */
#define AHCI_PX_SERR_B              (1 << 13) /* Bad Address */
#define AHCI_PX_SERR_I_E            (1 << 16) /* Incorrect FIS */
#define AHCI_PX_SERR_M_E            (1 << 17) /* CRC Error */
#define AHCI_PX_SERR_T_E            (1 << 18) /* Transient Error */
#define AHCI_PX_SERR_H_E            (1 << 19) /* Handshake Error */
#define AHCI_PX_SERR_I_E2           (1 << 20) /* Internal Error */
#define AHCI_PX_SERR_P_E            (1 << 21) /* Protocol Error */
#define AHCI_PX_SERR_E_E            (1 << 22) /* External Status Error */
#define AHCI_PX_SERR_S_E            (1 << 23) /* Sequence Error */
#define AHCI_PX_SERR_A_E            (1 << 24) /* Host Asserted Error */
#define AHCI_PX_SERR_D_E            (1 << 25) /* Device Asserted Error */
#define AHCI_PX_SERR_TR_E           (1 << 26) /* Transport State Transition Error */
#define AHCI_PX_SERR_UN_E           (1 << 27) /* Unrecognized FIS Error */
#define AHCI_PX_SERR_W_E            (1 << 28) /* Wakeup Error */
#define AHCI_PX_SERR_B_E            (1 << 29) /* Bad Address Error */

/* Task File Data */
#define AHCI_PX_TFD_ERR             (1 << 0)  /* Error */
#define AHCI_PX_TFD_DRQ             (1 << 3)  /* Data Request */
#define AHCI_PX_TFD_SRV             (1 << 4)  /* Service */
#define AHCI_PX_TFD_DF              (1 << 5)  /* Device Fault */
#define AHCI_PX_TFD_RDY             (1 << 6)  /* Ready */
#define AHCI_PX_TFD_BUSY            (1 << 7)  /* Busy */

/* Interrupt Status/Enable */
#define AHCI_PX_IS_DHRS             (1 << 0)  /* Device to Host Register FIS */
#define AHCI_PX_IS_PSS              (1 << 1)  /* PIO Setup FIS */
#define AHCI_PX_IS_DSS              (1 << 2)  /* DMA Setup FIS */
#define AHCI_PX_IS_SDBS             (1 << 3)  /* Set Device Bits FIS */
#define AHCI_PX_IS_UFS              (1 << 4)  /* Unknown FIS */
#define AHCI_PX_IS_DPS              (1 << 5)  /* Descriptor Processed */
#define AHCI_PX_IS_PCS              (1 << 6)  /* Port Change Status */
#define AHCI_PX_IS_DMPS             (1 << 7)  /* Device Mechanical Presence */
#define AHCI_PX_IS_PRCS             (1 << 22) /* Port Register Clear Status */
#define AHCI_PX_IS_IFS              (1 << 27) /* Interface Not Ready */
#define AHCI_PX_IS_INFS             (1 << 26) /* FIS Notification */
#define AHCI_PX_IS_HBDS             (1 << 28) /* Host to Device Busy */
#define AHCI_PX_IS_HBFS             (1 << 29) /* Host to Device FIS */
#define AHCI_PX_IS_TFES             (1 << 30) /* Task File Error */
#define AHCI_PX_IS_CPDS             (1 << 31) /* Cold Port Detect Status */

/* Command Header Flags */
#define AHCI_CMD_CFL_MASK           0x1F      /* Command FIS Length */
#define AHCI_CMD_ATAPI              (1 << 5)  /* ATAPI Command */
#define AHCI_CMD_WRITE              (1 << 6)  /* Write Command */
#define AHCI_CMD_PREFETCH           (1 << 7)  /* Prefetchable */
#define AHCI_CMD_RESET              (1 << 8)  /* Reset Command */
#define AHCI_CMD_BIST               (1 << 9)  /* BIST Command */
#define AHCI_CMD_CLEAR_BUSY         (1 << 10) /* Clear Busy */
#define AHCI_CMD_R                  (1 << 11) /* Reserved */
#define AHCI_CMD_PMP_MASK           0xF000    /* Port Multiplier Port */
#define AHCI_CMD_PRDTL_MASK         0xFFFF0000 /* PRDT Length */

/* Command Status */
#define AHCI_CMD_STS_C              (1 << 31) /* Command Complete */

/* PRDT Entry */
#define AHCI_PRDT_DBC_MASK          0x3FFFFF  /* Data Byte Count */
#define AHCI_PRDT_I                 (1 << 31) /* Interrupt on Completion */

/* FIS Types */
#define AHCI_FIS_TYPE_REG_H2D       0x27    /* Register FIS - Host to Device */
#define AHCI_FIS_TYPE_REG_D2H       0x34    /* Register FIS - Device to Host */
#define AHCI_FIS_TYPE_DMA_ACT       0x39    /* DMA Activate FIS */
#define AHCI_FIS_TYPE_DMA_SETUP     0x41    /* DMA Setup FIS */
#define AHCI_FIS_TYPE_DATA          0x46    /* Data FIS */
#define AHCI_FIS_TYPE_BIST          0x58    /* BIST Activate FIS */
#define AHCI_FIS_TYPE_PIO_SETUP     0x5F    /* PIO Setup FIS */
#define AHCI_FIS_TYPE_DEV_BITS      0xA1    /* Set Device Bits FIS */

/* FIS Control Flags */
#define AHCI_FIS_CTRL_C             (1 << 0)  /* Command FIS */
#define AHCI_FIS_CTRL_R             (1 << 1)  /* Read/Write */

/* ATA Commands */
#define ATA_CMD_READ_DMA_EXT        0x25    /* Read DMA Extended */
#define ATA_CMD_WRITE_DMA_EXT       0x35    /* Write DMA Extended */
#define ATA_CMD_READ_FPDMA_QUEUED   0x60    /* Read FPDMA Queued (NCQ) */
#define ATA_CMD_WRITE_FPDMA_QUEUED  0x61    /* Write FPDMA Queued (NCQ) */
#define ATA_CMD_READ_NATIVE_MAX_EXT 0x27    /* Read Native Max Extended */
#define ATA_CMD_IDENTIFY            0xEC    /* Identify Device */
#define ATA_CMD_SMART               0xB0    /* SMART */
#define ATA_CMD_SET_FEATURES        0xEF    /* Set Features */
#define ATA_CMD_FLUSH_EXT           0xEA    /* Flush Extended */
#define ATA_CMD_CHECK_POWER         0xE5    /* Check Power Mode */

/* SMART Subcommands */
#define ATA_SMART_READ_DATA         0xD0    /* Read SMART Data */
#define ATA_SMART_READ_THRESHOLDS   0xD1    /* Read SMART Thresholds */
#define ATA_SMART_ENABLE            0xD8    /* Enable SMART */
#define ATA_SMART_DISABLE           0xD9    /* Disable SMART */
#define ATA_SMART_STATUS            0xDA    /* Return SMART Status */

/* Device Detection */
#define AHCI_DEV_NONE               0
#define AHCI_DEV_SATA               1
#define AHCI_DEV_SEMB               2
#define AHCI_DEV_PM                 3
#define AHCI_DEV_SATAPI             4

/* Transfer Modes */
#define AHCI_MODE_PIO               0
#define AHCI_MODE_DMA               1
#define AHCI_MODE_UDMA              2
#define AHCI_MODE_NCQ               3

/* Power States */
#define AHCI_POWER_ACTIVE           0
#define AHCI_POWER_PARTIAL          1
#define AHCI_POWER_SLUMBER          2
#define AHCI_POWER_DEVSLP           3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * AHCI Command FIS (Register FIS - Host to Device)
 */
typedef struct __packed {
    u8 fis_type;              /* FIS Type (0x27) */
    u8 port_multiplier;       /* Port Multiplier */
    u8 command;               /* Command Register */
    u8 features_low;          /* Features Register (7:0) */
    u32 lba_low;              /* LBA (27:0) */
    u8 lba_high;              /* LBA (47:32) */
    u8 features_high;         /* Features Register (15:8) */
    u16 count;                /* Sector Count */
    u8 control;               /* Device Control Register */
    u8 reserved[3];           /* Reserved */
    u8 reserved2[4];          /* Reserved for NCQ */
} ahci_cmd_fis_t;

/**
 * AHCI PRDT Entry
 */
typedef struct __packed {
    u64 dba;                  /* Data Base Address */
    u32 reserved;             /* Reserved */
    u32 dbc;                  /* Data Byte Count */
} ahci_prdt_entry_t;

/**
 * AHCI Command Header
 */
typedef struct __packed {
    u32 flags;                /* Command Flags */
    u32 prdtl;                /* PRDT Length */
    u32 prdbc;                /* PRDT Bytes Transferred */
    u32 ctba;                 /* Command Table Base Address (Low 32 bits) */
    u32 reserved[4];          /* Reserved */
} ahci_cmd_header_t;

/**
 * AHCI Command Table
 */
typedef struct __packed {
    ahci_cmd_fis_t cfis;      /* Command FIS (64 bytes) */
    u8 acmd[16];              /* ATAPI Command (16 bytes) */
    u8 reserved[48];          /* Reserved */
    ahci_prdt_entry_t prdt[]; /* PRDT Entries */
} ahci_cmd_table_t;

/**
 * AHCI Received FIS
 */
typedef struct __packed {
    u8 dsfis[28];             /* DMA Setup FIS */
    u8 reserved[4];
    u8 psfis[20];             /* PIO Setup FIS */
    u8 reserved2[12];
    u8 rfis[20];              /* Register FIS */
    u8 reserved3[4];
    u8 sdbfis[8];             /* Set Device Bits FIS */
    u8 ufis[64];              /* Unknown FIS */
    u8 reserved4[96];
} ahci_received_fis_t;

/**
 * AHCI Port
 */
typedef struct {
    u32 port_id;              /* Port ID */
    u32 port_num;             /* Port Number */
    bool is_present;          /* Is Port Present */
    bool is_active;           /* Is Port Active */
    u32 device_type;          /* Device Type */
    u32 transfer_mode;        /* Transfer Mode */
    
    /* Register Addresses */
    volatile u32 *clb;        /* Command List Base */
    volatile u32 *fb;         /* FIS Base */
    phys_addr_t clb_phys;     /* Command List Physical */
    phys_addr_t fb_phys;      /* FIS Physical */
    
    /* Command Structures */
    ahci_cmd_header_t *cmd_list;  /* Command List */
    ahci_cmd_table_t *cmd_table;  /* Command Table */
    ahci_received_fis_t *fis;     /* Received FIS */
    
    /* Device Information */
    char model[41];           /* Model Number */
    char serial[21];          /* Serial Number */
    char firmware[9];         /* Firmware Revision */
    u64 size;                 /* Size in bytes */
    u32 block_size;           /* Block Size */
    u64 blocks;               /* Number of Blocks */
    u16 cylinders;            /* Cylinders */
    u16 heads;                /* Heads */
    u16 sectors_per_track;    /* Sectors per Track */
    
    /* SMART Data */
    u8 smart_enabled;         /* SMART Enabled */
    u8 smart_status;          /* SMART Status */
    u8 smart_attributes[512]; /* SMART Attributes */
    u16 smart_thresholds[512];/* SMART Thresholds */
    u32 temperature;          /* Temperature (Celsius) */
    u32 power_on_hours;       /* Power On Hours */
    u32 power_cycle_count;    /* Power Cycle Count */
    u64 host_reads;           /* Host Reads */
    u64 host_writes;          /* Host Writes */
    u64 read_bytes;           /* Bytes Read */
    u64 write_bytes;          /* Bytes Written */
    u32 error_count;          /* Error Count */
    
    /* State */
    bool is_initialized;      /* Is Port Initialized */
    bool has_error;           /* Has Error */
    u32 last_error;           /* Last Error Code */
    
    /* Synchronization */
    spinlock_t lock;          /* Port Lock */
} ahci_port_t;

/**
 * AHCI Controller
 */
typedef struct ahci_controller {
    u32 controller_id;        /* Controller ID */
    char name[32];            /* Controller Name */
    
    /* PCI Information */
    u16 vendor_id;            /* PCI Vendor ID */
    u16 device_id;            /* PCI Device ID */
    u16 subsystem_vendor_id;  /* PCI Subsystem Vendor ID */
    u16 subsystem_id;         /* PCI Subsystem ID */
    u8 revision_id;           /* PCI Revision ID */
    u8 programming_interface; /* Programming Interface */
    
    /* MMIO */
    volatile void *mmio;      /* MMIO Base Address */
    u64 mmio_size;            /* MMIO Region Size */
    
    /* Capabilities */
    u32 cap;                  /* Host Capabilities */
    u32 cap2;                 /* Host Capabilities Extended */
    u32 pi;                   /* Ports Implemented */
    u32 version;              /* AHCI Version */
    u32 num_ports;            /* Number of Ports */
    u32 port_start;           /* Port Start Index */
    
    /* Ports */
    ahci_port_t *ports;       /* Port Array */
    u32 active_ports;         /* Number of Active Ports */
    
    /* Controller State */
    bool is_initialized;      /* Is Controller Initialized */
    bool ahci_enabled;        /* Is AHCI Enabled */
    bool is_reset;            /* Is Controller Reset */
    
    /* Statistics */
    u64 total_reads;          /* Total Read Operations */
    u64 total_writes;         /* Total Write Operations */
    u64 total_read_bytes;     /* Total Bytes Read */
    u64 total_write_bytes;    /* Total Bytes Written */
    u64 total_errors;         /* Total Errors */
    
    /* Synchronization */
    spinlock_t lock;          /* Controller Lock */
    
    /* Links */
    struct ahci_controller *next; /* Next Controller */
} ahci_controller_t;

/**
 * AHCI Driver State
 */
typedef struct {
    bool initialized;         /* Is Driver Initialized */
    u32 controller_count;     /* Number of Controllers */
    ahci_controller_t *controllers; /* Controller List */
    spinlock_t lock;          /* Driver Lock */
    
    /* Statistics */
    u64 total_controllers;    /* Total Controllers Detected */
    u64 total_ports;          /* Total Ports Detected */
    u64 total_devices;        /* Total Devices Detected */
} ahci_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int ahci_init(void);
int ahci_shutdown(void);
bool ahci_is_initialized(void);

/* Controller operations */
int ahci_probe(void);
int ahci_add_controller(ahci_controller_t *ctrl);
int ahci_remove_controller(u32 controller_id);
ahci_controller_t *ahci_get_controller(u32 controller_id);
ahci_controller_t *ahci_get_controller_by_name(const char *name);
int ahci_list_controllers(ahci_controller_t *ctrls, u32 *count);

/* Controller initialization */
int ahci_controller_init(ahci_controller_t *ctrl);
int ahci_controller_reset(ahci_controller_t *ctrl);
int ahci_controller_enable(ahci_controller_t *ctrl);
int ahci_controller_disable(ahci_controller_t *ctrl);

/* Port operations */
int ahci_port_init(ahci_controller_t *ctrl, u32 port_num);
int ahci_port_start(ahci_controller_t *ctrl, u32 port_num);
int ahci_port_stop(ahci_controller_t *ctrl, u32 port_num);
int ahci_port_reset(ahci_controller_t *ctrl, u32 port_num);
ahci_port_t *ahci_get_port(ahci_controller_t *ctrl, u32 port_num);
int ahci_port_detect_device(ahci_controller_t *ctrl, u32 port_num);

/* Device operations */
int ahci_identify_device(ahci_port_t *port);
int ahci_get_device_info(ahci_port_t *port);
int ahci_set_transfer_mode(ahci_port_t *port, u32 mode);

/* I/O operations */
int ahci_read_sector(ahci_port_t *port, u64 lba, u32 count, void *buffer);
int ahci_write_sector(ahci_port_t *port, u64 lba, u32 count, const void *buffer);
int ahci_read_dma(ahci_port_t *port, u64 lba, u32 count, void *buffer);
int ahci_write_dma(ahci_port_t *port, u64 lba, u32 count, const void *buffer);
int ahci_flush_cache(ahci_port_t *port);

/* SMART operations */
int ahci_smart_enable(ahci_port_t *port);
int ahci_smart_disable(ahci_port_t *port);
int ahci_smart_get_status(ahci_port_t *port);
int ahci_smart_read_data(ahci_port_t *port);
int ahci_smart_read_thresholds(ahci_port_t *port);
bool ahci_is_smart_healthy(ahci_port_t *port);

/* Power management */
int ahci_set_power_state(ahci_controller_t *ctrl, u32 state);
int ahci_port_sleep(ahci_port_t *port);
int ahci_port_wake(ahci_port_t *port);

/* Block device interface */
int ahci_block_read(u32 controller_id, u32 port, u64 lba, u32 count, void *buffer);
int ahci_block_write(u32 controller_id, u32 port, u64 lba, u32 count, const void *buffer);
int ahci_block_flush(u32 controller_id, u32 port);
u64 ahci_block_get_size(u32 controller_id, u32 port);
u32 ahci_block_get_block_size(u32 controller_id, u32 port);

/* Utility functions */
const char *ahci_get_device_type_name(u32 type);
const char *ahci_get_transfer_mode_name(u32 mode);
const char *ahci_get_status_string(u32 status);
void ahci_print_controller_info(ahci_controller_t *ctrl);
void ahci_print_port_info(ahci_port_t *port);
void ahci_print_smart_data(ahci_port_t *port);

/* Global instance */
ahci_driver_t *ahci_get_driver(void);

#endif /* _AHCI_H */
