#!/bin/bash
#
# NEXUS OS - Quick Start Script
# Copyright (c) 2026 NEXUS Development Team
#
# Quick start NEXUS OS in VMware or QEMU
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$NEXUS_OS_DIR/build/nexus-kernel.iso"
VM_DIR="$HOME/vmware-vm/NEXUS-OS"
VMX_FILE="$VM_DIR/NEXUS-OS.vmx"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
RESET='\033[0m'

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }

# Check ISO
if [ ! -f "$ISO" ]; then
    print_error "ISO not found. Building..."
    cd "$NEXUS_OS_DIR" && make
    if [ $? -ne 0 ]; then
        print_error "Build failed!"
        exit 1
    fi
fi

# Check for VMware
if command -v vmplayer &> /dev/null || command -v vmware &> /dev/null; then
    print_info "VMware detected"
    
    # Create VM if not exists
    if [ ! -f "$VMX_FILE" ]; then
        print_info "Creating VM..."
        "$SCRIPT_DIR/nexus-advanced-vm-manager.sh" --create
    fi
    
    print_success "Starting NEXUS OS in VMware..."
    
    if command -v vmplayer &> /dev/null; then
        vmplayer "$VMX_FILE" &
    else
        vmware "$VMX_FILE" &
    fi
    
elif command -v qemu-system-x86_64 &> /dev/null; then
    print_warning "VMware not found, using QEMU"
    print_info "Starting NEXUS OS in QEMU..."
    
    qemu-system-x86_64 \
        -cdrom "$ISO" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS" \
        -display gtk,gl=on &
else
    print_error "Neither VMware nor QEMU found"
    print_info "Install QEMU: sudo apt-get install qemu-system-x86"
    exit 1
fi

print_success "NEXUS OS started!"
