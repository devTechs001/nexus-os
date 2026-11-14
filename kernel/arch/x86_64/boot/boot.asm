; NEXUS OS - Bootable Kernel Entry Point
; arch/x86_64/boot/boot.asm
; NASM assembly - Multiboot2 boot with 32->64 bit transition

[bits 32]
; NOTE: Do NOT use [default rel] - it causes RIP-relative addressing which breaks 32-bit boot

; Entry stub section - placed FIRST in the binary so that boot methods
; that jump to the physical load address (e.g. stage2 bootloader) land
; on valid code, not the multiboot header data.
section .text.entry
    align 8

    global _start
_start:
    ; Jump over the multiboot header to the real boot code
    jmp short _start_kernel
    times 6 nop  ; Pad to 8 bytes for 8-byte alignment of multiboot header

section .multiboot
    ; Already 8-byte aligned after .text.entry

    ; Multiboot2 header - within first 32KB
    ; Using raw bytes to avoid NASM 3.01 calculation bugs
    align 8
    mboot2_header:
        ; Magic number: 0xE85250D6 (little-endian)
        db 0xD6, 0x50, 0x52, 0xE8
        ; Architecture: 0 (i386 protected mode)
        db 0x00, 0x00, 0x00, 0x00
        ; Header length: 72 bytes (0x48)
        ;   16 header
        ; +  8 framebuffer tag header + 12 payload      = 36, padded to 40
        ; +  8 module-alignment tag                      = 48
        ; +  8 information-request tag header + 4 value  = 60, padded to 64
        ; +  8 end tag                                   = 72
        db 0x48, 0x00, 0x00, 0x00
        ; Checksum: 0x17ADAEE2 (little-endian)
        ; multiboot2 requires magic + architecture + length + checksum == 0
        ; modulo 2^32. Verified:
        ;   0xE85250D6 + 0x00000000 + 0x00000048 + 0x17ADAEE2 = 0x100000000
        db 0xE2, 0xAE, 0xAD, 0x17

        ; Framebuffer tag (request graphics mode) - Type 5
        dw 5                      ; Type (5 = framebuffer)
        dw 0                      ; Flags (0 = optional)
        dd 20                     ; Size (20 bytes total)
        dd 1024                   ; Width (1024)
        dd 768                    ; Height (768)
        dd 32                     ; Depth (32 bpp)
        ; Pad the tag to an 8-byte boundary (8 + 12 = 20 -> 24)
        dd 0

        ; Module alignment tag - Type 6
        ;
        ; Asks the loader to page-align the modules it passes us. The userspace
        ; image is copied into memory a page at a time; paging it in from a
        ; page-aligned source means one source page maps to one destination
        ; page with no shared-page edge case at either end.
        dw 6                      ; Type (6 = module alignment)
        dw 0                      ; Flags
        dd 8                      ; Size (8 bytes total)

        ; Information request tag - Type 1
        ;
        ; Without this tag the bootloader is not required to provide the tags
        ; named here, and no module tag would appear in the multiboot
        ; information structure. The kernel would then find no userspace image
        ; and PID 1 could never start. Requesting type 3 (modules) makes the
        ; loader supply it.
        dw 1                      ; Type (1 = information request)
        dw 0                      ; Flags
        dd 16                     ; Size (16 bytes total)
        dd 3                      ; Requested tag type: 3 = modules
        ; Pad to 8-byte boundary (8 + 4 = 12 -> 16)
        dd 0

        ; End tag - Type 0 (must be 8-byte aligned, and ends the header)
        dw 0                      ; Type (0 = end)
        dw 0                      ; Flags
        dd 8                      ; Size (8 bytes total)
    mboot2_header_end:

; Switch to text section for code
section .text
    align 8

    ; 32-bit Kernel entry point (jumped to from .text.entry stub)
    global _start_kernel
    extern kernel_main_64         ; 64-bit kernel entry point
    extern kernel_panic
    extern __bss_start
    extern __bss_end
    extern gdt64_descriptor       ; 64-bit GDT descriptor

; Macro for serial wait (inline, no stack needed)
%macro SERIAL_WAIT 0
%%wait:
    mov dx, 0x3f8 + 5  ; Line status register
    in al, dx
    test al, 0x20      ; Check if transmit holding register is empty
    jz %%wait
%endmacro

; Simple serial output (no register preservation - use only when safe)
%macro SERIAL_PUT_RAW 1
    mov dx, 0x3f8
    mov al, %1
    out dx, al
    SERIAL_WAIT
