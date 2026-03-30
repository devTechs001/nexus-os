# NEXUS-OS Virtualization Guide

## Overview

NEXUS-OS features a built-in hypervisor (NHV - NEXUS Hypervisor) that provides multiple virtualization modes selectable at boot time. This allows users to choose the optimal balance between performance, security, and compatibility for their specific use case.

## Virtualization Modes

### 1. Native Mode
**Best for**: Maximum performance, dedicated workstations, gaming

- No virtualization overhead
- Direct hardware access
- Lowest latency
- No application isolation

```bash
# Boot parameter
nexus.virt_mode=native
```

### 2. Light Virtualization (Default)
**Best for**: Desktop users, mobile devices, general purpose

- Process-level isolation using micro-VMs
- Application sandboxing
- Minimal overhead (< 1%)
- Good security/performance balance

```bash
# Boot parameter (or default)
nexus.virt_mode=light
```

### 3. Full Virtualization
**Best for**: Enterprise, servers, development environments

- Complete hardware virtualization
- Multiple isolated environments
- Run different OS instances
- Hardware-assisted (VT-x/AMD-V)

```bash
# Boot parameter
nexus.virt_mode=full
```

### 4. Container Mode
**Best for**: Development, deployment, microservices

- Lightweight container isolation
- Fast application startup
- Shared kernel
- Docker/Podman compatible

```bash
# Boot parameter
nexus.virt_mode=container
```

### 5. Secure Enclave Mode
**Best for**: Sensitive workloads, financial, healthcare

- Hardware-backed security (SGX, TrustZone, SEV)
- Memory encryption
- Remote attestation
- Highest isolation level

```bash
# Boot parameter
nexus.virt_mode=enclave
```

### 6. Compatibility Mode
**Best for**: Legacy applications, migration

- Run Windows/Linux/macOS applications
- Binary translation
- API compatibility layer
- Some performance overhead

```bash
# Boot parameter
nexus.virt_mode=compat
```

### 7. Custom Mode
**Best for**: Advanced users, specific requirements

- Mix and match virtualization types
- Per-application settings
- Fine-grained control
- Configuration file based

```bash
# Boot parameter
nexus.virt_mode=custom
nexus.virt_config=/etc/nexus/virt.conf
```

## Boot Configuration

### Interactive Boot Menu

At boot time, you'll see:

```
╔══════════════════════════════════════════════════════════╗
║           NEXUS-OS Boot Configuration                    ║
╠══════════════════════════════════════════════════════════╣
║  Select Virtualization Mode:                             ║
║                                                          ║
║  [1] Native Mode (No Virtualization)                     ║
║  [2] Light Virtualization (Default)                      ║
║  [3] Full Virtualization                                 ║
║  [4] Container Mode                                      ║
║  [5] Secure Enclave Mode                                 ║
║  [6] Compatibility Mode                                  ║
║  [7] Custom Configuration                                ║
║  [0] Boot with Defaults                                  ║
╚══════════════════════════════════════════════════════════╝
```

### Configuration File

`/etc/nexus/boot.conf`:

```ini
# Virtualization Mode
virt_mode=light

# Hardware Settings
num_cpus=0          # 0 = all available
memory_size=0       # 0 = all available
enable_gpu=true
enable_network=true

# Security Settings
secure_boot=true
enable_encryption=true
security_profile=balanced

# Virtualization Settings
nested_virt=false
hardware_assist=true
iommu_enabled=true
max_vms=64
```

## VM Management

### Creating VMs

```cpp
#include <nexus/hypervisor.h>

using namespace NexusOS;

// Get hypervisor instance
auto& hypervisor = NEXUSHypervisor::instance();

// Initialize with selected mode
hypervisor.initialize(VirtMode::LIGHT);

// Create VM configuration
VMConfig config;
config.name = "MyVM";
config.uuid = "vm-123456";
config.type = VMType::PROCESS_VM;
config.cpu.numVCPU = 4;
config.cpu.memorySize = 4096; // MB

// Create and start VM
auto vm = hypervisor.createVM(config);
vm->addDevice(VDeviceConfig{"network", {{"mac", "00:11:22:33:44:55"}}});
vm->addDevice(VDeviceConfig{"storage", {{"file", "/vm/disk.img"}, {"size", "64"}}});
vm->start();
```

