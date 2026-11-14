# NEXUS OS - Complete Build System
# Builds a bootable VMware/QEMU compatible kernel with ALL features

# Project directory
PROJECT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Tools
CC = gcc

# ---------------------------------------------------------------------------
# Compiler scratch space
#
# gcc writes an intermediate assembly file (and, with -g, DWARF data) into
# $TMPDIR before assembling. On a machine whose /tmp is a small tmpfs and whose
# disk is close to full, gcc fails with:
#
#     Assembler messages:
#     Error: can't open /tmp/ccXXXXXX.s for reading: No such file or directory
#
# which reads like a missing source file but is really "could not write the
# temporary". The build then reported it as "[ERR] compile failed: <source>"
# and the source was blamed for something it had nothing to do with.
#
# Compile into a scratch directory inside build/ instead, so the space used is
# on the same filesystem as the output and is removed by `make clean`.
# ---------------------------------------------------------------------------
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
APPS_DIR = apps
BUILD_DIR = build

BUILD_TMP := $(CURDIR)/$(BUILD_DIR)/.tmp
export TMPDIR := $(BUILD_TMP)
ISO_DIR = $(BUILD_DIR)/iso

# Kernel name
KERNEL_NAME = nexus-kernel
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME).bin
KERNEL_ISO = $(BUILD_DIR)/$(KERNEL_NAME).iso

# Userspace PID 1 image. Packaged into the ISO as a multiboot2 module and
# loaded by the kernel at boot; never linked into the kernel image.
USERSPACE_ELF = $(BUILD_DIR)/userspace/init.elf

# Compiler flags - freestanding kernel
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -nostdlib -nodefaultlibs \
         -Wall -Wextra -O2 -g \
         -m64 -mno-red-zone \
         -I$(KERNEL_DIR)/include \
         -DEFINES -D__FREESTANDING__=1 \
         -fno-builtin

# Disable specific warnings for incomplete code
CFLAGS += -Wno-implicit-function-declaration \
          -Wno-unused-variable \
          -Wno-unused-parameter \
          -Wno-missing-declarations

ASFLAGS = -f elf64
LDFLAGS = -T $(KERNEL_DIR)/arch/x86_64/boot/linker.ld -nostdlib -z max-page-size=0x1000 --gc-sections

# Source files - ALL kernel components
ASM_SOURCES = $(KERNEL_DIR)/arch/x86_64/boot/boot.asm \
              $(KERNEL_DIR)/arch/x86_64/interrupts/interrupts.asm

