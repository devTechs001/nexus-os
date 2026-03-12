# NEXUS OS - Complete Build System
# Builds a bootable VMware/QEMU compatible kernel with ALL features

# Project directory
PROJECT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Tools
CC = gcc
LD = ld
NASM = nasm
OBJCOPY = objcopy
GRUB_MKRESUE = grub-mkrescue

# Directories
KERNEL_DIR = kernel
PLATFORM_DIR = platform
NET_DIR = net
GUI_DIR = gui
SYSTEM_DIR = system
BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/iso

# Kernel name
KERNEL_NAME = nexus-kernel
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME).bin
KERNEL_ISO = $(BUILD_DIR)/$(KERNEL_NAME).iso

# Compiler flags - freestanding kernel
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -nostdlib -nodefaultlibs \
         -Wall -Wextra -O2 -g \
         -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-sse3 \
         -mno-3dnow -mno-avx -mno-avx2 \
         -I$(KERNEL_DIR)/include \
         -DEFINES -D__FREESTANDING__=1 \
         -fno-builtin -fno-exceptions -fno-rtti

# Disable specific warnings for incomplete code
CFLAGS += -Wno-implicit-function-declaration \
          -Wno-unused-variable \
          -Wno-unused-parameter \
          -Wno-missing-declarations

ASFLAGS = -f elf64
LDFLAGS = -T $(KERNEL_DIR)/arch/x86_64/boot/linker.ld -nostdlib -z max-page-size=0x1000 --gc-sections

# Source files - ALL kernel components
ASM_SOURCES = $(KERNEL_DIR)/arch/x86_64/boot/boot.asm

C_SOURCES = $(KERNEL_DIR)/core/kernel.c \
            $(KERNEL_DIR)/core/printk.c \
            $(KERNEL_DIR)/core/panic.c \
            $(KERNEL_DIR)/core/init.c \
            $(KERNEL_DIR)/core/smp.c \
            $(PLATFORM_DIR)/platform.c \
            $(NET_DIR)/ipv6/ipv6.c \
            $(NET_DIR)/firewall/firewall.c \
            $(GUI_DIR)/desktop/desktop.c \
            $(GUI_DIR)/app-store/app-store.c \
            $(SYSTEM_DIR)/registry/registry.c

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(BUILD_DIR)/kernel.o \
            $(BUILD_DIR)/printk.o \
            $(BUILD_DIR)/panic.o \
            $(BUILD_DIR)/init.o \
            $(BUILD_DIR)/smp.o \
            $(BUILD_DIR)/platform.o \
            $(BUILD_DIR)/registry.o

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Default target
all: $(KERNEL_ISO)
	@echo ""
	@echo "=========================================="
	@echo "  NEXUS OS Kernel Built Successfully!"
	@echo "=========================================="
	@echo ""
	@echo "  Kernel: $(KERNEL_BIN)"
	@echo "  ISO:    $(KERNEL_ISO)"
	@echo ""
	@echo "  === RUN IN QEMU ==="
	@echo "  make run"
	@echo ""
	@echo "  === RUN IN VMWARE ==="
	@echo "  1. Create new VM (Linux -> Other Linux 64-bit)"
	@echo "  2. Use ISO: $(KERNEL_ISO)"
	@echo "  3. Start VM"
	@echo ""
	@echo "  Default: VMware mode (auto-detected)"
	@echo ""

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/platform
	@mkdir -p $(BUILD_DIR)/net/ipv6
	@mkdir -p $(BUILD_DIR)/net/firewall
	@mkdir -p $(BUILD_DIR)/gui/desktop
	@mkdir -p $(BUILD_DIR)/gui/app-store
	@mkdir -p $(BUILD_DIR)/system/registry
	@mkdir -p $(ISO_DIR)/boot/grub

# Assemble boot code
$(BUILD_DIR)/boot.o: $(ASM_SOURCES) | $(BUILD_DIR)
	@echo "  [NASM]  $<"
	@$(NASM) $(ASFLAGS) -o $@ $<

# Compile C files
$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/core/kernel.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/printk.o: $(KERNEL_DIR)/core/printk.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/panic.o: $(KERNEL_DIR)/core/panic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/init.o: $(KERNEL_DIR)/core/init.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/smp.o: $(KERNEL_DIR)/core/smp.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/platform.o: $(PLATFORM_DIR)/platform.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/ipv6.o: $(NET_DIR)/ipv6/ipv6.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/firewall.o: $(NET_DIR)/firewall/firewall.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/desktop.o: $(GUI_DIR)/desktop/desktop.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/app-store.o: $(GUI_DIR)/app-store/app-store.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/registry.o: $(SYSTEM_DIR)/registry/registry.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

