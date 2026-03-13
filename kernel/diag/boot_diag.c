/*
 * NEXUS OS - Real-Time Boot Diagnostics
 * kernel/diag/boot_diag.c
 * 
 * This module provides real-time boot diagnostics and failure capture.
 * It logs all boot stages to a ring buffer and provides runtime access.
 */

#include "../include/kernel.h"
#include "../include/types.h"

/*===========================================================================*/
/*                         CONFIGURATION                                     */
/*===========================================================================*/

#define BOOT_DIAG_MAGIC         0xDEADB007
#define BOOT_DIAG_VERSION       1
#define BOOT_DIAG_BUFFER_SIZE   4096
#define BOOT_DIAG_MAX_ENTRIES   256

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Boot stage identifiers */
typedef enum {
    BOOT_STAGE_UNKNOWN      = 0,
    BOOT_STAGE_EARLY        = 1,    /* Early boot (before console) */
    BOOT_STAGE_SERIAL       = 2,    /* Serial initialized */
    BOOT_STAGE_MULTIBOOT    = 3,    /* Multiboot info parsed */
    BOOT_STAGE_GDT          = 4,    /* GDT initialized */
    BOOT_STAGE_IDT          = 5,    /* IDT initialized */
    BOOT_STAGE_PMM          = 6,    /* Physical memory manager */
    BOOT_STAGE_VMM          = 7,    /* Virtual memory manager */
    BOOT_STAGE_HEAP         = 8,    /* Kernel heap */
    BOOT_STAGE_INTERRUPTS   = 9,    /* Interrupt system */
    BOOT_STAGE_SCHEDULER    = 10,   /* Scheduler */
    BOOT_STAGE_SMP          = 11,   /* SMP initialization */
    BOOT_STAGE_DRIVERS      = 12,   /* Device drivers */
    BOOT_STAGE_SERVICES     = 13,   /* System services */
    BOOT_STAGE_COMPLETE     = 14,   /* Boot complete */
    BOOT_STAGE_FAILED       = 255   /* Boot failure */
} boot_stage_t;

/* Boot log entry */
typedef struct {
    u64 timestamp;
    boot_stage_t stage;
    u32 cpu_id;
    u32 error_code;
    char message[64];
} __attribute__((packed)) boot_log_entry_t;

/* Boot diagnostics ring buffer */
typedef struct {
    u32 magic;
    u32 version;
    u32 head;
    u32 tail;
    u32 count;
    u32 overflow;
    u64 boot_start_time;
    u64 boot_current_time;
    boot_stage_t current_stage;
    boot_stage_t last_successful_stage;
    bool boot_complete;
    bool boot_failed;
    char failure_message[128];
    boot_log_entry_t entries[BOOT_DIAG_MAX_ENTRIES];
} __attribute__((packed)) boot_diag_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

__section(".data") static boot_diag_t boot_diag = {
    .magic = BOOT_DIAG_MAGIC,
    .version = BOOT_DIAG_VERSION,
    .head = 0,
    .tail = 0,
    .count = 0,
    .overflow = 0,
    .boot_start_time = 0,
    .boot_current_time = 0,
    .current_stage = BOOT_STAGE_UNKNOWN,
    .last_successful_stage = BOOT_STAGE_UNKNOWN,
    .boot_complete = false,
    .boot_failed = false,
};

/*===========================================================================*/
/*                         EXTERNAL FUNCTIONS                                */
/*===========================================================================*/

extern u64 arch_get_ticks(void);
extern u32 arch_get_cpu_id(void);

/*===========================================================================*/
/*                         INTERNAL FUNCTIONS                                */
/*===========================================================================*/

/**
 * boot_diag_get_timestamp - Get current timestamp in microseconds
 */
static u64 boot_diag_get_timestamp(void)
{
    return arch_get_ticks() / 1000;  /* Convert to microseconds */
}

/**
 * boot_diag_add_entry - Add entry to ring buffer
 */