C_SOURCES = $(KERNEL_DIR)/core/kernel.c \
            $(KERNEL_DIR)/core/printk.c \
            $(KERNEL_DIR)/core/panic.c \
            $(KERNEL_DIR)/core/init.c \
            $(KERNEL_DIR)/core/smp.c \
            $(KERNEL_DIR)/core/module_loader.c \
            $(KERNEL_DIR)/arch/x86_64/cpu/gdt.c \
            $(KERNEL_DIR)/arch/x86_64/interrupts/idt.c \
            $(KERNEL_DIR)/arch/x86_64/interrupts/pic.c \
            $(KERNEL_DIR)/arch/x86_64/cpu/timer.c \
            $(KERNEL_DIR)/arch/x86_64/cpu/acpi.c \
            $(KERNEL_DIR)/arch/x86_64/cpu/smp.c \
            $(KERNEL_DIR)/arch/x86_64/cpu/cpu.c \
            $(KERNEL_DIR)/arch/x86_64/mm/paging.c \
            $(KERNEL_DIR)/sync/spinlock.c \
            $(KERNEL_DIR)/sync/rwlock.c \
            $(KERNEL_DIR)/sync/atomic.c \
            $(KERNEL_DIR)/sync/waitqueue.c \
            $(KERNEL_DIR)/ipc/pipe.c \
            $(KERNEL_DIR)/ipc/mutex.c \
            $(KERNEL_DIR)/ipc/semaphore.c \
            $(KERNEL_DIR)/ipc/message_queue.c \
            $(KERNEL_DIR)/ipc/shm.c \
            $(KERNEL_DIR)/ipc/ipc.c \
            $(KERNEL_DIR)/sync/sync.c \
            $(FS_DIR)/vfs/vfs_core.c \
            $(FS_DIR)/vfs/inode.c \
            $(FS_DIR)/vfs/dentry.c \
            $(FS_DIR)/vfs/mount.c \
            $(KERNEL_DIR)/mm/pmm.c \
            $(KERNEL_DIR)/mm/page_alloc.c \
            $(KERNEL_DIR)/mm/heap.c \
            $(KERNEL_DIR)/mm/vmm.c \
            $(KERNEL_DIR)/mm/slab.c \
            $(KERNEL_DIR)/sched/scheduler.c \
            $(KERNEL_DIR)/sched/process.c \
            $(KERNEL_DIR)/sched/thread.c \
            $(KERNEL_DIR)/sched/cfs.c \
            $(KERNEL_DIR)/sched/realtime.c \
            $(KERNEL_DIR)/syscall/syscall.c \
            $(KERNEL_DIR)/syscall/syscall_table.c \
            $(KERNEL_DIR)/syscall/syscall_handlers.c \
            $(PLATFORM_DIR)/platform.c \
            $(NET_DIR)/ipv6/ipv6.c \
            $(NET_DIR)/firewall/firewall.c \
            $(NET_DIR)/network_manager.c \
            $(NET_DIR)/protocols/network_protocols.c \
            $(GUI_DIR)/desktop/desktop.c \
            $(GUI_DIR)/gui_core.c \
            $(GUI_DIR)/desktop/desktop_grid.c \
            $(GUI_DIR)/app-launcher/app-launcher.c \
            $(GUI_DIR)/task-manager/task-manager.c \
            $(GUI_DIR)/control-panel/control-panel.c \
            $(GUI_DIR)/compositor/compositing_manager.c \
            $(GUI_DIR)/compositor/compositor.c \
            $(GUI_DIR)/compositor/window_manager.c \
            $(GUI_DIR)/control-center/control-center.c \
            $(GUI_DIR)/file-manager/file-manager.c \
            $(GUI_DIR)/system-settings/system-settings.c \
            $(GUI_DIR)/app-store/app-store.c \
            $(GUI_DIR)/virtualization/virt_manager_gui.c \
            $(GUI_DIR)/virtualization/vm_manager_enhanced.c \
            $(GUI_DIR)/virtualization/virt_render.c \
            $(GUI_DIR)/apps_nexus.c \
            $(GUI_DIR)/login/login-screen.c \
            $(GUI_DIR)/onboarding/onboarding-wizard.c \
            $(GUI_DIR)/widgets/widget.c \
            $(SYSTEM_DIR)/registry/registry.c \
            $(SYSTEM_DIR)/optimizer/booster.c \
            $(SYSTEM_DIR)/thermal/cpu_cooler.c \
            $(SYSTEM_DIR)/cleanup/cache_cleaner.c \
            $(SYSTEM_DIR)/storage/storage_sense.c \
            $(SYSTEM_DIR)/restore/restore_points.c \
            $(SYSTEM_DIR)/startup/startup_health.c \
            $(SYSTEM_DIR)/update/update_manager.c \
            $(SYSTEM_DIR)/reset/reset_manager.c \
            $(SYSTEM_DIR)/audio/equalizer.c \
            $(SYSTEM_DIR)/logging/log_manager.c \
            $(SYSTEM_DIR)/feedback/feedback_manager.c \
            $(SYSTEM_DIR)/resource-sharing/resource-sharing.c \
            $(APPS_DIR)/terminal/terminal_features.c \
            $(APPS_DIR)/terminal/terminal_animations.c \
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
            $(DRIVERS_DIR)/biometrics/biometrics.c \
            $(DRIVERS_DIR)/media/camera.c \
            $(DRIVERS_DIR)/serial/serial.c \
            $(DRIVERS_DIR)/watchdog/watchdog.c \
            $(DRIVERS_DIR)/rtc/rtc.c \
            $(DRIVERS_DIR)/pci/pci.c \
            $(DRIVERS_DIR)/sensors/sensor_hub.c \
            $(DRIVERS_DIR)/video/vga.c \
            $(DRIVERS_DIR)/video/interface.c \
            $(DRIVERS_DIR)/video/dsi.c \
            $(DRIVERS_DIR)/storage/storage.c \
            $(DRIVERS_DIR)/storage/ahci.c \
            $(DRIVERS_DIR)/storage/nvme.c \
            $(DRIVERS_DIR)/storage/sd.c \
            $(DRIVERS_DIR)/storage/emmc.c \
            $(DRIVERS_DIR)/drivers.c \
            $(NET_DIR)/network.c \
            $(NET_DIR)/core/net_core.c \
            $(NET_DIR)/core/skbuff.c \
            $(NET_DIR)/core/socket.c \
            $(NET_DIR)/core/net_device.c \
            $(NET_DIR)/ipv4/ip.c \
            $(NET_DIR)/ipv4/icmp.c \
            $(NET_DIR)/ipv4/udp.c \
            $(NET_DIR)/ipv4/tcp.c \
            $(SECURITY_DIR)/security.c \
            $(SECURITY_DIR)/security_manager.c \
            $(SECURITY_DIR)/encryption/disk_encryption.c \
            $(AI_ML_DIR)/inference/inference_engine.c \
            $(HAL_DIR)/power/power_management.c \
            $(HAL_DIR)/power/power_manager.c \
            $(IOT_DIR)/protocols/iot_protocols.c \
            $(VIRT_DIR)/containers/container_runtime.c \
            $(VIRT_DIR)/containers/container_orchestration.c \
            $(VIRT_DIR)/hypervisor/nexus_hypervisor_core.c \
            $(VIRT_DIR)/vm/vm_manager.c \
            $(VIRT_DIR)/emulation/nested_virt.c \
            $(FS_DIR)/procfs/procfs.c \
            $(MOBILE_DIR)/telephony/telephony_manager.c \
            $(HAL_DIR)/interrupts/irq.c \
            $(GUI_DIR)/renderer/renderer.c \
            $(SECURITY_DIR)/auth/auth.c \
            $(SECURITY_DIR)/auth/authorization.c \
            $(SECURITY_DIR)/auth/pam.c \
            boot/bootloader/boot_menu.c

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o \
              $(BUILD_DIR)/interrupts.o \
              $(BUILD_DIR)/syscall_entry.o