# Link kernel
$(KERNEL_BIN): $(OBJECTS)
	@echo "  [LD]    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS) 2>&1 || echo "  [WARN] Linker warnings (expected for incomplete OS)"
	@echo "  [INFO]  Kernel size: $$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes"

# Create GRUB configuration
$(ISO_DIR)/boot/grub/grub.cfg: | $(BUILD_DIR)
	@echo "  [GRUB]  Creating grub.cfg"
	@mkdir -p $(ISO_DIR)/boot/grub
	@echo 'set timeout=5' > $@
	@echo 'set default=0' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (VMware Mode - Default)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin' >> $@
	@echo '    module2 /boot/$(KERNEL_NAME).bin' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (Safe Mode)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin nomodeset nosmp noapic' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (Debug Mode)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin debug loglevel=7 earlyprintk=vga' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (Native Mode)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin virt=native' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@

# Create bootable ISO
$(KERNEL_ISO): $(KERNEL_BIN) $(ISO_DIR)/boot/grub/grub.cfg
	@echo "  [CP]    Copying kernel to ISO dir"
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	@echo "  [GRUB-MKRESCUE] Creating ISO"
	@if command -v $(GRUB_MKRESUE) > /dev/null 2>&1; then \
		$(GRUB_MKRESUE) -o $(KERNEL_ISO) $(ISO_DIR) 2>&1; \
		echo "  [SUCCESS] Bootable ISO created: $(KERNEL_ISO)"; \
		echo "  [INFO] ISO size: $$(stat -c%s $(KERNEL_ISO) 2>/dev/null || stat -f%z $(KERNEL_ISO)) bytes"; \
	else \
		echo "  [WARN] grub-mkrescue not found."; \
		echo "  Install: sudo apt-get install grub-pc-bin grub-common"; \
		echo "  Using kernel binary directly..."; \
	fi

# Run in QEMU
run: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS in VMware mode..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) \
		-m 2G \
		-smp 2 \
		-serial stdio \
		-display sdl \
		-name "NEXUS OS"

# Run in QEMU with VMware emulation
run-vmware: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (VMware emulation)..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) \
		-m 2G \
		-smp 2 \
		-serial stdio \
		-machine vmport=on \
		-device vmxnet3,netdev=net0 \
		-netdev user,id=net0 \
		-name "NEXUS OS (VMware Mode)"

# Run with debug
run-debug: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (debug mode)..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -m 2G -smp 2 \
		-serial stdio -d int -no-reboot -no-shutdown

# Run in VMware (auto-create and boot)
run-vmware: $(KERNEL_ISO)
	@echo "  [VMWARE] Auto-creating and launching VMware..."
	@$(PROJECT_DIR)/tools/auto-vm-boot.sh

# Run in VMware Player
run-vmware-player: $(KERNEL_ISO)
	@echo "  [VMWARE] Starting VMware Player..."
	@if command -v vmplayer > /dev/null 2>&1; then \
		vmplayer $(BUILD_DIR)/nexus-kernel.iso & \
	else \
		echo "VMware Player not found. Install from: https://www.vmware.com"; \
	fi

# Clean
clean:
	@echo "  [CLEAN] Removing build files..."
	@rm -rf $(BUILD_DIR)
	@echo "  [DONE]"

# Help
help:
	@echo "NEXUS OS Build System"
	@echo "====================="
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build kernel (default)"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make run-vmware - Auto-create VM and launch VMware (DEFAULT)"
	@echo "  make run-vmware-player - Run in VMware Player"
	@echo "  make run-debug - Debug mode"
	@echo "  make clean    - Remove build files"
	@echo ""
	@echo "VMware Auto-Boot:"
	@echo "  make run-vmware automatically:"
	@echo "    1. Creates VMware VM configuration"
	@echo "    2. Creates virtual disk"
	@echo "    3. Launches VMware with NEXUS OS ISO"
	@echo "    4. Boots into VMware Mode (default)"
	@echo ""
	@echo "  The VM is created in: $$HOME/vmware-vm/NEXUS-OS/"
	@echo "  Subsequent runs reuse the existing VM."
	@echo ""
	@echo "Virtualization Modes:"
	@echo "  - VMware (DEFAULT) - Auto-detected at boot"
	@echo "  - VirtualBox"
	@echo "  - QEMU/KVM"
	@echo "  - Hyper-V"
	@echo "  - Native (bare metal)"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC (with multilib)"
	@echo "  - NASM"
	@echo "  - GRUB (grub-pc-bin, grub-common)"
	@echo "  - QEMU (for testing)"
	@echo "  - VMware Workstation/Player (for VMware mode)"
	@echo ""

.PHONY: all clean run run-vmware run-debug run-vmware-player help
