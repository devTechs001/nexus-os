/*
 * NEXUS OS - PS/2 Driver
 * drivers/input/keyboard/ps2.c
 *
 * PS/2 keyboard and mouse driver
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../input.h"
#include "../../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         PS/2 CONFIGURATION                                */
/*===========================================================================*/

#define PS2_DATA_PORT           0x60
#define PS2_STATUS_PORT         0x64
#define PS2_COMMAND_PORT        0x64

/* PS/2 Status Register Bits */
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_SYSTEM_FLAG  0x04
#define PS2_STATUS_COMMAND_DATA 0x08
#define PS2_STATUS_TIMEOUT      0x40
#define PS2_STATUS_PARITY       0x80

/* PS/2 Commands */
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT    0xAD
#define PS2_CMD_ENABLE_PORT     0xAE
#define PS2_CMD_TEST_PORT       0xAC
#define PS2_CMD_SEND_TO_MOUSE   0xD4
#define PS2_CMD_RESEND          0xFE
#define PS2_CMD_RESET           0xFF

/* PS/2 Responses */
#define PS2_RESP_ACK            0xFA
#define PS2_RESP_RESEND         0xFE
#define PS2_RESP_ERROR          0xFC

/* Mouse Commands */
#define PS2_MOUSE_SET_SCALE     0xE6
#define PS2_MOUSE_SET_RES       0xE8
#define PS2_MOUSE_GET_INFO      0xE9
#define PS2_MOUSE_SET_STREAM    0xEA
#define PS2_MOUSE_SET_SAMPLE    0xF3
#define PS2_MOUSE_ENABLE        0xF4
#define PS2_MOUSE_DISABLE       0xF5
#define PS2_MOUSE_RESET         0xFF

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    bool is_keyboard;
    bool is_mouse;
    bool is_enabled;
    u8 config;
    u8 last_command;
    u32 packets_received;
    u32 errors;
} ps2_controller_t;

typedef struct {
    u32 device_id;
    char name[64];
    bool is_keyboard;
    bool is_enabled;
    u8 leds;
    u32 scan_code_set;
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
    u8 last_scan_code;
    u64 last_key_time;
} ps2_keyboard_t;

typedef struct {
    u32 device_id;
    char name[64];
    bool is_mouse;
    bool is_enabled;
    s32 x;
    s32 y;
    s32 z;
    u8 buttons;
    u8 resolution;
    u8 sample_rate;
    u32 packet_count;
    u32 errors;
} ps2_mouse_t;

typedef struct {
    bool initialized;
    ps2_controller_t controller;
    ps2_keyboard_t keyboard;
    ps2_mouse_t mouse;
    spinlock_t lock;
} ps2_driver_t;

static ps2_driver_t g_ps2;

/*===========================================================================*/
/*                         LOW-LEVEL PS/2 OPERATIONS                         */
/*===========================================================================*/

static inline u8 ps2_read_status(void)
{
    return vga_inb(PS2_STATUS_PORT);
}

static inline u8 ps2_read_data(void)
{
    return vga_inb(PS2_DATA_PORT);
}

static inline void ps2_write_data(u8 data)
{
    vga_outb(PS2_DATA_PORT, data);
}

static void ps2_write_command(u8 cmd)
{
    /* Wait for input buffer to be empty */
    while (ps2_read_status() & PS2_STATUS_INPUT_FULL);
    vga_outb(PS2_COMMAND_PORT, cmd);
}

static int ps2_wait_read(void)
{
    u32 timeout = 100000;
    while (timeout-- > 0) {
        if (ps2_read_status() & PS2_STATUS_OUTPUT_FULL) {
            return 0;
        }
        cpu_relax();
    }
    return -ETIMEDOUT;
}

static int ps2_wait_write(void)
{
    u32 timeout = 100000;
    while (timeout-- > 0) {
        if (!(ps2_read_status() & PS2_STATUS_INPUT_FULL)) {
            return 0;
        }
        cpu_relax();
    }
    return -ETIMEDOUT;
}

static u8 ps2_read_byte(void)
{
    if (ps2_wait_read() < 0) {
        return 0;
    }
    return ps2_read_data();
}

