/*
 * NEXUS OS - SD/eMMC Storage Driver
 * drivers/storage/sd.h
 *
 * Complete SD 6.0 and eMMC 5.1 compliant driver
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _SD_H
#define _SD_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SD/eMMC SPECIFICATION CONSTANTS                   */
/*===========================================================================*/

/* SD Specification Version */
#define SD_VERSION_MAJOR            6
#define SD_VERSION_MINOR            0
#define SD_VERSION                  ((SD_VERSION_MAJOR << 16) | SD_VERSION_MINOR)

/* eMMC Specification Version */
#define EMMC_VERSION_MAJOR          5
#define EMMC_VERSION_MINOR          1
#define EMMC_VERSION                ((EMMC_VERSION_MAJOR << 16) | EMMC_VERSION_MINOR)

/* Card Types */
#define CARD_TYPE_UNKNOWN           0
#define CARD_TYPE_SD_SC             1   /* SD Standard Capacity (≤2GB) */
#define CARD_TYPE_SDHC              2   /* SD High Capacity (4-32GB) */
#define CARD_TYPE_SDXC              3   /* SD eXtended Capacity (64GB-2TB) */
#define CARD_TYPE_SDUC              4   /* SD Ultra Capacity (2TB-128TB) */
#define CARD_TYPE_MMC               5   /* MultiMediaCard */
#define CARD_TYPE_EMMC              6   /* Embedded MMC */

/* Card States */
#define CARD_STATE_IDLE             0
#define CARD_STATE_READY            1
#define CARD_STATE_IDENT            2
#define CARD_STATE_STBY             3
#define CARD_STATE_TRAN             4
#define CARD_STATE_DATA             5
#define CARD_STATE_RCV              6
#define CARD_STATE_PRG              7
#define CARD_STATE_DIS              8
#define CARD_STATE_INA              9

/* Voltage Windows */
#define VOLTAGE_WINDOW_3_3V         0x01
#define VOLTAGE_WINDOW_3_0V         0x02
#define VOLTAGE_WINDOW_1_8V         0x04
#define VOLTAGE_WINDOW_1_2V         0x08

/* SD Bus Modes */
#define BUS_MODE_DEFAULT            0
#define BUS_MODE_HIGH_SPEED         1
#define BUS_MODE_SDR12              2
#define BUS_MODE_SDR25              3
#define BUS_MODE_SDR50              4
#define BUS_MODE_SDR104             5
#define BUS_MODE_DDR50              6
#define BUS_MODE_HS200              7
#define BUS_MODE_HS400              8

/* MMC Bus Width */
#define BUS_WIDTH_1_BIT             0
#define BUS_WIDTH_4_BIT             1
#define BUS_WIDTH_8_BIT             2

/* Command Classes */
#define CMD_CLASS_BASIC             0x001   /* Basic commands */
#define CMD_CLASS_STREAM_READ       0x002   /* Stream read */
#define CMD_CLASS_STREAM_WRITE      0x004   /* Stream write */
#define CMD_CLASS_ERASE             0x008   /* Erase commands */
#define CMD_CLASS_WRITE_PROTECT     0x010   /* Write protect */
#define CMD_CLASS_LOCK_CARD         0x020   /* Lock card */
#define CMD_CLASS_APP_SPECIFIC      0x040   /* Application specific */
#define CMD_CLASS_IO_MODE           0x080   /* I/O mode */
#define CMD_CLASS_SWITCH            0x100   /* Switch function */
#define CMD_CLASS_APP_CMD           0x200   /* Application command */

