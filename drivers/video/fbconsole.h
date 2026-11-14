/*
 * NEXUS OS - Framebuffer Text Console
 * drivers/video/fbconsole.h
 *
 * Makes the kernel log visible on a linear framebuffer, so a blank or failed
 * graphical boot can explain itself without a serial console.
 */

#ifndef NEXUS_FBCONSOLE_H
#define NEXUS_FBCONSOLE_H

#include "../../kernel/include/types.h"

/* Record one character of kernel output (called from the printk path). */
void fbcon_capture_char(char c);

/* Point the text console at a linear framebuffer (16/24/32bpp). */
void fbcon_set_framebuffer(void *fb, u32 width, u32 height, u32 pitch, u32 bpp);

/* True once a usable framebuffer has been set. */
bool fbcon_is_ready(void);

/* Fill the surface with a solid colour. */
void fbcon_clear(u32 color);

/* Paint the most recent kernel log lines onto the screen. */
void fbcon_render(u32 bg, u32 fg);

/* Paint one message line onto the screen. */
void fbcon_render_message(const char *msg, u32 bg, u32 fg);

#endif /* NEXUS_FBCONSOLE_H */
