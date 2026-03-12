#!/bin/bash
# NEXUS OS - VMware Auto-Launch Script
# Detects if running in VMware, if not launches VMware with the ISO

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO_PATH="${SCRIPT_DIR}/build/nexus-kernel.iso"
VM_NAME="NEXUS-OS"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}"
echo "  ____   ___   __  __   _   _  _____ "
echo " |  _ \\ / _ \\ |  \\/  | | \\ | || ____|"
echo " | |_) | | | || |\\/| | |  \\| ||  _|  "
echo " |  _ <| |_| || |  | | | |\\  || |___ "
echo " |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|"
echo ""
echo "  NEXUS OS - VMware Auto-Launcher"
echo -e "${NC}"

# Check if ISO exists
if [ ! -f "$ISO_PATH" ]; then
    echo -e "${RED}Error: NEXUS OS ISO not found at: $ISO_PATH${NC}"
    echo "Please build the ISO first with: make"
    exit 1
fi

# Function to detect if running in VMware
detect_vmware() {
    # Check for VMware-specific files
    if [ -d "/proc/bus/pci" ]; then
        # Check PCI devices for VMware
        if lspci 2>/dev/null | grep -qi "vmware"; then
            return 0
        fi
    fi
    
    # Check for VMware tools
    if command -v vmware-toolbox-cmd &> /dev/null; then
        return 0
    fi
    
    # Check dmidecode for VMware
    if command -v dmidecode &> /dev/null; then
        if dmidecode -s system-manufacturer 2>/dev/null | grep -qi "vmware"; then
            return 0
        fi
    fi
    
    # Check for VMware virtual hardware
    if [ -e "/dev/mem" ]; then
        # Could check for VMware signatures in memory
        :
    fi
    
    return 1
}

# Function to detect virtualization
detect_virtualization() {
    echo -e "${YELLOW}Detecting virtualization environment...${NC}"
    
    # Check for various VM types
    if detect_vmware; then
        echo -e "${GREEN}✓ Running in VMware${NC}"
        return 0
    fi
    
    # Check for VirtualBox
    if lspci 2>/dev/null | grep -qi "virtualbox"; then
        echo -e "${GREEN}✓ Running in VirtualBox${NC}"
        return 1
    fi
    
    # Check for QEMU/KVM
    if lspci 2>/dev/null | grep -qi "qemu\|kvm"; then
        echo -e "${GREEN}✓ Running in QEMU/KVM${NC}"
        return 2
    fi
    
    # Check for Hyper-V
    if [ -d "/sys/hypervisor" ]; then
        echo -e "${GREEN}✓ Running in Hyper-V${NC}"
        return 3
    fi
    
    echo -e "${YELLOW}✓ Running on bare metal (no virtualization detected)${NC}"
    return 255
}

# Function to launch VMware
launch_vmware() {
    echo ""
    echo -e "${BLUE}Launching VMware with NEXUS OS ISO...${NC}"
    echo ""
    
    # Check for VMware Workstation
    if command -v vmware &> /dev/null; then
        echo "Starting VMware Workstation..."
        vmware "$@" &
        return $?
    fi
    
    # Check for VMware Player
    if command -v vmplayer &> /dev/null; then
        echo "Starting VMware Player..."
        vmplayer "$@" &
        return $?
    fi
    
    # Check for VMware Fusion (macOS)
    if command -v /Applications/VMware\ Fusion.app/Contents/Library/vmrun &> /dev/null; then
        echo "Starting VMware Fusion..."
        /Applications/VMware\ Fusion.app/Contents/Library/vmrun start "$@" &
        return $?
    fi
    
    echo -e "${RED}VMware not found. Please install VMware Workstation or Player.${NC}"
    echo ""
    echo "Download from: https://www.vmware.com/products/workstation-player.html"
    return 1
}