static void boot_diag_add_entry(boot_stage_t stage, const char *message, u32 error_code)
{
    boot_log_entry_t *entry;
    u32 idx;
    
    /* Get current index */
    idx = boot_diag.head;
    
    /* Get entry pointer */
    entry = &boot_diag.entries[idx];
    
    /* Fill entry */
    entry->timestamp = boot_diag_get_timestamp() - boot_diag.boot_start_time;
    entry->stage = stage;
    entry->cpu_id = arch_get_cpu_id();
    entry->error_code = error_code;
    
    /* Copy message (safe copy) */
    u32 i;
    for (i = 0; i < 63 && message[i] != '\0'; i++) {
        entry->message[i] = message[i];
    }
    entry->message[i] = '\0';
    
    /* Update head */
    boot_diag.head = (boot_diag.head + 1) % BOOT_DIAG_MAX_ENTRIES;
    
    /* Update count */
    if (boot_diag.count < BOOT_DIAG_MAX_ENTRIES) {
        boot_diag.count++;
    } else {
        boot_diag.overflow++;
        boot_diag.tail = (boot_diag.tail + 1) % BOOT_DIAG_MAX_ENTRIES;
    }
    
    /* Update current stage */
    boot_diag.current_stage = stage;
    boot_diag.boot_current_time = entry->timestamp;
}

/*===========================================================================*/
/*                         PUBLIC API                                        */
/*===========================================================================*/

/**
 * boot_diag_init - Initialize boot diagnostics
 */
void boot_diag_init(void)
{
    boot_diag.magic = BOOT_DIAG_MAGIC;
    boot_diag.version = BOOT_DIAG_VERSION;
    boot_diag.head = 0;
    boot_diag.tail = 0;
    boot_diag.count = 0;
    boot_diag.overflow = 0;
    boot_diag.boot_start_time = boot_diag_get_timestamp();
    boot_diag.boot_current_time = boot_diag.boot_start_time;
    boot_diag.current_stage = BOOT_STAGE_EARLY;
    boot_diag.last_successful_stage = BOOT_STAGE_EARLY;
    boot_diag.boot_complete = false;
    boot_diag.boot_failed = false;
    
    boot_diag_add_entry(BOOT_STAGE_EARLY, "Boot diagnostics initialized", 0);
}

/**
 * boot_diag_log - Log boot stage
 */
void boot_diag_log(boot_stage_t stage, const char *message)
{
    if (boot_diag.magic != BOOT_DIAG_MAGIC) {
        return;  /* Not initialized */
    }
    
    boot_diag_add_entry(stage, message, 0);
    
    /* Update last successful stage */
    if (stage != BOOT_STAGE_FAILED) {
        boot_diag.last_successful_stage = stage;
    }
}

/**
 * boot_diag_log_error - Log boot error
 */
void boot_diag_log_error(boot_stage_t stage, const char *message, u32 error_code)
{
    if (boot_diag.magic != BOOT_DIAG_MAGIC) {
        return;  /* Not initialized */
    }
    
    boot_diag_add_entry(stage, message, error_code);
    boot_diag.boot_failed = true;
    
    /* Copy failure message */
    u32 i;
    for (i = 0; i < 127 && message[i] != '\0'; i++) {
        boot_diag.failure_message[i] = message[i];
    }
    boot_diag.failure_message[i] = '\0';
}

/**
 * boot_diag_mark_complete - Mark boot as complete
 */
void boot_diag_mark_complete(void)
{
    if (boot_diag.magic != BOOT_DIAG_MAGIC) {
        return;
    }
    
    boot_diag.boot_complete = true;
    boot_diag.current_stage = BOOT_STAGE_COMPLETE;
    boot_diag_add_entry(BOOT_STAGE_COMPLETE, "Boot complete", 0);
}

/**
 * boot_diag_mark_failed - Mark boot as failed
 */
