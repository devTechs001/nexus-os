/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * boot/uefi/include/efilib.h - GNU-EFI helper shim
 *
 * uefi_boot.c included <efilib.h> (the GNU-EFI convenience layer) and used
 * nothing from it: every console write goes through gST->ConOut->OutputString,
 * every allocation through gBS->AllocatePool/AllocatePages, and the formatting
 * is done locally by efi_print()/efi_print_ascii(). The include was there and
 * the header was not, which is why the file never compiled.
 *
 * This header exists so the include resolves. If the build moves to GNU-EFI,
 * drop -Iboot/uefi/include from the UEFI target and the real efilib.h takes
 * over with no source changes.
 */

#ifndef _NEXUS_EFILIB_H
#define _NEXUS_EFILIB_H

#include "efi.h"

#endif /* _NEXUS_EFILIB_H */
