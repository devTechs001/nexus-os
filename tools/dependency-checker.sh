#!/bin/bash
#
# NEXUS OS - Dependency Checker and Auto-Installer
# Copyright (c) 2026 NEXUS Development Team
#
# Automatically detects and installs missing dependencies
# Provides fallback solutions when installation fails
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"
LOG_FILE="/tmp/nexus-os-deps.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
RESET='\033[0m'

# Dependency status
declare -A MISSING_DEPS
declare -A INSTALLED_DEPS
declare -A OPTIONAL_DEPS
HAS_SUDO=false
AUTO_INSTALL=true
INTERACTIVE=true

print_header() {
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  Dependency Checker & Auto-Installer"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }
print_step() { echo -e "${MAGENTA}▶ $1${RESET}"; }

# Log function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

# Check for sudo
check_sudo() {
    if command -v sudo &> /dev/null && sudo -n true 2>/dev/null; then
        HAS_SUDO=true
        print_success "Sudo access available"
    elif command -v sudo &> /dev/null; then
        print_warning "Sudo password may be required for some installations"
        HAS_SUDO=true
    else
        print_warning "Sudo not available - some installations may fail"
        HAS_SUDO=false
    fi
    log "Sudo check: $HAS_SUDO"
}

# Detect package manager
detect_package_manager() {
    if command -v apt-get &> /dev/null; then
        PKG_MANAGER="apt"
        PKG_UPDATE="sudo apt-get update"
        PKG_INSTALL="sudo apt-get install -y"
        PKG_SEARCH="apt-cache search"
        print_info "Detected package manager: apt (Debian/Ubuntu/Kali)"
    elif command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
        PKG_UPDATE="sudo dnf check-update"
        PKG_INSTALL="sudo dnf install -y"
        PKG_SEARCH="dnf search"
        print_info "Detected package manager: dnf (Fedora/RHEL)"
    elif command -v yum &> /dev/null; then
        PKG_MANAGER="yum"
        PKG_UPDATE="sudo yum check-update"
        PKG_INSTALL="sudo yum install -y"
        PKG_SEARCH="yum search"
        print_info "Detected package manager: yum (RHEL/CentOS)"
    elif command -v pacman &> /dev/null; then
        PKG_MANAGER="pacman"
        PKG_UPDATE="sudo pacman -Sy"
        PKG_INSTALL="sudo pacman -S --noconfirm"
        PKG_SEARCH="pacman -Ss"
        print_info "Detected package manager: pacman (Arch Linux)"
    elif command -v zypper &> /dev/null; then
        PKG_MANAGER="zypper"
        PKG_UPDATE="sudo zypper refresh"
        PKG_INSTALL="sudo zypper install -y"
        PKG_SEARCH="zypper search"
        print_info "Detected package manager: zypper (openSUSE)"
    else
        PKG_MANAGER="unknown"
        print_error "No supported package manager found!"
        log "ERROR: No package manager detected"
        return 1
    fi
    log "Package manager: $PKG_MANAGER"
}

# Check if package is installed
is_installed() {
    local pkg="$1"
    
    case $PKG_MANAGER in
        apt)
            dpkg -l "$pkg" &>/dev/null && return 0
            ;;
        dnf|yum)
            rpm -q "$pkg" &>/dev/null && return 0
            ;;
        pacman)
            pacman -Q "$pkg" &>/dev/null && return 0
            ;;
        zypper)
            rpm -q "$pkg" &>/dev/null && return 0
            ;;
    esac
    
    return 1
}

