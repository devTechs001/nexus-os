# NEXUS OS - Quick Start Guide

## 🚀 One-Command Start (VMware - DEFAULT)

```bash
cd /path/to/NEXUS-OS && make && make run-vmware
```

This automatically:
1. ✅ Builds the bootable kernel ISO
2. ✅ Creates a VMware VM (first run only)
3. ✅ Launches VMware Workstation/Player
4. ✅ Boots NEXUS OS with VMware Mode optimizations

---

## Prerequisites

### Required Packages

```bash
# Kali/Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential nasm grub-pc-bin grub-common xorriso mtools qemu-system-x86

# Optional: VMware for best experience
# Download from: https://www.vmware.com/products/workstation-player.html
```

### Optional: VMware Installation

VMware provides the best experience with NEXUS OS:

- **VMware Workstation Pro** (free for personal use)
- **VMware Player** (free)

Download: https://www.vmware.com/products/workstation-player.html

---

## Build Options

### 1. Build Only

```bash
make
```

Output:
- `build/nexus-kernel.bin` - Raw kernel binary
- `build/nexus-kernel.iso` - Bootable ISO (15MB)

### 2. Run in VMware (RECOMMENDED)

```bash
make run-vmware
```

**What happens:**
- Creates VM at `~/vmware-vm/NEXUS-OS/`
- Configures 2GB RAM, 2 CPUs
- Attaches NEXUS OS ISO
- Launches VMware automatically
- Boots with VMware Mode (default)

**Subsequent runs** reuse the existing VM.

### 3. Run in QEMU

```bash
make run
```

QEMU is good for quick testing but VMware provides better performance.

### 4. Debug Mode

```bash
make run-debug
```

Shows detailed boot logs and QEMU debug output.

---

## Boot Menu

When NEXUS OS boots, you'll see:

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

**Press Enter** for default (VMware Mode).

---

## What You'll See

```
  ____   ___   __  __   _   _  _____ 
 |  _ \ / _ \ |  \/  | | \ | || ____|
 | |_) | | | || |\/| | |  \| ||  _|  
 |  _ <| |_| || |  | | | |\  || |___ 
 |_| \_\\___/ |_|  |_| |_| \_||_____|

  NEXUS OS v1.0.0 (Genesis)
  ==============================================

[VM] VMware environment detected - using VMware optimizations
[OK] Early initialization complete
[    ] Initializing GDT...
[OK]  GDT initialized
...
```

---

## VMware Auto-Creation Details

### First Run

The `make run-vmware` command creates:

```
~/vmware-vm/NEXUS-OS/
├── NEXUS-OS.vmx      # VM configuration
├── NEXUS-OS.vmdk     # Virtual disk (20GB)
├── NEXUS-OS.nvram    # BIOS/UEFI settings
└── boot.log          # Boot logs
```

### VM Configuration

- **Memory**: 2048 MB
- **CPUs**: 2 (2 cores per socket)
- **Network**: NAT with vmxnet3 adapter
- **Storage**: 20GB virtual disk
- **Boot**: CD-ROM (NEXUS OS ISO)

### Subsequent Runs

- VM is reused
- ISO is reattached (for latest kernel)
- Previous state is preserved

---

## Virtualization Modes

NEXUS OS auto-detects and optimizes for:

| Mode | Description | Default |
|------|-------------|---------|
| **VMware** | VMware Workstation/Player/ESXi | ✅ YES |
| VirtualBox | Oracle VM VirtualBox | |
| QEMU/KVM | QEMU with KVM acceleration | |
| Hyper-V | Microsoft Hyper-V | |
| Native | Bare metal (no virtualization) | |

Detection happens automatically at boot via CPUID.

---

## Troubleshooting

### "VMware not found"

Install VMware:
```bash
# Download and install from vmware.com
# Or use package manager (if available)
sudo apt-get install vmware-workstation
```

### "ISO not found"

Rebuild:
```bash
make clean
make
make run-vmware
```

### VM doesn't boot

1. Check VM settings > CD/DVD
2. Ensure ISO is connected
3. Set boot order to CD-ROM first

### Clean boot (reset VM state)

```bash
./tools/auto-vm-boot.sh --clean
```

### Rebuild VM from scratch

```bash
rm -rf ~/vmware-vm/NEXUS-OS
make run-vmware
```

---

## Command Reference

| Command | Description |
|---------|-------------|
| `make` | Build kernel and ISO |
| `make run` | Run in VMware (default) |
| `make run-vmware` | Same as make run (auto-create and launch VMware) |
| `make run-qemu` | Run in QEMU (use when VMware not available) |
| `make run-debug` | Run in QEMU with debug output |
| `make clean` | Remove build files |
| `./tools/auto-vm-boot.sh --clean` | Clean VM boot |
| `./tools/auto-vm-boot.sh --recreate` | Recreate VM config |

---

## Next Steps

After booting NEXUS OS:

1. **Explore the boot logs** - See VMware detection and initialization
2. **Try different boot modes** - Safe Mode, Debug Mode, Native Mode
3. **Check the GUI Boot Manager** - `cd gui/boot-manager && make`
4. **Read the documentation** - `docs/architecture/overview.md`

---

## System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | x86_64 | x86_64 with VT-x/AMD-V |
| RAM | 512 MB | 2 GB+ |
| Storage | 100 MB | 20 GB+ |
| VMware | Player 12+ | Workstation 16+ |

---

## License

NEXUS OS - Copyright (c) 2024 NEXUS Development Team
