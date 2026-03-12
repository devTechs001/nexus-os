/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2024 NEXUS Development Team
 *
 * uefi_boot.c - UEFI Bootloader
 *
 * This module implements the UEFI bootloader for NEXUS OS.
 * It handles UEFI initialization, memory map acquisition,
 * and kernel loading via the UEFI boot services.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <efi.h>
#include <efilib.h>

/*===========================================================================*/
/*                         UEFI BOOTLOADER CONFIGURATION                     */
/*===========================================================================*/

#define NEXUS_BOOTLOADER_VERSION    "1.0.0"
#define NEXUS_BOOTLOADER_NAME       L"NEXUS OS UEFI Bootloader"
#define KERNEL_PATH                 L"\\NEXUS\\KERNEL.ELF"
#define INITRD_PATH                 L"\\NEXUS\\INITRD.IMG"

#define MAX_MEMORY_MAP_SIZE         (1024 * 1024)
#define MAX_KERNEL_SIZE             (64 * 1024 * 1024)
#define MAX_CMDLINE_SIZE            1024

#define KERNEL_LOAD_ADDRESS         0x100000
#define KERNEL_ENTRY_ADDRESS        0x100000

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Boot parameters passed to kernel */
typedef struct {
    u64 magic;
    u64 kernel_base;
    u64 kernel_size;
    u64 kernel_entry;
    u64 initrd_base;
    u64 initrd_size;
    u64 memory_map;
    u64 memory_map_size;
    u64 memory_map_descriptor_size;
    u32 memory_map_descriptor_version;
    u64 rsdp_address;
    u64 framebuffer_base;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 framebuffer_pitch;
    u8  framebuffer_bpp;
    u8  framebuffer_format;
    u16 reserved;
    char cmdline[MAX_CMDLINE_SIZE];
} boot_params_t;

#define BOOT_PARAMS_MAGIC           0x4E5853424F4F5450ULL  /* "NXSBOOTP" */

/* Memory map entry */
typedef struct {
    u32 type;
    u32 reserved;
    u64 physical_start;
    u64 virtual_start;
    u64 number_of_pages;
    u64 attribute;
} memory_map_entry_t;

/* Memory types (UEFI) */
#define EfiLoaderCode             1
#define EfiLoaderData             2
#define EfiBootServicesCode       3
#define EfiBootServicesData       4
#define EfiRuntimeServicesCode    5
#define EfiRuntimeServicesData    6
#define EfiConventionalMemory     7
#define EfiUnusableMemory         8
#define EfiACPIReclaimMemory      9
#define EfiACPIMemoryNVS          10
#define EfiMemoryMappedIO         11
#define EfiMemoryMappedIOPortSpace 12
#define EfiPalCode                13
#define EfiPersistentMemory       14

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static EFI_SYSTEM_TABLE *gST = NULL;
static EFI_BOOT_SERVICES *gBS = NULL;
static EFI_RUNTIME_SERVICES *gRT = NULL;

static boot_params_t boot_params;
static void *memory_map = NULL;
static u64 memory_map_size = 0;
static u64 mmap_desc_size = 0;
static u32 mmap_desc_version = 0;

static void *kernel_image = NULL;
static u64 kernel_size = 0;

static void *initrd_image = NULL;
static u64 initrd_size = 0;

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * efi_print - Print formatted string to UEFI console
 */
static void efi_print(const char16_t *fmt, ...)
{
    if (gST && gST->ConOut) {
        va_list args;
        va_start(args, fmt);
        gST->ConOut->OutputString(gST->ConOut, (char16_t *)fmt);
        va_end(args);
    }
}

/**
 * efi_print_ascii - Print ASCII string to UEFI console
 */
static void efi_print_ascii(const char *str)
{
    if (!gST || !gST->ConOut) return;

    char16_t buffer[256];
    int i = 0;

    while (str[i] && i < 255) {
        buffer[i] = (char16_t)str[i];
        i++;
    }
    buffer[i] = 0;

    gST->ConOut->OutputString(gST->ConOut, buffer);
}

/**
 * efi_status_str - Convert EFI status to string
 */
