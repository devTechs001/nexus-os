/*
 * NEXUS OS - Text Console
 * drivers/video/console.h
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NEXUS_DRIVERS_VIDEO_CONSOLE_H
#define _NEXUS_DRIVERS_VIDEO_CONSOLE_H

#include "../../kernel/include/types.h"

/* Initialise the text-mode console and clear the screen. */
void console_init(void);

/* Clear the console. */
void console_clear(void);

/*
 * Print a formatted string to the console. Output goes to VGA text mode
 * and the serial port - it is NOT visible once the bootloader has set a
 * linear framebuffer graphics mode, which is why the splash uses the
 * bitmap font renderer instead.
 */
void console_print(const char *fmt, ...);

#endif /* _NEXUS_DRIVERS_VIDEO_CONSOLE_H */
