#!/bin/bash
#
# NEXUS OS - Setup Script
# Copyright (c) 2026 NEXUS Development Team
#
# This script installs aliases and sets up the development environment
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"
ALIASES_FILE="$SCRIPT_DIR/nexus-aliases.sh"

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
    echo "  NEXUS OS - Setup Script"
    echo -e "${RESET}"
}

print_success() {
    echo -e "${GREEN}✓ $1${RESET}"
}

print_error() {
    echo -e "${RED}✗ $1${RESET}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${RESET}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${RESET}"
}

# Detect shell
detect_shell() {
    if [ -n "$ZSH_VERSION" ]; then
        SHELL_RC="$HOME/.zshrc"
        SHELL_NAME="zsh"
    elif [ -n "$BASH_VERSION" ]; then
        SHELL_RC="$HOME/.bashrc"
        SHELL_NAME="bash"
    else
        SHELL_RC="$HOME/.profile"
        SHELL_NAME="sh"
    fi
}

# Check if aliases already installed
check_installed() {
    if grep -q "nexus-aliases.sh" "$SHELL_RC" 2>/dev/null; then
        return 0
    fi
    return 1
}

# Install aliases
install_aliases() {
    print_info "Installing NEXUS OS aliases to $SHELL_RC..."
    
    # Create backup
    if [ -f "$SHELL_RC" ]; then
        cp "$SHELL_RC" "$SHELL_RC.backup.$(date +%Y%m%d%H%M%S)"
        print_success "Created backup of $SHELL_RC"
    fi
    
    # Add alias source to shell rc
    echo "" >> "$SHELL_RC"
    echo "# NEXUS OS Aliases" >> "$SHELL_RC"
    echo "export NEXUS_OS_DIR=\"$NEXUS_OS_DIR\"" >> "$SHELL_RC"
    echo "source \"$ALIASES_FILE\"" >> "$SHELL_RC"
    
    print_success "Aliases installed to $SHELL_RC"
}

# Uninstall aliases
uninstall_aliases() {
    print_info "Uninstalling NEXUS OS aliases from $SHELL_RC..."
    
    if [ -f "$SHELL_RC" ]; then
        # Remove NEXUS OS related lines
        sed -i '/# NEXUS OS Aliases/d' "$SHELL_RC"
        sed -i '/NEXUS_OS_DIR=/d' "$SHELL_RC"
        sed -i '/nexus-aliases.sh/d' "$SHELL_RC"
        
        print_success "Aliases removed from $SHELL_RC"
    fi
}

# Install dependencies
install_dependencies() {
    print_info "Checking dependencies..."
    
    # Check for required tools
    local missing=()
    
    command -v make &>/dev/null || missing+=("make")
    command -v gcc &>/dev/null || missing+=("gcc")
    command -v nasm &>/dev/null || missing+=("nasm")
    
    if [ ${#missing[@]} -gt 0 ]; then
        print_warning "Missing tools: ${missing[*]}"
        print_info "Install with:"
        
        if command -v apt-get &>/dev/null; then
            echo "  sudo apt-get install -y build-essential nasm"
        elif command -v dnf &>/dev/null; then
            echo "  sudo dnf install -y gcc nasm"
        elif command -v pacman &>/dev/null; then
            echo "  sudo pacman -S base-devel nasm"
        fi
        
        read -p "Install now? (y/N): " install
        if [ "$install" = "y" ] || [ "$install" = "Y" ]; then
            if command -v apt-get &>/dev/null; then
                sudo apt-get update && sudo apt-get install -y build-essential nasm
            elif command -v dnf &>/dev/null; then
                sudo dnf install -y gcc nasm
            elif command -v pacman &>/dev/null; then
                sudo pacman -S base-devel nasm
            fi
            print_success "Dependencies installed!"
        fi
    else
        print_success "All dependencies found!"
    fi
}

# Install QEMU
install_qemu() {
    print_info "Checking QEMU..."
    
    if ! command -v qemu-system-x86_64 &>/dev/null; then
        print_warning "QEMU not installed"
        read -p "Install QEMU? (y/N): " install
        
        if [ "$install" = "y" ] || [ "$install" = "Y" ]; then
            if command -v apt-get &>/dev/null; then
                sudo apt-get update && sudo apt-get install -y qemu-system-x86 qemu-utils
            elif command -v dnf &>/dev/null; then
                sudo dnf install -y qemu-system-x86 qemu-img
            elif command -v pacman &>/dev/null; then
                sudo pacman -S qemu-system-x86 qemu-img
            fi
            print_success "QEMU installed!"
        fi
    else
        print_success "QEMU found!"
    fi
}

# Show usage
show_usage() {
    echo ""
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  --install, -i     Install aliases"
    echo "  --uninstall, -u   Uninstall aliases"
    echo "  --deps, -d        Install dependencies"
    echo "  --qemu, -q        Install QEMU"
    echo "  --full, -f        Full installation (aliases + deps + qemu)"
    echo "  --check, -c       Check installation status"
    echo "  --help, -h        Show this help"
    echo ""
}

# Check installation status
check_status() {
    print_header
    
    echo "Installation Status:"
    echo "===================="
    echo ""
    
    # Check aliases
    if check_installed; then
        print_success "Aliases installed in $SHELL_RC"
    else
        print_error "Aliases not installed"
    fi
    
    # Check dependencies
    echo ""
    echo "Dependencies:"
    
    for cmd in make gcc nasm; do
        if command -v $cmd &>/dev/null; then
            print_success "$cmd found"
        else
            print_error "$cmd not found"
        fi
    done
    
    echo ""
    
    if command -v qemu-system-x86_64 &>/dev/null; then
        print_success "QEMU found"
    else
        print_error "QEMU not found"
    fi
    
    echo ""
    echo "NEXUS OS Directory: $NEXUS_OS_DIR"
    echo "Aliases File: $ALIASES_FILE"
    echo "Shell: $SHELL_NAME ($SHELL_RC)"
    echo ""
}

# Main
main() {
    print_header
    
    detect_shell
    
    case "${1:-}" in
        --install|-i)
            install_aliases
            print_success "Installation complete!"
            print_info "Restart your shell or run: source $SHELL_RC"
            ;;
        
        --uninstall|-u)
            uninstall_aliases
            print_success "Uninstallation complete!"
            print_info "Restart your shell or run: source $SHELL_RC"
            ;;
        
        --deps|-d)
            install_dependencies
            ;;
        
        --qemu|-q)
            install_qemu
            ;;
        
        --full|-f)
            install_dependencies
            install_qemu
            install_aliases
            print_success "Full installation complete!"
            print_info "Restart your shell or run: source $SHELL_RC"
            ;;
        
        --check|-c)
            check_status
            ;;
        
        --help|-h)
            show_usage
            ;;
        
        *)
            # Interactive mode
            echo ""
            echo "Welcome to NEXUS OS Setup!"
            echo ""
            echo "This script will help you set up the NEXUS OS development environment."
            echo ""
            read -p "Continue? (y/N): " confirm
            
            if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
                print_info "Cancelled"
                exit 0
            fi
            
            echo ""
            echo "Step 1: Installing dependencies..."
            install_dependencies
            
            echo ""
            echo "Step 2: Installing QEMU..."
            install_qemu
            
            echo ""
            echo "Step 3: Installing aliases..."
            install_aliases
            
            echo ""
            print_success "Setup complete!"
            echo ""
            print_info "Restart your shell or run: source $SHELL_RC"
            print_info "Then type 'nexus-help' for available commands"
            echo ""
            ;;
    esac
}

main "$@"
