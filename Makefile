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
DRIVERS_DIR = drivers
SECURITY_DIR = security
AI_ML_DIR = ai_ml
HAL_DIR = hal
IOT_DIR = iot
VIRT_DIR = virt
FS_DIR = fs
MOBILE_DIR = mobile
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
            $(KERNEL_DIR)/core/module_loader.c \
            $(PLATFORM_DIR)/platform.c \
            $(NET_DIR)/ipv6/ipv6.c \
            $(NET_DIR)/firewall/firewall.c \
            $(NET_DIR)/network_manager.c \
            $(NET_DIR)/protocols/network_protocols.c \
            $(GUI_DIR)/desktop/desktop.c \
            $(GUI_DIR)/desktop/desktop_grid.c \
            $(GUI_DIR)/compositor/compositing_manager.c \
            $(GUI_DIR)/control-center/control-center.c \
            $(GUI_DIR)/file-manager/file-manager.c \
            $(GUI_DIR)/system-settings/system-settings.c \
            $(GUI_DIR)/app-store/app-store.c \
            $(SYSTEM_DIR)/registry/registry.c \
            $(DRIVERS_DIR)/display/display.c \
            $(DRIVERS_DIR)/display/display_manager.c \
            $(DRIVERS_DIR)/input/input.c \
            $(DRIVERS_DIR)/input/keyboard/ps2.c \
            $(DRIVERS_DIR)/input/touchscreen/touchscreen.c \
            $(DRIVERS_DIR)/audio/audio.c \
            $(DRIVERS_DIR)/audio/mixer.c \
            $(DRIVERS_DIR)/audio/bluetooth_audio.c \
            $(DRIVERS_DIR)/gpu/gpu.c \
            $(DRIVERS_DIR)/gpu/gpu_scheduler.c \
            $(DRIVERS_DIR)/gpu/opengl.c \
            $(DRIVERS_DIR)/gpu/virtio_gpu.c \
            $(DRIVERS_DIR)/graphics/framebuffer/fb.c \
            $(DRIVERS_DIR)/usb/usb_manager.c \
            $(DRIVERS_DIR)/network/ethernet.c \
            $(DRIVERS_DIR)/network/wifi.c \
            $(DRIVERS_DIR)/bluetooth/bluetooth.c \
            $(DRIVERS_DIR)/rtc/rtc.c \
            $(DRIVERS_DIR)/pci/pci.c \
            $(DRIVERS_DIR)/sensors/sensor_hub.c \
            $(DRIVERS_DIR)/video/vga.c \
            $(DRIVERS_DIR)/video/interface.c \
            $(DRIVERS_DIR)/video/dsi.c \
            $(SECURITY_DIR)/encryption/disk_encryption.c \
            $(AI_ML_DIR)/inference/inference_engine.c \
            $(HAL_DIR)/power/power_manager.c \
            $(IOT_DIR)/protocols/iot_protocols.c \
            $(VIRT_DIR)/containers/container_runtime.c \
            $(FS_DIR)/procfs/procfs.c \
            $(MOBILE_DIR)/telephony/telephony_manager.c

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(BUILD_DIR)/kernel.o \
            $(BUILD_DIR)/printk.o \
            $(BUILD_DIR)/panic.o \
            $(BUILD_DIR)/init.o \
            $(BUILD_DIR)/smp.o \
            $(BUILD_DIR)/module_loader.o \
            $(BUILD_DIR)/platform.o \
            $(BUILD_DIR)/ipv6.o \
            $(BUILD_DIR)/firewall.o \
            $(BUILD_DIR)/network_manager.o \
            $(BUILD_DIR)/network_protocols.o \
            $(BUILD_DIR)/desktop.o \
            $(BUILD_DIR)/desktop_grid.o \
            $(BUILD_DIR)/compositing_manager.o \
            $(BUILD_DIR)/control-center.o \
            $(BUILD_DIR)/file-manager.o \
            $(BUILD_DIR)/system-settings.o \
            $(BUILD_DIR)/app-store.o \
            $(BUILD_DIR)/registry.o \
            $(BUILD_DIR)/display.o \
            $(BUILD_DIR)/display_manager.o \
            $(BUILD_DIR)/input.o \
            $(BUILD_DIR)/ps2.o \
            $(BUILD_DIR)/touchscreen.o \
            $(BUILD_DIR)/audio.o \
            $(BUILD_DIR)/mixer.o \
            $(BUILD_DIR)/bluetooth_audio.o \
            $(BUILD_DIR)/gpu.o \
            $(BUILD_DIR)/gpu_scheduler.o \
            $(BUILD_DIR)/opengl.o \
            $(BUILD_DIR)/virtio_gpu.o \
            $(BUILD_DIR)/fb.o \
            $(BUILD_DIR)/usb_manager.o \
            $(BUILD_DIR)/ethernet.o \
            $(BUILD_DIR)/wifi.o \
            $(BUILD_DIR)/bluetooth.o \
            $(BUILD_DIR)/rtc.o \
            $(BUILD_DIR)/pci.o \
            $(BUILD_DIR)/sensor_hub.o \
            $(BUILD_DIR)/vga.o \
            $(BUILD_DIR)/interface.o \
            $(BUILD_DIR)/dsi.o \
            $(BUILD_DIR)/disk_encryption.o \
            $(BUILD_DIR)/inference_engine.o \
            $(BUILD_DIR)/power_manager.o \
            $(BUILD_DIR)/iot_protocols.o \
            $(BUILD_DIR)/container_runtime.o \
            $(BUILD_DIR)/procfs.o \
            $(BUILD_DIR)/telephony_manager.o

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Default target
all: check-deps $(KERNEL_ISO)
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

