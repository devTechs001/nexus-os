#!/bin/bash
#
# NEXUS OS - Shell Aliases and Helper Functions
# Copyright (c) 2026 NEXUS Development Team
#
# Usage: Add to your ~/.bashrc or ~/.zshrc:
#   source /path/to/nexus-os/tools/nexus-aliases.sh
#

#===========================================================================
#                         CONFIGURATION
#===========================================================================

# NEXUS OS directory (auto-detect if not set)
if [ -z "$NEXUS_OS_DIR" ]; then
    # Try to detect from script location
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"
fi

# Build directory
NEXUS_BUILD_DIR="$NEXUS_OS_DIR/build"

# ISO path
NEXUS_ISO="$NEXUS_BUILD_DIR/nexus-kernel.iso"

# Kernel binary
NEXUS_KERNEL="$NEXUS_BUILD_DIR/nexus-kernel.bin"

# Colors for output
NEXUS_COLOR_RESET='\033[0m'
NEXUS_COLOR_RED='\033[0;31m'
NEXUS_COLOR_GREEN='\033[0;32m'
NEXUS_COLOR_YELLOW='\033[1;33m'
NEXUS_COLOR_BLUE='\033[0;34m'
NEXUS_COLOR_CYAN='\033[0;36m'

#===========================================================================
#                         HELPER FUNCTIONS
#===========================================================================

nexus_print_header() {
    echo -e "${NEXUS_COLOR_CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  NEXUS OS - Command Line Tools"
    echo -e "${NEXUS_COLOR_RESET}"
}

nexus_print_success() {
    echo -e "${NEXUS_COLOR_GREEN}✓ $1${NEXUS_COLOR_RESET}"
}

nexus_print_error() {
    echo -e "${NEXUS_COLOR_RED}✗ $1${NEXUS_COLOR_RESET}"
}

nexus_print_warning() {
    echo -e "${NEXUS_COLOR_YELLOW}⚠ $1${NEXUS_COLOR_RESET}"
}

nexus_print_info() {
    echo -e "${NEXUS_COLOR_BLUE}ℹ $1${NEXUS_COLOR_RESET}"
}

nexus_check_dir() {
    if [ ! -d "$NEXUS_OS_DIR" ]; then
        nexus_print_error "NEXUS OS directory not found: $NEXUS_OS_DIR"
        return 1
    fi
    return 0
}

nexus_check_build() {
    if [ ! -f "$NEXUS_KERNEL" ]; then
        nexus_print_warning "Kernel not built yet. Run 'nexus-build' first."
        return 1
    fi
    return 0
}

nexus_check_iso() {
    if [ ! -f "$NEXUS_ISO" ]; then
        nexus_print_warning "ISO not built yet. Run 'nexus-build' first."
        return 1
    fi
    return 0
}

nexus_check_qemu() {
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        nexus_print_error "QEMU not installed. Install with:"
        echo "  sudo apt-get install qemu-system-x86"
        return 1
    fi
    return 0
}

nexus_check_vmware() {
    if ! command -v vmware &> /dev/null && \
       ! command -v vmplayer &> /dev/null; then
        nexus_print_warning "VMware not found. Install from:"
        echo "  https://www.vmware.com/products/workstation-player.html"
        return 1
    fi
    return 0
}

#===========================================================================
#                         BUILD ALIASES
#===========================================================================

# Build the kernel and ISO
nexus-build() {
    nexus_print_header
    nexus_check_dir || return 1
    
    nexus_print_info "Building NEXUS OS..."
    cd "$NEXUS_OS_DIR" && make
    
    if [ $? -eq 0 ]; then
        nexus_print_success "Build complete!"
        nexus_print_info "ISO: $NEXUS_ISO"
        ls -lh "$NEXUS_ISO" 2>/dev/null
    else
        nexus_print_error "Build failed!"
        return 1
    fi
}

# Clean build files
nexus-clean() {
    nexus_check_dir || return 1
    
    nexus_print_info "Cleaning build files..."
    cd "$NEXUS_OS_DIR" && make clean
    
    if [ $? -eq 0 ]; then
        nexus_print_success "Clean complete!"
    else
        nexus_print_error "Clean failed!"
        return 1
    fi
}

# Rebuild from scratch
nexus-rebuild() {
    nexus-clean
    nexus-build
}

