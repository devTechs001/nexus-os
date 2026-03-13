/*
 * NEXUS OS - Input Drivers
 * drivers/input/input.h
 *
 * Comprehensive input driver support for PS/2, USB, touchpad, 
 * touchscreen, joystick/gamepad
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _INPUT_H
#define _INPUT_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         INPUT CONFIGURATION                               */
/*===========================================================================*/

#define INPUT_MAX_DEVICES           64
#define INPUT_MAX_KEYS              512
#define INPUT_MAX_AXES              32
#define INPUT_MAX_BUTTONS           64
#define INPUT_EVENT_QUEUE_SIZE      1024

/*===========================================================================*/
/*                         INPUT DEVICE TYPES                                */
/*===========================================================================*/

#define INPUT_TYPE_KEYBOARD         1
#define INPUT_TYPE_MOUSE            2
#define INPUT_TYPE_TOUCHPAD         3
#define INPUT_TYPE_TOUCHSCREEN      4
#define INPUT_TYPE_TABLET           5
#define INPUT_TYPE_JOYSTICK         6
#define INPUT_TYPE_GAMEPAD          7
#define INPUT_TYPE_TRACKBALL        8
#define INPUT_TYPE_TRACKPOINT       9
#define INPUT_TYPE_ACCELEROMETER    10
#define INPUT_TYPE_GYROSCOPE        11

/*===========================================================================*/
/*                         INPUT BUS TYPES                                   */
/*===========================================================================*/

#define INPUT_BUS_UNKNOWN           0
#define INPUT_BUS_PS2               1
#define INPUT_BUS_USB               2
#define INPUT_BUS_BLUETOOTH         3
#define INPUT_BUS_I2C               4
#define INPUT_BUS_SPI               5
#define INPUT_BUS_SERIAL            6
#define INPUT_BUS_VIRTUAL           7

/*===========================================================================*/
/*                         EVENT TYPES                                       */
/*===========================================================================*/

#define EV_SYN                      0x00
#define EV_KEY                      0x01
#define EV_REL                      0x02
#define EV_ABS                      0x03
#define EV_MSC                      0x04
#define EV_SW                       0x05
#define EV_LED                      0x11
#define EV_SND                      0x12
#define EV_REP                      0x14
#define EV_FF                       0x15
#define EV_PWR                      0x16
#define EV_FF_STATUS                0x17
#define EV_MAX                      0x1f

/* Absolute Axes */
#define ABS_X                       0
#define ABS_Y                       1
#define ABS_Z                       2
#define ABS_RX                      3
#define ABS_RY                      4
#define ABS_RZ                      5
#define ABS_MAX                     0x3f

/* Relative Axes */
#define REL_X                       0
#define REL_Y                       1
#define REL_Z                       2
#define REL_RX                      3
#define REL_RY                      4
#define REL_RZ                      5
#define REL_MAX                     0x0f

/* LEDs */
#define LED_NUML                    0
#define LED_CAPSL                   1
#define LED_SCROLLL                 2
#define LED_COMPOSE                 3
#define LED_KANA                    4
#define LED_SLEEP                 5
#define LED_SUSPEND               6
#define LED_MUTE                  7
#define LED_MISC                  8
#define LED_MAIL                  9
#define LED_CHARGING              10
#define LED_MAX                   0x0f

/*===========================================================================*/
/*                         KEY CODES (Subset)                                */
/*===========================================================================*/

#define KEY_RESERVED                0
#define KEY_ESC                     1
#define KEY_1                       2
#define KEY_9                       10
#define KEY_0                       11
#define KEY_MINUS                   12
#define KEY_EQUAL                   13
#define KEY_BACKSPACE               14
#define KEY_TAB                     15
#define KEY_Q                       16
#define KEY_P                       25
#define KEY_ENTER                   28
#define KEY_LEFTCTRL                29
#define KEY_A                       30
#define KEY_L                       38
#define KEY_LEFTSHIFT               42
#define KEY_Z                       44
#define KEY_M                       50
#define KEY_RIGHTSHIFT              54
#define KEY_SPACE                   57
#define KEY_F1                      59
#define KEY_F12                     88
#define KEY_LEFTALT                 56
#define KEY_RIGHTALT                100
#define KEY_LEFTMETA                125
#define KEY_RIGHTMETA               126
#define KEY_MENU                    139

/* Mouse buttons */
#define BTN_LEFT                    0x110
#define BTN_RIGHT                   0x111
#define BTN_MIDDLE                  0x112
#define BTN_SIDE                    0x113
#define BTN_EXTRA                   0x114