C_OBJECTS = $(BUILD_DIR)/kernel.o \
            $(BUILD_DIR)/printk.o \
            $(BUILD_DIR)/string.o \
            $(BUILD_DIR)/cmdline.o \
            $(BUILD_DIR)/panic.o \
            $(BUILD_DIR)/init.o \
            $(BUILD_DIR)/smp.o \
            $(BUILD_DIR)/elf_loader.o \
            $(BUILD_DIR)/module_loader.o \
            $(BUILD_DIR)/gdt.o \
            $(BUILD_DIR)/idt.o \
            $(BUILD_DIR)/pic.o \
            $(BUILD_DIR)/timer.o \
            $(BUILD_DIR)/acpi.o \
            $(BUILD_DIR)/arch_smp.o \
            $(BUILD_DIR)/arch_cpu.o \
            $(BUILD_DIR)/paging.o \
            $(BUILD_DIR)/spinlock.o \
            $(BUILD_DIR)/rwlock.o \
            $(BUILD_DIR)/atomic.o \
            $(BUILD_DIR)/waitqueue.o \
            $(BUILD_DIR)/pipe.o \
            $(BUILD_DIR)/mutex.o \
            $(BUILD_DIR)/semaphore.o \
            $(BUILD_DIR)/message_queue.o \
            $(BUILD_DIR)/shm.o \
            $(BUILD_DIR)/ipc.o \
            $(BUILD_DIR)/sync.o \
            $(BUILD_DIR)/vfs_core.o \
            $(BUILD_DIR)/inode.o \
            $(BUILD_DIR)/dentry.o \
            $(BUILD_DIR)/mount.o \
            $(BUILD_DIR)/widget.o \
            $(BUILD_DIR)/socket.o \
            $(BUILD_DIR)/ip.o \
            $(BUILD_DIR)/icmp.o \
            $(BUILD_DIR)/udp.o \
            $(BUILD_DIR)/tcp.o \
            $(BUILD_DIR)/security_manager.o \
            $(BUILD_DIR)/ahci.o \
            $(BUILD_DIR)/nvme.o \
            $(BUILD_DIR)/sd.o \
            $(BUILD_DIR)/emmc.o \
            $(BUILD_DIR)/pmm.o \
            $(BUILD_DIR)/page_alloc.o \
            $(BUILD_DIR)/heap.o \
            $(BUILD_DIR)/vmm.o \
            $(BUILD_DIR)/slab.o \
            $(BUILD_DIR)/scheduler.o \
            $(BUILD_DIR)/process.o \
            $(BUILD_DIR)/thread.o \
            $(BUILD_DIR)/cfs.o \
            $(BUILD_DIR)/realtime.o \
            $(BUILD_DIR)/syscall.o \
            $(BUILD_DIR)/syscall_table.o \
            $(BUILD_DIR)/syscall_handlers.o \
            $(BUILD_DIR)/platform.o \
            $(BUILD_DIR)/firewall.o \
            $(BUILD_DIR)/network_manager.o \
            $(BUILD_DIR)/network_protocols.o \
            $(BUILD_DIR)/net_core.o \
            $(BUILD_DIR)/skbuff.o \
            $(BUILD_DIR)/gui_core.o \
            $(BUILD_DIR)/desktop.o \
            $(BUILD_DIR)/desktop_grid.o \
            $(BUILD_DIR)/app-launcher.o \
            $(BUILD_DIR)/task-manager.o \
            $(BUILD_DIR)/control-panel.o \
            $(BUILD_DIR)/compositing_manager.o \
            $(BUILD_DIR)/compositor.o \
            $(BUILD_DIR)/wm_window.o \
            $(BUILD_DIR)/window_manager.o \
            $(BUILD_DIR)/control-center.o \
            $(BUILD_DIR)/file-manager.o \
            $(BUILD_DIR)/system-settings.o \
            $(BUILD_DIR)/app-store.o \
            $(BUILD_DIR)/virt_manager_gui.o \
            $(BUILD_DIR)/virt_render.o \
            $(BUILD_DIR)/apps_nexus.o \
            $(BUILD_DIR)/vm_manager_enhanced.o \
            $(BUILD_DIR)/login-screen.o \
            $(BUILD_DIR)/session-manager.o \
            $(BUILD_DIR)/onboarding-wizard.o \
            $(BUILD_DIR)/registry.o \
            $(BUILD_DIR)/booster.o \
            $(BUILD_DIR)/cpu_cooler.o \
            $(BUILD_DIR)/cache_cleaner.o \
            $(BUILD_DIR)/storage_sense.o \
            $(BUILD_DIR)/restore_points.o \
            $(BUILD_DIR)/startup_health.o \
            $(BUILD_DIR)/update_manager.o \
            $(BUILD_DIR)/reset_manager.o \
            $(BUILD_DIR)/equalizer.o \
            $(BUILD_DIR)/log_manager.o \
            $(BUILD_DIR)/feedback_manager.o \
            $(BUILD_DIR)/resource-sharing.o \
            $(BUILD_DIR)/shell.o \
            $(BUILD_DIR)/lang_runtime.o \
            $(BUILD_DIR)/terminal.o \
            $(BUILD_DIR)/terminal_enhanced.o \
            $(BUILD_DIR)/terminal_features.o \
            $(BUILD_DIR)/terminal_animations.o \
            $(BUILD_DIR)/display.o \
            $(BUILD_DIR)/display_manager.o \
            $(BUILD_DIR)/console.o \
            $(BUILD_DIR)/splash.o \
            $(BUILD_DIR)/fbconsole.o \
            $(BUILD_DIR)/font8x16.o \
            $(BUILD_DIR)/icon_library.o \
            $(BUILD_DIR)/icon_library_extra.o \
            $(BUILD_DIR)/display_settings.o \
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
            $(BUILD_DIR)/ipv6.o \
            $(BUILD_DIR)/bluetooth.o \
            $(BUILD_DIR)/biometrics.o \
            $(BUILD_DIR)/camera.o \
            $(BUILD_DIR)/serial.o \
            $(BUILD_DIR)/watchdog.o \
            $(BUILD_DIR)/rtc.o \
            $(BUILD_DIR)/storage.o \
            $(BUILD_DIR)/drivers.o \
            $(BUILD_DIR)/network.o \
            $(BUILD_DIR)/net_device.o \
            $(BUILD_DIR)/security.o \
            $(BUILD_DIR)/pci.o \
            $(BUILD_DIR)/sensor_hub.o \
            $(BUILD_DIR)/vga.o \
            $(BUILD_DIR)/interface.o \
            $(BUILD_DIR)/dsi.o \
            $(BUILD_DIR)/disk_encryption.o \
            $(BUILD_DIR)/inference_engine.o \
            $(BUILD_DIR)/power_management.o \
            $(BUILD_DIR)/power_manager.o \
            $(BUILD_DIR)/iot_protocols.o \
            $(BUILD_DIR)/container_runtime.o \
            $(BUILD_DIR)/container_orchestration.o \
            $(BUILD_DIR)/nexus_hypervisor_core.o \
            $(BUILD_DIR)/vm_manager.o \
            $(BUILD_DIR)/nested_virt.o \
            $(BUILD_DIR)/nexfs_core.o \
            $(BUILD_DIR)/nexfs_inode.o \
            $(BUILD_DIR)/nexfs_journal.o \
            $(BUILD_DIR)/procfs.o \
            $(BUILD_DIR)/telephony_manager.o \
            $(BUILD_DIR)/irq.o \
            $(BUILD_DIR)/renderer.o \
            $(BUILD_DIR)/auth.o \
            $(BUILD_DIR)/authorization.o \
            $(BUILD_DIR)/pam.o \
            $(BUILD_DIR)/math.o \
            $(BUILD_DIR)/acpi_hal.o \
            $(BUILD_DIR)/cpu_core.o \
            $(BUILD_DIR)/apic.o \
            $(BUILD_DIR)/ecc.o \
            $(BUILD_DIR)/sha.o \
            $(BUILD_DIR)/boot_menu.o

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Default target
all: check-deps check-space $(KERNEL_ISO)
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

