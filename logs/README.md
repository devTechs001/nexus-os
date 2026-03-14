# NEXUS OS - Log Files Directory

This directory contains all system and build log files for NEXUS OS.

## Log Files

### Build Logs
| File | Description |
|------|-------------|
| `build_errors.log` | Build error logs and compilation warnings |

### Boot Logs
| File | Description |
|------|-------------|
| `boot.log` | General boot sequence logs |
| `boot_arch_test.log` | Architecture boot tests |
| `grub_error_capture.log` | GRUB bootloader errors |

### Runtime Logs
| File | Description |
|------|-------------|
| `serial.log` | Serial console output |
| `serial2.log` | Secondary serial output |
| `qemu_run.log` | QEMU runtime logs |
| `qemu_boot.log` | QEMU boot sequence |
| `qemu_full_boot.log` | Complete QEMU boot logs |
| `qemu_full.log` | Full QEMU session logs |
| `final_run.log` | Final test run logs |
| `run_test.log` | Test execution logs |

## System Logs (Runtime)

When NEXUS OS is running, system logs are stored in:

```
/var/log/
├── system.log          # System-wide events
├── kernel.log          # Kernel messages
├── boot.log            # Boot sequence
├── security.log        # Security events
├── diagnostic.log      # System diagnostics
├── sessions/           # Session logs
│   └── session_<id>_<timestamp>.log
└── users/              # User activity logs
    └── <username>.log
```

## Log Management

### Log Rotation
- Automatic rotation at 100MB per file
- 10 rotation cycles kept
- Compressed archives

### Log Levels
- DEBUG - Detailed debugging
- INFO - General information
- WARNING - Potential issues
- ERROR - Error conditions
- CRITICAL - Critical failures

### Viewing Logs

**Recent Logs:**
```c
log_show_recent(50, LOG_LEVEL_WARNING);  // Last 50 warnings+
```

**Statistics:**
```c
log_show_statistics();
```

**Diagnostic Report:**
```c
char buffer[2048];
log_get_diagnostic_report(buffer, 2048);
printk("%s\n", buffer);
```

## Feedback Integration

Logs can be automatically attached to feedback submissions:

```c
// Submit bug with auto-attached logs
u32 id = feedback_submit_bug(
    "System crash",
    "Description here",
    "Steps to reproduce",
    "Expected behavior",
    "Actual behavior",
    FEEDBACK_PRIORITY_HIGH
);
// System logs automatically attached
```

## Log Retention

- **Build logs:** Kept indefinitely
- **Boot logs:** Last 10 boots
- **Session logs:** Per session, deleted on logout
- **User logs:** Kept per user account
- **System logs:** Rotated, 100MB limit

## Privacy

- User logs contain activity information
- Session logs track commands and errors
- Anonymous feedback doesn't include user logs
- Logs are local-only unless submitted with feedback

---

**NEXUS OS Logging System**

*For more information, see `docs/components/system.md`*
