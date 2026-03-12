#!/bin/bash
#
# NEXUS OS - Resource Sharing Configuration Tool
# Copyright (c) 2026 NEXUS Development Team
#
# Easy configuration for host-guest resource sharing
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$HOME/.config/nexus-os"
SHARING_CONFIG="$CONFIG_DIR/sharing.conf"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ╔════════════════════════════════════════════════════════╗"
    echo "  ║      NEXUS OS - Resource Sharing Configuration        ║"
    echo "  ╚════════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }
print_step() { echo -e "${MAGENTA}▶ $1${RESET}"; }

# Detect virtualization
detect_virtualization() {
    if [ -f "/proc/cpuinfo" ]; then
        if grep -q "vmx" /proc/cpuinfo; then
            echo "vmware"
        elif grep -q "svm" /proc/cpuinfo; then
            echo "amd"
        else
            echo "native"
        fi
    else
        echo "unknown"
    fi
}

# Initialize config
init_config() {
    mkdir -p "$CONFIG_DIR"
    
    if [ ! -f "$SHARING_CONFIG" ]; then
        cat > "$SHARING_CONFIG" << EOF
# NEXUS OS Resource Sharing Configuration
# Copyright (c) 2026 NEXUS Development Team

# Virtualization Mode (auto, vmware, virtualbox, qemu, hyperv, native)
VIRTUALIZATION_MODE=auto

# Sharing Level (none, basic, enhanced, full)
SHARING_LEVEL=full

# Shared Folders
SHARED_FOLDERS_ENABLED=true
DEFAULT_GUEST_PATH=/mnt/shared

# Network
NETWORK_ENABLED=true
NETWORK_MODE=nat
PORT_FORWARDING=false

# USB Devices
USB_ENABLED=true
USB_AUTO_CONNECT=true

# Clipboard
CLIPBOARD_ENABLED=true
CLIPBOARD_HOST_TO_GUEST=true
CLIPBOARD_GUEST_TO_HOST=true

# Drag and Drop
DRAGDROP_ENABLED=true
DRAGDROP_HOST_TO_GUEST=true
DRAGDROP_GUEST_TO_HOST=true

# Display
DISPLAY_AUTO_RESIZE=true
DISPLAY_3D_ACCELERATION=true

# Performance
CACHE_ENABLED=true
CACHE_SIZE_MB=256
COMPRESSION_ENABLED=true

# Security
VERIFY_HOST=true
SANDBOX_SHARED=true
EOF
        print_success "Configuration file created: $SHARING_CONFIG"
    fi
}

# Show main menu
show_main_menu() {
    clear
    print_header
    
    echo ""
    echo "  Main Menu"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  1.  Quick Setup"
    echo "  2.  Shared Folders"
    echo "  3.  Network Sharing"
    echo "  4.  USB Devices"
    echo "  5.  Clipboard & Drag-Drop"
    echo "  6.  Display Settings"
    echo "  7.  Performance Options"
    echo "  8.  Security Settings"
    echo "  9.  View Configuration"
    echo "  10. Reset to Defaults"
    echo "  0.  Exit"
    echo ""
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
}

# Quick setup
quick_setup() {
    clear
    print_header
    
    echo ""
    echo "  Quick Setup"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  Select your virtualization environment:"
    echo ""
    echo "  1. VMware (Workstation/Player)"
    echo "  2. VirtualBox"
    echo "  3. QEMU/KVM"
    echo "  4. Hyper-V"
    echo "  5. Native (Bare Metal)"
    echo "  6. Auto-Detect"
    echo ""
    read -p "Select [1-6]: " choice
    
    case $choice in
        1)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=vmware/' "$SHARING_CONFIG"
            print_success "VMware mode configured"
            ;;
        2)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=virtualbox/' "$SHARING_CONFIG"
            print_success "VirtualBox mode configured"
            ;;
        3)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=qemu/' "$SHARING_CONFIG"
            print_success "QEMU mode configured"
            ;;
        4)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=hyperv/' "$SHARING_CONFIG"
            print_success "Hyper-V mode configured"
            ;;
        5)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=native/' "$SHARING_CONFIG"
            print_success "Native mode configured"
            ;;
        6)
            sed -i 's/VIRTUALIZATION_MODE=.*/VIRTUALIZATION_MODE=auto/' "$SHARING_CONFIG"
            print_success "Auto-detect enabled"
            ;;
        *)
            print_error "Invalid option"
            sleep 2
            return
            ;;
    esac
    
    echo ""
    print_info "Optimal settings applied for selected mode"
    sleep 2
}