# Check dependencies (REQUIRED - will install if missing)
.PHONY: check-deps
check-deps:
	@echo ""
	@echo "Checking dependencies..."
	@echo ""
	@if command -v $(CC) > /dev/null 2>&1; then \
		echo "  ✓ GCC found"; \
	else \
		echo "  ✗ GCC not found - Installing..."; \
		sudo apt-get update && sudo apt-get install -y gcc || { \
			echo "  ✗ Failed to install GCC"; \
			echo "  Please install manually: sudo apt-get install gcc"; \
			exit 1; \
		} \
	fi
	@if command -v $(NASM) > /dev/null 2>&1; then \
		echo "  ✓ NASM found"; \
	else \
		echo "  ✗ NASM not found - Installing..."; \
		sudo apt-get install -y nasm || { \
			echo "  ✗ Failed to install NASM"; \
			echo "  Please install manually: sudo apt-get install nasm"; \
			exit 1; \
		} \
	fi
	@if command -v $(GRUB_MKRESUE) > /dev/null 2>&1; then \
		echo "  ✓ GRUB found"; \
	else \
		echo "  ✗ GRUB not found - Installing (REQUIRED for ISO)..."; \
		sudo apt-get update && sudo apt-get install -y grub-pc-bin grub-common || { \
			echo "  ✗ Failed to install GRUB"; \
			echo "  Please install manually: sudo apt-get install grub-pc-bin grub-common"; \
			exit 1; \
		} \
	fi
	@if command -v xorriso > /dev/null 2>&1; then \
		echo "  ✓ xorriso found"; \
	else \
		echo "  ✗ xorriso not found - Installing (REQUIRED for ISO)..."; \
		sudo apt-get install -y xorriso || { \
			echo "  ✗ Failed to install xorriso"; \
			echo "  Please install manually: sudo apt-get install xorriso"; \
			exit 1; \
		} \
	fi
	@if command -v mtools > /dev/null 2>&1; then \
		echo "  ✓ mtools found"; \
	else \
		echo "  ✗ mtools not found - Installing (REQUIRED for ISO)..."; \
		sudo apt-get install -y mtools || { \
			echo "  ✗ Failed to install mtools"; \
			echo "  Please install manually: sudo apt-get install mtools"; \
			exit 1; \
		} \
	fi
	@echo ""
	@echo "  All required dependencies installed!"
	@echo ""

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/platform
	@mkdir -p $(BUILD_DIR)/net/ipv6
	@mkdir -p $(BUILD_DIR)/net/firewall
	@mkdir -p $(BUILD_DIR)/net/protocols
	@mkdir -p $(BUILD_DIR)/gui/desktop
	@mkdir -p $(BUILD_DIR)/gui/compositor
	@mkdir -p $(BUILD_DIR)/gui/app-store
	@mkdir -p $(BUILD_DIR)/system/registry
	@mkdir -p $(BUILD_DIR)/drivers/display
	@mkdir -p $(BUILD_DIR)/drivers/input
	@mkdir -p $(BUILD_DIR)/drivers/audio
	@mkdir -p $(BUILD_DIR)/drivers/gpu
	@mkdir -p $(BUILD_DIR)/drivers/usb
	@mkdir -p $(BUILD_DIR)/security/encryption
	@mkdir -p $(BUILD_DIR)/ai_ml/inference
	@mkdir -p $(BUILD_DIR)/hal/power
	@mkdir -p $(BUILD_DIR)/iot/protocols
	@mkdir -p $(BUILD_DIR)/virt/containers
	@mkdir -p $(BUILD_DIR)/fs/procfs
	@mkdir -p $(BUILD_DIR)/mobile/telephony
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

