# NEXUS OS - GUI System Documentation

## Overview

NEXUS OS features a modern, enterprise-grade graphical user interface with glassmorphic effects, comprehensive display settings, and professional iconography.

## Components

### 1. Display Settings (`gui/display/`)

**File:** `display_settings.c`

Features:
- Resolution settings (1920x1080, 2560x1440, 4K)
- Refresh rate configuration
- Color depth settings
- DPI and scaling
- HDR support
- Night light mode
- Color temperature control

### 2. Background System

**8 Background Presets:**

**Solid Colors:**
- Midnight Blue (#1E3A5F)
- Dark Gray (#2C3E50)
- Pure Black (#000000)

**Gradients:**
- Sunset (Horizontal: Red → Orange → Gold)
- Ocean (Vertical: Deep Blue → Turquoise)
- Aurora (Radial: Green → Cyan → Blue)
- NEXUS Brand (Vertical: Navy → Blue → Sky)

**Special:**
- Glassmorphic (Frosted glass effect)

### 3. Glassmorphic Effects

| Property | Range | Default |
|----------|-------|---------|
| Blur Radius | 0-50px | 20px |
| Transparency | 0-100% | 70% |
| Border Color | Any | White |
| Border Width | 0-5px | 1px |
| Corner Radius | 0-24px | 12px |
| Noise Amount | 0-20% | 5% |
| Tint | Color + Opacity | Optional |
| Shadow | Blur, Offset, Color | Enabled |

### 4. Panel System

**Top Panel:**
- Height: 28px
- Widgets: Start menu, Search, Tasks, System tray (network, sound, battery, clock)
- Glassmorphic styling
- Auto-hide option

**Bottom Dock:**
- Height: 64px
- 12 widget slots
- App launcher icons
- Glassmorphic styling

**Side Panels:**
- Left Panel (64px, optional)
- Right Panel (320px, optional)
- Both with auto-hide

### 5. Icon Library (`gui/icons/`)

**Total Icons:** 70+

**Categories:**

**Network (16):**
- WiFi (full, medium, low, off)
- Ethernet
- Network status (connected, disconnected, error)
- Bluetooth (on, off)
- Server, Cloud (upload, download)
- Router, Firewall, VPN

**System (11):**
- CPU, Memory, Storage, GPU
- Battery (full, medium, low)
- Temperature, Fan

**Display (5):**
- Display, Brightness, Resolution
- Night light, HDR

**Desktop (7):**
- Desktop, Wallpaper, Theme
- Effects, Glassmorphic
- Panel, Dock

**Application (8):**
- Terminal, Settings, Files, Browser
- VM, Container, Update, Security

**System Info (7):**
- Info, Version, Build
- Kernel, Architecture
- Owner, Install date

**Utility (6):**
- Search, Tasks, Spacer
- Trash, Minimize all, Show desktop

### 6. Desktop Environment (`gui/desktop-environment/`)

**File:** `desktop-env.c` (2012 lines)

Features:
- Icon management with grid layout
- Panel creation and management
- Start menu
- Search functionality
- System tray
- Notifications
- Theme support
- Animation support

### 7. Login Screen (`gui/login/`)

**File:** `login-screen.c` (1866 lines)

Features:
- User selection with avatars
- Password/PIN/Biometric authentication
- Session type selector
- Accessibility menu
- Power options
- Clock and date display
- 2FA support

### 8. Onboarding Wizard (`gui/onboarding/`)

**File:** `onboarding-wizard.c` (1675 lines)

**12 Steps:**
1. Welcome
2. Theme Selection
3. Desktop Layout
4. Taskbar Position
5. Start Menu Style
6. Window Management
7. File Associations
8. App Permissions
9. Keyboard Shortcuts
10. Touchpad Gestures
11. Notifications
12. Complete

## Usage

### Display Settings

```c
// Initialize display settings
display_settings_init();

// Set background
display_set_background("Sunset");

// Apply glassmorphic effects
display_apply_glassmorphic(&glass_settings);

// Show display settings
display_show_settings();
```

### Icon Usage

```c
// Get icon
const char *icon = icon_get("wifi-full");

// Render icon
icon_render("cpu", x, y, size);

// List all icons
icon_list_all();
```

### OS Information

```c
// Get OS version
const char *version = display_get_os_version();

// Get full OS info
char buffer[512];
display_get_os_info(buffer, 512);

// Show system info
display_show_system_info();
```

## Color Theme

**Primary Colors:**
- Primary: #3498DB (Blue)
- Success: #2ECC71 (Green)
- Warning: #F39C12 (Orange)
- Error: #E74C3C (Red)
- Purple: #9B59B6
- Gray: #95A5A6
- Dark: #2C3E50

## References

- [GUI README](../README.md)
- [Display Settings API](display/display_settings.c)
- [Icon Library](icons/icon_library.c)
