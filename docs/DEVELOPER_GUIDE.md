# NEXUS OS - Developer Quick Start Guide

## Copyright (c) 2026 NEXUS Development Team

---

## 🚀 Getting Started

### Prerequisites

```bash
# Build dependencies
sudo apt-get install -y build-essential nasm \
    grub-pc-bin grub-common xorriso mtools qemu-system-x86

# Development tools
sudo apt-get install -y git gdb valgrind
```

### Clone Repository

```bash
git clone https://github.com/devTechs001/nexus-os.git
cd nexus-os
```

### Build

```bash
# Build kernel and ISO
make

# Run in QEMU
make run

# Debug mode
make run-debug
```

---

## 📁 Project Structure

```
NEXUS-OS/
├── kernel/                 # Kernel core
├── drivers/                # Device drivers
│   ├── storage/           # NVMe, AHCI, SD
│   ├── gpu/               # GPU graphics
│   ├── network/           # Network
│   ├── input/             # Input devices
│   ├── audio/             # Audio
│   ├── scanner/           # Scanners
│   └── video/             # Display (VESA, DRM)
├── gui/                    # GUI framework
│   ├── desktop-environment/
│   ├── window-manager/
│   ├── compositor/
│   ├── login/
│   ├── onboarding/
│   ├── session/
│   ├── storage/           # Storage manager UI
│   └── qt-platform/       # Qt plugin
├── apps/                   # Applications
│   ├── file-explorer/
│   ├── terminal/
│   └── [new apps here]
├── system/                 # System services
├── security/               # Security framework
├── boot/                   # Bootloader
├── docs/                   # Documentation
└── Makefile               # Build system
```

---

## 🛠️ Adding New Components

### New Driver

1. **Create driver directory:**
```bash
mkdir -p drivers/<category>/<driver-name>
```

2. **Create header file:**
```c
/* drivers/<category>/<driver-name>/<driver>.h */
#ifndef _DRIVER_H
#define _DRIVER_H

#include "../../../kernel/include/types.h"

// Driver structures and functions

#endif
```

3. **Create implementation:**
```c
/* drivers/<category>/<driver-name>/<driver>.c */
#include "<driver>.h"
#include "../../../kernel/include/kernel.h"

// Driver implementation
```

4. **Update Makefile:**
```makefile
# Add to C_SOURCES
$(DRIVERS_DIR)/<category>/<driver-name>/<driver>.c
```

### New Application

1. **Create app directory:**
```bash
mkdir -p apps/<app-name>
```

2. **Create app files:**
```c
/* apps/<app-name>/<app>.h */
/* apps/<app-name>/<app>.c */
```

3. **Update Makefile**

### New GUI Component

1. **Create component directory:**
```bash
mkdir -p gui/<component-name>
```

2. **Follow existing GUI patterns** (see `gui/desktop-environment/`)

---

## 🧪 Testing

### Unit Tests

```bash
# Run tests
make test

# Run specific test
make test-<component>
```

### QEMU Testing

```bash
# Standard QEMU
make run

# With GDB
make run-gdb

# With serial console
make run-serial
```

### VMware Testing

```bash
# Auto-create and launch
make run-vmware
```

---

## 📝 Code Style

### Naming Conventions

```c
// Types: snake_case_t
typedef struct {
    u32 field_name;
} my_type_t;

// Functions: snake_case
int my_function(void);

// Constants: UPPER_CASE
#define MY_CONSTANT 100

// Variables: snake_case
u32 my_variable;
```

### File Structure

```c
/*
 * NEXUS OS - Component Name
 * path/to/file.h
 *
 * Description
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _FILE_H
#define _FILE_H

// Includes
// Constants
// Types
// Functions

#endif
```

---

## 🔍 Debugging

### Kernel Debugging

```bash
# Start QEMU with GDB
make run-gdb

# In another terminal
gdb build/nexus-kernel.bin
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

### Logging

```c
// Kernel logging
printk("[COMPONENT] Message: %d\n", value);

// User-space logging
printf("[APP] Message\n");
```

### Memory Debugging

```bash
# Check for memory leaks
valgrind --leak-check=full ./test-program
```

---

## 📚 Documentation

### Reading Order

1. `README.md` - Project overview
2. `docs/IMPLEMENTATION_ROADMAP.md` - What to implement
3. `docs/PHASE_PROGRESS.md` - Current progress
4. Component-specific docs in `docs/`

### Writing Documentation

- Use Markdown format
- Include code examples
- Add diagrams where helpful
- Keep updated with code changes

---

## 🎯 Current Priorities

### Immediate (Phase 3A)
1. Text Editor
2. System Settings
3. Enhanced Status Bar
4. Application Dock

### Short Term (Phase 3B)
5. Control Panel
6. App Store
7. Desktop Dashboard
8. Notification Center

See `docs/IMPLEMENTATION_ROADMAP.md` for complete list.

---

## 🤝 Contributing

### Pull Request Process

1. Fork repository
2. Create feature branch
3. Implement feature
4. Write tests
5. Update documentation
6. Submit PR

### Commit Message Format

```
type: Short description

Longer description if needed.

- Feature 1
- Feature 2

Fixes: #123
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Code style
- `refactor`: Refactoring
- `test`: Tests
- `chore`: Build/config

---

## 📞 Support

- **Issues:** https://github.com/devTechs001/nexus-os/issues
- **Discussions:** https://github.com/devTechs001/nexus-os/discussions
- **Email:** nexus-os@darkhat.dev

---

**Happy Coding! 🚀**
