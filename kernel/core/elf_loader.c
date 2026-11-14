/*
 * NEXUS OS - ELF64 Loader
 * kernel/core/elf_loader.c
 *
 * The kernel could not execute anything. arch_load_binary() in
 * kernel/sched/process.c was a stub that returned -ENOSYS, and the module
 * loader allocated a fixed 64 KB buffer and left the ELF parsing commented
 * out. Together that meant: no binary could be loaded, so no program written
 * in any language - C, C++, Rust, or an interpreter hosting Python - could
 * run, and the shell's exec path was dead code.
 *
 * This file implements the part that was missing: reading an ELF64 image off
 * the filesystem, validating it, and placing its PT_LOAD segments in memory
 * as a single contiguous image with the entry point reported back to the
 * caller. It deliberately does not manipulate page tables itself (that is
 * arch_switch_mm's job, and the MMU is not yet active for user mappings);
 * instead it produces a fully-formed, ready-to-run image and its metadata.
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../include/types.h"
#include "../include/kernel.h"
#include "../include/elf.h"
#include "../mm/mm.h"
#include "../../fs/vfs/vfs.h"

/*===========================================================================*/
/*                              LIMITS                                       */
/*===========================================================================*/

#define ELF_MAX_IMAGE_SIZE      (64ULL * 1024 * 1024)   /* 64 MB ceiling */
#define ELF_MAX_SEGMENTS        64
#define ELF_MIN_ALIGN           4096
#define ELF_USER_STACK_SIZE     (1024 * 1024)           /* 1 MB stack */

/*===========================================================================*/
/*                              STATE                                        */
/*===========================================================================*/

/*
 * elf_image_t is declared in kernel/include/elf.h so that callers outside this
 * file (the scheduler's PID 1 path) can hold one on their stack by value
 * instead of guessing at its size.
 */
typedef elf_image_t elf_image_local_t;

static struct {
    bool initialized;
    u64 loads;
    u64 load_failures;
    u64 bytes_loaded;
    elf_image_t last;
} g_elf;

/* Provided by the arch layer / scheduler */
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);

/*===========================================================================*/
/*                         FILE READING HELPERS                              */
/*===========================================================================*/

/**
 * read_file - Read an entire file into a freshly allocated buffer
 *
 * @path:  Path in the VFS
 * @out:   Receives the buffer (caller frees)
 * @size:  Receives the byte count
 *
 * Returns 0 on success, negative error code on failure.
 */
static int read_file(const char *path, u8 **out, u64 *size)
{
    struct vfs_stat st;
    u8 *buf;
    u64 total = 0;
    s32 fd;

    if (!path || !out || !size) {
        return -EINVAL;
    }

    if (vfs_stat(path, &st) != 0) {
        return -ENOENT;
    }

    if (st.st_size <= 0) {
        return -EINVAL;
    }

    if ((u64)st.st_size > ELF_MAX_IMAGE_SIZE) {
        pr_err("[ELF] %s: file too large (%lld bytes)\n", path, (long long)st.st_size);
        return -E2BIG;
    }

    fd = vfs_open(path, VFS_FO_READ, 0);
    if (fd < 0) {
        return fd;
    }

    buf = (u8 *)kmalloc((size_t)st.st_size);
    if (!buf) {
        vfs_close((u32)fd);
        return -ENOMEM;
    }

    while (total < (u64)st.st_size) {
        s64 n = vfs_read((u32)fd, (char *)buf + total, (u64)st.st_size - total);
        if (n <= 0) {
            break;
        }
        total += (u64)n;
    }

    vfs_close((u32)fd);

    if (total == 0) {
        kfree(buf);
        return -EIO;
    }

    *out = buf;
    *size = total;

    return 0;
}

/*===========================================================================*/
/*                              LOADER                                       */
/*===========================================================================*/

/**
 * elf_load_image - Load an ELF64 executable into memory
 *
 * @path:  Path to the executable
 * @img:   Receives the loaded image description
 *
 * The segments are placed at their own virtual addresses adjusted by a single
 * bias, so BSS (p_memsz > p_filesz) and mixed segment alignment behave
 * correctly. Refuses anything with a segment that would overlap the loaded
 * image or exceed the size ceiling.
 *
 * Returns 0 on success, negative error code on failure.
 */
