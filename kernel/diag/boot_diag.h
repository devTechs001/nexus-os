/*
 * NEXUS OS - Real-Time Boot Diagnostics Header
 * kernel/diag/boot_diag.h
 */

#ifndef _BOOT_DIAG_H
#define _BOOT_DIAG_H

#include "../include/types.h"

/* Boot stage identifiers */
typedef enum {
    BOOT_STAGE_UNKNOWN      = 0,
    BOOT_STAGE_EARLY        = 1,
    BOOT_STAGE_SERIAL       = 2,
    BOOT_STAGE_MULTIBOOT    = 3,
    BOOT_STAGE_GDT          = 4,
    BOOT_STAGE_IDT          = 5,
    BOOT_STAGE_PMM          = 6,
    BOOT_STAGE_VMM          = 7,
    BOOT_STAGE_HEAP         = 8,
    BOOT_STAGE_INTERRUPTS   = 9,
    BOOT_STAGE_SCHEDULER    = 10,
    BOOT_STAGE_SMP          = 11,
    BOOT_STAGE_DRIVERS      = 12,
    BOOT_STAGE_SERVICES     = 13,
    BOOT_STAGE_COMPLETE     = 14,
    BOOT_STAGE_FAILED       = 255
} boot_stage_t;

/* Initialize boot diagnostics */
void boot_diag_init(void);

/* Log boot stage */
void boot_diag_log(boot_stage_t stage, const char *message);

/* Log boot error */
void boot_diag_log_error(boot_stage_t stage, const char *message, u32 error_code);

/* Mark boot as complete */
void boot_diag_mark_complete(void);

/* Mark boot as failed */
void boot_diag_mark_failed(const char *reason);

/* Get current status */
const void* boot_diag_get_status(void);

/* Dump boot log to console */
void boot_diag_dump(void);

/* Proc interface */
s32 boot_diag_proc_read(void *buffer, u32 size, u32 offset);

/* Convenience macros */
#define BOOT_LOG(stage, msg) boot_diag_log(stage, msg)
#define BOOT_LOG_ERROR(stage, msg, err) boot_diag_log_error(stage, msg, err)
#define BOOT_SUCCESS(stage, msg) do { \
    boot_diag_log(stage, msg); \
} while(0)
#define BOOT_FAIL(stage, msg) do { \
    boot_diag_log_error(stage, msg, 0); \
    boot_diag_mark_failed(msg); \
} while(0)

#endif /* _BOOT_DIAG_H */
