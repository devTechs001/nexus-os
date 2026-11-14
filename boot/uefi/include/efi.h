/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * boot/uefi/include/efi.h - Minimal UEFI type definitions
 *
 * uefi_boot.c included <efi.h> and <efilib.h> and neither existed anywhere in
 * the tree, so the file had never been compiled: a fatal "No such file or
 * directory" before a single line of it was checked.
 *
 * This is not a replacement for EDK2 or GNU-EFI. It declares exactly the
 * subset uefi_boot.c uses, with the field offsets of the real UEFI 2.x ABI,
 * because these are hardware-defined structures reached by fixed offset. If
 * the layout here were invented rather than transcribed, every call through
 * gBS would land in the wrong slot.
 *
 * Where the full specification is needed, build against EDK2's MdePkg or
 * GNU-EFI and drop -Iboot/uefi/include from the UEFI target.
 */

#ifndef _NEXUS_EFI_H
#define _NEXUS_EFI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*===========================================================================*/
/*                         BASIC TYPES                                       */
/*===========================================================================*/

typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef int8_t    INT8;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef int64_t   INT64;
typedef uint64_t  UINTN;
typedef int64_t   INTN;
typedef uint8_t   BOOLEAN;
/* char16_t is a <uchar.h> type; it is not available under -ffreestanding and
 * not guaranteed by the UEFI build either, so define the 16-bit unit here. */
typedef uint16_t  CHAR16;

/* The driver source spells it this way too. */
typedef uint16_t  char16_t;
typedef void      VOID;

#ifndef TRUE
#define TRUE  ((BOOLEAN)1)
#endif
#ifndef FALSE
#define FALSE ((BOOLEAN)0)
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Wide string literal, as the EDK2 headers define it. */
#ifndef L
#define L(string) u##string
#endif

typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;
typedef UINTN  EFI_STATUS;
typedef void  *EFI_HANDLE;
typedef void  *EFI_EVENT;

#define EFIAPI __attribute__((ms_abi))
#define IN
#define OUT
#define OPTIONAL
#define CONST const

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/*===========================================================================*/
/*                         STATUS CODES                                      */
/*===========================================================================*/

#define EFI_SUCCESS               0ULL
#define EFI_LOAD_ERROR            (1ULL | (1ULL << 63))
#define EFI_INVALID_PARAMETER     (2ULL | (1ULL << 63))
#define EFI_UNSUPPORTED           (3ULL | (1ULL << 63))
#define EFI_BAD_BUFFER_SIZE       (4ULL | (1ULL << 63))
#define EFI_BUFFER_TOO_SMALL      (5ULL | (1ULL << 63))
#define EFI_NOT_READY             (6ULL | (1ULL << 63))
#define EFI_DEVICE_ERROR          (7ULL | (1ULL << 63))
#define EFI_WRITE_PROTECTED       (8ULL | (1ULL << 63))
#define EFI_OUT_OF_RESOURCES      (9ULL | (1ULL << 63))
#define EFI_VOLUME_CORRUPTED      (10ULL | (1ULL << 63))
#define EFI_NOT_FOUND             (14ULL | (1ULL << 63))
#define EFI_ACCESS_DENIED         (15ULL | (1ULL << 63))
#define EFI_ABORTED               (21ULL | (1ULL << 63))
#define EFI_SECURITY_VIOLATION    (26ULL | (1ULL << 63))
#define EFI_ALREADY_STARTED       (20ULL | (1ULL << 63))
#define EFI_INCOMPATIBLE_VERSION  (23ULL | (1ULL << 63))
#define EFI_PROTOCOL_ERROR        (35ULL | (1ULL << 63))
#define EFI_ICMP_ERROR            (36ULL | (1ULL << 63))
#define EFI_TFTP_ERROR            (37ULL | (1ULL << 63))

#define EFI_ERROR(Status)  (((INTN)(Status)) < 0)

/*===========================================================================*/
/*                         CONSOLE                                           */
/*===========================================================================*/

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    VOID *Reset;
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      CHAR16 *String);
    VOID *TestString;
    EFI_STATUS (EFIAPI *QueryMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                   UINTN ModeNumber, UINTN *Columns,
                                   UINTN *Rows);
    EFI_STATUS (EFIAPI *SetMode)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                 UINTN ModeNumber);
    EFI_STATUS (EFIAPI *SetAttribute)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      UINTN Attribute);
    EFI_STATUS (EFIAPI *ClearScreen)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    VOID *SetCursorPosition;
    VOID *EnableCursor;
    VOID *Mode;
};

/* Attribute byte: foreground in the low nibble, background in the high. */
#define EFI_BLACK          0x00
#define EFI_BLUE           0x01
#define EFI_GREEN          0x02
#define EFI_CYAN           0x03
#define EFI_RED            0x04
#define EFI_MAGENTA        0x05
#define EFI_BROWN          0x06
#define EFI_LIGHTGRAY      0x07
#define EFI_DARKGRAY       0x08
#define EFI_LIGHTBLUE      0x09
#define EFI_LIGHTGREEN     0x0A
#define EFI_LIGHTCYAN      0x0B
#define EFI_LIGHTRED       0x0C
#define EFI_LIGHTMAGENTA   0x0D
#define EFI_YELLOW         0x0E
#define EFI_WHITE          0x0F

#define EFI_BACKGROUND_BLACK     0x00
#define EFI_BACKGROUND_BLUE      (EFI_BLUE << 4)
#define EFI_BACKGROUND_GREEN     (EFI_GREEN << 4)
#define EFI_BACKGROUND_CYAN      (EFI_CYAN << 4)
#define EFI_BACKGROUND_RED       (EFI_RED << 4)
#define EFI_BACKGROUND_MAGENTA   (EFI_MAGENTA << 4)
#define EFI_BACKGROUND_BROWN     (EFI_BROWN << 4)
#define EFI_BACKGROUND_LIGHTGRAY (EFI_LIGHTGRAY << 4)

/*===========================================================================*/
/*                         TABLES                                            */
/*===========================================================================*/

typedef struct {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    UINT32                     Type;
    EFI_PHYSICAL_ADDRESS       PhysicalStart;
    EFI_VIRTUAL_ADDRESS        VirtualStart;
    UINT64                     NumberOfPages;
    UINT64                     Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* Event types and notify functions, referenced only as opaque handles. */
typedef VOID (EFIAPI *EFI_EVENT_NOTIFY)(EFI_EVENT Event, VOID *Context);

/*
 * EFI_BOOT_SERVICES -- UEFI 2.x layout.
 *
 * Only the entries uefi_boot.c calls are declared with real signatures; the
 * rest are retained as pointer-sized placeholders so the offsets of the ones
 * that matter stay correct. Dropping any of them would shift every subsequent
 * field.
 */
struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    /* Task Priority Services */
    VOID *RaiseTPL;
    VOID *RestoreTPL;

    /* Memory Services */
    EFI_STATUS (EFIAPI *AllocatePages)(EFI_ALLOCATE_TYPE Type,
                                       EFI_MEMORY_TYPE MemoryType,
                                       UINTN Pages,
                                       EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS Memory,
                                   UINTN Pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize,
                                      EFI_MEMORY_DESCRIPTOR *MemoryMap,
                                      UINTN *MapKey,
                                      UINTN *DescriptorSize,
                                      UINT32 *DescriptorVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size,
                                      VOID **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(VOID *Buffer);

    /* Event & Timer Services */
    VOID *CreateEvent;
    VOID *SetTimer;
    VOID *WaitForEvent;
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;

    /* Protocol Handler Services */
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE Handle, const EFI_GUID *Protocol,
                                        VOID **Interface);
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;

    /* Image Services */
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle,
                                          UINTN MapKey);

    /* Miscellaneous Services */
    VOID *GetNextMonotonicCount;
    VOID *Stall;
    VOID *SetWatchdogTimer;

    /* DriverSupport Services */
    VOID *ConnectController;
    VOID *DisconnectController;

    /* Open and Close Protocol Services */
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;

    /* Library Services */
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;
    EFI_STATUS (EFIAPI *LocateProtocol)(const EFI_GUID *Protocol,
                                        VOID *Registration OPTIONAL,
                                        VOID **Interface);
    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC Services */
    VOID *CalculateCrc32;
};

struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER Hdr;
    VOID *GetTime;
    VOID *SetTime;
    VOID *GetWakeupTime;
    VOID *SetWakeupTime;
    VOID *SetVirtualAddressMap;
    VOID *ConvertPointer;
    VOID *GetVariable;
    VOID *GetNextVariableName;
    VOID *SetVariable;
    VOID *GetNextHighMonotonicCount;
    VOID *ResetSystem;
};

