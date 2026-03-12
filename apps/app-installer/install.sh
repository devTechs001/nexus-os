#!/bin/bash
#
# NEXUS OS - App Installer (GUI & CLI)
# apps/app-installer/install.sh
# 
# Copyright (c) 2026 NEXUS Development Team
#
# Install applications via:
# - Package name
# - .nexus-pkg file
# - Source code (C, C++, etc.)
# - AppImage
# - Flatpak
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ╔════════════════════════════════════════════════════════╗"
    echo "  ║          NEXUS OS - Application Installer             ║"
    echo "  ╚════════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }
print_step() { echo -e "${MAGENTA}▶ $1${RESET}"; }

# Install from package manager
install_package() {
    local pkg_name="$1"
    
    print_step "Installing package: $pkg_name"
    
    # Try NEXUS package manager first
    if command -v pkg &>/dev/null; then
        pkg install "$pkg_name" && return 0
    fi
    
    # Try apt
    if command -v apt-get &>/dev/null; then
        print_info "Using apt..."
        sudo apt-get update && sudo apt-get install -y "$pkg_name" && return 0
    fi
    
    # Try dnf
    if command -v dnf &>/dev/null; then
        print_info "Using dnf..."
        sudo dnf install -y "$pkg_name" && return 0
    fi
    
    # Try pacman
    if command -v pacman &>/dev/null; then
        print_info "Using pacman..."
        sudo pacman -S --noconfirm "$pkg_name" && return 0
    fi
    
    print_error "No package manager found or package not available"
    return 1
}

# Install from source (C/C++)
install_from_source() {
    local source_dir="$1"
    
    print_step "Building from source: $source_dir"
    
    if [ ! -d "$source_dir" ]; then
        print_error "Source directory not found: $source_dir"
        return 1
    fi
    
    cd "$source_dir"
    
    # Install build dependencies
    print_info "Installing build dependencies..."
    install_package "build-essential"
    install_package "gcc"
    install_package "g++"
    install_package "make"
    install_package "cmake"
    
    # Detect build system and build
    if [ -f "CMakeLists.txt" ]; then
        print_info "Building with CMake..."
        mkdir -p build && cd build
        cmake .. && make -j$(nproc) && sudo make install
    elif [ -f "configure" ]; then
        print_info "Building with Autotools..."
        ./configure && make -j$(nproc) && sudo make install
    elif [ -f "Makefile" ]; then
        print_info "Building with Make..."
        make -j$(nproc) && sudo make install
    elif [ -f "meson.build" ]; then
        print_info "Building with Meson..."
        meson setup build && ninja -C build && sudo ninja -C build install
    else
        print_error "Unknown build system"
        return 1
    fi
    
    print_success "Installation complete"
}

# Install AppImage
install_appimage() {
    local appimage="$1"
    
    print_step "Installing AppImage: $appimage"
    
    if [ ! -f "$appimage" ]; then
        print_error "AppImage not found: $appimage"
        return 1
    fi
    
    # Make executable
    chmod +x "$appimage"
    
    # Extract and integrate
    local app_name=$(basename "$appimage" .AppImage)
    local install_dir="$HOME/Applications"
    
    mkdir -p "$install_dir"
    mv "$appimage" "$install_dir/"
    
    # Create desktop entry
    cat > "$HOME/.local/share/applications/$app_name.desktop" << EOF
[Desktop Entry]
Type=Application
Name=$app_name
Exec=$HOME/Applications/$appimage
Icon=application-x-executable
Categories=Utility;
EOF
    
    print_success "AppImage installed: $app_name"
}

# Install Flatpak
install_flatpak() {
    local app_id="$1"
    
    print_step "Installing Flatpak: $app_id"
    
    if ! command -v flatpak &>/dev/null; then
        print_warning "Flatpak not installed"
        read -p "Install Flatpak? (y/N): " install_fp
        
        if [ "$install_fp" = "y" ] || [ "$install_fp" = "Y" ]; then
            install_package "flatpak"
        else
            print_error "Flatpak required"
            return 1
        fi
    fi
    
    # Add Flathub if not added
    flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
    
    flatpak install -y flathub "$app_id"
    print_success "Flatpak installed: $app_id"
}