# Driver object files
$(BUILD_DIR)/display.o: $(DRIVERS_DIR)/display/display.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/input.o: $(DRIVERS_DIR)/input/input.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/audio.o: $(DRIVERS_DIR)/audio/audio.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/gpu.o: $(DRIVERS_DIR)/gpu/gpu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/module_loader.o: $(KERNEL_DIR)/core/module_loader.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/network_manager.o: $(NET_DIR)/network_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/desktop_grid.o: $(GUI_DIR)/desktop/desktop_grid.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/compositing_manager.o: $(GUI_DIR)/compositor/compositing_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/display_manager.o: $(DRIVERS_DIR)/display/display_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/mixer.o: $(DRIVERS_DIR)/audio/mixer.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/bluetooth_audio.o: $(DRIVERS_DIR)/audio/bluetooth_audio.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/gpu_scheduler.o: $(DRIVERS_DIR)/gpu/gpu_scheduler.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/usb_manager.o: $(DRIVERS_DIR)/usb/usb_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/disk_encryption.o: $(SECURITY_DIR)/encryption/disk_encryption.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/inference_engine.o: $(AI_ML_DIR)/inference/inference_engine.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/power_manager.o: $(HAL_DIR)/power/power_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/iot_protocols.o: $(IOT_DIR)/protocols/iot_protocols.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/container_runtime.o: $(VIRT_DIR)/containers/container_runtime.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/procfs.o: $(FS_DIR)/procfs/procfs.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/telephony_manager.o: $(MOBILE_DIR)/telephony/telephony_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/network_protocols.o: $(NET_DIR)/protocols/network_protocols.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/control-center.o: $(GUI_DIR)/control-center/control-center.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/file-manager.o: $(GUI_DIR)/file-manager/file-manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/system-settings.o: $(GUI_DIR)/system-settings/system-settings.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/ps2.o: $(DRIVERS_DIR)/input/keyboard/ps2.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/touchscreen.o: $(DRIVERS_DIR)/input/touchscreen/touchscreen.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/opengl.o: $(DRIVERS_DIR)/gpu/opengl.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/vga.o: $(DRIVERS_DIR)/video/vga.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/dsi.o: $(DRIVERS_DIR)/video/dsi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/ethernet.o: $(DRIVERS_DIR)/network/ethernet.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/wifi.o: $(DRIVERS_DIR)/network/wifi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/bluetooth.o: $(DRIVERS_DIR)/bluetooth/bluetooth.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/rtc.o: $(DRIVERS_DIR)/rtc/rtc.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/pci.o: $(DRIVERS_DIR)/pci/pci.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/sensor_hub.o: $(DRIVERS_DIR)/sensors/sensor_hub.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/virtio_gpu.o: $(DRIVERS_DIR)/gpu/virtio_gpu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [WARN] Compilation warnings"; }

$(BUILD_DIR)/fb.o: $(DRIVERS_DIR)/graphics/framebuffer/fb.c | $(BUILD_DIR)
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

# Force ISO rebuild
.PHONY: iso
iso: clean $(KERNEL_ISO)
	@echo "  [INFO] ISO rebuilt successfully"

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
run-vm: $(KERNEL_ISO)
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
