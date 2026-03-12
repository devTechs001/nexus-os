/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/power/acpi.c - ACPI Support
 *
 * Implements Advanced Configuration and Power Interface (ACPI) support
 * for system configuration, power management, and hardware discovery
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ACPI CONSTANTS                                    */
/*===========================================================================*/

/* ACPI Signatures */
#define ACPI_SIG_RSDP         "RSD PTR "
#define ACPI_SIG_RSDT         "RSDT"
#define ACPI_SIG_XSDT         "XSDT"
#define ACPI_SIG_FADT         "FACP"
#define ACPI_SIG_DSDT         "DSDT"
#define ACPI_SIG_FACS         "FACS"
#define ACPI_SIG_APIC         "APIC"
#define ACPI_SIG_MCFG         "MCFG"
#define ACPI_SIG_HPET         "HPET"
#define ACPI_SIG_WAET         "WAET"
#define ACPI_SIG_SSDT         "SSDT"
#define ACPI_SIG_PSDT         "PSDT"
#define ACPI_SIG_SRAT         "SRAT"
#define ACPI_SIG_SLIT         "SLIT"

/* ACPI Versions */
#define ACPI_VERSION_1        1
#define ACPI_VERSION_2        2
#define ACPI_VERSION_3        3
#define ACPI_VERSION_4        4
#define ACPI_VERSION_5        5
#define ACPI_VERSION_6        6

/* FADT Flags */
#define FADT_WBINVD           (1 << 0)
#define FADT_WBINVD_FLUSH     (1 << 1)
#define FADT_PROC_C1          (1 << 2)
#define FADT_P_LVL2_UP        (1 << 3)
#define FADT_PWR_BUTTON       (1 << 4)
#define FADT_SLP_BUTTON       (1 << 5)
#define FADT_FIX_RTC          (1 << 6)
#define FADT_RTC_S4           (1 << 7)
#define FADT_TMR_VAL_EXT      (1 << 8)
#define FADT_DCK_CAP          (1 << 9)
#define FADT_RESET_REG_SUP    (1 << 10)
#define FADT_SEALED_CASE      (1 << 11)
#define FADT_HEADLESS         (1 << 12)
#define FADT_CPU_SW_SLP       (1 << 13)
#define FADT_PCI_EXP_WAK      (1 << 14)
#define FADT_USE_PLATFORM     (1 << 15)

/* Sleep States */
#define ACPI_SLEEP_S0         0
#define ACPI_SLEEP_S1         1
#define ACPI_SLEEP_S2         2
#define ACPI_SLEEP_S3         3
#define ACPI_SLEEP_S4         4
#define ACPI_SLEEP_S5         5

/* PM Register Addresses */
#define PM1A_EVT_BLK          0
#define PM1B_EVT_BLK          2
#define PM1A_CNT_BLK          4
#define PM1B_CNT_BLK          6
#define PM2_CNT_BLK           8
#define PM_TMR_BLK            10
#define GPE0_BLK              12
#define GPE1_BLK              14

/* PM1 Control Register Bits */
#define PM1_SCI_EN            (1 << 0)
#define PM1_BUS_RLSE          (1 << 1)
#define PM1_GLOBAL_LOCK       (1 << 2)
#define PM1_SLP_TYP           (1 << 4)
#define PM1_SLP_TYP_MASK      0x00000038
#define PM1_SLP_EN            (1 << 13)

/*===========================================================================*/
/*                         ACPI DATA STRUCTURES                              */
/*===========================================================================*/

/**
 * acpi_rsdp_t - Root System Description Pointer
 */
typedef struct acpi_rsdp {
    char signature[8];            /* "RSD PTR " */
    u8 checksum;                  /* Checksum */
    char oem_id[6];               /* OEM ID */
    u8 revision;                  /* ACPI revision */
    u32 rsdt_address;             /* RSDT address (32-bit) */
    u32 length;                   /* Table length (v2+) */
    u64 xsdt_address;             /* XSDT address (v2+) */
    u8 extended_checksum;         /* Extended checksum (v2+) */
    u8 reserved[3];               /* Reserved */
} __packed acpi_rsdp_t;