# Function to create VMware VM configuration
create_vm_config() {
    local vm_dir="$HOME/vmware-vm/NEXUS-OS"
    local vmx_file="$vm_dir/NEXUS-OS.vmx"
    
    echo -e "${YELLOW}Creating VMware VM configuration...${NC}"
    
    mkdir -p "$vm_dir"
    
    # Create VMX configuration
    cat > "$vmx_file" << EOF
config.version = "8"
virtualHW.version = "19"
memsize = "2048"
numvcpus = "2"
cpuid.coresPerSocket = "2"
scsi0.present = "TRUE"
scsi0.sharedBus = "none"
scsi0.virtualDev = "lsilogic"
ethernet0.present = "TRUE"
ethernet0.connectionType = "nat"
ethernet0.virtualDev = "vmxnet3"
ethernet0.wakeOnPcktRcv = "FALSE"
usb.present = "TRUE"
ehci.present = "TRUE"
sound.present = "TRUE"
sound.virtualDev = "hdaudio"
floppy0.present = "FALSE"
displayName = "NEXUS OS"
guestOS = "other-64"
nvram = "NEXUS-OS.nvram"
virtualHW.productCompatibility = "hosted"
powerType.powerOff = "soft"
powerType.powerOn = "soft"
powerType.suspend = "soft"
powerType.reset = "soft"
ide1:0.present = "TRUE"
ide1:0.fileName = "$ISO_PATH"
ide1:0.deviceType = "cdrom-image"
ide1:0.startConnected = "TRUE"
boot.order = "cdrom"
EOF

    echo -e "${GREEN}✓ VM configuration created: $vmx_file${NC}"
    echo "$vmx_file"
}

# Main execution
echo ""
detect_virtualization
VIRT_STATUS=$?

echo ""

if [ $VIRT_STATUS -eq 0 ]; then
    # Already in VMware - can boot directly
    echo -e "${GREEN}Already running in VMware. You can boot the ISO directly.${NC}"
    echo ""
    echo "ISO location: $ISO_PATH"
    echo ""
    echo "To mount in existing VM:"
    echo "  1. VM Settings > CD/DVD"
    echo "  2. Select: $ISO_PATH"
    echo "  3. Ensure 'Connect at power on' is checked"
    echo "  4. Boot the VM"
elif [ $VIRT_STATUS -eq 255 ]; then
    # Bare metal - offer to launch VMware
    echo -e "${YELLOW}Running on bare metal.${NC}"
    echo ""
    echo "NEXUS OS default mode is VMware."
    echo "Would you like to launch VMware and boot the ISO?"
    echo ""
    read -p "Launch VMware? [Y/n]: " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]] || [ -z "$REPLY" ]; then
        # Check if VM config exists
        VM_DIR="$HOME/vmware-vm/NEXUS-OS"
        VMX_FILE="$VM_DIR/NEXUS-OS.vmx"
        
        if [ ! -f "$VMX_FILE" ]; then
            VMX_FILE=$(create_vm_config)
        fi
        
        launch_vmware "$VMX_FILE"
        
        echo ""
        echo -e "${GREEN}VMware is starting with NEXUS OS...${NC}"
        echo ""
        echo "The VM will boot automatically into NEXUS OS."
        echo "VMware Mode optimizations will be enabled."
    else
        echo "You can still boot the ISO manually in VMware."
        echo "ISO location: $ISO_PATH"
    fi
else
    # Other VM - inform user
    echo -e "${YELLOW}Running in a virtual machine (not VMware).${NC}"
    echo ""
    echo "NEXUS OS is optimized for VMware but will work in other VMs."
    echo "Some VMware-specific optimizations may not be available."
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}NEXUS OS Boot Options:${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "1. VMware Mode (Default) - Optimized for VMware"
echo "2. Safe Mode - Minimal drivers, no SMP"
echo "3. Debug Mode - Verbose logging"
echo "4. Native Mode - Bare metal (no virtualization)"
echo ""
echo "Select mode at boot menu or press Enter for default."
echo ""
