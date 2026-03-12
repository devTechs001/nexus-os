/*
 * NEXUS OS - NVMe Storage Driver
 * drivers/storage/nvme.h
 *
 * Complete NVMe 1.4 compliant driver with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NVME_H
#define _NVME_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         NVME SPECIFICATION CONSTANTS                      */
/*===========================================================================*/

/* NVMe Specification Version */
#define NVME_VERSION_MAJOR          1
#define NVME_VERSION_MINOR          4
#define NVME_VERSION                ((NVME_VERSION_MAJOR << 16) | NVME_VERSION_MINOR)

/* PCI IDs */
#define NVME_PCI_CLASS              0x010802  /* NVM Express */
#define NVME_PCI_CLASS_MASK         0xFFFFFF

/* Controller Registers */
#define NVME_REG_CAP                0x00    /* Controller Capabilities */
#define NVME_REG_CC                 0x14    /* Controller Configuration */
#define NVME_REG_CSTS               0x1C    /* Controller Status */
#define NVME_REG_NSSR               0x20    /* NVM Subsystem Reset */
#define NVME_REG_AQA                0x24    /* Admin Queue Attributes */
#define NVME_REG_ASQ                0x28    /* Admin SQ Base Address */
#define NVME_REG_ACQ                0x30    /* Admin CQ Base Address */
#define NVME_REG_CMBLOC             0x38    /* Controller Memory Buffer Location */
#define NVME_REG_CMBSZ              0x3C    /* Controller Memory Buffer Size */
#define NVME_REG_BPINFO             0x40    /* Boot Partition Information */
#define NVME_REG_BPRSEL             0x44    /* Boot Partition Read Select */
#define NVME_REG_BPMBL              0x48    /* Boot Partition Memory Buffer Location */
#define NVME_REG_CMBMSC             0x50    /* Controller Memory Buffer Memory Space Control */
#define NVME_REG_PMRCAP             0xE00   /* Persistent Memory Region Capabilities */
#define NVME_REG_PMRCTL             0xE04   /* Persistent Memory Region Control */
#define NVME_REG_PMRSTS             0xE08   /* Persistent Memory Region Status */
#define NVME_REG_PMREBS             0xE0C   /* Persistent Memory Region Elasticity Buffer Size */
#define NVME_REG_PMRSWTP            0xE10   /* Persistent Memory Region Sustained Write Throughput */
#define NVME_REG_PMRCMSC            0xE14   /* Persistent Memory Region Controller Memory Space Control */
#define NVME_REG_DBS                0x1000  /* Doorbell Space Base */

/* Doorbell stride */
#define NVME_DOORBELL_STRIDE        4

/* Controller Configuration (CC) */
#define NVME_CC_EN                  (1 << 0)    /* Enable */
#define NVME_CC_CSS_SHIFT           4           /* I/O Completion Queue Entry Size */
#define NVME_CC_CSS_MASK            (7 << NVME_CC_CSS_SHIFT)
#define NVME_CC_MPS_SHIFT           7           /* Memory Page Size */
#define NVME_CC_MPS_MASK            (0xF << NVME_CC_MPS_SHIFT)
#define NVME_CC_AMS_SHIFT           11          /* Arbitration Mechanism Selected */
#define NVME_CC_AMS_MASK            (7 << NVME_CC_AMS_SHIFT)
#define NVME_CC_SHN_SHIFT           14          /* Shutdown Notification */
#define NVME_CC_SHN_MASK            (3 << NVME_CC_SHN_SHIFT)
#define NVME_CC_SHN_NONE            0
#define NVME_CC_SHN_NORMAL          1
#define NVME_CC_SHN_ABRUPT          2
#define NVME_CC_IOSQES_SHIFT        16          /* I/O Submission Queue Entry Size */
#define NVME_CC_IOSQES_MASK         (0xF << NVME_CC_IOSQES_SHIFT)
#define NVME_CC_IOCQES_SHIFT        20          /* I/O Completion Queue Entry Size */
#define NVME_CC_IOCQES_MASK         (0xF << NVME_CC_IOCQES_SHIFT)

/* Controller Status (CSTS) */
#define NVME_CSTS_RDY               (1 << 0)    /* Ready */
#define NVME_CSTS_CFS               (1 << 1)    /* Controller Fatal Status */
#define NVME_CSTS_NSSRO             (1 << 2)    /* NVM Subsystem Reset Occurred */
#define NVME_CSTS_PP                (1 << 3)    /* Processing Paused */
#define NVME_CSTS_SHST_SHIFT        2           /* Shutdown Status */
#define NVME_CSTS_SHST_MASK         (3 << NVME_CSTS_SHST_SHIFT)
#define NVME_CSTS_SHST_NORMAL       0
#define NVME_CSTS_SHST_OCCURRING    1
#define NVME_CSTS_SHST_COMPLETE     2
#define NVME_CSTS_SHST_NOT_ALLOWED  3

