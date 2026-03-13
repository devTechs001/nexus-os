# NEXUS OS - Boot Diagnostics System

## Overview

The NEXUS OS Boot Diagnostics System provides real-time capture and analysis of boot failures. It includes:

1. **Kernel Module** (`kernel/diag/boot_diag.c`) - In-kernel boot logging
2. **Capture Script** (`tools/boot_diag_capture.sh`) - Real-time boot monitoring
3. **Analysis Tools** - Automatic failure detection and reporting

## Components

### 1. Kernel Boot Diagnostics Module

**Location:** `kernel/diag/boot_diag.c`, `kernel/diag/boot_diag.h`

**Features:**
- Ring buffer for boot log entries (256 entries, 4KB total)
- Real-time stage tracking
- Error capture with error codes
- Proc interface for runtime access
- Dump function for console output

**Boot Stages Tracked:**
| Stage ID | Name | Description |
|----------|------|-------------|
| 1 | EARLY | Early boot (before console) |
| 2 | SERIAL | Serial initialized |
| 3 | MULTIBOOT | Multiboot info parsed |
| 4 | GDT | GDT initialized |
| 5 | IDT | IDT initialized |
| 6 | PMM | Physical memory manager |
| 7 | VMM | Virtual memory manager |
| 8 | HEAP | Kernel heap |
| 9 | INTERRUPTS | Interrupt system |
| 10 | SCHEDULER | Scheduler |
| 11 | SMP | SMP initialization |
| 12 | DRIVERS | Device drivers |
| 13 | SERVICES | System services |
| 14 | COMPLETE | Boot complete |
| 255 | FAILED | Boot failure |

**Usage in Kernel Code:**
```c
#include "diag/boot_diag.h"

// Initialize (call early in boot)
boot_diag_init();

// Log stage
BOOT_LOG(BOOT_STAGE_GDT, "GDT initialized");

// Log error
BOOT_LOG_ERROR(BOOT_STAGE_PMM, "Memory detection failed", error_code);

// Mark complete
boot_diag_mark_complete();

// Dump to console
boot_diag_dump();
```

### 2. Boot Capture Script

**Location:** `tools/boot_diag_capture.sh`

**Features:**
- Real-time serial output monitoring
- Automatic boot stage detection
- Failure pattern recognition
- Comprehensive report generation

**Usage:**
```bash
# Run with default 15s timeout
./tools/boot_diag_capture.sh

# Run with custom timeout
./tools/boot_diag_capture.sh 30
```

**Output Files:**
- `boot_diagnostics/serial_capture.log` - Raw serial output
- `boot_diagnostics/vga_capture.log` - VGA/Console output
- `boot_diagnostics/boot_analysis.log` - Analysis report
- `boot_diagnostics/qemu_errors.log` - QEMU error messages

### 3. Serial Debug Markers

The boot assembly outputs debug characters to serial port:

| Character | Stage | Meaning |
|-----------|-------|---------|
| `B` | 32-bit entry | Boot loader entered kernel |
| `C` | BSS cleared | Uninitialized data zeroed |
| `L` | Long mode | CPU supports 64-bit |
| `P` | Paging | Page tables enabled |
| `6` | 64-bit mode | Long mode entered |
| `S` | Stack ready | Kernel stack initialized |
| `I` | Info passed | Multiboot info to kernel |
| `M` | **ERROR** | Invalid multiboot magic |
| `X` | **ERROR** | No long mode support |

## Quick Start

### 1. Build the Kernel
```bash
make clean && make
```

### 2. Run Diagnostics Capture
```bash
./tools/boot_diag_capture.sh
```

### 3. Review Report
```bash
cat boot_diagnostics/boot_analysis.log
```

## Interpreting Results

### Successful Boot
```
[SUCCESS] Boot stage 'B' detected (32-bit entry)
[SUCCESS] Boot stage 'C' detected (BSS cleared)
[SUCCESS] Boot stage 'L' detected (Long mode)
[SUCCESS] Boot stage 'P' detected (Paging enabled)
[SUCCESS] Boot stage '6' detected (64-bit mode)
[SUCCESS] Boot stage 'S' detected (Stack ready)
[SUCCESS] Boot stage 'I' detected (Info to kernel)
Boot stages found: 7/7
```

### Common Failures