/**
 * acpi_sdt_header_t - ACPI SDT header
 */
typedef struct acpi_sdt_header {
    char signature[4];            /* Table signature */
    u32 length;                   /* Table length */
    u8 revision;                  /* Table revision */
    u8 checksum;                  /* Checksum */
    char oem_id[6];               /* OEM ID */
    char oem_table_id[8];         /* OEM table ID */
    u32 oem_revision;             /* OEM revision */
    u32 creator_id;               /* Creator ID */
    u32 creator_revision;         /* Creator revision */
} __packed acpi_sdt_header_t;

/**
 * acpi_rsdt_t - Root System Description Table
 */
typedef struct acpi_rsdt {
    acpi_sdt_header_t header;     /* SDT header */
    u32 entry[1];                 /* Entry pointers (32-bit) */
} __packed acpi_rsdt_t;

/**
 * acpi_xsdt_t - Extended Root System Description Table
 */
typedef struct acpi_xsdt {
    acpi_sdt_header_t header;     /* SDT header */
    u64 entry[1];                 /* Entry pointers (64-bit) */
} __packed acpi_xsdt_t;

/**
 * acpi_fadt_t - Fixed ACPI Description Table
 */
typedef struct acpi_fadt {
    acpi_sdt_header_t header;     /* SDT header */
    u32 firmware_ctrl;            /* FACS address */
    u32 dsdt;                     /* DSDT address */
    u8 reserved1;                 /* Reserved */
    u8 preferred_pm_profile;      /* Preferred PM profile */
    u16 sci_int;                  /* SCI interrupt */
    u32 smi_cmd;                  /* SMI command port */
    u8 acpi_enable;               /* ACPI enable value */
    u8 acpi_disable;              /* ACPI disable value */
    u8 s4bios_req;                /* S4BIOS request value */
    u8 pstate_control;            /* P-state control value */
    u32 pm1a_evt_blk;             /* PM1a event block */
    u32 pm1b_evt_blk;             /* PM1b event block */
    u32 pm1a_cnt_blk;             /* PM1a control block */
    u32 pm1b_cnt_blk;             /* PM1b control block */
    u32 pm2_cnt_blk;              /* PM2 control block */
    u32 pm_tmr_blk;               /* PM timer block */
    u32 gpe0_blk;                 /* GPE0 block */
    u32 gpe1_blk;                 /* GPE1 block */
    u8 pm1_evt_len;               /* PM1 event length */
    u8 pm1_cnt_len;               /* PM1 control length */
    u8 pm2_cnt_len;               /* PM2 control length */
    u8 pm_tmr_len;                /* PM timer length */
    u8 gpe0_blk_len;              /* GPE0 block length */
    u8 gpe1_blk_len;              /* GPE1 block length */
    u8 gpe1_base;                 /* GPE1 base */
    u8 cstate_control;            /* C-state control */
    u16 worst_c2_latency;         /* Worst C2 latency */
    u16 worst_c3_latency;         /* Worst C3 latency */
    u16 flush_size;               /* Flush size */
    u16 flush_stride;             /* Flush stride */
    u8 duty_offset;               /* Duty offset */
    u8 duty_width;                /* Duty width */
    u8 day_alarm;                 /* Day alarm */
    u8 month_alarm;               /* Month alarm */
    u8 century;                   /* Century */
    u16 boot_arch;                /* Boot architecture */
    u8 reserved2;                 /* Reserved */
    u32 flags;                    /* FADT flags */
    /* ... more fields ... */
} __packed acpi_fadt_t;

/**
 * acpi_madt_t - Multiple APIC Description Table
 */
typedef struct acpi_madt {
    acpi_sdt_header_t header;     /* SDT header */
    u32 local_apic_addr;          /* Local APIC address */
    u32 flags;                    /* MADT flags */
    /* APIC structures follow */
} __packed acpi_madt_t;

/**
 * acpi_sleep_state_t - ACPI sleep state info
 */