/* Controller Capabilities (CAP) */
#define NVME_CAP_MQES_SHIFT         0
#define NVME_CAP_MQES_MASK          (0xFFFF << NVME_CAP_MQES_SHIFT)
#define NVME_CAP_TO                 (1 << 16)   /* Timeout */
#define NVME_CAP_DSTRD_SHIFT        32          /* Doorbell Stride */
#define NVME_CAP_DSTRD_MASK         (0xF << NVME_CAP_DSTRD_SHIFT)
#define NVME_CAP_NSSRS              (1 << 36)   /* NVM Subsystem Reset Supported */
#define NVME_CAP_MSS                (1 << 37)   /* Memory Space Supported */
#define NVME_CAP_CSS_SHIFT          40          /* Command Sets Supported */
#define NVME_CAP_CSS_MASK           (0xFF << NVME_CAP_CSS_SHIFT)
#define NVME_CAP_BPS                (1 << 48)   /* Boot Partition Support */
#define NVME_CAP_PMRS               (1 << 56)   /* Persistent Memory Region Supported */
#define NVME_CAP_CMBS               (1 << 57)   /* Controller Memory Buffer Supported */

/* Admin Queue Attributes (AQA) */
#define NVME_AQA_ASQS_SHIFT         0           /* Admin Submission Queue Size */
#define NVME_AQA_ASQS_MASK          (0xFFF << NVME_AQA_ASQS_SHIFT)
#define NVME_AQA_ACQS_SHIFT         16          /* Admin Completion Queue Size */
#define NVME_AQA_ACQS_MASK          (0xFFF << NVME_AQA_ACQS_SHIFT)

/* Namespace Identifier */
#define NVME_NSID_BROADCAST         0xFFFFFFFF
#define NVME_NSID_NONE              0

/* Opcodes */
#define NVME_OPC_FLUSH              0x00
#define NVME_OPC_WRITE              0x01
#define NVME_OPC_READ               0x02
#define NVME_OPC_WRITE_UNCORRECTABLE 0x04
#define NVME_OPC_COMPARE            0x05
#define NVME_OPC_WRITE_ZEROES       0x08
#define NVME_OPC_DATASET_MANAGEMENT 0x09
#define NVME_OPC_RESERVATION_REGISTER 0x0D
#define NVME_OPC_RESERVATION_REPORT 0x0E
#define NVME_OPC_RESERVATION_ACQUIRE 0x11
#define NVME_OPC_RESERVATION_RELEASE 0x15

/* Admin Opcodes */
#define NVME_ADM_OPC_DELETE_SQ      0x00
#define NVME_ADM_OPC_CREATE_SQ      0x01
#define NVME_ADM_OPC_GET_LOG_PAGE   0x02
#define NVME_ADM_OPC_DELETE_CQ      0x04
#define NVME_ADM_OPC_CREATE_CQ      0x05
#define NVME_ADM_OPC_IDENTIFY       0x06
#define NVME_ADM_OPC_ABORT          0x08
#define NVME_ADM_OPC_SET_FEATURES   0x09
#define NVME_ADM_OPC_GET_FEATURES   0x0A
#define NVME_ADM_OPC_ASYNC_EVENT    0x0C
#define NVME_ADM_OPC_NS_MANAGEMENT  0x0D
#define NVME_ADM_OPC_FW_COMMIT      0x10
#define NVME_ADM_OPC_FW_DOWNLOAD    0x11
#define NVME_ADM_OPC_NS_ATTACH      0x15
#define NVME_ADM_OPC_KEEP_ALIVE     0x18
#define NVME_ADM_OPC_DIRECTIVE_SEND 0x19
#define NVME_ADM_OPC_DIRECTIVE_RECV 0x1A
#define NVME_ADM_OPC_VIRTUAL_MGMT   0x1C
#define NVME_ADM_OPC_NVME_SEND      0x1D
#define NVME_ADM_OPC_NVME_RECV      0x1E
#define NVME_ADM_OPC_FABRIC         0x1F
#define NVME_ADM_OPC_FORMAT         0x80
#define NVME_ADM_OPC_SECURITY_SEND  0x81
#define NVME_ADM_OPC_SECURITY_RECV  0x82

/* Identify Commands */
#define NVME_IDENTIFY_CNS_NS        0x00    /* Identify Namespace */
#define NVME_IDENTIFY_CNS_CTRL      0x01    /* Identify Controller */
#define NVME_IDENTIFY_CNS_NS_LIST   0x02    /* Identify Namespace List */
#define NVME_IDENTIFY_CNS_NS_DESC   0x03    /* Identify Namespace Description */
#define NVME_IDENTIFY_CNS_CSI_NS    0x05    /* Identify CSI Namespace */
#define NVME_IDENTIFY_CNS_CSI_CTRL  0x06    /* Identify CSI Controller */
#define NVME_IDENTIFY_CNS_NS_PRESENT 0x11   /* Identify Namespace Present */
#define NVME_IDENTIFY_CNS_CTRL_LIST 0x12    /* Identify Controller List */
#define NVME_IDENTIFY_CNS_PRIMARY_CTRL 0x13 /* Identify Primary Controller */
#define NVME_IDENTIFY_CNS_SEC_CTRL_LIST 0x14 /* Identify Secondary Controller List */
#define NVME_IDENTIFY_CNS_NS_GRANULARITY 0x15 /* Identify Namespace Granularity */
#define NVME_IDENTIFY_CNS_UUID_LIST 0x16    /* Identify UUID List */