# Show build status
nexus-status() {
    nexus_print_header
    nexus_check_dir || return 1
    
    echo ""
    echo "Build Status:"
    echo "============="
    
    if [ -f "$NEXUS_KERNEL" ]; then
        nexus_print_success "Kernel binary exists"
        ls -lh "$NEXUS_KERNEL"
    else
        nexus_print_error "Kernel binary not found"
    fi
    
    echo ""
    
    if [ -f "$NEXUS_ISO" ]; then
        nexus_print_success "ISO exists"
        ls -lh "$NEXUS_ISO"
    else
        nexus_print_error "ISO not found"
    fi
    
    echo ""
    echo "Directory: $NEXUS_OS_DIR"
    echo ""
}

#===========================================================================
#                         RUN ALIASES
#===========================================================================

# Run in QEMU (basic)
nexus-run() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    nexus_check_qemu || return 1
    
    nexus_print_info "Starting NEXUS OS in QEMU..."
    echo ""
    
    qemu-system-x86_64 \
        -cdrom "$NEXUS_ISO" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS" \
        -display gtk,gl=on
}

# Run in QEMU with serial console
nexus-run-serial() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    nexus_check_qemu || return 1
    
    nexus_print_info "Starting NEXUS OS in QEMU (serial console)..."
    echo ""
    
    qemu-system-x86_64 \
        -cdrom "$NEXUS_ISO" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS" \
        -serial stdio \
        -display none
}

# Run in QEMU with debug output
nexus-run-debug() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    nexus_check_qemu || return 1
    
    nexus_print_info "Starting NEXUS OS in QEMU (debug mode)..."
    echo ""
    
    qemu-system-x86_64 \
        -cdrom "$NEXUS_ISO" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS" \
        -serial stdio \
        -d int \
        -no-reboot \
        -no-shutdown
}

# Run in QEMU with more resources
nexus-run-full() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    nexus_check_qemu || return 1
    
    nexus_print_info "Starting NEXUS OS in QEMU (full resources)..."
    echo ""
    
    qemu-system-x86_64 \
        -cdrom "$NEXUS_ISO" \
        -m 4G \
        -smp 4 \
        -name "NEXUS OS" \
        -display gtk,gl=on \
        -netdev user,id=net0 \
        -device e1000,netdev=net0
}

# Run in VMware
nexus-run-vmware() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    nexus_check_vmware || return 1
    
    nexus_print_info "Starting NEXUS OS in VMware..."
    echo ""
    
    if command -v vmplayer &> /dev/null; then
        vmplayer "$NEXUS_ISO" &
    elif command -v vmware &> /dev/null; then
        vmware "$NEXUS_ISO" &
    fi
    
    nexus_print_success "VMware started"
}

# Auto-create VM and run in VMware
nexus-run-vm() {
    nexus_print_header
    nexus_check_dir || return 1
    nexus_check_iso || return 1
    
    nexus_print_info "Auto-creating VMware VM..."
    echo ""
    
    cd "$NEXUS_OS_DIR" && ./tools/auto-vm-boot.sh
}

#===========================================================================
#                         INSTALLATION ALIASES
#===========================================================================

# Create bootable USB
nexus-create-usb() {
    local device="$1"
    
    nexus_print_header
    
    if [ -z "$device" ]; then
        nexus_print_error "Usage: nexus-create-usb /dev/sdX"
        echo ""
        echo "Available devices:"
        lsblk -d -o NAME,SIZE,TYPE,MOUNTPOINT 2>/dev/null | grep -v loop
        return 1
    fi
    
    nexus_check_iso || return 1
    
    nexus_print_warning "This will DESTROY all data on $device!"
    read -p "Are you sure? (y/N): " confirm
    
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        nexus_print_info "Cancelled"
        return 1
    fi
    
    nexus_print_info "Creating bootable USB on $device..."
    
    sudo dd if="$NEXUS_ISO" of="$device" bs=4M status=progress
    sudo sync
    
    if [ $? -eq 0 ]; then
        nexus_print_success "USB created successfully!"
    else
        nexus_print_error "Failed to create USB!"
        return 1
    fi
}

