# NEXUS-OS GUI Framework

Modern, cross-platform user interfaces for NEXUS-OS built with Qt6/QML.

## 🖥️ GUI Applications

### 1. Boot Manager (`gui/boot-manager/`)
Interactive boot configuration with virtualization mode selection.

**Features:**
- 🎯 Virtualization mode selector (7 modes)
- 🔐 Secure boot configuration
- ⚡ Fast boot options
- 🖥️ Hardware resource allocation
- 🔒 Security profile selection
- 💡 Smart recommendations

**Screenshot Preview:**
```
╔══════════════════════════════════════════════════════════╗
║           NEXUS-OS Boot Configuration                    ║
╠══════════════════════════════════════════════════════════╣
║  ⚡ Virtualization Mode                                  ║
║  ┌────────────────────────────────────────────────────┐ ║
║  │ 🔷 Light Virtualization (Default)                  │ ◉║
║  │    Process isolation, balanced security/performance │ ║
║  │    Overhead: <1%  Security: Medium                 │ ║
║  └────────────────────────────────────────────────────┘ ║
║  ┌────────────────────────────────────────────────────┐ ║
║  │ 🖥️ Full Virtualization                             │ ○║
║  │    Complete hardware virtualization, enterprise    │ ║
║  │    Overhead: 2-5%  Security: High                  │ ║
║  └────────────────────────────────────────────────────┘ ║
║                                                          ║
║  🔧 Boot Options                                         ║
║  🔒 Secure Boot        [✓]                               ║
║  ⚡ Fast Boot          [✓]                               ║
║  📝 Verbose Boot       [ ]                               ║
╚══════════════════════════════════════════════════════════╝
```

### 2. VM Manager (`gui/vm-manager/`)
Complete virtual machine management dashboard.

**Features:**
- 📋 VM list with filtering and search
- 📊 Real-time resource monitoring (CPU, Memory)
- 🎮 Console access
- 📸 Snapshot management
- 🔄 VM lifecycle control (Start, Stop, Pause, Resume)
- 📤 Export/Import VMs
- 🔗 Live migration
- 📈 Performance charts

**Dashboard Layout:**
```
┌─────────────────────────────────────────────────────────────┐
│  🖥️ NEXUS VM Manager          [4 Running]  [➕ Create VM]   │
├──────────────┬──────────────────────────────────────────────┤
│  Search      │  ┌────────────────────────────────────┐     │
│  ┌────────┐  │  │ 🖥️ Development Environment         │     │
│  │🔍      │  │  │ ▶ Running  │  4 vCPU  │  4 GB     │     │
│  └────────┘  │  │ CPU: ████████░░ 23%               │     │
│              │  │ MEM: ██████████ 67%               │     │
│  [All]       │  └────────────────────────────────────┘     │
│  [Running]   │  ┌────────────────────────────────────┐     │
│  [Stopped]   │  │ 🖥️ Production Server                │     │
│  [Paused]    │  │ ▶ Running  │  8 vCPU  │  16 GB    │     │
│              │  │ CPU: ████████████ 45%              │     │
│              │  │ MEM: ████████████████ 82%          │     │
│              │  └────────────────────────────────────┘     │
├──────────────┼──────────────────────────────────────────────┤
│  Selected VM │  📊 CPU Usage    📈 Memory Usage            │
│  ┌────────┐  │  ┌──────────┐    ┌──────────┐              │
│  │   DE   │  │  │  Chart   │    │  Chart   │              │
│  │ Dev VM │  │  │          │    │          │              │
│  └────────┘  │  └──────────┘    └──────────┘              │
│              │  ⚙️ VM Configuration                        │
│  [Start]     │  💻 4 vCPUs  📦 4 GB  💾 64 GB  🌐 192.168 │
│  [Console]   │  🔷 Process   🖥️ NEXUS   📅 2024-01-15    │
└──────────────┴──────────────────────────────────────────────┘
```

### 3. System Settings (`gui/system-settings/`)
Comprehensive system configuration.

**Modules:**
- General Settings
- Display & Appearance
- Network Configuration
- Security Settings
- User Accounts
- Update Management
- Backup & Restore

### 4. Control Center (`gui/control-center/`)
System monitoring and quick settings.

**Features:**
- 📊 System dashboard
- 🔋 Power management
- 📶 Network status
- 💾 Storage management
- 🔔 Notifications
- ⚡ Quick toggles

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt6/QML Application                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   C++       │  │   C++       │  │   C++       │        │
│  │  Backend    │  │  Backend    │  │  Backend    │        │
│  │  (Model)    │  │ (Controller)│  │ (Manager)   │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                │                 │
│         └────────────────┼────────────────┘                 │
│                          │                                  │
│                  ┌───────▼────────┐                         │
│                  │  QML Context   │                         │
│                  └───────┬────────┘                         │
├──────────────────────────┼──────────────────────────────────┤
│                   ┌──────▼──────┐                           │
│                   │  QML/UI     │                           │
│                   │  Frontend   │                           │
│                   └─────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

