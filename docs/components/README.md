# NEXUS OS - Boot System Documentation

## Overview

NEXUS OS uses a multiboot2-compliant boot system that transitions from 32-bit protected mode to 64-bit long mode, providing a robust foundation for the operating system.

## Boot Sequence

### Stage-by-Stage Boot Process

| Character | Stage | Description |
|-----------|-------|-------------|
| **B** | Boot Entry | 32-bit entry point `_start` reached |
| **m** | Multiboot | Magic number validated |
| **C** | BSS Clear | Zero-initialized data cleared |
| **L** | Long Mode | x86-64 CPU support detected |
| **P** | Paging | CR0.PG enabled, paging active |
| **6** | 64-bit Mode | Long mode entered successfully |
| **A** | 64-bit Exec | 64-bit code executing |
| **G** | GDT Load | 64-bit GDT loaded |
| **s** | Segments | DS/ES/FS/GS segment registers set |
| **R** | Stack Setup | RSP stack pointer configured |
| **S** | Stack Ready | Flags cleared, stack ready |
| **I** | Info Passed | Multiboot info to kernel_main_64() |

## Files

- `kernel/arch/x86_64/boot/boot.asm` - Boot assembly code
- `kernel/arch/x86_64/boot/linker.ld` - Linker script
- `build/iso/boot/grub/grub.cfg` - GRUB configuration

## Multiboot2 Header

The multiboot2 header is located at offset 0x1000 in the kernel binary:

```
Magic:      0xE85250D6
Architecture: 0 (i386)
Length:     48 bytes
Checksum:   0x17ADAEFA
```

## GRUB Configuration

The GRUB configuration provides 5 boot options:

1. **Graphical Mode** (Default) - Full GUI with framebuffer
2. **Text Mode** - VGA text console
3. **Safe Mode** - Minimal drivers, no SMP
4. **Debug Mode** - Verbose logging
5. **Native Hardware** - No virtualization optimizations

## Troubleshooting

### No Serial Output

If you don't see the `BmCLP6AGsRSI` sequence:

1. Check QEMU is running with `-serial stdio` or `-serial file:log.txt`
2. Verify multiboot2 header: `grub-file --is-x86-multiboot2 build/nexus-kernel.bin`
3. Check kernel entry point: `objdump -f build/nexus-kernel.bin`

### Triple Fault

If the system resets repeatedly:

1. Check the last character in serial output
2. 'P' but not '6' = paging setup issue
3. '6' but not 'S' = 64-bit transition issue
4. 'S' but not 'I' = stack setup issue

## Building

```bash
make clean
make
```

## Running

```bash
# Text mode with serial output
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -serial stdio -display none

# GUI mode
qemu-system-x86_64 -cdrom build/nexus-kernel.iso -m 2G -display gtk,gl=on
```

## References

- [BOOT_REPAIRS.md](../BOOT_REPAIRS.md) - Detailed boot repair documentation
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/)
- [OSDev Wiki - Multiboot2](https://wiki.osdev.org/Multiboot2)