/* Gamepad buttons */
#define BTN_GAMEPAD_A               0x130
#define BTN_GAMEPAD_B               0x131
#define BTN_GAMEPAD_X               0x133
#define BTN_GAMEPAD_Y               0x134
#define BTN_GAMEPAD_LB              0x136
#define BTN_GAMEPAD_RB              0x137
#define BTN_GAMEPAD_LT              0x138
#define BTN_GAMEPAD_RT              0x139
#define BTN_GAMEPAD_SELECT          0x13a
#define BTN_GAMEPAD_START           0x13b
#define BTN_GAMEPAD_L3              0x13c
#define BTN_GAMEPAD_R3              0x13d
#define BTN_GAMEPAD_UP              0x220
#define BTN_GAMEPAD_DOWN            0x221
#define BTN_GAMEPAD_LEFT            0x222
#define BTN_GAMEPAD_RIGHT           0x223

/*===========================================================================*/
/*                         ABSOLUTE AXES                                     */
/*===========================================================================*/

#define ABS_X                       0x00
#define ABS_Y                       0x01
#define ABS_Z                       0x02
#define ABS_RX                      0x03
#define ABS_RY                      0x04
#define ABS_RZ                      0x05
#define ABS_THROTTLE                0x06
#define ABS_RUDDER                0x07
#define ABS_WHEEL                   0x08
#define ABS_GAS                     0x09
#define ABS_BRAKE                   0x0a
#define ABS_HAT0X                   0x10
#define ABS_HAT0Y                   0x11
#define ABS_HAT1X                   0x12
#define ABS_HAT1Y                   0x13
#define ABS_HAT2X                   0x14
#define ABS_HAT2Y                   0x15
#define ABS_HAT3X                   0x16
#define ABS_HAT3Y                   0x17
#define ABS_PRESSURE                0x18
#define ABS_DISTANCE                0x19
#define ABS_TILT_X                  0x1a
#define ABS_TILT_Y                  0x1b
#define ABS_TOOL_WIDTH              0x1c
#define ABS_VOLUME                  0x20
#define ABS_MISC                    0x28

/*===========================================================================*/
/*                         LED CODES                                         */
/*===========================================================================*/

#define LED_NUML                    0x00
#define LED_CAPSL                   0x01
#define LED_SCROLLL                 0x02
#define LED_COMPOSE                 0x03
#define LED_KANA                    0x04
#define LED_SLEEP                   0x05
#define LED_SUSPEND                 0x06
#define LED_MUTE                    0x07
#define LED_MISC                    0x08
#define LED_MAIL                    0x09
#define LED_CHARGING                0x0a

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Input Event
 */
typedef struct {
    u64 timestamp;                  /* Timestamp (microseconds) */
    u32 type;                       /* Event Type */
    u32 code;                       /* Event Code */
    s32 value;                      /* Event Value */
} input_event_t;

/**
 * Input Device Capabilities
 */
typedef struct {
    u8 ev_types[EV_MAX / 8 + 1];   /* Event Types */
    u8 key_bits[INPUT_MAX_KEYS / 8 + 1]; /* Key Bits */
    u8 abs_bits[ABS_MAX + 1];      /* Absolute Axes */
    u8 rel_bits[REL_MAX + 1];      /* Relative Axes */
    u8 led_bits[LED_MAX + 1];      /* LED Bits */
    s32 abs_min[ABS_MAX + 1];      /* Absolute Min Values */
    s32 abs_max[ABS_MAX + 1];      /* Absolute Max Values */
    s32 abs_fuzz[ABS_MAX + 1];     /* Absolute Fuzz Values */
    s32 abs_flat[ABS_MAX + 1];     /* Absolute Flat Values */
} input_capabilities_t;

/**
 * Input Device Info
 */
typedef struct {
    char name[128];                 /* Device Name */
    char phys[64];                  /* Physical Path */
    char uniq[64];                  /* Unique ID */
    u16 bustype;                    /* Bus Type */
    u16 vendor;                     /* Vendor ID */
    u16 product;                    /* Product ID */
    u16 version;                    /* Version */
} input_devinfo_t;

/**
 * Input Device
 */
