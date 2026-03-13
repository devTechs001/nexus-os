#!/bin/bash
#
# NEXUS OS - Real-Time Boot Diagnostics Capture
# Captures boot output and analyzes for failures
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO_PATH="$SCRIPT_DIR/build/nexus-kernel.iso"
DIAG_DIR="$SCRIPT_DIR/boot_diagnostics"
SERIAL_LOG="$DIAG_DIR/serial_capture.log"
VGA_LOG="$DIAG_DIR/vga_capture.log"
ANALYSIS_LOG="$DIAG_DIR/boot_analysis.log"

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
    echo "  ║     NEXUS OS Boot Diagnostics Capture System         ║"
    echo "  ╚══════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
}

print_status() { echo -e "${BLUE}[STATUS]${RESET} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${RESET} $1"; }
print_error() { echo -e "${RED}[ERROR]${RESET} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${RESET} $1"; }
print_info() { echo -e "${CYAN}[INFO]${RESET} $1"; }

setup_diag_dir() {
    print_status "Setting up diagnostics directory..."
    mkdir -p "$DIAG_DIR"
    
    # Clear old logs
    rm -f "$SERIAL_LOG" "$VGA_LOG" "$ANALYSIS_LOG"
    
    # Create timestamp
    echo "Boot Diagnostics Capture - $(date)" > "$ANALYSIS_LOG"
    echo "==========================================" >> "$ANALYSIS_LOG"
}

check_iso() {
    if [ ! -f "$ISO_PATH" ]; then
        print_error "ISO not found: $ISO_PATH"
        print_status "Building..."
        cd "$SCRIPT_DIR" && make -j$(nproc)
    fi
    print_success "ISO found: $ISO_PATH ($(ls -lh "$ISO_PATH" | awk '{print $5}'))"
}

verify_multiboot() {
    print_status "Verifying Multiboot2 header..."
    if grub-file --is-x86-multiboot2 "$ISO_PATH" 2>/dev/null || \
       grub-file --is-x86-multiboot2 "$SCRIPT_DIR/build/nexus-kernel.bin" 2>/dev/null; then
        print_success "Multiboot2 validation: PASSED"
        echo "Multiboot2: PASSED" >> "$ANALYSIS_LOG"
    else
        print_error "Multiboot2 validation: FAILED"
        echo "Multiboot2: FAILED" >> "$ANALYSIS_LOG"
        exit 1
    fi
}

capture_boot() {
    local timeout=${1:-15}
    
    print_status "Starting QEMU with diagnostics capture (timeout: ${timeout}s)..."
    print_info "Capturing serial output to: $SERIAL_LOG"
    
    # Create named pipe for real-time capture
    local fifo="$DIAG_DIR/serial_fifo"
    rm -f "$fifo"
    mkfifo "$fifo"
    
    # Start QEMU with multiple output methods
    timeout "$timeout" qemu-system-x86_64 \
        -cdrom "$ISO_PATH" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS - Diagnostics Capture" \
        -display none \
        -serial file:"$SERIAL_LOG" \
        -serial unix:"$DIAG_DIR/serial.sock",server=on,wait=off \
        -no-reboot \
        -no-shutdown \
        -d guest_errors,unimp \
        -D "$DIAG_DIR/qemu_errors.log" \
        2>&1 | tee -a "$VGA_LOG" &
    
    local qemu_pid=$!
    
    # Monitor serial output in real-time
    print_status "Monitoring boot progress..."
    local start_time=$(date +%s)
    local boot_detected=false
    local serial_chars=""
    
    while kill -0 $qemu_pid 2>/dev/null; do
        local elapsed=$(($(date +%s) - start_time))
        
        # Check for serial output
        if [ -f "$SERIAL_LOG" ] && [ -s "$SERIAL_LOG" ]; then
            serial_chars=$(cat "$SERIAL_LOG" 2>/dev/null | tr -d '\n')
            
            # Check for boot stage markers
            if [[ "$serial_chars" == *"B"* ]]; then
                print_success "Boot stage 'B' detected (32-bit entry)"
                boot_detected=true
            fi
            if [[ "$serial_chars" == *"C"* ]]; then
                print_success "Boot stage 'C' detected (BSS cleared)"
            fi
            if [[ "$serial_chars" == *"L"* ]]; then
                print_success "Boot stage 'L' detected (Long mode)"
            fi
            if [[ "$serial_chars" == *"P"* ]]; then
                print_success "Boot stage 'P' detected (Paging enabled)"
            fi
            if [[ "$serial_chars" == *"6"* ]]; then
                print_success "Boot stage '6' detected (64-bit mode)"
            fi
            if [[ "$serial_chars" == *"S"* ]]; then
                print_success "Boot stage 'S' detected (Stack ready)"
            fi
            if [[ "$serial_chars" == *"I"* ]]; then
                print_success "Boot stage 'I' detected (Info to kernel)"
            fi
            
            # Check for panic or error messages
            if [[ "$serial_chars" == *"PANIC"* ]] || [[ "$serial_chars" == *"panic"* ]]; then
                print_error "KERNEL PANIC detected!"
                echo "PANIC detected at ${elapsed}s" >> "$ANALYSIS_LOG"
            fi
        fi
        
        # Timeout check
        if [ $elapsed -ge $timeout ]; then
            print_warning "Timeout reached"
            break
        fi
        
        sleep 0.5
    done
    
    # Cleanup
    kill $qemu_pid 2>/dev/null || true
    rm -f "$fifo"
    
    # Record results
    echo "" >> "$ANALYSIS_LOG"
    echo "Boot Capture Results:" >> "$ANALYSIS_LOG"
    echo "  Duration: ${elapsed}s" >> "$ANALYSIS_LOG"
    echo "  Boot detected: ${boot_detected}" >> "$ANALYSIS_LOG"
    echo "  Serial output: $(wc -c < "$SERIAL_LOG" 2>/dev/null || echo 0) bytes" >> "$ANALYSIS_LOG"
}

