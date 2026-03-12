# NEXUS OS - Storage Subsystem Documentation

## Copyright (c) 2026 NEXUS Development Team

**Version:** 1.0.0
**Last Updated:** March 2026
**Status:** Phase 2 In Progress

---

## Overview

The NEXUS OS storage subsystem provides comprehensive support for modern storage devices including NVMe, SATA, and SD/eMMC drives. It includes low-level drivers, a unified storage management API, and a graphical storage manager application.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    NEXUS OS Storage Subsystem                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   Storage Manager GUI                            │   │
│  │  (gui/storage/storage-manager.c/h)                               │   │
│  │  - Device enumeration & display                                  │   │
│  │  - SMART monitoring & health visualization                       │   │
│  │  - Partition management                                          │   │
│  │  - I/O statistics & graphing                                     │   │
│  │  - Encryption management                                         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                  │                                      │
│                                  ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                  Storage Manager API                             │   │
│  │  - Device abstraction layer                                      │   │
│  │  - Unified block device interface                                │   │
│  │  - SMART data aggregation                                        │   │
│  │  - Statistics collection                                         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                  │                                      │
│              ┌───────────────────┼───────────────────┐                  │
│              ▼                   ▼                   ▼                  │
│  ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐    │
│  │   NVMe Driver     │ │   AHCI Driver     │ │  SD/eMMC Driver   │    │
│  │   (nvme.c/h)      │ │   (ahci.c/h)      │ │  (sd.c/h)         │    │
│  │   - NVMe 1.4      │ │   - AHCI 1.3.1    │ │   - SD 6.0        │    │
│  │   - PCIe interface│ │   - SATA I/II/III │ │   - eMMC 5.1      │    │
│  │   - NCQ support   │ │   - FIS-based     │ │   - HS400/HS200   │    │
│  │   - SMART         │ │   - SMART         │ │   - HW reset      │    │
│  └───────────────────┘ └───────────────────┘ └───────────────────┘    │
│              │                   │                   │                  │
│              └───────────────────┼───────────────────┘                  │
│                                  ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    VFS / Block Layer                            │   │
│  │  - Virtual File System                                           │   │
│  │  - Block device registration                                     │   │
│  │  - I/O scheduling                                                │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Components

### 1. NVMe Driver (`drivers/storage/nvme.c/h`)

**Specification:** NVMe 1.4 compliant

**Features:**
- Controller initialization and management
- Admin and I/O queue creation (up to 64K entries)
- Namespace identification and management
- Full command submission and completion handling
- Doorbell register management with stride support
- SMART/Health monitoring
- Temperature tracking with thresholds
- Error logging and statistics
- Power state management
- Controller reset and shutdown

**Supported Devices:**
- NVMe SSDs (PCIe 3.0/4.0/5.0)
- NVMe add-in cards (AIC)
- M.2 NVMe drives
- U.2/U.3 enterprise drives

**API Functions:**
```c
// Driver lifecycle
int nvme_init(void);
int nvme_shutdown(void);
bool nvme_is_initialized(void);

// Device management
int nvme_probe(void);
nvme_device_t *nvme_get_device(u32 device_id);
int nvme_list_devices(nvme_device_t *devices, u32 *count);

// I/O operations
int nvme_io_read(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb, void *buffer);
int nvme_io_write(nvme_device_t *dev, u32 nsid, u64 lba, u16 nlb, const void *buffer);
int nvme_io_flush(nvme_device_t *dev, u32 nsid);

// Block interface
int nvme_block_read(u32 device_id, u64 lba, u32 count, void *buffer);
int nvme_block_write(u32 device_id, u64 lba, u32 count, const void *buffer);
u64 nvme_block_get_size(u32 device_id);

// SMART/Health
int nvme_get_smart_log(nvme_device_t *dev);
bool nvme_is_healthy(nvme_device_t *dev);
u32 nvme_get_temperature(nvme_device_t *dev);
```

**Statistics Tracked:**
- Read/write operation counts
- Bytes transferred
- Error counts
- Temperature history
- Power-on hours
- Available spare percentage
- Percentage used (wear level)

---

### 2. AHCI Driver (`drivers/storage/ahci.c/h`)

**Specification:** AHCI 1.3.1 compliant

**Features:**
- Controller initialization and reset
- Port enumeration (up to 32 ports)
- FIS-based command submission
- PRDT (Physical Region Descriptor Table)
- Command list management
- Device detection and identification
- Transfer mode negotiation (PIO/DMA/UDMA/NCQ)
- SMART support
- Power management (Partial/Slumber/DevSLP)
- Hot-plug detection

