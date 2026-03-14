# NEXUS OS - Terminal Documentation

## Overview

NEXUS OS includes an enhanced terminal emulator with sudo support, command history, aliases, emulations, help system, animations, and visual themes.

## Components

### 1. Terminal Core (`apps/terminal/`)

**Files:**
- `terminal.c` - Main terminal emulator
- `terminal.h` - Header file
- `terminal_enhanced.c` - Sudo and advanced features
- `terminal_features.c` - History, aliases, help
- `terminal_animations.c` - Visual effects

### 2. Sudo System

**Features:**
- Password authentication
- Session management (300s timeout)
- Command permission checking
- Failed attempt locking (max 3)
- Command history tracking
- 256 sudo users support
- 64 predefined admin commands

**Sudo Permissions:**
- SUDO_PERM_READ - Read access
- SUDO_PERM_WRITE - Write access
- SUDO_PERM_EXECUTE - Execute access
- SUDO_PERM_ADMIN - Administrative access
- SUDO_PERM_ROOT - Root access
- SUDO_PERM_ALL - Full access

**Command Categories:**
- CMD_CAT_GENERAL - General commands
- CMD_CAT_FILE - File operations
- CMD_CAT_NETWORK - Network commands
- CMD_CAT_SYSTEM - System commands
- CMD_CAT_PACKAGE - Package management
- CMD_CAT_USER - User management
- CMD_CAT_SERVICE - Service control
- CMD_CAT_SECURITY - Security commands

### 3. Command History

**Features:**
- 10,000 command capacity
- No duplicates
- Circular buffer
- Search functionality (Ctrl+R)
- Navigation (Up/Down arrows)
- Timestamp support
- Save to file

**Commands:**
```bash
history              # Show all history
history 20           # Show last 20 commands
history -c           # Clear history
```

### 4. Aliases

**19 Default Aliases:**

| Alias | Command | Description |
|-------|---------|-------------|
| `ll` | `ls -la` | List long format |
| `la` | `ls -A` | List all except . .. |
| `l` | `ls -CF` | List with classification |
| `gs` | `git status` | Git status |
| `ga` | `git add` | Git add |
| `gc` | `git commit` | Git commit |
| `gp` | `git push` | Git push |
| `gl` | `git log` | Git log |
| `upd` | `update` | Update system |
| `inst` | `install` | Install package |
| `rm` | `rm -i` | Remove with confirmation |
| `mv` | `mv -i` | Move with confirmation |
| `cp` | `cp -i` | Copy with confirmation |
| `..` | `cd ..` | Go up one directory |
| `...` | `cd ../..` | Go up two directories |
| `h` | `history` | Show history |
| `help` | `help` | Show help |
| `cls` | `clear` | Clear screen |

**Commands:**
```bash
alias                    # List all aliases
alias ll='ls -la'        # Create alias
unalias ll               # Remove alias
```

### 5. Emulation Modes

**4 Modes:**
- **bash** - Bash shell emulation
- **sh** - Bourne shell emulation
- **cmd** - Windows CMD emulation
- **powershell** - PowerShell emulation

**Commands:**
```bash
emulation list           # List modes
emulation set bash       # Switch to bash mode
```

### 6. Help System

**10 Built-in Topics:**
- sudo, history, alias, clear, cd
- ls, cat, grep, ps, kill

**Commands:**
```bash
help                     # Show all commands
help sudo                # Help for specific command
help search network      # Search help topics
```

### 7. Color Themes

**10 Built-in Themes:**
- Dracula (Dark with vibrant colors)
- Monokai (Classic dark)
- Solarized Dark
- Gruvbox Dark
- Nord (Arctic north)
- One Dark (Atom default)
- Material Dark
- Tomorrow Night
- Solarized Light
- GitHub Light

**Commands:**
```bash
schemes                  # List themes
theme Dracula            # Switch theme
```

### 8. Preferences

**Categories:**

**Appearance:**
- Theme selection
- Font family and size
- Font bold/italic/ligatures
- Anti-aliasing

**Window:**
- Width/height in chars
- Show/hide title bar, tabs, scrollbar, status
- Fullscreen/maximized
- Opacity (0-100%)
- Blur background
- Rounded corners

**Cursor:**
- Style (Block/Underline/Vertical)
- Blink enable/disable
- Blink rate (ms)

**Text:**
- Line height
- Letter spacing
- Smooth scrolling
- Scrollback lines

**Input:**
- Natural scrolling
- Mouse reporting
- Alt sends escape
- Ctrl+Shift shortcuts

**Advanced:**
- GPU acceleration
- VSync
- Pixel perfect
- True color (24-bit)
- Unicode support

### 9. Animations

**10 Animation Types:**
- Cursor blink
- Smooth scroll
- Fade in/out
- Typing effect
- Wave effect
- Pulse effect
- Rainbow effect
- Matrix effect
- Glitch effect

**Speeds:**
- SLOW (1000ms)
- NORMAL (500ms)
- FAST (250ms)
- INSTANT (0ms)

## Usage

### Sudo Commands

```bash
sudo ls /root          # List root directory
sudo -i                # Root shell
sudo reboot            # Reboot system
```

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+T | New Tab |
| Ctrl+Shift+N | New Window |
| Ctrl+Shift+W | Close Tab |
| Ctrl+Shift+C | Copy |
| Ctrl+Shift+V | Paste |
| Ctrl+Shift++ | Zoom In |
| Ctrl+Shift+- | Zoom Out |
| Ctrl+Shift+0 | Reset Zoom |
| Ctrl+L | Clear Screen |
| Ctrl+D | Exit |
| Ctrl+R | Reverse Search |
| ↑/↓ | Command History |

### Preferences Commands

```bash
preferences                    # Show all settings
set theme Monokai              # Set theme
set font_size 16               # Set font size
set cursor_style block         # Set cursor style
set cursor_blink true          # Enable blink
```

## Visual Effects

```c
// Fade effects
effect_fade_in(500);           // 500ms fade in
effect_fade_out(500);          // 500ms fade out

// Special effects
effect_rainbow(2000);          // 2s rainbow
effect_matrix(5000);           // 5s matrix
effect_glitch(50, 1000);       // Glitch effect
```

## References

- [Terminal Core](terminal/terminal.c)
- [Terminal Features](terminal/terminal_features.c)
- [Terminal Animations](terminal/terminal_animations.c)