#### Multiboot Magic Error
```
[ERROR] Boot stage 'M' detected
✗ Stage M: Multiboot magic error!
```
**Cause:** Kernel not loaded as Multiboot2 image  
**Fix:** Ensure GRUB uses `multiboot2` command

#### Long Mode Not Supported
```
[ERROR] Boot stage 'X' detected
✗ Stage X: Long mode not supported!
```
**Cause:** CPU doesn't support 64-bit mode  
**Fix:** Use 32-bit kernel or upgrade CPU

#### No Serial Output
```
[WARNING] No serial output captured
```
**Cause:** Serial port not initialized or wrong baud rate  
**Fix:** Check serial initialization in boot.asm

#### Triple Fault
```
QEMU exited unexpectedly
No boot stages found
```
**Cause:** CPU exception during boot  
**Fix:** Check QEMU error log for exception details

## Advanced Usage

### Real-Time Monitoring
```bash
# Monitor serial output in real-time
tail -f boot_diagnostics/serial_capture.log

# Watch for specific patterns
grep -E "B|C|L|P|6|S|I" boot_diagnostics/serial_capture.log
```

### Manual Analysis
```bash
# Check serial output
cat boot_diagnostics/serial_capture.log | od -c

# Check QEMU errors
cat boot_diagnostics/qemu_errors.log

# View full analysis
cat boot_diagnostics/boot_analysis.log
```

### Integration with Kernel

Add to `kernel/core/kernel.c`:
```c
#include "diag/boot_diag.h"

void kernel_main(u64 multiboot_info)
{
    // Initialize diagnostics
    boot_diag_init();
    BOOT_LOG(BOOT_STAGE_MULTIBOOT, "Multiboot info received");
    
    // ... boot code ...
    
    boot_diag_log(BOOT_STAGE_GDT, "GDT initialized");
    boot_diag_log(BOOT_STAGE_IDT, "IDT initialized");
    
    // ... more boot code ...
    
    boot_diag_mark_complete();
    boot_diag_dump();  // Print boot summary
}
```

## Troubleshooting

### Issue: No boot stages detected
**Solutions:**
1. Verify ISO exists: `ls -la build/nexus-kernel.iso`
2. Verify multiboot: `grub-file --is-x86-multiboot2 build/nexus-kernel.bin`
3. Check QEMU version: `qemu-system-x86_64 --version`
4. Try longer timeout: `./tools/boot_diag_capture.sh 30`

### Issue: Serial output empty
**Solutions:**
1. Check serial initialization in boot.asm
2. Verify baud rate (115200)
3. Check QEMU serial options
4. Try different serial backend: `-serial stdio`

### Issue: Boot stages incomplete
**Solutions:**
1. Check which stage is last reported
2. Review kernel code at that stage
3. Check for exceptions in QEMU error log
4. Use GDB for detailed debugging

## File Structure

```
NEXUS-OS/
├── kernel/
│   └── diag/
│       ├── boot_diag.c      # Diagnostics module
│       └── boot_diag.h      # Header file
├── tools/
│   └── boot_diag_capture.sh # Capture script
├── boot_diagnostics/        # Output directory (created at runtime)
│   ├── serial_capture.log
│   ├── vga_capture.log
│   ├── boot_analysis.log
│   └── qemu_errors.log
└── docs/
    └── BOOT_DIAGNOSTICS.md  # This file
```

## API Reference

### boot_diag_init()
Initialize the diagnostics system. Call early in boot.

### boot_diag_log(stage, message)
Log a boot stage. Parameters:
- `stage`: Boot stage identifier
- `message`: Human-readable message

### boot_diag_log_error(stage, message, error_code)
Log an error. Parameters:
- `stage`: Boot stage where error occurred
- `message`: Error description
- `error_code`: Numeric error code

### boot_diag_mark_complete()
Mark boot as successfully completed.

### boot_diag_mark_failed(reason)
Mark boot as failed. Parameters:
- `reason`: Failure description

### boot_diag_dump()
Print entire boot log to console.

### boot_diag_get_status()
Get pointer to current status structure.

## References

- BOOT_FIXES.md - Boot system fixes documentation
- BOOT_STATUS.md - Implementation status
- kernel/arch/x86_64/boot/boot.asm - Boot assembly with serial debug
- kernel/core/kernel.c - Kernel entry point

---

**Version:** 1.0  
**Author:** NEXUS OS Development Team  
**Date:** 2026-03-14
