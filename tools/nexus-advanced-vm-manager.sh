#!/bin/bash
#
# NEXUS OS - Advanced VM Manager
# Copyright (c) 2026 NEXUS Development Team
#
# Advanced virtual machine manager with full customization,
# preferences, and optimizations
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"
VM_DIR="$HOME/vmware-vm/NEXUS-OS"
VMX_FILE="$VM_DIR/NEXUS-OS.vmx"
CONFIG_FILE="$HOME/.config/nexus-os/vm-config.conf"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
RESET='\033[0m'

# Default VM settings
DEFAULT_RAM=2048
DEFAULT_CPUS=2
DEFAULT_DISK_SIZE="20G"
DEFAULT_NETWORK="nat"
DEFAULT_DISPLAY="vmware"

print_header() {
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  Advanced VM Manager"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }

# Create config directory
init_config() {
    mkdir -p "$(dirname "$CONFIG_FILE")"
    mkdir -p "$VM_DIR"
    
    if [ ! -f "$CONFIG_FILE" ]; then
        cat > "$CONFIG_FILE" << EOF
# NEXUS OS VM Configuration
# Copyright (c) 2026 NEXUS Development Team

# Memory Settings
RAM_SIZE=$DEFAULT_RAM
MEMORY_BALLOON=enabled

# CPU Settings
CPU_COUNT=$DEFAULT_CPUS
CPU_CORES=2
CPU_THREADS=1
VIRTUALIZATION=enabled

# Display Settings
DISPLAY_MEMORY=128
ACCELERATE_3D=true
ACCELERATE_2D=true

# Network Settings
NETWORK_TYPE=$DEFAULT_NETWORK
NETWORK_BRIDGE=
NETWORK_HOSTONLY=

# Storage Settings
DISK_SIZE=$DEFAULT_DISK_SIZE
DISK_TYPE=thin
CDROM_AUTOCONNECT=true

# Advanced Settings
USB_ENABLED=true
USB_VERSION=3.0
SOUND_ENABLED=false
PRINTER_ENABLED=false
SHARED_FOLDERS=enabled
DRAG_DROP=enabled
COPY_PASTE=enabled

# Performance
HARDWARE_VIRTUALIZATION=true
NESTED_VIRTUALIZATION=false
IOMMU=enabled
SR_IOV=disabled

# Snapshots
AUTO_SNAPSHOT=false
SNAPSHOT_INTERVAL=3600

# Boot Options
BOOT_ORDER=cdrom
BOOT_TIMEOUT=10
DEFAULT_BOOT_MODE=vmware
EOF
        print_success "Configuration file created: $CONFIG_FILE"
    fi
}

# Load configuration
load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
        print_info "Configuration loaded"
    fi
}

# Show main menu
show_main_menu() {
    clear
    print_header
    
    echo ""
    echo "  Main Menu"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    echo "  1.  Create New VM"
    echo "  2.  Start VM"
    echo "  3.  Stop VM"
    echo "  4.  Restart VM"
    echo "  5.  VM Settings"
    echo "  6.  Performance Optimization"
    echo "  7.  Network Configuration"
    echo "  8.  Display & Graphics"
    echo "  9.  Storage Management"
    echo "  10. USB Devices"
    echo "  11. Shared Folders"
    echo "  12. Snapshots"
    echo "  13. Clone VM"
    echo "  14. Export VM"
    echo "  15. Import VM"
    echo "  16. Reset to Defaults"
    echo "  0.  Exit"
    echo ""
    echo "  ═══════════════════════════════════════════════════"
    echo ""
}

# Show VM settings menu
show_settings_menu() {
    clear
    print_header
    
    echo ""
    echo "  VM Settings"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    echo "  Current Configuration:"
    echo "  ───────────────────────────────────────────────────"
    load_config
    echo "    Memory:        ${RAM_SIZE}MB"
    echo "    CPUs:          ${CPU_COUNT}"
    echo "    Disk Size:     ${DISK_SIZE}"
    echo "    Network:       ${NETWORK_TYPE}"
    echo "    Display:       ${DISPLAY_MEMORY}MB"
    echo "    USB:           ${USB_ENABLED} (${USB_VERSION})"
    echo "    3D Acceleration: ${ACCELERATE_3D}"
    echo "  ───────────────────────────────────────────────────"
    echo ""
    echo "  1.  Change Memory"
    echo "  2.  Change CPU Count"
    echo "  3.  Change Disk Size"
    echo "  4.  Network Settings"
    echo "  5.  Display Settings"
    echo "  6.  USB Settings"
    echo "  7.  Boot Options"
    echo "  8.  Advanced Settings"
    echo "  9.  Save Configuration"
    echo "  0.  Back"
    echo ""
    echo "  ═══════════════════════════════════════════════════"
    echo ""
}