# Install package with retry
install_package() {
    local pkg="$1"
    local description="$2"
    local fallback="$3"
    
    print_step "Installing: $pkg ($description)"
    log "Installing: $pkg"
    
    # Try installation
    if $PKG_INSTALL "$pkg" &>/dev/null; then
        if is_installed "$pkg"; then
            print_success "✓ $pkg installed successfully"
            log "SUCCESS: $pkg installed"
            INSTALLED_DEPS["$pkg"]="installed"
            return 0
        fi
    fi
    
    # Retry with update
    print_warning "First attempt failed, updating package list..."
    log "Retrying $pkg with update"
    
    if eval "$PKG_UPDATE" &>/dev/null; then
        if $PKG_INSTALL "$pkg" &>/dev/null; then
            if is_installed "$pkg"; then
                print_success "✓ $pkg installed after update"
                log "SUCCESS: $pkg installed after update"
                INSTALLED_DEPS["$pkg"]="installed"
                return 0
            fi
        fi
    fi
    
    # Installation failed - offer fallback
    print_error "✗ Failed to install $pkg"
    log "FAILED: $pkg"
    MISSING_DEPS["$pkg"]="$fallback"
    
    if [ -n "$fallback" ] && [ "$INTERACTIVE" = true ]; then
        echo ""
        print_warning "Fallback solution available: $fallback"
        echo ""
        read -p "Would you like to try the fallback? (y/N): " try_fallback
        
        if [ "$try_fallback" = "y" ] || [ "$try_fallback" = "Y" ]; then
            eval "$fallback"
            return $?
        fi
    fi
    
    return 1
}

# Check and install build dependencies
check_build_deps() {
    print_header
    echo ""
    print_info "Checking build dependencies..."
    echo ""

    local build_deps=(
        "build-essential:GCC and build tools:echo 'Install manually: sudo apt-get install build-essential'"
        "gcc:GNU Compiler Collection:$PKG_INSTALL gcc"
        "make:Build automation tool:$PKG_INSTALL make"
        "nasm:Netwide Assembler:$PKG_INSTALL nasm"
        "grub-pc-bin:GRUB bootloader (REQUIRED):$PKG_INSTALL grub-pc-bin"
        "grub-common:GRUB common files (REQUIRED):$PKG_INSTALL grub-common"
        "xorriso:ISO creation tool (REQUIRED):$PKG_INSTALL xorriso"
        "mtools:MS-DOS tools (REQUIRED):$PKG_INSTALL mtools"
    )

    local missing_count=0
    local critical_missing=0

    for dep_info in "${build_deps[@]}"; do
        IFS=':' read -r pkg desc fallback <<< "$dep_info"

        if is_installed "$pkg"; then
            print_success "✓ $pkg ($desc)"
            INSTALLED_DEPS["$pkg"]="installed"
        else
            if [[ "$desc" == *"REQUIRED"* ]]; then
                print_error "✗ $pkg ($desc) - CRITICAL"
                ((critical_missing++))
            else
                print_warning "✗ $pkg ($desc) - MISSING"
            fi
            MISSING_DEPS["$pkg"]="$fallback"
            ((missing_count++))
        fi
    done

    echo ""

    if [ $critical_missing -gt 0 ]; then
        print_error "$critical_missing CRITICAL dependencies missing!"
        echo ""
        print_info "These MUST be installed for the build to work."
        echo ""

        if [ "$AUTO_INSTALL" = true ] || [ "$INTERACTIVE" = true ]; then
            read -p "Install ALL critical dependencies now? (Y/n): " install_critical

            if [ "$install_critical" != "n" ] && [ "$install_critical" != "N" ]; then
                print_info "Updating package list..."
                eval "$PKG_UPDATE" || {
                    print_error "Failed to update package list"
                    return 1
                }

                # Install critical packages
                for dep_info in "${build_deps[@]}"; do
                    IFS=':' read -r pkg desc fallback <<< "$dep_info"

                    if [[ "$desc" == *"REQUIRED"* ]] && [ -n "${MISSING_DEPS[$pkg]}" ]; then
                        install_package "$pkg" "$desc" "$fallback" || {
                            print_error "Failed to install $pkg"
                            return 1
                        }
                    fi
                done

                print_success "Critical dependencies installed!"
            else
                print_error "Build cannot continue without critical dependencies"
                return 1
            fi
        else
            print_error "Run with --auto or --fix to auto-install"
            return 1
        fi
    elif [ $missing_count -gt 0 ]; then
        print_warning "$missing_count optional dependencies missing"
        echo ""

        if [ "$AUTO_INSTALL" = true ]; then
            print_info "Attempting to install missing dependencies..."
            echo ""

            for dep_info in "${build_deps[@]}"; do
                IFS=':' read -r pkg desc fallback <<< "$dep_info"

                if [ -n "${MISSING_DEPS[$pkg]}" ]; then
                    install_package "$pkg" "$desc" "$fallback"
                fi
            done
        fi
    else
        print_success "All build dependencies installed!"
    fi

    return 0
}

