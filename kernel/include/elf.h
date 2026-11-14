/*
 * NEXUS OS - ELF64 Definitions
 * kernel/include/elf.h
 *
 * Minimal but complete ELF64 (little-endian, x86-64) structures needed to
 * load a static executable. There were no ELF definitions anywhere in the
 * tree, which is why arch_load_binary() was a stub returning -ENOSYS: nothing
 * could parse a binary, so no program could ever run - including anything
 * built from Rust, C++ or Python.
 */

#ifndef _NEXUS_ELF_H
#define _NEXUS_ELF_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/*                            IDENTIFICATION                                 */
/*===========================================================================*/

#define ELF_MAGIC               0x464C457FU     /* "\x7fELF" */
#define ELF_MAGIC_0             0x7F
#define ELF_MAGIC_1             'E'
#define ELF_MAGIC_2             'L'
#define ELF_MAGIC_3             'F'

#define EI_NIDENT               16
#define EI_MAG0                 0
#define EI_MAG1                 1
#define EI_MAG2                 2
#define EI_MAG3                 3
#define EI_CLASS                4
#define EI_DATA                 5
#define EI_VERSION              6
#define EI_OSABI                7

#define ELFCLASS32              1
#define ELFCLASS64              2

#define ELFDATA2LSB             1
#define ELFDATA2MSB             2

#define EV_CURRENT              1

/*===========================================================================*/
/*                              TYPES                                        */
/*===========================================================================*/

#define ET_NONE                 0
#define ET_REL                  1
#define ET_EXEC                 2
#define ET_DYN                  3               /* PIE / shared object */

#define EM_X86_64               62

#define ET_EXEC_OR_DYN(t)       ((t) == ET_EXEC || (t) == ET_DYN)

/*===========================================================================*/
/*                          PROGRAM HEADERS                                  */
/*===========================================================================*/

#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_TLS                  7
#define PT_GNU_STACK            0x6474E551
#define PT_GNU_RELRO            0x6474E552

#define PF_X                    0x1             /* Executable */
#define PF_W                    0x2             /* Writable */
#define PF_R                    0x4             /* Readable */

typedef struct {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} elf64_phdr_t;

/*===========================================================================*/
/*                           SECTION HEADERS                                 */
/*===========================================================================*/

#define SHT_NULL                0
#define SHT_PROGBITS            1
#define SHT_SYMTAB              2
#define SHT_STRTAB              3
#define SHT_RELA                4
#define SHT_NOBITS              8

typedef struct {
    u32 sh_name;
    u32 sh_type;
    u64 sh_flags;
    u64 sh_addr;
    u64 sh_offset;
    u64 sh_size;
    u32 sh_link;
    u32 sh_info;
    u64 sh_addralign;
    u64 sh_entsize;
} elf64_shdr_t;

/*===========================================================================*/
/*                                HEADER                                     */
/*===========================================================================*/

typedef struct {
    u8  e_ident[EI_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} elf64_ehdr_t;

/*===========================================================================*/
/*                            SYMBOLS                                        */
/*===========================================================================*/

typedef struct {
    u32 st_name;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
    u64 st_value;
    u64 st_size;
} elf64_sym_t;

#define ELF64_ST_BIND(i)        ((i) >> 4)
#define ELF64_ST_TYPE(i)        ((i) & 0xF)
#define STB_GLOBAL              1
#define STT_FUNC                2

/*===========================================================================*/
/*                         VALIDATION HELPERS                                */
/*===========================================================================*/

/**
 * elf_check_header - Verify that a buffer looks like a loadable x86-64 ELF
 *
 * Returns true only for a little-endian 64-bit executable/shared object
 * targeting x86-64. Anything else is rejected rather than mis-parsed.
 */
static inline bool elf_check_header(const void *buf, u64 size)
{
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)buf;

    if (!buf || size < sizeof(elf64_ehdr_t)) {
        return false;
    }

    if (eh->e_ident[EI_MAG0] != ELF_MAGIC_0 ||
        eh->e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        eh->e_ident[EI_MAG2] != ELF_MAGIC_2 ||
        eh->e_ident[EI_MAG3] != ELF_MAGIC_3) {
        return false;
    }

    if (eh->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }
    if (eh->e_machine != EM_X86_64) {
        return false;
    }
    if (!ET_EXEC_OR_DYN(eh->e_type)) {
        return false;
    }

    /* Program header table must be inside the file. */
    if (eh->e_phnum == 0 || eh->e_phoff == 0) {
        return false;
    }
    if (eh->e_phoff + (u64)eh->e_phnum * sizeof(elf64_phdr_t) > size) {
        return false;
    }

    return true;
}

/*
 * elf_image_t - A loaded ELF image
 *
 * Filled in by the loader in kernel/core/elf_loader.c and passed back to
 * callers that need the entry point or the address span. Defined here rather
 * than in the .c file so the scheduler's PID 1 path can hold one by value.
 */
typedef struct {
    virt_addr_t base;           /* Where the image was placed */
    u64 size;                   /* Image span (max_vaddr - min_vaddr) */
    u64 entry;                  /* Entry point, adjusted to the load base */
    u64 vaddr_min;
    u64 vaddr_max;
    u32 segment_count;
    u32 entry_segment;
    bool loaded;
} elf_image_t;

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_ELF_H */