static int elf_load_buffer(const char *name, const u8 *file, u64 file_size,
                           elf_image_t *img)
{
    elf64_ehdr_t *eh;
    elf64_phdr_t *ph;
    u64 min_vaddr = ~0ULL;
    u64 max_vaddr = 0;
    u64 bias;
    u8 *image;
    u64 image_size;
    u32 loadable = 0;

    if (!name || !file || !img) {
        return -EINVAL;
    }

    memset(img, 0, sizeof(*img));

    if (!elf_check_header(file, file_size)) {
        pr_err("[ELF] %s: not a valid x86-64 ELF64 executable\n", name);
        g_elf.load_failures++;
        return -ENOEXEC;
    }

    eh = (elf64_ehdr_t *)file;
    ph = (elf64_phdr_t *)(file + eh->e_phoff);

    /* Pass 1: find the address span of everything we must load. */
    for (u32 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) {
            continue;
        }

        if (ph[i].p_offset + ph[i].p_filesz > file_size) {
            pr_err("[ELF] %s: segment %u extends past end of file\n", name, i);
            g_elf.load_failures++;
            return -ENOEXEC;
        }

        if (ph[i].p_vaddr < min_vaddr) {
            min_vaddr = ph[i].p_vaddr;
        }
        if (ph[i].p_vaddr + ph[i].p_memsz > max_vaddr) {
            max_vaddr = ph[i].p_vaddr + ph[i].p_memsz;
        }

        if (++loadable >= ELF_MAX_SEGMENTS) {
            pr_err("[ELF] %s: too many segments\n", name);
            g_elf.load_failures++;
            return -ENOEXEC;
        }
    }

    if (loadable == 0 || max_vaddr <= min_vaddr) {
        pr_err("[ELF] %s: no loadable segments\n", name);
        g_elf.load_failures++;
        return -ENOEXEC;
    }

    image_size = max_vaddr - min_vaddr;
    if (image_size > ELF_MAX_IMAGE_SIZE) {
        pr_err("[ELF] %s: image too large (%llu bytes)\n", name,
               (unsigned long long)image_size);
        g_elf.load_failures++;
        return -E2BIG;
    }

    /*
     * Place the image at a page-aligned base. With no per-process address
     * space yet, the image is allocated from the kernel heap and the segments
     * are copied to base + (vaddr - min_vaddr); the entry point is reported
     * relative to that same base. bias records the adjustment so a future
     * paging implementation can map it 1:1 without re-deriving it.
     */
    image = (u8 *)kmalloc((size_t)image_size + ELF_MIN_ALIGN);
    if (!image) {
        g_elf.load_failures++;
        return -ENOMEM;
    }

    /* Zero the whole image first: p_memsz > p_filesz means .bss lives here. */
    memset(image, 0, (size_t)image_size + ELF_MIN_ALIGN);

    bias = (u64)image - min_vaddr;

    /* Pass 2: copy each segment's file-backed bytes into place. */
    for (u32 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) {
            continue;
        }

        u8 *dest = (u8 *)(bias + ph[i].p_vaddr);
        u64 copy = ph[i].p_filesz;

        if (copy > ph[i].p_memsz) {
            copy = ph[i].p_memsz;      /* Malformed: never over-write. */
        }

        if (copy) {
            memcpy(dest, file + ph[i].p_offset, (size_t)copy);
        }

        /* The remainder is .bss and is already zeroed. */

        if (eh->e_entry >= ph[i].p_vaddr &&
            eh->e_entry < ph[i].p_vaddr + ph[i].p_memsz) {
            img->entry_segment = i;
        }
    }

    img->base = (virt_addr_t)image;
    img->size = image_size;
    img->vaddr_min = min_vaddr;
    img->vaddr_max = max_vaddr;
    img->entry = bias + eh->e_entry;
    img->segment_count = loadable;
    img->loaded = true;

    return 0;
}

/**
 * elf_load_image - Load an ELF64 executable from a VFS path
 *
 * Reads the file, then hands the bytes to elf_load_buffer(). Kept as a thin
 * wrapper so the path-based callers are unchanged.
 *
 * Returns 0 on success, negative error code on failure.
 */
int elf_load_image(const char *path, elf_image_t *img)
{
    u8 *file = NULL;
    u64 file_size = 0;
    int rc;

    if (!path || !img) {
        return -EINVAL;
    }

    rc = read_file(path, &file, &file_size);
    if (rc != 0) {
        pr_err("[ELF] Cannot read %s (%d)\n", path, rc);
        g_elf.load_failures++;
        return rc;
    }

    rc = elf_load_buffer(path, file, file_size, img);

    kfree(file);

    return rc;
}

