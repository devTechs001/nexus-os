#!/bin/bash
#
# NEXUS OS - Quick Start Script
# Copyright (c) 2026 NEXUS Development Team
#
# One-command setup and run for NEXUS OS
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
MAGENTA='\033[0;35m'
RESET='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  NEXUS OS - Quick Start"
    echo "  Copyright (c) 2026 NEXUS Development Team"
    echo -e "${RESET}"
}

print_success() { echo -e "${GREEN}✓ $1${RESET}"; }
print_error() { echo -e "${RED}✗ $1${RESET}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${RESET}"; }
print_info() { echo -e "${BLUE}ℹ $1${RESET}"; }
print_step() { echo -e "${MAGENTA}▶ $1${RESET}"; }

# Check dependencies
check_deps() {
    print_step "Checking dependencies..."
    
    local missing=()
    
    command -v make &>/dev/null || missing+=("make")
    command -v gcc &>/dev/null || missing+=("gcc")
    command -v nasm &>/dev/null || missing+=("nasm")
    command -v grub-mkrescue &>/dev/null || missing+=("grub-pc-bin")
    command -v xorriso &>/dev/null || missing+=("xorriso")
    command -v qemu-system-x86_64 &>/dev/null || missing+=("qemu-system-x86")
    
    if [ ${#missing[@]} -gt 0 ]; then
        print_warning "Missing: ${missing[*]}"
        print_info "Installing missing dependencies..."
        
        if command -v apt-get &>/dev/null; then
            sudo apt-get update
            sudo apt-get install -y "${missing[@]}"
        elif command -v dnf &>/dev/null; then
            sudo dnf install -y "${missing[@]}"
        elif command -v pacman &>/dev/null; then
            sudo pacman -S --noconfirm "${missing[@]}"
        fi
        
        print_success "Dependencies installed!"
    else
        print_success "All dependencies found!"
    fi
}

# Build the OS
build_os() {
    print_step "Building NEXUS OS..."
    
    cd "$NEXUS_OS_DIR"
    
    if make clean && make; then
        print_success "Build complete!"
        print_info "ISO created: $NEXUS_OS_DIR/build/nexus-kernel.iso"
        ls -lh "$NEXUS_OS_DIR/build/nexus-kernel.iso"
    else
        print_error "Build failed!"
        print_info "Run './tools/dependency-checker.sh --fix' to fix issues"
        exit 1
    fi
}

# Run in QEMU
run_qemu() {
    print_step "Starting NEXUS OS in QEMU..."
    
    if [ ! -f "$NEXUS_OS_DIR/build/nexus-kernel.iso" ]; then
        print_error "ISO not found. Building first..."
        build_os
    fi
    
    qemu-system-x86_64 \
        -cdrom "$NEXUS_OS_DIR/build/nexus-kernel.iso" \
        -m 2G \
        -smp 2 \
        -name "NEXUS OS" \
        -display gtk,gl=on \
        -boot d
}

# Run in VMware
run_vmware() {
    print_step "Starting NEXUS OS in VMware..."
    
    if [ ! -f "$NEXUS_OS_DIR/build/nexus-kernel.iso" ]; then
        print_error "ISO not found. Building first..."
        build_os
    fi
    
    if command -v vmplayer &>/dev/null; then
        vmplayer "$NEXUS_OS_DIR/build/nexus-kernel.iso" &
        print_success "VMware Player started!"
    elif command -v vmware &>/dev/null; then
        vmware "$NEXUS_OS_DIR/build/nexus-kernel.iso" &
        print_success "VMware Workstation started!"
    else
        print_error "VMware not found!"
        print_info "Install from: https://www.vmware.com/products/workstation-player.html"
        print_info "Or use QEMU: $0 --qemu"
        exit 1
    fi
}

# Setup development environment
setup_dev() {
    print_step "Setting up development environment..."
    
    cd "$NEXUS_OS_DIR"
    
    # Install development tools
    print_info "Installing development tools..."
    
    if command -v apt-get &>/dev/null; then
        sudo apt-get install -y \
            build-essential \
            gcc \
            g++ \
            gdb \
            cmake \
            make \
            nasm \
            git \
            vim \
            clang \
            clang-format \
            clang-tidy
    fi
    
    # Setup VS Code settings
    if [ -d "$NEXUS_OS_DIR/.vscode" ]; then
        print_info "VS Code settings found"
    else
        print_info "Creating VS Code settings..."
        mkdir -p "$NEXUS_OS_DIR/.vscode"
        
        cat > "$NEXUS_OS_DIR/.vscode/settings.json" << 'EOF'
{
    "C_Cpp.default.compilerPath": "/usr/bin/gcc",
    "C_Cpp.default.intelliSenseMode": "linux-gcc-x64",
    "C_Cpp.errorSquiggles": "EnabledIfIncludesResolve",
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "ms-vscode.cpptools",
    "files.associations": {
        "*.h": "c"
    }
}
EOF
        
        cat > "$NEXUS_OS_DIR/.vscode/tasks.json" << 'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build NEXUS OS",
            "type": "shell",
            "command": "make",
            "group": "build",
            "problemMatcher": ["gcc"]
        },
        {
            "label": "Clean",
            "type": "shell",
            "command": "make clean",
            "group": "build"
        },
        {
            "label": "Run in QEMU",
            "type": "shell",
            "command": "make run",
            "group": "test"
        }
    ]
}
EOF
    fi
    
    print_success "Development environment ready!"
}