### Command Line Tools

```bash
# List all VMs
nexus-vm list

# Create VM
nexus-vm create --name myvm --cpu 4 --memory 4096

# Start VM
nexus-vm start myvm

# Stop VM
nexus-vm stop myvm

# VM console
nexus-vm console myvm

# VM statistics
nexus-vm stats myvm

# Snapshot
nexus-vm snapshot create myvm backup1
nexus-vm snapshot restore myvm backup1

# Live migration
nexus-vm migrate myvm target-host
```

## VMware Integration

NEXUS OS provides seamless integration when running in VMware environments.

### Virtualization Manager GUI

The NEXUS OS Virtualization Manager includes a comprehensive GUI with menu bars and toolbars:

#### Menu Bar

| Menu | Description | Actions |
|------|-------------|---------|
| **File** | VM file operations | New VM, Open, Save, Export, Import, Exit |
| **Edit** | Edit operations | Cut, Copy, Paste, Preferences |
| **View** | Display options | Refresh, Zoom, Fullscreen |
| **VM** | VM lifecycle | Start, Stop, Pause, Restart, Clone |
| **Container** | Container ops | Create, Start, Stop, Logs, Shell |
| **Hardware** | HW config | Add/Remove, GPU, Network, Storage |
| **VMware** | VMware features | Settings, Tools, Snapshots, Migration |
| **Tools** | System tools | Performance, Monitoring, Backup |
| **Help** | Documentation | Documentation, About |

#### Toolbar

| Button | Function |
|--------|----------|
| + | New VM |
| > | Start |
| || | Pause |
| [] | Stop |
| R | Restart |
| S | Snapshot |
| C | Clone |
| M | Migrate |
| X | Delete |
| E | Export |
| I | Import |
| H | VMware Settings |
| G | GPU Passthrough |

#### Sidebar Categories

- All VMs
- Running
- Paused
- Stopped
- Templates
- Snapshots
- Networks
- Storage
- **Containers**
- **VMware**
- **Console**
- **Migration**
- **GPU Passthrough**
- **Export/Import**

### VMware Console

NEXUS OS provides a built-in console for VM management:

```bash
# Access VM console
nexus-vm console myvm

# Console features:
# - Full screen mode
# - Copy/paste support
# - Resolution adjustment
# - Remote display (VNC)
```

### VM Migration

Live migration support for moving VMs between hosts:

```bash
# Live migration
nexus-vm migrate myvm target-host

# Pre-migration checks:
# - Network connectivity
# - Storage access
# - Memory transfer
# - CPU state
```

### GPU Passthrough

Configure GPU passthrough for graphics-intensive workloads:

```c
// Configure GPU passthrough
virt_manager_configure_gpu_passthrough(vm_id, gpu_id, true);

// Features:
// - Direct GPU access
// - NVIDIA/AMD support
// - Multiple GPUs
// - Performance monitoring
```

### Export/Import

Export and import VMs in standard formats:

```bash
# Export to OVA
nexus-vm export myvm /path/to/vm.ova

# Import from OVA/OVF
nexus-vm import /path/to/vm.ova

# Formats supported:
// - OVA (Open Virtual Appliance)
// - OVF (Open Virtualization Format)
// - VMDK (VMware)
// - QCOW2 (QEMU)
```

### Automatic Detection

When NEXUS OS boots in VMware, it automatically detects the environment and enables optimized settings:

```
[VM] VMware environment detected - using VMware optimizations
[VMWARE] Version: 17, Tools: Present
[VMWARE] Enabling optimized graphics and performance...
```

### VMware-Specific Features

