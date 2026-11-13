/*
 * NEXUS OS - Language Runtime Interface
 * apps/terminal/lang_runtime.h
 */

#ifndef _NEXUS_LANG_RUNTIME_H
#define _NEXUS_LANG_RUNTIME_H

#include "../../kernel/include/types.h"

/* Provided by shell.c */
const char *shell_getenv(const char *name);

/* ELF64 loader (kernel/core/elf_loader.c) */
int  elf_exec(const char *path, u64 *entry, u64 *image_size, u32 *segments);
bool elf_can_execute(const char *path);

/* Process execution (kernel/sched/process.c) */
struct task_struct;
int  exec_process(struct task_struct *task, const char *path,
                  char **argv, char **envp);
struct task_struct *get_current(void);

#ifdef __cplusplus
extern "C" {
#endif

/* Output helpers, provided by shell.c so language reporting uses the shell's
 * active sink (terminal, console or redirection) rather than printk. */
void shell_puts_ex(const char *text);
void shell_printf_ex(const char *fmt, ...);
void shell_error_ex(const char *fmt, ...);

/* Path resolution */
bool lang_find_in_path(const char *name, char *out, u32 size);

/* Detection and execution */
const char *lang_name_for_path(const char *path);
int  lang_exec_file(const char *path);

/* Reporting */
void lang_runtime_status(void);
int  lang_mern_status(void);

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_LANG_RUNTIME_H */
