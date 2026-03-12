# Building NEXUS-OS

## Overview

This guide provides step-by-step instructions for building NEXUS-OS from source. The build system supports multiple target platforms and configurations.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Getting the Source](#getting-the-source)
3. [Build Configuration](#build-configuration)
4. [Building the Kernel](#building-the-kernel)
5. [Building Platform Images](#building-platform-images)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Tools

| Tool | Minimum Version | Purpose |
|------|-----------------|---------|
| GCC | 12.0 | C compiler |
| G++ | 12.0 | C++ compiler |
| NASM | 2.15 | Assembler |
| GNU Make | 4.3 | Build system |
| CMake | 3.20 | Build configuration |
| Python | 3.10 | Build scripts |
| Git | 2.30 | Version control |
| Grub | 2.06 | Bootloader |
| QEMU | 7.0 | Emulation |
| xorriso | 1.5 | ISO creation |

### Installing Dependencies

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    gcc \
    g++ \
    nasm \
    make \
    cmake \
    python3 \
    python3-pip \
    git \
    grub-common \
    grub-pc-bin \
    qemu-system-x86 \
    qemu-system-arm \
    xorriso \
    mtools \
    dosfstools \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    bison \
    flex \
    libssl-dev \
    libelf-dev
```

#### Fedora/RHEL

```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    nasm \
    make \
    cmake \
    python3 \
    python3-pip \
    git \
    grub2-tools \
    grub2-efi-x64-modules \
    qemu-system-x86 \
    qemu-system-arm \
    xorriso \
    mtools \
    dosfstools \
    gmp-devel \
    mpfr-devel \
    libmpc-devel \
    bison \
    flex \
    openssl-devel \
    elfutils-libelf-devel
```

#### macOS

```bash
brew install \
    gcc \
    nasm \
    make \
    cmake \
    python3 \
    git \
    grub \
    qemu \
    xorriso \
    mtools \
    dosfstools \
    gmp \
    mpfr \
    libmpc \
    bison \
    flex \
    openssl
```

### Cross-Compilation Toolchain

For building for different architectures:

```bash
# x86_64 (native on most systems)
# Usually no cross-compiler needed

# ARM64
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# RISC-V 64
sudo apt install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
```

Or build your own toolchain:

```bash
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
./bootstrap && ./configure --prefix=$HOME/ctng && make && make install

# Configure and build toolchain
ctng menuconfig  # Select target architecture
ctng build
```

---

## Getting the Source

### Clone the Repository

```bash
git clone https://github.com/darkhat/NEXUS-OS.git
cd NEXUS-OS
```

### Initialize Submodules

```bash
git submodule update --init --recursive
```

### Check Out Specific Version

```bash
# List available tags
git tag -l

# Check out a specific version
git checkout v1.0.0-alpha
```

---

## Build Configuration

### Configuration Files

NEXUS-OS uses configuration files to control the build:

| File | Purpose |
|------|---------|
| `config/kernel.config` | Kernel options |
| `config/mobile.config` | Mobile platform |
| `config/desktop.config` | Desktop platform |
| `config/server.config` | Server platform |
| `config/iot.config` | IoT platform |

### Using the Configuration Tool

```bash
# Interactive configuration
./tools/build/config.py menu

# Load a preset configuration
./tools/build/config.py load desktop

# Save current configuration
./tools/build/config.py save myconfig

# Show current configuration
./tools/build/config.py show
```

### Manual Configuration

Edit `config/kernel.config`:

```ini
# Architecture
CONFIG_ARCH=x86_64
CONFIG_ARCH_ARM64=n
CONFIG_ARCH_RISCV=n

# Platform
CONFIG_PLATFORM=desktop
CONFIG_PLATFORM_MOBILE=n
CONFIG_PLATFORM_SERVER=n
CONFIG_PLATFORM_IOT=n

# Kernel Features
CONFIG_SMP=y
CONFIG_PREEMPT=y
CONFIG_DEBUG_KERNEL=n
CONFIG_KASAN=n

# Virtualization
CONFIG_HYPERVISOR=y
CONFIG_VMX=y
CONFIG_SVM=y

# Drivers
CONFIG_DRIVER_GPU=y
CONFIG_DRIVER_NETWORK=y
CONFIG_DRIVER_STORAGE=y
CONFIG_DRIVER_USB=y

# Filesystems
CONFIG_FS_NEXFS=y
CONFIG_FS_FAT32=y
CONFIG_FS_EXT4=y
CONFIG_FS_NTFS=n

# Networking
CONFIG_NETWORK=y
CONFIG_IPV6=y
CONFIG_WIFI=y
CONFIG_BLUETOOTH=y
```

---

## Building the Kernel

### Quick Build

```bash
# Configure for desktop
./tools/build/config.py load desktop

# Build everything
make -j$(nproc)
```

### Incremental Build

```bash
# Build only the kernel
make kernel

# Build only a specific subsystem
make kernel/mm
make kernel/sched
make kernel/ipc
```

### Debug Build

```bash
# Enable debug configuration
./tools/build/config.py set CONFIG_DEBUG_KERNEL=y
./tools/build/config.py set CONFIG_KASAN=y

# Build with debug symbols
make DEBUG=1 -j$(nproc)
```

### Release Build

```bash
# Optimize for release
./tools/build/config.py set CONFIG_DEBUG_KERNEL=n
./tools/build/config.py set CONFIG_OPTIMIZE_SIZE=y

# Build release
make RELEASE=1 -j$(nproc)
```

### Cross-Compilation

```bash
# Build for ARM64
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)

# Build for RISC-V
make ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc)
```

### Build Output

```
build/
├── kernel/
│   ├── nexus.elf          # Kernel executable
│   ├── nexus.bin          # Raw kernel binary
│   └── System.map         # Symbol table
├── modules/
│   └── *.ko               # Kernel modules
├── images/
│   ├── nexus.iso          # Bootable ISO
│   ├── nexus.img          # Disk image
│   └── nexus-usb.img      # USB image
└── debug/
    ├── kernel.sym         # Debug symbols
    └── *.dmp              # Memory dumps
```

---

## Building Platform Images

### Desktop Image

```bash
# Configure for desktop
./tools/build/config.py load desktop

# Build desktop ISO
make image-desktop

# Output: build/images/nexus-desktop.iso
```

### Server Image

```bash
# Configure for server
./tools/build/config.py load server

# Build server ISO (headless)
make image-server

# Output: build/images/nexus-server.iso
```

### Mobile Image

```bash
# Configure for mobile
./tools/build/config.py load mobile

# Build mobile image
make image-mobile

# Output: build/images/nexus-mobile.img
```

### IoT Image

```bash
# Configure for IoT
./tools/build/config.py load iot

# Build minimal IoT image
make image-iot

# Output: build/images/nexus-iot.bin
```

### Custom Image

```bash
# Create custom disk image
./tools/build/create_image.py \
    --size 8G \
    --format qcow2 \
    --output nexus-custom.img \
    --base build/images/nexus-desktop.iso

# Install to image
./tools/build/install.py \
    --image nexus-custom.img \
    --kernel build/kernel/nexus.elf \
    --modules build/modules/
```

---

## Testing

### QEMU Testing

#### Desktop (x86_64)

```bash
# Basic QEMU boot
make qemu

# With GUI
make qemu-gui

# With specific memory
make qemu MEM=4096

# With KVM acceleration
make qemu-kvm

# With debug console
make qemu-debug
```

#### Manual QEMU

```bash
# x86_64 Desktop
qemu-system-x86_64 \
    -kernel build/kernel/nexus.elf \
    -m 2048 \
    -cpu qemu64 \
    -serial stdio \
    -display gtk

# With disk image
qemu-system-x86_64 \
    -kernel build/kernel/nexus.elf \
    -hda build/images/nexus.img \
    -m 4096 \
    -cpu host \
    -enable-kvm \
    -serial stdio \
    -display gtk

# UEFI boot
qemu-system-x86_64 \
    -drive if=pflash,format=raw,file=OVMF.fd \
    -kernel build/kernel/nexus.elf \
    -m 4096 \
    -cpu qemu64 \
    -serial stdio
```

#### ARM64

```bash
qemu-system-aarch64 \
    -kernel build/kernel/nexus.elf \
    -m 2048 \
    -cpu cortex-a72 \
    -machine virt \
    -serial stdio
```

#### RISC-V

```bash
qemu-system-riscv64 \
    -kernel build/kernel/nexus.elf \
    -m 2048 \
    -cpu rv64 \
    -machine virt \
    -serial stdio
```

### Unit Testing

```bash
# Run all unit tests
make test

# Run specific test suite
make test-kernel
make test-drivers
make test-filesystem

# Run with coverage
make test-coverage
```

### Integration Testing

```bash
# Run integration tests
make test-integration

# Run stress tests
make test-stress

# Run performance benchmarks
make test-benchmark
```

### Debugging

#### GDB Setup

```bash
# Start QEMU with GDB stub
make qemu-gdb

# In another terminal
gdb build/kernel/nexus.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

#### Kernel Debugging

```bash
# Enable kernel debug output
./tools/build/config.py set CONFIG_DEBUG_KERNEL=y
./tools/build/config.py set CONFIG_PRINTK_TIME=y
./tools/build/config.py set CONFIG_DEBUG_SPINLOCK=y

# Build and run with debug console
make DEBUG=1
make qemu-debug
```

#### Memory Debugging

```bash
# Enable KASAN (memory sanitizer)
./tools/build/config.py set CONFIG_KASAN=y

# Enable KMSAN (uninitialized memory)
./tools/build/config.py set CONFIG_KMSAN=y

# Build and test
make DEBUG=1
make test-memory
```

---

## Troubleshooting

### Common Build Errors

#### "gcc: error: unrecognized command-line option"

**Solution**: Ensure you're using GCC 12 or newer:
```bash
gcc --version
sudo apt install gcc-12 g++-12
```

#### "nasm: command not found"

**Solution**: Install NASM:
```bash
sudo apt install nasm
```

#### "grub-mkrescue: command not found"

**Solution**: Install GRUB tools:
```bash
sudo apt install grub-pc-bin grub-efi-amd64-bin
```

#### "undefined reference to"

**Solution**: Clean and rebuild:
```bash
make clean
make -j$(nproc)
```

### Runtime Issues

#### "Boot failed: not a bootable disk"

**Solution**: Ensure the image was created correctly:
```bash
make clean-image
make image-desktop
```

#### "Kernel panic - not syncing: VFS: Unable to mount root fs"

**Solution**: Check that the root filesystem is included:
```bash
./tools/build/config.py set CONFIG_ROOT_FS=y
make
```

#### "No bootable device"

**Solution**: Verify QEMU command:
```bash
qemu-system-x86_64 \
    -kernel build/kernel/nexus.elf \
    -m 2048 \
    -serial stdio
```

### Performance Issues

#### Slow emulation

**Solution**: Enable KVM:
```bash
# Check KVM support
ls -la /dev/kvm

# Add user to kvm group
sudo usermod -aG kvm $USER

# Run with KVM
make qemu-kvm
```

#### High memory usage

**Solution**: Reduce QEMU memory:
```bash
make qemu MEM=1024
```

### Getting Help

- **Documentation**: See `/docs/` directory
- **Issues**: https://github.com/darkhat/NEXUS-OS/issues
- **Discussions**: https://github.com/darkhat/NEXUS-OS/discussions
- **IRC**: #nexus-os on Libera.Chat

---

*Last Updated: March 2026*
*NEXUS-OS Version 1.0.0-alpha*