%endmacro

; Macro for serial port initialization (115200 baud, 8N1)
%macro SERIAL_INIT 0
    push ax
    push dx
    ; Disable interrupts
    mov dx, 0x3f8 + 1
    mov al, 0x00
    out dx, al
    ; Enable DLAB (set baud rate divisor)
    mov dx, 0x3f8 + 3
    mov al, 0x80
    out dx, al
    ; Set divisor (115200 baud)
    mov dx, 0x3f8      ; DLL
    mov al, 0x01
    out dx, al
    mov dx, 0x3f8 + 1  ; DLM
    mov al, 0x00
    out dx, al
    ; Set line control (8 bits, no parity, one stop bit)
    mov dx, 0x3f8 + 3
    mov al, 0x03
    out dx, al
    ; Enable FIFO
    mov dx, 0x3f8 + 2
    mov al, 0xC7
    out dx, al
    ; Enable interrupts
    mov dx, 0x3f8 + 1
    mov al, 0x00
    out dx, al
    pop dx
    pop ax
%endmacro

_start_kernel:
    ; CLI - Disable interrupts immediately
    cli

    ; EARLY SERIAL INIT (115200 baud, 8N1) - No stack needed
    ; This must happen BEFORE any push/pop instructions
    ; COM1: 0x3F8
    mov dx, 0x3f8 + 1      ; Interrupt Enable Register
    mov al, 0x00
    out dx, al

    mov dx, 0x3f8 + 3      ; Line Control Register
    mov al, 0x80           ; Enable DLAB
    out dx, al

    mov dx, 0x3f8          ; Divisor Latch Low
    mov al, 0x01           ; 115200 baud
    out dx, al

    mov dx, 0x3f8 + 1      ; Divisor Latch High
    mov al, 0x00
    out dx, al

    mov dx, 0x3f8 + 3      ; Line Control Register
    mov al, 0x03           ; 8 bits, no parity, 1 stop
    out dx, al

    mov dx, 0x3f8 + 2      ; FIFO Control Register
    mov al, 0xC7           ; Enable FIFO
    out dx, al

    mov dx, 0x3f8 + 1      ; Interrupt Enable Register
    mov al, 0x00
    out dx, al

    ; Output 'B' to serial - Boot entry confirmed
    mov dx, 0x3f8
    mov al, 'B'
    out dx, al
    ; Wait for transmit
.wait_b:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_b

    ; Save multiboot2 info pointer (EBX = pointer to multiboot info)
    ; Use absolute addressing, not RIP-relative
    mov [multiboot_info], ebx
    mov [multiboot_magic], eax

    ; Verify multiboot magic (EAX should be 0x36d76289 for multiboot2)
    ; Output 'm' if magic doesn't match but continue booting (for debugging)
    cmp eax, 0x36d76289
    jne .multiboot_warn

    ; Magic OK - output 'K' for OK
    mov dx, 0x3f8
    mov al, 'K'
    out dx, al
.wait_k:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_k
    jmp .clear_bss

.multiboot_warn:
    ; Magic mismatch - output 'm' but continue (GRUB might still work)
    mov dx, 0x3f8
    mov al, 'm'
    out dx, al
.wait_m2:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_m2
    ; Continue booting anyway - multiboot info in EBX should still be valid

.clear_bss:
    ; Initialize ECX with BSS start and EDX with BSS size
    mov ecx, __bss_start
    mov edx, __bss_end
    sub edx, ecx
    jz .bss_done

.bss_loop:
    mov byte [ecx], 0
    inc ecx
    dec edx
    jnz .bss_loop

.bss_done:
    ; Output 'C' - BSS cleared
    mov dx, 0x3f8
    mov al, 'C'
    out dx, al
.wait_c:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_c

    ; Check for CPUID support
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    cmp eax, ecx
    jne .no_cpuid
    push ecx
    popfd