**Supported Devices:**
- SATA SSDs
- SATA HDDs (5400/7200/10000/15000 RPM)
- SATAPI (optical drives)
- External SATA (eSATA)
- Port multipliers

**API Functions:**
```c
// Driver lifecycle
int ahci_init(void);
int ahci_shutdown(void);
bool ahci_is_initialized(void);

// Controller operations
int ahci_probe(void);
ahci_controller_t *ahci_get_controller(u32 controller_id);
int ahci_controller_init(ahci_controller_t *ctrl);

// Port operations
int ahci_port_init(ahci_controller_t *ctrl, u32 port_num);
int ahci_port_start(ahci_controller_t *ctrl, u32 port_num);
ahci_port_t *ahci_get_port(ahci_controller_t *ctrl, u32 port_num);

// I/O operations
int ahci_read_sector(ahci_port_t *port, u64 lba, u32 count, void *buffer);
int ahci_write_sector(ahci_port_t *port, u64 lba, u32 count, const void *buffer);
int ahci_flush_cache(ahci_port_t *port);

// Block interface
int ahci_block_read(u32 controller_id, u32 port, u64 lba, u32 count, void *buffer);
int ahci_block_write(u32 controller_id, u32 port, u64 lba, u32 count, const void *buffer);
u64 ahci_block_get_size(u32 controller_id, u32 port);

// SMART operations
int ahci_smart_enable(ahci_port_t *port);
int ahci_smart_get_status(ahci_port_t *port);
bool ahci_is_smart_healthy(ahci_port_t *port);
```

**Device Detection:**
- Signature recognition (SATA: 0x00000000, SATAPI: 0xEB140101)
- SATA status monitoring (DET, SPD, IPM)
- Device identification via IDENTIFY DEVICE command

---

### 3. Storage Manager UI (`gui/storage/storage-manager.h`)

**Purpose:** Unified graphical interface for storage management

**Features:**
- Device enumeration and display
- Real-time I/O monitoring
- SMART health visualization
- Temperature graphing
- Partition management
- Encryption support
- Alert system

**Data Structures:**

```c
// Storage Device Information
typedef struct {
    u32 device_id;
    char name[64];
    char model[64];
    char serial[32];
    u32 device_type;          // NVMe, SATA, SSD, HDD, etc.
    u64 total_capacity;
    u64 used_capacity;
    u64 free_capacity;
    
    // Health
    u32 health_status;
    u8 health_percent;
    smart_data_t smart;
    
    // Statistics
    io_stats_t stats;
    io_sample_t history[120];
    
    // Partitions
    partition_info_t *partitions;
    u32 partition_count;
    
    // State
    bool is_present;
    bool supports_trim;
    bool supports_smart;
    bool is_encrypted;
    
    // Temperature
    u32 temperature;
    u32 temperature_min;
    u32 temperature_max;
} storage_device_t;

// SMART Data
typedef struct {
    bool supported;
    bool enabled;
    bool healthy;
    u16 temperature;
    u32 power_on_hours;
    u32 power_cycle_count;
    u64 host_reads;
    u64 host_writes;
    u32 available_spare;
    u32 percentage_used;
    u32 media_errors;
    smart_attribute_t attrs[64];
} smart_data_t;

// I/O Statistics
typedef struct {
    u64 reads;
    u64 writes;
    u64 read_bytes;
    u64 write_bytes;
    u64 read_latency_avg;
    u64 write_latency_avg;
    u32 iops_read;
    u32 iops_write;
    u32 utilization;
} io_stats_t;
```

**Health Status Levels:**
| Status | Description | Action |
|--------|-------------|--------|
| Good | 90-100% health | Normal operation |
| Fair | 70-89% health | Monitor closely |
| Poor | 40-69% health | Backup recommended |
| Critical | 10-39% health | Immediate backup |
| Failed | 0-9% health | Replace drive |

---

## File Structure

```
NEXUS-OS/
├── drivers/
│   └── storage/
│       ├── nvme.c              # NVMe driver implementation (2,268 lines)
│       ├── nvme.h              # NVMe driver header
│       ├── ahci.c              # AHCI driver implementation (1,634 lines)
│       ├── ahci.h              # AHCI driver header
│       ├── sd.c                # SD/eMMC driver (TODO)
│       └── sd.h                # SD/eMMC header (TODO)
│
├── gui/
│   └── storage/
│       ├── storage-manager.c   # Storage manager UI (TODO)
│       ├── storage-manager.h   # Storage manager header (408 lines)
│       ├── smart-widget.c      # SMART display widget (TODO)
│       ├── disk-graph.c        # I/O graphing widget (TODO)
│       └── partition-dialog.c  # Partition dialog (TODO)
│
└── docs/
    └── storage/
        ├── STORAGE_DOCUMENTATION.md    # This file
        ├── nvme-specs.md               # NVMe specification details
        ├── ahci-specs.md               # AHCI specification details
        ├── smart-attributes.md         # SMART attribute reference
        └── encryption-guide.md         # Encryption setup guide
```