void boot_diag_mark_failed(const char *reason)
{
    if (boot_diag.magic != BOOT_DIAG_MAGIC) {
        return;
    }
    
    boot_diag.boot_failed = true;
    boot_diag.current_stage = BOOT_STAGE_FAILED;
    boot_diag_log_error(BOOT_STAGE_FAILED, reason, 0);
}

/**
 * boot_diag_get_status - Get current boot status
 */
const boot_diag_t* boot_diag_get_status(void)
{
    return &boot_diag;
}

/**
 * boot_diag_dump - Dump boot log to console
 */
void boot_diag_dump(void)
{
    u32 i;
    u32 idx;
    boot_log_entry_t *entry;
    
    printk("\n=== BOOT DIAGNOSTICS DUMP ===\n");
    printk("Version: %u\n", boot_diag.version);
    printk("Boot start time: %u us\n", (u32)boot_diag.boot_start_time);
    printk("Current time: %u us\n", (u32)boot_diag.boot_current_time);
    printk("Entries: %u (overflow: %u)\n", boot_diag.count, boot_diag.overflow);
    printk("Current stage: %u\n", boot_diag.current_stage);
    printk("Last successful: %u\n", boot_diag.last_successful_stage);
    printk("Boot complete: %s\n", boot_diag.boot_complete ? "yes" : "no");
    printk("Boot failed: %s\n", boot_diag.boot_failed ? "yes" : "no");
    
    if (boot_diag.boot_failed) {
        printk("Failure message: %s\n", boot_diag.failure_message);
    }
    
    printk("\n=== BOOT LOG ===\n");
    
    /* Dump entries */
    idx = boot_diag.tail;
    for (i = 0; i < boot_diag.count; i++) {
        entry = &boot_diag.entries[idx];
        printk("[%6u us] CPU%u Stage %2u: %s",
               (u32)entry->timestamp,
               entry->cpu_id,
               entry->stage,
               entry->message);
        if (entry->error_code != 0) {
            printk(" (error: %u)", entry->error_code);
        }
        printk("\n");
        
        idx = (idx + 1) % BOOT_DIAG_MAX_ENTRIES;
    }
    
    printk("=== END BOOT DIAGNOSTICS ===\n\n");
}

/*===========================================================================*/
/*                         PROC INTERFACE                                    */
/*===========================================================================*/

/**
 * boot_diag_proc_read - Read boot diagnostics via proc
 */
s32 boot_diag_proc_read(void *buffer, u32 size, u32 offset)
{
    char *buf = (char *)buffer;
    u32 len = 0;
    u32 i;
    u32 idx;
    boot_log_entry_t *entry;
    
    if (offset != 0) {
        return 0;  /* No support for offset reads */
    }
    
    /* Header */
    len += snprintf(buf + len, size - len, 
                    "NEXUS OS Boot Diagnostics\n");
    len += snprintf(buf + len, size - len,
                    "========================\n\n");
    len += snprintf(buf + len, size - len,
                    "Status: %s\n",
                    boot_diag.boot_failed ? "FAILED" : 
                    (boot_diag.boot_complete ? "COMPLETE" : "IN_PROGRESS"));
    len += snprintf(buf + len, size - len,
                    "Stage: %u\n", boot_diag.current_stage);
    len += snprintf(buf + len, size - len,
                    "Boot time: %u us\n\n",
                    (u32)(boot_diag.boot_current_time - boot_diag.boot_start_time));
    
    /* Log entries */
    len += snprintf(buf + len, size - len, "Boot Log:\n");
    idx = boot_diag.tail;
    for (i = 0; i < boot_diag.count && len < size - 64; i++) {
        entry = &boot_diag.entries[idx];
        len += snprintf(buf + len, size - len, 
                        "  [%6u] %s\n",
                        (u32)entry->timestamp,
                        entry->message);
        idx = (idx + 1) % BOOT_DIAG_MAX_ENTRIES;
    }
    
    return (s32)len;
}