| Feature | Boot Parameter | Description |
|---------|---------------|-------------|
| 3D Acceleration | `vmware.3d=true` | Hardware-accelerated graphics |
| High Resolution | `vmware.resolution=1920x1080` | Display resolution |
| Shared Folders | `vmware.hgfs=true` | Host-guest file sharing |
| Copy/Paste | `vmware.clipboard=true` | Clipboard integration |
| Time Sync | `vmware.timesync=true` | Automatic time sync |

### VMware Boot Configuration

Edit `tools/auto-vm-boot.sh` or VMX file for enhanced VMware performance:

```bash
# Enhanced graphics
svga.vramSize = "268435456"  # 256MB VRAM
mks.enable3d = "TRUE"

# Hardware virtualization
vhv.enable = "TRUE"
monitor.virtual_exec = "hardware"
monitor.virtual_mmu = "hardware"

# Network performance
ethernet0.virtualDev = "vmxnet3"

# USB 3.0 support
usb.xhci.present = "TRUE"
```

### Running in VMware

```bash
# Build and run in VMware mode
make && make run-vmware

# Or manually with auto-vm-boot.sh
./tools/auto-vm-boot.sh
```

### VMware Tools Integration

NEXUS OS detects VMware Tools and enables:

- **Time Synchronization**: Guest clock synced with host
- **Drag & Drop**: File transfer between host and guest
- **Copy & Paste**: Clipboard sharing
- **Shared Folders**: Access host filesystem from guest
- **Optimized Graphics**: Better display performance

### VMware Display Modes

NEXUS OS supports multiple display configurations in VMware:

| Resolution | Color Depth | 3D Accel | Use Case |
|------------|-------------|-----------|----------|
| 1024x768 | 24-bit | No | Basic |
| 1280x800 | 32-bit | Yes | Standard |
| 1920x1080 | 32-bit | Yes | Full HD |
| 2560x1600 | 32-bit | Yes | High DPI |

## Performance Comparison

| Mode | Overhead | Boot Time | Isolation | Use Case |
|------|----------|-----------|-----------|----------|
| Native | 0% | Fast | None | Gaming, HPC |
| Light | <1% | Fast | Process | Desktop |
| Full | 2-5% | Medium | Full | Server |
| Container | <1% | Instant | Namespace | Dev |
| Enclave | 5-10% | Slow | Maximum | Security |
| Compat | 10-30% | Slow | Medium | Legacy |

## Security Features

### Mandatory Access Control
- Per-process capabilities
- Resource quotas
- Network policies

### Memory Protection
- ASLR (Address Space Layout Randomization)
- DEP (Data Execution Prevention)
- Memory encryption (enclave mode)

### Device Isolation
- IOMMU support
- Device passthrough
- Virtual device emulation

## Nested Virtualization

NEXUS-OS supports running VMs inside VMs:

```bash
# Enable nested virtualization
nexus.virt.nested=true

# Check support
nexus-vm check-nested
```

## Resource Management

### CPU Pinning

```bash
# Pin VM to specific CPUs
nexus-vm pin myvm --cpus 0,1,2,3
```

### Memory Ballooning

```bash
# Adjust VM memory dynamically
nexus-vm memory myvm 8192
```

### Network QoS

```bash
# Set network bandwidth limits
nexus-vm network myvm --bandwidth 1Gbps
```

## Troubleshooting

### Check Virtualization Support

```bash
# Check hardware support
nexus-vm check-support

# View hypervisor logs
journalctl -u nexus-hypervisor
```

### Common Issues

1. **VM fails to start**
   - Check hardware virtualization support in BIOS
   - Verify sufficient resources
   - Check hypervisor logs

2. **Poor performance**
   - Consider Native or Light mode
   - Enable hardware assistance
   - Check CPU pinning

3. **Network issues**
   - Verify virtual network configuration
   - Check firewall rules
   - Test with different network mode

## API Reference

See `hypervisor/nhv-core/nexus-hypervisor.h` for complete API documentation.

## License

Proprietary (NEXUS-OS Core)