/* Log Page Identifiers */
#define NVME_LOG_ERROR              0x01    /* Error Information */
#define NVME_LOG_SMART              0x02    /* SMART/Health Information */
#define NVME_LOG_FW_SLOT            0x03    /* Firmware Slot Information */
#define NVME_LOG_CHANGED_NS         0x04    /* Changed Namespace List */
#define NVME_LOG_CMD_EFFECTS        0x05    /* Command Effects Log */
#define NVME_LOG_DEVICE_SELF_TEST   0x06    /* Device Self Test */
#define NVME_LOG_TELEMETRY_HOST     0x07    /* Telemetry Host-Initiated */
#define NVME_LOG_TELEMETRY_CTRL     0x08    /* Telemetry Controller-Initiated */
#define NVME_LOG_ENDURANCE_GROUP    0x09    /* Endurance Group Information */
#define NVME_LOG_PREDICTABLE_LAT    0x0A    /* Predictable Latency */
#define NVME_LOG_ANA                0x0C    /* Asymmetric Namespace Access */
#define NVME_LOG_PERSISTENT_EVENT   0x0D    /* Persistent Event Log */
#define NVME_LOG_LBA_STATUS         0x0E    /* LBA Status Information */
#define NVME_LOG_ENDURANCE_GROUP_EVT 0x0F   /* Endurance Group Event Aggregate */
#define NVME_LOG_MEDIA_UNIT_STATUS  0x10    /* Media Unit Status */
#define NVME_LOG_SUPPORTED_CAP      0x11    /* Supported Capabilities */
#define NVME_LOG_RESERVATION        0x12    /* Reservation Notification */
#define NVME_LOG_SANITIZE           0x13    /* Sanitize Status */
#define NVME_LOG_ZONED_STARTING_LBA 0x14    /* Zoned Namespace Starting LBA */
#define NVME_LOG_DISC               0x70    /* Discovery Log */
#define NVME_LOG_RESERVATION_ACT    0x80    /* Reservation Action Log */
#define NVME_LOG_SANITIZE_STATUS    0x81    /* Sanitize Status Log */
#define NVME_LOG_SUPPORTED_LOG      0xFF    /* Supported Log Pages */

/* Feature Identifiers */
#define NVME_FEAT_ARBITRATION       0x01    /* Arbitration */
#define NVME_FEAT_POWER_MGMT        0x02    /* Power Management */
#define NVME_FEAT_LBA_RANGE         0x03    /* LBA Range Type */
#define NVME_FEAT_TEMP_THRESH       0x04    /* Temperature Threshold */
#define NVME_FEAT_ERROR_RECOVERY    0x05    /* Error Recovery */
#define NVME_FEAT_VOLATILE_WC       0x06    /* Volatile Write Cache */
#define NVME_FEAT_NUM_QUEUES        0x07    /* Number of Queues */
#define NVME_FEAT_IRQ_COALESCE      0x08    /* Interrupt Coalescing */
#define NVME_FEAT_IRQ_CONFIG        0x09    /* Interrupt Vector Configuration */
#define NVME_FEAT_WRITE_ATOMIC      0x0A    /* Write Atomicity Normal */
#define NVME_FEAT_ASYNC_EVENT       0x0B    /* Async Event Configuration */
#define NVME_FEAT_AUTO_PST          0x0C    /* Autonomous Power State Transition */
#define NVME_FEAT_HMEM              0x0D    /* Host Memory Buffer */
#define NVME_FEAT_KATO              0x0E    /* Keep Alive Timer */
#define NVME_FEAT_NOPSC             0x0F    /* Non-Operational Power State Config */
#define NVME_FEAT_RRL               0x10    /* Recovery Lockout */
#define NVME_FEAT_PLM_CONFIG        0x11    /* Predicatable Latency Mode Config */
#define NVME_FEAT_PLM_WINDOW        0x12    /* Predicatable Latency Mode Window */
#define NVME_FEAT_LBA_STS_INTERVAL  0x13    /* LBA Status Information Interval */
#define NVME_FEAT_HOST_BEHAVIOR     0x16    /* Host Behavior Support */
#define NVME_FEAT_SANITIZE          0x17    /* Sanitize */
#define NVME_FEAT_ENDURANCE         0x18    /* Endurance Group Event Configuration */
#define NVME_FEAT_IOCS_PROFILE      0x19    /* I/O Command Set Profile */
#define NVME_FEAT_SPINUP            0x1A    /* Spinup Control */
#define NVME_FEAT_SW_PROGRESS       0x80    /* Software Progress Marker */
#define NVME_FEAT_HOST_ID           0x81    /* Host Identifier */
#define NVME_FEAT_RESV_MASK         0x82    /* Reservation Notification Mask */
#define NVME_FEAT_RESV_PERSIST      0x83    /* Reservation Persist */
#define NVME_FEAT_WRITE_PROTECT     0x84    /* Namespace Write Protect Config */

/* Status Codes */
#define NVME_STATUS_SHIFT           1
#define NVME_STATUS_MASK            (0x7FF << NVME_STATUS_SHIFT)
#define NVME_STATUS_PHASE_SHIFT     8
#define NVME_STATUS_PHASE_MASK      (3 << NVME_STATUS_PHASE_SHIFT)
#define NVME_STATUS_PHASE_GENERIC     0
#define NVME_STATUS_PHASE_COMMAND   1
#define NVME_STATUS_PHASE_MEDIA     2

