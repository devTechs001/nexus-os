# NEXUS OS - System Components Documentation

## Overview

NEXUS OS includes comprehensive system management components for updates, recovery, health monitoring, and audio.

## Components

### 1. Startup Health System (`system/startup/`)

**File:** `startup_health.c`

Features:
- Dependency checking (256 max)
- Service monitoring (128 max)
- Auto-repair system
- Health status levels
- Startup modes
- Restore point integration

**Health Status Levels:**
- OK (0) - All systems operational
- WARNING (1) - Minor issues
- DEGRADED (2) - Some failures
- CRITICAL (3) - Major failures
- FAILED (4) - System unusable

**Startup Modes:**
- NORMAL - Full startup
- SAFE - Minimal drivers
- RECOVERY - Repair mode
- DIAGNOSTIC - Testing mode

### 2. Update Manager (`system/update/`)

**File:** `update_manager.c`

Features:
- Update channels (Stable, Beta, Dev, Nightly)
- Update types (System, Security, Driver, Application, Firmware, Feature)
- Auto-update modes
- Delta updates
- Checksum verification
- Dependency resolution
- Progress tracking
- Restart scheduling

**Auto-Update Modes:**
- DISABLED - Manual only
- DOWNLOAD_ONLY - Download but don't install
- NOTIFY - Notify before install
- FULL_AUTO - Download + install automatically

### 3. Reset Manager (`system/reset/`)

**File:** `reset_manager.c`

**Reset Options:**

| Option | Time | Restart | Preserves Data |
|--------|------|---------|----------------|
| Reset Settings | 5 min | No | Yes |
| Reset Applications | 10 min | Yes | Yes |
| Reset User Data | 30 min | Yes | No |
| Factory Reset | 60 min | Yes | No |
| Secure Erase | 120 min | Yes | No |

**Restore Points:**
- Auto-create before updates
- Auto-create daily
- Manual creation
- Restore from any point
- Delete old points
- Max: 100 restore points

### 4. Audio Equalizer (`system/audio/`)

**File:** `equalizer.c`

**7-Band Equalizer:**

| Band | Frequency | Range |
|------|-----------|-------|
| Sub-bass | 40 Hz | ±12 dB |
| Bass | 150 Hz | ±12 dB |
| Low-mid | 400 Hz | ±12 dB |
| Mid | 1000 Hz | ±12 dB |
| High-mid | 3000 Hz | ±12 dB |
| Presence | 5000 Hz | ±12 dB |
| Brilliance | 10000 Hz | ±12 dB |

**12 Audio Presets:**
- Flat, Rock, Pop, Jazz, Classical, Electronic
- Movie, Gaming, Voice
- Bass Boost, Treble Boost, Live

**6 Audio Effects:**
- Reverb, Chorus, Delay
- Compressor, Distortion, Spatial Audio

**Device Management:**
- 8 output devices
- 4 input devices
- Per-device volume/mute
- Noise reduction
- Echo cancellation

## Usage

### Startup Health

```c
// Initialize
startup_health_init();

// Register dependencies
startup_register_dependencies();
startup_register_services();

// Run health check
int health = startup_health_check();

// Auto-repair if needed
if (health != HEALTH_STATUS_OK) {
    startup_auto_repair();
}

// Show status
startup_show_status();
```

### Update Manager

```c
// Initialize
update_manager_init();

// Check for updates
update_check_for_updates();

// Download updates
update_download_all();

// Install updates
update_install_all();

// Enable auto-update
update_enable_auto_update(AUTO_UPDATE_FULL);
```

### Reset Manager

```c
// Initialize
reset_manager_init();

// List restore points
restore_list_points();

// Create restore point
restore_create_point("Before Update", "Backup before system update");

// Restore from point
restore_from_point(point_id);

// Reset system
reset_factory();  // Factory reset
reset_soft();     // Settings only
```

### Audio Equalizer

```c
// Initialize
equalizer_init();

// Set band gain
eq_set_band_gain(0, 6.0);  // +6 dB sub-bass

// Load preset
eq_load_preset(1);  // Load Rock preset

// Set effects
eq_set_effect("Reverb", true);
eq_set_effect_intensity("Reverb", 40.0);

// Master controls
eq_set_master_volume(80.0);
eq_set_mute(false);

// Show status
eq_show_status();
eq_list_presets();
```

## Integration

All system components integrate with each other:

```
Boot → Startup Health Check
           ↓
    Auto-Repair if needed
           ↓
    Check for Updates
           ↓
    Create Restore Point
           ↓
    Download/Install
           ↓
    Schedule Restart
           ↓
    Audio Feedback
```

## References

- [Startup Health API](startup/startup_health.c)
- [Update Manager API](update/update_manager.c)
- [Reset Manager API](reset/reset_manager.c)
- [Equalizer API](audio/equalizer.c)
