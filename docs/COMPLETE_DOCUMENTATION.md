# NEXUS OS - Complete System Documentation

## Overview

NEXUS OS is a next-generation, universal operating system designed for deployment across mobile, desktop, server, IoT, and embedded platforms with native hypervisor support and modern security features.

**Version:** 1.0.0 (Genesis)  
**License:** Proprietary with open-source components  
**Architecture:** x86_64 (ARM64, RISC-V planned)

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Boot Process](#boot-process)
3. [Kernel Subsystems](#kernel-subsystems)
4. [Platform Support](#platform-support)
5. [Networking](#networking)
6. [Security](#security)
7. [GUI & Desktop](#gui--desktop)
8. [Application Framework](#application-framework)
9. [Development](#development)

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     NEXUS User Interface (NUI)                   │
│   Mobile UI  │  Desktop UI  │  Server UI  │  Voice/Gesture      │
├─────────────────────────────────────────────────────────────────┤
│                   Application Framework Layer                    │
│  Android Runtime │ Native Apps │ Web Apps │ Containers │ VMs    │
├─────────────────────────────────────────────────────────────────┤
│                    System Services Layer                         │
│ Power │ Network │ Storage │ Security │ AI │ Media │ Graphics    │
├─────────────────────────────────────────────────────────────────┤
│                   NEXUS Hypervisor (NHV)                         │
│  Hardware VM │ Process VM │ Container VM │ Security Enclaves    │
├─────────────────────────────────────────────────────────────────┤
│                     Microkernel Core                             │
│ Scheduler │ Memory │ IPC │ Drivers │ Filesystem │ Networking    │
├─────────────────────────────────────────────────────────────────┤
│                  Hardware Abstraction Layer                      │
│  x86_64 │ ARM64 │ RISC-V │ GPU │ NPU │ TPU │ Quantum Ready     │
├─────────────────────────────────────────────────────────────────┤
│                      Hardware Layer                              │
│  Mobile │ Desktop │ Server │ IoT │ Embedded │ Edge │ Cloud      │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

| Component | Description | Location |
|-----------|-------------|----------|
| **Kernel** | Hybrid microkernel with preemptive multitasking | `kernel/` |
| **HAL** | Hardware abstraction for multiple architectures | `hal/` |
| **Hypervisor** | Type-1 hypervisor with nested virtualization | `hypervisor/` |
| **Platform** | Multi-platform support (VMware, VirtualBox, etc.) | `platform/` |
| **Network** | Full IPv4/IPv6 stack with firewall | `net/` |
| **Security** | Crypto, auth, sandbox, TPM support | `security/` |
| **GUI** | Modern desktop environment with panels, menus | `gui/` |
| **Registry** | Central configuration database | `system/registry/` |

---

## Boot Process

### Boot Sequence

```
1. UEFI/BIOS Initialization
   ↓
2. Secure Boot Verification
   ↓
3. NEXUS Bootloader (NBL)
   ↓
4. Virtualization Mode Selection
   ↓
5. Hypervisor Initialization (if enabled)
   ↓
6. Microkernel Loading
   ↓
7. Driver Initialization
   ↓
8. System Services Start
   ↓
9. Security Subsystem Activation
   ↓
10. User Interface Launch
   ↓
11. Application Runtime Ready
```

### Boot Menu Options

```
menuentry "NEXUS OS (VMware Mode - Default)" {
    multiboot2 /boot/nexus-kernel.bin
    boot
}

menuentry "NEXUS OS (Safe Mode)" {
    multiboot2 /boot/nexus-kernel.bin nomodeset nosmp noapic
    boot
}

menuentry "NEXUS OS (Debug Mode)" {
    multiboot2 /boot/nexus-kernel.bin debug loglevel=7
    boot
}

menuentry "NEXUS OS (Native Mode)" {
    multiboot2 /boot/nexus-kernel.bin virt=native
    boot
}
```

### Virtualization Detection

At boot, NEXUS OS automatically detects the running environment:

| Signature | Platform | Optimizations |
|-----------|----------|---------------|
| `VMwareVMware` | VMware | VMCI, HGFS, vmxnet3 |
| `VBoxVBoxVBox` | VirtualBox | Guest Additions, VirtIO |
| `KVMKVMKVM` | KVM | VirtIO paravirtualized |
| `Microsoft Hv` | Hyper-V | Hypercall, SynIC |
| (none) | Bare Metal | Native drivers |

---

## Kernel Subsystems

### 1. Memory Management (`kernel/mm/`)

```
├── pmm.c          # Physical memory manager (bitmap-based)
├── vmm.c          # Virtual memory (4-level paging)
├── heap.c         # Kernel heap allocator
├── slab.c         # Slab allocator for objects
└── page_alloc.c   # Buddy system page allocator
```

**Features:**
- Physical page tracking with bitmaps
- Virtual memory with 48-bit address space
- Kernel heap with coalescing
- Slab allocator for common sizes (32B - 4KB)
- Buddy system for multi-page allocations

### 2. Scheduler (`kernel/sched/`)

```
├── scheduler.c    # Main scheduler core
├── process.c      # Process management
├── thread.c       # Thread management
├── cfs.c          # Completely Fair Scheduler
└── realtime.c     # Real-time scheduler (FIFO, RR)
```

**Features:**
- CFS with vruntime-based fairness
- Real-time scheduling (FIFO, Round-Robin)
- Per-CPU run queues
- Load balancing across CPUs
- Priority inheritance

### 3. IPC (`kernel/ipc/`)

```
├── pipe.c         # Pipes (unnamed and named)
├── shm.c          # Shared memory
├── semaphore.c    # System V & POSIX semaphores
├── mutex.c        # Kernel mutexes
└── message_queue.c # POSIX message queues
```

### 4. Synchronization (`kernel/sync/`)

```
├── spinlock.c     # Spinlocks with backoff
├── rwlock.c       # Read-write locks
├── atomic.c       # Atomic operations
└── waitqueue.c    # Wait queues
```

### 5. Syscalls (`kernel/syscall/`)

**256 System Calls:**
- Process: `fork`, `exec`, `exit`, `wait`, `getpid`
- Memory: `mmap`, `munmap`, `brk`, `mprotect`
- File: `open`, `close`, `read`, `write`, `stat`
- IPC: `pipe`, `shmget`, `semop`, `msgget`
- Network: `socket`, `bind`, `connect`, `send`, `recv`
- Time: `gettimeofday`, `nanosleep`, `clock_gettime`

---

## Platform Support

### Supported Platforms

| Platform | Detection | Features |
|----------|-----------|----------|
| **VMware** | CPUID | VMCI, HGFS, vmxnet3, drag&drop |
| **VirtualBox** | CPUID | Guest Additions, VirtIO |
| **QEMU/KVM** | CPUID | VirtIO, MSI/MSI-X |
| **Hyper-V** | CPUID | Hypercall, SynIC, netvsc |
| **Xen** | CPUID | Paravirtualized drivers |
| **Bare Metal** | N/A | Native hardware access |

### Platform Detection Code

```c
platform_type_t platform_detect(void)
{
    // Check CPUID for hypervisor signature
    if (strcmp(signature, "VMwareVMware") == 0) {
        return PLATFORM_VMWARE;
    }
    // ... other platforms
}
```

---

## Networking

### IPv4 Stack (`net/ipv4/`)

- Full TCP/IP implementation
- UDP with checksum offload
- ICMP for diagnostics
- Routing with longest prefix match
- Fragmentation and reassembly

### IPv6 Stack (`net/ipv6/`)

- Complete IPv6 addressing (RFC 4291)
- Neighbor Discovery Protocol (NDP)
- Stateless Address Autoconfiguration (SLAAC)
- EUI-64 interface identifiers
- ICMPv6 (ping)
- Extension headers

### Firewall (`net/firewall/`)

```
├── filter         # Packet filtering
├── nat            # NAT (masquerade, redirect)
├── mangle         # Packet modification
├── raw            # Pre-connection tracking
└── security       # Mandatory access control
```

**Features:**
- Stateful packet inspection
- Connection tracking
- NAT (SNAT, DNAT, masquerade)
- Rate limiting
- Logging

---

## Security

### Cryptography (`security/crypto/`)

| Algorithm | Implementation |
|-----------|----------------|
| AES | 128/192/256-bit (ECB, CBC, CTR) |
| SHA | SHA-256, SHA-512 |
| RSA | 1024/2048/4096-bit |
| ECC | NIST P-256, P-384, P-521 |

### Authentication (`security/auth/`)

- User account management
- Password hashing (PBKDF2)
- Session management
- PAM integration

### Sandboxing (`security/sandbox/`)

- Process isolation
- Seccomp filters
- Resource limits
- Capability-based security

### TPM (`security/tpm/`)

- TPM 2.0 support
- PCR extend/read
- Secure boot attestation
- Hardware random number generation

---

## GUI & Desktop

### Desktop Environment (`gui/desktop/`)

**Components:**
- **Desktop Icons** - Grid or free layout
- **Taskbar** - Bottom panel with window list
- **Start Menu** - Application launcher with search
- **System Tray** - Background apps and notifications
- **Panels** - Top/bottom/left/right panels
- **Workspaces** - Multiple virtual desktops (10 max)

**Theme:**
- Modern dark theme by default
- Accent color customization
- Transparency effects
- Animations

### App Store (`gui/app-store/`)

**Features:**
- Browse by category
- Search with filters
- Ratings and reviews
- Auto-install and update
- Download management
- Recommendations

### File Explorer

- Tree view navigation
- Thumbnail previews
- Search and filters
- Network locations
- Archive support

### Control Panel

- System settings
- User accounts
- Network configuration
- Display settings
- Security policies
- Update management

---

## Application Framework

### Application Types

| Type | Description | Runtime |
|------|-------------|---------|
| Native | Compiled C/C++ apps | Native |
| Web | HTML/JS/CSS apps | Web Engine |
| Container | Containerized apps | Container Runtime |
| Android | APK packages | Android Runtime |
| VM | Virtualized apps | Hypervisor |

### App Package Format

```
.nexus-app/
├── manifest.json    # App metadata
├── icon.svg         # App icon
├── main             # Executable
├── resources/       # Assets
└── lib/             # Dependencies
```

---

## Registry System

### Structure

```
HKEY_ROOT
├── HKEY_CLASSES_ROOT    # File associations
├── HKEY_CURRENT_USER    # User settings
├── HKEY_LOCAL_MACHINE   # System settings
├── HKEY_USERS           # All user profiles
└── HKEY_CURRENT_CONFIG  # Hardware config
```

### Value Types

| Type | Description |
|------|-------------|
| REG_STRING | Null-terminated string |
| REG_INT | 32-bit integer |
| REG_INT64 | 64-bit integer |
| REG_BINARY | Binary data |
| REG_ARRAY | Array of values |
| REG_PATH | File system path |

### Usage Example

```c
// Set a value
registry_set_string(reg, HKEY_CURRENT_USER, 
    "Software/NEXUS/Desktop", "Wallpaper", 
    "/usr/share/wallpapers/default.jpg");

// Get a value
char wallpaper[256];
registry_get_string(reg, HKEY_CURRENT_USER,
    "Software/NEXUS/Desktop", "Wallpaper",
    wallpaper, sizeof(wallpaper));
```

---

## Development

### Building

```bash
# Build kernel
make

# Build and run in QEMU
make run

# Build and auto-launch VMware
make run-vmware

# Debug mode
make run-debug
```

### Prerequisites

```bash
# Ubuntu/Debian/Kali
sudo apt-get install build-essential nasm \
    grub-pc-bin grub-common xorriso mtools \
    qemu-system-x86 vmware-workstation
```

### Directory Structure

```
NEXUS-OS/
├── kernel/          # Kernel core
├── hal/             # Hardware abstraction
├── platform/        # Platform support
├── net/             # Networking (IPv4, IPv6, firewall)
├── security/        # Security framework
├── gui/             # GUI and desktop
├── system/          # System services (registry)
├── tools/           # Build and utility tools
├── docs/            # Documentation
└── build/           # Build output
```

### Testing

```bash
# Unit tests
make test

# Integration tests
make test-integration

# Performance tests
make bench
```

---

## Quick Reference

### Boot Commands

| Command | Description |
|---------|-------------|
| `make run-vmware` | Auto-create VM and boot |
| `make run` | Run in QEMU |
| `make run-debug` | Debug mode |

### Default Settings

| Setting | Value |
|---------|-------|
| Virtualization Mode | VMware (auto-detected) |
| Memory | 2048 MB |
| CPUs | 2 |
| Theme | NEXUS Dark |
| Accent Color | #E94560 |

### File Locations

| Path | Description |
|------|-------------|
| `/boot/nexus-kernel.bin` | Kernel binary |
| `/boot/nexus-kernel.iso` | Bootable ISO |
| `~/vmware-vm/NEXUS-OS/` | VMware VM directory |
| `/etc/nexus/registry.db` | Registry database |

---

## Support

- **Documentation:** `docs/`
- **Quick Start:** `QUICKSTART.md`
- **Architecture:** `docs/architecture/overview.md`
- **API Reference:** `docs/api/kernel_api.md`
- **GitHub:** https://github.com/devTechs001/nexus-os

---

## License

NEXUS OS - Copyright (c) 2024 NEXUS Development Team

Proprietary with open-source components. Dual-licensing available.