typedef struct acpi_sleep_state {
    u8 valid;                     /* State is valid */
    u8 sleep_type_a;              /* SLP_TYP for PM1a */
    u8 sleep_type_b;              /* SLP_TYP for PM1b */
    u32 latency;                  /* Entry latency (us) */
} acpi_sleep_state_t;

/**
 * acpi_data_t - ACPI manager data
 */
typedef struct acpi_data {
    acpi_rsdp_t *rsdp;            /* RSDP pointer */
    acpi_rsdt_t *rsdt;            /* RSDT pointer */
    acpi_xsdt_t *xsdt;            /* XSDT pointer */
    acpi_fadt_t *fadt;            /* FADT pointer */
    acpi_madt_t *madt;            /* MADT pointer */

    u8 acpi_version;              /* ACPI version */
    u8 revision;                  /* Table revision */

    u32 pm1a_cnt_addr;            /* PM1a control address */
    u32 pm1b_cnt_addr;            /* PM1b control address */
    u32 pm_tmr_addr;              /* PM timer address */

    acpi_sleep_state_t sleep_states[6]; /* Sleep states S0-S5 */

    bool initialized;             /* ACPI initialized */
} acpi_data_t;

/**
 * acpi_manager_t - ACPI manager global data
 */
typedef struct acpi_manager {
    acpi_data_t acpi;             /* ACPI data */

    u32 total_tables;             /* Total tables found */
    u64 last_sleep_time;          /* Last sleep timestamp */
    u64 total_sleep_time;         /* Total sleep time */
    u32 wakeup_count;             /* Wakeup count */

    spinlock_t lock;              /* Global lock */
    bool initialized;             /* Manager initialized */
} acpi_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static acpi_manager_t acpi_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         ACPI TABLE ACCESS                                 */
/*===========================================================================*/

/**
 * acpi_checksum - Calculate ACPI checksum
 * @buffer: Buffer to checksum
 * @length: Buffer length
 *
 * Returns: Checksum value
 */
static u8 acpi_checksum(const void *buffer, u32 length)
{
    const u8 *bytes = (const u8 *)buffer;
    u8 sum = 0;
    u32 i;

    for (i = 0; i < length; i++) {
        sum += bytes[i];
    }

    return sum;
}

/**
 * acpi_validate_rsdp - Validate RSDP structure
 * @rsdp: RSDP pointer
 *
 * Returns: 0 if valid, negative error code otherwise
 */
static int acpi_validate_rsdp(acpi_rsdp_t *rsdp)
{
    u8 checksum;

    if (!rsdp) {
        return -EINVAL;
    }

    /* Check signature */
    if (memcmp(rsdp->signature, ACPI_SIG_RSDP, 8) != 0) {
        return -EINVAL;
    }

    /* Check checksum (first 20 bytes) */
    checksum = acpi_checksum(rsdp, 20);
    if (checksum != 0) {
        pr_err("ACPI: RSDP checksum invalid\n");
        return -EINVAL;
    }

    /* For ACPI 2.0+, check extended checksum */
    if (rsdp->revision >= 2) {
        checksum = acpi_checksum(rsdp, rsdp->length);
        if (checksum != 0) {
            pr_err("ACPI: RSDP extended checksum invalid\n");
            return -EINVAL;
        }
    }

    return 0;
}

/**
 * acpi_validate_table - Validate ACPI table
 * @header: Table header
 *
 * Returns: 0 if valid, negative error code otherwise
 */
static int acpi_validate_table(acpi_sdt_header_t *header)
{
    u8 checksum;

    if (!header) {
        return -EINVAL;
    }

    checksum = acpi_checksum(header, header->length);
    if (checksum != 0) {
        pr_err("ACPI: Table %.4s checksum invalid\n", header->signature);
        return -EINVAL;
    }

    return 0;
}

/**
 * acpi_find_rsdp - Find RSDP in memory
 *
 * Returns: RSDP pointer, or NULL if not found
 */
