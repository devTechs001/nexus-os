/*
 * NEXUS OS Userspace Linker Script
 * userspace/linker.ld
 *
 * Userspace is linked as a position-independent, flat ET_EXEC image at a fixed
 * base well above the kernel's 1 MB load address, so an image placed there by
 * the kernel's ELF loader cannot collide with kernel text or the boot stack.
 *
 * These programs are freestanding: no crt0, no libc startup, and no interpreter
 * (no PT_INTERP tag), because there is no dynamic linker in the image.
 *
 * _start is supplied by userspace/libsys/crt0.c, which calls main() and then
 * exit() via the SYS_EXIT system call.
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    /*
     * Base address.
     *
     * Must not overlap the kernel (1 MB, with its own .bss extending a few MB)
     * and must be below the stack the kernel hands us in elf_alloc_user_stack().
     * 4 MB gives a clean margin above the kernel image and keeps the whole
     * user program plus its stack inside what the loader can kmalloc today.
     */
    . = 0x400000;

    .text : ALIGN(4K)
    {
        *(.text.entry)
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Discard what a freestanding static executable does not need. */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