---

## Usage Examples

### Enumerating Storage Devices

```c
#include "drivers/storage/nvme.h"
#include "drivers/storage/ahci.h"

// Initialize drivers
nvme_init();
ahci_init();

// List NVMe devices
nvme_device_t nvme_devices[16];
u32 nvme_count = 16;
nvme_list_devices(nvme_devices, &nvme_count);

for (u32 i = 0; i < nvme_count; i++) {
    printk("NVMe %d: %s - %d MB\n", 
           i, 
           nvme_devices[i].name,
           nvme_block_get_size(i) / (1024 * 1024));
}

// List AHCI controllers
ahci_controller_t ahci_ctrls[8];
u32 ahci_count = 8;
ahci_list_controllers(ahci_ctrls, &ahci_count);

for (u32 c = 0; c < ahci_count; c++) {
    for (u32 p = 0; p < ahci_ctrls[c].active_ports; p++) {
        u64 size = ahci_block_get_size(c, p);
        printk("AHCI %d:%d - %d MB\n", c, p, size / (1024 * 1024));
    }
}
```

### Reading/Writing Data

```c
// NVMe read
u8 buffer[4096];
nvme_block_read(device_id, lba, 8, buffer);  // 8 sectors = 4KB

// AHCI write
ahci_block_write(controller_id, port, lba, 8, buffer);

// Flush cache
nvme_block_flush(device_id);
ahci_block_flush(controller_id, port);
```

### SMART Monitoring

```c
// Get SMART data
nvme_device_t *dev = nvme_get_device(0);
nvme_get_smart_log(dev);

// Check health
if (nvme_is_healthy(dev)) {
    printk("Drive is healthy\n");
} else {
    printk("WARNING: Drive health degraded!\n");
}

// Get temperature
u32 temp = nvme_get_temperature(dev);
printk("Temperature: %d K (%.1f°C)\n", 
       temp, (temp - 273.15));

// Check SMART attributes
for (u32 i = 0; i < dev->smart.attr_count; i++) {
    smart_attribute_t *attr = &dev->smart.attrs[i];
    if (attr->current_value <= attr->threshold) {
        printk("ALERT: Attribute %s below threshold!\n", attr->name);
    }
}
```

---

## SMART Attributes Reference

### Common NVMe SMART Attributes

| ID | Name | Description |
|----|------|-------------|
| 01 | Available Spare | Remaining spare capacity (%) |
| 02 | Percentage Used | Wear level indicator (%) |
| 03 | Power On Hours | Total powered-on time |
| 04 | Power Cycle Count | Number of power cycles |
| 05 | Temperature | Current temperature |
| 06 | Critical Warning | Critical status flags |
| 07 | Media Errors | Count of media errors |
| 08 | Host Reads | Total host read commands |
| 09 | Host Writes | Total host write commands |
| 0A | Data Units Read | Data read (512B units) |
| 0B | Data Units Written | Data written (512B units) |

### Common ATA/SMART Attributes

| ID | Name | Description |
|----|------|-------------|
| 01 | Read Error Rate | Rate of hardware read errors |
| 03 | Spin-Up Time | Time to spin up platters |
| 04 | Start/Stop Count | Spindle start/stop cycles |
| 05 | Reallocated Sectors | Bad sectors remapped |
| 07 | Seek Error Rate | Rate of seek errors |
| 09 | Power-On Hours | Total powered-on time |
| 0A | Spin Retry Count | Spin retry attempts |
| 0C | Power Cycle Count | Power-on cycles |
| C0 | Power-Off Retract Count | Head parking events |
| C2 | Temperature | Current temperature |
| C3 | Hardware ECC Recovered | Error correction count |
| C5 | Current Pending Sector | Unstable sectors |
| C6 | Uncorrectable Errors | Uncorrectable errors |
| C7 | UltraDMA CRC Errors | Cable/interface errors |
| C8 | Write Error Rate | Write error rate |

---

## Performance Benchmarks

### NVMe Performance (Typical)

| Metric | PCIe 3.0 | PCIe 4.0 | PCIe 5.0 |
|--------|----------|----------|----------|
| Sequential Read | 3,500 MB/s | 7,000 MB/s | 14,000 MB/s |
| Sequential Write | 3,000 MB/s | 6,500 MB/s | 12,000 MB/s |
| Random Read (4K) | 500K IOPS | 800K IOPS | 1.5M IOPS |
| Random Write (4K) | 450K IOPS | 700K IOPS | 1.2M IOPS |
| Latency (avg) | 80 μs | 60 μs | 40 μs |