static acpi_rsdp_t *acpi_find_rsdp(void)
{
    acpi_rsdp_t *rsdp;
    u8 *addr;

    /* Search in EBDA (Extended BIOS Data Area) */
    /* First search 0x000E0000 - 0x000FFFFF */
    for (addr = (u8 *)0x000E0000; addr < (u8 *)0x00100000; addr += 16) {
        rsdp = (acpi_rsdp_t *)addr;
        if (memcmp(rsdp->signature, ACPI_SIG_RSDP, 8) == 0) {
            if (acpi_validate_rsdp(rsdp) == 0) {
                return rsdp;
            }
        }
    }

    /* Search in EBDA if pointer available */
    /* (Simplified - would read EBDA pointer from BIOS) */

    return NULL;
}

/**
 * acpi_find_table - Find ACPI table by signature
 * @signature: Table signature
 * @instance: Instance number (for multiple tables)
 *
 * Returns: Table header pointer, or NULL if not found
 */
static acpi_sdt_header_t *acpi_find_table(const char *signature, u32 instance)
{
    acpi_sdt_header_t *header;
    u32 i, count = 0;

    if (!acpi_manager.acpi.xsdt && !acpi_manager.acpi.rsdt) {
        return NULL;
    }

    if (acpi_manager.acpi.xsdt) {
        /* XSDT (64-bit) */
        u32 entries = (acpi_manager.acpi.xsdt->header.length -
                       sizeof(acpi_sdt_header_t)) / 8;

        for (i = 0; i < entries; i++) {
            header = (acpi_sdt_header_t *)(ureg)acpi_manager.acpi.xsdt->entry[i];
            if (header && memcmp(header->signature, signature, 4) == 0) {
                if (count == instance) {
                    return header;
                }
                count++;
            }
        }
    } else {
        /* RSDT (32-bit) */
        u32 entries = (acpi_manager.acpi.rsdt->header.length -
                       sizeof(acpi_sdt_header_t)) / 4;

        for (i = 0; i < entries; i++) {
            header = (acpi_sdt_header_t *)(ureg)acpi_manager.acpi.rsdt->entry[i];
            if (header && memcmp(header->signature, signature, 4) == 0) {
                if (count == instance) {
                    return header;
                }
                count++;
            }
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         ACPI INITIALIZATION                               */
/*===========================================================================*/

/**
 * acpi_parse_rsdp - Parse RSDP and find RSDT/XSDT
 *
 * Returns: 0 on success, negative error code on failure
 */
static int acpi_parse_rsdp(void)
{
    acpi_rsdp_t *rsdp;

    pr_info("ACPI: Searching for RSDP...\n");

    rsdp = acpi_find_rsdp();
    if (!rsdp) {
        pr_err("ACPI: RSDP not found\n");
        return -ENODEV;
    }

    pr_info("ACPI: RSDP found at 0x%016X\n", (u64)rsdp);
    pr_info("ACPI: OEM ID: %.6s\n", rsdp->oem_id);
    pr_info("ACPI: Revision: %u\n", rsdp->revision);

    acpi_manager.acpi.rsdp = rsdp;
    acpi_manager.acpi.revision = rsdp->revision;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        /* Use XSDT (ACPI 2.0+) */
        acpi_manager.acpi.xsdt = (acpi_xsdt_t *)(ureg)rsdp->xsdt_address;

        if (acpi_validate_table(&acpi_manager.acpi.xsdt->header) != 0) {
            pr_err("ACPI: XSDT validation failed\n");
            return -EINVAL;
        }

        acpi_manager.acpi.acpi_version = ACPI_VERSION_2;
        pr_info("ACPI: Using XSDT at 0x%016X\n", (u64)acpi_manager.acpi.xsdt);
    } else {
        /* Use RSDT (ACPI 1.0) */
        acpi_manager.acpi.rsdt = (acpi_rsdt_t *)(ureg)rsdp->rsdt_address;

        if (acpi_validate_table(&acpi_manager.acpi.rsdt->header) != 0) {
            pr_err("ACPI: RSDT validation failed\n");
            return -EINVAL;
        }

        acpi_manager.acpi.acpi_version = ACPI_VERSION_1;
        pr_info("ACPI: Using RSDT at 0x%016X\n", (u64)acpi_manager.acpi.rsdt);
    }

    return 0;
}

/**
 * acpi_parse_fadt - Parse FADT table
 *
 * Returns: 0 on success, negative error code on failure
 */
static int acpi_parse_fadt(void)
{
    acpi_fadt_t *fadt;

    fadt = (acpi_fadt_t *)acpi_find_table(ACPI_SIG_FADT, 0);
    if (!fadt) {
        pr_warn("ACPI: FADT not found\n");
        return -ENOENT;
    }

    if (acpi_validate_table(&fadt->header) != 0) {
        pr_err("ACPI: FADT validation failed\n");
        return -EINVAL;
    }

    acpi_manager.acpi.fadt = fadt;

    /* Extract PM register addresses */
    acpi_manager.acpi.pm1a_cnt_addr = fadt->pm1a_cnt_blk;
    acpi_manager.acpi.pm1b_cnt_addr = fadt->pm1b_cnt_blk;
    acpi_manager.acpi.pm_tmr_addr = fadt->pm_tmr_blk;

    pr_info("ACPI: FADT found\n");
    pr_info("ACPI: PM1a CNT: 0x%08X\n", fadt->pm1a_cnt_blk);
    pr_info("ACPI: SCI IRQ: %u\n", fadt->sci_int);
    pr_info("ACPI: Flags: 0x%08X\n", fadt->flags);

    return 0;
}

/**
 * acpi_parse_madt - Parse MADT table
 *
 * Returns: 0 on success, negative error code on failure
 */
static int acpi_parse_madt(void)
{
    acpi_madt_t *madt;

    madt = (acpi_madt_t *)acpi_find_table(ACPI_SIG_APIC, 0);
    if (!madt) {
        pr_warn("ACPI: MADT not found\n");
        return -ENOENT;
    }

    if (acpi_validate_table(&madt->header) != 0) {
        pr_err("ACPI: MADT validation failed\n");
        return -EINVAL;
    }

    acpi_manager.acpi.madt = madt;

    pr_info("ACPI: MADT found\n");
    pr_info("ACPI: Local APIC: 0x%08X\n", madt->local_apic_addr);
    pr_info("ACPI: MADT Flags: 0x%08X\n", madt->flags);

    return 0;
}

/**
 * acpi_parse_sleep_states - Parse sleep state info from FADT
 */
static void acpi_parse_sleep_states(void)
{
    /* Initialize sleep states from FADT */
    /* (Simplified - would parse _Sx methods from DSDT) */

    /* S0 - Working */
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S0].valid = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S0].sleep_type_a = 0;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S0].sleep_type_b = 0;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S0].latency = 0;

    /* S1 - CPU stopped */
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S1].valid = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S1].sleep_type_a = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S1].sleep_type_b = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S1].latency = 100;

    /* S3 - Suspend to RAM */
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S3].valid = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S3].sleep_type_a = 5;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S3].sleep_type_b = 5;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S3].latency = 5000;

    /* S4 - Hibernation */
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S4].valid = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S4].sleep_type_a = 6;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S4].sleep_type_b = 6;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S4].latency = 10000;

    /* S5 - Soft off */
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S5].valid = 1;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S5].sleep_type_a = 7;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S5].sleep_type_b = 7;
    acpi_manager.acpi.sleep_states[ACPI_SLEEP_S5].latency = 0;
}