## 🛠️ Build Instructions

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install qt6-base-dev qt6-declarative-dev \
                     qt6-quick-controls2-dev qt6-charts-dev \
                     qml6-module-qtquick-controls \
                     qml6-module-qtquick-layouts \
                     qml6-module-qtcharts

# Arch Linux
sudo pacman -S qt6-base qt6-declarative qt6-quickcontrols2 \
               qt6-charts

# macOS
brew install qt@6
```

### Build

```bash
cd NEXUS-OS/gui
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_PREFIX_PATH=/path/to/qt6

make -j$(nproc)

# Install
sudo make install
```

### Run Applications

```bash
# Boot Manager
./nexus-boot-manager

# VM Manager
./nexus-vm-manager

# System Settings
./nexus-settings

# Control Center
./nexus-control-center
```

## 📱 Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux Desktop | ✅ Full | Primary target |
| Windows | 🔄 Partial | Testing required |
| macOS | 🔄 Partial | Testing required |
| Android | 📋 Planned | Touch optimization |
| iOS | 📋 Planned | Touch optimization |

## 🎨 Design System

### Color Palette

```cpp
// Dark Theme (Default)
Background:    #0d1117
Surface:       #161b22
Border:        #30363d
Primary:       #58a6ff
Accent:        #e94560
Success:       #2ea043
Warning:       #d29922
Error:         #f85149
Text:          #e6edf3
Text Muted:    #8b949e
```

### Material Design

```qml
Material.theme: Material.Dark
Material.primary: "#58a6ff"
Material.accent: "#e94560"
```

## 📦 Components

### Boot Manager Components

| Component | Description |
|-----------|-------------|
| `BootConfigModel` | Boot configuration data model |
| `VirtModeManager` | Virtualization mode management |
| `Main.qml` | Boot configuration UI |

### VM Manager Components

| Component | Description |
|-----------|-------------|
| `VMModel` | VM list data model |
| `VMController` | VM lifecycle controller |
| `Main.qml` | VM management dashboard |

## 🔌 Integration

### With Hypervisor

```cpp
#include <nexus/hypervisor.h>

// Get hypervisor instance
auto& hypervisor = NexusOS::NEXUSHypervisor::instance();

// Initialize
hypervisor.initialize(NexusOS::VirtMode::LIGHT);

// Create VM
NexusOS::VMConfig config;
config.name = "MyVM";
auto vm = hypervisor.createVM(config);
vm->start();
```

### With QML

```qml
// Expose to QML
engine.rootContext()->setContextProperty("vmController", &controller);

// Use in QML
Button {
    onClicked: vmController.startVM(selectedVM)
}
```

## 📖 API Reference

### BootConfigModel

```qml
// Properties
bootConfig.selectedVirtMode      // int
bootConfig.selectedVirtModeName  // string
bootConfig.secureBoot            // bool
bootConfig.fastBoot              // bool
bootConfig.cpuCount              // int
bootConfig.memorySize            // int

// Methods
bootConfig.getVirtModes()        // Returns available modes
bootConfig.applyConfig()         // Apply configuration
bootConfig.resetToDefaults()     // Reset to defaults
bootConfig.validateConfig()      // Validate current config
```

### VMModel

```qml
// Properties
vmModel.totalCount    // int - Total VMs
vmModel.runningCount  // int - Running VMs

// Methods
vmModel.getAllVMs()           // Returns all VMs
vmModel.getVM(id)             // Get specific VM
vmModel.addVM(config)         // Add new VM
vmModel.removeVM(id)          // Remove VM
vmModel.filterByStatus(s)     // Filter by status
```

### VMController

```qml
// Methods
vmController.createVM(config)     // Create new VM
vmController.startVM(id)          // Start VM
vmController.stopVM(id)           // Stop VM
vmController.pauseVM(id)          // Pause VM
vmController.resumeVM(id)         // Resume VM
vmController.deleteVM(id)         // Delete VM
vmController.openConsole(id)      // Open console
vmController.createSnapshot(id, name)  // Create snapshot
```

## 🧪 Testing

```bash
# Run tests
ctest --output-on-failure

# Run with QML profiler
qmlprofiler ./nexus-vm-manager
```

## 📝 License

Proprietary (NEXUS-OS Core)

## 🤝 Contributing

See `CONTRIBUTING.md` for guidelines.

## 📧 Support

- Documentation: `docs/`
- Issues: GitHub Issues
- Email: nexus-gui@darkhat.dev
