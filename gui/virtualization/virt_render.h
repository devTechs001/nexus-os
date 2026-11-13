/*
 * NEXUS OS - VM Manager GUI Render Backend Header
 * gui/virtualization/virt_render.h
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef VIRT_RENDER_H
#define VIRT_RENDER_H

#include "../../kernel/include/types.h"
#include "../../drivers/display/display.h"

/* A 32bpp linear destination surface */
typedef struct {
    u32 *fb;        /* base pointer */
    u32 width;      /* pixels per scanline */
    u32 height;     /* scanlines */
    u32 pitch;      /* bytes per scanline */
    bool ready;
} virt_surface_t;

/* Init / attach */
int  virt_render_init(u32 *fb, u32 width, u32 height, u32 pitch);
int  virt_render_attach_display(void);
bool virt_render_ready(void);
u32  virt_render_width(void);
u32  virt_render_height(void);
virt_surface_t *virt_render_surface(void);

/* Clipping test */
bool virt_clip_visible(s32 x, s32 y);

/* Pixels */
void virt_pixel(s32 x, s32 y, u32 color);
void virt_blend_pixel(s32 x, s32 y, u32 color, u8 alpha);

/* Rectangles */
void virt_fill_rect(s32 x, s32 y, u32 width, u32 height, u32 color);
void virt_gradient_rect(s32 x, s32 y, u32 width, u32 height, u32 top, u32 bottom);
void virt_rect_outline(s32 x, s32 y, u32 width, u32 height, u32 thickness, u32 color);
void virt_rounded_rect(s32 x, s32 y, u32 width, u32 height, u32 radius, u32 color);

/* Lines and circles */
void virt_line(s32 x0, s32 y0, s32 x1, s32 y1, u32 color);
void virt_circle(s32 cx, s32 cy, u32 radius, u32 color, bool filled);

/* Text */
void virt_text(s32 x, s32 y, const char *text, u32 color, u32 scale, u8 opacity);
u32  virt_text_width(const char *text, u32 scale);
void virt_text_centered(s32 cx, s32 y, const char *text, u32 color, u32 scale, u8 opacity);

#endif /* VIRT_RENDER_H */
