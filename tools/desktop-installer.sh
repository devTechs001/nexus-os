#!/bin/bash
#
# NEXUS OS - Desktop Installer
# Copyright (c) 2026 NEXUS Development Team
#
# Creates desktop icons, start menu entries, and file associations
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEXUS_OS_DIR="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
RESET='\033[0m'

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }

# Detect desktop environment
detect_desktop() {
    if [ -n "$KDE_FULL_SESSION" ]; then
        DESKTOP_ENV="kde"
        ICON_DIR="$HOME/.local/share/icons"
        APP_DIR="$HOME/.local/share/applications"
        DESKTOP_DIR="$HOME/Desktop"
    elif [ -n "$GNOME_DESKTOP_SESSION_ID" ]; then
        DESKTOP_ENV="gnome"
        ICON_DIR="$HOME/.local/share/icons"
        APP_DIR="$HOME/.local/share/applications"
        DESKTOP_DIR="$HOME/Desktop"
    elif [ -n "$XFCE_DESKTOP_SESSION" ]; then
        DESKTOP_ENV="xfce"
        ICON_DIR="$HOME/.local/share/icons"
        APP_DIR="$HOME/.local/share/applications"
        DESKTOP_DIR="$HOME/Desktop"
    else
        DESKTOP_ENV="generic"
        ICON_DIR="$HOME/.local/share/icons"
        APP_DIR="$HOME/.local/share/applications"
        DESKTOP_DIR="$HOME/Desktop"
    fi
    
    print_info "Detected desktop environment: $DESKTOP_ENV"
}

# Create directories
create_directories() {
    print_info "Creating directories..."
    
    mkdir -p "$ICON_DIR/hicolor/256x256/apps"
    mkdir -p "$ICON_DIR/hicolor/128x128/apps"
    mkdir -p "$ICON_DIR/hicolor/64x64/apps"
    mkdir -p "$ICON_DIR/hicolor/48x48/apps"
    mkdir -p "$APP_DIR"
    mkdir -p "$DESKTOP_DIR"
    
    print_success "Directories created"
}

# Create NEXUS OS icon
create_icon() {
    print_info "Creating application icons..."
    
    # Create SVG icon
    cat > "$ICON_DIR/hicolor/256x256/apps/nexus-os.svg" << 'EOF'
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="grad1" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#42A5F5;stop-opacity:1" />
      <stop offset="100%" style="stop-color:#2962FF;stop-opacity:1" />
    </linearGradient>
  </defs>
  <circle cx="128" cy="128" r="120" fill="url(#grad1)"/>
  <path d="M128 50 L180 102 L168 114 L128 74 L88 114 L76 102 Z" fill="#FFFFFF" opacity="0.9"/>
  <circle cx="128" cy="140" r="50" fill="none" stroke="#FFFFFF" stroke-width="10" opacity="0.7"/>
  <circle cx="128" cy="140" r="24" fill="#FFFFFF" opacity="0.5"/>
</svg>
EOF

    # Create PNG icons (simplified - in real implementation, use actual PNG)
    for size in 128 64 48; do
        cp "$ICON_DIR/hicolor/256x256/apps/nexus-os.svg" \
           "$ICON_DIR/hicolor/${size}x${size}/apps/nexus-os.svg"
    done
    
    print_success "Icons created"
}

# Create NEXUS OS launcher
create_nexus_launcher() {
    print_info "Creating NEXUS OS launcher..."
    
    cat > "$APP_DIR/nexus-os.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=NEXUS OS
Comment=NEXUS OS Boot Manager - Launch and manage virtual machines
Exec=$SCRIPT_DIR/nexus-vm-manager.sh
Icon=nexus-os
Terminal=false
Categories=System;VirtualMachine;
Keywords=virtual machine;vm;boot;os;
StartupNotify=true
StartupWMClass=nexus-os
EOF

    chmod +x "$APP_DIR/nexus-os.desktop"
    
    # Create desktop shortcut
    if [ -d "$DESKTOP_DIR" ]; then
        cp "$APP_DIR/nexus-os.desktop" "$DESKTOP_DIR/NEXUS OS.desktop"
        chmod +x "$DESKTOP_DIR/NEXUS OS.desktop"
    fi
    
    print_success "NEXUS OS launcher created"
}