typedef struct {
    UINT32                         Version;
    UINT32                         HorizontalResolution;
    UINT32                         VerticalResolution;
    UINT32                         PixelFormat;
    UINT32                         PixelRedMask;
    UINT32                         PixelGreenMask;
    UINT32                         PixelBlueMask;
    UINT32                         PixelReservedMask;
    UINT32                         PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32                                MaxMode;
    UINT32                                Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
    UINTN                                 SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                  FrameBufferBase;
    UINTN                                 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                                   UINT32 ModeNumber,
                                   UINTN *SizeOfInfo,
                                   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS (EFIAPI *SetMode)(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                                 UINT32 ModeNumber);
    VOID *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

typedef struct {
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} EFI_LOADED_IMAGE_PROTOCOL_PLACEHOLDER;

/* Real EFI_LOADED_IMAGE is large; only the device handle is ever read here. */
typedef struct {
    UINT32          Revision;
    EFI_HANDLE      ParentHandle;
    VOID           *SystemTable;
    EFI_HANDLE      DeviceHandle;
    VOID           *FilePath;
    VOID           *Reserved;
    UINT32          LoadOptionsSize;
    VOID           *LoadOptions;
    VOID           *ImageBase;
    UINT64          ImageSize;
    UINT32          ImageCodeType;
    UINT32          ImageDataType;
    EFI_STATUS    (*Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE;

typedef struct {
    EFI_TABLE_HEADER            Hdr;
    CHAR16                     *FirmwareVendor;
    UINT32                      FirmwareRevision;
    EFI_HANDLE                  ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE                  ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                  StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES       *RuntimeServices;
    EFI_BOOT_SERVICES          *BootServices;
    UINTN                       NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE    *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* Simple File System, used only to open the kernel image. */
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef EFI_FILE_PROTOCOL *EFI_FILE_HANDLE;

#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

struct EFI_FILE_PROTOCOL {
    UINT64       Revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *This,
                              EFI_FILE_PROTOCOL **NewHandle,
                              CHAR16 *FileName,
                              UINT64 OpenMode,
                              UINT64 Attributes);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize,
                              VOID *Buffer);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize,
                               VOID *Buffer);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL *This,
                                     UINT64 *Position);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL *This,
                                     UINT64 Position);
    VOID *GetInfo;
    VOID *SetInfo;
    VOID *Flush;
};

typedef struct {
    VOID *Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(VOID *This, EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

/*===========================================================================*/
/*                         WELL-KNOWN GUIDS                                   */
/*===========================================================================*/

/*
 * These values are fixed by the specification. A wrong byte here means
 * LocateProtocol finds nothing and the driver silently installs nothing.
 */

/* EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID
 * 9042a9de-23dc-4a38-96fb-7aded080516a */
static const EFI_GUID gEfiGraphicsOutputProtocolGuid = {
    0x9042A9DE, 0x23DC, 0x4A38,
    { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }
};

/* EFI_LOADED_IMAGE_PROTOCOL_GUID
 * 5b1b31a1-9562-11d2-8e3f-00a0c969723b */
static const EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1, 0x9562, 0x11D2,
    { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

/* EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID
 * 964e5b22-6459-11d2-8e39-00a0c969723b */
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964E5B22, 0x6459, 0x11D2,
    { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

/* ACPI 2.0 table: 8868e871-e4f1-11d3-bc22-0080c73c8881 */
#define ACPI_20_TABLE_GUID \
    { 0x8868E871, 0xE4F1, 0x11D3, \
      { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } }

/* ACPI 1.0 table: eb9d2d30-2d88-11d3-9a16-0090273fc14d */
#define ACPI_TABLE_GUID \
    { 0xEB9D2D30, 0x2D88, 0x11D3, \
      { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

/*===========================================================================*/
/*                         GUID HELPERS                                      */
/*===========================================================================*/

/*
 * CompareGuid - Compare two GUIDs
 *
 * Provided by GNU-EFI's efilib; uefi_boot.c calls it, so a freestanding build
 * needs its own.
 */
static inline BOOLEAN CompareGuid(const EFI_GUID *a, const EFI_GUID *b)
{
    const UINT8 *pa = (const UINT8 *)a;
    const UINT8 *pb = (const UINT8 *)b;
    UINTN i;

    if (!a || !b) {
        return FALSE;
    }

    for (i = 0; i < sizeof(EFI_GUID); i++) {
        if (pa[i] != pb[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

#endif /* _NEXUS_EFI_H */
