;
; NEXUS OS - Universal Operating System
; Copyright (c) 2024 NEXUS Development Team
;
; mbr.asm - Master Boot Record (MBR) Boot Sector
;
; This is the first-stage bootloader for BIOS systems.
; It loads the second-stage bootloader from disk.
;
; NASM Syntax - x86 Real Mode (16-bit)
;

BITS 16
ORG 0x7C00

;===========================================================================
;                         BOOT SECTOR CONSTANTS
;===========================================================================

%define BOOT_DRIVE      0x00            ; BIOS boot drive
%define KERNEL_LOAD_SEG 0x1000          ; Segment to load kernel
%define KERNEL_LOAD_OFF 0x0000          ; Offset to load kernel
%define STAGE2_LOAD_SEG 0x0000          ; Segment for stage2 loader
%define STAGE2_LOAD_OFF 0x7C00          ; Offset for stage2 loader
%define STAGE2_SECTOR   1               ; Stage2 starts at sector 1
%define STAGE2_SECTORS  15              ; Number of sectors for stage2

; Boot signature
%define BOOT_SIGNATURE  0xAA55

;===========================================================================
;                         BOOT CODE ENTRY POINT
;===========================================================================

start:
    ; Disable interrupts during setup
    cli

    ; Clear direction flag
    cld

    ; Initialize segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows down from boot sector

    ; Enable interrupts
    sti

    ; Save boot drive number
    mov [boot_drive], dl

    ; Display boot message
    mov si, msg_boot
    call print_string

    ; Initialize disk parameters
    call init_disk

    ; Load stage2 bootloader
    mov si, msg_load_stage2
    call print_string

    mov eax, STAGE2_SECTOR
    mov cx, STAGE2_SECTORS
    mov bx, STAGE2_LOAD_OFF
    mov dx, [boot_drive]
    call disk_read

    ; Verify stage2 signature
    cmp word [STAGE2_LOAD_SEG:STAGE2_LOAD_OFF + 510], BOOT_SIGNATURE
    jne boot_error

    ; Jump to stage2
    mov si, msg_stage2_ok
    call print_string

    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

;===========================================================================
;                         ERROR HANDLING
;===========================================================================

boot_error:
    mov si, msg_error
    call print_string

    ; Wait for keypress
    mov ah, 0x00
    int 0x16

    ; Reboot
    jmp 0xFFFF:0x0000

;===========================================================================
;                         DISK INITIALIZATION
;===========================================================================

init_disk:
    pusha

    ; Reset disk system
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc disk_reset_failed

    ; Check for LBA support
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc use_chs

    cmp bx, 0xAA55
    jne use_chs

    ; LBA supported
    mov byte [use_lba], 1
    jmp init_disk_done

use_chs:
    mov byte [use_lba], 0

disk_reset_failed:
    ; Continue with CHS even if reset failed

init_disk_done:
    popa
    ret

;===========================================================================
;                         DISK READ ROUTINE
;===========================================================================

; Input:
;   EAX = Starting sector (LBA)
;   CX  = Number of sectors to read
;   ES:BX = Destination buffer
;   DL  = Drive number
;
; Clobbers: AX, BX, CX, DX, SI, DI

disk_read:
    pusha

    cmp byte [use_lba], 1
    je disk_read_lba

    ; CHS Read
disk_read_chs:
    push eax            ; Save sector number
    push cx             ; Save sector count

    ; Convert LBA to CHS
    ; CHS = (LBA / (heads * sectors_per_track),
    ;        (LBA / sectors_per_track) % heads,
    ;        (LBA % sectors_per_track) + 1)
    ; Assuming: heads = 255, sectors_per_track = 63

    xor dx, dx
    mov bx, 63          ; sectors per track
    div bx              ; AX = LBA / 63, DX = LBA % 63

    mov cl, dl          ; CL = sector (0-based)
    inc cl              ; CL = sector (1-based)

    xor dx, dx
    mov bx, 255         ; heads
    div bx              ; AX = cylinder, DX = head

    mov ch, al          ; CH = cylinder (low 8 bits)
    mov dh, dl          ; DH = head
    mov cl, ch          ; Save cylinder low bits
    and cl, 0x3F        ; CL bits 6-7 = cylinder bits 8-9
    or cl, ch           ; CL = sector | (cylinder << 6)
    mov ch, ah          ; CH = cylinder high 8 bits

    pop cx              ; Restore sector count
    pop eax             ; Restore sector number (not needed for CHS)

    ; Read sectors using INT 13h AH=02h
    mov ah, 0x02
    mov dl, [boot_drive]
    int 0x13
    jc disk_read_chs_retry

    jmp disk_read_done

disk_read_chs_retry:
    ; Retry reset and read
    push cx
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    pop cx
    jc disk_read_error
    jmp disk_read_chs

    ; LBA Read
disk_read_lba:
    ; Setup DAP (Disk Address Packet)
    mov si, dap
    mov [dap_size], byte 0x10
    mov [dap_reserved], byte 0
    mov [dap_count], cx
    mov [dap_offset], bx
    mov [dap_segment], es
    mov [dap_lba_low], eax
    mov [dap_lba_high], edx

    ; Extended read
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_read_error

disk_read_done:
    popa
    ret

disk_read_error:
    popa
    mov si, msg_disk_error
    call print_string
    jmp boot_error

; DAP structure
dap:
    dap_size        db 0x10
    dap_reserved    db 0
    dap_count       dw 0
    dap_offset      dw 0
    dap_segment     dw 0
    dap_lba_low     dd 0
    dap_lba_high    dd 0

;===========================================================================
;                         VIDEO OUTPUT
;===========================================================================

; Print null-terminated string at DS:SI
print_string:
    pusha
    mov ah, 0x0E
    mov bh, 0x00      ; Page 0
    mov bl, 0x07      ; Attribute (light gray on black)

.print_loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .print_loop

.done:
    popa
    ret

; Print newline
print_newline:
    pusha
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10
    popa
    ret

; Print character in AL
print_char:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    popa
    ret

;===========================================================================
;                         DATA SECTION
;===========================================================================

boot_drive      db 0
use_lba         db 0

; Boot messages
msg_boot        db "NEXUS OS", 0x0D, 0x0A
                db "BIOS Bootloader", 0x0D, 0x0A
                db "----------------", 0x0D, 0x0A, 0

msg_load_stage2 db "Loading stage2...", 0

msg_stage2_ok   db " OK", 0x0D, 0x0A, 0

msg_error       db 0x0D, 0x0A
                db "Boot Error!", 0x0D, 0x0A
                db "Press any key to reboot...", 0

msg_disk_error  db "Disk Read Error!", 0x0D, 0x0A, 0

;===========================================================================
;                         BOOT SECTOR PADDING & SIGNATURE
;===========================================================================

; Pad to 510 bytes
times 510 - ($ - $$) db 0

; Boot signature
dw BOOT_SIGNATURE