.no_cpuid:
    ; Check for long mode support (CPUID.80000001:EDX[29])
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000000
    jbe .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ; Serial debug - output 'L' for long mode supported
    SERIAL_PUT_RAW 'L'

    ; Disable interrupts (already disabled, but be explicit)
    cli

    ; Load unified GDT for transition
    lgdt [gdt_descriptor]

    ; Enable PAE (Physical Address Extension) - CR4.PAE = bit 5
    ; Also enable SSE (CR4.OSFXSR = bit 9, CR4.OSXMMEXCPT = bit 10)
    ; so the kernel can use SSE/SSE2 instructions generated by GCC -O2
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; Load page map level 4 table (identity mapping)
    mov eax, pml4_table
    mov cr3, eax

    ; Enable EFER (Extended Feature Enable Register)
    ;   EFER.LME = bit 8  (Long Mode Enable)
    ;   EFER.NXE = bit 11 (No-Execute Enable)
    ;
    ; NXE is REQUIRED, not optional: mm/vmm.c pte_create() sets the NX bit
    ; (bit 63) on every mapping that is not requested with VMA_EXEC - the
    ; kernel heap is mapped READ|WRITE. If EFER.NXE is clear, bit 63 is a
    ; reserved bit and any access to (or page walk through) such a mapping
    ; raises #PF with error code bit 3 (RSVD) set, e.g. 0xa. That is what
    ; killed boot at CR2=0xffffffff90000008, the kernel heap arena.
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    ; Enable paging - CR0.PG = bit 31
    ; This activates long mode
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Serial debug - output 'P' for paging enabled
    SERIAL_PUT_RAW 'P'

    ; Jump to 64-bit code using 0x18 selector (64-bit code)
    jmp 0x18:.start_64

.invalid_multiboot:
    ; Serial debug - output 'M' for invalid multiboot
    mov dx, 0x3f8
    mov al, 'M'
    out dx, al
.wait_m:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_m
    ; Halt on multiboot error
.halt_m:
    cli
    hlt
    jmp .halt_m

.multiboot_error:
    db "Error: Invalid multiboot magic!", 0

[bits 64]

.start_64:
    ; Immediately output '6' without wait - test if we're in 64-bit
    mov edx, 0x3f8
    mov al, '6'
    out dx, al

    ; Output 'A' to confirm 64-bit execution
    mov edx, 0x3f8
    mov al, 'A'
    out dx, al

    ; GDT already loaded in 32-bit mode, but segments need to be set
    ; In 64-bit mode, DS/ES/SS are mostly ignored (must be null or valid)
    ; Just zero them - don't load selectors
    xor eax, eax
    mov ds, ax
    mov es, ax
    ; SS is already set up by the far jump, don't touch it
    xor eax, eax
    mov fs, ax
    mov gs, ax

    ; Debug: output 's' after segment setup
    mov edx, 0x3f8
    mov al, 's'
    out dx, al

    ; Set up stack - use absolute address
    mov rsp, stack_top

    ; Debug: output 'R' after stack setup
    mov edx, 0x3f8
    mov al, 'R'
    out dx, al

    ; Clear flags
    push 0
    popfq

    ; Serial debug - output 'S' for stack ready
    mov edx, 0x3f8
    mov al, 'S'
    out dx, al

    ; Get multiboot info pointer (saved earlier)
    ; Pass in RDI as per System V AMD64 ABI
    mov rdi, [rel multiboot_info]
    mov rsi, [rel multiboot_magic]

    ; Serial debug - output 'I' for info passed
    mov edx, 0x3f8
    mov al, 'I'
    out dx, al

    ; Call 64-bit kernel entry point
    ; kernel_main_64(multiboot_info, multiboot_magic)
    call kernel_main_64

.halt:
    cli
    hlt
    jmp .halt

.no_long_mode:
    ; Serial debug - output 'X' for error
    mov dx, 0x3f8
    mov al, 'X'
    out dx, al
.wait_x:
    mov dx, 0x3f8 + 5
    in al, dx
    test al, 0x20
    jz .wait_x
    ; Halt on long mode error
.halt_x:
    cli
    hlt
    jmp .halt_x

.error_msg:
    db "Error: x86_64 long mode not supported!", 0

; ============================================================================
; Architecture-specific functions (64-bit)
; ============================================================================
[bits 64]

; global arch_get_cpu_id
; arch_get_cpu_id:
;     mov eax, 1
;     cpuid
;     mov eax, ebx
;     shr eax, 24
;     ret

global arch_get_ticks
arch_get_ticks:
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret

global arch_enable_interrupts
arch_enable_interrupts:
    sti
    ret

global arch_disable_interrupts
arch_disable_interrupts:
    cli
    ret

global arch_reboot
arch_reboot:
    ; Triple fault to reboot
    xor eax, eax
    lidt [rax]
    int3
    ret

global arch_shutdown
arch_shutdown:
    ; ACPI shutdown - halt
    cli
    hlt
    ret

