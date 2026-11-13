; NEXUS OS - Interrupt Service Routine Stubs
; kernel/arch/x86_64/interrupts/interrupts.asm

[bits 64]

extern interrupt_handler
global isr_stub_table

section .text

; Common ISR handler
interrupt_common_stub:
    ; Save all registers
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; The interrupt_handler expects a pointer to the registers
    mov rdi, rsp
    call interrupt_handler

    ; Restore all registers (MUST mirror the pushes above exactly,
    ; in reverse order - rdi included)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Clean up error code and ISR number
    add rsp, 16
    
    ; Return from interrupt
    iretq

; Generate ISR stubs for all 256 interrupts
%assign i 0
%rep 256
global isr_stub_%+i
isr_stub_%+i:
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21 || i == 29 || i == 30
        ; Error code already on stack
    %else
        push qword 0      ; Push dummy error code
    %endif
    push qword i          ; Push ISR number
    jmp interrupt_common_stub
%assign i i+1
%endrep

; ISR Stub Table
section .data
align 8
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