/**
 * acpi_init - Initialize ACPI subsystem
 */
void acpi_init(void)
{
    int ret;

    pr_info("ACPI: Initializing ACPI subsystem...\n");

    spin_lock_init_named(&acpi_manager.lock, "acpi_manager");

    memset(&acpi_manager.acpi, 0, sizeof(acpi_data_t));
    acpi_manager.total_tables = 0;
    acpi_manager.last_sleep_time = 0;
    acpi_manager.total_sleep_time = 0;
    acpi_manager.wakeup_count = 0;
    acpi_manager.initialized = false;

    /* Parse RSDP */
    ret = acpi_parse_rsdp();
    if (ret < 0) {
        pr_err("ACPI: Failed to parse RSDP\n");
        return;
    }

    /* Parse FADT */
    acpi_parse_fadt();

    /* Parse MADT */
    acpi_parse_madt();

    /* Parse sleep states */
    acpi_parse_sleep_states();

    /* Count tables */
    if (acpi_manager.acpi.xsdt) {
        acpi_manager.total_tables = (acpi_manager.acpi.xsdt->header.length -
                                      sizeof(acpi_sdt_header_t)) / 8;
    } else if (acpi_manager.acpi.rsdt) {
        acpi_manager.total_tables = (acpi_manager.acpi.rsdt->header.length -
                                      sizeof(acpi_sdt_header_t)) / 4;
    }

    acpi_manager.initialized = true;

    pr_info("ACPI: Initialization complete (v%u, %u tables)\n",
            acpi_manager.acpi.acpi_version,
            acpi_manager.total_tables);
}