; arch_init_gdt, arch_init_idt, arch_init_paging, arch_init_smp are defined in kernel.c as stubs

global arch_halt_cpu
arch_halt_cpu:
    cli
    hlt
    ret

global inb
inb:
    xor eax, eax
    mov dx, di
    in al, dx
    ret

global outb
outb:
    mov dx, di
    mov eax, esi
    out dx, al
    ret

global inw
inw:
    xor eax, eax
    mov dx, di
    in ax, dx
    ret

global outw
outw:
    mov dx, di
    mov eax, esi
    out dx, ax
    ret

global inl
inl:
    xor eax, eax
    mov dx, di
    in eax, dx
    ret

global outl
outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

global io_wait
io_wait:
    out 0x80, al
    ret

; ============================================================================
; Serial Port Helper Functions
; ============================================================================

; Wait for serial port transmit buffer to be empty (32-bit)
serial_wait_xmit:
    push dx
    push ax
.wait:
    mov dx, 0x3f8 + 5  ; Line status register
    in al, dx
    test al, 0x20      ; Check if transmit holding register is empty
    jz .wait
    pop ax
    pop dx
    ret

; Wait for serial port transmit buffer to be empty (64-bit)
serial_wait_xmit_64:
    push rdx
    push rax
.wait:
    mov dx, 0x3f8 + 5  ; Line status register
    in al, dx
    test al, 0x20      ; Check if transmit holding register is empty
    jz .wait
    pop rax
    pop rdx
    ret

; ============================================================================
; Page Tables (identity mapping for first 4GB)
; ============================================================================
section .data
    align 4096

pml4_table:
    ; Entry 0: point to pdpt_table
    dq pdpt_table + 0x03
    times 511 dq 0

pdpt_table:
    ; Entries 0-3: point to pd_tables (4 x 1GB = 4GB)
    dq pd_table_0 + 0x03
    dq pd_table_1 + 0x03
    dq pd_table_2 + 0x03
    dq pd_table_3 + 0x03
    times 508 dq 0

pd_table_0: ; 0GB - 1GB
    %assign i 0
    %rep 512
    dq (i * 0x200000) + 0x83  ; Present + Read/Write + 2MB page
    %assign i i+1
    %endrep

pd_table_1: ; 1GB - 2GB
    %assign i 512
    %rep 512
    dq (i * 0x200000) + 0x83
    %assign i i+1
    %endrep

pd_table_2: ; 2GB - 3GB
    %assign i 1024
    %rep 512
    dq (i * 0x200000) + 0x83
    %assign i i+1
    %endrep

pd_table_3: ; 3GB - 4GB
    %assign i 1536
    %rep 512
    dq (i * 0x200000) + 0x83
    %assign i i+1
    %endrep

; ============================================================================
; Unified GDT (for transition and long mode)
; ============================================================================
    align 16

gdt_start:
    ; 0x00: Null descriptor
    dq 0

    ; 0x08: 32-bit code segment (base=0, limit=4GB, type=code, D=1)
    dw 0xFFFF                   ; Limit (low)
    dw 0                        ; Base (low)
    db 0                        ; Base (middle)
    db 10011010b                ; Access (present, ring 0, code, readable)
    db 11001111b                ; Flags (4KB, 32-bit) + Limit (high)
    db 0                        ; Base (high)

    ; 0x10: 32-bit data segment (base=0, limit=4GB, type=data, D=1)
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b                ; Access (present, ring 0, data, writable)
    db 11001111b
    db 0

    ; 0x18: 64-bit code segment (base=0, limit=0, type=code, L=1)
    dw 0                        ; Limit (low)
    dw 0                        ; Base (low)
    db 0                        ; Base (middle)
    db 10011010b                ; Access (present, ring 0, code, readable)
    db 00100000b                ; Flags (64-bit) + Limit (high)
    db 0                        ; Base (high)

    ; 0x20: 64-bit data segment (base=0, limit=0, type=data)
    dw 0
    dw 0
    db 0
    db 10010010b                ; Access (present, ring 0, data, writable)
    db 00000000b
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============================================================================
; Storage for multiboot info (in .data to avoid BSS zeroing)
; ============================================================================
section .data
    align 16
    multiboot_info:
        dq 0
    multiboot_magic:
        dd 0

; ============================================================================
; Kernel stack (in .bss)
; ============================================================================
section .bss
    align 16
    stack_bottom:
        resb 65536
    stack_top:
