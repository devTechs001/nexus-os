# NEXUS-OS Virtualization Architecture

## Overview

The NEXUS Hypervisor (NHV) is a Type-1 hypervisor integrated into the NEXUS-OS kernel. It provides hardware-assisted virtualization with support for multiple VM types, nested virtualization, and live migration capabilities.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         NEXUS Hypervisor Stack                          │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    VM Manager (VMM)                              │   │
│  │  VM Lifecycle │ Resource Allocation │ Migration │ Snapshots     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Virtual Device Framework                      │   │
│  │  VirtIO │ PCI Passthrough │ Emulated Devices │ SR-IOV          │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Memory Virtualization                         │   │
│  │  EPT/NPT │ Shadow Page Tables │ Memory Ballooning │ Sharing    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    CPU Virtualization                            │   │
│  │  VMX/SVM │ ARM Virtualization │ VCPU Scheduling │ Nested VMX   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Hardware Abstraction                          │   │
│  │  Intel VT-x │ AMD-V │ ARM VE │ RISC-V H-Extension              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

## VM Types

NEXUS-OS supports multiple virtualization modes selectable at boot or runtime:

### 1. Hardware VM (HVM)

Full hardware virtualization with complete isolation:

```
┌─────────────────────────────────────────┐
│            Guest OS (Unmodified)        │
│  ┌─────────────────────────────────┐    │
│  │        Guest Applications       │    │
│  ├─────────────────────────────────┤    │
│  │        Guest Kernel             │    │
│  ├─────────────────────────────────┤    │
│  │        Guest Drivers            │    │
│  └─────────────────────────────────┘    │
│         Virtual Hardware                │
│  ┌─────────────────────────────────┐    │
│  │  vCPU  │  vRAM  │  vNIC  │ vDisk│   │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│         NEXUS Hypervisor                │
└─────────────────────────────────────────┘
```

**Use Cases**: Running unmodified operating systems, legacy application support

### 2. Process VM (PVM)

Lightweight VM for process isolation:

```
┌─────────────────────────────────────────┐
│         Process VM 1 │ Process VM 2     │
│  ┌─────────────┐     ┌─────────────┐    │
│  │  Process A  │     │  Process B  │    │
│  │  (isolated) │     │  (isolated) │    │
│  └─────────────┘     └─────────────┘    │
│         Minimal Runtime                  │
├─────────────────────────────────────────┤
│         NEXUS Hypervisor                │
└─────────────────────────────────────────┘
```

**Use Cases**: Application sandboxing, security isolation, containers with VM-level security

### 3. Container VM (CVM)

OCI-compatible container runtime with VM isolation:

```
┌─────────────────────────────────────────┐
│  Container │ Container │ Container      │
│  ┌──────┐  ┌──────┐  ┌──────┐          │
│  │ App  │  │ App  │  │ App  │          │
│  │ + Lib│  │ + Lib│  │ + Lib│          │
│  └──────┘  └──────┘  └──────┘          │
│         Container Runtime               │
├─────────────────────────────────────────┤
│         NEXUS Hypervisor                │
└─────────────────────────────────────────┘
```

**Use Cases**: Cloud-native applications, microservices, DevOps

### 4. Security Enclave

Hardware-backed isolated execution environment:

```
┌─────────────────────────────────────────┐
│         Security Enclave                │
│  ┌─────────────────────────────────┐    │
│  │      Encrypted Memory           │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │   Trusted Application   │    │    │
│  │  │   (Attested)            │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
│         Hardware Security (TPM/SGX)     │
├─────────────────────────────────────────┤
│         NEXUS Hypervisor                │
└─────────────────────────────────────────┘
```

**Use Cases**: DRM, cryptographic operations, sensitive data processing

## CPU Virtualization

### Intel VT-x Architecture

```
┌─────────────────────────────────────────┐
│            Root Operation               │
│         (Hypervisor - VMM)              │
│  ┌─────────────────────────────────┐    │
│  │        VMCS Management          │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│            Non-Root Operation           │
│         (Guest OS)                      │
│  ┌─────────────────────────────────┐    │
│  │        Guest Execution          │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │   Guest Applications    │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘

VMCS (Virtual Machine Control Structure):
┌─────────────────────────────────────────┐
│  Guest-State Area                       │
│  Host-State Area                        │
│  VM-Execution Control Fields            │
│  VM-Exit Control Fields                 │
│  VM-Entry Control Fields                │
│  VM-Exit Information Fields             │
└─────────────────────────────────────────┘
```

### AMD-V Architecture

```
┌─────────────────────────────────────────┐
│            Root Mode                    │
│         (Hypervisor - VMM)              │
│  ┌─────────────────────────────────┐    │
│  │        VMCB Management          │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│            Guest Mode                   │
│         (Guest OS)                      │
│  ┌─────────────────────────────────┐    │
│  │        Guest Execution          │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘

VMCB (Virtual Machine Control Block):
┌─────────────────────────────────────────┐
│  Guest State Save Area                  │
│  Control Area                           │
└─────────────────────────────────────────┘
```

### ARM Virtualization Extensions

```
┌─────────────────────────────────────────┐
│            EL2 (Hypervisor)             │
│         (NEXUS Hypervisor)              │
├─────────────────────────────────────────┤
│            EL1/EL0 (Guest)              │
│         (Guest OS / Applications)       │
└─────────────────────────────────────────┘

Exception Levels:
EL3 - Secure Monitor
EL2 - Hypervisor
EL1 - Kernel
EL0 - User
```

## Memory Virtualization

### Extended Page Tables (EPT) / Nested Page Tables (NPT)

```
Guest Virtual Address (GVA)
         │
         ▼
    ┌─────────┐
    │  GPT    │  Guest Page Table
    └────┬────┘
         │
         ▼
Guest Physical Address (GPA)
         │
         ▼
    ┌─────────┐
    │  EPT    │  Extended Page Table
    └────┬────┘
         │
         ▼
Host Physical Address (HPA)
```

### Memory Management Features

| Feature | Description |
|---------|-------------|
| EPT/NPT | Hardware-assisted nested paging |
| Large Pages | 2MB and 1GB page support |
| Memory Ballooning | Dynamic memory adjustment |
| Page Sharing | Transparent page sharing |
| NUMA Awareness | NUMA-aware memory allocation |
| IOMMU | DMA remapping for device passthrough |

### Memory Ballooning

```
┌─────────────────────────────────────────┐
│            Guest Memory                 │
│  ┌─────────────────────────────────┐    │
│  │     Used Memory                 │    │
│  ├─────────────────────────────────┤    │
│  │     Balloon Driver              │◄───┼── Hypervisor control
│  │     (Reclaimable)               │    │
│  ├─────────────────────────────────┤    │
│  │     Free Memory                 │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

## I/O Virtualization

### VirtIO Framework

```
┌─────────────────────────────────────────┐
│            Guest OS                     │
│  ┌─────────────────────────────────┐    │
│  │        Guest Application        │    │
│  ├─────────────────────────────────┤    │
│  │        VirtIO Driver            │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │   Virtqueue (Ring)      │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│            Hypervisor                   │
│  ┌─────────────────────────────────┐    │
│  │        VirtIO Backend           │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │   Physical Device       │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

### Supported VirtIO Devices

- **virtio-net**: Network interface
- **virtio-blk**: Block storage
- **virtio-scsi**: SCSI storage
- **virtio-fs**: Filesystem sharing
- **virtio-gpu**: Graphics acceleration
- **virtio-input**: Input devices
- **virtio-console**: Console/serial
- **virtio-rng**: Random number generator

### PCI Passthrough

```
┌─────────────────────────────────────────┐
│            Guest OS                     │
│  ┌─────────────────────────────────┐    │
│  │        Native Driver            │    │
│  └─────────────────────────────────┘    │
│              │                          │
│         PCI Configuration               │
├─────────────────────────────────────────┤
│            Hypervisor                   │
│  ┌─────────────────────────────────┐    │
│  │        IOMMU Mapping            │    │
│  │        (VT-d / AMD-Vi)          │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│            Physical Hardware            │
│  ┌─────────────────────────────────┐    │
│  │        PCI Device               │    │
│  │        (GPU / NIC / etc)        │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

## Nested Virtualization

### Architecture

```
┌─────────────────────────────────────────┐
│            L1 Guest (Hypervisor)        │
│  ┌─────────────────────────────────┐    │
│  │        L2 Guest OS              │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │   L2 Applications       │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤
│            L0 Host (NEXUS Hypervisor)   │
└─────────────────────────────────────────┘
```

### Use Cases

- Cloud providers offering VM-based services
- Development and testing of hypervisors
- Security research and malware analysis
- Running Android emulators inside VMs

## VM Lifecycle Management

### VM States

```
                    ┌──────────┐
                    │  NEW     │
                    └────┬─────┘
                         │ create
                         ▼
                    ┌──────────┐
             ┌──────│ STARTING │──────┐
             │      └────┬─────┘      │
             │           │             │
        restart      initialized      destroy
             │           │             │
             │           ▼             │
             │      ┌──────────┐       │
             │      │ RUNNING  │───────┤
             │      └────┬─────┘       │
             │           │             │
             │     pause │             │
             │           │             │
             │           ▼             │
             │      ┌──────────┐       │
             └─────>│ PAUSED   │───────┘
                    └────┬─────┘
                         │ resume
                         ▼
                    ┌──────────┐
                    │ RUNNING  │
                    └──────────┘
