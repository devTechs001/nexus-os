#!/bin/bash
#
# NEXUS OS - VM Settings and Preferences
# Copyright (c) 2026 NEXUS Development Team
#
# Configure VM preferences and optimizations
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$HOME/.config/nexus-os/vm-config.conf"

# Colors
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ╔════════════════════════════════════════════════════════╗"
    echo "  ║         NEXUS OS - VM Settings & Preferences          ║"
    echo "  ╚════════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }

# Load config
load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
    else
        # Defaults
        RAM_SIZE=2048
        CPU_COUNT=2
        DISK_SIZE="20G"
        NETWORK_TYPE="nat"
        DISPLAY_MEMORY=128
        USB_ENABLED=true
        ACCELERATE_3D=true
    fi
}

# Show settings
show_settings() {
    clear
    print_header
    
    load_config
    
    echo ""
    echo "  Current Settings"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │  HARDWARE                                           │"
    echo "  ├─────────────────────────────────────────────────────┤"
    printf "  │  %-20s %35s │\n" "Memory:" "${RAM_SIZE}MB"
    printf "  │  %-20s %35s │\n" "CPUs:" "${CPU_COUNT}"
    printf "  │  %-20s %35s │\n" "Disk Size:" "${DISK_SIZE}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │  NETWORK                                            │"
    echo "  ├─────────────────────────────────────────────────────┤"
    printf "  │  %-20s %35s │\n" "Network Type:" "${NETWORK_TYPE}"
    printf "  │  %-20s %35s │\n" "Bridge:" "${NETWORK_BRIDGE:-Not set}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │  DISPLAY & GRAPHICS                                 │"
    echo "  ├─────────────────────────────────────────────────────┤"
    printf "  │  %-20s %35s │\n" "Video Memory:" "${DISPLAY_MEMORY}MB"
    printf "  │  %-20s %35s │\n" "3D Acceleration:" "${ACCELERATE_3D}"
    printf "  │  %-20s %35s │\n" "2D Acceleration:" "${ACCELERATE_2D:-true}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │  DEVICES                                            │"
    echo "  ├─────────────────────────────────────────────────────┤"
    printf "  │  %-20s %35s │\n" "USB Enabled:" "${USB_ENABLED}"
    printf "  │  %-20s %35s │\n" "USB Version:" "${USB_VERSION:-3.0}"
    printf "  │  %-20s %35s │\n" "Sound:" "${SOUND_ENABLED:-false}"
    printf "  │  %-20s %35s │\n" "Shared Folders:" "${SHARED_FOLDERS:-enabled}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │  PERFORMANCE                                        │"
    echo "  ├─────────────────────────────────────────────────────┤"
    printf "  │  %-20s %35s │\n" "Hardware Virt:" "${HARDWARE_VIRTUALIZATION:-true}"
    printf "  │  %-20s %35s │\n" "Nested Virt:" "${NESTED_VIRTUALIZATION:-false}"
    printf "  │  %-20s %35s │\n" "IOMMU:" "${IOMMU:-enabled}"
    printf "  │  %-20s %35s │\n" "Memory Balloon:" "${MEMORY_BALLOON:-enabled}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
}

