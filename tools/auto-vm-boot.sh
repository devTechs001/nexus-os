#!/bin/bash
# NEXUS OS - Auto VMware Creator and Boot Manager
# Automatically creates VMware VM and boots NEXUS OS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO_PATH="${PROJECT_DIR}/build/nexus-kernel.iso"
VM_DIR="$HOME/vmware-vm/NEXUS-OS"
VMX_FILE="$VM_DIR/NEXUS-OS.vmx"
VMLOG_FILE="$VM_DIR/boot.log"

# Ensure VM dir exists before any log() call
mkdir -p "$VM_DIR"

# Configuration
VM_NAME="NEXUS-OS"
VM_MEMORY="2048"
VM_CPUS="2"
VM_DISK_SIZE="20G"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log() {
    echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$VMLOG_FILE"
    echo -e "$1"
}

print_banner() {
    echo -e "${CYAN}"
    echo "  ____   ___   __  __   _   _  _____ "
    echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
    echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
    echo " |  _ <| |_| || |  | | | |\\  || |___ "
    echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
    echo ""
    echo "  NEXUS OS - Auto VMware Boot Manager"
    echo -e "${NC}"
}

check_prerequisites() {
    log "${YELLOW}Checking prerequisites...${NC}"
    
    local missing=()
    
    # Check for ISO
    if [ ! -f "$ISO_PATH" ]; then
        missing+=("NEXUS OS ISO (run 'make' first)")
    fi
    
    # Check for VMware
    if ! command -v vmware &> /dev/null && \
       ! command -v vmplayer &> /dev/null && \
       ! command -v vmrun &> /dev/null; then
        missing+=("VMware (Workstation/Player)")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        log "${RED}Missing prerequisites:${NC}"
        for item in "${missing[@]}"; do
            log "  - $item"
        done
        return 1
    fi
    
    log "${GREEN}✓ All prerequisites met${NC}"
    return 0
}

create_vm_directory() {
    if [ ! -d "$VM_DIR" ]; then
        log "${YELLOW}Creating VM directory: $VM_DIR${NC}"
        mkdir -p "$VM_DIR"
    fi
}

create_vm_config() {
    log "${YELLOW}Creating VMware VM configuration...${NC}"
    
    cat > "$VMX_FILE" << EOF
config.version = "8"
virtualHW.version = "19"
virtualHW.productCompatibility = "hosted"
displayName = "$VM_NAME"
guestOS = "other-64"
nvram = "$VM_NAME.nvram"
uuid.location = "56 4d $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 ))"
uuid.bios = "56 4d $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 ))"

# Memory and CPU
memsize = "$VM_MEMORY"
numvcpus = "$VM_CPUS"
cpuid.coresPerSocket = "2"

# Boot configuration
boot.order = "cdrom"
bios.bootOrder = "cdrom,hdd,ethernet"

# IDE CD-ROM for ISO
ide1:0.present = "TRUE"
ide1:0.fileName = "$ISO_PATH"
ide1:0.deviceType = "cdrom-image"
ide1:0.startConnected = "TRUE"
ide1:0.autodetect = "FALSE"

# Hard disk (optional, for persistence)
scsi0.present = "TRUE"
scsi0.virtualDev = "lsilogic"
scsi0:0.present = "TRUE"
scsi0:0.fileName = "$VM_NAME.vmdk"
scsi0:0.writeThrough = "FALSE"

# Network
ethernet0.present = "TRUE"
ethernet0.connectionType = "nat"
ethernet0.virtualDev = "vmxnet3"
ethernet0.wakeOnPcktRcv = "FALSE"
ethernet0.addressType = "generated"

# USB
usb.present = "TRUE"
ehci.present = "TRUE"
uhci.present = "TRUE"

# Sound (optional)
sound.present = "FALSE"

# Display
svga.vramSize = "128"
svga.autodetect = "TRUE"
mks.enable3d = "FALSE"

# Power management
powerType.powerOff = "soft"
powerType.powerOn = "soft"
powerType.suspend = "soft"
powerType.reset = "soft"

# VMware Tools (not needed for NEXUS OS but good to have)
isolation.tools.hgfs.disable = "FALSE"
isolation.tools.dnd.disable = "FALSE"
isolation.tools.copy.disable = "FALSE"
isolation.tools.paste.disable = "FALSE"

# VMware-specific optimizations for NEXUS OS
monitor.virtual_exec = "software"
monitor.virtual_mmu = "software"
vhv.enable = "TRUE"
EOF

    log "${GREEN}✓ VM configuration created: $VMX_FILE${NC}"
}

