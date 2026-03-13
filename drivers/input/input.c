/*
 * NEXUS OS - Input Driver Implementation
 * drivers/input/input.c
 *
 * Comprehensive input driver implementation
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "input.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL INPUT DRIVER STATE                         */
/*===========================================================================*/

static input_driver_t g_input_driver;

/*===========================================================================*/
/*                         DRIVER INITIALIZATION                             */
/*===========================================================================*/

/**
 * input_init - Initialize input driver
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_init(void)
{
    printk("[INPUT] ========================================\n");
    printk("[INPUT] NEXUS OS Input Driver\n");
    printk("[INPUT] ========================================\n");
    printk("[INPUT] Initializing input driver...\n");

    /* Clear driver state */
    memset(&g_input_driver, 0, sizeof(input_driver_t));
    g_input_driver.initialized = true;

    /* Default settings */
    g_input_driver.key_repeat_delay = 500;  /* ms */
    g_input_driver.key_repeat_rate = 50;    /* ms */
    g_input_driver.mouse_speed = 100;
    g_input_driver.natural_scroll = false;
    g_input_driver.tap_to_click = true;
    g_input_driver.touchpad_enabled = true;

    /* Enumerate devices */
    input_enumerate_devices();

    printk("[INPUT] Found %d input device(s)\n", g_input_driver.device_count);

    /* Print device info */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        printk("[INPUT]   [%d] %s (%s)\n", 
               i, dev->info.name, input_get_type_name(dev->type));
    }

    printk("[INPUT] Input driver initialized\n");
    printk("[INPUT] ========================================\n");

    return 0;
}

/**
 * input_shutdown - Shutdown input driver
 *
 * Returns: 0 on success
 */
int input_shutdown(void)
{
    printk("[INPUT] Shutting down input driver...\n");

    /* Close all devices */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_close_device(i + 1);
    }

    g_input_driver.initialized = false;

    printk("[INPUT] Input driver shutdown complete\n");

    return 0;
}

/**
 * input_is_initialized - Check if input driver is initialized
 *
 * Returns: true if initialized, false otherwise
 */
bool input_is_initialized(void)
{
    return g_input_driver.initialized;
}

/*===========================================================================*/
/*                         DEVICE ENUMERATION                                */
/*===========================================================================*/

/**
 * input_enumerate_devices - Enumerate input devices
 *
 * Returns: Number of devices found
 */
