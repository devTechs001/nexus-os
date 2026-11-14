/*
 * NEXUS OS - Keyboard character access
 * drivers/input/input_chars.h
 *
 * A narrow, type-free view of the keyboard for GUI code.
 *
 * Why this header exists: the tree contains two incompatible definitions of
 * input_event_t - a flat one in drivers/input/input.h and a struct with a
 * mouse/keyboard/touch union in gui/gui.h. Including both is a compile error,
 * and including either drags the whole driver (or the whole GUI) into a widget
 * source file. GUI code that only wants "what did the user type" can include
 * this instead and stay clear of both.
 *
 * Implemented in drivers/input/input.c, which owns the real event queue.
 */

#ifndef _NEXUS_INPUT_CHARS_H
#define _NEXUS_INPUT_CHARS_H

#include "../../kernel/include/types.h"

#define INPUT_KEY_NONE      0

/**
 * input_poll_char - Pop the next typed character
 *
 * Returns: The character typed (0x20..0x7E), or one of the values below:
 *          INPUT_KEY_NONE         no character pending
 *          INPUT_KEY_CHAR_ENTER   Enter/Return
 *          INPUT_KEY_CHAR_BACKSPACE
 *          INPUT_KEY_CHAR_ESCAPE
 */
#define INPUT_KEY_CHAR_ENTER        '\n'
#define INPUT_KEY_CHAR_BACKSPACE    '\b'
#define INPUT_KEY_CHAR_ESCAPE       27

int input_poll_char(void);

/** Flush any pending typed characters. */
void input_flush_chars(void);

#endif /* _NEXUS_INPUT_CHARS_H */
