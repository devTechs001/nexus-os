# NEXUS OS - Quick Start Guide

## Build and Run Immediately

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential nasm grub-pc-bin grub-common qemu-system-x86 xorriso

# Fedora/RHEL
sudo dnf install -y gcc nasm grub2-tools qemu-system-x86 xorriso

# Arch Linux
sudo pacman -S base-devel nasm grub qemu-system-x86 xorriso
```

### Build and Run

```bash
# Navigate to NEXUS-OS directory
cd /path/to/NEXUS-OS

# Build the kernel
make

# Run in QEMU (recommended for testing)
make run

# Or run manually
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2

# Run with serial debug output
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -smp 2 -serial stdio
```

### Run in VMware

1. Create a new virtual machine
2. Select "I will install the operating system later"
3. Choose Linux -> Other Linux 64-bit
4. Create/use a new ISO and select `build/nexus-kernel.iso`
5. Finish VM creation
6. Start the VM

## What You'll See

When NEXUS OS boots, you'll see:

```
  ____   ___   __  __   _   _  _____ 
 |  _ \ / _ \ |  \/  | | \ | || ____|
 | |_) | | | || |\/| | |  \| ||  _|  
 |  _ <| |_| || |  | | | |\  || |___ 
 |_| \_\\___/ |_|  |_| |_| \_||_____|

  Version: 1.0.0 (Genesis)
  Built: [Date] [Time]
  ==============================================

[VM] VMware environment detected - using VMware optimizations
[OK] Early initialization complete
[    ] Initializing GDT...
[OK]  GDT initialized
[    ] Initializing IDT...
[OK]  IDT initialized
...
```

## Boot Menu

The ISO includes a GRUB boot menu with options:

1. **NEXUS OS** - Normal boot (VMware mode by default)
2. **NEXUS OS (Safe Mode)** - Minimal drivers, no SMP
3. **NEXUS OS (Debug Mode)** - Verbose logging

## Virtualization Modes

NEXUS OS automatically detects and optimizes for:

- **VMware** (DEFAULT) - Workstation, Player, ESXi
- **VirtualBox** - Oracle VM VirtualBox  
- **QEMU** - QEMU/KVM
- **Hyper-V** - Microsoft Hyper-V
- **Native** - Bare metal (no virtualization)

## Troubleshooting

### Build fails with "grub-mkrescue not found"
Install GRUB:
```bash
sudo apt-get install grub-pc-bin grub-common
```

### QEMU fails to start
Try with different display options:
```bash
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -display gtk
```

### No output in QEMU
Add serial console:
```bash
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -serial stdio
```

## Next Steps

After confirming the kernel boots:

1. Explore the GUI Boot Manager:
   ```bash
   cd gui/boot-manager
   mkdir build && cd build
   cmake ..
   make
   ./nexus-boot-manager
   ```

2. Check the documentation:
   - `docs/architecture/overview.md` - System architecture
   - `docs/guides/building.md` - Detailed build instructions
   - `docs/api/kernel_api.md` - Kernel API reference

3. Customize boot configuration:
   - Edit `build/iso/boot/grub/grub.cfg`
   - Or use the GUI Boot Manager

## System Requirements

- **CPU**: x86_64 processor (Intel or AMD)
- **RAM**: 512MB minimum, 2GB recommended
- **Storage**: 100MB for build, 1GB+ recommended
- **Virtualization**: Optional (runs on bare metal or in VM)

## License

NEXUS OS - Copyright (c) 2024 NEXUS Development Team