static int ps2_write_byte(u8 byte)
{
    if (ps2_wait_write() < 0) {
        return -ETIMEDOUT;
    }
    ps2_write_data(byte);
    return 0;
}

static int ps2_send_command(u8 cmd, u8 response_expected)
{
    spinlock_lock(&g_ps2.lock);
    
    ps2_write_command(cmd);
    
    u8 response = ps2_read_byte();
    
    spinlock_unlock(&g_ps2.lock);
    
    if (response == response_expected) {
        return 0;
    }
    
    g_ps2.controller.errors++;
    return -EIO;
}

/*===========================================================================*/
/*                         KEYBOARD OPERATIONS                               */
/*===========================================================================*/

/* US QWERTY Scan Code Set 2 */
static const char ps2_keymap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void ps2_keyboard_handler(u8 scan_code)
{
    ps2_keyboard_t *kbd = &g_ps2.keyboard;
    
    if (!kbd->is_enabled) return;
    
    /* Check for key release (bit 7 set) */
    bool is_release = (scan_code & 0x80) != 0;
    u8 key_code = scan_code & 0x7F;
    
    kbd->last_scan_code = scan_code;
    kbd->last_key_time = ktime_get_ms();
    
    /* Handle modifier keys */
    if (key_code == 0x12 || key_code == 0x59) {  /* Left/Right Shift */
        kbd->shift_pressed = !is_release;
        return;
    }
    if (key_code == 0x14) {  /* Left Ctrl */
        kbd->ctrl_pressed = !is_release;
        return;
    }
    if (key_code == 0x11) {  /* Left Alt */
        kbd->alt_pressed = !is_release;
        return;
    }
    
    /* Get character from keymap */
    char c = ps2_keymap[key_code];
    if (c == 0) return;
    
    /* Apply modifiers */
    if (kbd->shift_pressed) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c == '1') c = '!';
        else if (c == '2') c = '@';
        else if (c == '3') c = '#';
        /* ... more shift mappings */
    }
    
    /* Create input event */
    input_event_t event;
    memset(&event, 0, sizeof(event));
    event.timestamp = ktime_get_us();
    
    if (!is_release) {
        event.type = EV_KEY;
        event.code = key_code;
        event.value = 1;
        input_queue_event(&event);
    }
}

static int ps2_keyboard_init(void)
{
    ps2_keyboard_t *kbd = &g_ps2.keyboard;
    
    strcpy(kbd->name, "PS/2 Keyboard");
    kbd->is_keyboard = true;
    kbd->is_enabled = true;
    kbd->scan_code_set = 2;
    kbd->leds = 0;
    
    /* Enable keyboard port */
    ps2_send_command(PS2_CMD_ENABLE_PORT, 0);
    
    printk("[PS2] Keyboard initialized\n");
    return 0;
}

int ps2_keyboard_set_leds(u8 leds)
{
    ps2_keyboard_t *kbd = &g_ps2.keyboard;
    
    /* Send LED command: 0xED, then LED bitmask */
    ps2_write_byte(0xED);
    if (ps2_read_byte() != PS2_RESP_ACK) {
        return -EIO;
    }
    
    ps2_write_byte(leds & 0x07);
    if (ps2_read_byte() != PS2_RESP_ACK) {
        return -EIO;
    }
    
    kbd->leds = leds & 0x07;
    return 0;
}

/*===========================================================================*/
/*                         MOUSE OPERATIONS                                  */
/*===========================================================================*/

