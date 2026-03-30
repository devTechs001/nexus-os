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

# VM Presets
VM_PRESET="standard"  # minimal, standard, performance

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
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

print_presets() {
    echo ""
    echo -e "${CYAN}Available VM Presets:${NC}"
    echo "  minimal    - 1 CPU, 1GB RAM, 10GB disk"
    echo "  standard   - 2 CPUs, 2GB RAM, 20GB disk (default)"
    echo "  performance - 4 CPUs, 4GB RAM, 40GB disk"
    echo ""
    echo -e "Usage: $0 --preset <preset>${NC}"
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
# NEXUS OS - VMware Configuration
# Generated for NEXUS OS Virtualization

config.version = "8"
virtualHW.version = "19"
virtualHW.productCompatibility = "hosted"
displayName = "$VM_NAME"
guestOS = "other-64"
nvram = "$VM_NAME.nvram"
uuid.location = "56 4d $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 ))"
uuid.bios = "56 4d $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 )) $(( RANDOM % 256 ))"

# ============================================
# ENHANCED HARDWARE CONFIGURATION
# ============================================

# Memory and CPU - Optimized for NEXUS OS
memsize = "$VM_MEMORY"
numvcpus = "$VM_CPUS"
cpuid.coresPerSocket = "2"
sched.cpu.latencySensitivity = "normal"
sched.cpu.hyperthreadedKernel = "TRUE"

# ============================================
# ENHANCED GRAPHICS CONFIGURATION
# ============================================

# SVGA Display with 3D Acceleration
svga.present = "TRUE"
svga.vramSize = "268435456"
svga.numDisplays = "1"
svga.displayName = "Auto-detect"
svga.guestBackedPrimaryAwareEnabled = "TRUE"

# 3D Graphics Acceleration
mks.enable3d = "TRUE"
mks.maxWidth = "2560"
mks.maxHeight = "1600"
mks.overrideWindowedMode = "TRUE"
mks.windowedDirectMode = "FALSE"

# Fullscreen resolution support
pref.mruDisplays.userLevel = "6"
pref.mruDisplays.enableAllDisplays = "TRUE"

# ============================================
# BOOT CONFIGURATION
# ============================================

boot.order = "cdrom"
bios.bootOrder = "cdrom,hdd,ethernet"
bios.bootDelay = "2000"
bios.displayName = "NEXUS OS - Universal Operating System"

# ============================================
# ENHANCED STORAGE CONFIGURATION
# ============================================

# IDE CD-ROM for ISO
ide1:0.present = "TRUE"
ide1:0.fileName = "$ISO_PATH"
ide1:0.deviceType = "cdrom-image"
ide1:0.startConnected = "TRUE"
ide1:0.autodetect = "FALSE"
ide1:0.mode = "persistent"

# SCSI Controller for Hard disk
scsi0.present = "TRUE"
scsi0.virtualDev = "lsilogic"
scsi0.sharedBus = "none"
scsi0:0.present = "TRUE"
scsi0:0.fileName = "$VM_NAME.vmdk"
scsi0:0.writeThrough = "FALSE"
scsi0:0.deviceType = "scsi-hardDisk"

# ============================================
# ADVANCED NETWORK CONFIGURATION
# ============================================

# Primary Network Adapter (NAT)
ethernet0.present = "TRUE"
ethernet0.connectionType = "nat"
ethernet0.virtualDev = "vmxnet3"
ethernet0.wakeOnPcktRcv = "FALSE"
ethernet0.addressType = "generated"
ethernet0.MACAddress = "00:0c:29:$(printf '%02x' $((RANDOM%256))):$(printf '%02x' $((RANDOM%256))):$(printf '%02x' $((RANDOM%256)))"

# Secondary Network Adapter (Host-only)
ethernet1.present = "TRUE"
ethernet1.connectionType = "hostonly"
ethernet1.virtualDev = "vmxnet3"
ethernet1.addressType = "generated"

# ============================================
# USB AND INPUT DEVICES
# ============================================

# USB Controller
usb.present = "TRUE"
usb.xhci.present = "TRUE"
ehci.present = "TRUE"
uhci.present = "TRUE"

# Pointing devices
pointingDevice.present = "TRUE"
mouse.vusb.enable = "TRUE"

# Keyboard
keyboard.present = "TRUE"

# ============================================
# SOUND CONFIGURATION
# ============================================

sound.present = "TRUE"
sound.virtualDev = "hdaudio"
sound.fileName = "-1"
sound.autodetect = "TRUE"

# ============================================
# SHARED FOLDERS
# ============================================

# Enable HGFS for shared folders
isolation.tools.hgfs.disable = "FALSE"
isolation.tools.hgfs.folderAutoMount = "TRUE"

# ============================================
# ENHANCED GUEST FEATURES
# ============================================

# Copy/Paste and Drag/Drop
isolation.tools.dnd.disable = "FALSE"
isolation.tools.copy.disable = "FALSE"
isolation.tools.paste.disable = "FALSE"

# Time synchronization
tools.syncTime = "TRUE"
time.synchronize.continue = "TRUE"
time.synchronize.restore = "TRUE"
time.synchronize.resume.disk = "TRUE"
time.synchronize.resume.memory = "TRUE"

# ============================================
# VMWARE SPECIFIC OPTIMIZATIONS
# ============================================

# Hardware virtualization
vhv.enable = "TRUE"

# Memory management
sched.mem.pshare.enable = "TRUE"
sched.mem.min = "512"
sched.mem.max = "$VM_MEMORY"

# CPU management
monitor.virtual_exec = "hardware"
monitor.virtual_mmu = "hardware"

# I/O management
sched.scsi0:0.shares = "normal"
sched.scsi0:0.latency = "normal"

# Network performance
net.rmtDev0.enable = "TRUE"
net.tcpipEnabled = "TRUE"

# ============================================
# POWER MANAGEMENT
# ============================================

powerType.powerOff = "soft"
powerType.powerOn = "soft"
powerType.suspend = "soft"
powerType.reset = "soft"

# ============================================
# ADVANCED FEATURES
# ============================================

# Logging
logging = "TRUE"
log.rotateSize = "100000"
log.keepOld = "10"

# Feature flags
featureKey.proxy = "FALSE"
vmx.buildType = "release"

# Guest memory management
disable_accel = "FALSE"
numa.autosize.onceAuto = "FALSE"
numa.autosize = "FALSE"
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

apply_vm_preset() {
    local preset=$1
    
    case $preset in
        minimal)
            VM_MEMORY="1024"
            VM_CPUS="1"
            VM_DISK_SIZE="10G"
            log "${YELLOW}Applied minimal preset: 1 CPU, 1GB RAM, 10GB disk${NC}"
            ;;
        standard)
            VM_MEMORY="2048"
            VM_CPUS="2"
            VM_DISK_SIZE="20G"
            log "${YELLOW}Applied standard preset: 2 CPUs, 2GB RAM, 20GB disk${NC}"
            ;;
        performance)
            VM_MEMORY="4096"
            VM_CPUS="4"
            VM_DISK_SIZE="40G"
            log "${YELLOW}Applied performance preset: 4 CPUs, 4GB RAM, 40GB disk${NC}"
            ;;
        *)
            log "${RED}Unknown preset: $preset${NC}"
            print_presets
            return 1
            ;;
    esac
    
    VM_PRESET=$preset
    return 0
}