# Shared folders configuration
shared_folders_menu() {
    clear
    print_header
    
    echo ""
    echo "  Shared Folders"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    
    # Read current settings
    source "$SHARING_CONFIG"
    
    echo "  Current Settings:"
    echo "  ───────────────────────────────────────────────────────"
    echo "  Enabled: $SHARED_FOLDERS_ENABLED"
    echo "  Guest Path: $DEFAULT_GUEST_PATH"
    echo ""
    echo "  1. Toggle Enabled/Disabled"
    echo "  2. Change Guest Path"
    echo "  3. Add Shared Folder"
    echo "  4. Remove Shared Folder"
    echo "  5. List Shared Folders"
    echo "  0. Back"
    echo ""
    read -p "Select [0-5]: " choice
    
    case $choice in
        1)
            if [ "$SHARED_FOLDERS_ENABLED" = "true" ]; then
                sed -i 's/SHARED_FOLDERS_ENABLED=.*/SHARED_FOLDERS_ENABLED=false/' "$SHARING_CONFIG"
                print_success "Shared folders disabled"
            else
                sed -i 's/SHARED_FOLDERS_ENABLED=.*/SHARED_FOLDERS_ENABLED=true/' "$SHARING_CONFIG"
                print_success "Shared folders enabled"
            fi
            ;;
        2)
            read -p "Enter guest path [$DEFAULT_GUEST_PATH]: " path
            path=${path:-$DEFAULT_GUEST_PATH}
            sed -i "s|DEFAULT_GUEST_PATH=.*|DEFAULT_GUEST_PATH=$path|" "$SHARING_CONFIG"
            print_success "Guest path updated"
            ;;
        3)
            read -p "Folder name: " name
            read -p "Host path: " host_path
            read -p "Guest path [$DEFAULT_GUEST_PATH/$name]: " guest_path
            guest_path=${guest_path:-$DEFAULT_GUEST_PATH/$name}
            echo "SHARED_FOLDER_$name=$host_path:$guest_path" >> "$SHARING_CONFIG"
            print_success "Shared folder added: $name"
            ;;
        4)
            read -p "Folder name to remove: " name
            sed -i "/SHARED_FOLDER_$name/d" "$SHARING_CONFIG"
            print_success "Shared folder removed: $name"
            ;;
        5)
            echo ""
            echo "  Configured Shared Folders:"
            echo "  ───────────────────────────────────────────────"
            grep "^SHARED_FOLDER_" "$SHARING_CONFIG" | while IFS='=' read -r key value; do
                name=${key#SHARED_FOLDER_}
                host_path=${value%%:*}
                guest_path=${value#*:}
                echo "    $name: $host_path -> $guest_path"
            done
            echo ""
            read -p "Press Enter to continue..."
            ;;
        0) return ;;
        *) print_error "Invalid option" ; sleep 2 ;;
    esac
    
    sleep 2
}

# Network sharing configuration
network_menu() {
    clear
    print_header
    
    echo ""
    echo "  Network Sharing"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    
    source "$SHARING_CONFIG"
    
    echo "  Current Settings:"
    echo "  ───────────────────────────────────────────────────────"
    echo "  Enabled: $NETWORK_ENABLED"
    echo "  Mode: $NETWORK_MODE"
    echo "  Port Forwarding: $PORT_FORWARDING"
    echo ""
    echo "  1. Toggle Network"
    echo "  2. Change Mode (nat/bridge/host-only)"
    echo "  3. Add Port Forward"
    echo "  4. Remove Port Forward"
    echo "  5. List Port Forwards"
    echo "  0. Back"
    echo ""
    read -p "Select [0-5]: " choice
    
    case $choice in
        1)
            if [ "$NETWORK_ENABLED" = "true" ]; then
                sed -i 's/NETWORK_ENABLED=.*/NETWORK_ENABLED=false/' "$SHARING_CONFIG"
                print_success "Network sharing disabled"
            else
                sed -i 's/NETWORK_ENABLED=.*/NETWORK_ENABLED=true/' "$SHARING_CONFIG"
                print_success "Network sharing enabled"
            fi
            ;;
        2)
            echo "  1. NAT (Default)"
            echo "  2. Bridged"
            echo "  3. Host-Only"
            read -p "Select [1-3]: " net_mode
            case $net_mode in
                1) sed -i 's/NETWORK_MODE=.*/NETWORK_MODE=nat/' "$SHARING_CONFIG" ;;
                2) sed -i 's/NETWORK_MODE=.*/NETWORK_MODE=bridge/' "$SHARING_CONFIG" ;;
                3) sed -i 's/NETWORK_MODE=.*/NETWORK_MODE=host-only/' "$SHARING_CONFIG" ;;
            esac
            print_success "Network mode updated"
            ;;
        3)
            read -p "Host port: " host_port
            read -p "Guest port: " guest_port
            read -p "Protocol (tcp/udp) [tcp]: " proto
            proto=${proto:-tcp}
            echo "PORT_FORWARD_$host_port=$guest_port:$proto" >> "$SHARING_CONFIG"
            print_success "Port forward added: $host_port -> $guest_port/$proto"
            ;;
        4)
            read -p "Host port to remove: " port
            sed -i "/PORT_FORWARD_$port/d" "$SHARING_CONFIG"
            print_success "Port forward removed"
            ;;
        5)
            echo ""
            echo "  Port Forwards:"
            echo "  ───────────────────────────────────────────────"
            grep "^PORT_FORWARD_" "$SHARING_CONFIG" | while IFS='=' read -r key value; do
                port=${key#PORT_FORWARD_}
                guest_port=${value%%:*}
                proto=${value#*:}
                echo "    $port -> $guest_port/$proto"
            done
            echo ""
            read -p "Press Enter to continue..."
            ;;
        0) return ;;
        *) print_error "Invalid option" ; sleep 2 ;;
    esac
    
    sleep 2
}

