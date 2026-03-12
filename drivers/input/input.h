/*
 * NEXUS OS - Input Driver
 * drivers/input/input.h
 *
 * Input driver for keyboard, mouse, touchpad, touchscreen
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _INPUT_H
#define _INPUT_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         INPUT CONFIGURATION                               */
/*===========================================================================*/

#define INPUT_MAX_DEVICES           32
#define INPUT_MAX_KEYS              256
#define INPUT_MAX_AXES              16
#define INPUT_MAX_BUTTONS           32

/*===========================================================================*/
/*                         INPUT DEVICE TYPES                                */
/*===========================================================================*/

#define INPUT_TYPE_KEYBOARD         0
#define INPUT_TYPE_MOUSE            1
#define INPUT_TYPE_TOUCHPAD         2
#define INPUT_TYPE_TOUCHSCREEN      3
#define INPUT_TYPE_TABLET           4
#define INPUT_TYPE_JOYSTICK         5
#define INPUT_TYPE_GAMEPAD          6

/*===========================================================================*/
/*                         KEY CODES                                         */
/*===========================================================================*/

#define KEY_ESCAPE                  0x01
#define KEY_F1                      0x3B
#define KEY_F12                     0x58
#define KEY_ENTER                   0x1C
#define KEY_BACKSPACE               0x0E
#define KEY_TAB                     0x0F
#define KEY_SPACE                   0x39
#define KEY_LEFT_CTRL               0x1D
#define KEY_RIGHT_CTRL              0x9D
#define KEY_LEFT_SHIFT              0x2A
#define KEY_RIGHT_SHIFT             0x36
#define KEY_LEFT_ALT                0x38
#define KEY_RIGHT_ALT               0xB8
#define KEY_LEFT_SUPER              0xDB
#define KEY_RIGHT_SUPER             0xDC
#define KEY_MENU                    0xDD

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Input Event
 */
typedef struct {
    u32 type;                       /* Event Type */
    u32 code;                       /* Event Code */
    s32 value;                      /* Event Value */
    u64 timestamp;                  /* Timestamp */
} input_event_t;

/**
 * Input Device
 */
typedef struct {
    u32 device_id;                  /* Device ID */
    char name[64];                  /* Device Name */
    u32 type;                       /* Device Type */
    u32 vendor_id;                  /* Vendor ID */
    u32 product_id;                 /* Product ID */
    bool is_connected;              /* Is Connected */
    bool is_enabled;                /* Is Enabled */
    u8 key_state[INPUT_MAX_KEYS / 8]; /* Key State */
    s32 axes[INPUT_MAX_AXES];       /* Axis Values */
    u32 buttons;                    /* Button State */
    s32 rel_x;                      /* Relative X */
    s32 rel_y;                      /* Relative Y */
    s32 rel_z;                      /* Relative Z */
    void *driver_data;              /* Driver Data */
} input_device_t;

/**
 * Input Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    input_device_t devices[INPUT_MAX_DEVICES]; /* Devices */
    u32 device_count;               /* Device Count */
    input_event_t event_queue[256]; /* Event Queue */
    u32 event_head;                 /* Event Queue Head */
    u32 event_tail;                 /* Event Queue Tail */
    void (*on_event)(input_event_t *);
    void *user_data;                /* User Data */
} input_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int input_init(void);
int input_shutdown(void);
bool input_is_initialized(void);

/* Device management */
int input_enumerate_devices(void);
input_device_t *input_get_device(u32 device_id);
int input_list_devices(input_device_t *devices, u32 *count);
int input_get_device_count(void);

/* Device control */
int input_enable_device(u32 device_id);
int input_disable_device(u32 device_id);
int input_set_led(u32 device_id, u32 led, bool state);

/* Event handling */
int input_read_event(input_event_t *event);
int input_poll_event(input_event_t *event, u32 timeout_ms);
int input_flush_events(void);
int input_set_event_callback(void (*callback)(input_event_t *));

/* Keyboard */
bool input_is_key_pressed(u32 key);
int input_get_key_state(u32 key);
int input_set_key_repeat_rate(u32 delay, u32 rate);

/* Mouse */
int input_get_mouse_position(s32 *x, s32 *y);
int input_set_mouse_position(s32 x, s32 y);
int input_get_mouse_buttons(u32 *buttons);
int input_set_mouse_speed(u32 speed);

/* Touchpad */
int input_set_touchpad_enabled(bool enabled);
int input_set_touchpad_natural_scroll(bool natural);
int input_set_touchpad_tap_to_click(bool enabled);

/* Utility functions */
const char *input_get_type_name(u32 type);
const char *input_get_key_name(u32 key);

/* Global instance */
input_driver_t *input_get_driver(void);

#endif /* _INPUT_H */
