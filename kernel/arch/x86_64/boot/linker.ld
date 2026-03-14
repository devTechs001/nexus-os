/* NEXUS OS Kernel Linker Script */
/* kernel/arch/x86_64/boot/linker.ld */

ENTRY(_start)

SECTIONS
{
    . = 1M;  /* Load kernel at 1MB - GRUB requires this */

    /* Text section (code) - multiboot header MUST be first */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        KEEP(*(.multiboot))
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* Uninitialized data (BSS) */
    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    }

    __kernel_start = 1M;
    __kernel_end = .;

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.ARM.attributes)
    }
}