static const char *efi_status_str(EFI_STATUS status)
{
    switch (status) {
        case EFI_SUCCESS:               return "SUCCESS";
        case EFI_LOAD_ERROR:            return "LOAD_ERROR";
        case EFI_INVALID_PARAMETER:     return "INVALID_PARAMETER";
        case EFI_UNSUPPORTED:           return "UNSUPPORTED";
        case EFI_BAD_BUFFER_SIZE:       return "BAD_BUFFER_SIZE";
        case EFI_BUFFER_TOO_SMALL:      return "BUFFER_TOO_SMALL";
        case EFI_NOT_FOUND:             return "NOT_FOUND";
        case EFI_ALREADY_STARTED:       return "ALREADY_STARTED";
        case EFI_ABORTED:               return "ABORTED";
        case EFI_ICMP_ERROR:            return "ICMP_ERROR";
        case EFI_TFTP_ERROR:            return "TFTP_ERROR";
        case EFI_PROTOCOL_ERROR:        return "PROTOCOL_ERROR";
        case EFI_INCOMPATIBLE_VERSION:  return "INCOMPATIBLE_VERSION";
        case EFI_SECURITY_VIOLATION:    return "SECURITY_VIOLATION";
        default:                        return "UNKNOWN";
    }
}

/**
 * efi_clear_screen - Clear the UEFI console screen
 */
static void efi_clear_screen(void)
{
    if (gST && gST->ConOut) {
        gST->ConOut->ClearScreen(gST->ConOut);
    }
}

/**
 * efi_set_text_mode - Set console text mode
 */
static void efi_set_text_mode(bool enable)
{
    if (gST && gST->ConOut) {
        gST->ConOut->SetAttribute(gST->ConOut,
            enable ? EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE : EFI_LIGHTGRAY);
    }
}

/*===========================================================================*/
/*                         MEMORY MANAGEMENT                                 */
/*===========================================================================*/

/**
 * efi_allocate_pages - Allocate pages from UEFI
 */
static EFI_STATUS efi_allocate_pages(u64 *address, u64 num_pages)
{
    EFI_PHYSICAL_ADDRESS alloc_addr = *address;

    return gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                               num_pages, &alloc_addr);
}

/**
 * efi_free_pages - Free pages back to UEFI
 */
static EFI_STATUS efi_free_pages(u64 address, u64 num_pages)
{
    return gBS->FreePages(address, num_pages);
}

/**
 * efi_allocate_pool - Allocate from pool
 */
static EFI_STATUS efi_allocate_pool(u64 size, void **buffer)
{
    return gBS->AllocatePool(EfiLoaderData, size, buffer);
}

/**
 * efi_free_pool - Free pool memory
 */
static EFI_STATUS efi_free_pool(void *buffer)
{
    return gBS->FreePool(buffer);
}

/*===========================================================================*/
/*                         MEMORY MAP HANDLING                               */
/*===========================================================================*/

/**
 * efi_get_memory_map - Get UEFI memory map
 */
static EFI_STATUS efi_get_memory_map(void)
{
    EFI_STATUS status;

    /* Initial attempt to get size */
    status = gBS->GetMemoryMap(&memory_map_size, NULL,
                                &mmap_desc_version, &mmap_desc_size,
                                NULL);

    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    /* Add some extra space for safety */
    memory_map_size += 2 * mmap_desc_size;

    /* Allocate buffer for memory map */
    status = efi_allocate_pool(memory_map_size, &memory_map);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to allocate memory map buffer\r\n");
        return status;
    }

    /* Get actual memory map */
    status = gBS->GetMemoryMap(&memory_map_size, memory_map,
                                &mmap_desc_version, &mmap_desc_size,
                                NULL);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to get memory map\r\n");
        efi_free_pool(memory_map);
        memory_map = NULL;
        return status;
    }

    efi_print_ascii("[INFO] Memory map acquired (");
    /* Would print size here */
    efi_print_ascii(" bytes)\r\n");

    return EFI_SUCCESS;
}

/**
 * efi_print_memory_map - Print memory map entries
 */