static void ps2_mouse_handler(u8 byte1, u8 byte2, u8 byte3)
{
    ps2_mouse_t *mouse = &g_ps2.mouse;
    
    if (!mouse->is_enabled) return;
    
    mouse->packet_count++;
    
    /* Parse mouse packet */
    bool left = (byte1 & 0x01) != 0;
    bool right = (byte1 & 0x02) != 0;
    bool middle = (byte1 & 0x04) != 0;
    bool x_sign = (byte1 & 0x10) != 0;
    bool y_sign = (byte1 & 0x20) != 0;
    bool x_overflow = (byte1 & 0x40) != 0;
    bool y_overflow = (byte1 & 0x80) != 0;
    
    if (x_overflow || y_overflow) {
        mouse->errors++;
        return;
    }
    
    s32 dx = (s8)byte2;
    s32 dy = (s8)byte3;
    
    /* Apply sign extension for negative values */
    if (x_sign) dx |= 0xFFFFFF00;
    if (y_sign) dy |= 0xFFFFFF00;
    
    /* Invert Y for screen coordinates */
    dy = -dy;
    
    mouse->x += dx;
    mouse->y += dy;
    mouse->buttons = (left ? 1 : 0) | (right ? 2 : 0) | (middle ? 4 : 0);
    
    /* Create input events */
    input_event_t event;
    memset(&event, 0, sizeof(event));
    
    /* Relative motion */
    event.type = EV_REL;
    event.code = ABS_X;
    event.value = dx;
    input_queue_event(&event);
    
    event.code = ABS_Y;
    event.value = dy;
    input_queue_event(&event);
    
    /* Button states */
    if (left) {
        event.type = EV_KEY;
        event.code = BTN_LEFT;
        event.value = 1;
        input_queue_event(&event);
    }
}

static int ps2_mouse_init(void)
{
    ps2_mouse_t *mouse = &g_ps2.mouse;
    
    strcpy(mouse->name, "PS/2 Mouse");
    mouse->is_mouse = true;
    mouse->is_enabled = true;
    mouse->resolution = 4;  /* 8 counts/mm */
    mouse->sample_rate = 100;
    
    /* Enable mouse */
    ps2_send_command(PS2_CMD_ENABLE_PORT, 0);
    
    /* Send mouse commands */
    ps2_write_command(PS2_CMD_SEND_TO_MOUSE);
    ps2_write_byte(PS2_MOUSE_ENABLE);
    
    printk("[PS2] Mouse initialized\n");
    return 0;
}

/*===========================================================================*/
/*                         CONTROLLER INITIALIZATION                         */
/*===========================================================================*/

int ps2_controller_init(void)
{
    printk("[PS2] ========================================\n");
    printk("[PS2] NEXUS OS PS/2 Driver\n");
    printk("[PS2] ========================================\n");
    
    memset(&g_ps2, 0, sizeof(ps2_driver_t));
    spinlock_init(&g_ps2.lock);
    
    /* Self-test controller */
    ps2_send_command(0xAA, 0x55);  /* Controller self-test */
    
    /* Get current configuration */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    g_ps2.controller.config = ps2_read_byte();
    
    /* Disable devices during initialization */
    ps2_send_command(PS2_CMD_DISABLE_PORT, 0);
    
    /* Test ports */
    ps2_send_command(PS2_CMD_TEST_PORT, 0);
    
    /* Enable devices */
    ps2_send_command(PS2_CMD_ENABLE_PORT, 0);
    
    /* Initialize keyboard and mouse */
    ps2_keyboard_init();
    ps2_mouse_init();
    
    g_ps2.controller.is_enabled = true;
    
    printk("[PS2] PS/2 controller initialized\n");
    return 0;
}

/*===========================================================================*/
/*                         INTERRUPT HANDLER                                 */
/*===========================================================================*/

void ps2_interrupt_handler(void)
{
    u8 status = ps2_read_status();
    
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
        return;
    }
    
    u8 data = ps2_read_data();
    
    /* Route to appropriate device */
    if (g_ps2.controller.last_command == PS2_CMD_SEND_TO_MOUSE) {
        /* Mouse data */
        static u8 mouse_packet[3];
        static u8 mouse_index = 0;
        
        mouse_packet[mouse_index++] = data;
        
        if (mouse_index >= 3) {
            ps2_mouse_handler(mouse_packet[0], mouse_packet[1], mouse_packet[2]);
            mouse_index = 0;
        }
    } else {
        /* Keyboard data */
        ps2_keyboard_handler(data);
    }
    
    g_ps2.controller.packets_received++;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

ps2_driver_t *ps2_get_driver(void)
{
    return &g_ps2;
}

int ps2_keyboard_get_leds(void)
{
    return g_ps2.keyboard.leds;
}

int ps2_mouse_get_position(s32 *x, s32 *y)
{
    if (!x || !y) return -EINVAL;
    *x = g_ps2.mouse.x;
    *y = g_ps2.mouse.y;
    return 0;
}