# Check and install runtime dependencies
check_runtime_deps() {
    echo ""
    print_info "Checking runtime dependencies..."
    echo ""
    
    local runtime_deps=(
        "qemu-system-x86:QEMU emulator:$PKG_INSTALL qemu-system-x86"
        "qemu-utils:QEMU utilities:$PKG_INSTALL qemu-utils"
    )
    
    local missing_count=0
    
    for dep_info in "${runtime_deps[@]}"; do
        IFS=':' read -r pkg desc fallback <<< "$dep_info"
        
        if is_installed "$pkg"; then
            print_success "✓ $pkg ($desc)"
            INSTALLED_DEPS["$pkg"]="installed"
        else
            print_warning "✗ $pkg ($desc) - MISSING"
            MISSING_DEPS["$pkg"]="$fallback"
            ((missing_count++))
        fi
    done
    
    echo ""
    
    if [ $missing_count -gt 0 ]; then
        print_warning "$missing_count runtime dependencies missing"
        echo ""
        
        # Check for VMware as alternative
        if command -v vmplayer &>/dev/null || command -v vmware &>/dev/null; then
            print_success "VMware detected - QEMU is optional"
            OPTIONAL_DEPS["qemu"]="VMware available"
        else
            print_info "VMware not found - QEMU recommended for testing"
            echo ""
            print_info "Alternative: Install VMware from https://www.vmware.com"
        fi
        
        if [ "$AUTO_INSTALL" = true ]; then
            echo ""
            read -p "Install QEMU? (Y/n): " install_qemu
            
            if [ "$install_qemu" != "n" ] && [ "$install_qemu" != "N" ]; then
                for dep_info in "${runtime_deps[@]}"; do
                    IFS=':' read -r pkg desc fallback <<< "$dep_info"
                    
                    if [ -n "${MISSING_DEPS[$pkg]}" ]; then
                        install_package "$pkg" "$desc" "$fallback"
                    fi
                done
            fi
        fi
    else
        print_success "All runtime dependencies installed!"
    fi
    
    return 0
}

# Check VMware availability
check_vmware() {
    echo ""
    print_info "Checking VMware availability..."
    echo ""
    
    if command -v vmplayer &>/dev/null; then
        print_success "✓ VMware Player found"
        INSTALLED_DEPS["vmware-player"]="installed"
        return 0
    elif command -v vmware &>/dev/null; then
        print_success "✓ VMware Workstation found"
        INSTALLED_DEPS["vmware-workstation"]="installed"
        return 0
    elif command -v vmrun &>/dev/null; then
        print_success "✓ VMware CLI (vmrun) found"
        INSTALLED_DEPS["vmware-cli"]="installed"
        return 0
    else
        print_warning "✗ VMware not found"
        MISSING_DEPS["vmware"]="Download from https://www.vmware.com/products/workstation-player.html"
        
        echo ""
        print_info "VMware provides better performance than QEMU"
        print_info "VMware Workstation Player is FREE for personal use"
        echo ""
        
        if [ "$INTERACTIVE" = true ]; then
            read -p "Open VMware download page? (y/N): " open_vmware
            
            if [ "$open_vmware" = "y" ] || [ "$open_vmware" = "Y" ]; then
                if command -v xdg-open &>/dev/null; then
                    xdg-open "https://www.vmware.com/products/workstation-player.html"
                elif command -v gnome-open &>/dev/null; then
                    gnome-open "https://www.vmware.com/products/workstation-player.html"
                else
                    print_info "Please visit: https://www.vmware.com/products/workstation-player.html"
                fi
            fi
        fi
    fi
    
    return 0
}