# Install from .nexus-pkg
install_nexus_pkg() {
    local pkg_file="$1"
    
    print_step "Installing NEXUS package: $pkg_file"
    
    if [ ! -f "$pkg_file" ]; then
        print_error "Package file not found: $pkg_file"
        return 1
    fi
    
    # Extract package
    local temp_dir=$(mktemp -d)
    cd "$temp_dir"
    
    tar -xzf "$pkg_file"
    
    # Run installer
    if [ -f "install.sh" ]; then
        ./install.sh
    elif [ -f "install" ]; then
        ./install
    else
        print_error "No installer found in package"
        return 1
    fi
    
    # Cleanup
    cd -
    rm -rf "$temp_dir"
    
    print_success "NEXUS package installed"
}

# Interactive mode
interactive_install() {
    clear
    print_header
    
    echo ""
    echo "  Installation Method"
    echo "  ═══════════════════════════════════════════════════════"
    echo ""
    echo "  1. Install from Package Manager"
    echo "  2. Install from Source (C/C++)"
    echo "  3. Install AppImage"
    echo "  4. Install Flatpak"
    echo "  5. Install NEXUS Package"
    echo "  6. Search Packages"
    echo "  0. Exit"
    echo ""
    read -p "Select method [0-6]: " method
    
    case $method in
        1)
            read -p "Package name: " pkg_name
            install_package "$pkg_name"
            ;;
        2)
            read -p "Source directory: " source_dir
            install_from_source "$source_dir"
            ;;
        3)
            read -p "AppImage file: " appimage
            install_appimage "$appimage"
            ;;
        4)
            read -p "Flatpak app ID: " app_id
            install_flatpak "$app_id"
            ;;
        5)
            read -p "Package file: " pkg_file
            install_nexus_pkg "$pkg_file"
            ;;
        6)
            read -p "Search query: " query
            if command -v pkg &>/dev/null; then
                pkg search "$query"
            elif command -v apt-cache &>/dev/null; then
                apt-cache search "$query"
            fi
            ;;
        0)
            exit 0
            ;;
        *)
            print_error "Invalid option"
            sleep 2
            interactive_install
            ;;
    esac
}

# Show help
show_help() {
    print_header
    echo ""
    echo "  Usage: $0 [OPTION] [ARGUMENT]"
    echo ""
    echo "  Installation Methods:"
    echo "    -p, --package <name>      Install from package manager"
    echo "    -s, --source <dir>        Install from source"
    echo "    -a, --appimage <file>     Install AppImage"
    echo "    -f, --flatpak <id>        Install Flatpak"
    echo "    -n, --nexus-pkg <file>    Install NEXUS package"
    echo ""
    echo "  Options:"
    echo "    -i, --interactive         Interactive mode"
    echo "    -S, --search <query>      Search packages"
    echo "    -l, --list                List installed packages"
    echo "    -h, --help                Show this help"
    echo ""
    echo "  Examples:"
    echo "    $0 -p firefox             # Install Firefox"
    echo "    $0 -s ./myapp             # Build from source"
    echo "    $0 -a MyApp.AppImage      # Install AppImage"
    echo "    $0 -i                     # Interactive mode"
    echo ""
}

# Main
main() {
    case "${1:-}" in
        -p|--package)
            install_package "$2"
            ;;
        -s|--source)
            install_from_source "$2"
            ;;
        -a|--appimage)
            install_appimage "$2"
            ;;
        -f|--flatpak)
            install_flatpak "$2"
            ;;
        -n|--nexus-pkg)
            install_nexus_pkg "$2"
            ;;
        -S|--search)
            if command -v pkg &>/dev/null; then
                pkg search "$2"
            elif command -v apt-cache &>/dev/null; then
                apt-cache search "$2"
            fi
            ;;
        -l|--list)
            if command -v pkg &>/dev/null; then
                pkg list --installed
            elif command -v dpkg &>/dev/null; then
                dpkg --list
            fi
            ;;
        -i|--interactive)
            interactive_install
            ;;
        -h|--help)
            show_help
            ;;
        *)
            interactive_install
            ;;
    esac
}

main "$@"
