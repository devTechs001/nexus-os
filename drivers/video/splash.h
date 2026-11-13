/*
 * NEXUS OS - Graphical Boot Splash
 * drivers/video/splash.h
 */

#ifndef _NEXUS_DRIVERS_VIDEO_SPLASH_H
#define _NEXUS_DRIVERS_VIDEO_SPLASH_H

#include "../../kernel/include/types.h"

/**
 * splash_init - Start the graphical boot splash on a linear framebuffer.
 * If @fb is NULL the splash is skipped and boot continues in text mode.
 */
void splash_init(u32 *fb, u32 width, u32 height, u32 pitch, u32 bpp);

/** Advance the progress bar toward @percent, annotating with @status. */
void splash_update_progress(u32 percent, const char *status);

/** Show a transient message above the progress bar. */
void splash_show_message(const char *message);

/** Pin the splash at 100% (final frame stays on screen). */
void splash_done(void);

/** Advance one animation frame; cheap, safe to call from the timer tick. */
void splash_animate(void);

/** True while the splash owns the framebuffer. */
bool splash_is_active(void);

/** Print splash diagnostics to the console. */
void splash_test(void);

#endif /* _NEXUS_DRIVERS_VIDEO_SPLASH_H */