# Install QEMU dependencies
nexus-install-qemu() {
    nexus_print_header
    nexus_print_info "Installing QEMU..."
    
    if command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y qemu-system-x86 qemu-utils
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y qemu-system-x86 qemu-img
    elif command -v pacman &> /dev/null; then
        sudo pacman -S qemu-system-x86 qemu-img
    else
        nexus_print_error "Unsupported package manager"
        return 1
    fi
    
    if [ $? -eq 0 ]; then
        nexus_print_success "QEMU installed successfully!"
    else
        nexus_print_error "Failed to install QEMU!"
        return 1
    fi
}

# Install build dependencies
nexus-install-build-deps() {
    nexus_print_header
    nexus_print_info "Installing build dependencies..."
    
    if command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y \
            build-essential \
            nasm \
            grub-pc-bin \
            grub-common \
            xorriso \
            mtools
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y \
            gcc \
            nasm \
            grub2-tools \
            xorriso \
            mtools
    elif command -v pacman &> /dev/null; then
        sudo pacman -S \
            base-devel \
            nasm \
            grub \
            xorriso \
            mtools
    else
        nexus_print_error "Unsupported package manager"
        return 1
    fi
    
    if [ $? -eq 0 ]; then
        nexus_print_success "Build dependencies installed!"
    else
        nexus_print_error "Failed to install dependencies!"
        return 1
    fi
}

# Install all dependencies
nexus-install-all() {
    nexus-install-build-deps
    nexus-install-qemu
}

#===========================================================================
#                         UTILITY ALIASES
#===========================================================================

# Show help
nexus-help() {
    nexus_print_header
    
    echo "NEXUS OS Command Line Tools"
    echo "============================"
    echo ""
    echo "Build Commands:"
    echo "  nexus-build       - Build kernel and ISO"
    echo "  nexus-clean       - Clean build files"
    echo "  nexus-rebuild     - Clean and rebuild"
    echo "  nexus-status      - Show build status"
    echo ""
    echo "Run Commands:"
    echo "  nexus-run         - Run in QEMU (basic)"
    echo "  nexus-run-serial  - Run in QEMU (serial console)"
    echo "  nexus-run-debug   - Run in QEMU (debug mode)"
    echo "  nexus-run-full    - Run in QEMU (4GB RAM, 4 CPUs)"
    echo "  nexus-run-vmware  - Run in VMware"
    echo "  nexus-run-vm      - Auto-create VM and run in VMware"
    echo ""
    echo "Installation Commands:"
    echo "  nexus-create-usb  - Create bootable USB"
    echo "  nexus-install-qemu - Install QEMU"
    echo "  nexus-install-build-deps - Install build dependencies"
    echo "  nexus-install-all - Install all dependencies"
    echo ""
    echo "Utility Commands:"
    echo "  nexus-help        - Show this help"
    echo "  nexus-version     - Show version info"
    echo "  nexus-docs        - Open documentation"
    echo "  nexus-gui         - Launch GUI tools"
    echo ""
    echo "Environment:"
    echo "  NEXUS_OS_DIR      - NEXUS OS directory: $NEXUS_OS_DIR"
    echo "  NEXUS_BUILD_DIR   - Build directory: $NEXUS_BUILD_DIR"
    echo "  NEXUS_ISO         - ISO path: $NEXUS_ISO"
    echo ""
}

# Show version
nexus-version() {
    nexus_print_header
    
    echo "Version Information:"
    echo "===================="
    echo ""
    echo "NEXUS OS Version: 1.0.0 (Genesis)"
    echo "Copyright (c) 2026 NEXUS Development Team"
    echo ""
    
    if [ -f "$NEXUS_KERNEL" ]; then
        echo "Kernel Binary:"
        ls -lh "$NEXUS_KERNEL"
        echo ""
    fi
    
    if [ -f "$NEXUS_ISO" ]; then
        echo "ISO Image:"
        ls -lh "$NEXUS_ISO"
        echo ""
    fi
    
    echo "QEMU Version:"
    qemu-system-x86_64 --version 2>/dev/null | head -1
    
    echo ""
}