int input_enumerate_devices(void)
{
    printk("[INPUT] Enumerating input devices...\n");

    g_input_driver.device_count = 0;

    /* Mock PS/2 Keyboard */
    input_device_t *kbd = &g_input_driver.devices[g_input_driver.device_count++];
    kbd->device_id = 1;
    kbd->type = INPUT_TYPE_KEYBOARD;
    strcpy(kbd->info.name, "AT Translated Set 2 keyboard");
    strcpy(kbd->info.phys, "isa0060/serio0/input0");
    kbd->info.bustype = INPUT_BUS_PS2;
    kbd->info.vendor = 0x0001;
    kbd->info.product = 0x0001;
    kbd->info.version = 0x0100;
    kbd->is_connected = true;
    kbd->is_enabled = true;

    /* Mock PS/2 Mouse */
    input_device_t *mouse = &g_input_driver.devices[g_input_driver.device_count++];
    mouse->device_id = 2;
    mouse->type = INPUT_TYPE_MOUSE;
    strcpy(mouse->info.name, "PS/2 Generic Mouse");
    strcpy(mouse->info.phys, "isa0060/serio1/input0");
    mouse->info.bustype = INPUT_BUS_PS2;
    mouse->info.vendor = 0x0002;
    mouse->info.product = 0x0001;
    mouse->info.version = 0x0100;
    mouse->is_connected = true;
    mouse->is_enabled = true;

    /* Mock USB Keyboard */
    input_device_t *usb_kbd = &g_input_driver.devices[g_input_driver.device_count++];
    usb_kbd->device_id = 3;
    usb_kbd->type = INPUT_TYPE_KEYBOARD;
    strcpy(usb_kbd->info.name, "USB Keyboard");
    strcpy(usb_kbd->info.phys, "usb-0000:00:14.0-1/input0");
    usb_kbd->info.bustype = INPUT_BUS_USB;
    usb_kbd->info.vendor = 0x046D;  /* Logitech */
    usb_kbd->info.product = 0xC31C;
    usb_kbd->info.version = 0x0110;
    usb_kbd->is_connected = false;  /* Not connected by default */
    usb_kbd->is_enabled = false;

    /* Mock USB Mouse */
    input_device_t *usb_mouse = &g_input_driver.devices[g_input_driver.device_count++];
    usb_mouse->device_id = 4;
    usb_mouse->type = INPUT_TYPE_MOUSE;
    strcpy(usb_mouse->info.name, "USB Optical Mouse");
    strcpy(usb_mouse->info.phys, "usb-0000:00:14.0-2/input0");
    usb_mouse->info.bustype = INPUT_BUS_USB;
    usb_mouse->info.vendor = 0x046D;  /* Logitech */
    usb_mouse->info.product = 0xC077;
    usb_mouse->info.version = 0x0110;
    usb_mouse->is_connected = false;
    usb_mouse->is_enabled = false;

    /* Mock Touchpad */
    input_device_t *touchpad = &g_input_driver.devices[g_input_driver.device_count++];
    touchpad->device_id = 5;
    touchpad->type = INPUT_TYPE_TOUCHPAD;
    strcpy(touchpad->info.name, "Synaptics Touchpad");
    strcpy(touchpad->info.phys, "i2c-SYN1234:00/input0");
    touchpad->info.bustype = INPUT_BUS_I2C;
    touchpad->info.vendor = 0x06CB;  /* Synaptics */
    touchpad->info.product = 0x7A13;
    touchpad->info.version = 0x0100;
    touchpad->is_connected = true;
    touchpad->is_enabled = true;

    /* Mock Gamepad */
    input_device_t *gamepad = &g_input_driver.devices[g_input_driver.device_count++];
    gamepad->device_id = 6;
    gamepad->type = INPUT_TYPE_GAMEPAD;
    strcpy(gamepad->info.name, "X-Box Compatible Gamepad");
    strcpy(gamepad->info.phys, "usb-0000:00:14.0-3/input0");
    gamepad->info.bustype = INPUT_BUS_USB;
    gamepad->info.vendor = 0x045E;  /* Microsoft */
    gamepad->info.product = 0x028E;
    gamepad->info.version = 0x0110;
    gamepad->is_connected = false;
    gamepad->is_enabled = false;

    return g_input_driver.device_count;
}

/**
 * input_get_device - Get input device
 * @device_id: Device ID
 *
 * Returns: Device pointer, or NULL if not found
 */
input_device_t *input_get_device(u32 device_id)
{
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        if (g_input_driver.devices[i].device_id == device_id) {
            return &g_input_driver.devices[i];
        }
    }

    return NULL;
}

/**
 * input_list_devices - List all input devices
 * @devices: Devices array
 * @count: Count pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_list_devices(input_device_t *devices, u32 *count)
{
    if (!devices || !count || *count == 0) {
        return -EINVAL;
    }

    u32 copy_count = (*count < g_input_driver.device_count) ? 
                     *count : g_input_driver.device_count;
    
    memcpy(devices, g_input_driver.devices, sizeof(input_device_t) * copy_count);
    *count = copy_count;

    return 0;
}

/*===========================================================================*/
/*                         DEVICE CONTROL                                    */
/*===========================================================================*/

/**
 * input_open_device - Open input device
 * @device_id: Device ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_open_device(u32 device_id)
{
    input_device_t *dev = input_get_device(device_id);
    if (!dev) {
        return -ENOENT;
    }

    if (dev->is_open) {
        return -EBUSY;
    }

    if (dev->open) {
        int ret = dev->open(dev);
        if (ret < 0) {
            return ret;
        }
    }

    dev->is_open = true;
    printk("[INPUT] Opened device: %s\n", dev->info.name);

    return 0;
}

/**
 * input_close_device - Close input device
 * @device_id: Device ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_close_device(u32 device_id)
{
    input_device_t *dev = input_get_device(device_id);
    if (!dev) {
        return -ENOENT;
    }

    if (!dev->is_open) {
        return 0;
    }

    if (dev->close) {
        dev->close(dev);
    }

    dev->is_open = false;
    printk("[INPUT] Closed device: %s\n", dev->info.name);

    return 0;
}

/**
 * input_set_led - Set LED on input device
 * @device_id: Device ID
 * @led: LED code
 * @state: LED state
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_set_led(u32 device_id, u32 led, bool state)
{
    input_device_t *dev = input_get_device(device_id);
    if (!dev) {
        return -ENOENT;
    }

    if (state) {
        dev->led_state |= (1 << led);
    } else {
        dev->led_state &= ~(1 << led);
    }

    /* Update keyboard LEDs */
    if (dev->type == INPUT_TYPE_KEYBOARD) {
        /* In real implementation, would send command to keyboard */
        printk("[INPUT] Keyboard LED %d: %s\n", led, state ? "ON" : "OFF");
    }

    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLING                                    */