/* SD Commands (SPI and SD modes) */
#define SD_CMD_GO_IDLE_STATE        0       /* Reset card */
#define SD_CMD_SEND_OP_COND         1       /* Send operation condition */
#define SD_CMD_ALL_SEND_CID         2       /* Ask card to send CID */
#define SD_CMD_SEND_RELATIVE_ADDR   3       /* Ask card to publish RCA */
#define SD_CMD_SET_DSR              4       /* Program DSR */
#define SD_CMD_SLEEP_AWAKE_CARD     5       /* Sleep/awake card */
#define SD_CMD_SWITCH_FUNC          6       /* Switch function */
#define SD_CMD_SELECT_CARD          7       /* Select/deselect card */
#define SD_CMD_SEND_EXT_CSD         8       /* Send EXT_CSD (MMC) */
#define SD_CMD_SEND_CSD             9       /* Send card CSD */
#define SD_CMD_SEND_CID             10      /* Send card CID */
#define SD_CMD_READ_DAT_UNTIL_STOP  11      /* Read data until stop */
#define SD_CMD_STOP_TRANSMISSION    12      /* Stop transmission */
#define SD_CMD_SEND_STATUS          13      /* Send status */
#define SD_CMD_GO_INACTIVE_STATE    15      /* Go inactive state */
#define SD_CMD_SET_BLOCKLEN         16      /* Set block length */
#define SD_CMD_READ_SINGLE_BLOCK    17      /* Read single block */
#define SD_CMD_READ_MULTIPLE_BLOCK  18      /* Read multiple blocks */
#define SD_CMD_SEND_TUNING_BLOCK    19      /* Send tuning block */
#define SD_CMD_SPEED_CLASS_CONTROL  20      /* Speed class control */
#define SD_CMD_SET_BLOCK_COUNT      23      /* Set block count */
#define SD_CMD_WRITE_DAT_UNTIL_STOP 21      /* Write data until stop */
#define SD_CMD_WRITE_SINGLE_BLOCK   24      /* Write single block */
#define SD_CMD_WRITE_MULTIPLE_BLOCK 25      /* Write multiple blocks */
#define SD_CMD_PROGRAM_CID          26      /* Program CID */
#define SD_CMD_PROGRAM_CSD          27      /* Program CSD */
#define SD_CMD_SET_WRITE_PROT       28      /* Set write protect */
#define SD_CMD_CLR_WRITE_PROT       29      /* Clear write protect */
#define SD_CMD_SEND_WRITE_PROT      30      /* Send write protect */
#define SD_CMD_ERASE_WR_BLK_START   32      /* Erase write block start */
#define SD_CMD_ERASE_WR_BLK_END     33      /* Erase write block end */
#define SD_CMD_ERASE_GROUP_START    35      /* Erase group start */
#define SD_CMD_ERASE_GROUP_END      36      /* Erase group end */
#define SD_CMD_ERASE                38      /* Erase */
#define SD_CMD_FAST_IO              39      /* Fast I/O */
#define SD_CMD_GO_IRQ_STATE         40      /* Go IRQ state */
#define SD_CMD_LOCK_UNLOCK          42      /* Lock/unlock card */
#define SD_CMD_APP_CMD              55      /* Application command */
#define SD_CMD_GEN_CMD              56      /* General command */

/* SD Application Commands (after CMD55) */
#define SD_ACMD_SET_BUS_WIDTH       6       /* Set bus width */
#define SD_ACMD_SD_STATUS           13      /* SD status */
#define SD_ACMD_SEND_NUM_WR_BLOCKS  22      /* Send number of written blocks */
#define SD_ACMD_SET_WR_BLK_ERASE_COUNT 23   /* Set write block erase count */
#define SD_ACMD_SD_SEND_OP_COND     41      /* Send operation condition (SD) */
#define SD_ACMD_SET_CLR_CARD_DETECT 42      /* Set/clear card detect */
#define SD_ACMD_SEND_SCR            51      /* Send SCR */

/* MMC Commands */
#define MMC_CMD_SEND_OP_COND        1       /* Send operation condition */
#define MMC_CMD_FAST_IO             39      /* Fast I/O */
#define MMC_CMD_GO_IRQ_STATE        40      /* Go IRQ state */
#define MMC_CMD_SLEEP_AWAKE         5       /* Sleep/awake */
#define MMC_CMD_SWITCH              6       /* Switch */
#define MMC_CMD_SEND_EXT_CSD        8       /* Send EXT_CSD */
#define MMC_CMD_BUS_TEST_W          14      /* Bus test write */
#define MMC_CMD_BUS_TEST_R          19      /* Bus test read */
#define MMC_CMD_HS_MMC_SEND_OP_COND 1       /* HS MMC send op cond */

/* Response Types */
#define RESP_NONE                   0
#define RESP_R1                     1
#define RESP_R1B                    2
#define RESP_R2                     3
#define RESP_R3                     4
#define RESP_R4                     5
#define RESP_R5                     6
#define RESP_R6                     7
#define RESP_R7                     8

/* OCR Register Bits */
#define OCR_BUSY                    (1 << 31)
#define OCR_CCS                     (1 << 30)   /* Card Capacity Status */
#define OCR_S18A                    (1 << 24)   /* Switching to 1.8V Accepted */
#define OCR_VDD_3_3_3_4             (1 << 20)
#define OCR_VDD_3_2_3_3             (1 << 19)
#define OCR_VDD_3_1_3_2             (1 << 18)
#define OCR_VDD_3_0_3_1             (1 << 17)
#define OCR_VDD_2_9_3_0             (1 << 16)
#define OCR_VDD_2_8_2_9             (1 << 15)
#define OCR_VDD_2_7_2_8             (1 << 14)

/* R1 Status Bits */
#define R1_OUT_OF_RANGE             (1 << 31)
#define R1_ADDRESS_ERROR            (1 << 30)
#define R1_BLOCK_LEN_ERROR          (1 << 29)
#define R1_ERASE_SEQ_ERROR          (1 << 28)
#define R1_ERASE_PARAM              (1 << 27)
#define R1_WP_VIOLATION             (1 << 26)
#define R1_CARD_IS_LOCKED           (1 << 25)
#define R1_LOCK_UNLOCK_FAILED       (1 << 24)
#define R1_COM_CRC_ERROR            (1 << 23)
#define R1_ILLEGAL_COMMAND          (1 << 22)
#define R1_CARD_ECC_FAILED          (1 << 21)
#define R1_CC_ERROR                 (1 << 20)
#define R1_ERROR                    (1 << 19)
#define R1_UNDERRUN                 (1 << 18)
#define R1_OVERRUN                  (1 << 17)
#define R1_CID_CSD_OVERWRITE        (1 << 16)
#define R1_CARD_STATUS              (1 << 15)

/* EXT_CSD Register Offsets (MMC/eMMC) */
#define EXT_CSD_CMD_SET             0
#define EXT_CSD_CMD_SET_REV         1
#define EXT_CSD_POWER_CLASS         2
#define EXT_CSD_HS_TIMING           3
#define EXT_CSD_REV                 4
#define EXT_CSD_STRUCTURE           5
#define EXT_CSD_CARD_TYPE           6
#define EXT_CSD_DRIVER_STRENGTH     7
#define EXT_CSD_OUT_OF_INTERRUPT_TIME 8
#define EXT_CSD_PART_SWITCH_TIME    9
#define EXT_CSD_PWR_CL_52_195       10
#define EXT_CSD_PWR_CL_52_360       11
#define EXT_CSD_PWR_CL_26_195       12
#define EXT_CSD_PWR_CL_26_360       13
#define EXT_CSD_RESERVED_14         14
#define EXT_CSD_BUS_WIDTH           183
#define EXT_CSD_STROBE_SUPPORT      184
#define EXT_CSD_ERASE_GROUP_DEF     175
#define EXT_CSD_PART_CONFIG         179
#define EXT_CSD_ERASED_MEM_CONT     181
#define EXT_CSD_BOOT_WP             173
#define EXT_CSD_USER_WP             171
#define EXT_CSD_FW_CONFIG           169
#define EXT_CSD_RPMB_SIZE           168
#define EXT_CSD_BOOT_SIZE_MULT      226
#define EXT_CSD_ACCESS_SIZE         227
#define EXT_CSD_HC_WP_GRP_SIZE      221
#define EXT_CSD_HC_ERASE_GRP_SIZE   224
#define EXT_CSD_SEC_COUNT           212
#define EXT_CSD_MIN_PERF_R_26_26    230
#define EXT_CSD_MIN_PERF_W_26_26    231
#define EXT_CSD_MIN_PERF_R_52_52    232
#define EXT_CSD_MIN_PERF_W_52_52    233
#define EXT_CSD_PWR_CL_200_360      236
#define EXT_CSD_PWR_CL_200_195      237
#define EXT_CSD_DRIVER_STRENGTH_1V8 238
#define EXT_CSD_DRIVER_STRENGTH_1V2 239
#define EXT_CSD_DRIVER_STRENGTH_1V95 240
#define EXT_CSD_DEVICE_TYPE         196
#define EXT_CSD_PARTITIONING_SUPPORT 160
#define EXT_CSD_MAX_ENH_SIZE_MULT   157
#define EXT_CSD_PARTITIONING_SETTING 156
#define EXT_CSD_GP_SIZE_MULT        143
#define EXT_CSD_RND_W_R_TYPE        192
#define EXT_CSD_FW_VERSION          228
#define EXT_CSD_CACHE_SIZE          249
#define EXT_CSD_GENERIC_CMD6_TIME   248
#define EXT_CSD_POWER_OFF_LONG_TIME 247
#define EXT_CSD_BKOPS_SUPPORT       163
#define EXT_CSD_BKOPS_EN            162
#define EXT_CSD_INIT_TIMEOUT        246
#define EXT_CSD_CORRECT_PRG_SECTORS 246
#define EXT_CSD_DEVICE_VERSION      245
#define EXT_CSD_FFU_FEATURES        264
#define EXT_CSD_FFU_ARG             265
#define EXT_CSD_OPERATION_CODE_TIMEOUT 266
#define EXT_CSD_FFU_STATUS          267
#define EXT_CSD_MODE_CONFIG         268
#define EXT_CSD_MODE_OPERATION_CODES 269
#define EXT_CSD_FLUSH_CACHE         32
#define EXT_CSD_CACHE_CTRL          33
#define EXT_CSD_POWER_OFF           34
#define EXT_CSD_BKOPS_START         35
#define EXT_CSD_SANITIZE_START      36

/* Partition Configuration */
#define EXT_CSD_PART_CONFIG_ACC_MASK 0x38
#define EXT_CSD_PART_CONFIG_PART_MASK 0x07
#define EXT_CSD_PART_ACCESS_DEFAULT   0
#define EXT_CSD_PART_ACCESS_BOOT1     1
#define EXT_CSD_PART_ACCESS_BOOT2     2
#define EXT_CSD_PART_ACCESS_RPMB      3
#define EXT_CSD_PART_ACCESS_GP1       4
#define EXT_CSD_PART_ACCESS_GP2       5
#define EXT_CSD_PART_ACCESS_GP3       6
#define EXT_CSD_PART_ACCESS_GP4       7
#define EXT_CSD_PART_ACCESS_USER      0

/* Hardware Reset */
#define EXT_CSD_RST_N_FUNCTION        (1 << 0)
#define EXT_CSD_RST_N_ENABLE          (1 << 1)
#define EXT_CSD_RST_N_DISABLE         (0 << 1)

/* Card Status for MMC */
#define MMC_STATUS_IDLE               0
#define MMC_STATUS_READY              1
#define MMC_STATUS_IDENT              2
#define MMC_STATUS_STBY               3
#define MMC_STATUS_TRAN               4
#define MMC_STATUS_DATA               5
#define MMC_STATUS_RCV                6
#define MMC_STATUS_PRG                7
#define MMC_STATUS_DIS                8

/* Maximum Values */
#define SD_MAX_DEVICES              8
#define SD_MAX_BLOCK_SIZE           4096
#define SD_FIFO_SIZE                512
#define SD_CMD_TIMEOUT_MS           1000
#define SD_DATA_TIMEOUT_MS          5000
#define SD_INIT_TIMEOUT_MS          10000

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * SD Card CID Register
 */
typedef struct __packed {
    u8 mid;           /* Manufacturer ID */
    u8 oid[2];        /* OEM/Application ID */
    char pnm[5];      /* Product Name */
    u8 prv;           /* Product Revision */
    u32 psn;          /* Product Serial Number */
    u8 reserved;      /* Reserved */
    u16 mdt;          /* Manufacturing Date */
    u8 crc;           /* CRC7 */
} sd_cid_t;

/**
 * SD Card CSD Register (Version 1.0)
 */
typedef struct __packed {
    u8 csd_structure:2;
    u8 taac:8;
    u8 nsac:8;
    u8 tran_speed:8;
    u16 ccc:12;
    u16 read_bl_len:4;
    u16 read_bl_partial:1;
    u16 write_blk_misalign:1;
    u16 read_blk_misalign:1;
    u16 dsr_imp:1;
    u16 c_size:12;
    u16 vdd_r_curr_min:3;
    u16 vdd_r_curr_max:3;
    u16 vdd_w_curr_min:3;
    u16 vdd_w_curr_max:3;
    u16 c_size_mult:3;
    u16 erase_grp_size:5;
    u16 erase_grp_mult:5;
    u16 wp_grp_size:7;
    u16 sector_size:1;
    u16 wp_grp_enable:1;
    u16 default_ecc:2;
    u16 r2w_factor:3;
    u16 write_bl_len:4;
    u16 write_bl_partial:1;
    u8  file_format_grp:1;
    u8  copy:1;
    u8  perm_write_protect:1;
    u8  tmp_write_protect:1;
    u8  file_format:2;
    u8  ecc:2;
    u8  crc;
} sd_csd_v1_t;

/**
 * SD Card CSD Register (Version 2.0 - SDHC/SDXC)
 */
typedef struct __packed {
    u8 csd_structure:2;
    u8 taac:8;
    u8 nsac:8;
    u8 tran_speed:8;
    u16 ccc:12;
    u16 read_bl_len:4;
    u16 read_bl_partial:1;
    u16 write_blk_misalign:1;
    u16 read_blk_misalign:1;
    u16 dsr_imp:1;
    u16 reserved:6;
    u32 c_size:22;
    u8 reserved2:1;
    u8 erase_grp_size:5;
    u8 erase_grp_mult:5;
    u8 wp_grp_size:7;
    u8 sector_size:1;
    u8 wp_grp_enable:1;
    u8 default_ecc:2;
    u8 r2w_factor:3;
    u8 write_bl_len:4;
    u8 write_bl_partial:1;
    u8 file_format_grp:1;
    u8 copy:1;
    u8 perm_write_protect:1;
    u8 tmp_write_protect:1;
    u8 file_format:2;
    u8 ecc:2;
    u8 crc;
} sd_csd_v2_t;

/**
 * eMMC EXT_CSD Register
 */
typedef struct __packed {
    u8 cmd_set;                     /* [0] Command Set */
    u8 cmd_set_rev;                 /* [1] Command Set Revision */
    u8 power_class;                 /* [2] Power Class */
    u8 hs_timing;                   /* [3] High Speed Timing */
    u8 rev;                         /* [4] Revision */
    u8 structure;                   /* [5] Structure Version */
    u8 card_type;                   /* [6] Card Type */
    u8 driver_strength;             /* [7] Driver Strength */
    u8 out_of_int_time;             /* [8] Out of Interrupt Time */
    u8 part_switch_time;            /* [9] Partition Switch Time */
    u8 pwr_cl_52_195;               /* [10] Power Class for 52MHz at 1.95V */
    u8 pwr_cl_52_360;               /* [11] Power Class for 52MHz at 3.6V */
    u8 pwr_cl_26_195;               /* [12] Power Class for 26MHz at 1.95V */
    u8 pwr_cl_26_360;               /* [13] Power Class for 26MHz at 3.6V */
    u8 reserved_14;                 /* [14] Reserved */
    u8 reserved[168];               /* [15-182] Reserved */
    u8 bus_width;                   /* [183] Bus Width */
    u8 strobe_support;              /* [184] Strobe Support */
    u8 reserved_185[8];             /* [185-192] Reserved */
    u8 device_type;                 /* [196] Device Type */
    u8 reserved_197[15];            /* [197-211] Reserved */
    u8 sec_count[4];                /* [212-215] Sector Count */
    u8 reserved_216[5];             /* [216-220] Reserved */
    u8 hc_erase_grp_size;           /* [224] HC Erase Group Size */
    u8 hc_wp_grp_size;              /* [221] HC Write Protect Group Size */
    u8 reserved_222[2];             /* [222-223] Reserved */
    u8 min_perf_r_26_26;            /* [230] Min Read Perf for 26MHz */
    u8 min_perf_w_26_26;            /* [231] Min Write Perf for 26MHz */
    u8 min_perf_r_52_52;            /* [232] Min Read Perf for 52MHz */
    u8 min_perf_w_52_52;            /* [233] Min Write Perf for 52MHz */
    u8 reserved_234[2];             /* [234-235] Reserved */
    u8 pwr_cl_200_360;              /* [236] Power Class for 200MHz at 3.6V */
    u8 pwr_cl_200_195;              /* [237] Power Class for 200MHz at 1.95V */
    u8 driver_strength_1v8;          /* [238] Driver Strength for 1.8V */
    u8 driver_strength_1v2;          /* [239] Driver Strength for 1.2V */
    u8 driver_strength_1v95;         /* [240] Driver Strength for 1.95V */
    u8 reserved_241[5];             /* [241-245] Reserved */
    u8 bkops_support;               /* [163] Background Operations Support */
    u8 bkops_en;                    /* [162] Background Operations Enable */
    u8 reserved_247;                /* [247] Reserved */
    u8 power_off_long_time;         /* [247] Power Off Long Time */
    u8 generic_cmd6_time;           /* [248] Generic CMD6 Time */
    u8 cache_size[4];               /* [249-252] Cache Size */
    u8 reserved_253[11];            /* [253-263] Reserved */
    u8 ffu_features;                /* [264] FFU Features */
    u8 ffu_arg[4];                  /* [265-268] FFU Argument */
    u8 operation_code_timeout;      /* [269] Operation Code Timeout */
    u8 ffu_status;                  /* [270] FFU Status */
    u8 mode_config;                 /* [271] Mode Config */
    u8 mode_operation_codes;        /* [272] Mode Operation Codes */
    u8 reserved_273[383];           /* [273-655] Reserved */
    u8 flush_cache;                 /* [32] Flush Cache */
    u8 cache_ctrl;                  /* [33] Cache Control */
    u8 power_off;                   /* [34] Power Off */
    u8 bkops_start;                 /* [35] Background Operations Start */
    u8 sanitize_start;              /* [36] Sanitize Start */
    u8 reserved_656[1];             /* [656] Reserved */
    u8 vendor_specific[159];        /* [657-815] Vendor Specific */
} mmc_ext_csd_t;

/**
 * SD Card SCR Register
 */
typedef struct __packed {
    u8 scr_structure:4;
    u8 scr_spec_ver:4;
    u8 sta_after_spe:1;
    u8 reserved1:3;
    u8 bus_widths:4;
    u8 reserved2:4;
    u8 sd_spec4:1;
    u8 reserved3:3;
    u8 sd_spec5:1;
    u8 reserved4:24;
    u8 sd_spec6:1;
    u8 reserved5:7;
} sd_scr_t;

/**
 * SD Card Status
 */
typedef struct __packed {
    u8 card_state;
    u8 reserved;
    u16 max_bus_clock_freq;
    u8 reserved2[2];
    u8 card_size;
    u8 speed_class;
    u8 performance_move;
    u8 au_size;
    u32 erase_offset;
    u32 erase_timeout;
    u8 reserved3;
    u8 uhs_speed_grade;
    u8 video_speed_grade;
    u8 vsc_erase_unit_size;
    u16 current_perf;
    u8 reserved4[2];
    u8 au_size2;
    u8 reserved5[13];
} sd_status_t;

/**
 * SD/eMMC Command
 */
typedef struct {
    u32 cmd_idx;                    /* Command Index */
    u32 arg;                        /* Command Argument */
    u32 resp[4];                    /* Response */
    u32 resp_type;                  /* Response Type */
    u32 flags;                      /* Command Flags */
    u32 timeout_ms;                 /* Timeout in ms */
    bool is_app_cmd;                /* Is Application Command */
    bool has_data;                  /* Has Data Transfer */
    bool is_read;                   /* Is Read Operation */
    u32 blocks;                     /* Number of Blocks */
    u32 block_size;                 /* Block Size */
    void *data;                     /* Data Buffer */
    phys_addr_t data_phys;          /* Data Physical Address */
} sd_cmd_t;

/**
 * SD/eMMC Device
 */
typedef struct sd_device {
    u32 device_id;                  /* Device ID */
    char name[32];                  /* Device Name */
    u32 card_type;                  /* Card Type */
    u32 card_state;                 /* Card State */
    u32 rca;                        /* Relative Card Address */
    
    /* Card Identification */
    sd_cid_t cid;                   /* Card ID */
    union {
        sd_csd_v1_t csd_v1;         /* CSD Version 1 */
        sd_csd_v2_t csd_v2;         /* CSD Version 2 */
    };
    sd_scr_t scr;                   /* SCR Register */
    mmc_ext_csd_t ext_csd;          /* EXT_CSD (MMC/eMMC) */
    
    /* Capacity */
    u64 capacity;                   /* Total Capacity */
    u64 usable_capacity;            /* Usable Capacity */
    u32 block_size;                 /* Block Size */
    u64 block_count;                /* Number of Blocks */
    u32 read_bl_len;                /* Read Block Length */
    u32 write_bl_len;               /* Write Block Length */
    
    /* Speed */
    u32 max_clock_freq;             /* Maximum Clock Frequency */
    u32 current_clock_freq;         /* Current Clock Frequency */
    u32 bus_mode;                   /* Bus Mode */
    u32 bus_width;                  /* Bus Width */
    u32 speed_class;                /* Speed Class */
    u32 uhs_grade;                  /* UHS Speed Grade */
    
    /* Voltage */
    u32 voltage;                    /* Current Voltage */
    u32 voltage_window;             /* Voltage Window */
    
    /* Partitions (eMMC) */
    u32 partition_count;            /* Number of Partitions */
    u32 current_partition;          /* Current Partition */
    u64 boot_partition_size;        /* Boot Partition Size */
    u64 rpmb_size;                  /* RPMB Size */
    u64 gp_partition_size[4];       /* General Purpose Partition Sizes */
    
    /* Hardware Reset */
    bool hw_reset_supported;        /* Hardware Reset Supported */
    bool hw_reset_enabled;          /* Hardware Reset Enabled */
    
    /* Cache */
    bool cache_supported;           /* Cache Supported */
    bool cache_enabled;             /* Cache Enabled */
    u32 cache_size;                 /* Cache Size */
    
    /* Background Operations */
    bool bkops_supported;           /* Background Operations Supported */
    bool bkops_enabled;             /* Background Operations Enabled */
    
    /* Power Management */
    u32 power_class;                /* Power Class */
    u32 sleep_awake_supported;      /* Sleep/Awake Supported */
    
    /* Write Protection */
    bool perm_write_protect;        /* Permanent Write Protect */
    bool tmp_write_protect;         /* Temporary Write Protect */
    bool boot_wp;                   /* Boot Write Protect */
    
    /* Statistics */
    u64 reads;                      /* Read Operations */
    u64 writes;                     /* Write Operations */
    u64 read_bytes;                 /* Bytes Read */
    u64 write_bytes;                /* Bytes Written */
    u32 errors;                     /* Error Count */
    
    /* State */
    bool is_present;                /* Is Card Present */
    bool is_initialized;            /* Is Initialized */
    bool is_readonly;               /* Is Read Only */
    bool is_locked;                 /* Is Locked */
    bool is_sleeping;               /* Is Sleeping */
    
    /* Controller Interface */
    volatile void *base;            /* Controller Base Address */
    u32 irq;                        /* Interrupt Number */
    u32 dma_channel;                /* DMA Channel */
    
    /* Synchronization */
    spinlock_t lock;                /* Device Lock */
    
    /* Links */
    struct sd_device *next;         /* Next Device */
} sd_device_t;

/**
 * SD/eMMC Controller
 */
typedef struct sd_controller {
    u32 controller_id;              /* Controller ID */
    char name[32];                  /* Controller Name */
    
    /* Controller Registers */
    volatile void *base;            /* Base Address */
    u32 irq;                        /* Interrupt Number */
    u32 dma_channel;                /* DMA Channel */
    
    /* Capabilities */
    u32 voltage_support;            /* Voltage Support */
    u32 bus_width_support;          /* Bus Width Support */
    u32 max_clock_freq;             /* Maximum Clock Frequency */
    u32 max_current_3v3;            /* Max Current at 3.3V */
    u32 max_current_3v0;            /* Max Current at 3.0V */
    u32 max_current_1v8;            /* Max Current at 1.8V */
    u32 max_current_1v2;            /* Max Current at 1.2V */
    u32 caps;                       /* Controller Capabilities */
    u32 caps2;                      /* Extended Capabilities */
    
    /* Devices */
    sd_device_t *devices;           /* Device List */
    u32 device_count;               /* Device Count */
    
    /* State */
    bool is_initialized;            /* Is Initialized */
    bool card_present;              /* Is Card Present */
    bool card_detect_irq;           /* Card Detect IRQ */
    
    /* DMA */
    void *dma_buffer;               /* DMA Buffer */
    phys_addr_t dma_buffer_phys;    /* DMA Buffer Physical */
    
    /* Synchronization */
    spinlock_t lock;                /* Controller Lock */
    
    /* Links */
    struct sd_controller *next;     /* Next Controller */
} sd_controller_t;

/**
 * SD/eMMC Driver State
 */
typedef struct {
    bool initialized;               /* Is Driver Initialized */
    u32 controller_count;           /* Number of Controllers */
    sd_controller_t *controllers;   /* Controller List */
    spinlock_t lock;                /* Driver Lock */
    
    /* Statistics */
    u64 total_devices;              /* Total Devices */
    u64 total_reads;                /* Total Reads */
    u64 total_writes;               /* Total Writes */
    u64 total_read_bytes;           /* Total Bytes Read */
    u64 total_write_bytes;          /* Total Bytes Written */
} sd_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int sd_init(void);
int sd_shutdown(void);
bool sd_is_initialized(void);

/* Controller operations */
int sd_probe(void);
int sd_add_controller(sd_controller_t *ctrl);
int sd_remove_controller(u32 controller_id);
sd_controller_t *sd_get_controller(u32 controller_id);
int sd_list_controllers(sd_controller_t *ctrls, u32 *count);

/* Device operations */
int sd_detect_card(sd_controller_t *ctrl);
int sd_init_card(sd_controller_t *ctrl, sd_device_t *dev);
int sd_remove_card(sd_controller_t *ctrl);
sd_device_t *sd_get_device(u32 device_id);
int sd_list_devices(sd_device_t *devices, u32 *count);

/* Command operations */
int sd_send_command(sd_controller_t *ctrl, sd_cmd_t *cmd);
int sd_send_app_command(sd_controller_t *ctrl, sd_cmd_t *cmd);
int sd_wait_ready(sd_controller_t *ctrl, u32 timeout_ms);

/* Card initialization */
int sd_card_go_idle(sd_controller_t *ctrl);
int sd_card_send_op_cond(sd_controller_t *ctrl, u32 ocr);
int sd_card_all_send_cid(sd_controller_t *ctrl, sd_cid_t *cid);
int sd_card_send_relative_addr(sd_controller_t *ctrl, u32 *rca);
int sd_card_select(sd_controller_t *ctrl, u32 rca);
int sd_card_send_csd(sd_controller_t *ctrl, u32 rca, void *csd);
int sd_card_send_scr(sd_controller_t *ctrl, sd_scr_t *scr);

/* MMC specific */
int mmc_card_send_op_cond(sd_controller_t *ctrl, u32 ocr);
int mmc_card_send_ext_csd(sd_controller_t *ctrl, mmc_ext_csd_t *ext_csd);
int mmc_card_switch(sd_controller_t *ctrl, u8 access, u8 index, u8 value);
int mmc_card_set_bus_width(sd_controller_t *ctrl, u8 width);
int mmc_card_set_hs_timing(sd_controller_t *ctrl, u8 timing);

/* I/O operations */
int sd_read_block(sd_device_t *dev, u64 block, u32 count, void *buffer);
int sd_write_block(sd_device_t *dev, u64 block, u32 count, const void *buffer);
int sd_erase(sd_device_t *dev, u64 start_block, u64 end_block);

/* Block device interface */
int sd_block_read(u32 device_id, u64 lba, u32 count, void *buffer);
int sd_block_write(u32 device_id, u64 lba, u32 count, const void *buffer);
int sd_block_flush(u32 device_id);
u64 sd_block_get_size(u32 device_id);
u32 sd_block_get_block_size(u32 device_id);

/* Power management */
int sd_card_sleep(sd_device_t *dev);
int sd_card_awake(sd_device_t *dev);
int sd_set_power_class(sd_device_t *dev, u8 power_class);

/* Partition operations (eMMC) */
int sd_switch_partition(sd_device_t *dev, u32 partition);
int sd_get_partition_info(sd_device_t *dev, u32 partition, u64 *size);

/* Utility functions */
const char *sd_get_card_type_name(u32 type);
const char *sd_get_state_name(u32 state);
const char *sd_get_bus_mode_name(u32 mode);
u64 sd_format_capacity(u64 bytes, char *buffer, u32 buffer_size);
void sd_print_card_info(sd_device_t *dev);

/* Global instance */
sd_driver_t *sd_get_driver(void);

#endif /* _SD_H */