static void efi_print_memory_map(void)
{
    if (!memory_map) return;

    u64 num_entries = memory_map_size / mmap_desc_size;
    memory_map_entry_t *entry = (memory_map_entry_t *)memory_map;

    efi_print_ascii("\r\nMemory Map:\r\n");
    efi_print_ascii("  Type    Physical Start   Pages      Attributes\r\n");
    efi_print_ascii("  ----    --------------   -----      ----------\r\n");

    for (u64 i = 0; i < num_entries; i++) {
        const char *type_str = "Unknown";

        switch (entry->type) {
            case EfiLoaderCode:           type_str = "LoaderCode"; break;
            case EfiLoaderData:           type_str = "LoaderData"; break;
            case EfiBootServicesCode:     type_str = "BS_Code"; break;
            case EfiBootServicesData:     type_str = "BS_Data"; break;
            case EfiRuntimeServicesCode:  type_str = "RT_Code"; break;
            case EfiRuntimeServicesData:  type_str = "RT_Data"; break;
            case EfiConventionalMemory:   type_str = "Available"; break;
            case EfiUnusableMemory:       type_str = "Unusable"; break;
            case EfiACPIReclaimMemory:    type_str = "ACPI"; break;
            case EfiACPIMemoryNVS:        type_str = "ACPI_NVS"; break;
            case EfiMemoryMappedIO:       type_str = "MMIO"; break;
            case EfiMemoryMappedIOPortSpace: type_str = "MMIO_Port"; break;
            case EfiPalCode:              type_str = "PAL"; break;
            case EfiPersistentMemory:     type_str = "Persistent"; break;
        }

        /* Print entry info */
        /* In real implementation, would convert numbers to strings */

        entry = (memory_map_entry_t *)((u64)entry + mmap_desc_size);
    }

    efi_print_ascii("\r\n");
}

/*===========================================================================*/
/*                         FILE LOADING                                      */
/*===========================================================================*/

/**
 * efi_load_file - Load file from EFI file system
 */
static EFI_STATUS efi_load_file(EFI_FILE_HANDLE root, const char16_t *path,
                                 void **buffer, u64 *size)
{
    EFI_STATUS status;
    EFI_FILE_HANDLE file_handle;
    u64 file_size;

    /* Open file */
    status = root->Open(root, &file_handle, (char16_t *)path,
                        EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to open file: ");
        /* Would print path */
        efi_print_ascii("\r\n");
        return status;
    }

    /* Get file size */
    file_size = 0;
    status = file_handle->GetPosition(file_handle, &file_size);
    if (EFI_ERROR(status)) {
        file_handle->Close(file_handle);
        return status;
    }

    /* Seek to end to get size */
    file_handle->SetPosition(file_handle, ~0ULL);
    file_handle->GetPosition(file_handle, &file_size);
    file_handle->SetPosition(file_handle, 0);

    *size = file_size;

    /* Allocate buffer */
    status = efi_allocate_pool(file_size, buffer);
    if (EFI_ERROR(status)) {
        file_handle->Close(file_handle);
        return status;
    }

    /* Read file */
    status = file_handle->Read(file_handle, &file_size, *buffer);
    if (EFI_ERROR(status)) {
        efi_free_pool(*buffer);
        *buffer = NULL;
        file_handle->Close(file_handle);
        return status;
    }

    file_handle->Close(file_handle);

    return EFI_SUCCESS;
}

/**
 * efi_load_kernel - Load kernel image
 */
static EFI_STATUS efi_load_kernel(EFI_FILE_HANDLE root)
{
    EFI_STATUS status;

    efi_print_ascii("[INFO] Loading kernel: ");
    efi_print_ascii((const char *)KERNEL_PATH);
    efi_print_ascii("\r\n");

    status = efi_load_file(root, KERNEL_PATH, &kernel_image, &kernel_size);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to load kernel: ");
        efi_print_ascii(efi_status_str(status));
        efi_print_ascii("\r\n");
        return status;
    }

    efi_print_ascii("[INFO] Kernel loaded: ");
    /* Would print size */
    efi_print_ascii(" bytes\r\n");

    return EFI_SUCCESS;
}

/**
 * efi_load_initrd - Load initial ramdisk
 */
static EFI_STATUS efi_load_initrd(EFI_FILE_HANDLE root)
{
    EFI_STATUS status;

    efi_print_ascii("[INFO] Loading initrd: ");
    efi_print_ascii((const char *)INITRD_PATH);
    efi_print_ascii("\r\n");

    status = efi_load_file(root, INITRD_PATH, &initrd_image, &initrd_size);
    if (status == EFI_NOT_FOUND) {
        efi_print_ascii("[INFO] No initrd found, continuing without\r\n");
        initrd_image = NULL;
        initrd_size = 0;
        return EFI_SUCCESS;
    }

    if (EFI_ERROR(status)) {
        efi_print_ascii("[WARN] Failed to load initrd: ");
        efi_print_ascii(efi_status_str(status));
        efi_print_ascii("\r\n");
        initrd_image = NULL;
        initrd_size = 0;
        return EFI_SUCCESS;  /* Continue without initrd */
    }

    efi_print_ascii("[INFO] Initrd loaded: ");
    /* Would print size */
    efi_print_ascii(" bytes\r\n");

    return EFI_SUCCESS;
}

/*===========================================================================*/
/*                         GRAPHICS INITIALIZATION                           */
/*===========================================================================*/

/**
 * efi_init_graphics - Initialize graphics output protocol
 */
static EFI_STATUS efi_init_graphics(void)
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    boot_params.framebuffer_base = 0;
    boot_params.framebuffer_width = 0;
    boot_params.framebuffer_height = 0;
    boot_params.framebuffer_pitch = 0;
    boot_params.framebuffer_bpp = 0;
    boot_params.framebuffer_format = 0;

    /* Locate GOP */
    status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                                  NULL, (void **)&gop);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[WARN] Graphics output not available\r\n");
        return status;
    }

    /* Get current mode info */
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    u64 size_of_info;

    status = gop->QueryMode(gop, gop->Mode->Mode, &size_of_info, &mode_info);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[WARN] Failed to query graphics mode\r\n");
        return status;
    }

    boot_params.framebuffer_base = (u64)gop->Mode->FrameBufferBase;
    boot_params.framebuffer_width = mode_info->HorizontalResolution;
    boot_params.framebuffer_height = mode_info->VerticalResolution;
    boot_params.framebuffer_pitch = mode_info->PixelsPerScanLine * 4;
    boot_params.framebuffer_bpp = 32;
    boot_params.framebuffer_format = (u8)mode_info->PixelFormat;

    efi_print_ascii("[INFO] Graphics initialized: ");
    /* Would print resolution */
    efi_print_ascii("\r\n");

    return EFI_SUCCESS;
}

/*===========================================================================*/
/*                         ACPI / RSDP                                       */
/*===========================================================================*/

/**
 * efi_find_rsdp - Find ACPI RSDP table
 */
static EFI_STATUS efi_find_rsdp(void)
{
    EFI_STATUS status;

    boot_params.rsdp_address = 0;

    /* Try to get ACPI table from configuration table */
    EFI_GUID acpi2_guid = ACPI_20_TABLE_GUID;
    EFI_GUID acpi1_guid = ACPI_TABLE_GUID;

    for (u64 i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *table = &gST->ConfigurationTable[i];

        if (CompareGuid(&table->VendorGuid, &acpi2_guid) ||
            CompareGuid(&table->VendorGuid, &acpi1_guid)) {
            boot_params.rsdp_address = (u64)table->VendorTable;
            efi_print_ascii("[INFO] ACPI RSDP found\r\n");
            return EFI_SUCCESS;
        }
    }

    efi_print_ascii("[WARN] ACPI RSDP not found\r\n");
    return EFI_NOT_FOUND;
}

/*===========================================================================*/
/*                         BOOT PARAMETER SETUP                              */
/*===========================================================================*/

/**
 * efi_setup_boot_params - Setup boot parameters for kernel
 */
static void efi_setup_boot_params(void)
{
    /* Zero out boot params */
    memset(&boot_params, 0, sizeof(boot_params_t));

    /* Set magic */
    boot_params.magic = BOOT_PARAMS_MAGIC;

    /* Kernel info */
    boot_params.kernel_base = KERNEL_LOAD_ADDRESS;
    boot_params.kernel_size = kernel_size;
    boot_params.kernel_entry = KERNEL_ENTRY_ADDRESS;

    /* Initrd info */
    boot_params.initrd_base = (u64)initrd_image;
    boot_params.initrd_size = initrd_size;

    /* Memory map */
    boot_params.memory_map = (u64)memory_map;
    boot_params.memory_map_size = memory_map_size;
    boot_params.memory_map_descriptor_size = mmap_desc_size;
    boot_params.memory_map_descriptor_version = mmap_desc_version;

    /* Command line */
    /* Would copy from NVRAM or config */
    strcpy(boot_params.cmdline, "root=/dev/nvme0n1p2 quiet splash");
}

/*===========================================================================*/
/*                         KERNEL HANDOFF                                    */
/*===========================================================================*/

/**
 * efi_exit_boot_services - Exit UEFI boot services
 */
static EFI_STATUS efi_exit_boot_services(void)
{
    EFI_STATUS status;
    u64 map_key;

    /* Get memory map one more time before exiting */
    u64 new_map_size = memory_map_size;
    void *new_map = NULL;

    status = gBS->GetMemoryMap(&new_map_size, NULL,
                                &mmap_desc_version, &mmap_desc_size,
                                &map_key);

    if (new_map_size > memory_map_size) {
        status = efi_allocate_pool(new_map_size, &new_map);
        if (EFI_ERROR(status)) {
            return status;
        }

        status = gBS->GetMemoryMap(&new_map_size, new_map,
                                    &mmap_desc_version, &mmap_desc_size,
                                    &map_key);
        if (EFI_ERROR(status)) {
            efi_free_pool(new_map);
            return status;
        }

        if (memory_map) {
            efi_free_pool(memory_map);
        }
        memory_map = new_map;
        memory_map_size = new_map_size;
    }

    /* Exit boot services */
    status = gBS->ExitBootServices(gST->ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to exit boot services: ");
        efi_print_ascii(efi_status_str(status));
        efi_print_ascii("\r\n");
        return status;
    }

    efi_print_ascii("[INFO] Boot services exited\r\n");

    return EFI_SUCCESS;
}

/**
 * efi_jump_to_kernel - Jump to kernel entry point
 */
static void __noreturn efi_jump_to_kernel(void)
{
    typedef void (*kernel_entry_t)(boot_params_t *);

    kernel_entry_t kernel_entry = (kernel_entry_t)KERNEL_ENTRY_ADDRESS;

    efi_print_ascii("[INFO] Jumping to kernel...\r\n");

    /* Clear screen for kernel */
    efi_clear_screen();

    /* Jump to kernel */
    kernel_entry(&boot_params);

    /* Should never reach here */
    for (;;);
}

/*===========================================================================*/
/*                         UEFI ENTRY POINT                                  */
/*===========================================================================*/

/**
 * efi_main - UEFI bootloader entry point
 */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image;
    EFI_FILE_HANDLE root;

    /* Initialize globals */
    gST = system_table;
    gBS = system_table->BootServices;
    gRT = system_table->RuntimeServices;

    /* Initialize EFI library */
    InitializeLib(image_handle, system_table);

    /* Display boot banner */
    efi_clear_screen();
    efi_set_text_mode(true);

    efi_print_ascii("\r\n");
    efi_print_ascii("===============================================\r\n");
    efi_print_ascii("  NEXUS OS UEFI Bootloader v");
    efi_print_ascii(NEXUS_BOOTLOADER_VERSION);
    efi_print_ascii("\r\n");
    efi_print_ascii("===============================================\r\n");
    efi_print_ascii("\r\n");

    /* Get loaded image info */
    status = gBS->HandleProtocol(image_handle, &gEfiLoadedImageProtocolGuid,
                                  (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to get loaded image info\r\n");
        return status;
    }

    /* Open root directory */
    status = loaded_image->DeviceHandle->OpenVolume(loaded_image->DeviceHandle,
                                                     &root);
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to open volume\r\n");
        return status;
    }

    /* Initialize graphics */
    efi_init_graphics();

    /* Get memory map */
    status = efi_get_memory_map();
    if (EFI_ERROR(status)) {
        efi_print_ascii("[ERROR] Failed to get memory map\r\n");
        return status;
    }

    /* Print memory map (debug) */
    /* efi_print_memory_map(); */

    /* Find ACPI RSDP */
    efi_find_rsdp();

    /* Load kernel */
    status = efi_load_kernel(root);
    if (EFI_ERROR(status)) {
        return status;
    }

    /* Load initrd */
    efi_load_initrd(root);

    /* Setup boot parameters */
    efi_setup_boot_params();

    /* Exit boot services */
    status = efi_exit_boot_services();
    if (EFI_ERROR(status)) {
        return status;
    }

    /* Jump to kernel */
    efi_jump_to_kernel();

    /* Should never reach here */
    return EFI_ABORTED;
}

/*===========================================================================*/
/*                         STRING FUNCTIONS                                  */
/*===========================================================================*/

/**
 * memset - Set memory to value
 */
static void *memset(void *s, int c, size_t n)
{
    u8 *p = (u8 *)s;
    while (n--) {
        *p++ = (u8)c;
    }
    return s;
}

/**
 * strcpy - Copy string
 */
static char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}