# Edit setting
edit_setting() {
    clear
    print_header
    
    echo ""
    echo "  Edit Setting"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  1.  Memory (RAM)"
    echo "  2.  CPU Count"
    echo "  3.  Disk Size"
    echo "  4.  Network Type"
    echo "  5.  Video Memory"
    echo "  6.  USB Settings"
    echo "  7.  3D Acceleration"
    echo "  8.  Hardware Virtualization"
    echo "  9.  Nested Virtualization"
    echo "  10. Shared Folders"
    echo "  0.  Back"
    echo ""
    read -p "Select [0-10]: " choice
    
    load_config
    
    case $choice in
        1)
            read -p "Enter Memory (MB) [$RAM_SIZE]: " val
            RAM_SIZE=${val:-$RAM_SIZE}
            ;;
        2)
            read -p "Enter CPU Count [$CPU_COUNT]: " val
            CPU_COUNT=${val:-$CPU_COUNT}
            ;;
        3)
            read -p "Enter Disk Size (e.g., 20G) [$DISK_SIZE]: " val
            DISK_SIZE=${val:-$DISK_SIZE}
            ;;
        4)
            echo "  1. NAT  2. Bridged  3. Host-Only  4. None"
            read -p "Select [1-4]: " val
            case $val in
                1) NETWORK_TYPE="nat" ;;
                2) NETWORK_TYPE="bridged" ;;
                3) NETWORK_TYPE="hostonly" ;;
                4) NETWORK_TYPE="none" ;;
            esac
            ;;
        5)
            read -p "Enter Video Memory (MB) [$DISPLAY_MEMORY]: " val
            DISPLAY_MEMORY=${val:-$DISPLAY_MEMORY}
            ;;
        6)
            read -p "USB Enabled? (true/false) [$USB_ENABLED]: " val
            USB_ENABLED=${val:-$USB_ENABLED}
            read -p "USB Version? (2.0/3.0) [$USB_VERSION]: " val
            USB_VERSION=${val:-$USB_VERSION}
            ;;
        7)
            read -p "3D Acceleration? (true/false) [$ACCELERATE_3D]: " val
            ACCELERATE_3D=${val:-$ACCELERATE_3D}
            ;;
        8)
            read -p "Hardware Virtualization? (true/false) [$HARDWARE_VIRTUALIZATION]: " val
            HARDWARE_VIRTUALIZATION=${val:-$HARDWARE_VIRTUALIZATION}
            ;;
        9)
            read -p "Nested Virtualization? (true/false) [$NESTED_VIRTUALIZATION]: " val
            NESTED_VIRTUALIZATION=${val:-$NESTED_VIRTUALIZATION}
            ;;
        10)
            read -p "Shared Folders? (enabled/disabled) [$SHARED_FOLDERS]: " val
            SHARED_FOLDERS=${val:-$SHARED_FOLDERS}
            ;;
        0) return ;;
        *) print_error "Invalid option" ; sleep 2 ; return ;;
    esac
    
    save_config
    print_success "Setting updated!"
    sleep 2
}

# Save config
save_config() {
    mkdir -p "$(dirname "$CONFIG_FILE")"
    
    cat > "$CONFIG_FILE" << EOF
# NEXUS OS VM Configuration
# Copyright (c) 2026 NEXUS Development Team

# Memory Settings
RAM_SIZE=$RAM_SIZE
MEMORY_BALLOON=${MEMORY_BALLOON:-enabled}

# CPU Settings
CPU_COUNT=$CPU_COUNT
CPU_CORES=2
CPU_THREADS=1
VIRTUALIZATION=enabled

# Display Settings
DISPLAY_MEMORY=$DISPLAY_MEMORY
ACCELERATE_3D=$ACCELERATE_3D
ACCELERATE_2D=${ACCELERATE_2D:-true}

# Network Settings
NETWORK_TYPE=$NETWORK_TYPE
NETWORK_BRIDGE=${NETWORK_BRIDGE:-}
NETWORK_HOSTONLY=${NETWORK_HOSTONLY:-}

# Storage Settings
DISK_SIZE=$DISK_SIZE
DISK_TYPE=${DISK_TYPE:-thin}
CDROM_AUTOCONNECT=${CDROM_AUTOCONNECT:-true}

# USB Settings
USB_ENABLED=$USB_ENABLED
USB_VERSION=$USB_VERSION
SOUND_ENABLED=${SOUND_ENABLED:-false}
PRINTER_ENABLED=${PRINTER_ENABLED:-false}
SHARED_FOLDERS=$SHARED_FOLDERS
DRAG_DROP=${DRAG_DROP:-enabled}
COPY_PASTE=${COPY_PASTE:-enabled}

# Performance
HARDWARE_VIRTUALIZATION=$HARDWARE_VIRTUALIZATION
NESTED_VIRTUALIZATION=$NESTED_VIRTUALIZATION
IOMMU=${IOMMU:-enabled}
SR_IOV=${SR_IOV:-disabled}

# Snapshots
AUTO_SNAPSHOT=${AUTO_SNAPSHOT:-false}
SNAPSHOT_INTERVAL=${SNAPSHOT_INTERVAL:-3600}

# Boot Options
BOOT_ORDER=${BOOT_ORDER:-cdrom}
BOOT_TIMEOUT=${BOOT_TIMEOUT:-10}
DEFAULT_BOOT_MODE=${DEFAULT_BOOT_MODE:-vmware}
EOF
    
    print_success "Configuration saved to: $CONFIG_FILE"
}