# Create VM Manager launcher
create_vm_manager_launcher() {
    print_info "Creating VM Manager launcher..."
    
    cat > "$APP_DIR/nexus-vm-manager.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=NEXUS VM Manager
Comment=Advanced Virtual Machine Manager with full customization
Exec=$SCRIPT_DIR/nexus-advanced-vm-manager.sh
Icon=preferences-system-virtualization
Terminal=false
Categories=System;VirtualMachine;Settings;
Keywords=virtual machine;vm;manager;virtualization;
StartupNotify=true
StartupWMClass=nexus-vm-manager
EOF

    chmod +x "$APP_DIR/nexus-vm-manager.desktop"
    
    print_success "VM Manager launcher created"
}

# Create Quick Start launcher
create_quick_start_launcher() {
    print_info "Creating Quick Start launcher..."
    
    cat > "$APP_DIR/nexus-quick-start.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=NEXUS Quick Start
Comment=Quick start NEXUS OS in VMware/QEMU
Exec=$SCRIPT_DIR/nexus-quick-start.sh
Icon=nexus-os
Terminal=false
Categories=System;VirtualMachine;
Keywords=quick;start;boot;run;
StartupNotify=true
EOF

    chmod +x "$APP_DIR/nexus-quick-start.desktop"
    
    print_success "Quick Start launcher created"
}

# Create Settings launcher
create_settings_launcher() {
    print_info "Creating Settings launcher..."
    
    cat > "$APP_DIR/nexus-vm-settings.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=NEXUS VM Settings
Comment=Configure virtual machine preferences and optimizations
Exec=$SCRIPT_DIR/nexus-vm-settings.sh
Icon=preferences-system
Terminal=false
Categories=System;Settings;
Keywords=settings;preferences;configuration;options;
StartupNotify=true
EOF

    chmod +x "$APP_DIR/nexus-vm-settings.desktop"
    
    print_success "Settings launcher created"
}

# Update icon cache
update_icon_cache() {
    print_info "Updating icon cache..."
    
    if command -v gtk-update-icon-cache &> /dev/null; then
        gtk-update-icon-cache -f "$ICON_DIR/hicolor" 2>/dev/null || true
    fi
    
    if command -v kbuildsycoca5 &> /dev/null; then
        kbuildsycoca5 2>/dev/null || true
    fi
    
    print_success "Icon cache updated"
}

# Register file types
register_file_types() {
    print_info "Registering file types..."
    
    # Create MIME type for NEXUS VM files
    mkdir -p "$HOME/.local/share/mime/packages"
    
    cat > "$HOME/.local/share/mime/packages/nexus-vm.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-nexus-vm">
    <comment>NEXUS OS Virtual Machine</comment>
    <glob pattern="*.nexus-vm"/>
    <glob pattern="*.nvm"/>
    <icon name="nexus-os"/>
  </mime-type>
</mime-info>
EOF

    if command -v update-mime-database &> /dev/null; then
        update-mime-database "$HOME/.local/share/mime" 2>/dev/null || true
    fi
    
    print_success "File types registered"
}

# Show completion message
show_completion() {
    echo ""
    echo -e "${CYAN}"
    echo "  ╔════════════════════════════════════════════════════════╗"
    echo "  ║          NEXUS OS Installation Complete!              ║"
    echo "  ╠════════════════════════════════════════════════════════╣"
    echo "  ║                                                        ║"
    echo "  ║  Application icons created:                            ║"
    echo "  ║    ✓ NEXUS OS (Main launcher)                          ║"
    echo "  ║    ✓ NEXUS VM Manager (Advanced VM management)         ║"
    echo "  ║    ✓ NEXUS Quick Start (Quick boot)                    ║"
    echo "  ║    ✓ NEXUS VM Settings (Preferences)                   ║"
    echo "  ║                                                        ║"
    echo "  ║  Locations:                                            ║"
    echo "  ║    • Applications Menu > System                        ║"
    echo "  ║    • Desktop shortcuts (if available)                  ║"
    echo "  ║                                                        ║"
    echo "  ║  To launch NEXUS OS:                                   ║"
    echo "  ║    1. Click NEXUS OS icon in applications menu         ║"
    echo "  ║    2. Or use desktop shortcut                          ║"
    echo "  ║    3. Or run: $SCRIPT_DIR/nexus-quick-start.sh           ║"
    echo "  ║                                                        ║"
    echo "  ╚════════════════════════════════════════════════════════╝"
    echo -e "${RESET}"
    echo ""
}

# Main installation
main() {
    echo ""
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  Desktop Installer"
    echo -e "${RESET}"
    echo ""
    
    detect_desktop
    create_directories
    create_icon
    create_nexus_launcher
    create_vm_manager_launcher
    create_quick_start_launcher
    create_settings_launcher
    update_icon_cache
    register_file_types
    show_completion
    
    print_success "Installation complete!"
    print_info "You may need to log out and log back in for changes to take effect."
}

main "$@"