# Create project template
create_project() {
    local project_name="$1"
    local project_type="${2:-c}"
    
    print_step "Creating $project_type project: $project_name"
    
    mkdir -p "$project_name"
    cd "$project_name"
    
    case $project_type in
        c)
            cat > main.c << 'EOF'
/*
 * NEXUS OS - Sample C Project
 * Copyright (c) 2026 NEXUS Development Team
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    printf("Hello from NEXUS OS!\n");
    printf("Project: %s\n", argv[0]);
    
    if (argc > 1) {
        printf("Arguments: ");
        for (int i = 1; i < argc; i++) {
            printf("%s ", argv[i]);
        }
        printf("\n");
    }
    
    return 0;
}
EOF
            
            cat > Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = main

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
EOF
            ;;
        
        cpp)
            cat > main.cpp << 'EOF'
/*
 * NEXUS OS - Sample C++ Project
 * Copyright (c) 2026 NEXUS Development Team
 */

#include <iostream>
#include <vector>
#include <string>

int main(int argc, char *argv[]) {
    std::cout << "Hello from NEXUS OS!" << std::endl;
    std::cout << "Project: " << argv[0] << std::endl;
    
    if (argc > 1) {
        std::cout << "Arguments: ";
        for (int i = 1; i < argc; i++) {
            std::cout << argv[i] << " ";
        }
        std::cout << std::endl;
    }
    
    return 0;
}
EOF
            
            cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(NEXUS_CPP_Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)
EOF
            ;;
        
        python)
            cat > main.py << 'EOF'
#!/usr/bin/env python3
"""
NEXUS OS - Sample Python Project
Copyright (c) 2026 NEXUS Development Team
"""

import sys

def main():
    print("Hello from NEXUS OS!")
    print(f"Project: {sys.argv[0]}")
    
    if len(sys.argv) > 1:
        print(f"Arguments: {' '.join(sys.argv[1:])}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
EOF
            
            chmod +x main.py
            ;;
        
        rust)
            if command -v cargo &>/dev/null; then
                cargo init --name "$project_name"
            else
                print_error "Rust not installed!"
                print_info "Install with: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
                return 1
            fi
            ;;
        
        go)
            if command -v go &>/dev/null; then
                go mod init "$project_name"
                cat > main.go << 'EOF'
package main

import "fmt"

func main() {
    fmt.Println("Hello from NEXUS OS!")
}
EOF
            else
                print_error "Go not installed!"
                print_info "Install with: sudo apt-get install golang-go"
                return 1
            fi
            ;;
        
        *)
            print_error "Unknown project type: $project_type"
            print_info "Supported: c, cpp, python, rust, go"
            return 1
            ;;
    esac
    
    print_success "Project created: $project_name"
    print_info "cd $project_name && make (or appropriate build command)"
}

# Show help
show_help() {
    print_header
    echo ""
    echo "  Usage: $0 [OPTION]"
    echo ""
    echo "  Quick Start:"
    echo "    --run, -r           Build and run in QEMU (default)"
    echo "    --vmware, -v        Build and run in VMware"
    echo "    --build, -b         Build only"
    echo ""
    echo "  Development:"
    echo "    --setup, -s         Setup development environment"
    echo "    --project <name>    Create new project"
    echo "    --type <type>       Project type (c, cpp, python, rust, go)"
    echo ""
    echo "  Other:"
    echo "    --check, -c         Check dependencies"
    echo "    --help, -h          Show this help"
    echo ""
    echo "  Examples:"
    echo "    $0                  # Build and run in QEMU"
    echo "    $0 --vmware         # Build and run in VMware"
    echo "    $0 --setup          # Setup dev environment"
    echo "    $0 --project myapp  # Create C project"
    echo "    $0 --project myapp --type cpp"
    echo ""
}

# Main
main() {
    print_header
    
    case "${1:-}" in
        --run|-r)
            check_deps
            build_os
            run_qemu
            ;;
        
        --vmware|-v)
            check_deps
            build_os
            run_vmware
            ;;
        
        --build|-b)
            check_deps
            build_os
            ;;
        
        --setup|-s)
            setup_dev
            ;;
        
        --project)
            shift
            if [ -z "$1" ]; then
                print_error "Project name required"
                exit 1
            fi
            setup_dev
            create_project "$1" "${2:-c}"
            ;;
        
        --type)
            shift
            PROJECT_TYPE="$1"
            ;;
        
        --check|-c)
            check_deps
            ;;
        
        --help|-h)
            show_help
            ;;
        
        *)
            # Default: build and run
            check_deps
            build_os
            run_qemu
            ;;
    esac
}

main "$@"
