# NEXUS OS - Complete Driver Documentation

## Copyright (c) 2026 NEXUS Development Team

**Version:** 1.0.0
**Date:** March 2026
**Status:** Implementation Complete

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Driver Architecture](#driver-architecture)
3. [Display/Graphics Drivers](#displaygraphics-drivers)
4. [Input Drivers](#input-drivers)
5. [Audio Drivers](#audio-drivers)
6. [Acceleration Drivers](#acceleration-drivers)
7. [Window System Drivers](#window-system-drivers)
8. [Interface Drivers](#interface-drivers)
9. [Qt Platform Plugins](#qt-platform-plugins)
10. [Additional GUI Support](#additional-gui-support)
11. [API Reference](#api-reference)
12. [Integration Guide](#integration-guide)

---

## Overview

NEXUS OS provides comprehensive driver support for all major hardware components required for a modern graphical operating system. This document describes the complete driver stack implemented for GUI support.

### Supported Driver Categories

| Category | Drivers | Status |
|----------|---------|--------|
| Display/Graphics | VGA, SVGA, VESA, Framebuffer, DisplayPort, HDMI, Touchscreen | ✅ Complete |
| Input | PS/2, USB Keyboard/Mouse, Touchpad, Joystick/Gamepad | ✅ Complete |
| Audio | AC'97, HDA, Intel HD Audio, USB Audio, SoundBlaster | ✅ Complete |
| Acceleration | OpenGL, Vulkan, DRM, KMS, 2D/3D Acceleration | ✅ Complete |
| Window System | Wayland, X11 Bridge, Compositor | ✅ Complete |
| Interface | DVI, I2C, SPI Display | ✅ Complete |
| Qt Platform | LinuxFB, EGLFS, Native NEXUS | ✅ Complete |
| GUI Support | Multi-monitor, HiDPI, Color Management, VSync | ✅ Complete |

---

## Driver Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        NEXUS OS Driver Stack                             │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Application Layer                              │   │
│  │     Qt/PySide6 Apps  │  Native Apps  │  X11 Apps  │  Wayland    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Qt Platform Plugins                            │   │
│  │     LinuxFB  │  EGLFS  │  Wayland  │  Native NEXUS               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Window System Layer                            │   │
│  │     Wayland Compositor  │  X11 Server  │  XWayland Bridge        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Graphics Acceleration                          │   │
│  │     OpenGL  │  Vulkan  │  DRM/KMS  │  2D/3D Engine               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Device Drivers                                 │   │
│  │  Display │ Input │ Audio │ Interface │ Sensors │ Network         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Kernel Core                                    │   │
│  │     Scheduler │ Memory │ IPC │ Drivers │ HAL                      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Hardware Layer                                 │   │
│  │     GPU │ Display │ Input │ Audio │ Network │ Storage             │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Display/Graphics Drivers

### Location
```
drivers/display/
├── display.h          # Main display driver header
├── display.c          # Display driver implementation
└── (subdrivers)
```

### Supported Display Types

| Type | Description | Header |
|------|-------------|--------|
| VGA | Video Graphics Array | `drivers/video/vesa.h` |
| SVGA | Super VGA | `drivers/video/vesa.h` |
| VESA | VESA BIOS Extensions | `drivers/video/vesa.h` |
| Framebuffer | Linear Framebuffer | `drivers/display/display.h` |
| DisplayPort | DisplayPort Interface | `drivers/display/display.h` |
| HDMI | High-Definition Multimedia Interface | `drivers/display/display.h` |
| DVI | Digital Visual Interface | `drivers/display/display.h` |
| Touchscreen | Touch Input Display | `drivers/input/touchscreen/` |

### Key Structures

```c
// Display Adapter
typedef struct {
    u32 adapter_id;
    u32 type;                       // DISPLAY_TYPE_*
    char name[128];
    u32 vendor_id;
    u32 device_id;
    u64 vram_size;
    u32 capabilities;               // DISPLAY_CAP_*
    display_connector_t *connectors;
    display_crtc_t *crtcs;
    display_plane_t *planes;
} display_adapter_t;

// Display Mode
typedef struct {
    u32 clock;                      // Pixel Clock (kHz)
    u16 hdisplay, hsync_start, hsync_end, htotal;
    u16 vdisplay, vsync_start, vsync_end, vtotal;
    u32 vrefresh;                   // Refresh Rate (Hz)
    u32 flags;
    char name[64];
    bool is_preferred;
} display_mode_t;

// Framebuffer
typedef struct {
    u32 fb_id;
    void *base;                     // Virtual Address
    phys_addr_t phys_base;          // Physical Address
    u32 width, height, stride;
    u32 bpp;                        // Bits Per Pixel
    u32 format;                     // PIXEL_FORMAT_*
} display_framebuffer_t;
```

### API Functions

```c
// Initialization
int display_init(void);
int display_shutdown(void);

// Adapter Management
int display_enumerate_adapters(void);
display_adapter_t *display_get_adapter(u32 adapter_id);
int display_set_active_adapter(u32 adapter_id);

// Mode Setting
int display_get_modes(u32 connector_id, display_mode_t *modes, u32 *count);
int display_set_mode(u32 crtc_id, u32 connector_id, display_mode_t *mode);
int display_get_preferred_mode(u32 connector_id, display_mode_t *mode);

// Framebuffer
int display_create_framebuffer(u32 width, u32 height, u32 format, 
                               display_framebuffer_t *fb);
int display_destroy_framebuffer(u32 fb_id);
display_framebuffer_t *display_get_primary_framebuffer(void);

// Drawing Primitives
int display_set_pixel(u32 x, u32 y, u32 color);
int display_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
int display_draw_line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
int display_draw_circle(u32 cx, u32 cy, u32 r, u32 color);

// Multi-Monitor
int display_get_monitor_count(void);
int display_clone_monitor(u32 source, u32 target);
int display_extend_monitor(u32 source, u32 target, u32 position);

// HiDPI Support
int display_set_scale_factor(u32 adapter_id, u32 scale_x, u32 scale_y);
int display_get_dpi(u32 adapter_id, u32 *dpi_x, u32 *dpi_y);
```

### Pixel Formats

```c
#define PIXEL_FORMAT_RGB565         // 16-bit RGB
#define PIXEL_FORMAT_RGB888         // 24-bit RGB
#define PIXEL_FORMAT_BGR888         // 24-bit BGR
#define PIXEL_FORMAT_ARGB8888       // 32-bit ARGB
#define PIXEL_FORMAT_ABGR8888       // 32-bit ABGR
#define PIXEL_FORMAT_XRGB8888       // 32-bit XRGB
```

---

## Input Drivers

### Location
```
drivers/input/
├── input.h          # Main input driver header
├── input.c          # Input driver implementation
├── keyboard/        # Keyboard drivers
├── mouse/           # Mouse drivers
└── touchscreen/     # Touchscreen drivers
```

### Supported Input Types

| Type | Bus | Description |
|------|-----|-------------|
| Keyboard | PS/2, USB | AT, USB HID keyboards |
| Mouse | PS/2, USB | PS/2, USB HID mice |
| Touchpad | I2C, USB | Synaptics, ELAN touchpads |
| Touchscreen | I2C, USB | Multi-touch screens |
| Gamepad | USB, Bluetooth | X-Box, PlayStation controllers |
| Joystick | USB | USB joysticks |

### Key Structures

```c
// Input Device
typedef struct {
    u32 device_id;
    input_devinfo_t info;
    u32 type;                       // INPUT_TYPE_*
    u32 bustype;                    // INPUT_BUS_*
    bool is_connected;
    bool is_enabled;
    input_capabilities_t caps;
    u8 key_state[INPUT_MAX_KEYS / 8 + 1];
    s32 abs_state[ABS_MAX + 1];
} input_device_t;

// Input Event
typedef struct {
    u64 timestamp;
    u32 type;                       // EV_*
    u32 code;                       // Key/Button/Axes code
    s32 value;                      // Value
} input_event_t;
```

### API Functions

```c
// Initialization
int input_init(void);
int input_shutdown(void);

// Device Management
int input_enumerate_devices(void);
input_device_t *input_get_device(u32 device_id);
int input_list_devices(input_device_t *devices, u32 *count);

// Event Handling
int input_read_event(input_event_t *event);
int input_queue_event(input_event_t *event);
int input_set_event_callback(void (*callback)(input_event_t *, void *), 
                             void *user_data);

// Keyboard
bool input_is_key_pressed(u32 key);
int input_set_key_repeat(u32 delay, u32 rate);
const char *input_get_key_name(u32 key);

// Mouse
int input_get_mouse_position(s32 *x, s32 *y);
int input_set_mouse_position(s32 x, s32 y);
int input_set_mouse_speed(u32 speed);
int input_mouse_button_press(u32 button);
int input_mouse_button_release(u32 button);

// Touchpad
int input_set_touchpad_enabled(bool enabled);
int input_set_touchpad_natural_scroll(bool natural);
int input_set_touchpad_tap_to_click(bool enabled);
int input_get_touch_position(s32 *x, s32 *y, u32 *finger_count);

// Gamepad
int input_get_gamepad_state(s32 *axes, u32 *buttons);
int input_set_rumble(u32 device_id, u16 weak, u16 strong);
```

### Event Types

```c
#define EV_KEY      0x01    // Key/Button press/release
#define EV_REL      0x02    // Relative motion (mouse)
#define EV_ABS      0x03    // Absolute position (touch)
#define EV_SYN      0x00    // Synchronization event
#define EV_LED      0x11    // LED control
```

---

## Audio Drivers

### Location
```
drivers/audio/
├── audio.h          # Main audio driver header
└── audio.c          # Audio driver implementation
```

### Supported Audio Types

| Type | Description |
|------|-------------|
| AC'97 | AC'97 Audio Codec |
| HDA | Intel High Definition Audio |
| Intel HDA | Intel HD Audio Controller |
| USB Audio | USB Audio Class devices |
| SoundBlaster | Creative SoundBlaster compatible |

### Key Structures

```c
// Audio Device
typedef struct {
    u32 device_id;
    char name[128];
    u32 type;                       // AUDIO_TYPE_*
    u32 vendor_id;
    u32 device_id_pci;
    bool is_connected;
    bool is_default;
    audio_format_t format;
} audio_device_t;

// Audio Format
typedef struct {
    u32 format;                     // AUDIO_FORMAT_*
    u32 sample_rate;                // Hz (8000-192000)
    u32 channels;                   // 1-8
    u32 bits_per_sample;            // 8, 16, 24, 32
    u32 buffer_size;                // bytes
} audio_format_t;

// Audio Stream
typedef struct {
    u32 stream_id;
    u32 device_id;
    u32 type;                       // PLAYBACK/CAPTURE
    u32 state;                      // AUDIO_STATE_*
    audio_format_t format;
    void *buffer;
    u32 hw_ptr, app_ptr;
} audio_stream_t;
```

### API Functions

```c
// Initialization
int audio_init(void);
int audio_shutdown(void);

// Device Management
int audio_enumerate_devices(void);
audio_device_t *audio_get_device(u32 device_id);
audio_device_t *audio_get_default_output(void);
int audio_set_default_output(u32 device_id);

// Stream Management
audio_stream_t *audio_create_stream(u32 device_id, u32 type, 
                                    audio_format_t *format);
int audio_destroy_stream(u32 stream_id);
int audio_start_stream(u32 stream_id);
int audio_stop_stream(u32 stream_id);
int audio_pause_stream(u32 stream_id);
int audio_resume_stream(u32 stream_id);

// Stream I/O
int audio_write_stream(u32 stream_id, const void *data, u32 size);
int audio_read_stream(u32 stream_id, void *data, u32 size);

// Volume Control
int audio_set_master_volume(u32 volume);
int audio_get_master_volume(void);
int audio_set_master_mute(bool mute);
int audio_set_pcm_volume(u32 volume);
int audio_set_mic_volume(u32 volume);

// HDA Codec
int audio_hda_init_codec(u32 codec_id);
int audio_hda_set_pin_control(u32 widget_id, u32 control);
int audio_hda_set_amp_gain(u32 widget_id, u32 gain, bool mute);
```

### Audio Formats

```c
#define AUDIO_FORMAT_S8             // Signed 8-bit
#define AUDIO_FORMAT_U8             // Unsigned 8-bit
#define AUDIO_FORMAT_S16_LE         // Signed 16-bit Little Endian
#define AUDIO_FORMAT_S24_LE         // Signed 24-bit Little Endian
#define AUDIO_FORMAT_S32_LE         // Signed 32-bit Little Endian
#define AUDIO_FORMAT_FLOAT32        // 32-bit Float
```

---

## Acceleration Drivers

### Location
```
drivers/gpu/
├── gpu.h              # GPU driver header
├── gpu.c              # GPU driver implementation
├── acceleration.h     # Acceleration API header
└── acceleration.c     # Acceleration implementation
```

### Supported APIs

| API | Version | Status |
|-----|---------|--------|
| OpenGL | 4.6 | ✅ Complete |
| OpenGL ES | 3.2 | ✅ Complete |
| Vulkan | 1.3 | ✅ Complete |
| DRM/KMS | Latest | ✅ Complete |

### Key Structures

```c
// OpenGL Context
typedef struct {
    u32 context_id;
    u32 api_version;
    u32 profile;
    u32 framebuffer_width, framebuffer_height;
    bool is_current;
    bool is_valid;
} gl_context_t;

// OpenGL Buffer
typedef struct {
    u32 buffer_id;
    u32 target;
    u32 size;
    u32 usage;
    void *data;
    u64 gpu_address;
} gl_buffer_t;

// OpenGL Texture
typedef struct {
    u32 texture_id;
    u32 target;
    u32 width, height, depth;
    u32 mip_levels;
    u32 internal_format;
    u32 format, type;
} gl_texture_t;

// DRM Device
typedef struct {
    int fd;
    char path[64];
    char driver_name[64];
    u32 vendor_id, device_id;
    u32 total_vram;
    u32 capabilities;
} drm_device_t;
```

### API Functions

```c
// Initialization
int accel_init(void);
int accel_set_active_api(u32 api);

// OpenGL
gl_context_t *gl_create_context(u32 width, u32 height, 
                                u32 version, u32 flags);
int gl_make_current(u32 context_id);
int gl_swap_buffers(void);

u32 gl_create_buffer(u32 target, u32 size, const void *data, u32 usage);
u32 gl_create_texture(u32 target, u32 width, u32 height, u32 format);
u32 gl_create_shader(u32 type, const char *source);
u32 gl_create_program(void);
int gl_link_program(u32 program_id);
int gl_use_program(u32 program_id);

void gl_clear(u32 mask);
void gl_draw_arrays(u32 mode, s32 first, s32 count);
void gl_draw_elements(u32 mode, s32 count, u32 type, const void *indices);
void gl_viewport(s32 x, s32 y, s32 w, s32 h);

// Vulkan
vk_instance_t *vk_create_instance(const char *app_name, 
                                  const char *engine_name);
int vk_enumerate_physical_devices(vk_physical_device_t *devices, 
                                  u32 *count);

// DRM
int drm_open(const char *path);
int drm_get_device_info(drm_device_t *info);
int drm_wait_vblank(u32 crtc_id);
int drm_page_flip(u32 crtc_id, u32 fb_id);

// 2D Acceleration
int accel_2d_fill_rect(void *dst, u32 x, u32 y, u32 w, u32 h, u32 color);
int accel_2d_blit(void *dst, u32 dx, u32 dy, void *src, 
                  u32 sx, u32 sy, u32 w, u32 h);
int accel_2d_scale(void *dst, u32 dw, u32 dh, void *src, u32 sw, u32 sh);

// VSync
int accel_enable_vsync(bool enable);
int accel_wait_vsync(void);
```

---

## Window System Drivers

### Location
```
drivers/video/
└── window.h         # Window system header

gui/
├── compositor/      # Compositor implementation
└── window-manager/  # Window manager
```

### Supported Window Systems

| System | Description |
|--------|-------------|
| Wayland | Native Wayland compositor |
| X11 | X Window System |
| XWayland | X11 compatibility layer |

### Key Structures

```c
// Window
typedef struct {
    u32 window_id;
    u32 type;                       // WINDOW_TYPE_*
    u32 state;                      // WINDOW_STATE_*
    rect_t geometry;
    u32 z_order;
    char title[256];
    char class[256];
    void *buffer;
    bool is_mapped;
    bool is_focused;
} window_t;

// Compositor
typedef struct {
    u32 compositor_id;
    char name[64];
    u32 version;
    rect_t output_geometry;
    window_t *windows;
    u32 window_count;
    window_t *focused_window;
    u32 cursor_x, cursor_y;
    bool vsync_enabled;
    bool hw_accel;
} compositor_t;
```

### API Functions

```c
// Initialization
int window_init(void);
int window_set_active_system(u32 system);

// Wayland
wl_surface_t *wayland_create_surface(void);
wl_shell_surface_t *wayland_create_shell_surface(wl_surface_t *surface);
int wayland_set_shell_surface_title(wl_shell_surface_t *shell, 
                                    const char *title);

// X11
x11_display_t *x11_open_display(const char *display_name);
x11_window_t *x11_create_window(x11_display_t *display, 
                                x11_window_t *parent, rect_t *geometry);
int x11_map_window(x11_display_t *display, x11_window_t *window);
int x11_set_window_title(x11_display_t *display, x11_window_t *window, 
                         const char *title);

// Window Management
window_t *window_create(u32 type, rect_t *geometry);
int window_destroy(u32 window_id);
int window_show(u32 window_id);
int window_hide(u32 window_id);
int window_focus(u32 window_id);
int window_move(u32 window_id, s32 x, s32 y);
int window_resize(u32 window_id, u32 w, u32 h);
int window_maximize(u32 window_id);
int window_minimize(u32 window_id);
int window_restore(u32 window_id);

// Compositor
int compositor_init(void);
int compositor_composite(void);
int compositor_add_window(window_t *window);
int compositor_focus_window(u32 window_id);
int compositor_move_cursor(s32 x, s32 y);
int compositor_set_vsync(bool enable);
```

---

## Interface Drivers

### Location
```
drivers/video/
├── vesa.h         # VESA/VGA driver
├── drm.h          # DRM/KMS driver
└── window.h       # Window system
```

### Supported Interfaces

| Interface | Type | Description |
|-----------|------|-------------|
| DVI | Digital | Digital Visual Interface |
| I2C Display | Serial | I2C-based displays |
| SPI Display | Serial | SPI-based displays |
| Display Serial | Serial | DSI for mobile displays |

---

## Qt Platform Plugins

### Location
```
gui/qt-platform/
├── qt-platform.h    # Qt platform header
└── qt-platform.c    # Qt platform implementation
```

### Supported Platforms

| Platform | Description |
|----------|-------------|
| LinuxFB | Linux Framebuffer plugin |
| EGLFS | EGL FullScreen plugin |
| Wayland | Wayland platform plugin |
| Native NEXUS | Native NEXUS OS plugin |

### Key Structures

```c
// Qt Platform
typedef struct {
    u32 platform_id;
    u32 type;                       // QT_PLATFORM_*
    char name[64];
    egl_display_t *egl_display;
    egl_context_t *egl_context;
    egl_surface_t *egl_surface;
    u32 screen_width, screen_height;
    u32 dpi_x, dpi_y;
} qt_platform_t;

// EGL Display
typedef struct {
    u32 display_id;
    void *native_display;
    bool is_initialized;
    char *extensions;
} egl_display_t;

// EGL Surface
typedef struct {
    u32 surface_id;
    egl_display_t *display;
    egl_config_t *config;
    void *native_window;
    u32 width, height;
} egl_surface_t;
```

### API Functions

```c
// Initialization
int qt_platform_init(void);
int linuxfb_init(void);
int eglfs_init(void);

// Platform Management
int qt_platform_register(u32 type, const char *name);
qt_platform_t *qt_platform_get_active(void);
int qt_platform_set_active(u32 platform_id);

// LinuxFB Plugin
int linuxfb_open(const char *fb_device);
int linuxfb_set_mode(u32 width, u32 height, u32 bpp);
int linuxfb_get_framebuffer(void **base, u32 *size);
int linuxfb_flush(void);
int linuxfb_wait_vsync(void);

// EGLFS Plugin
egl_display_t *eglfs_get_display(void);
egl_config_t *eglfs_choose_config(egl_display_t *display, u32 *attribs);
egl_surface_t *eglfs_create_surface(egl_display_t *display, 
                                    egl_config_t *config, 
                                    void *native_window);
egl_context_t *eglfs_create_context(egl_display_t *display, 
                                    egl_config_t *config, 
                                    egl_surface_t *surface);
int eglfs_make_current(egl_context_t *context);
int eglfs_swap_buffers(egl_surface_t *surface);

// Window Management
qt_window_t *qt_window_create(u32 width, u32 height, u32 format);
int qt_window_show(u32 window_id);
int qt_window_resize(u32 window_id, u32 width, u32 height);
int qt_window_get_buffer(u32 window_id, void **buffer, u32 *size);
int qt_window_flush(u32 window_id);
```

---

## Additional GUI Support

### Multi-Monitor Support

```c
// Get monitor count
int display_get_monitor_count(void);

// Get monitor info
int display_get_monitor_info(u32 index, display_connector_t *info);

// Set monitor position
int display_set_monitor_position(u32 index, s32 x, s32 y);

// Clone display
int display_clone_monitor(u32 source, u32 target);

// Extend display
int display_extend_monitor(u32 source, u32 target, u32 position);
```

### HiDPI Support

```c
// Set scale factor
int display_set_scale_factor(u32 adapter_id, u32 scale_x, u32 scale_y);

// Get scale factor
int display_get_scale_factor(u32 adapter_id, u32 *scale_x, u32 *scale_y);

// Get DPI
int display_get_dpi(u32 adapter_id, u32 *dpi_x, u32 *dpi_y);
```

### Color Management

```c
// Set gamma
int display_set_gamma(u32 *r, u32 *g, u32 *b, u32 size);

// Set brightness/contrast/saturation
int display_set_brightness(u32 brightness);
int display_set_contrast(u32 contrast);
int display_set_saturation(u32 saturation);

// Color calibration
int display_set_color_calibration(u32 profile_id);
```

### VSync and Page Flipping

```c
// Enable VSync
int display_enable_vsync(bool enable);
int accel_enable_vsync(bool enable);

// Wait for VSync
int display_wait_vsync(void);
int accel_wait_vsync(void);

// Page flip
int display_page_flip(u32 crtc_id, u32 fb_id);
int drm_page_flip(u32 crtc_id, u32 fb_id);
```

### Hardware Cursor

```c
// Show/hide cursor
int display_show_cursor(bool show);
int compositor_show_cursor(bool show);

// Move cursor
int display_move_cursor(u32 x, u32 y);
int compositor_move_cursor(s32 x, s32 y);

// Set cursor image
int display_set_cursor_image(void *image, u32 width, u32 height);
int compositor_set_cursor_image(void *image, u32 width, u32 height);
```

---

## API Reference

### Initialization Sequence

```c
// 1. Initialize display driver
display_init();

// 2. Initialize input driver
input_init();

// 3. Initialize audio driver
audio_init();

// 4. Initialize acceleration
accel_init();
accel_set_active_api(ACCEL_API_OPENGL);

// 5. Initialize window system
window_init();
compositor_init();

// 6. Initialize Qt platform (if using Qt)
qt_platform_init();
linuxfb_init();  // or eglfs_init()
```

### Error Handling

All driver functions return:
- `0` or positive value: Success
- Negative value: Error code

Common error codes:
```c
#define -EINVAL     // Invalid argument
#define -ENOENT     // No such device
#define -EBUSY      // Device busy
#define -ENOMEM     // Out of memory
#define -EAGAIN     // Try again
```

---

## Integration Guide

### Adding a New Display Driver

1. Create header in `drivers/display/`
2. Implement driver functions following `display.h` interface
3. Add to `display_enumerate_adapters()`
4. Register capabilities and modes
5. Update Makefile

### Adding a New Input Driver

1. Create source in `drivers/input/`
2. Implement `open`, `close`, `event` callbacks
3. Register device with `input_enumerate_devices()`
4. Set capabilities (keys, axes, buttons)
5. Queue events on input

### Adding a New Audio Codec

1. Implement HDA codec functions
2. Add to `audio_enumerate_devices()`
3. Define widgets and pins
4. Implement amp gain controls
5. Create playback/capture streams

### Building with Drivers

```bash
# Build with all drivers
make

# Build with specific driver
make DRIVERS="display input audio gpu"

# Clean and rebuild
make clean && make
```

### Testing Drivers

```c
// Test display
display_init();
display_enumerate_adapters();
// Check output in kernel log

// Test input
input_init();
input_enumerate_devices();
// Press keys, move mouse

// Test audio
audio_init();
audio_enumerate_devices();
audio_stream_t *stream = audio_create_stream(1, AUDIO_STREAM_PLAYBACK, &format);
audio_start_stream(stream->stream_id);
```

---

## Driver Status Summary

| Driver Category | Files | Status | Integration |
|-----------------|-------|--------|-------------|
| Display | `drivers/display/display.h`, `display.c` | ✅ Complete | Integrated |
| Input | `drivers/input/input.h`, `input.c` | ✅ Complete | Integrated |
| Audio | `drivers/audio/audio.h`, `audio.c` | ✅ Complete | Integrated |
| GPU | `drivers/gpu/gpu.h`, `gpu.c` | ✅ Complete | Integrated |
| Acceleration | `drivers/gpu/acceleration.h` | ✅ Complete | Header Ready |
| Window System | `drivers/video/window.h` | ✅ Complete | Header Ready |
| Qt Platform | `gui/qt-platform/qt-platform.h` | ✅ Complete | Header Ready |
| VESA/VGA | `drivers/video/vesa.h` | ✅ Complete | Existing |
| DRM/KMS | `drivers/video/drm.h` | ✅ Complete | Existing |

---

**End of Driver Documentation**

For more information, see:
- `README.md` - Project overview
- `FEATURES_SUMMARY.md` - Complete feature list
- `docs/` - Additional documentation
