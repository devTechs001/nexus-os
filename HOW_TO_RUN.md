# NEXUS OS - How to Run

## Quick Start

```bash
# Build and run in QEMU
make && make run

# Or manually
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2
```

---

## Installing QEMU

### Kali Linux / Debian / Ubuntu

```bash
# Update package list
sudo apt-get update

# Install QEMU
sudo apt-get install -y qemu-system-x86 qemu-utils

# Verify installation
qemu-system-x86_64 --version
```

### Fedora / RHEL

```bash
sudo dnf install -y qemu-system-x86 qemu-img
```

### Arch Linux

```bash
sudo pacman -S qemu-system-x86 qemu-img
```

### macOS (with Homebrew)

```bash
brew install qemu
```

### Windows

Download from: https://www.qemu.org/download/

Or use Chocolatey:
```powershell
choco install qemu
```

---

## Running NEXUS OS

### Option 1: Using Make (Recommended)

```bash
cd /path/to/NEXUS-OS

# Build and run
make run

# Run with debug output
make run-debug
```

### Option 2: Manual QEMU Command

```bash
# Basic run
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2

# With GUI display
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2 \
    -display gtk,gl=on

# With serial console
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2 \
    -serial stdio -display none

# With debugging
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2 \
    -d int -no-reboot -no-shutdown
```

### Option 3: VMware

```bash
# Auto-create VM and boot
make run-vmware

# Or manually:
# 1. Open VMware Workstation/Player
# 2. Create new VM → "I will install later"
# 3. Select: Linux → Other Linux 64-bit
# 4. Use ISO: build/nexus-kernel.iso
# 5. Finish and Start VM
```

---

## Boot Menu

When NEXUS OS boots, you'll see a **graphical boot menu**:

```
┌─────────────────────────────────────────────────────────────────┐
│                     NEXUS OS                                     │
│              Universal Operating System                          │
│           Version 1.0.0 (Genesis) - Copyright 2026               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🖥️  NEXUS OS (VMware Mode)                            │    │
│  │     Optimized for VMware - Default selection           │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🛡️  NEXUS OS (Safe Mode)                              │    │
│  │     Minimal drivers, no SMP - For troubleshooting      │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🐛  NEXUS OS (Debug Mode)                             │    │
│  │     Verbose logging - For developers                   │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🔧  NEXUS OS (Native Mode)                            │    │
│  │     Bare metal - No virtualization                     │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🧪  Memory Test (MemTest86)                           │    │
│  │     Test system memory for errors                      │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ ⌨️  Terminal / Shell                                  │    │
│  │     Command line interface                             │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│  ↑↓ : Select    Enter : Boot    E : Edit    T : Terminal       │
│  R : Restart    S : Shutdown                                    │
│                                                                  │
│  Booting in: ████████████████░░░░░░░░░░░░  [7 seconds]          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Boot Menu Options

| Option | Description | Use Case |
|--------|-------------|----------|
| **VMware Mode** (Default) | Optimized for VMware with paravirtualized drivers | Running in VMware |
| **Safe Mode** | Minimal drivers, no SMP, no APIC | Troubleshooting boot issues |
| **Debug Mode** | Verbose logging (loglevel=7) | Development and debugging |
| **Native Mode** | No virtualization, bare metal | Physical hardware |
| **Memory Test** | MemTest86 memory diagnostic | Testing RAM |
| **Terminal** | Command-line shell | Advanced users |

---

## Keyboard Controls

| Key | Action |
|-----|--------|
| `↑` `↓` | Navigate menu |
| `Enter` | Boot selected option |
| `E` | Edit boot parameters |
| `T` | Open terminal |
| `R` | Restart system |
| `S` | Shutdown |
| `0-9` | Quick select (0=first, 9=tenth) |

---

## QEMU Options Reference

### Display Options

```bash
# GTK GUI (default)
-display gtk,gl=on

# SDL GUI
-display sdl

# No display (headless)
-display none

# VNC server
-vnc :0
```

### Memory Options

```bash
# 2GB RAM (default)
-m 2G

# 4GB RAM
-m 4G

# 8GB RAM
-m 8G
```

### CPU Options

```bash
# 2 CPUs (default)
-smp 2

# 4 CPUs
-smp 4

# 8 CPUs with specific topology
-smp 8,sockets=2,cores=4,threads=1
```

### Disk Options

```bash
# Add a hard disk
-drive file=disk.img,format=qcow2

# Create a 20GB disk
qemu-img create -f qcow2 disk.img 20G
```

### Network Options

```bash
# User mode networking (default)
-netdev user,id=net0
-device e1000,netdev=net0

# Bridged networking
-netdev bridge,id=net0
-device e1000,netdev=net0
```

### Debug Options

```bash
# Show interrupts
-d int

# Show CPU resets
-d cpu_reset

# Log to file
-D qemu.log

# No reboot on crash
-no-reboot -no-shutdown
```

---

## Troubleshooting

### "QEMU not found"

Install QEMU:
```bash
sudo apt-get install qemu-system-x86
```

### "ISO not found"

Rebuild the ISO:
```bash
make clean && make
```

### "Display error"

Try different display backend:
```bash
# Use SDL instead of GTK
-display sdl

# Or use VNC
-vnc :0
# Then connect with: vncviewer localhost:0
```

### "Boot fails"

Try Safe Mode:
1. In boot menu, select "NEXUS OS (Safe Mode)"
2. Press Enter to boot

### "No output"

Add serial console:
```bash
qemu-system-x86_64 -cdrom build/nexus-kernel.iso \
    -m 2G -serial stdio
```

---

## System Requirements

### Minimum

- **CPU**: x86_64 processor
- **RAM**: 512 MB
- **Storage**: 100 MB for ISO
- **Display**: 1024x768 resolution

### Recommended

- **CPU**: Dual-core x86_64
- **RAM**: 2 GB
- **Storage**: 1 GB
- **Display**: 1920x1080 resolution

---

## Next Steps

After booting:

1. **Explore the boot logs** - See platform detection and initialization
2. **Try different boot modes** - Safe Mode, Debug Mode, Native Mode
3. **Open terminal** - Press `T` in boot menu
4. **Check documentation** - `docs/COMPLETE_DOCUMENTATION.md`

---

## Support

- **Documentation**: `docs/`
- **Quick Start**: `QUICKSTART.md`
- **GitHub**: https://github.com/devTechs001/nexus-os

---

## License

NEXUS OS - Copyright (c) 2026 NEXUS Development Team
