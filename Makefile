# NEXUS OS - Makefile
# Builds a bootable kernel ISO that can run in QEMU/VMware

# Tools
CC = gcc
LD = ld
NASM = nasm
OBJCOPY = objcopy
GRUB_MKRESUE = grub-mkrescue

# Directories
KERNEL_DIR = kernel
BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/iso

# Kernel name
KERNEL_NAME = nexus-kernel
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME).bin
KERNEL_ISO = $(BUILD_DIR)/$(KERNEL_NAME).iso

# Flags
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -nostdlib -nodefaultlibs \
         -Wall -Wextra -Werror -O2 -g \
         -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -I$(KERNEL_DIR)/include
         
ASFLAGS = -f elf64
LDFLAGS = -T $(KERNEL_DIR)/arch/x86_64/boot/linker.ld -nostdlib -z max-page-size=0x1000

# Source files
ASM_SOURCES = $(KERNEL_DIR)/arch/x86_64/boot/boot.asm
C_SOURCES = $(KERNEL_DIR)/core/kernel.c \
            $(KERNEL_DIR)/core/printk.c \
            $(KERNEL_DIR)/core/panic.c

# Object files
ASM_OBJECTS = $(BUILD_DIR)/boot.o
C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
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
	@echo "  Run with QEMU:"
	@echo "    qemu-system-x86_64 -cdrom $(KERNEL_ISO)"
	@echo ""
	@echo "  Run with QEMU (with debug):"
	@echo "    qemu-system-x86_64 -cdrom $(KERNEL_ISO) -serial stdio -d int"
	@echo ""
	@echo "  Run in VMware:"
	@echo "    Create VM and boot from ISO"
	@echo ""

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(ISO_DIR)/boot/grub

# Assemble boot code
$(BUILD_DIR)/boot.o: $(ASM_SOURCES) | $(BUILD_DIR)
	@echo "  [NASM]  $<"
	@$(NASM) $(ASFLAGS) -o $@ $<

# Compile C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	@echo "  [CC]    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Link kernel
$(KERNEL_BIN): $(OBJECTS)
	@echo "  [LD]    $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "  [OBJCOPY] Strip debug info"
	@$(OBJCOPY) -O binary $@ $@.raw
	@mv $@.raw $@

# Create GRUB configuration
$(ISO_DIR)/boot/grub/grub.cfg: | $(BUILD_DIR)
	@echo "  [GRUB]  Creating grub.cfg"
	@mkdir -p $(ISO_DIR)/boot/grub
	@echo 'set timeout=5' > $@
	@echo 'set default=0' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (Safe Mode)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin nomodeset nosmp' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@
	@echo '' >> $@
	@echo 'menuentry "NEXUS OS (Debug Mode)" {' >> $@
	@echo '    multiboot2 /boot/$(KERNEL_NAME).bin debug loglevel=7' >> $@
	@echo '    boot' >> $@
	@echo '}' >> $@

# Create bootable ISO
$(KERNEL_ISO): $(KERNEL_BIN) $(ISO_DIR)/boot/grub/grub.cfg
	@echo "  [CP]    Copying kernel to ISO dir"
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	@echo "  [GRUB-MKRESCUE] Creating ISO"
	@if command -v $(GRUB_MKRESUE) > /dev/null 2>&1; then \
		$(GRUB_MKRESUE) -o $(KERNEL_ISO) $(ISO_DIR); \
		echo "  [SUCCESS] Bootable ISO created: $(KERNEL_ISO)"; \
	else \
		echo "  [WARNING] grub-mkrescue not found. Creating raw binary only."; \
		echo "  Install grub-pc-bin and grub-common for ISO creation."; \
		echo "  You can still use the binary with: qemu-system-x86_64 -kernel $(KERNEL_BIN)"; \
	fi

# Run in QEMU
run: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -m 2G -smp 2 \
		-serial stdio -display sdl

# Run in QEMU with debug
run-debug: $(KERNEL_ISO)
	@echo "  [QEMU]  Starting NEXUS OS (debug mode)..."
	@qemu-system-x86_64 -cdrom $(KERNEL_ISO) -m 2G -smp 2 \
		-serial stdio -d int -no-reboot -no-shutdown

# Run in QEMU with GDB
run-gdb: $(KERNEL_BIN)
	@echo "  [QEMU]  Starting NEXUS OS (waiting for GDB)..."
	@qemu-system-x86_64 -kernel $(KERNEL_BIN) -m 2G -smp 2 \
		-serial stdio -s -S

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
	@echo "  all       - Build kernel (default)"
	@echo "  run       - Build and run in QEMU"
	@echo "  run-debug - Build and run in QEMU with debug output"
	@echo "  run-gdb   - Build and run in QEMU waiting for GDB"
	@echo "  clean     - Remove build files"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC (with multilib support)"
	@echo "  - NASM (Netwide Assembler)"
	@echo "  - GRUB (for ISO creation)"
	@echo "  - QEMU (for testing)"
	@echo ""

.PHONY: all clean run run-debug run-gdb help