/*===========================================================================*/
/*                         ACPI SLEEP STATES                                 */
/*===========================================================================*/

/**
 * acpi_get_sleep_state - Get sleep state by name
 * @name: State name (e.g., "S3")
 *
 * Returns: State number, or -1 if not found
 */
int acpi_get_sleep_state(const char *name)
{
    if (!name || !acpi_manager.initialized) {
        return -1;
    }

    if (strcmp(name, "S0") == 0) return ACPI_SLEEP_S0;
    if (strcmp(name, "S1") == 0) return ACPI_SLEEP_S1;
    if (strcmp(name, "S2") == 0) return ACPI_SLEEP_S2;
    if (strcmp(name, "S3") == 0) return ACPI_SLEEP_S3;
    if (strcmp(name, "S4") == 0) return ACPI_SLEEP_S4;
    if (strcmp(name, "S5") == 0) return ACPI_SLEEP_S5;

    return -1;
}

/**
 * acpi_get_sleep_state_name - Get sleep state name
 * @state: State number
 *
 * Returns: State name string
 */
const char *acpi_get_sleep_state_name(u32 state)
{
    switch (state) {
        case ACPI_SLEEP_S0: return "S0 (Working)";
        case ACPI_SLEEP_S1: return "S1 (CPU Stopped)";
        case ACPI_SLEEP_S2: return "S2 (CPU Off)";
        case ACPI_SLEEP_S3: return "S3 (Suspend to RAM)";
        case ACPI_SLEEP_S4: return "S4 (Hibernation)";
        case ACPI_SLEEP_S5: return "S5 (Soft Off)";
        default: return "Unknown";
    }
}

/**
 * acpi_enter_sleep_state - Enter ACPI sleep state
 * @state: Sleep state (S1-S5)
 *
 * Returns: 0 on success, negative error code on failure
 */
int acpi_enter_sleep_state(u32 state)
{
    acpi_sleep_state_t *sleep_state;
    u16 pm1a_value, pm1b_value;
    u32 i;

    if (!acpi_manager.initialized) {
        return -ENODEV;
    }

    if (state > ACPI_SLEEP_S5) {
        return -EINVAL;
    }

    sleep_state = &acpi_manager.acpi.sleep_states[state];

    if (!sleep_state->valid) {
        pr_err("ACPI: Sleep state S%u not valid\n", state);
        return -EINVAL;
    }

    pr_info("ACPI: Entering %s...\n", acpi_get_sleep_state_name(state));

    /* Disable interrupts */
    local_irq_disable();

    /* Build PM1 control values */
    pm1a_value = (sleep_state->sleep_type_a << 4) | PM1_SLP_EN;
    pm1b_value = (sleep_state->sleep_type_b << 4) | PM1_SLP_EN;

    /* Write to PM1a control register */
    if (acpi_manager.acpi.pm1a_cnt_addr) {
        /* outw(pm1a_value, acpi_manager.acpi.pm1a_cnt_addr); */
    }

    /* Write to PM1b control register if present */
    if (acpi_manager.acpi.pm1b_cnt_addr) {
        /* outw(pm1b_value, acpi_manager.acpi.pm1b_cnt_addr); */
    }

    /* Wait for sleep (CPU will halt) */
    for (i = 0; i < 1000; i++) {
        __asm__ __volatile__("hlt" ::: "memory");
    }

    /* Should not reach here unless waking up */
    acpi_manager.wakeup_count++;
    acpi_manager.total_sleep_time += get_time_ns() - acpi_manager.last_sleep_time;

    pr_info("ACPI: Woke up from %s\n", acpi_get_sleep_state_name(state));

    return 0;
}

/*===========================================================================*/
/*                         ACPI INFORMATION                                  */
/*===========================================================================*/

/**
 * acpi_dump_tables - Dump ACPI table information
 */
void acpi_dump_tables(void)
{
    acpi_sdt_header_t *header;
    const char *signatures[] = {
        "APIC", "DSDT", "FACP", "FACS", "HPET",
        "MCFG", "SRAT", "SLIT", "SSDT", "WAET"
    };
    u32 i;

    if (!acpi_manager.initialized) {
        pr_info("ACPI: Not initialized\n");
        return;
    }

    pr_info("ACPI Tables:\n");
    pr_info("  Version: %u\n", acpi_manager.acpi.acpi_version);
    pr_info("  Total Tables: %u\n", acpi_manager.total_tables);
    pr_info("  RSDP: 0x%016X\n", (u64)acpi_manager.acpi.rsdp);

    if (acpi_manager.acpi.xsdt) {
        pr_info("  XSDT: 0x%016X\n", (u64)acpi_manager.acpi.xsdt);
    } else if (acpi_manager.acpi.rsdt) {
        pr_info("  RSDT: 0x%016X\n", (u64)acpi_manager.acpi.rsdt);
    }

    if (acpi_manager.acpi.fadt) {
        pr_info("  FADT: 0x%016X\n", (u64)acpi_manager.acpi.fadt);
    }

    if (acpi_manager.acpi.madt) {
        pr_info("  MADT: 0x%016X\n", (u64)acpi_manager.acpi.madt);
    }

    pr_info("\n");
    pr_info("Known Tables:\n");

    for (i = 0; i < ARRAY_SIZE(signatures); i++) {
        header = acpi_find_table(signatures[i], 0);
        if (header) {
            pr_info("  %.4s: 0x%016X (length=%u, rev=%u)\n",
                    header->signature,
                    (u64)header,
                    header->length,
                    header->revision);
        }
    }
}

/**
 * acpi_dump_info - Dump ACPI information for debugging
 */
void acpi_dump_info(void)
{
    u32 i;

    if (!acpi_manager.initialized) {
        pr_info("ACPI: Not initialized\n");
        return;
    }

    pr_info("ACPI Information:\n");
    pr_info("  Version: %u\n", acpi_manager.acpi.acpi_version);
    pr_info("  OEM ID: %.6s\n", acpi_manager.acpi.rsdp->oem_id);
    pr_info("  Total Wakeups: %u\n", acpi_manager.wakeup_count);
    pr_info("  Total Sleep Time: %llu ns\n", acpi_manager.total_sleep_time);
    pr_info("\n");

    pr_info("Sleep States:\n");
    for (i = 0; i < 6; i++) {
        acpi_sleep_state_t *state = &acpi_manager.acpi.sleep_states[i];
        if (state->valid) {
            pr_info("  S%u: TypeA=%u, TypeB=%u, Latency=%uus\n",
                    i, state->sleep_type_a, state->sleep_type_b, state->latency);
        }
    }
}

/*===========================================================================*/
/*                         ACPI SHUTDOWN                                     */
/*===========================================================================*/

/**
 * acpi_shutdown - Shutdown ACPI subsystem
 */
void acpi_shutdown(void)
{
    pr_info("ACPI: Shutting down...\n");

    /* Enter S5 (soft off) state */
    acpi_enter_sleep_state(ACPI_SLEEP_S5);

    acpi_manager.initialized = false;

    pr_info("ACPI: Shutdown complete\n");
}