# Show performance menu
show_performance_menu() {
    clear
    print_header
    
    echo ""
    echo "  Performance Optimization"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    echo "  Current Settings:"
    echo "  ───────────────────────────────────────────────────"
    load_config
    echo "    Hardware Virtualization: ${HARDWARE_VIRTUALIZATION}"
    echo "    Nested Virtualization:   ${NESTED_VIRTUALIZATION}"
    echo "    IOMMU:                   ${IOMMU}"
    echo "    SR-IOV:                  ${SR_IOV}"
    echo "    Memory Ballooning:       ${MEMORY_BALLOON}"
    echo "  ───────────────────────────────────────────────────"
    echo ""
    echo "  Optimization Profiles:"
    echo "  ───────────────────────────────────────────────────"
    echo "  1.  Balanced (Default)"
    echo "  2.  Performance Mode"
    echo "  3.  Development Mode"
    echo "  4.  Gaming Mode"
    echo "  5.  Server Mode"
    echo "  6.  Minimal Resources"
    echo ""
    echo "  Advanced:"
    echo "  ───────────────────────────────────────────────────"
    echo "  7.  Toggle Hardware Virtualization"
    echo "  8.  Toggle Nested Virtualization"
    echo "  9.  Configure IOMMU"
    echo "  10. Memory Ballooning"
    echo "  0.  Back"
    echo ""
    echo "  ═══════════════════════════════════════════════════"
    echo ""
}

# Create VM
create_vm() {
    clear
    print_header
    
    echo ""
    echo "  Create New Virtual Machine"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    
    # Check if VM already exists
    if [ -f "$VMX_FILE" ]; then
        print_warning "VM already exists at: $VM_DIR"
        read -p "Overwrite existing VM? (y/N): " overwrite
        if [ "$overwrite" != "y" ] && [ "$overwrite" != "Y" ]; then
            return
        fi
        rm -rf "$VM_DIR"
    fi
    
    echo ""
    echo "  VM Configuration"
    echo "  ───────────────────────────────────────────────────"
    echo ""
    
    # Memory
    read -p "Memory (MB) [$DEFAULT_RAM]: " ram
    ram=${ram:-$DEFAULT_RAM}
    
    # CPUs
    read -p "CPU Count [$DEFAULT_CPUS]: " cpus
    cpus=${cpus:-$DEFAULT_CPUS}
    
    # Disk
    read -p "Disk Size (e.g., 20G) [$DEFAULT_DISK_SIZE]: " disk
    disk=${disk:-$DEFAULT_DISK_SIZE}
    
    # Network
    echo ""
    echo "  Network Type:"
    echo "  1. NAT (Default)"
    echo "  2. Bridged"
    echo "  3. Host-Only"
    echo "  4. None"
    read -p "Select [1-4]: " net_choice
    
    case $net_choice in
        1) network="nat" ;;
        2) network="bridged" ;;
        3) network="hostonly" ;;
        4) network="none" ;;
        *) network="nat" ;;
    esac
    
    echo ""
    print_info "Creating VM directory..."
    mkdir -p "$VM_DIR"
    
    print_info "Creating VMX configuration..."
    cat > "$VMX_FILE" << EOF
config.version = "8"
virtualHW.version = "19"
virtualHW.productCompatibility = "hosted"
displayName = "NEXUS OS"
guestOS = "other-64"
nvram = "NEXUS-OS.nvram"
uuid.location = "56 4d $(printf '%02x %02x %02x %02x %02x %02x %02x %02x' $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM)"
uuid.bios = "56 4d $(printf '%02x %02x %02x %02x %02x %02x %02x %02x' $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM $RANDOM)"

# Memory and CPU
memsize = "$ram"
numvcpus = "$cpus"
cpuid.coresPerSocket = "2"

# Boot configuration
boot.order = "cdrom"
bios.bootOrder = "cdrom,hdd,ethernet"

# IDE CD-ROM for ISO
ide1:0.present = "TRUE"
ide1:0.fileName = "$NEXUS_OS_DIR/build/nexus-kernel.iso"
ide1:0.deviceType = "cdrom-image"
ide1:0.startConnected = "TRUE"
ide1:0.autodetect = "FALSE"

# Hard disk
scsi0.present = "TRUE"
scsi0.virtualDev = "lsilogic"
scsi0:0.present = "TRUE"
scsi0:0.fileName = "NEXUS-OS.vmdk"
scsi0:0.writeThrough = "FALSE"

# Network
ethernet0.present = "TRUE"
ethernet0.connectionType = "$network"
ethernet0.virtualDev = "vmxnet3"
ethernet0.wakeOnPcktRcv = "FALSE"
ethernet0.addressType = "generated"

