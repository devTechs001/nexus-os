#!/bin/bash
#
# NEXUS OS - VM Run Diagnostic Script
# Captures boot process and shows what's on screen
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO_PATH="$PROJECT_DIR/build/nexus-kernel.iso"
DIAG_DIR="$PROJECT_DIR/vm_diagnostics"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ╔══════════════════════════════════════════════════════╗"
    echo "  ║        NEXUS OS VM Run Diagnostics                   ║"
    echo "  ╚══════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
}

print_status() { echo -e "${BLUE}[STATUS]${RESET} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${RESET} $1"; }
print_error() { echo -e "${RED}[ERROR]${RESET} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${RESET} $1"; }
print_info() { echo -e "${CYAN}[INFO]${RESET} $1"; }

setup() {
    mkdir -p "$DIAG_DIR"
    rm -f "$DIAG_DIR"/*.log "$DIAG_DIR"/*.ppm 2>/dev/null || true
}

check_prerequisites() {
    print_status "Checking prerequisites..."
    
    if [ ! -f "$ISO_PATH" ]; then
        print_error "ISO not found: $ISO_PATH"
        print_status "Building..."
        cd "$PROJECT_DIR" && make -j$(nproc)
    fi
    
    print_success "ISO found ($(ls -lh "$ISO_PATH" | awk '{print $5}'))"

    if grub-file --is-x86-multiboot2 "$PROJECT_DIR/build/nexus-kernel.bin" 2>/dev/null; then
        print_success "Multiboot2: VALIDATED"
    else
        print_error "Multiboot2: FAILED"
        exit 1
    fi
}

run_with_display() {
    local timeout=${1:-20}
    
    print_status "Starting QEMU with GTK display (${timeout}s timeout)..."
    print_info "Watch the QEMU window for the GRUB menu"
    print_info "GRUB timeout is set to 10 seconds"
    print_info ""
    print_info "Boot options:"
    print_info "  1. Graphical Mode (Default) - auto-selects after 10s"
    print_info "  2. Text Mode (VGA Console)"
    print_info "  3. Safe Mode"
    print_info "  4. Debug Mode"
    print_info "  5. Native Hardware"
    print_info ""
    print_info "Press arrow keys to select, Enter to boot"
    print_info "Press ESC to cancel timeout"
    print_info ""
    
    timeout "$timeout" qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS - Diagnostics" \
        -display gtk,gl=off \
        -no-reboot \
        -no-shutdown \
        -serial file:"$DIAG_DIR/serial.log" \
        -d guest_errors,unimp \
        -D "$DIAG_DIR/qemu_errors.log" \
        2>&1 || true
    
    print_status "QEMU session ended"
}

analyze() {
    print_status "Analyzing boot process..."
    
    echo "NEXUS OS VM Run Diagnostics" > "$DIAG_DIR/analysis.log"
    echo "===========================" >> "$DIAG_DIR/analysis.log"
    echo "Date: $(date)" >> "$DIAG_DIR/analysis.log"
    echo "" >> "$DIAG_DIR/analysis.log"
    
    # Check serial output
    if [ -f "$DIAG_DIR/serial.log" ] && [ -s "$DIAG_DIR/serial.log" ]; then
        local serial_size=$(wc -c < "$DIAG_DIR/serial.log")
        print_info "Serial log: $serial_size bytes"
        echo "Serial log: $serial_size bytes" >> "$DIAG_DIR/analysis.log"
        
        # Check for boot markers
        local serial_content=$(cat "$DIAG_DIR/serial.log" 2>/dev/null | tr -d '\n')
        
        if [[ "$serial_content" == *"B"* ]]; then
            print_success "  ✓ Boot stage B: 32-bit entry"
        fi
        if [[ "$serial_content" == *"C"* ]]; then
            print_success "  ✓ Boot stage C: BSS cleared"
        fi
        if [[ "$serial_content" == *"L"* ]]; then
            print_success "  ✓ Boot stage L: Long mode"
        fi
        if [[ "$serial_content" == *"P"* ]]; then
            print_success "  ✓ Boot stage P: Paging"
        fi
        if [[ "$serial_content" == *"6"* ]]; then
            print_success "  ✓ Boot stage 6: 64-bit mode"
        fi
        if [[ "$serial_content" == *"S"* ]]; then
            print_success "  ✓ Boot stage S: Stack ready"
        fi
        if [[ "$serial_content" == *"I"* ]]; then
            print_success "  ✓ Boot stage I: Info to kernel"
        fi
    else
        print_warning "No serial output (kernel may not have serial init yet)"
        echo "No serial output" >> "$DIAG_DIR/analysis.log"
    fi
    
    # Check QEMU errors
    if [ -f "$DIAG_DIR/qemu_errors.log" ] && [ -s "$DIAG_DIR/qemu_errors.log" ]; then
        local error_count=$(wc -l < "$DIAG_DIR/qemu_errors.log")
        print_info "QEMU error log: $error_count lines"
        echo "" >> "$DIAG_DIR/analysis.log"
        echo "QEMU Errors:" >> "$DIAG_DIR/analysis.log"
        tail -20 "$DIAG_DIR/qemu_errors.log" >> "$DIAG_DIR/analysis.log"
    fi
    
    # Summary
    echo "" >> "$DIAG_DIR/analysis.log"
    echo "Summary:" >> "$DIAG_DIR/analysis.log"
    echo "  ISO: $ISO_PATH" >> "$DIAG_DIR/analysis.log"
    echo "  Multiboot2: Valid" >> "$DIAG_DIR/analysis.log"
    echo "  QEMU: Completed" >> "$DIAG_DIR/analysis.log"
}

show_summary() {
    print_status "Diagnostics Summary"
    echo ""
    echo "═══════════════════════════════════════════════════"
    cat "$DIAG_DIR/analysis.log"
    echo "═══════════════════════════════════════════════════"
    echo ""
    print_info "Log files saved to: $DIAG_DIR/"
    echo "  - serial.log"
    echo "  - qemu_errors.log"
    echo "  - analysis.log"
    echo ""
    
    # Show serial content if any
    if [ -f "$DIAG_DIR/serial.log" ] && [ -s "$DIAG_DIR/serial.log" ]; then
        print_status "Serial Output:"
        cat "$DIAG_DIR/serial.log" | od -c | head -20
    fi
}

# Main
main() {
    print_header
    setup
    check_prerequisites
    run_with_display "${1:-20}"
    analyze
    show_summary
    
    print_success "Diagnostics complete!"
}

main "$@"