/*===========================================================================*/

/**
 * input_queue_event - Queue input event
 * @event: Event to queue
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_queue_event(input_event_t *event)
{
    if (!g_input_driver.initialized || !event) {
        return -EINVAL;
    }

    u32 next_head = (g_input_driver.event_head + 1) % INPUT_EVENT_QUEUE_SIZE;
    
    if (next_head == g_input_driver.event_tail) {
        /* Queue full, drop oldest event */
        g_input_driver.event_tail = (g_input_driver.event_tail + 1) % INPUT_EVENT_QUEUE_SIZE;
    }

    g_input_driver.event_queue[g_input_driver.event_head] = *event;
    g_input_driver.event_head = next_head;

    /* Call callback if set */
    if (g_input_driver.on_event) {
        g_input_driver.on_event(event, g_input_driver.event_user_data);
    }

    return 0;
}

/**
 * input_read_event - Read input event
 * @event: Event pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_read_event(input_event_t *event)
{
    if (!g_input_driver.initialized || !event) {
        return -EINVAL;
    }

    if (g_input_driver.event_head == g_input_driver.event_tail) {
        return -EAGAIN;  /* No events */
    }

    *event = g_input_driver.event_queue[g_input_driver.event_tail];
    g_input_driver.event_tail = (g_input_driver.event_tail + 1) % INPUT_EVENT_QUEUE_SIZE;

    return 0;
}

/**
 * input_flush_events - Flush all pending events
 *
 * Returns: 0 on success
 */
int input_flush_events(void)
{
    g_input_driver.event_head = 0;
    g_input_driver.event_tail = 0;
    return 0;
}

/*===========================================================================*/
/*                         KEYBOARD FUNCTIONS                                */
/*===========================================================================*/

/**
 * input_is_key_pressed - Check if key is pressed
 * @key: Key code
 *
 * Returns: true if pressed, false otherwise
 */
bool input_is_key_pressed(u32 key)
{
    if (key >= INPUT_MAX_KEYS) {
        return false;
    }

    /* Check all keyboard devices */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        if (dev->type == INPUT_TYPE_KEYBOARD && dev->is_enabled) {
            if (dev->key_state[key / 8] & (1 << (key % 8))) {
                return true;
            }
        }
    }

    return false;
}

/**
 * input_set_key_repeat - Set key repeat rate
 * @delay: Repeat delay (ms)
 * @rate: Repeat rate (ms)
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_set_key_repeat(u32 delay, u32 rate)
{
    if (delay > 1000 || rate > 500) {
        return -EINVAL;
    }

    g_input_driver.key_repeat_delay = delay;
    g_input_driver.key_repeat_rate = rate;

    printk("[INPUT] Key repeat: delay=%dms, rate=%dms\n", delay, rate);

    return 0;
}

/**
 * input_get_key_name - Get key name
 * @key: Key code
 *
 * Returns: Key name string
 */