# Open documentation
nexus-docs() {
    nexus_print_header
    nexus_check_dir || return 1
    
    echo "Documentation Files:"
    echo "===================="
    echo ""
    
    if [ -d "$NEXUS_OS_DIR/docs" ]; then
        ls -1 "$NEXUS_OS_DIR/docs/"*.md 2>/dev/null
    fi
    
    echo ""
    echo "Quick Links:"
    echo "  - PROJECT_SUMMARY.md"
    echo "  - docs/SETUP_GUIDE.md"
    echo "  - docs/DEVELOPMENT_STATUS.md"
    echo "  - docs/GUI_IMPLEMENTATION.md"
    echo "  - docs/NETWORK_MANAGEMENT.md"
    echo ""
    
    # Try to open in browser if available
    if command -v xdg-open &> /dev/null; then
        read -p "Open documentation in browser? (y/N): " open
        if [ "$open" = "y" ] || [ "$open" = "Y" ]; then
            xdg-open "$NEXUS_OS_DIR/PROJECT_SUMMARY.md" 2>/dev/null &
        fi
    fi
}

# Launch GUI tools
nexus-gui() {
    nexus_print_header
    
    echo "GUI Tools:"
    echo "=========="
    echo ""
    echo "1. Boot Manager"
    echo "2. VM Manager"
    echo "3. File Explorer (coming soon)"
    echo "4. Terminal (coming soon)"
    echo "5. Settings (coming soon)"
    echo ""
    
    read -p "Select tool [1-5]: " choice
    
    case $choice in
        1)
            nexus_print_info "Launching Boot Manager..."
            cd "$NEXUS_OS_DIR/gui/boot-manager"
            if [ -d "build" ]; then
                ./build/nexus-boot-manager
            else
                nexus_print_error "Boot Manager not built. Build it first."
            fi
            ;;
        2)
            nexus_print_info "Launching VM Manager..."
            cd "$NEXUS_OS_DIR/gui/vm-manager"
            if [ -d "build" ]; then
                ./build/nexus-vm-manager
            else
                nexus_print_error "VM Manager not built. Build it first."
            fi
            ;;
        *)
            nexus_print_info "Tool not available yet"
            ;;
    esac
}

# Quick start (build and run)
nexus-start() {
    nexus_print_header
    nexus_print_info "Quick Start: Build and Run"
    echo ""
    
    nexus-build
    if [ $? -eq 0 ]; then
        echo ""
        nexus-run
    fi
}

# Interactive mode
nexus-interactive() {
    nexus_print_header
    
    while true; do
        echo ""
        echo "NEXUS OS Interactive Mode"
        echo "========================="
        echo ""
        echo "1. Build"
        echo "2. Clean"
        echo "3. Run in QEMU"
        echo "4. Run in QEMU (Debug)"
        echo "5. Run in VMware"
        echo "6. Create Bootable USB"
        echo "7. Show Status"
        echo "8. Show Help"
        echo "0. Exit"
        echo ""
        read -p "Select option [0-8]: " option
        
        case $option in
            1) nexus-build ;;
            2) nexus-clean ;;
            3) nexus-run ;;
            4) nexus-run-debug ;;
            5) nexus-run-vmware ;;
            6) read -p "Enter device (e.g., /dev/sdb): " dev; nexus-create-usb "$dev" ;;
            7) nexus-status ;;
            8) nexus-help ;;
            0) break ;;
            *) nexus_print_error "Invalid option" ;;
        esac
    done
}

#===========================================================================
#                         SHORTCUT ALIASES
#===========================================================================

# Short aliases for common commands
alias nb='nexus-build'
alias nc='nexus-clean'
alias nr='nexus-rebuild'
alias ns='nexus-status'
alias nrun='nexus-run'
alias nrd='nexus-run-debug'
alias nrv='nexus-run-vmware'
alias nvm='nexus-run-vm'
alias nh='nexus-help'
alias nv='nexus-version'
alias nd='nexus-docs'
alias nstart='nexus-start'
alias ni='nexus-interactive'

#===========================================================================
#                         AUTO-COMPLETION
#===========================================================================

_nexus_complete() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=( $(compgen -W "build clean rebuild status run run-serial run-debug run-full run-vmware run-vm create-usb install-qemu install-build-deps install-all help version docs gui start interactive" -- "$cur") )
}

if command -v complete &> /dev/null; then
    complete -F _nexus_complete nexus-build
    complete -F _nexus_complete nexus-run
    complete -F _nexus_complete nexus-create-usb
fi

#===========================================================================
#                         INITIALIZATION
#===========================================================================

# Print welcome message if interactive shell
if [[ $- == *i* ]]; then
    nexus_print_info "NEXUS OS tools loaded. Type 'nexus-help' for commands."
fi

# End of nexus-aliases.sh