create_virtual_disk() {
    local vmdk_path="$VM_DIR/$VM_NAME.vmdk"
    
    if [ ! -f "$vmdk_path" ]; then
        log "${YELLOW}Creating virtual disk: $VM_DISK_SIZE${NC}"
        
        # Use vmware-vdiskmanager if available
        if command -v vmware-vdiskmanager &> /dev/null; then
            vmware-vdiskmanager -c -s "$VM_DISK_SIZE" -a lsilogic -t 0 "$vmdk_path"
        else
            # Create sparse disk using dd
            dd if=/dev/zero of="$vmdk_path" bs=1M count=1 seek=$((20 * 1024)) 2>/dev/null
        fi
        
        log "${GREEN}✓ Virtual disk created: $vmdk_path${NC}"
    else
        log "${GREEN}✓ Virtual disk already exists: $vmdk_path${NC}"
    fi
}

check_vm_running() {
    if command -v vmrun &> /dev/null; then
        if vmrun list 2>/dev/null | grep -q "$VMX_FILE"; then
            return 0
        fi
    fi
    return 1
}

launch_vmware() {
    log "${BLUE}Launching VMware...${NC}"
    
    # Check if VM is already running
    if check_vm_running; then
        log "${YELLOW}VM is already running. Stopping...${NC}"
        vmrun stop "$VMX_FILE" nogui 2>/dev/null || true
        sleep 2
    fi
    
    # Find VMware executable
    local vmware_cmd=""
    
    if command -v vmplayer &> /dev/null; then
        vmware_cmd="vmplayer"
        log "Using VMware Player"
    elif command -v vmware &> /dev/null; then
        vmware_cmd="vmware"
        log "Using VMware Workstation"
    elif command -v vmrun &> /dev/null; then
        vmware_cmd="vmrun"
        log "Using vmrun (headless)"
    fi
    
    if [ -n "$vmware_cmd" ]; then
        case "$vmware_cmd" in
            vmrun)
                log "Starting VM in headless mode..."
                vmrun start "$VMX_FILE" nogui
                ;;
            *)
                log "Starting VM with GUI..."
                $vmware_cmd "$VMX_FILE" &
                ;;
        esac
        
        log "${GREEN}✓ VMware launched with NEXUS OS${NC}"
        return 0
    else
        log "${RED}Error: No VMware executable found${NC}"
        return 1
    fi
}

show_boot_info() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}NEXUS OS Boot Information${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${CYAN}Virtualization Mode:${NC} VMware (Default)"
    echo -e "${CYAN}VM Name:${NC} $VM_NAME"
    echo -e "${CYAN}Memory:${NC} $VM_MEMORY MB"
    echo -e "${CYAN}CPUs:${NC} $VM_CPUS"
    echo -e "${CYAN}ISO:${NC} $ISO_PATH"
    echo -e "${CYAN}VM Directory:${NC} $VM_DIR"
    echo ""
    echo -e "${YELLOW}Boot Menu Options:${NC}"
    echo "  1. NEXUS OS (VMware Mode - Default)"
    echo "  2. NEXUS OS (Safe Mode)"
    echo "  3. NEXUS OS (Debug Mode)"
    echo "  4. NEXUS OS (Native Mode)"
    echo ""
    echo -e "${GREEN}The VM will boot automatically into VMware Mode.${NC}"
    echo -e "${GREEN}VMware-specific optimizations will be enabled.${NC}"
    echo ""
}

cleanup_old_vm() {
    # Remove old NVRAM to ensure clean boot
    local nvram_file="$VM_DIR/$VM_NAME.nvram"
    if [ -f "$nvram_file" ] && [ "$1" == "--clean" ]; then
        log "${YELLOW}Removing old NVRAM for clean boot...${NC}"
        rm -f "$nvram_file"
    fi
}

main() {
    print_banner
    echo ""
    
    # Parse arguments
    local clean_boot=false
    local recreate_config=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_boot=true
                shift
                ;;
            --recreate)
                recreate_config=true
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --clean     Remove old NVRAM for clean boot"
                echo "  --recreate  Recreate VM configuration"
                echo "  --help      Show this help"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi
    
    # Create VM directory
    create_vm_directory
    
    # Create or recreate VM config
    if [ "$recreate_config" == true ] || [ ! -f "$VMX_FILE" ]; then
        create_vm_config
        create_virtual_disk
    else
        log "${GREEN}✓ VM configuration already exists${NC}"
    fi
    
    # Cleanup if requested
    if [ "$clean_boot" == true ]; then
        cleanup_old_vm --clean
    fi
    
    # Show boot info
    show_boot_info
    
    # Launch VMware
    launch_vmware
    
    log ""
    log "${GREEN}========================================${NC}"
    log "${GREEN}NEXUS OS is booting in VMware!${NC}"
    log "${GREEN}========================================${NC}"
    log ""
    log "VMware Mode optimizations:"
    log "  - VMware paravirtualized drivers"
    log "  - vmxnet3 network adapter"
    log "  - Optimized memory management"
    log "  - VMware hypervisor detection"
    log ""
}

# Run main function
main "$@"