const char *input_get_key_name(u32 key)
{
    switch (key) {
        case KEY_ESC:         return "Escape";
        case KEY_1:           return "1";
        case KEY_9:           return "9";
        case KEY_0:           return "0";
        case KEY_MINUS:       return "Minus";
        case KEY_EQUAL:       return "Equal";
        case KEY_BACKSPACE:   return "Backspace";
        case KEY_TAB:         return "Tab";
        case KEY_Q:           return "Q";
        case KEY_P:           return "P";
        case KEY_ENTER:       return "Enter";
        case KEY_LEFTCTRL:    return "Left Ctrl";
        case KEY_A:           return "A";
        case KEY_L:           return "L";
        case KEY_LEFTSHIFT:   return "Left Shift";
        case KEY_Z:           return "Z";
        case KEY_M:           return "M";
        case KEY_RIGHTSHIFT:  return "Right Shift";
        case KEY_SPACE:       return "Space";
        case KEY_F1:          return "F1";
        case KEY_F12:         return "F12";
        case KEY_LEFTALT:     return "Left Alt";
        case KEY_RIGHTALT:    return "Right Alt";
        case KEY_LEFTMETA:    return "Left Super";
        case KEY_RIGHTMETA:   return "Right Super";
        case KEY_MENU:        return "Menu";
        default:              return "Unknown";
    }
}

/*===========================================================================*/
/*                         MOUSE FUNCTIONS                                   */
/*===========================================================================*/

/**
 * input_get_mouse_position - Get mouse position
 * @x: X coordinate pointer
 * @y: Y coordinate pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_get_mouse_position(s32 *x, s32 *y)
{
    if (!x || !y) {
        return -EINVAL;
    }

    /* Get position from mouse devices */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        if (dev->type == INPUT_TYPE_MOUSE && dev->is_enabled) {
            *x = dev->rel_state[ABS_X];
            *y = dev->rel_state[ABS_Y];
            return 0;
        }
    }

    return -ENOENT;
}

/**
 * input_set_mouse_speed - Set mouse speed
 * @speed: Speed (0-200)
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_set_mouse_speed(u32 speed)
{
    if (speed > 200) {
        return -EINVAL;
    }

    g_input_driver.mouse_speed = speed;
    printk("[INPUT] Mouse speed: %d%%\n", speed);

    return 0;
}

/**
 * input_mouse_button_press - Simulate mouse button press
 * @button: Button code
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_mouse_button_press(u32 button)
{
    input_event_t event;
    memset(&event, 0, sizeof(event));
    
    event.type = EV_KEY;
    event.code = button;
    event.value = 1;
    
    return input_queue_event(&event);
}

/**
 * input_mouse_button_release - Simulate mouse button release
 * @button: Button code
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_mouse_button_release(u32 button)
{
    input_event_t event;
    memset(&event, 0, sizeof(event));
    
    event.type = EV_KEY;
    event.code = button;
    event.value = 0;
    
    return input_queue_event(&event);
}

/*===========================================================================*/
/*                         TOUCHPAD FUNCTIONS                                */
/*===========================================================================*/

/**
 * input_set_touchpad_enabled - Enable/disable touchpad
 * @enabled: Enable state
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_set_touchpad_enabled(bool enabled)
{
    g_input_driver.touchpad_enabled = enabled;

    /* Update touchpad devices */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        if (dev->type == INPUT_TYPE_TOUCHPAD) {
            dev->is_enabled = enabled;
        }
    }

    printk("[INPUT] Touchpad %s\n", enabled ? "enabled" : "disabled");

    return 0;
}

/**
 * input_set_touchpad_natural_scroll - Enable/disable natural scroll
 * @natural: Natural scroll state
 *
 * Returns: 0 on success
 */
int input_set_touchpad_natural_scroll(bool natural)
{
    g_input_driver.natural_scroll = natural;
    printk("[INPUT] Natural scroll %s\n", natural ? "enabled" : "disabled");
    return 0;
}

/**
 * input_set_touchpad_tap_to_click - Enable/disable tap-to-click
 * @enabled: Tap-to-click state
 *
 * Returns: 0 on success
 */
int input_set_touchpad_tap_to_click(bool enabled)
{
    g_input_driver.tap_to_click = enabled;
    printk("[INPUT] Tap-to-click %s\n", enabled ? "enabled" : "disabled");
    return 0;
}