### SATA Performance (Typical)

| Metric | SATA II | SATA III |
|--------|---------|----------|
| Sequential Read | 280 MB/s | 550 MB/s |
| Sequential Write | 250 MB/s | 520 MB/s |
| Random Read (4K) | 30K IOPS | 90K IOPS |
| Random Write (4K) | 25K IOPS | 80K IOPS |
| Latency (avg) | 150 μs | 100 μs |

---

## Troubleshooting

### Common Issues

**1. Device Not Detected**
```
Problem: NVMe/SATA device not showing in device list
Solution:
  - Check physical connection
  - Verify PCIe slot is enabled in BIOS
  - Check for driver conflicts
  - Run: nvme_probe() or ahci_probe() to rescan
```

**2. SMART Data Not Available**
```
Problem: SMART returns empty or invalid data
Solution:
  - Verify SMART is enabled: ahci_smart_enable(port)
  - Check device supports SMART
  - Some external enclosures block SMART
```

**3. High Temperature**
```
Problem: Drive temperature exceeds warning threshold
Solution:
  - Check airflow and cooling
  - Reduce I/O load
  - Consider adding heatsink
  - Warning threshold: 70°C
  - Critical threshold: 80°C
```

**4. Performance Degradation**
```
Problem: Drive performance significantly reduced
Solution:
  - Check drive health (SMART)
  - Run TRIM: nvme_io_flush() or fstrim
  - Check for thermal throttling
  - Verify queue depth is adequate
```

---

## Security Considerations

### Encryption Support

The storage subsystem supports:
- **Software Encryption:** LUKS, dm-crypt
- **Hardware Encryption:** TCG Opal, Pyrite
- **Self-Encrypting Drives (SED):** Automatic encryption

### Secure Erase

```c
// NVMe secure erase
nvme_admin_format(dev, nsid, format_type);

// ATA secure erase
// Send SECURITY ERASE UNIT command via FIS
```

### Password Protection

```c
// Lock device
storage_manager_lock_device(manager, device_id);

// Unlock device
storage_manager_unlock_device(manager, device_id, password);
```

---

## Development Roadmap

### Phase 2 (Current)
- [x] NVMe Driver (100%)
- [x] AHCI Driver (100%)
- [ ] SD/eMMC Driver (0%)
- [x] Storage Manager Header (100%)
- [ ] Storage Manager UI (0%)
- [ ] SMART Visualization (0%)
- [ ] Encryption Manager (0%)

### Phase 3 (Next)
- [ ] RAID Support
- [ ] LVM Integration
- [ ] ZFS Support
- [ ] Network Storage (iSCSI, NFS)
- [ ] Storage Tiering

---

## API Reference

### Driver Initialization

| Function | Description | Returns |
|----------|-------------|---------|
| `nvme_init()` | Initialize NVMe driver | 0 on success |
| `ahci_init()` | Initialize AHCI driver | 0 on success |
| `nvme_shutdown()` | Shutdown NVMe driver | 0 on success |
| `ahci_shutdown()` | Shutdown AHCI driver | 0 on success |

### Device Information

| Function | Description | Returns |
|----------|-------------|---------|
| `nvme_get_device(id)` | Get NVMe device by ID | Device pointer |
| `ahci_get_controller(id)` | Get AHCI controller | Controller pointer |
| `nvme_block_get_size(id)` | Get device size in bytes | Size (u64) |
| `ahci_block_get_size(ctrl, port)` | Get port size | Size (u64) |

### I/O Operations

| Function | Description | Returns |
|----------|-------------|---------|
| `nvme_block_read(id, lba, count, buf)` | Read blocks | 0 on success |
| `nvme_block_write(id, lba, count, buf)` | Write blocks | 0 on success |
| `ahci_block_read(ctrl, port, lba, count, buf)` | Read blocks | 0 on success |
| `ahci_block_write(ctrl, port, lba, count, buf)` | Write blocks | 0 on success |

### SMART/Health

| Function | Description | Returns |
|----------|-------------|---------|
| `nvme_get_smart_log(dev)` | Get SMART log | 0 on success |
| `nvme_is_healthy(dev)` | Check health status | bool |
| `nvme_get_temperature(dev)` | Get temperature | Kelvin (u32) |
| `ahci_smart_get_status(port)` | Get SMART status | Status code |

---

## Contact & Support

- **Documentation:** `docs/storage/`
- **Issues:** https://github.com/devTechs001/nexus-os/issues
- **Email:** nexus-os@darkhat.dev

---

**Built with ❤️ by the NEXUS Development Team**
