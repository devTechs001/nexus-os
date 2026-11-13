/*
 * NEXUS OS - Bitmap Font Renderer
 * drivers/video/font8x16.h
 */

#ifndef _NEXUS_DRIVERS_VIDEO_FONT8X16_H
#define _NEXUS_DRIVERS_VIDEO_FONT8X16_H

#include "../../kernel/include/types.h"

/* Glyph cell geometry */
#define FONT8X16_WIDTH      8
#define FONT8X16_HEIGHT     16

/* Printable ASCII range covered by the table */
#define FONT8X16_FIRST      0x20
#define FONT8X16_LAST       0x7E
#define FONT8X16_GLYPHS     (FONT8X16_LAST - FONT8X16_FIRST + 1)

/* Opacity helpers */
#define FONT_OPAQUE         255
#define FONT_TRANSPARENT    0

/**
 * font8x16_glyph - Get the 16-byte glyph bitmap for a character.
 * Unknown characters return the '?' glyph rather than blank space.
 */
const u8 *font8x16_glyph(char c);

/**
 * font8x16_draw_char - Render one character into a linear framebuffer.
 *
 * @fb      : framebuffer base (32bpp XRGB/BGRX as used by the bootloader)
 * @fb_w/h  : framebuffer dimensions in pixels
 * @pitch   : bytes per scanline (pitch / 4 is the stride in pixels)
 * @x, y    : top-left of the glyph cell
 * @color   : 0x00RRGGBB
 * @scale   : integer magnification, minimum 1
 * @opacity : 0 = invisible, 255 = solid; in between blends with the
 *            existing pixels (used for splash fades)
 */
void font8x16_draw_char(u32 *fb, u32 fb_w, u32 fb_h, u32 pitch,
                        s32 x, s32 y, char c, u32 color, u32 scale,
                        u8 opacity);

/**
 * font8x16_draw_string - Render a NUL-terminated string, honouring '\n'.
 * Returns the x coordinate just past the last glyph drawn.
 */
s32 font8x16_draw_string(u32 *fb, u32 fb_w, u32 fb_h, u32 pitch,
                         s32 x, s32 y, const char *s, u32 color,
                         u32 scale, u8 opacity);

/** Width in pixels of a string at the given scale. */
u32 font8x16_string_width(const char *s, u32 scale);

/** Render a string horizontally centred on @cx. */
void font8x16_draw_string_centered(u32 *fb, u32 fb_w, u32 fb_h, u32 pitch,
                                   s32 cx, s32 y, const char *s, u32 color,
                                   u32 scale, u8 opacity);

#endif /* _NEXUS_DRIVERS_VIDEO_FONT8X16_H */