# Check disk space
check_disk_space() {
    echo ""
    print_info "Checking disk space..."
    echo ""
    
    local required_space=1073741824  # 1GB in bytes
    local available_space=$(df -B1 "$NEXUS_OS_DIR" | tail -1 | awk '{print $4}')
    
    if [ "$available_space" -lt "$required_space" ]; then
        print_error "✗ Insufficient disk space"
        print_warning "Required: 1GB, Available: $(($available_space / 1024 / 1024))MB"
        MISSING_DEPS["disk-space"]="Free up disk space or use a different partition"
        return 1
    else
        print_success "✓ Sufficient disk space ($(($available_space / 1024 / 1024))MB available)"
        return 0
    fi
}

# Check permissions
check_permissions() {
    echo ""
    print_info "Checking permissions..."
    echo ""
    
    if [ -w "$NEXUS_OS_DIR" ]; then
        print_success "✓ Write access to NEXUS OS directory"
    else
        print_error "✗ No write access to: $NEXUS_OS_DIR"
        MISSING_DEPS["permissions"]="chmod u+w $NEXUS_OS_DIR  OR  sudo chown -R $USER:$USER $NEXUS_OS_DIR"
        
        if [ "$INTERACTIVE" = true ]; then
            echo ""
            read -p "Fix permissions automatically? (y/N): " fix_perms
            
            if [ "$fix_perms" = "y" ] || [ "$fix_perms" = "Y" ]; then
                if $HAS_SUDO; then
                    sudo chown -R "$USER":"$USER" "$NEXUS_OS_DIR"
                    print_success "Permissions fixed!"
                else
                    print_error "Cannot fix permissions without sudo"
                    print_info "Run manually: sudo chown -R $USER:$USER $NEXUS_OS_DIR"
                fi
            fi
        fi
        return 1
    fi
    
    return 0
}

# Show summary
show_summary() {
    echo ""
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    print_info "Dependency Check Summary"
    echo ""
    echo "  ┌─────────────────────────────────────────────────────┐"
    printf "  │  %-30s %25s │\n" "Installed:" "${#INSTALLED_DEPS[@]}"
    printf "  │  %-30s %25s │\n" "Missing:" "${#MISSING_DEPS[@]}"
    printf "  │  %-30s %25s │\n" "Optional:" "${#OPTIONAL_DEPS[@]}"
    echo "  └─────────────────────────────────────────────────────┘"
    echo ""
    
    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        print_warning "Missing Dependencies:"
        echo ""
        for pkg in "${!MISSING_DEPS[@]}"; do
            echo "  • $pkg"
            echo "    Solution: ${MISSING_DEPS[$pkg]}"
            echo ""
        done
        
        echo "  ═══════════════════════════════════════════════════════"
        echo ""
        print_info "The system can still function with some features disabled."
        print_info "Install missing dependencies for full functionality."
        echo ""
    else
        print_success "All dependencies satisfied!"
        echo ""
    fi
}