# USB configuration
usb_menu() {
    clear
    print_header
    
    echo ""
    echo "  USB Devices"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    
    source "$SHARING_CONFIG"
    
    echo "  Current Settings:"
    echo "  ───────────────────────────────────────────────────────"
    echo "  USB Enabled: $USB_ENABLED"
    echo "  Auto-Connect: $USB_AUTO_CONNECT"
    echo ""
    echo "  1. Toggle USB"
    echo "  2. Toggle Auto-Connect"
    echo "  3. Add USB Device Filter"
    echo "  4. Remove USB Device Filter"
    echo "  5. List USB Devices"
    echo "  0. Back"
    echo ""
    read -p "Select [0-5]: " choice
    
    case $choice in
        1)
            if [ "$USB_ENABLED" = "true" ]; then
                sed -i 's/USB_ENABLED=.*/USB_ENABLED=false/' "$SHARING_CONFIG"
                print_success "USB sharing disabled"
            else
                sed -i 's/USB_ENABLED=.*/USB_ENABLED=true/' "$SHARING_CONFIG"
                print_success "USB sharing enabled"
            fi
            ;;
        2)
            if [ "$USB_AUTO_CONNECT" = "true" ]; then
                sed -i 's/USB_AUTO_CONNECT=.*/USB_AUTO_CONNECT=false/' "$SHARING_CONFIG"
                print_success "Auto-connect disabled"
            else
                sed -i 's/USB_AUTO_CONNECT=.*/USB_AUTO_CONNECT=true/' "$SHARING_CONFIG"
                print_success "Auto-connect enabled"
            fi
            ;;
        3)
            read -p "Vendor ID (e.g., 8087): " vendor
            read -p "Product ID (e.g., 0024): " product
            echo "USB_DEVICE_$vendor:$product=enabled" >> "$SHARING_CONFIG"
            print_success "USB device filter added: $vendor:$product"
            ;;
        4)
            read -p "Vendor:Product to remove: " device
            sed -i "/USB_DEVICE_$device/d" "$SHARING_CONFIG"
            print_success "USB device filter removed"
            ;;
        5)
            echo ""
            echo "  USB Device Filters:"
            echo "  ───────────────────────────────────────────────"
            grep "^USB_DEVICE_" "$SHARING_CONFIG" | while IFS='=' read -r key value; do
                device=${key#USB_DEVICE_}
                echo "    $device: $value"
            done
            echo ""
            read -p "Press Enter to continue..."
            ;;
        0) return ;;
        *) print_error "Invalid option" ; sleep 2 ;;
    esac
    
    sleep 2
}

# View configuration
view_config() {
    clear
    print_header
    
    echo ""
    echo "  Current Configuration"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    
    if [ -f "$SHARING_CONFIG" ]; then
        cat "$SHARING_CONFIG"
    else
        print_warning "Configuration file not found"
    fi
    
    echo ""
    read -p "Press Enter to continue..."
}

# Reset to defaults
reset_defaults() {
    print_warning "Reset all settings to defaults?"
    read -p "Continue? (y/N): " confirm
    
    if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
        rm -f "$SHARING_CONFIG"
        init_config
        print_success "Configuration reset to defaults"
        sleep 2
    fi
}

# Main loop
main_loop() {
    init_config
    
    while true; do
        show_main_menu
        read -p "Select option [0-10]: " choice
        
        case $choice in
            1) quick_setup ;;
            2) shared_folders_menu ;;
            3) network_menu ;;
            4) usb_menu ;;
            5)
                print_info "Clipboard & Drag-Drop (coming soon)"
                sleep 2
                ;;
            6)
                print_info "Display Settings (coming soon)"
                sleep 2
                ;;
            7)
                print_info "Performance Options (coming soon)"
                sleep 2
                ;;
            8)
                print_info "Security Settings (coming soon)"
                sleep 2
                ;;
            9) view_config ;;
            10) reset_defaults ;;
            0) exit 0 ;;
            *) print_error "Invalid option" ; sleep 2 ;;
        esac
    done
}

# Handle arguments
case "${1:-}" in
    --quick|-q) quick_setup ;;
    --folders|-f) shared_folders_menu ;;
    --network|-n) network_menu ;;
    --usb|-u) usb_menu ;;
    --view|-v) view_config ;;
    --reset|-r) reset_defaults ;;
    --help|-h)
        print_header
        echo ""
        echo "  Usage: $0 [OPTION]"
        echo ""
        echo "  Options:"
        echo "    --quick, -q     Quick setup"
        echo "    --folders, -f   Shared folders"
        echo "    --network, -n   Network sharing"
        echo "    --usb, -u       USB devices"
        echo "    --view, -v      View configuration"
        echo "    --reset, -r     Reset to defaults"
        echo "    --help, -h      Show this help"
        echo ""
        echo "  Without options, opens interactive menu."
        echo ""
        ;;
    *) main_loop ;;
esac