analyze_boot() {
    print_status "Analyzing boot capture..."
    
    echo "" >> "$ANALYSIS_LOG"
    echo "Boot Analysis:" >> "$ANALYSIS_LOG"
    echo "=============" >> "$ANALYSIS_LOG"
    
    # Check serial log
    if [ -f "$SERIAL_LOG" ] && [ -s "$SERIAL_LOG" ]; then
        local serial_content=$(cat "$SERIAL_LOG" 2>/dev/null | tr -d '\n')
        
        print_info "Serial log: $(wc -c < "$SERIAL_LOG") bytes"
        echo "Serial log: $(wc -c < "$SERIAL_LOG") bytes" >> "$ANALYSIS_LOG"
        
        # Check for boot stages
        local stages_found=0
        
        if [[ "$serial_content" == *"B"* ]]; then
            print_success "  ✓ Stage B: 32-bit entry point"
            echo "  ✓ Stage B: 32-bit entry point" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"C"* ]]; then
            print_success "  ✓ Stage C: BSS cleared"
            echo "  ✓ Stage C: BSS cleared" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"L"* ]]; then
            print_success "  ✓ Stage L: Long mode supported"
            echo "  ✓ Stage L: Long mode supported" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"P"* ]]; then
            print_success "  ✓ Stage P: Paging enabled"
            echo "  ✓ Stage P: Paging enabled" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"6"* ]]; then
            print_success "  ✓ Stage 6: 64-bit mode entered"
            echo "  ✓ Stage 6: 64-bit mode entered" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"S"* ]]; then
            print_success "  ✓ Stage S: Stack ready"
            echo "  ✓ Stage S: Stack ready" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        if [[ "$serial_content" == *"I"* ]]; then
            print_success "  ✓ Stage I: Multiboot info to kernel"
            echo "  ✓ Stage I: Multiboot info to kernel" >> "$ANALYSIS_LOG"
            ((stages_found++))
        fi
        
        print_info "Boot stages found: $stages_found/7"
        echo "Boot stages found: $stages_found/7" >> "$ANALYSIS_LOG"
        
        # Check for errors
        if [[ "$serial_content" == *"M"* ]]; then
            print_error "  ✗ Stage M: Multiboot magic error!"
            echo "  ✗ Stage M: Multiboot magic error!" >> "$ANALYSIS_LOG"
        fi
        
        if [[ "$serial_content" == *"X"* ]]; then
            print_error "  ✗ Stage X: Long mode not supported!"
            echo "  ✗ Stage X: Long mode not supported!" >> "$ANALYSIS_LOG"
        fi
    else
        print_warning "No serial output captured"
        echo "No serial output captured" >> "$ANALYSIS_LOG"
    fi
    
    # Check QEMU error log
    if [ -f "$DIAG_DIR/qemu_errors.log" ] && [ -s "$DIAG_DIR/qemu_errors.log" ]; then
        print_info "QEMU errors: $(wc -l < "$DIAG_DIR/qemu_errors.log") lines"
        echo "" >> "$ANALYSIS_LOG"
        echo "QEMU Error Log:" >> "$ANALYSIS_LOG"
        tail -20 "$DIAG_DIR/qemu_errors.log" >> "$ANALYSIS_LOG"
    fi
}

generate_report() {
    print_status "Generating boot diagnostics report..."
    
    {
        echo ""
        echo "═══════════════════════════════════════════════════"
        echo "     NEXUS OS BOOT DIAGNOSTICS REPORT"
        echo "═══════════════════════════════════════════════════"
        echo "Generated: $(date)"
        echo ""
        cat "$ANALYSIS_LOG"
        echo ""
        echo "═══════════════════════════════════════════════════"
        echo "Files:"
        echo "  Serial log: $SERIAL_LOG"
        echo "  VGA log: $VGA_LOG"
        echo "  QEMU errors: $DIAG_DIR/qemu_errors.log"
        echo "  Analysis: $ANALYSIS_LOG"
        echo "═══════════════════════════════════════════════════"
    } | tee -a "$ANALYSIS_LOG"
}

# Main
main() {
    print_header
    
    setup_diag_dir
    check_iso
    verify_multiboot
    capture_boot "${1:-15}"
    analyze_boot
    generate_report
    
    print_success "Diagnostics capture complete!"
    print_info "Report saved to: $ANALYSIS_LOG"
}

main "$@"
