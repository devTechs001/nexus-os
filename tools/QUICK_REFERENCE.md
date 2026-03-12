# NEXUS OS - Quick Reference Card

## Copyright (c) 2026 NEXUS Development Team

---

## 🚀 Quick Start

```bash
# 1. Install aliases (one-time)
cd /path/to/nexus-os/tools
./setup.sh

# 2. Restart shell or run:
source ~/.bashrc  # or source ~/.zshrc

# 3. Build and run
nexus-start
```

---

## 📋 Command Reference

### Build Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `nexus-build` | `nb` | Build kernel and ISO |
| `nexus-clean` | `nc` | Clean build files |
| `nexus-rebuild` | `nr` | Clean and rebuild |
| `nexus-status` | `ns` | Show build status |

### Run Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `nexus-run` | `nrun` | Run in QEMU (basic, 2GB RAM) |
| `nexus-run-serial` | - | Run in QEMU (serial console) |
| `nexus-run-debug` | `nrd` | Run in QEMU (debug mode) |
| `nexus-run-full` | - | Run in QEMU (4GB RAM, 4 CPUs) |
| `nexus-run-vmware` | `nrv` | Run in VMware |
| `nexus-run-vm` | `nvm` | Auto-create VM and run |

### Installation Commands

| Command | Description |
|---------|-------------|
| `nexus-create-usb /dev/sdX` | Create bootable USB |
| `nexus-install-qemu` | Install QEMU |
| `nexus-install-build-deps` | Install build dependencies |
| `nexus-install-all` | Install all dependencies |

### Utility Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `nexus-help` | `nh` | Show help |
| `nexus-version` | `nv` | Show version info |
| `nexus-docs` | `nd` | Open documentation |
| `nexus-gui` | - | Launch GUI tools |
| `nexus-start` | - | Quick start (build + run) |
| `nexus-interactive` | `ni` | Interactive mode |

---

## 🎯 Common Workflows

### First Time Setup

```bash
# Clone repository
git clone https://github.com/devTechs001/nexus-os.git
cd nexus-os/tools

# Run setup (installs aliases + dependencies)
./setup.sh

# Restart shell
source ~/.bashrc

# Verify installation
nexus-status
```

### Daily Development

```bash
# Build
nb

# Run in QEMU
nrun

# Debug
nrd

# Clean and rebuild
nr
```

### Create Bootable USB

```bash
# List available devices
lsblk

# Create USB (REPLACE sdX with your device!)
nexus-create-usb /dev/sdX
```

### Install Dependencies

```bash
# Install everything
nexus-install-all

# Or individually
nexus-install-build-deps
nexus-install-qemu
```

---

## 🔧 Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `NEXUS_OS_DIR` | NEXUS OS directory | Auto-detected |
| `NEXUS_BUILD_DIR` | Build directory | `$NEXUS_OS_DIR/build` |
| `NEXUS_ISO` | ISO path | `$NEXUS_BUILD_DIR/nexus-kernel.iso` |

---

## 📁 File Locations

| File | Path |
|------|------|
| Kernel Binary | `build/nexus-kernel.bin` |
| Bootable ISO | `build/nexus-kernel.iso` |
| Aliases Script | `tools/nexus-aliases.sh` |
| Setup Script | `tools/setup.sh` |
| Documentation | `docs/` |

---

## ⌨️ Shell Shortcuts

After installing aliases, you can use these shortcuts:

```bash
nb      # Build
nc      # Clean
nr      # Rebuild
ns      # Status
nrun    # Run in QEMU
nrd     # Run debug
nrv     # Run VMware
nvm     # Auto VM
nh      # Help
nv      # Version
nd      # Docs
nstart  # Quick start
ni      # Interactive
```

---

## 🐛 Troubleshooting

### "Command not found"

```bash
# Make sure aliases are installed
./tools/setup.sh --install

# Or manually source
source tools/nexus-aliases.sh

# Restart shell
source ~/.bashrc
```

### "ISO not found"

```bash
# Build the ISO first
nb

# Check status
ns
```

### "QEMU not found"

```bash
# Install QEMU
nexus-install-qemu

# Or manually
sudo apt-get install qemu-system-x86
```

---

## 📖 Full Documentation

- `PROJECT_SUMMARY.md` - Project overview
- `docs/SETUP_GUIDE.md` - Installation guide
- `docs/DEVELOPMENT_STATUS.md` - Development status
- `docs/GUI_IMPLEMENTATION.md` - GUI guide
- `docs/NETWORK_MANAGEMENT.md` - Network guide
- `HOW_TO_RUN.md` - QEMU usage guide

---

## 💡 Tips

1. **Use aliases** - They're much faster to type
2. **Interactive mode** - Great for beginners: `ni`
3. **Debug mode** - Use `nrd` when troubleshooting
4. **Status check** - Use `ns` to verify build
5. **Help** - Type `nh` anytime for help

---

## 📞 Support

- **GitHub:** https://github.com/devTechs001/nexus-os
- **Issues:** https://github.com/devTechs001/nexus-os/issues
- **Email:** nexus-os@darkhat.dev

---

**Happy Coding! 🚀**