/* Generic Status Codes */
#define NVME_SC_SUCCESS             0x00    /* Successful Completion */
#define NVME_SC_INVALID_OPCODE      0x01    /* Invalid Command Opcode */
#define NVME_SC_INVALID_FIELD       0x02    /* Invalid Field in Command */
#define NVME_SC_CMDID_CONFLICT      0x03    /* Command ID Conflict */
#define NVME_SC_DATA_TRANSFER_ERROR 0x04    /* Data Transfer Error */
#define NVME_SC_ABORTED_POWER_LOSS  0x05    /* Commands Aborted due to Power Loss Notification */
#define NVME_SC_INTERNAL            0x06    /* Internal Error */
#define NVME_SC_ABORTED_BY_REQUEST  0x07    /* Command Abort Requested */
#define NVME_SC_ABORTED_SQ_DELETION 0x08    /* Command Aborted due to SQ Deletion */
#define NVME_SC_ABORTED_FAILED_FUSED 0x09   /* Command Aborted due to Failed Fused Command */
#define NVME_SC_ABORTED_MISSING_FUSED 0x0A  /* Command Aborted due to Missing Fused Command */
#define NVME_SC_INVALID_NSID        0x0B    /* Invalid Namespace or Format */
#define NVME_SC_CMD_SEQ_ERROR       0x0C    /* Command Sequence Error */
#define NVME_SC_INVALID_SGL_SEG     0x0D    /* Invalid SGL Segment Descriptor */
#define NVME_SC_INVALID_NUM_SGL     0x0E    /* Invalid Number of SGL Descriptors */
#define NVME_SC_DATA_SGL_INVALID    0x0F    /* Data SGL Offset Invalid */
#define NVME_SC_HOSTID_INCONSIST    0x10    /* Host Identifier Inconsistent Format */
#define NVME_SC_KEEP_ALIVE_EXPIRED  0x11    /* Keep Alive Timer Expired */
#define NVME_SC_KEEP_ALIVE_INVALID  0x12    /* Keep Alive Timeout Invalid */
#define NVME_SC_ABORTED_PREEMPT     0x13    /* Command Aborted due to Preempt and Abort */
#define NVME_SC_SANITIZE_FAILED     0x14    /* Sanitize Failed */
#define NVME_SC_SANITIZE_IN_PROG    0x15    /* Sanitize In Progress */
#define NVME_SC_SGL_INVALID_TYPE    0x16    /* SGL Descriptor Type Invalid */
#define NVME_SC_CQ_INVALID_SIZE     0x17    /* Completion Queue Invalid Size */
#define NVME_SC_CMD_INVALID_SIZE    0x18    /* Command Invalid Size */
#define NVME_SC_QUEUE_INVALID_SIZE  0x19    /* I/O Completion Queue Not Ready */
#define NVME_SC_FQ_INVALID_SIZE     0x1A    /* Feature Identifier Not Supported */
#define NVME_SC_NAMESPACE_NOT_READY 0x1B    /* Namespace Not Ready */
#define NVME_SC_RESERVATION_CONFLICT 0x1C   /* Reservation Conflict */
#define NVME_SC_FORMAT_IN_PROGRESS  0x1D    /* Format In Progress */
#define NVME_SC_INSUFFICIENT_CAP    0x1E    /* Insufficient Capacity */
#define NVME_SC_NAMESPACE_UNAVAILABLE 0x1F  /* Namespace Unavailable */
#define NVME_SC_DEVICE_OVERLOAD     0x20    /* Device Overload */
#define NVME_SC_CMD_CANCELLED       0x21    /* Command Cancelled */
#define NVME_SC_INVALID_CTRL_ID     0x22    /* Invalid Controller Identifier */
#define NVME_SC_INVALID_SEC_CTRL    0x23    /* Invalid Secondary Controller State */
#define NVME_SC_INVALID_NUM_RESOURCES 0x24  /* Invalid Number of Controller Resources */
#define NVME_SC_INVALID_RESOURCE_ID 0x25    /* Invalid Resource Identifier */
#define NVME_SC_PMR_SAN_PROHIBITED  0x26    /* Sanitize Prohibited While PMR Enabled */
#define NVME_SC_ANA_GROUP_NOT_FOUND 0x27    /* ANA Group Identifier Not Found */
#define NVME_SC_KEY_CONFLICT        0x28    /* Key Conflict */
#define NVME_SC_PROTECTION_INFO     0x29    /* Protection Information Check Failed */
#define NVME_SC_SGL_INVALID_OFFSET  0x2A    /* SGL Offset Invalid */
#define NVME_SC_HOSTID_UNSUPPORTED  0x2B    /* Host Identifier Unsupported */
#define NVME_SC_KEEP_ALIVE_REQUIRED 0x2C    /* Keep Alive Required */
#define NVME_SC_ABORTED_LIMIT_REACHED 0x2D  /* Abort Command Limit Reached */
#define NVME_SC_CMD_CANCELLED_BY_PREEMPT 0x2E /* Command Cancelled by Preempt */
#define NVME_SC_INVALID_PLACEMENT   0x2F    /* Invalid Placement */
#define NVME_SC_MEDIUM_NOT_READY    0x30    /* Medium Not Ready */
#define NVME_SC_RESERVATION_PREEMPT 0x31    /* Reservation Preempted */
#define NVME_SC_RESERVATION_RELEASE 0x32    /* Reservation Released */
#define NVME_SC_REGISTRATION_PREEMPT 0x33   /* Registration Preempted */
#define NVME_SC_ZONE_IS_FULL      0xB7      /* Zone Is Full */
#define NVME_SC_ZONE_IS_OFFLINE   0xB8      /* Zone Is Offline */
#define NVME_SC_ZONE_IS_WRITELOCKED 0xB9    /* Zone Is Write Locked */
#define NVME_SC_ZONE_IS_READONLY  0xBA      /* Zone Is Read Only */
#define NVME_SC_ZONE_BOUNDARY_ERROR 0xBB    /* Zone Boundary Error */
#define NVME_SC_ZONE_FULL         0xBC      /* Zone Full */
#define NVME_SC_ZONE_RESET        0xBD      /* Zone Reset */
#define NVME_SC_ZONE_OPEN         0xBE      /* Zone Open */
#define NVME_SC_ZONE_CLOSE        0xBF      /* Zone Close */
#define NVME_SC_ZONE_FINISH       0xC0      /* Zone Finish */
#define NVME_SC_ZONE_APPEND_LIMIT 0xC1      /* Zone Append Limit */
#define NVME_SC_ZONE_INVALID_WRITE 0xC2     /* Zone Invalid Write */
#define NVME_SC_ZONE_TOO_MANY_OPEN 0xC3     /* Zone Too Many Open */
#define NVME_SC_ZONE_INVAL_STATE  0xC4      /* Zone Invalid State */
#define NVME_SC_ZONE_INVAL_TRANS  0xC5      /* Zone Invalid Transition */
#define NVME_SC_ZONE_TRY_AGAIN    0xC6      /* Zone Try Again */

