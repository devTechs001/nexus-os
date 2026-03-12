/*
 * NEXUS OS - Bootable Kernel Entry Point
 * arch/x86_64/boot/boot.asm
 * 
 * NASM assembly - 64-bit kernel entry point
 * This is the FIRST code that runs after bootloader
 */

[bits 64]
[default rel]

section .multiboot
    align 8
    MBOOT_MAGIC equ 0x1BADB002
    MBOOT_FLAGS equ 0x00000003
    MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)
    
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

section .bss
    align 16
    stack_bottom:
        resb 65536  ; 64KB stack
    stack_top:

section .text
    global _start
    global _start_kernel
    extern kernel_main
    extern kernel_panic

_start:
    ; Set up stack
    mov rsp, stack_top
    
    ; Clear EFLAGS
    push 0
    popfq
    
    ; Check if we're running on x86_64
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000000
    jbe .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29  ; Check LM bit
    jz .no_long_mode
    
    ; Call kernel main
    call kernel_main
    
    ; If kernel_main returns, halt
.halt:
    cli
    hlt
    jmp .halt

.no_long_mode:
    ; Can't run in 64-bit mode
    mov rdi, .error_msg
    call kernel_panic
    
.error_msg:
    db "Error: x86_64 long mode not supported!", 0

; Port I/O functions
global inb
global outb
global inw
global outw
global inl
global outl
global io_wait

inb:
    xor eax, eax
    mov dx, di
    in al, dx
    ret

outb:
    mov dx, di
    mov eax, esi
    out dx, al
    ret

inw:
    xor eax, eax
    mov dx, di
    in ax, dx
    ret

outw:
    mov dx, di
    mov eax, esi
    out dx, ax
    ret

inl:
    xor eax, eax
    mov dx, di
    in eax, dx
    ret

outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

io_wait:
    out 0x80, al
    ret

; CPUID wrapper
global cpuid
cpuid:
    push rbx
    mov eax, edi
    mov ecx, esi
    cpuid
    ; Results in eax, ebx, ecx, edx
    pop rbx
    ret

; Get CPU ID
global get_cpu_id
get_cpu_id:
    mov eax, 1
    cpuid
    mov eax, ebx
    shr eax, 24
    ret

; Halt CPU
global halt_cpu
halt_cpu:
    cli
    hlt
    ret

; Enable interrupts
global enable_interrupts
enable_interrupts:
    sti
    ret

; Disable interrupts
global disable_interrupts
disable_interrupts:
    cli
    ret

; Get RFLAGS
global get_rflags
get_rflags:
    pushfq
    pop rax
    ret

; Read CR0
global read_cr0
read_cr0:
    mov rax, cr0
    ret

; Read CR3
global read_cr3
read_cr3:
    mov rax, cr3
    ret

; Read CR4
global read_cr4
read_cr4:
    mov rax, cr4
    ret

; Write CR3
global write_cr3
write_cr3:
    mov cr3, rdi
    ret

; Read MSR
global read_msr
read_msr:
    mov ecx, edi
    rdmsr
    shl rdx, 32
    or rax, rdx
    ret

; Write MSR
global write_msr
write_msr:
    mov ecx, edi
    mov eax, esi
    mov edx, esi
    shr rdx, 32
    wrmsr
    ret

; Read TSC
global read_tsc
read_tsc:
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret

; Read TSC auxiliary (synchronized)
global read_tsc_aux
read_tsc_aux:
    rdtscp
    shl rdx, 32
    or rax, rdx
    ret

; Memory barrier
global memory_barrier
memory_barrier:
    mfence
    ret

; Read memory barrier
global read_memory_barrier
read_memory_barrier:
    lfence
    ret

; Write memory barrier
global write_memory_barrier
write_memory_barrier:
    sfence
    ret

; Pause instruction (for spinlocks)
global cpu_pause
cpu_pause:
    pause
    ret

; System call instruction
global syscall_invoke
syscall_invoke:
    ; rdi = syscall number
    ; rsi = arg0
    ; rdx = arg1
    ; r10 = arg2
    ; r8  = arg3
    ; r9  = arg4
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, r10
    mov r10, r8
    mov r8, r9
    syscall
    ret

; Get stack pointer
global get_stack_pointer
get_stack_pointer:
    mov rax, rsp
    ret

; Get base pointer
global get_base_pointer
get_base_pointer:
    mov rax, rbp
    ret

; Get instruction pointer
global get_instruction_pointer
get_instruction_pointer:
    mov rax, [rsp]
    ret

; Simple memcpy (for early boot before full kernel)
global early_memcpy
early_memcpy:
    ; rdi = dest, rsi = src, rdx = count
    push rdi
    push rsi
    push rdx
    
    mov rcx, rdx
    rep movsb
    
    pop rdx
    pop rsi
    pop rdi
    mov rax, rdi
    ret

; Simple memset (for early boot)
global early_memset
early_memset:
    ; rdi = dest, esi = value, rdx = count
    push rdi
    push rsi
    push rdx
    
    mov rax, rdi
    mov rcx, rdx
    mov al, sil
    rep stosb
    
    pop rdx
    pop rsi
    pop rdi
    mov rax, rdi
    ret

; Kernel entry point called from boot.asm
global _start_kernel
_start_kernel:
    jmp _start

section .data
    align 8
    
; GDT Descriptor
global gdt_descriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; GDT
align 16
gdt_start:
    ; Null descriptor
    dq 0x0000000000000000
    
    ; Kernel code segment (offset 0x08)
    dw 0xFFFF      ; Limit (bits 0-15)
    dw 0x0000      ; Base (bits 0-15)
    db 0x00        ; Base (bits 16-23)
    db 10011010b   ; Access byte (present, ring 0, code, readable)
    db 00100000b   ; Flags + Limit (bits 16-19)
    db 0x00        ; Base (bits 24-31)
    dd 0x00000000  ; Base (bits 32-63) + Reserved
    
    ; Kernel data segment (offset 0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b   ; Access byte (present, ring 0, data, writable)
    db 00000000b
    db 0x00
    dd 0x00000000
    
    ; User code segment (offset 0x18)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11111010b   ; Access byte (present, ring 3, code, readable)
    db 00100000b
    db 0x00
    dd 0x00000000
    
    ; User data segment (offset 0x20)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11110010b   ; Access byte (present, ring 3, data, writable)
    db 00000000b
    db 0x00
    dd 0x00000000
gdt_end:

; TSS (Task State Segment)
align 16
global tss_start
tss_start:
    dd 0x00000000  ; Reserved
    dq stack_top   ; RSP0 (kernel stack)
    dq 0x00000000  ; RSP1
    dq 0x00000000  ; RSP2
    dq 0x00000000  ; Reserved
    dq 0x00000000  ; IST1
    dq 0x00000000  ; IST2
    dq 0x00000000  ; IST3
    dq 0x00000000  ; Reserved
    dw 0x0000      ; Reserved
    dw 0x68        ; IOPB offset (no I/O permission bitmap)
tss_end:

; TSS Descriptor
global tss_descriptor
tss_descriptor:
    dw tss_end - tss_start - 1
    dw tss_start & 0xFFFF
    db (tss_start >> 16) & 0xFF
    db 10001001b   ; Present, ring 0, TSS (available)
    db ((tss_start >> 24) & 0xF) | 0x40
    db (tss_start >> 32) & 0xFF
    dq (tss_start >> 40) & 0xFFFFFFFF
    dd 0x00000000