# USB
usb.present = "TRUE"
ehci.present = "TRUE"
uhci.present = "TRUE"
usb.autoConnect.device0 = "FALSE"

# Display
svga.vramSize = "128"
svga.autodetect = "TRUE"
mks.enable3d = "TRUE"

# Power management
powerType.powerOff = "soft"
powerType.powerOn = "soft"
powerType.suspend = "soft"
powerType.reset = "soft"

# VMware Tools
isolation.tools.hgfs.disable = "FALSE"
isolation.tools.dnd.disable = "FALSE"
isolation.tools.copy.disable = "FALSE"
isolation.tools.paste.disable = "FALSE"

# Advanced
monitor.virtual_exec = "software"
monitor.virtual_mmu = "software"
vhv.enable = "TRUE"
EOF

    print_info "Creating virtual disk..."
    if command -v vmware-vdiskmanager &> /dev/null; then
        vmware-vdiskmanager -c -s "$disk" -a lsilogic -t 0 "$VM_DIR/NEXUS-OS.vmdk"
    else
        # Create sparse disk using dd
        dd if=/dev/zero of="$VM_DIR/NEXUS-OS.vmdk" bs=1M count=1 seek=$(echo "$disk" | sed 's/G/*1024/') 2>/dev/null || true
    fi
    
    # Save configuration
    sed -i "s/RAM_SIZE=.*/RAM_SIZE=$ram/" "$CONFIG_FILE"
    sed -i "s/CPU_COUNT=.*/CPU_COUNT=$cpus/" "$CONFIG_FILE"
    sed -i "s/DISK_SIZE=.*/DISK_SIZE=$disk/" "$CONFIG_FILE"
    sed -i "s/NETWORK_TYPE=.*/NETWORK_TYPE=$network/" "$CONFIG_FILE"
    
    print_success "VM created successfully!"
    echo ""
    echo "  VM Location: $VM_DIR"
    echo "  Configuration: $VMX_FILE"
    echo ""
    read -p "Press Enter to continue..."
}

# Start VM
start_vm() {
    clear
    print_header
    
    echo ""
    echo "  Starting Virtual Machine"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    
    if [ ! -f "$VMX_FILE" ]; then
        print_error "VM not found. Create VM first."
        echo ""
        read -p "Press Enter to continue..."
        return
    fi
    
    load_config
    
    print_info "VM Configuration:"
    echo "    Memory: ${RAM_SIZE}MB"
    echo "    CPUs: ${CPU_COUNT}"
    echo "    Network: ${NETWORK_TYPE}"
    echo ""
    
    # Check for VMware
    if command -v vmplayer &> /dev/null; then
        print_info "Starting VMware Player..."
        vmplayer "$VMX_FILE" &
        print_success "VM started"
    elif command -v vmware &> /dev/null; then
        print_info "Starting VMware Workstation..."
        vmware "$VMX_FILE" &
        print_success "VM started"
    elif command -v vmrun &> /dev/null; then
        print_info "Starting VM in headless mode..."
        vmrun start "$VMX_FILE" nogui
        print_success "VM started (headless)"
    else
        print_error "VMware not found. Please install VMware."
    fi
    
    echo ""
    read -p "Press Enter to continue..."
}

# Stop VM
stop_vm() {
    clear
    print_header
    
    echo ""
    echo "  Stopping Virtual Machine"
    echo "  ═══════════════════════════════════════════════════"
    echo ""
    
    if command -v vmrun &> /dev/null; then
        print_info "Stopping VM..."
        vmrun stop "$VMX_FILE"
        print_success "VM stopped"
    else
        print_warning "vmrun not found. Please stop VM manually."
    fi
    
    echo ""
    read -p "Press Enter to continue..."
}

# Apply performance profile
apply_performance_profile() {
    local profile="$1"
    
    load_config
    
    case $profile in
        balanced)
            RAM_SIZE=2048
            CPU_COUNT=2
            HARDWARE_VIRTUALIZATION=true
            NESTED_VIRTUALIZATION=false
            MEMORY_BALLOON=enabled
            ;;
        performance)
            RAM_SIZE=4096
            CPU_COUNT=4
            HARDWARE_VIRTUALIZATION=true
            NESTED_VIRTUALIZATION=true
            MEMORY_BALLOON=disabled
            ACCELERATE_3D=true
            ;;
        development)
            RAM_SIZE=8192
            CPU_COUNT=4
            HARDWARE_VIRTUALIZATION=true
            NESTED_VIRTUALIZATION=true
            MEMORY_BALLOON=disabled
            USB_ENABLED=true
            SHARED_FOLDERS=enabled
            ;;
        gaming)
            RAM_SIZE=8192
            CPU_COUNT=4
            HARDWARE_VIRTUALIZATION=true
            ACCELERATE_3D=true
            DISPLAY_MEMORY=256
            USB_ENABLED=true
            ;;
        server)
            RAM_SIZE=4096
            CPU_COUNT=8
            HARDWARE_VIRTUALIZATION=true
            NETWORK_TYPE=bridged
            USB_ENABLED=false
            SOUND_ENABLED=false
            ;;
        minimal)
            RAM_SIZE=1024
            CPU_COUNT=1
            HARDWARE_VIRTUALIZATION=false
            NESTED_VIRTUALIZATION=false
            ACCELERATE_3D=false
            ;;
    esac
    
    # Save configuration
    save_config
    
    print_success "Performance profile applied: $profile"
    sleep 2
}