typedef struct input_device {
    u32 device_id;                  /* Device ID */
    input_devinfo_t info;           /* Device Info */
    u32 type;                       /* Device Type */
    bool is_connected;              /* Is Connected */
    bool is_enabled;                /* Is Enabled */
    bool is_open;                   /* Is Open */
    input_capabilities_t caps;      /* Capabilities */
    u8 key_state[INPUT_MAX_KEYS / 8 + 1]; /* Key State */
    s32 abs_state[ABS_MAX + 1];    /* Absolute State */
    s32 rel_state[REL_MAX + 1];    /* Relative State */
    u32 led_state;                  /* LED State */
    void *private_data;             /* Driver Private Data */
    int (*open)(struct input_device *dev);
    int (*close)(struct input_device *dev);
    int (*event)(struct input_device *dev, input_event_t *event);
} input_device_t;

/**
 * Input Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    input_device_t devices[INPUT_MAX_DEVICES]; /* Devices */
    u32 device_count;               /* Device Count */
    input_event_t event_queue[INPUT_EVENT_QUEUE_SIZE]; /* Event Queue */
    u32 event_head;                 /* Event Queue Head */
    u32 event_tail;                 /* Event Queue Tail */
    void (*on_event)(input_event_t *, void *);
    void *event_user_data;          /* Event Callback User Data */
    u32 key_repeat_delay;           /* Key Repeat Delay (ms) */
    u32 key_repeat_rate;            /* Key Repeat Rate (ms) */
    u32 mouse_speed;                /* Mouse Speed */
    bool natural_scroll;            /* Natural Scroll */
    bool tap_to_click;              /* Tap to Click */
    bool touchpad_enabled;          /* Touchpad Enabled */
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
int input_find_device_by_name(const char *name);

/* Device control */
int input_open_device(u32 device_id);
int input_close_device(u32 device_id);
int input_enable_device(u32 device_id);
int input_disable_device(u32 device_id);
int input_grab_device(u32 device_id);
int input_ungrab_device(u32 device_id);
int input_set_led(u32 device_id, u32 led, bool state);
int input_get_led_state(u32 device_id);

/* Event handling */
int input_read_event(input_event_t *event);
int input_poll_event(input_event_t *event, u32 timeout_ms);
int input_queue_event(input_event_t *event);
int input_flush_events(void);
int input_set_event_callback(void (*callback)(input_event_t *, void *), void *user_data);

/* Keyboard functions */
bool input_is_key_pressed(u32 key);
int input_get_key_state(u32 key);
int input_set_key_repeat(u32 delay, u32 rate);
int input_get_key_repeat(u32 *delay, u32 *rate);
const char *input_get_key_name(u32 key);

/* Mouse functions */
int input_get_mouse_position(s32 *x, s32 *y);
int input_set_mouse_position(s32 x, s32 y);
int input_get_mouse_buttons(u32 *buttons);
int input_set_mouse_speed(u32 speed);
int input_get_mouse_speed(void);
int input_mouse_button_press(u32 button);
int input_mouse_button_release(u32 button);

/* Touchpad functions */
int input_set_touchpad_enabled(bool enabled);
bool input_is_touchpad_enabled(void);
int input_set_touchpad_natural_scroll(bool natural);
bool input_is_natural_scroll(void);
int input_set_touchpad_tap_to_click(bool enabled);
bool input_is_tap_to_click(void);
int input_set_touchpad_speed(u32 speed);
int input_get_touch_position(s32 *x, s32 *y, u32 *finger_count);

/* Touchscreen functions */
int input_get_touchscreen_state(u32 *x, u32 *y, u32 *pressure, u32 *finger_count);
int input_set_touchscreen_calibration(s32 *matrix);
int input_get_touchscreen_size(u32 *width, u32 *height);

/* Joystick/Gamepad functions */
int input_get_joystick_axes(s32 *axes, u32 count);
int input_get_joystick_buttons(u32 *buttons);
int input_get_gamepad_state(s32 *axes, u32 *buttons);
int input_set_rumble(u32 device_id, u16 weak, u16 strong);
int input_get_joystick_count(void);

/* Tablet functions */
int input_get_tablet_position(s32 *x, s32 *y, s32 *pressure, s32 *tilt_x, s32 *tilt_y);
int input_get_tablet_buttons(u32 *buttons);

/* Utility functions */
const char *input_get_type_name(u32 type);
const char *input_get_bus_type_name(u32 bus);
const char *input_get_event_type_name(u32 type);
const char *input_get_event_code_name(u32 type, u32 code);

/* Global instance */
input_driver_t *input_get_driver(void);

#endif /* _INPUT_H */