/**
 * input_get_touch_position - Get touch position
 * @x: X coordinate pointer
 * @y: Y coordinate pointer
 * @finger_count: Finger count pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_get_touch_position(s32 *x, s32 *y, u32 *finger_count)
{
    if (!x || !y || !finger_count) {
        return -EINVAL;
    }

    /* Get position from touchpad/touchscreen devices */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        if ((dev->type == INPUT_TYPE_TOUCHPAD || dev->type == INPUT_TYPE_TOUCHSCREEN) 
            && dev->is_enabled) {
            *x = dev->abs_state[ABS_X];
            *y = dev->abs_state[ABS_Y];
            *finger_count = (dev->abs_state[ABS_PRESSURE] > 0) ? 1 : 0;
            return 0;
        }
    }

    return -ENOENT;
}

/*===========================================================================*/
/*                         GAMEPAD FUNCTIONS                                 */
/*===========================================================================*/

/**
 * input_get_gamepad_state - Get gamepad state
 * @axes: Axes array
 * @buttons: Buttons bitfield
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_get_gamepad_state(s32 *axes, u32 *buttons)
{
    if (!axes || !buttons) {
        return -EINVAL;
    }

    /* Find gamepad device */
    for (u32 i = 0; i < g_input_driver.device_count; i++) {
        input_device_t *dev = &g_input_driver.devices[i];
        if (dev->type == INPUT_TYPE_GAMEPAD && dev->is_enabled) {
            /* Copy axes */
            for (u32 j = 0; j < 6; j++) {
                axes[j] = dev->abs_state[ABS_X + j];
            }
            *buttons = dev->led_state;  /* Using led_state for buttons */
            return 0;
        }
    }

    return -ENOENT;
}

/**
 * input_set_rumble - Set gamepad rumble
 * @device_id: Device ID
 * @weak: Weak motor strength (0-65535)
 * @strong: Strong motor strength (0-65535)
 *
 * Returns: 0 on success, negative error code on failure
 */
int input_set_rumble(u32 device_id, u16 weak, u16 strong)
{
    input_device_t *dev = input_get_device(device_id);
    if (!dev) {
        return -ENOENT;
    }

    if (dev->type != INPUT_TYPE_GAMEPAD) {
        return -EINVAL;
    }

    /* In real implementation, would send rumble command */
    printk("[INPUT] Rumble: weak=%d, strong=%d\n", weak, strong);

    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * input_get_type_name - Get device type name
 * @type: Device type
 *
 * Returns: Type name string
 */
const char *input_get_type_name(u32 type)
{
    switch (type) {
        case INPUT_TYPE_KEYBOARD:    return "Keyboard";
        case INPUT_TYPE_MOUSE:       return "Mouse";
        case INPUT_TYPE_TOUCHPAD:    return "Touchpad";
        case INPUT_TYPE_TOUCHSCREEN: return "Touchscreen";
        case INPUT_TYPE_TABLET:      return "Tablet";
        case INPUT_TYPE_JOYSTICK:    return "Joystick";
        case INPUT_TYPE_GAMEPAD:     return "Gamepad";
        case INPUT_TYPE_TRACKBALL:   return "Trackball";
        case INPUT_TYPE_TRACKPOINT:  return "Trackpoint";
        default:                     return "Unknown";
    }
}

/**
 * input_get_bus_type_name - Get bus type name
 * @bus: Bus type
 *
 * Returns: Bus type name string
 */
const char *input_get_bus_type_name(u32 bus)
{
    switch (bus) {
        case INPUT_BUS_PS2:       return "PS/2";
        case INPUT_BUS_USB:       return "USB";
        case INPUT_BUS_BLUETOOTH: return "Bluetooth";
        case INPUT_BUS_I2C:       return "I2C";
        case INPUT_BUS_SPI:       return "SPI";
        case INPUT_BUS_SERIAL:    return "Serial";
        case INPUT_BUS_VIRTUAL:   return "Virtual";
        default:                  return "Unknown";
    }
}

/**
 * input_get_event_type_name - Get event type name
 * @type: Event type
 *
 * Returns: Event type name string
 */
const char *input_get_event_type_name(u32 type)
{
    switch (type) {
        case EV_SYN:  return "Sync";
        case EV_KEY:  return "Key";
        case EV_REL:  return "Relative";
        case EV_ABS:  return "Absolute";
        case EV_LED:  return "LED";
        default:      return "Unknown";
    }
}

/**
 * input_get_driver - Get input driver instance
 *
 * Returns: Pointer to driver instance
 */
input_driver_t *input_get_driver(void)
{
    return &g_input_driver;
}
