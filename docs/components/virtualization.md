# NEXUS OS - Virtualization System Documentation

## Overview

NEXUS OS includes a comprehensive virtualization system with hypervisor support, VM management, nested virtualization, and container orchestration.

## Components

### 1. Hypervisor Core (`virt/hypervisor/`)

**File:** `nexus_hypervisor_core.c`

Features:
- Support for 64 VMs
- Up to 128 VCPUs per VM
- EPT (Extended Page Tables)
- VPID (Virtual Processor IDs)
- VMCS management
- Device virtualization
- PCI passthrough
- Live migration support
- Snapshot support
- Nested virtualization (3 levels)

### 2. VM Manager (`virt/vm/`)

**Files:**
- `vm_manager_enhanced.c` - Enhanced VM management
- `vm_manager.h` - Header file

Features:
- Detailed VM statistics (CPU, Memory, Storage, Network, GPU)
- VM modes (Normal, Idle, Performance, High Priority, Background)
- Extension support (32 slots)
- Health monitoring
- Resource allocation

### 3. Nested Virtualization (`virt/emulation/`)

**File:** `nested_virt.c`

Features:
- Intel VMX nested virtualization
- AMD SVM nested virtualization
- Nested EPT support
- Nested VPID support
- INVEPT/INVVPID support
- VMCS shadowing

### 4. Container Orchestration (`virt/containers/`)

**Files:**
- `container_runtime.c` - Basic container runtime
- `container_orchestration.c` - Advanced orchestration

Features:
- 256 services max
- 64 stacks max
- 256 networks max
- 128 volumes max
- Health checks
- Rolling updates
- Service scaling (1-100 replicas)

## Usage

### VM Management

```c
// Create VM
vm_config_t config;
config.num_vcpus = 4;
config.memory_size_mb = 4096;
virtual_machine_t *vm = vm_create(&config);

// Start VM
vm_start(vm);

// Get statistics
vm_detailed_stats_t stats;
vm_get_stats(vm, &stats);
```

### Container Management

```c
// Create service
container_service_t *svc = container_service_create("web", "nginx:latest", 3);

// Scale service
container_service_scale("web", 5);

// Update service
container_service_update("web", "nginx:1.21");
```

## VM Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| **Normal** | Standard operation | General use |
| **Idle** | CPU throttling, reduced priority | Background VMs |
| **Power Save** | Reduced frequency | Energy efficiency |
| **Performance** | Max frequency, high I/O priority | Production workloads |
| **High Priority** | CPU affinity set | Critical services |
| **Background** | Resource limits | Low-priority tasks |

## Extensions

Default extensions include:
- VMware Tools (v12.0.0)
- VirtualBox GA (v7.0.0)
- QEMU Guest Agent (v8.0.0)
- Shared Folders
- Drag & Drop
- Clipboard Share
- Time Sync
- Auto Resize

## References

- [VMware Documentation](https://docs.vmware.com/)
- [KVM Documentation](https://www.linux-kvm.org/page/Documentation)
- [Docker Documentation](https://docs.docker.com/)