# Try to fix all issues
fix_all() {
    print_header
    echo ""
    print_info "Attempting to fix all issues..."
    echo ""
    
    check_sudo
    detect_package_manager || return 1
    
    check_permissions
    check_disk_space
    check_build_deps
    check_runtime_deps
    check_vmware
    
    show_summary
    
    if [ ${#MISSING_DEPS[@]} -eq 0 ]; then
        echo ""
        print_success "═══════════════════════════════════════════════════════"
        echo ""
        print_success "  All issues resolved! System ready to use."
        echo ""
        print_success "═══════════════════════════════════════════════════════"
        echo ""
        
        # Offer to run build
        if [ "$INTERACTIVE" = true ]; then
            read -p "Build NEXUS OS now? (Y/n): " build_now
            
            if [ "$build_now" != "n" ] && [ "$build_now" != "N" ]; then
                cd "$NEXUS_OS_DIR" && make
            fi
        fi
        
        return 0
    else
        echo ""
        print_warning "Some issues remain. See summary above for solutions."
        echo ""
        return 1
    fi
}

# Interactive mode
interactive_mode() {
    while true; do
        clear
        print_header
        
        echo ""
        echo "  Main Menu"
        echo "  ═══════════════════════════════════════════════════"
        echo ""
        echo "  1. Check All Dependencies"
        echo "  2. Check Build Dependencies"
        echo "  3. Check Runtime Dependencies"
        echo "  4. Check VMware"
        echo "  5. Check Disk Space"
        echo "  6. Check Permissions"
        echo "  7. Fix All Issues"
        echo "  8. Show Summary"
        echo "  0. Exit"
        echo ""
        read -p "Select option [0-8]: " choice
        
        case $choice in
            1) check_sudo; detect_package_manager; check_build_deps; check_runtime_deps; check_vmware; show_summary ;;
            2) check_sudo; detect_package_manager; check_build_deps ;;
            3) check_sudo; detect_package_manager; check_runtime_deps ;;
            4) check_vmware ;;
            5) check_disk_space ;;
            6) check_permissions ;;
            7) fix_all ;;
            8) show_summary ;;
            0) exit 0 ;;
            *) print_error "Invalid option" ; sleep 2 ;;
        esac
        
        echo ""
        read -p "Press Enter to continue..."
    done
}

# Show help
show_help() {
    print_header
    echo ""
    echo "  Usage: $0 [OPTION]"
    echo ""
    echo "  Options:"
    echo "    --check, -c       Check all dependencies"
    echo "    --build, -b       Check build dependencies only"
    echo "    --runtime, -r     Check runtime dependencies only"
    echo "    --vmware, -v      Check VMware availability"
    echo "    --fix, -f         Fix all issues automatically"
    echo "    --auto, -a        Enable auto-install (non-interactive)"
    echo "    --no-auto, -n     Disable auto-install"
    echo "    --interactive, -i Interactive mode"
    echo "    --help, -h        Show this help"
    echo ""
    echo "  Without options, runs interactive mode."
    echo ""
    echo "  Examples:"
    echo "    $0 --check        # Check all dependencies"
    echo "    $0 --fix          # Fix all issues"
    echo "    $0 --fix --auto   # Fix all issues without prompts"
    echo ""
}

# Main
main() {
    log "=== NEXUS OS Dependency Checker Started ==="
    
    case "${1:-}" in
        --check|-c)
            check_sudo
            detect_package_manager || exit 1
            check_build_deps
            check_runtime_deps
            check_vmware
            show_summary
            ;;
        
        --build|-b)
            check_sudo
            detect_package_manager || exit 1
            check_build_deps
            ;;
        
        --runtime|-r)
            check_sudo
            detect_package_manager || exit 1
            check_runtime_deps
            ;;
        
        --vmware|-v)
            check_vmware
            ;;
        
        --disk|-d)
            check_disk_space
            ;;
        
        --perms|-p)
            check_permissions
            ;;
        
        --fix|-f)
            fix_all
            ;;
        
        --auto|-a)
            AUTO_INSTALL=true
            INTERACTIVE=false
            fix_all
            ;;
        
        --no-auto|-n)
            AUTO_INSTALL=false
            fix_all
            ;;
        
        --interactive|-i)
            interactive_mode
            ;;
        
        --help|-h)
            show_help
            ;;
        
        *)
            interactive_mode
            ;;
    esac
    
    log "=== NEXUS OS Dependency Checker Finished ==="
}

main "$@"