# ---------------------------------------------------------------------------
# Pre-flight disk check
#
# The kernel is ~28 MB of objects and a ~28 MB ISO, and gcc needs scratch space
# on top. A build that runs out of room halfway through fails on an arbitrary
# file with "can't open /tmp/ccXXXXXX.s" - which looks like a source error and
# sends you hunting in the wrong place. Catch it before the first compile.
# ---------------------------------------------------------------------------
.PHONY: check-space
check-space:
	@free_kb=$$(df -P "$(BUILD_TMP)" 2>/dev/null | awk 'NR==2{print $$4}'); \
	if [ -z "$$free_kb" ]; then \
		mkdir -p "$(BUILD_TMP)" 2>/dev/null; \
		free_kb=$$(df -P "$(BUILD_TMP)" 2>/dev/null | awk 'NR==2{print $$4}'); \
	fi; \
	if [ -n "$$free_kb" ] && [ "$$free_kb" -lt 102400 ]; then \
		echo "  [WARN]  only $$((free_kb / 1024)) MB free where the build writes"; \
		echo "          ($(BUILD_TMP)) - the kernel plus ISO needs roughly 150 MB"; \
		echo "          A build that dies with \"can't open /tmp/cc....s\" is this, not a source bug."; \
	fi

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_TMP)
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/sync
	@mkdir -p $(BUILD_DIR)/ipc
	@mkdir -p $(BUILD_DIR)/mm
	@mkdir -p $(BUILD_DIR)/sched
	@mkdir -p $(BUILD_DIR)/syscall
	@mkdir -p $(BUILD_DIR)/platform
	@mkdir -p $(BUILD_DIR)/net/ipv6
	@mkdir -p $(BUILD_DIR)/net/firewall
	@mkdir -p $(BUILD_DIR)/net/protocols
	@mkdir -p $(BUILD_DIR)/gui/desktop
	@mkdir -p $(BUILD_DIR)/gui/compositor
	@mkdir -p $(BUILD_DIR)/gui/app-store
	@mkdir -p $(BUILD_DIR)/gui/icons
	@mkdir -p $(BUILD_DIR)/gui/display
	@mkdir -p $(BUILD_DIR)/gui/virtualization
	@mkdir -p $(BUILD_DIR)/apps/terminal
	@mkdir -p $(BUILD_DIR)/system/registry
	@mkdir -p $(BUILD_DIR)/system/startup
	@mkdir -p $(BUILD_DIR)/system/update
	@mkdir -p $(BUILD_DIR)/system/reset
	@mkdir -p $(BUILD_DIR)/system/audio
	@mkdir -p $(BUILD_DIR)/system/logging
	@mkdir -p $(BUILD_DIR)/system/feedback
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
	@mkdir -p $(BUILD_DIR)/virt/hypervisor
	@mkdir -p $(BUILD_DIR)/virt/vm
	@mkdir -p $(BUILD_DIR)/virt/emulation
	@mkdir -p $(BUILD_DIR)/fs/procfs
	@mkdir -p $(BUILD_DIR)/mobile/telephony
	@mkdir -p $(ISO_DIR)/boot/grub

# Assemble boot code
$(BUILD_DIR)/boot.o: $(KERNEL_DIR)/arch/x86_64/boot/boot.asm | $(BUILD_DIR)
	@echo "  [NASM]  $<"
	@$(NASM) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/interrupts.o: $(KERNEL_DIR)/arch/x86_64/interrupts/interrupts.asm | $(BUILD_DIR)
	@echo "  [NASM]  $<"
	@$(NASM) $(ASFLAGS) -o $@ $<

# SYSCALL/SYSRET entry and the ring-3 transition. Assembled with gcc's
# integrated assembler rather than nasm, so it builds with the same toolchain
# as the rest of the kernel and is verifiable here.
$(BUILD_DIR)/syscall_entry.o: $(KERNEL_DIR)/arch/x86_64/syscall/syscall_entry.S | $(BUILD_DIR)
	@echo "  [AS]    $<"
	@$(CC) -c -o $@ $<

