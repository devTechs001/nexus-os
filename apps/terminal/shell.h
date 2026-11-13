/*
 * NEXUS OS - Shell Interface
 * apps/terminal/shell.h
 *
 * Public surface of the NEXUS command interpreter (apps/terminal/shell.c).
 */

#ifndef _NEXUS_SHELL_H
#define _NEXUS_SHELL_H

#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                              LIMITS                                       */
/*===========================================================================*/

/*
 * Shared limits. terminal.c feeds a line buffer sized by SHELL_MAX_LINE, so
 * this has to be visible through the header rather than being private to
 * shell.c.
 */
#define SHELL_MAX_LINE          4096
#define SHELL_MAX_ARGS          64

#ifdef __cplusplus
extern "C" {
#endif

/** Output callback. The shell never draws; the caller decides where bytes go. */
typedef void (*shell_write_fn)(void *ctx, const char *data, u32 len);

/* Lifecycle */
int  shell_init(void);
void shell_shutdown(void);
bool shell_is_running(void);

/* Wiring */
void shell_set_output(shell_write_fn fn, void *ctx);
void shell_set_error(shell_write_fn fn, void *ctx);

/* Driving it */
int  shell_submit_line(const char *line);
int  shell_execute_line(const char *line);
void shell_print_prompt(void);

/* State */
u32  shell_exit_status(void);
u32  shell_get_pid(void);
u32  shell_var_count(void);

/* Environment (also usable to configure the shell before it starts) */
int  shell_setenv(const char *name, const char *value, bool exported);
const char *shell_getenv(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_SHELL_H */
