# NEXUS OS - Complete Setup Guide

## Copyright (c) 2026 NEXUS Development Team

---

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Boot Process](#boot-process)
5. [Setup Options](#setup-options)
6. [Post-Installation](#post-installation)
7. [Troubleshooting](#troubleshooting)

---

## Overview

NEXUS OS is a next-generation, universal operating system designed for deployment across:
- Desktop computers
- Servers
- Mobile devices
- IoT devices
- Embedded systems
- Virtual machines (VMware, VirtualBox, QEMU/KVM, Hyper-V)

**Version:** 1.0.0 (Genesis)  
**Architecture:** x86_64  
**License:** Proprietary with open-source components

---

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | x86_64 processor (Intel or AMD) |
| **RAM** | 512 MB |
| **Storage** | 100 MB for installation |
| **Display** | 1024×768 resolution |
| **Boot** | UEFI or BIOS with multiboot2 support |

### Recommended Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | Dual-core x86_64 (2+ GHz) |
| **RAM** | 2 GB or more |
| **Storage** | 20 GB SSD |
| **Display** | 1920×1080 resolution |
| **Network** | Ethernet or WiFi adapter |

### Virtualization Requirements

| Platform | Requirements |
|----------|--------------|
| **VMware** | VMware Workstation 15+ or Player 15+ |
| **VirtualBox** | VirtualBox 6.0+ |
| **QEMU/KVM** | QEMU 4.0+, KVM support |
| **Hyper-V** | Windows 10 Pro/Enterprise or Windows Server |

---

## Installation

### Step 1: Download NEXUS OS

```bash
# Clone from GitHub
git clone https://github.com/devTechs001/nexus-os.git
cd nexus-os

# Or download the ISO directly
wget https://github.com/devTechs001/nexus-os/releases/download/v1.0.0/nexus-os-1.0.0.iso
```

### Step 2: Build from Source (Optional)

```bash
# Install build dependencies
sudo apt-get install -y build-essential nasm \
    grub-pc-bin grub-common xorriso mtools

# Build the ISO
make

# Output: build/nexus-kernel.iso (15 MB)
```

### Step 3: Create Bootable Media

#### USB Drive
```bash
# Write ISO to USB (replace /dev/sdX with your USB device)
sudo dd if=build/nexus-kernel.iso of=/dev/sdX bs=4M status=progress
sudo sync
```

#### DVD
```bash
# Burn ISO to DVD
growisofs -dvd-compat -Z /dev/dvd=build/nexus-kernel.iso
```

### Step 4: Boot NEXUS OS

#### Physical Hardware
1. Insert USB/DVD
2. Boot from media (may need to change boot order in BIOS/UEFI)
3. Select boot option from graphical menu

#### VMware
```bash
# Auto-create VM and boot
make run-vmware

# Or manually:
# 1. Open VMware Workstation/Player
# 2. Create new VM → "I will install later"
# 3. Select: Linux → Other Linux 64-bit
# 4. Use ISO: build/nexus-kernel.iso
# 5. Start VM
```

#### QEMU
```bash
# Run in QEMU
make run

# Or manually
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2 \
    -display gtk,gl=on
```

---

## Boot Process

### Graphical Boot Menu

When NEXUS OS boots, you'll see a **graphical boot menu**:

```
┌─────────────────────────────────────────────────────────────────┐
│                     NEXUS OS                                     │
│              Universal Operating System                          │
│           Version 1.0.0 (Genesis) - Copyright 2026               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ 🖥️  NEXUS OS (VMware Mode)      [SELECTED]            │    │
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
│  Booting in: ████████████████░░░░░░░░░░░░  [10 seconds]         │
└─────────────────────────────────────────────────────────────────┘
```

### Boot Options

| Option | Description | When to Use |
|--------|-------------|-------------|
| **VMware Mode** (Default) | Optimized for VMware with paravirtualized drivers | Running in VMware |
| **Safe Mode** | Minimal drivers, no SMP, no APIC | Troubleshooting boot issues |
| **Debug Mode** | Verbose logging (loglevel=7) | Development and debugging |
| **Native Mode** | No virtualization, bare metal | Physical hardware |
| **Memory Test** | MemTest86 memory diagnostic | Testing RAM |
| **Terminal** | Command-line shell | Advanced users, recovery |

### Keyboard Controls

| Key | Action |
|-----|--------|
| `↑` `↓` | Navigate menu |
| `Enter` | Boot selected option |
| `E` | Edit boot parameters |
| `T` | Open terminal mode |
| `R` | Restart system |
| `S` | Shutdown system |
| `0-9` | Quick select entry |

---

## Setup Options

After booting, you can choose between **Terminal** or **Graphical** setup:

### Option 1: Graphical Setup (Recommended)

```
┌─────────────────────────────────────────────────────────────────┐
│              NEXUS OS - Setup Wizard                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Welcome to NEXUS OS Setup!                                     │
│                                                                  │
│  This wizard will help you configure your system.               │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │  1. Language Selection                                 │    │
│  │  2. Keyboard Layout                                    │    │
│  │  3. Time Zone                                          │    │
│  │  4. User Account Creation                              │    │
│  │  5. Network Configuration                              │    │
│  │  6. Disk Partitioning (if installing)                  │    │
│  │  7. Installation Options                               │    │
│  │  8. Review and Confirm                                 │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  [Start Setup]  [Open Terminal]  [Reboot]  [Shutdown]          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**

1. **Language Selection**
   - Choose your preferred language
   - Over 100 languages supported

2. **Keyboard Layout**
   - Select keyboard layout
   - Test input

3. **Time Zone**
   - Select your location
   - Auto-detect available

4. **User Account**
   - Create admin account
   - Set password
   - Optional: Create additional users

5. **Network Configuration**
   - Auto-detect network
   - Configure WiFi (if available)
   - Set hostname

6. **Disk Partitioning** (if installing)
   - Guided partitioning
   - Manual partitioning
   - Full disk encryption option

7. **Installation Options**
   - Install location
   - Bootloader installation
   - Additional packages

8. **Review and Confirm**
   - Review all settings
   - Confirm installation

### Option 2: Terminal Setup

Press `T` in the boot menu or select "Terminal / Shell" to use terminal-based setup:

```bash
# NEXUS OS Terminal
# Copyright (c) 2026 NEXUS Development Team

nexus-login: root
Password: 

# Welcome to NEXUS OS (TTY1)
# Type 'setup' to start the installation wizard

root@nexus:~# setup

NEXUS OS Setup - Terminal Mode
===============================

1. Language Selection
2. Keyboard Layout
3. Time Zone
4. User Account Creation
5. Network Configuration
6. Disk Partitioning
7. Installation Options
8. Review and Confirm
0. Exit to Shell

Select option [1-8, 0]: 
```

**Terminal Commands:**

| Command | Description |
|---------|-------------|
| `setup` | Start setup wizard |
| `nexus-install` | Start installation |
| `nexus-config` | Configure system |
| `nexus-network` | Configure network |
| `nexus-user` | Manage users |
| `nexus-disk` | Disk management |
| `help` | Show available commands |
| `exit` | Exit to boot menu |

---

## Post-Installation

### First Boot

After installation, on first boot:

```
┌─────────────────────────────────────────────────────────────────┐
│              NEXUS OS - First Boot Setup                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Welcome to your new NEXUS OS installation!                     │
│                                                                  │
│  Please complete the following steps:                           │
│                                                                  │
│  □ Accept License Agreement                                     │
│  □ Create User Account                                          │
│  □ Configure Network                                            │
│  □ Enable Automatic Updates                                     │
│  □ Install Recommended Software                                 │
│  □ Configure Privacy Settings                                   │
│                                                                  │
│  [Continue]  [Skip]  [Help]                                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Desktop Environment

After setup, you'll see the NEXUS Desktop:

```
┌─────────────────────────────────────────────────────────────────┐
│  [NEXUS]  File  Edit  View  Go  Tools  Help                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  📁 File       🌐 Browser   ⚙️ Settings                         │
│  Explorer                                                       │
│                                                                  │
│  📧 Email     🎵 Media     🛒 App Store                         │
│                                                                  │
│  📝 Text      💻 Terminal  📊 System Monitor                    │
│  Editor                                                         │
│                                                                  │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│  [Start]  [Search]                          🕐 14:30  📶 📶 🔊  │
└─────────────────────────────────────────────────────────────────┘
```

### App Store

Access the NEXUS App Store to install applications:

```
┌─────────────────────────────────────────────────────────────────┐
│              NEXUS App Store                                    │
├─────────────────────────────────────────────────────────────────┤
│  Categories:  All  Productivity  Development  Games  More...   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │
│  │  📝 VS Code │ │  🌐 Firefox │ │  🎮 Steam   │               │
│  │  ⭐ 4.8     │ │  ⭐ 4.7     │ │  ⭐ 4.5     │               │
│  │  [Install]  │ │  [Install]  │ │  [Install]  │               │
│  └─────────────┘ └─────────────┘ └─────────────┘               │
│                                                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │
│  │  🎵 Spotify │ │  💬 Discord │ │  📹 OBS     │               │
│  │  ⭐ 4.6     │ │  ⭐ 4.4     │ │  ⭐ 4.7     │               │
│  │  [Install]  │ │  [Install]  │ │  [Install]  │               │
│  └─────────────┘ └─────────────┘ └─────────────┘               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Troubleshooting

### "No Bootable Device"

**Cause:** The ISO is not properly created or the VM is not configured correctly.

**Solution:**

1. **Rebuild the ISO:**
   ```bash
   make clean && make
   ```

2. **Verify ISO:**
   ```bash
   ls -lh build/nexus-kernel.iso
   # Should be ~15 MB
   ```

3. **VMware Configuration:**
   - VM Type: Linux → Other Linux 64-bit
   - Firmware: BIOS (not UEFI for now)
   - Boot Order: CD/DVD first
   - ISO attached: Yes

4. **QEMU Configuration:**
   ```bash
   qemu-system-x86_64 -cdrom build/nexus-kernel.iso \
       -m 2G -smp 2 -boot d
   ```

### Boot Menu Not Showing

**Cause:** GRUB theme not loading or resolution issue.

**Solution:**

1. **Force text mode:**
   - In boot menu, press `E` to edit
   - Remove `gfxmode` line
   - Press `F10` to boot

2. **Change resolution:**
   - Edit GRUB config
   - Set `gfxmode=1024x768`

### Network Not Working

**Solution:**

1. **Check network adapter:**
   ```bash
   nexus-network status
   ```

2. **Restart network:**
   ```bash
   nexus-network restart
   ```

3. **Manual configuration:**
   ```bash
   nexus-network set-static 192.168.1.100 255.255.255.0 192.168.1.1
   ```

### Installation Fails

**Solution:**

1. **Check disk space:**
   ```bash
   nexus-disk info
   ```

2. **Verify ISO integrity:**
   ```bash
   sha256sum build/nexus-kernel.iso
   ```

3. **Try Safe Mode:**
   - Select "NEXUS OS (Safe Mode)" in boot menu

---

## Support

### Documentation

- `HOW_TO_RUN.md` - Quick start guide
- `docs/COMPLETE_DOCUMENTATION.md` - Full system documentation
- `docs/architecture/` - Architecture guides
- `docs/api/` - API reference

### Community

- **GitHub:** https://github.com/devTechs001/nexus-os
- **Issues:** https://github.com/devTechs001/nexus-os/issues
- **Discussions:** https://github.com/devTechs001/nexus-os/discussions

### Contact

- **Email:** nexus-os@darkhat.dev
- **Website:** https://nexus-os.dev

---

## License

NEXUS OS - Copyright (c) 2026 NEXUS Development Team

Proprietary with open-source components. Dual-licensing available.

See `LICENSE` file for details.
