#!/bin/bash
#
# NEXUS OS - Boot Test Script
# Tests the boot process in QEMU with various configurations
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO_PATH="$SCRIPT_DIR/build/nexus-kernel.iso"
SERIAL_LOG="$SCRIPT_DIR/test_serial.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  Boot Test Script"
    echo -e "${RESET}"
}

print_step() { echo -e "${BLUE}▶ $1${RESET}"; }
print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }

check_iso() {
    if [ ! -f "$ISO_PATH" ]; then
        print_error "ISO not found: $ISO_PATH"
        print_step "Building..."
        cd "$SCRIPT_DIR" && make
    fi
    print_success "ISO found: $ISO_PATH"
}

verify_multiboot() {
    print_step "Verifying Multiboot2 header..."
    if grub-file --is-x86-multiboot2 "$ISO_PATH" 2>/dev/null || \
       grub-file --is-x86-multiboot2 "$SCRIPT_DIR/build/nexus-kernel.bin" 2>/dev/null; then
        print_success "Multiboot2 validation: PASSED"
    else
        print_error "Multiboot2 validation: FAILED"
        exit 1
    fi
}

test_normal() {
    print_step "Starting QEMU (Normal Boot)..."
    print_warning "Press Ctrl+A then X to exit"
    
    qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS - Normal Boot" \
        -display gtk,gl=on \
        -boot d \
        -serial file:"$SERIAL_LOG"
    
    print_success "QEMU exited normally"
}

test_debug() {
    print_step "Starting QEMU (Debug Mode with Serial Output)..."
    print_warning "Press Ctrl+A then X to exit"
    
    qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS - Debug Mode" \
        -display none \
        -serial stdio \
        -boot d \
        -append "debug loglevel=7 earlyprintk=vga"
}

test_safe() {
    print_step "Starting QEMU (Safe Mode)..."
    print_warning "Press Ctrl+A then X to exit"
    
    qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 1G \
        -smp 1 \
        -name "NEXUS OS - Safe Mode" \
        -display gtk \
        -boot d \
        -append "nomodeset nosmp noapic nolapic"
}

test_text() {
    print_step "Starting QEMU (Text Mode)..."
    print_warning "Press Ctrl+A then X to exit"
    
    qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS - Text Mode" \
        -display gtk \
        -boot d \
        -append "vga=791"
}

check_serial_log() {
    if [ -f "$SERIAL_LOG" ]; then
        print_step "Serial Log Output:"
        echo "----------------------------------------"
        cat "$SERIAL_LOG" | head -50
        echo "----------------------------------------"
        
        # Check for expected boot characters
        if grep -q "B" "$SERIAL_LOG"; then
            print_success "Boot stage 'B' (32-bit entry) found"
        fi
        if grep -q "C" "$SERIAL_LOG"; then
            print_success "Boot stage 'C' (BSS cleared) found"
        fi
        if grep -q "L" "$SERIAL_LOG"; then
            print_success "Boot stage 'L' (Long mode) found"
        fi
        if grep -q "P" "$SERIAL_LOG"; then
            print_success "Boot stage 'P' (Paging enabled) found"
        fi
        if grep -q "6" "$SERIAL_LOG"; then
            print_success "Boot stage '6' (64-bit mode) found"
        fi
        if grep -q "S" "$SERIAL_LOG"; then
            print_success "Boot stage 'S' (Stack ready) found"
        fi
        if grep -q "I" "$SERIAL_LOG"; then
            print_success "Boot stage 'I' (Info passed to kernel) found"
        fi
    fi
}

show_help() {
    echo ""
    echo "  Usage: $0 [OPTION]"
    echo ""
    echo "  Boot Test Options:"
    echo "    --normal, -n      Normal boot (default)"
    echo "    --debug, -d       Debug mode with serial output"
    echo "    --safe, -s        Safe mode (minimal hardware)"
    echo "    --text, -t        Text mode (VGA console)"
    echo "    --verify, -v      Verify multiboot header only"
    echo "    --log, -l         Check serial log"
    echo "    --help, -h        Show this help"
    echo ""
    echo "  Examples:"
    echo "    $0                # Normal boot test"
    echo "    $0 --debug        # Debug mode with serial"
    echo "    $0 --verify       # Verify multiboot header"
    echo "    $0 --log          # Check serial log"
    echo ""
}

# Main
main() {
    print_header
    
    case "${1:-}" in
        --normal|-n)
            check_iso
            verify_multiboot
            test_normal
            check_serial_log
            ;;
        
        --debug|-d)
            check_iso
            verify_multiboot
            test_debug
            ;;
        
        --safe|-s)
            check_iso
            verify_multiboot
            test_safe
            ;;
        
        --text|-t)
            check_iso
            verify_multiboot
            test_text
            ;;
        
        --verify|-v)
            verify_multiboot
            ;;
        
        --log|-l)
            check_serial_log
            ;;
        
        --help|-h)
            show_help
            exit 0
            ;;
        
        *)
            # Default: normal boot
            check_iso
            verify_multiboot
            test_normal
            check_serial_log
            ;;
    esac
    
    print_success "Boot test complete!"
}

main "$@"