/* Queue Flags */
#define NVME_Q_FLAG_PC              (1 << 0)    /* Physically Contiguous */
#define NVME_Q_FLAG_IEN             (1 << 1)    /* Interrupt Enabled */

/* SMART/Health Log Page */
#define NVME_SMART_CRITICAL         (1 << 0)    /* Critical Warning */
#define NVME_SMART_TEMP_EXCEEDED    (1 << 1)    /* Temperature Threshold Exceeded */
#define NVME_SMART_AVAILABLE_SPARE  (1 << 2)    /* Available Space Below Threshold */
#define NVME_SMART_READONLY         (1 << 3)    /* NVM Subsystem Read Only */
#define NVME_SMART_VOLATILE_BACKUP  (1 << 4)    /* Volatile Memory Backup Device Has Failed */
#define NVME_SMART_PMR_READONLY     (1 << 5)    /* Persistent Memory Region Has Become Read-Only */

/* Temperature */
#define NVME_TEMP_CRITICAL          80          /* 80°C */
#define NVME_TEMP_WARNING           70          /* 70°C */
#define NVME_TEMP_NORMAL            60          /* 60°C */

/* NVMe Command Structure */
#define NVME_COMMAND_SIZE           64          /* bytes */
#define NVME_COMPLETION_SIZE        16          /* bytes */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * NVMe Command (Submission Queue Entry)
 */
typedef struct __packed {
    union {
        struct {
            u16 opcode;         /* Opcode */
            u16 flags;          /* Flags */
            u16 command_id;     /* Command Identifier */
        };
        u32 dword_0;
    };
    u32 nsid;                   /* Namespace Identifier */
    u32 cdw2;                   /* Command Dword 2 */
    u32 cdw3;                   /* Command Dword 3 */
    u64 metadata;               /* Metadata Pointer */
    union {
        struct {
            u64 prp1;           /* PRP Entry 1 */
            u64 prp2;           /* PRP Entry 2 */
        };
        struct {
            u64 sgl1;           /* SGL Entry 1 */
            u64 sgl2;           /* SGL Entry 2 */
        };
    };
    u32 cdw10;                  /* Command Dword 10 */
    u32 cdw11;                  /* Command Dword 11 */
    u32 cdw12;                  /* Command Dword 12 */
    u32 cdw13;                  /* Command Dword 13 */
    u32 cdw14;                  /* Command Dword 14 */
    u32 cdw15;                  /* Command Dword 15 */
} nvme_command_t;

/**
 * NVMe Completion (Completion Queue Entry)
 */
typedef struct __packed {
    union {
        struct {
            u32 dword_0;
            u32 reserved;
        };
        struct {
            u16 status;         /* Status Field */
            u16 sq_head;        /* Submission Queue Head Pointer */
            u16 sq_id;          /* Submission Queue Identifier */
            u16 command_id;     /* Command Identifier */
        };
    };
    u32 cdw3;                   /* Command Specific Information */
    u64 result;                 /* Command Specific Result */
} nvme_completion_t;

/**
 * NVMe Identify Controller Data
 */
typedef struct __packed {
    /* Controller Information */
    u16 vid;                    /* PCI Vendor ID */
    u16 ssvid;                  /* PCI Subsystem Vendor ID */
    char sn[20];                /* Serial Number */
    char mn[40];                /* Model Number */
    char fr[8];                 /* Firmware Revision */
    u8 rab;                     /* Recommended Arbitration Burst */
    u8 ieee[3];                 /* IEEE OUI Identifier */
    u8 cmic;                    /* Controller Multi-Path I/O and Namespace Sharing Capabilities */
    u8 mdts;                    /* Maximum Data Transfer Size */
    u16 cntlid;                 /* Controller ID */
    u32 ver;                    /* Version */
    u32 rtd3r;                  /* RTD3 Resume Latency */
    u32 rtd3e;                  /* RTD3 Entry Latency */
    u32 oaes;                   /* Optional Asynchronous Events Supported */
    u32 ctratt;                 /* Controller Attributes */
    u8 rsvd100[11];
    u8 cntrltype;               /* Controller Type */
    u8 fguid[16];               /* FRU Globally Unique Identifier */
    u16 crdt1;                  /* Command Retry Delay Time 1 */
    u16 crdt2;                  /* Command Retry Delay Time 2 */
    u16 crdt3;                  /* Command Retry Delay Time 3 */
    u8 rsvd134[122];
    
    /* Admin Command Set Attributes */
    u8 oacs;                    /* Optional Admin Command Support */
    u8 acl;                     /* Abort Command Limit */
    u8 aerl;                    /* Asynchronous Event Request Limit */
    u8 frmw;                    /* Firmware Updates */
    u8 lpa;                     /* Log Page Attributes */
    u8 elpe;                    /* Error Log Page Entries */
    u8 npss;                    /* Number of Power States Supported */
    u8 avscc;                   /* Admin Vendor Specific Command Configuration */
    u8 apsta;                   /* Autonomous Power State Transition Attributes */
    u16 wctemp;                 /* Warning Composite Temperature Threshold */
    u16 cctemp;                 /* Critical Composite Temperature Threshold */
    u16 mtfa;                   /* Maximum Time for Firmware Activation */
    u32 hmpre;                  /* Host Memory Buffer Preferred Size */
    u32 hmmin;                  /* Host Memory Buffer Minimum Size */
    u8 tnvmcap[16];             /* Total NVM Capacity */
    u8 unvmcap[16];             /* Unallocated NVM Capacity */
    u32 rpmbs;                  /* Replay Protected Memory Block Support */
    u16 edstt;                  /* Extended Device Self-test Time */
    u8 dsto;                    /* Device Self-test Options */
    u8 fwug;                    /* Firmware Update Granularity */
    u16 kas;                    /* Keep Alive Support */
    u16 hctma;                  /* Host Controlled Thermal Management Attributes */
    u16 mntmt;                  /* Minimum NVM Management Temperature */
    u16 mxtmt;                  /* Maximum NVM Management Temperature */
    u32 sanicap;                /* Sanitize Capabilities */
    u32 hmminds;                /* Host Memory Buffer Minimum Descriptor Size */
    u16 hmmaxd;                 /* Host Memory Buffer Maximum Descriptors */
    u16 nsetid;                 /* NVM Set Identifier Maximum */
    u16 cntlid;                 /* Controller ID Maximum */
    u8 rsvd340[172];
    
    /* I/O Command Set Attributes */
    u8 sqes;                    /* Submission Queue Entry Size */
    u8 cqes;                    /* Completion Queue Entry Size */
    u16 maxcmd;                 /* Maximum Outstanding Commands */
    u32 nn;                     /* Number of Namespaces */
    u16 oncs;                   /* Optional NVM Command Support */
    u16 fuses;                  /* Fused Operation Support */
    u8 fna;                     /* Format NVM Attributes */
    u8 vwc;                     /* Volatile Write Cache */
    u16 awun;                   /* Atomic Write Unit Normal */
    u16 awupf;                  /* Atomic Write Unit Power Fail */
    u8 nvscc;                   /* NVM Vendor Specific Command Configuration */
    u8 nwpc;                    /* Namespace Write Protection Capabilities */
    u16 acwu;                   /* Atomic Compare and Write Unit */
    u16 rsvd534;
    u32 sglsp;                  /* SGL Support */
    u8 rsvd540[40];
    u32 ioccsz;                 /* I/O Command Set Specific Size */
    u32 iorcsz;                 /* I/O Command Set Specific Size */
    u16 icdoff;                 /* I/O Command Set Specific Offset */
    u8 ctrattr;                 /* Controller Attributes */
    u8 msdbd;                   /* Maximum SGL Descriptor Block Entries */
    u8 rsvd596[244];
    
    /* Power State Descriptors */
    struct {
        u16 mp;                 /* Maximum Power */
        u8 rsvd0;
        u8 mps;                 /* Max Power Scale */
        u8 nps;                 /* Non-Operational Power State */
        u8 rsvd3[3];
        u32 enlat;              /* Entry Latency */
        u32 exlat;              /* Exit Latency */
        u8 rrt;                 /* Relative Read Throughput */
        u8 rrl;                 /* Relative Read Latency */
        u8 rwt;                 /* Relative Write Throughput */
        u8 rwl;                 /* Relative Write Latency */
        u8 rsvd16[16];
    } psd[32];
    
    u8 rsvd800[1024];
} nvme_identify_controller_t;

/**
 * NVMe Identify Namespace Data
 */
typedef struct __packed {
    u64 nsze;                   /* Namespace Size */
    u64 ncap;                   /* Namespace Capacity */
    u64 nuse;                   /* Namespace Utilization */
    u8 nsfeat;                  /* Namespace Features */
    u8 nlbaf;                   /* Number of LBA Formats */
    u8 flbas;                   /* Formatted LBA Size */
    u8 mc;                      /* Metadata Capabilities */
    u8 dpc;                     /* Data Protection Capabilities */
    u8 dps;                     /* Data Protection Settings */
    u8 nmic;                    /* Namespace Multi-Path I/O and Namespace Sharing Capabilities */
    u8 rescap;                  /* Reservation Capabilities */
    u8 fpi;                     /* Format Progress Indicator */
    u8 dlfeat;                  /* Deallocate Logical Block Features */
    u16 nawun;                  /* Namespace Atomic Write Unit Normal */
    u16 nawupf;                 /* Namespace Atomic Write Unit Power Fail */
    u16 nacwu;                  /* Namespace Atomic Compare and Write Unit */
    u16 nabsn;                  /* Namespace Atomic Boundary Size Normal */
    u16 nabo;                   /* Namespace Atomic Boundary Offset */
    u16 nabspf;                 /* Namespace Atomic Boundary Size Power Fail */
    u16 noiob;                  /* Namespace Optimal I/O Boundary */
    u8 nvmcap[16];              /* NVM Capacity */
    u16 npwg;                   /* Namespace Preferred Write Granularity */
    u16 npwa;                   /* Namespace Preferred Write Alignment */
    u16 npdg;                   /* Namespace Preferred Deallocate Granularity */
    u16 npda;                   /* Namespace Preferred Deallocate Alignment */
    u16 nows;                   /* Namespace Optimal Write Size */
    u16 rsvd50;
    u32 mssrl;                  /* Maximum Single Source Range Length */
    u32 mcl;                    /* Maximum Copy Length */
    u16 msrc;                   /* Maximum Source Range Count */
    u8 rsvd62[2];
    u32 anagrpid;               /* ANA Group Identifier */
    u8 rsvd68[3];
    u8 nsattr;                  /* Namespace Attributes */
    u16 nvmsetid;               /* NVM Set Identifier */
    u16 endgid;                 /* Endurance Group Identifier */
    u8 nguid[16];               /* Namespace Globally Unique Identifier */
    u8 eui64[8];                /* IEEE Extended Unique Identifier */
    
    /* LBA Format Descriptors */
    struct {
        u16 lbads;              /* LBA Data Size */
        u16 rp;                 /* Relative Performance */
    } lbaf[16];
    
    u8 rsvd192[192];
} nvme_identify_namespace_t;

/**
 * NVMe SMART/Health Information Log
 */
typedef struct __packed {
    u8 critical_warning;        /* Critical Warning */
    u16 temperature;            /* Composite Temperature */
    u8 avail_spare;             /* Available Spare */
    u8 spare_thresh;            /* Percentage Used */
    u8 percent_used;            /* Percentage Used */
    u8 rsvd8[26];
    u64 data_units_read[2];     /* Data Units Read */
    u64 data_units_written[2];  /* Data Units Written */
    u64 host_reads[2];          /* Host Read Commands */
    u64 host_writes[2];         /* Host Write Commands */
    u64 ctrl_busy_time[2];      /* Controller Busy Time */
    u64 power_cycles[2];        /* Power Cycles */
    u64 power_on_hours[2];      /* Power On Hours */
    u64 unsafe_shutdowns[2];    /* Unsafe Shutdowns */
    u64 media_errors[2];        /* Media and Data Integrity Errors */
    u64 num_err_log_entries[2]; /* Number of Error Information Log Entries */
    u32 warning_temp_time;      /* Warning Composite Temperature Time */
    u32 critical_comp_time;     /* Critical Composite Temperature Time */
    u16 temp_sensor[8];         /* Temperature Sensors */
    u8 rsvd216[296];
} nvme_smart_log_t;

/**
 * NVMe Error Information Log
 */
typedef struct __packed {
    u64 error_count;            /* Error Count */
    u16 sqid;                   /* Submission Queue Identifier */
    u16 cmdid;                  /* Command Identifier */
    u32 status_field;           /* Status Field */
    u16 param_loc;              /* Parameter Error Location */
    u64 lba;                    /* LBA */
    u32 nsid;                   /* Namespace Identifier */
    u8 vs;                      /* Vendor Specific */
    u8 rsvd41[3];
    u64 cs;                     /* Command Specific Information */
    u8 trtype;                  /* Transport Type */
    u8 rsvd57[7];
} nvme_error_log_t;

/**
 * NVMe Queue
 */
typedef struct {
    u16 id;                     /* Queue ID */
    u16 size;                   /* Queue Size (entries) */
    u16 head;                   /* Head Pointer */
    u16 tail;                   /* Tail Pointer */
    u16 phase;                  /* Phase Tag */
    
    void *sq;                   /* Submission Queue (virtual) */
    phys_addr_t sq_phys;        /* Submission Queue (physical) */
    
    void *cq;                   /* Completion Queue (virtual) */
    phys_addr_t cq_phys;        /* Completion Queue (physical) */
    
    bool is_admin;              /* Is Admin Queue */
    bool is_initialized;        /* Is Queue Initialized */
} nvme_queue_t;

/**
 * NVMe Namespace
 */
typedef struct {
    u32 nsid;                   /* Namespace ID */
    u64 size;                   /* Namespace Size (bytes) */
    u64 capacity;               /* Namespace Capacity (bytes) */
    u32 block_size;             /* Block Size (bytes) */
    u32 blocks;                 /* Number of Blocks */
    u8 format;                  /* LBA Format */
    bool is_active;             /* Is Namespace Active */
    
    nvme_identify_namespace_t identify; /* Namespace Identify Data */
} nvme_namespace_t;

/**
 * NVMe Device
 */
