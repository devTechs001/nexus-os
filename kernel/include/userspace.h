/*
 * NEXUS OS - Boot-to-userspace handoff state
 * kernel/include/userspace.h
 *
 * kernel/core/kernel.c fills these in while parsing the multiboot2 tags and
 * the boot command line. kernel/sched/process.c reads them to start PID 1, and
 * kernel/syscall/syscall_handlers.c reads the command line for
 * SYS_GET_CMDLINE.
 *
 * Why this header exists
 * ----------------------
 * These symbols were declared `extern` at each use site, in three different
 * files, and defined in one. Nothing checked that the declaration and the
 * definition agreed, and nothing forced the defining translation unit to be
 * present.
 *
 * The failure that produced was confusing rather than wrong: a tree where
 * process.c and syscall_handlers.c had been updated but kernel.c had not
 * compiled cleanly, printed no warning, and failed only at the final link with
 *
 *     undefined reference to `g_userspace_module_present'
 *
 * followed by a wall of `multiple definition` errors from a stale Makefile.
 * Both halves of that report were symptoms of a partial update, and neither
 * pointed at the real cause.
 *
 * Declaring the handoff state in one place means a stale kernel.c now fails
 * with a missing-definition error in a file that visibly owns these globals,
 * instead of an anonymous undefined reference in the scheduler.
 */

#ifndef _NEXUS_USERSPACE_H
#define _NEXUS_USERSPACE_H

#include "types.h"

/*
 * The command line actually in force for this boot: either the one GRUB
 * supplied or whatever the interactive boot menu selected.
 *
 * NUL-terminated, never NULL, possibly empty. Bounded by the array size; the
 * bootloader's string is attacker-controlled and may be longer.
 *
 * Defined in kernel/core/kernel.c.
 */
#define BOOT_CMDLINE_MAX 512
extern char g_boot_cmdline_effective[BOOT_CMDLINE_MAX];

/**
 * boot_cmdline_set_effective - Record the command line userspace should see
 * @src: NUL-terminated option string to copy (NULL clears it)
 *
 * Bounded copy into g_boot_cmdline_effective.
 */
void boot_cmdline_set_effective(const char *src);

/*
 * The userspace init image, captured from the multiboot2 module tag (type 3)
 * during kernel_main().
 *
 * These are physical addresses as the bootloader reported them; the kernel does
 * not copy the module. g_userspace_module_present is false when the ISO carried
 * no module, which is the signal for PID 1 to stay in-kernel rather than fault.
 *
 * Defined in kernel/core/kernel.c.
 */
#define USERSPACE_MODULE_NAME_MAX 128

extern u64  g_userspace_module_start;
extern u64  g_userspace_module_end;
extern char g_userspace_module_name[USERSPACE_MODULE_NAME_MAX];
extern bool g_userspace_module_present;

/**
 * boot_userspace_module_size - Byte count of the init module
 *
 * Returns 0 when no module was supplied.
 */
static inline u64 boot_userspace_module_size(void)
{
    if (!g_userspace_module_present ||
        g_userspace_module_end <= g_userspace_module_start) {
        return 0;
    }

    return g_userspace_module_end - g_userspace_module_start;
}

#endif /* _NEXUS_USERSPACE_H */