# Compile C files
$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/core/kernel.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/gdt.o: $(KERNEL_DIR)/arch/x86_64/cpu/gdt.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/arch/x86_64/interrupts/idt.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/pic.o: $(KERNEL_DIR)/arch/x86_64/interrupts/pic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/paging.o: $(KERNEL_DIR)/arch/x86_64/mm/paging.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/timer.o: $(KERNEL_DIR)/arch/x86_64/cpu/timer.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/acpi.o: $(KERNEL_DIR)/arch/x86_64/cpu/acpi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/arch_smp.o: $(KERNEL_DIR)/arch/x86_64/cpu/smp.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/arch_cpu.o: $(KERNEL_DIR)/arch/x86_64/cpu/cpu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/printk.o: $(KERNEL_DIR)/core/printk.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/string.o: $(KERNEL_DIR)/core/string.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/cmdline.o: $(KERNEL_DIR)/core/cmdline.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/panic.o: $(KERNEL_DIR)/core/panic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/init.o: $(KERNEL_DIR)/core/init.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/smp.o: $(KERNEL_DIR)/core/smp.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/platform.o: $(PLATFORM_DIR)/platform.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/ipv6.o: $(NET_DIR)/ipv6/ipv6.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/net_core.o: $(NET_DIR)/core/net_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/skbuff.o: $(NET_DIR)/core/skbuff.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(NET_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/net_device.o: $(NET_DIR)/core/net_device.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(NET_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/firewall.o: $(NET_DIR)/firewall/firewall.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/desktop.o: $(GUI_DIR)/desktop/desktop.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/gui_core.o: $(GUI_DIR)/gui_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/app-store.o: $(GUI_DIR)/app-store/app-store.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/session-manager.o: $(GUI_DIR)/session/session-manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/login-screen.o: $(GUI_DIR)/login/login-screen.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/onboarding-wizard.o: $(GUI_DIR)/onboarding/onboarding-wizard.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/virt_manager_gui.o: $(GUI_DIR)/virtualization/virt_manager_gui.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/vm_manager_enhanced.o: $(GUI_DIR)/virtualization/vm_manager_enhanced.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/virt_render.o: $(GUI_DIR)/virtualization/virt_render.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/apps_nexus.o: $(GUI_DIR)/apps_nexus.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/registry.o: $(SYSTEM_DIR)/registry/registry.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/booster.o: $(SYSTEM_DIR)/optimizer/booster.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/cpu_cooler.o: $(SYSTEM_DIR)/thermal/cpu_cooler.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/cache_cleaner.o: $(SYSTEM_DIR)/cleanup/cache_cleaner.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/storage_sense.o: $(SYSTEM_DIR)/storage/storage_sense.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/restore_points.o: $(SYSTEM_DIR)/restore/restore_points.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/startup_health.o: $(SYSTEM_DIR)/startup/startup_health.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/update_manager.o: $(SYSTEM_DIR)/update/update_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/reset_manager.o: $(SYSTEM_DIR)/reset/reset_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/equalizer.o: $(SYSTEM_DIR)/audio/equalizer.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/log_manager.o: $(SYSTEM_DIR)/logging/log_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/feedback_manager.o: $(SYSTEM_DIR)/feedback/feedback_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/resource-sharing.o: $(SYSTEM_DIR)/resource-sharing/resource-sharing.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Driver object files
$(BUILD_DIR)/display.o: $(DRIVERS_DIR)/display/display.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/input.o: $(DRIVERS_DIR)/input/input.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/audio.o: $(DRIVERS_DIR)/audio/audio.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/gpu.o: $(DRIVERS_DIR)/gpu/gpu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/elf_loader.o: $(KERNEL_DIR)/core/elf_loader.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/module_loader.o: $(KERNEL_DIR)/core/module_loader.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel sync primitives
$(BUILD_DIR)/spinlock.o: $(KERNEL_DIR)/sync/spinlock.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/rwlock.o: $(KERNEL_DIR)/sync/rwlock.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/atomic.o: $(KERNEL_DIR)/sync/atomic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/waitqueue.o: $(KERNEL_DIR)/sync/waitqueue.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel IPC
$(BUILD_DIR)/pipe.o: $(KERNEL_DIR)/ipc/pipe.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/mutex.o: $(KERNEL_DIR)/ipc/mutex.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/semaphore.o: $(KERNEL_DIR)/ipc/semaphore.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/message_queue.o: $(KERNEL_DIR)/ipc/message_queue.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/shm.o: $(KERNEL_DIR)/ipc/shm.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/ipc.o: $(KERNEL_DIR)/ipc/ipc.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel synchronization
$(BUILD_DIR)/sync.o: $(KERNEL_DIR)/sync/sync.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel memory management
$(BUILD_DIR)/pmm.o: $(KERNEL_DIR)/mm/pmm.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/page_alloc.o: $(KERNEL_DIR)/mm/page_alloc.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/heap.o: $(KERNEL_DIR)/mm/heap.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/vmm.o: $(KERNEL_DIR)/mm/vmm.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/slab.o: $(KERNEL_DIR)/mm/slab.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel scheduler
$(BUILD_DIR)/scheduler.o: $(KERNEL_DIR)/sched/scheduler.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/process.o: $(KERNEL_DIR)/sched/process.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/thread.o: $(KERNEL_DIR)/sched/thread.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/cfs.o: $(KERNEL_DIR)/sched/cfs.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/realtime.o: $(KERNEL_DIR)/sched/realtime.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# Kernel syscalls
$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall/syscall.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/syscall_table.o: $(KERNEL_DIR)/syscall/syscall_table.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/syscall_handlers.o: $(KERNEL_DIR)/syscall/syscall_handlers.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/network_manager.o: $(NET_DIR)/network_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/desktop_grid.o: $(GUI_DIR)/desktop/desktop_grid.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/app-launcher.o: $(GUI_DIR)/app-launcher/app-launcher.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/task-manager.o: $(GUI_DIR)/task-manager/task-manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/control-panel.o: $(GUI_DIR)/control-panel/control-panel.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# gui/window-manager/window-manager.c and gui/compositor/window_manager.c
# both want the object name "window_manager.o", so the former builds to
# wm_window.o. They are different files implementing different things: the
# compositor's window registry vs. the WM window object API used by apps.
$(BUILD_DIR)/wm_window.o: $(GUI_DIR)/window-manager/window-manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/window_manager.o: $(GUI_DIR)/compositor/window_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/compositor.o: $(GUI_DIR)/compositor/compositor.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/compositing_manager.o: $(GUI_DIR)/compositor/compositing_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -mno-sse2 -msoft-float -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/display_manager.o: $(DRIVERS_DIR)/display/display_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/console.o: $(DRIVERS_DIR)/video/console.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/splash.o: $(DRIVERS_DIR)/video/splash.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/fbconsole.o: $(DRIVERS_DIR)/video/fbconsole.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/font8x16.o: $(DRIVERS_DIR)/video/font8x16.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/icon_library.o: $(GUI_DIR)/icons/icon_library.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/icon_library_extra.o: $(GUI_DIR)/icons/icon_library_extra.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/display_settings.o: $(GUI_DIR)/display/display_settings.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/lang_runtime.o: $(APPS_DIR)/terminal/lang_runtime.c $(APPS_DIR)/terminal/lang_runtime.h | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(APPS_DIR) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/shell.o: $(APPS_DIR)/terminal/shell.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(APPS_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/terminal.o: $(APPS_DIR)/terminal/terminal.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(APPS_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/terminal_enhanced.o: $(APPS_DIR)/terminal/terminal_enhanced.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(APPS_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/terminal_features.o: $(APPS_DIR)/terminal/terminal_features.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(APPS_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/terminal_animations.o: $(APPS_DIR)/terminal/terminal_animations.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(APPS_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/mixer.o: $(DRIVERS_DIR)/audio/mixer.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/bluetooth_audio.o: $(DRIVERS_DIR)/audio/bluetooth_audio.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/gpu_scheduler.o: $(DRIVERS_DIR)/gpu/gpu_scheduler.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/usb_manager.o: $(DRIVERS_DIR)/usb/usb_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/disk_encryption.o: $(SECURITY_DIR)/encryption/disk_encryption.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/inference_engine.o: $(AI_ML_DIR)/inference/inference_engine.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/power_management.o: $(HAL_DIR)/power/power_management.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/power_manager.o: $(HAL_DIR)/power/power_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/iot_protocols.o: $(IOT_DIR)/protocols/iot_protocols.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/container_runtime.o: $(VIRT_DIR)/containers/container_runtime.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/container_orchestration.o: $(VIRT_DIR)/containers/container_orchestration.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/nexus_hypervisor_core.o: $(VIRT_DIR)/hypervisor/nexus_hypervisor_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/vm_manager.o: $(VIRT_DIR)/vm/vm_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/nested_virt.o: $(VIRT_DIR)/emulation/nested_virt.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/nexfs_core.o: $(FS_DIR)/nexfs/nexfs_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/nexfs_inode.o: $(FS_DIR)/nexfs/nexfs_inode.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/nexfs_journal.o: $(FS_DIR)/nexfs/nexfs_journal.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/vfs_core.o: $(FS_DIR)/vfs/vfs_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/procfs.o: $(FS_DIR)/procfs/procfs.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/telephony_manager.o: $(MOBILE_DIR)/telephony/telephony_manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/network_protocols.o: $(NET_DIR)/protocols/network_protocols.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/control-center.o: $(GUI_DIR)/control-center/control-center.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/file-manager.o: $(GUI_DIR)/file-manager/file-manager.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/system-settings.o: $(GUI_DIR)/system-settings/system-settings.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -I$(GUI_DIR) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/ps2.o: $(DRIVERS_DIR)/input/keyboard/ps2.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/touchscreen.o: $(DRIVERS_DIR)/input/touchscreen/touchscreen.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/opengl.o: $(DRIVERS_DIR)/gpu/opengl.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(FS_DIR)/vfs/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(NET_DIR)/core/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(NET_DIR)/ipv4/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(GUI_DIR)/widgets/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(SECURITY_DIR)/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/storage/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/vga.o: $(DRIVERS_DIR)/video/vga.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/dsi.o: $(DRIVERS_DIR)/video/dsi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/interface.o: $(DRIVERS_DIR)/video/interface.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/ethernet.o: $(DRIVERS_DIR)/network/ethernet.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/wifi.o: $(DRIVERS_DIR)/network/wifi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/bluetooth.o: $(DRIVERS_DIR)/bluetooth/bluetooth.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/biometrics.o: $(DRIVERS_DIR)/biometrics/biometrics.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/camera.o: $(DRIVERS_DIR)/media/camera.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/serial.o: $(DRIVERS_DIR)/serial/serial.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/watchdog.o: $(DRIVERS_DIR)/watchdog/watchdog.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/rtc.o: $(DRIVERS_DIR)/rtc/rtc.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/storage.o: $(DRIVERS_DIR)/storage/storage.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/drivers.o: $(DRIVERS_DIR)/drivers.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/network.o: $(NET_DIR)/network.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/security.o: $(SECURITY_DIR)/security.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/pci.o: $(DRIVERS_DIR)/pci/pci.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/sensor_hub.o: $(DRIVERS_DIR)/sensors/sensor_hub.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/virtio_gpu.o: $(DRIVERS_DIR)/gpu/virtio_gpu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/fb.o: $(DRIVERS_DIR)/graphics/framebuffer/fb.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/irq.o: $(HAL_DIR)/interrupts/irq.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/renderer.o: $(GUI_DIR)/renderer/renderer.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/auth.o: $(SECURITY_DIR)/auth/auth.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/authorization.o: $(SECURITY_DIR)/auth/authorization.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/pam.o: $(SECURITY_DIR)/auth/pam.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/math.o: $(KERNEL_DIR)/core/math.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/acpi_hal.o: $(HAL_DIR)/power/acpi.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/cpu_core.o: $(HAL_DIR)/cpu/cpu_core.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/apic.o: $(HAL_DIR)/interrupts/apic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/ecc.o: $(SECURITY_DIR)/crypto/ecc.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/sha.o: $(SECURITY_DIR)/crypto/sha.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

$(BUILD_DIR)/boot_menu.o: boot/bootloader/boot_menu.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; if [ ! -w "$(TMPDIR)" ]; then echo "  [ERR]   compiler scratch dir $(TMPDIR) is not writable"; elif [ "$(df -P "$(TMPDIR)" 2>/dev/null | awk 'NR==2{print $4}')" != "" ] && [ "$(df -P "$(TMPDIR)" | awk 'NR==2{print $4}')" -lt 2048 ]; then echo "  [ERR]   only $(df -P "$(TMPDIR)" | awk 'NR==2{print $4}') KB free in $(TMPDIR)"; echo "  [ERR]   that is a disk-space problem, not a source problem"; fi; exit 1; }

# ---------------------------------------------------------------------------
# Userspace runtime (libc, libsys, init, services)
#
# These programs are NOT part of the kernel. They are a separate freestanding
# ET_EXEC image linked at 0x400000 by userspace/linker.ld, packaged into the ISO
# as a multiboot2 module, and handed to the kernel by GRUB at boot. The kernel
# loads it, maps it, and enters ring 3 at _start; see kernel/arch/x86_64/boot/
# boot.asm (module tag request) and kernel/core/userspace_loader.c.
#
# Linking them into the kernel was the old arrangement and it could not work:
# libc_string/libc_stdlib/libc_stdio redefine 33 symbols the kernel already
# defines (memcpy, strlen, atoi, vsnprintf, ...), so ld refused the kernel image
# with "multiple definition" errors. Two programs with two entry points and two
# address spaces need two binaries.
#
# Both sides stay in sync on syscall numbering through
# kernel/include/nexus_syscall_numbers.h, which libsys includes.
# ---------------------------------------------------------------------------
USERSPACE_DIRS = userspace/libc userspace/libsys userspace/init userspace/services
USERSPACE_CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -m64 \
                   -mno-red-zone -O2 -g -Wall -Wextra -fno-builtin \
                   -I$(KERNEL_DIR)/include -Iuserspace/include -Iuserspace \
                   -D__FREESTANDING__=1 \
                   -Wno-unused-parameter -Wno-unused-variable

# Userspace objects get their own directory so they can never be confused with
# kernel objects by a pattern rule or by the check-build guard.
USER_BUILD_DIR = $(BUILD_DIR)/userspace
USER_OBJECTS = $(USER_BUILD_DIR)/syscall_wrapper.o \
               $(USER_BUILD_DIR)/syscall_glue.o \
               $(USER_BUILD_DIR)/crt0.o \
               $(USER_BUILD_DIR)/string.o \
               $(USER_BUILD_DIR)/stdlib.o \
               $(USER_BUILD_DIR)/stdio.o \
               $(USER_BUILD_DIR)/math.o \
               $(USER_BUILD_DIR)/user_init.o

$(USER_BUILD_DIR):
	@mkdir -p $@

$(USER_BUILD_DIR)/%.o: userspace/libc/%.c | $(USER_BUILD_DIR)
	@echo "  [CC-u]  $<"
	@$(CC) $(USERSPACE_CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(USER_BUILD_DIR)/%.o: userspace/libsys/%.c | $(USER_BUILD_DIR)
	@echo "  [CC-u]  $<"
	@$(CC) $(USERSPACE_CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(USER_BUILD_DIR)/%.o: userspace/libsys/%.S | $(USER_BUILD_DIR)
	@echo "  [AS-u]  $<"
	@$(CC) $(USERSPACE_CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   assemble failed: $<"; exit 1; }

$(USER_BUILD_DIR)/user_init.o: userspace/init/init.c | $(USER_BUILD_DIR)
	@echo "  [CC-u]  $<"
	@$(CC) $(USERSPACE_CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

# ---------------------------------------------------------------------------
# userspace/init.elf - the PID 1 image
#
# Linked at its own base address with its own linker script, entry _start from
# crt0.S. -nostdlib keeps the host libc out; every symbol resolves inside the
# image. The result is what GRUB hands the kernel as a multiboot2 module.
# ---------------------------------------------------------------------------
$(USERSPACE_ELF): $(USER_OBJECTS) userspace/linker.ld
	@echo "  [LD-u]  $@"
	@$(LD) -T userspace/linker.ld -nostdlib -z max-page-size=0x1000 \
		-o $@ $(USER_OBJECTS) 2>&1
	@echo "  [INFO]  Userspace size: $$(stat -c %s $@ 2>/dev/null || stat -f %z $@ 2>/dev/null || echo 0) bytes"

# ---------------------------------------------------------------------------
# service_manager.elf - the first service init will exec
#
# init starts services by path through SYS_EXECVE (userspace/init/init.c:515),
# so the service manager is a separate program, not part of the PID 1 image.
# It shares every library object with init but supplies its own main(), which is
# exactly why the two cannot be linked together: two mains, one image.
#
# It is built but not yet packaged: init execs /sbin/service_manager, and no
# root filesystem is mounted at boot for that path to resolve against. Building
# it here keeps it compiling and linking so the gap is visible rather than
# silent; packaging it is part of getting a root filesystem mounted.
# ---------------------------------------------------------------------------
SERVICE_MANAGER_ELF = $(USER_BUILD_DIR)/service_manager.elf

$(USER_BUILD_DIR)/service_manager.o: userspace/services/service_manager.c | $(USER_BUILD_DIR)
	@echo "  [CC-u]  $<"
	@$(CC) $(USERSPACE_CFLAGS) -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(SERVICE_MANAGER_ELF): $(USER_BUILD_DIR)/service_manager.o \
                        $(filter-out $(USER_BUILD_DIR)/user_init.o,$(USER_OBJECTS)) \
                        userspace/linker.ld
	@echo "  [LD-u]  $@"
	@$(LD) -T userspace/linker.ld -nostdlib -z max-page-size=0x1000 \
		-o $@ $(USER_BUILD_DIR)/service_manager.o \
		$(filter-out $(USER_BUILD_DIR)/user_init.o,$(USER_OBJECTS)) 2>&1

userspace-bin: $(USERSPACE_ELF) $(SERVICE_MANAGER_ELF)
	@echo "  [INFO]  Userspace binaries built"

.PHONY: userspace-bin

$(BUILD_DIR)/boot_enhanced.o: boot/bootloader/gui-boot-enhanced.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/scanner.o: drivers/scanner/scanner.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/boot_diag.o: kernel/diag/boot_diag.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/dev_support.o: system/dev-support/dev-support.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/memory_map.o: hal/memory/memory_map.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/boot_splash.o: gui/boot-splash/boot_splash.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/coap.o: iot/protocols/coap.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/mqtt.o: iot/protocols/mqtt.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/cpu_features.o: hal/cpu/cpu_features.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/device_tree.o: hal/power/device_tree.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/device_registry.o: iot/device_management/device_registry.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/edge_runtime.o: iot/edge_computing/edge_runtime.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/gic.o: hal/interrupts/gic.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/status_bar.o: gui/status-bar/status-bar.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

$(BUILD_DIR)/gui_boot.o: boot/bootloader/gui-boot.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c -o $@ $< 2>&1 || { echo "  [ERR]   compile failed: $<"; exit 1; }

# ---------------------------------------------------------------------------
# Build guard
#
# This target DEPENDS ON $(OBJECTS). It verifies that every object listed in
# C_OBJECTS exists on disk, so it must run last, after everything is compiled.
# It previously had no prerequisites and was listed BEFORE $(OBJECTS) on the
# kernel's dependency line, so on a clean tree it inspected an empty build/
# directory, reported "objects never built (compile failed, see log above)" and
# exited 1 - before a single source had been compiled. That is why a failed
# build showed no [CC] lines and no compiler diagnostics.
#
# Every compile recipe ends in "2>&1", which routes diagnostics into the log
# without touching the exit status. That let `make` report success while a
# source failed to compile, and a repeated entry in C_OBJECTS put the same
# object on the link line twice ("multiple definition of ..."). Both are
# checked here so the build fails loudly at the point of the mistake.
# ---------------------------------------------------------------------------
.PHONY: check-build
check-build: $(OBJECTS)
	@dupes=$$(printf '%s\n' $(C_OBJECTS) | sort | uniq -d); \
	if [ -n "$$dupes" ]; then \
	  echo "  [ERR]   duplicate objects in C_OBJECTS:"; \
	  printf '            %s\n' $$dupes; \
	  exit 1; \
	fi
	@absent=""; \
	for o in $(C_OBJECTS); do \
	  [ -f "$$o" ] || absent="$$absent $$o"; \
	done; \
	if [ -n "$$absent" ]; then \
	  echo "  [ERR]   objects never built (compile failed, see log above):"; \
	  printf '            %s\n' $$absent; \
	  exit 1; \
	fi
	@leaked=$$(printf '%s\n' $(C_OBJECTS) | grep -E '(^|/)(syscall_wrapper|syscall_glue|crt0|libc_string|libc_stdlib|libc_stdio|user_init|service_manager)\.o$$'); \
	if [ -n "$$leaked" ]; then \
	  echo "  [ERR]   userspace objects found in C_OBJECTS:"; \
	  printf '            %s\n' $$leaked; \
	  echo "  [ERR]   these are linked into their own image by $(USERSPACE_ELF)"; \
	  echo "  [ERR]   linking them into the kernel reintroduces the multiple-definition failure"; \
	  exit 1; \
	fi
	@echo "  [CHECK] link inputs OK - $(words $(C_OBJECTS)) C objects"

# Link kernel
#
# ORDER MATTERS: $(OBJECTS) must be built BEFORE check-build runs.
#
# check-build verifies that every object listed in C_OBJECTS exists on disk.
# With "check-build $(OBJECTS)", make evaluates prerequisites left to right, so
# the guard ran first - against a freshly cleaned build/ directory - found
# nothing, printed "objects never built (compile failed, see log above)" and
# exited 1. Nothing had been compiled yet, which is why there were no [CC] lines
# and no compiler diagnostics above the failure: the build aborted before the
# first source was touched.
$(KERNEL_BIN): $(OBJECTS) check-build
	@echo "  [LD]    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS) 2>&1
	@echo "  [INFO]  Kernel size: $$(stat -c %s $@ 2>/dev/null || stat -f %z $@ 2>/dev/null || echo 0) bytes"

# Create GRUB configuration
$(ISO_DIR)/boot/grub/grub.cfg: | $(BUILD_DIR)
	@echo "  [GRUB]  Creating grub.cfg"
	@mkdir -p $(ISO_DIR)/boot/grub
	@echo 'set timeout=10' > $@
	@echo 'set default=0' >> $@
	@echo 'set gfxmode=1024x768x32,1024x768,auto' >> $@
	@echo 'set gfxpayload=keep' >> $@
	@echo '' >> $@
	@echo '# NEXUS OS Boot Menu' >> $@
	@echo 'set menu_color_highlight=black/blue' >> $@
	@echo 'set menu_color_normal=light-gray/black' >> $@
	@echo '' >> $@
	@echo 'menuentry "[GRAPHICAL] NEXUS OS - Graphical Mode (Default)" {' >> $@
	@echo '    set gfxpayload=1024x768x32' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin splash virt=auto' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[TEXT]      NEXUS OS - Text Mode (VGA Console)" {' >> $@
	@echo '    set gfxpayload=text' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[SAFE]      NEXUS OS - Safe Mode" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin nomodeset nosmp noapic nolapic' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[DEBUG]     NEXUS OS - Debug Mode" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin debug loglevel=7 earlyprintk=vga' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[NATIVE]    NEXUS OS - Native Hardware" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin virt=native' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[TERM]      NEXUS OS - Terminal (Nexus Shell)" {' >> $@
	@echo '    set gfxpayload=1024x768x32' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin splash terminal virt=auto' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "[MENU]      NEXUS OS - Interactive Boot Menu" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin' >> $@
	@echo '    module2 /boot/init.elf init' >> $@
	@echo '}' >> $@

# Create bootable ISO
$(KERNEL_ISO): $(KERNEL_BIN) $(USERSPACE_ELF) $(ISO_DIR)/boot/grub/grub.cfg
	@echo "  [CP]    Copying kernel to ISO dir"
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	@echo "  [CP]    Copying userspace init to ISO dir"
	@cp $(USERSPACE_ELF) $(ISO_DIR)/boot/init.elf
	@echo "  [GRUB-MKRESCUE] Creating ISO"
	@if command -v $(GRUB_MKRESUE) > /dev/null 2>&1; then \
		$(GRUB_MKRESUE) -o $(KERNEL_ISO) $(ISO_DIR) 2>&1; \
		echo "  [SUCCESS] Bootable ISO created: $(KERNEL_ISO)"; \
		echo "  [INFO] ISO size: $$(stat -c %s $(KERNEL_ISO) 2>/dev/null || stat -f %z $(KERNEL_ISO) 2>/dev/null || echo 0) bytes"; \
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
	@echo "  [QEMU]  Starting NEXUS OS..."
	@echo "  [INFO]  Press Ctrl+A then X to exit"
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -boot d \
		-m 2G \
		-smp 2 \
		-serial stdio \
		-display gtk \
		-name "NEXUS OS" \
		-no-reboot

# Run in QEMU with VMware emulation
run-vmware: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (VMware emulation)..."
	@echo "  [INFO]  Press Ctrl+A then X to exit"
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -boot d \
		-m 2G \
		-smp 2 \
		-serial stdio \
		-machine vmport=on \
		-device vmxnet3,netdev=net0 \
		-netdev user,id=net0 \
		-name "NEXUS OS (VMware Mode)" \
		-display gtk \
		-no-reboot

# Run with debug
run-debug: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (debug mode)..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -boot d -m 2G -smp 2 \
		-serial stdio -d int -no-reboot -no-shutdown

# Run in VMware (auto-create and boot)
run-vm: $(KERNEL_ISO)
	@echo "  [VMWARE] Auto-creating and launching VMware..."
	@$(PROJECT_DIR)/tools/auto-vm-boot.sh

# Run in QEMU with CD-only boot (use if "no bootable device" with make run)
run-cd: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (boot from CD only)..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -boot order=dc -net none \
		-m 2G -smp 2 -serial stdio -display sdl -name "NEXUS OS"

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
	@echo "  make run-vmware - QEMU with VMware device emulation"
	@echo "  make run-vm   - Auto-create VM and launch VMware"
	@echo "  make run-debug - Debug mode"
	@echo "  make run-cd   - QEMU, boot from CD only (if run shows no bootable device)"
	@echo "  make clean    - Remove build files"
	@echo ""

.PHONY: all clean run run-vmware run-vm run-cd run-debug run-vmware-player help