typedef struct nvme_device {
    u32 device_id;              /* Device ID */
    char name[32];              /* Device Name (e.g., nvme0) */
    
    /* PCI Information */
    u16 vendor_id;              /* PCI Vendor ID */
    u16 device_id_pci;          /* PCI Device ID */
    u16 subsystem_vendor_id;    /* PCI Subsystem Vendor ID */
    u16 subsystem_id;           /* PCI Subsystem ID */
    
    /* Controller Registers */
    volatile void *mmio;        /* MMIO Base Address */
    u64 mmio_size;              /* MMIO Region Size */
    
    /* Controller Information */
    nvme_identify_controller_t ctrl; /* Controller Identify Data */
    u32 controller_id;          /* Controller ID */
    u32 version;                /* NVMe Version */
    u32 max_qsize;              /* Maximum Queue Size */
    u32 doorbell_stride;        /* Doorbell Stride */
    
    /* Queues */
    nvme_queue_t admin_sq;      /* Admin Submission Queue */
    nvme_queue_t admin_cq;      /* Admin Completion Queue */
    nvme_queue_t *io_queues;    /* I/O Queues */
    u32 num_io_queues;          /* Number of I/O Queues */
    
    /* Namespaces */
    nvme_namespace_t *namespaces; /* Namespaces */
    u32 namespace_count;        /* Number of Namespaces */
    
    /* SMART/Health */
    nvme_smart_log_t smart;     /* SMART/Health Data */
    u64 last_smart_update;      /* Last SMART Update Time */
    u32 temperature;            /* Current Temperature (Kelvin) */
    u8 health_status;           /* Health Status */
    
    /* Device State */
    bool is_initialized;        /* Is Device Initialized */
    bool is_present;            /* Is Device Present */
    bool is_removed;            /* Is Device Removed */
    
    /* Statistics */
    u64 reads;                  /* Read Operations */
    u64 writes;                 /* Write Operations */
    u64 read_bytes;             /* Bytes Read */
    u64 write_bytes;            /* Bytes Written */
    u64 errors;                 /* Error Count */
    
    /* Synchronization */
    spinlock_t lock;            /* Device Lock */
    
    /* Links */
    struct nvme_device *next;   /* Next Device */
} nvme_device_t;

/**
 * NVMe Driver State
 */
typedef struct {
    bool initialized;           /* Is Driver Initialized */
    u32 device_count;           /* Number of Devices */
    nvme_device_t *devices;     /* Device List */
    spinlock_t lock;            /* Driver Lock */
    
    /* Statistics */
    u64 total_reads;            /* Total Read Operations */
    u64 total_writes;           /* Total Write Operations */
    u64 total_read_bytes;       /* Total Bytes Read */
    u64 total_write_bytes;      /* Total Bytes Written */
    u64 total_errors;           /* Total Errors */
} nvme_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int nvme_init(void);
int nvme_shutdown(void);
bool nvme_is_initialized(void);

/* Device management */
int nvme_probe(void);
int nvme_add_device(nvme_device_t *dev);
int nvme_remove_device(u32 device_id);
nvme_device_t *nvme_get_device(u32 device_id);
nvme_device_t *nvme_get_device_by_name(const char *name);
int nvme_list_devices(nvme_device_t *devices, u32 *count);

/* Controller operations */
int nvme_controller_init(nvme_device_t *dev);
int nvme_controller_enable(nvme_device_t *dev);
int nvme_controller_disable(nvme_device_t *dev);
int nvme_controller_reset(nvme_device_t *dev);
int nvme_identify_controller(nvme_device_t *dev);

/* Queue operations */
int nvme_create_admin_queues(nvme_device_t *dev);
int nvme_delete_admin_queues(nvme_device_t *dev);
int nvme_create_io_queues(nvme_device_t *dev, u32 count);
int nvme_delete_io_queues(nvme_device_t *dev);

/* Namespace operations */
int nvme_identify_namespaces(nvme_device_t *dev);
int nvme_get_namespace_info(nvme_device_t *dev, u32 nsid, nvme_namespace_t *ns);
int nvme_format_namespace(nvme_device_t *dev, u32 nsid, u8 format);

/* Admin commands */
int nvme_admin_identify(nvme_device_t *dev, u32 nsid, u64 addr, u32 cns);
int nvme_admin_create_sq(nvme_device_t *dev, nvme_queue_t *sq);
int nvme_admin_create_cq(nvme_device_t *dev, nvme_queue_t *cq);
int nvme_admin_delete_sq(nvme_device_t *dev, u16 qid);
int nvme_admin_delete_cq(nvme_device_t *dev, u16 qid);
int nvme_admin_get_features(nvme_device_t *dev, u32 fid, u32 *value);
int nvme_admin_set_features(nvme_device_t *dev, u32 fid, u32 value);
int nvme_admin_get_log_page(nvme_device_t *dev, u8 log_id, u64 addr, u32 len);

/* I/O commands */
int nvme_io_read(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb, void *buffer);
int nvme_io_write(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb, const void *buffer);
int nvme_io_write_zeroes(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb);
int nvme_io_flush(nvme_device_t *dev, u32 nsid);
int nvme_io_compare(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb, const void *buffer);

/* SMART/Health */
int nvme_get_smart_log(nvme_device_t *dev);
int nvme_get_error_log(nvme_device_t *dev, nvme_error_log_t *errors, u32 count);
int nvme_update_health(nvme_device_t *dev);
bool nvme_is_healthy(nvme_device_t *dev);
u32 nvme_get_temperature(nvme_device_t *dev);
u8 nvme_get_health_status(nvme_device_t *dev);

/* Block device interface */
int nvme_block_read(u32 device_id, u64 lba, u32 count, void *buffer);
int nvme_block_write(u32 device_id, u64 lba, u32 count, const void *buffer);
int nvme_block_flush(u32 device_id);
u64 nvme_block_get_size(u32 device_id);
u32 nvme_block_get_block_size(u32 device_id);

/* Utility functions */
const char *nvme_get_status_string(u16 status);
const char *nvme_get_opcode_string(u8 opcode);
void nvme_print_device_info(nvme_device_t *dev);
void nvme_print_smart_log(nvme_smart_log_t *smart);

/* Global instance */
nvme_driver_t *nvme_get_driver(void);

#endif /* _NVME_H */