show_vm_status() {
    echo ""
    echo -e "${MAGENTA}╔════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║       NEXUS OS - VM Status            ║${NC}"
    echo -e "${MAGENTA}╚════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}VM Configuration:${NC}"
    echo "  Name:       $VM_NAME"
    echo "  Preset:     $VM_PRESET"
    echo "  Memory:     $VM_MEMORY MB"
    echo "  CPUs:       $VM_CPUS"
    echo "  Disk:       $VM_DISK_SIZE"
    echo "  ISO:        $ISO_PATH"
    echo ""
    echo -e "${CYAN}VMware Features Enabled:${NC}"
    echo "  ✓ Hardware Virtualization (VT-x/AMD-V)"
    echo "  ✓ 3D Graphics Acceleration"
    echo "  ✓ vmxnet3 Network Adapter"
    echo "  ✓ USB 3.0 (xHCI)"
    echo "  ✓ Shared Folders (HGFS)"
    echo "  ✓ Copy/Paste Integration"
    echo "  ✓ Time Synchronization"
    echo "  ✓ High Resolution Display"
    echo ""
}

vm_operations() {
    local operation=$1
    
    case $operation in
        start)
            log "${BLUE}Starting VM...${NC}"
            if command -v vmrun &> /dev/null; then
                vmrun start "$VMX_FILE" nogui
                log "${GREEN}VM started${NC}"
            else
                launch_vmware
            fi
            ;;
        stop)
            log "${YELLOW}Stopping VM...${NC}"
            if command -v vmrun &> /dev/null; then
                vmrun stop "$VMX_FILE" nogui
                log "${GREEN}VM stopped${NC}"
            fi
            ;;
        reset)
            log "${YELLOW}Resetting VM...${NC}"
            if command -v vmrun &> /dev/null; then
                vmrun reset "$VMX_FILE" nogui
                log "${GREEN}VM reset${NC}"
            fi
            ;;
        delete)
            log "${RED}Deleting VM...${NC}"
            if command -v vmrun &> /dev/null; then
                vmrun deleteVM "$VMX_FILE"
                log "${GREEN}VM deleted${NC}"
            fi
            ;;
        list)
            if command -v vmrun &> /dev/null; then
                vmrun list
            else
                log "${RED}vmrun not found${NC}"
            fi
            ;;
        *)
            echo "Usage: $0 <start|stop|reset|delete|list>"
            ;;
    esac
}

main() {
    print_banner
    echo ""
    
    # Parse arguments
    local clean_boot=false
    local recreate_config=false
    local operation=""
    
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
            --preset)
                VM_PRESET="$2"
                shift 2
                ;;
            --memory)
                VM_MEMORY="$2"
                shift 2
                ;;
            --cpus)
                VM_CPUS="$2"
                shift 2
                ;;
            --disk)
                VM_DISK_SIZE="$2"
                shift 2
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --clean        Remove old NVRAM for clean boot"
                echo "  --recreate     Recreate VM configuration"
                echo "  --preset P     VM preset (minimal/standard/performance)"
                echo "  --memory MB    Set memory in MB"
                echo "  --cpus N       Set number of CPUs"
                echo "  --disk SIZE    Set disk size (e.g., 20G)"
                echo ""
                echo "VM Operations:"
                echo "  start          Start the VM"
                echo "  stop           Stop the VM"
                echo "  reset           Reset the VM"
                echo "  delete         Delete the VM"
                echo "  list           List running VMs"
                echo ""
                print_presets
                exit 0
                ;;
            start|stop|reset|delete|list)
                operation=$1
                shift
                ;;
            *)
                echo "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
    
    # Handle VM operations
    if [ -n "$operation" ]; then
        vm_operations "$operation"
        exit 0
    fi
    
    # Apply VM preset
    if [ -n "$VM_PRESET" ]; then
        apply_vm_preset "$VM_PRESET"
    fi
    
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
    show_vm_status
    
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
    log "  - Hardware 3D acceleration"
    log "  - Optimized memory management"
    log "  - VMware hypervisor detection"
    log "  - Guest Tools integration ready"
    log ""
}

# Run main function
main "$@"