# Apply optimization profile
apply_profile() {
    clear
    print_header
    
    echo ""
    echo "  Optimization Profiles"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  1. Balanced (Default)"
    echo "     - 2GB RAM, 2 CPUs"
    echo "     - Good for general use"
    echo ""
    echo "  2. Performance"
    echo "     - 4GB RAM, 4 CPUs"
    echo "     - 3D acceleration enabled"
    echo "     - Best for demanding apps"
    echo ""
    echo "  3. Development"
    echo "     - 8GB RAM, 4 CPUs"
    echo "     - Nested virtualization"
    echo "     - Shared folders enabled"
    echo ""
    echo "  4. Gaming"
    echo "     - 8GB RAM, 4 CPUs"
    echo "     - Maximum 3D acceleration"
    echo "     - 256MB video memory"
    echo ""
    echo "  5. Server"
    echo "     - 4GB RAM, 8 CPUs"
    echo "     - Bridged networking"
    echo "     - Minimal devices"
    echo ""
    echo "  6. Minimal"
    echo "     - 1GB RAM, 1 CPU"
    echo "     - No acceleration"
    echo "     - Lowest resource usage"
    echo ""
    read -p "Select profile [1-6]: " profile
    
    load_config
    
    case $profile in
        1)
            RAM_SIZE=2048; CPU_COUNT=2; DISPLAY_MEMORY=128
            ACCELERATE_3D=true; NESTED_VIRTUALIZATION=false
            ;;
        2)
            RAM_SIZE=4096; CPU_COUNT=4; DISPLAY_MEMORY=128
            ACCELERATE_3D=true; NESTED_VIRTUALIZATION=true
            ;;
        3)
            RAM_SIZE=8192; CPU_COUNT=4; DISPLAY_MEMORY=128
            ACCELERATE_3D=true; NESTED_VIRTUALIZATION=true
            SHARED_FOLDERS=enabled
            ;;
        4)
            RAM_SIZE=8192; CPU_COUNT=4; DISPLAY_MEMORY=256
            ACCELERATE_3D=true; NESTED_VIRTUALIZATION=false
            ;;
        5)
            RAM_SIZE=4096; CPU_COUNT=8; DISPLAY_MEMORY=64
            NETWORK_TYPE=bridged; SOUND_ENABLED=false
            ;;
        6)
            RAM_SIZE=1024; CPU_COUNT=1; DISPLAY_MEMORY=32
            ACCELERATE_3D=false; NESTED_VIRTUALIZATION=false
            ;;
        *) print_error "Invalid profile" ; sleep 2 ; return ;;
    esac
    
    save_config
    print_success "Profile applied!"
    sleep 2
}

# Main menu
main_menu() {
    while true; do
        show_settings
        
        echo "  1.  Edit Setting"
        echo "  2.  Apply Optimization Profile"
        echo "  3.  Save Configuration"
        echo "  4.  Reset to Defaults"
        echo "  5.  Open Config File"
        echo "  0.  Exit"
        echo ""
        read -p "Select [0-5]: " choice
        
        case $choice in
            1) edit_setting ;;
            2) apply_profile ;;
            3) save_config ; sleep 2 ;;
            4)
                print_warning "Reset to defaults?"
                read -p "Continue? (y/N): " confirm
                if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
                    rm -f "$CONFIG_FILE"
                    print_success "Configuration reset"
                    sleep 2
                fi
                ;;
            5)
                if [ -f "$CONFIG_FILE" ]; then
                    ${VISUAL:-${EDITOR:-nano}} "$CONFIG_FILE"
                else
                    print_error "Config file not found"
                    sleep 2
                fi
                ;;
            0) exit 0 ;;
            *) print_error "Invalid option" ; sleep 2 ;;
        esac
    done
}

# Handle arguments
case "${1:-}" in
    --show) show_settings ; exit 0 ;;
    --edit) edit_setting ; exit 0 ;;
    --profile) apply_profile ; exit 0 ;;
    --save) save_config ; exit 0 ;;
    --reset) rm -f "$CONFIG_FILE" ; print_success "Reset complete" ; exit 0 ;;
    --help|-h)
        print_header
        echo "Usage: $0 [OPTION]"
        echo ""
        echo "Options:"
        echo "  --show     Show current settings"
        echo "  --edit     Edit a setting"
        echo "  --profile  Apply optimization profile"
        echo "  --save     Save configuration"
        echo "  --reset    Reset to defaults"
        echo "  --help     Show this help"
        echo ""
        echo "Without options, opens interactive menu."
        echo ""
        ;;
    *) main_menu ;;
esac