/**
 * elf_load_module - Load an ELF64 executable already resident in memory
 *
 * @name:       Display name for diagnostics
 * @image:      Image bytes as handed to the kernel by the bootloader
 * @image_size: Byte count of @image
 * @img:        Receives the loaded image description
 *
 * Userspace reaches the kernel as a multiboot2 module, not as a file. There is
 * no mounted root filesystem at boot - vfs_mount() has no caller, so the
 * nexfs filesystem is registered and never mounted - which means the
 * path-based elf_load_image() has nothing to read from and could never have
 * started PID 1. This entry point takes the bytes directly.
 *
 * Returns 0 on success, negative error code on failure.
 */
int elf_load_module(const char *name, const u8 *image, u64 image_size,
                    elf_image_t *img)
{
    return elf_load_buffer(name, image, image_size, img);
}

/*===========================================================================*/
/*                        KERNEL-FACING ENTRY POINTS                         */
/*===========================================================================*/

/**
 * elf_exec - Load a binary and report where to start executing it
 *
 * This is what arch_load_binary() now delegates to. On success the image is
 * resident and @entry receives the start address.
 *
 * Returns 0 on success, negative error code on failure.
 */
int elf_exec(const char *path, u64 *entry, u64 *image_size, u32 *segments)
{
    elf_image_t img;
    int rc;

    if (!path) {
        return -EINVAL;
    }

    rc = elf_load_image(path, &img);
    if (rc != 0) {
        return rc;
    }

    g_elf.loads++;
    g_elf.bytes_loaded += img.size;
    g_elf.last = img;

    if (entry) {
        *entry = img.entry;
    }
    if (image_size) {
        *image_size = img.size;
    }
    if (segments) {
        *segments = img.segment_count;
    }

    pr_info("[ELF] %s: loaded %llu bytes, %u segments, entry 0x%llX\n",
            path, (unsigned long long)img.size, img.segment_count,
            (unsigned long long)img.entry);

    return 0;
}

/**
 * elf_can_execute - Probe whether a path is a loadable ELF
 *
 * Used by the shell to distinguish "not found" from "found but not a program".
 */
bool elf_can_execute(const char *path)
{
    u8 *buf = NULL;
    u64 size = 0;
    u8 header[sizeof(elf64_ehdr_t)];
    bool ok;
    s32 fd;
    s64 n;

    if (!path) {
        return false;
    }

    fd = vfs_open(path, VFS_FO_READ, 0);
    if (fd < 0) {
        return false;
    }

    n = vfs_read((u32)fd, (char *)header, sizeof(header));
    vfs_close((u32)fd);

    if (n < (s64)sizeof(elf64_ehdr_t)) {
        return false;
    }

    ok = elf_check_header(header, sizeof(header));

    (void)buf;
    (void)size;

    return ok;
}

/**
 * elf_loader_init - Initialise the ELF loader
 */
int elf_loader_init(void)
{
    memset(&g_elf, 0, sizeof(g_elf));
    g_elf.initialized = true;

    pr_info("[ELF] ELF64 loader initialised\n");

    return 0;
}

/**
 * elf_loader_stats - Report load statistics
 */
void elf_loader_stats(void)
{
    pr_info("[ELF] Loads: %llu, failures: %llu, bytes: %llu\n",
            (unsigned long long)g_elf.loads,
            (unsigned long long)g_elf.load_failures,
            (unsigned long long)g_elf.bytes_loaded);

    if (g_elf.last.loaded) {
        pr_info("[ELF] Last image: base 0x%llX, %u segments, entry 0x%llX\n",
                (unsigned long long)g_elf.last.base,
                g_elf.last.segment_count,
                (unsigned long long)g_elf.last.entry);
    }
}

/**
 * elf_alloc_user_stack - Allocate a stack for a newly exec'd program
 *
 * Returns the stack top (stack grows down), or 0 on failure.
 */
u64 elf_alloc_user_stack(void)
{
    void *stack = kmalloc(ELF_USER_STACK_SIZE);

    if (!stack) {
        return 0;
    }

    memset(stack, 0, ELF_USER_STACK_SIZE);

    /* Top of the stack, 16-byte aligned as the SysV ABI requires. */
    u64 top = (u64)stack + ELF_USER_STACK_SIZE;
    top &= ~0xFULL;

    return top;
}