```

### VM Operations

```c
// VM Management API
typedef struct vm vm_t;

nexus_result_t vm_create(vm_config_t *config, vm_t **vm);
nexus_result_t vm_destroy(vm_t *vm);
nexus_result_t vm_start(vm_t *vm);
nexus_result_t vm_stop(vm_t *vm);
nexus_result_t vm_pause(vm_t *vm);
nexus_result_t vm_resume(vm_t *vm);
nexus_result_t vm_reset(vm_t *vm);

// Snapshot and Migration
nexus_result_t vm_snapshot(vm_t *vm, const char *name);
nexus_result_t vm_restore(vm_t *vm, const char *name);
nexus_result_t vm_migrate(vm_t *vm, const char *dest_host);
```

## Performance Optimization

### Overhead Reduction Techniques

| Technique | Description | Benefit |
|-----------|-------------|---------|
| Paravirtualization | Guest-aware drivers | Reduced I/O overhead |
| CPU Pinning | Dedicated vCPU to pCPU | Reduced scheduling latency |
| Huge Pages | Large memory pages | Reduced TLB misses |
| IRQ Affinity | Interrupt routing | Better cache locality |
| Batched Exits | Coalesced VM exits | Reduced exit overhead |
| Posted Interrupts | Direct interrupt delivery | Lower latency |

### Performance Targets

| Metric | Target | Description |
|--------|--------|-------------|
| VM Exit Latency | < 1000 cycles | VM exit to re-entry |
| I/O Throughput | > 95% native | VirtIO block/network |
| Memory Overhead | < 1% | Hypervisor memory usage |
| CPU Overhead | < 3% | SPECint benchmark |

## Security Features

### VM Isolation

1. **Memory Isolation**: EPT/NPT prevents unauthorized access
2. **CPU Isolation**: Separate VMCS/VMCB per VM
3. **I/O Isolation**: IOMMU prevents DMA attacks
4. **Time Isolation**: Prevents timing-based side channels

### Secure Boot for VMs

```
┌─────────────────────────────────────────┐
│            VM Boot Process              │
│                                         │
│  1. Load VM firmware (OVMF/SeaBIOS)    │
│  2. Verify firmware signature           │
│  3. Load bootloader                     │
│  4. Verify bootloader signature         │
│  5. Load kernel                         │
│  6. Verify kernel signature             │
│  7. Boot kernel                         │
└─────────────────────────────────────────┘
```

### Attestation

```
┌─────────────────────────────────────────┐
│            Remote Attestation           │
│                                         │
│  Guest  ──────> Quote ──────> Verifier │
│                   │                     │
│                   ▼                     │
│            TPM/SGX Quote                │
│            (Signed Evidence)            │
└─────────────────────────────────────────┘
```

## Configuration

### VM Configuration Example

```ini
# VM Configuration File
[vm]
name = "my-vm"
uuid = "550e8400-e29b-41d4-a716-446655440000"
type = "hvm"

[cpu]
vcpus = 4
model = "host"
features = "vmx,ssse3"
pinning = "0-3"

[memory]
size = 8192
balloon = true
hugepages = true

[disk]
name = "disk0"
file = "/var/nexus/vm/my-vm/disk.qcow2"
format = "qcow2"
cache = "writeback"
io = "native"

[network]
name = "net0"
type = "virtio"
mac = "52:54:00:12:34:56"
bridge = "br0"

[graphics]
type = "vnc"
listen = "127.0.0.1"
port = 5900
```

## Related Documents

- [Architecture Overview](overview.md)
- [Kernel Design](kernel_design.md)
- [Kernel API](../api/kernel_api.md)

---

*Last Updated: March 2026*
*NEXUS-OS Version 1.0.0-alpha*