# Save configuration
save_config() {
    cat > "$CONFIG_FILE" << EOF
# NEXUS OS VM Configuration
# Copyright (c) 2026 NEXUS Development Team

# Memory Settings
RAM_SIZE=$RAM_SIZE
MEMORY_BALLOON=$MEMORY_BALLOON

# CPU Settings
CPU_COUNT=$CPU_COUNT
CPU_CORES=2
CPU_THREADS=1
VIRTUALIZATION=enabled

# Display Settings
DISPLAY_MEMORY=$DISPLAY_MEMORY
ACCELERATE_3D=$ACCELERATE_3D
ACCELERATE_2D=$ACCELERATE_2D

# Network Settings
NETWORK_TYPE=$NETWORK_TYPE
NETWORK_BRIDGE=
NETWORK_HOSTONLY=

# Storage Settings
DISK_SIZE=$DISK_SIZE
DISK_TYPE=thin
CDROM_AUTOCONNECT=true

# USB Settings
USB_ENABLED=$USB_ENABLED
USB_VERSION=$USB_VERSION
SOUND_ENABLED=$SOUND_ENABLED
PRINTER_ENABLED=false
SHARED_FOLDERS=$SHARED_FOLDERS
DRAG_DROP=$DRAG_DROP
COPY_PASTE=$COPY_PASTE

# Performance
HARDWARE_VIRTUALIZATION=$HARDWARE_VIRTUALIZATION
NESTED_VIRTUALIZATION=$NESTED_VIRTUALIZATION
IOMMU=$IOMMU
SR_IOV=$SR_IOV

# Snapshots
AUTO_SNAPSHOT=$AUTO_SNAPSHOT
SNAPSHOT_INTERVAL=$SNAPSHOT_INTERVAL

# Boot Options
BOOT_ORDER=cdrom
BOOT_TIMEOUT=10
DEFAULT_BOOT_MODE=vmware
EOF
    
    print_success "Configuration saved"
}

# Main loop
main_loop() {
    init_config
    load_config
    
    while true; do
        show_main_menu
        read -p "Select option [0-16]: " choice
        
        case $choice in
            1) create_vm ;;
            2) start_vm ;;
            3) stop_vm ;;
            4) start_vm ;;  # Restart (simplified)
            5) show_settings_menu ;;
            6) show_performance_menu ;;
            7) print_info "Network Configuration (coming soon)" ; sleep 2 ;;
            8) print_info "Display Settings (coming soon)" ; sleep 2 ;;
            9) print_info "Storage Management (coming soon)" ; sleep 2 ;;
            10) print_info "USB Devices (coming soon)" ; sleep 2 ;;
            11) print_info "Shared Folders (coming soon)" ; sleep 2 ;;
            12) print_info "Snapshots (coming soon)" ; sleep 2 ;;
            13) print_info "Clone VM (coming soon)" ; sleep 2 ;;
            14) print_info "Export VM (coming soon)" ; sleep 2 ;;
            15) print_info "Import VM (coming soon)" ; sleep 2 ;;
            16) 
                print_warning "Reset to defaults?"
                read -p "Continue? (y/N): " confirm
                if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
                    rm -f "$CONFIG_FILE"
                    init_config
                    print_success "Configuration reset"
                    sleep 2
                fi
                ;;
            0) 
                print_info "Exiting..."
                exit 0
                ;;
            *) print_error "Invalid option" ; sleep 2 ;;
        esac
    done
}

# Handle arguments
case "${1:-}" in
    --create) create_vm ;;
    --start) start_vm ;;
    --stop) stop_vm ;;
    --settings) show_settings_menu ;;
    --performance) show_performance_menu ;;
    --help|-h)
        print_header
        echo ""
        echo "Usage: $0 [OPTION]"
        echo ""
        echo "Options:"
        echo "  --create     Create new VM"
        echo "  --start      Start VM"
        echo "  --stop       Stop VM"
        echo "  --settings   Show settings"
        echo "  --performance Show performance options"
        echo "  --help       Show this help"
        echo ""
        echo "Without options, opens interactive menu."
        echo ""
        ;;
    *) main_loop ;;
esac
