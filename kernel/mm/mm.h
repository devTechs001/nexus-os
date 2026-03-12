/*
 * NEXUS OS - Memory Management Header
 * kernel/mm/mm.h
 */

#ifndef _NEXUS_MM_H
#define _NEXUS_MM_H

#include "../include/kernel.h"

/*===========================================================================*/
/*                         MEMORY MANAGEMENT                                 */
/*===========================================================================*/

/* Memory Region Types */
#define MEM_REGION_AVAILABLE    0
#define MEM_REGION_RESERVED     1
#define MEM_REGION_ACPI         2
#define MEM_REGION_NVS          3
#define MEM_REGION_UNUSABLE     4
#define MEM_REGION_KERNEL       5
#define MEM_REGION_INITRD       6

/* Memory Flags */
#define MEM_FLAG_PRESENT        (1ULL << 0)
#define MEM_FLAG_WRITABLE       (1ULL << 1)
#define MEM_FLAG_USER           (1ULL << 2)
#define MEM_FLAG_NOCACHE        (1ULL << 3)
#define MEM_FLAG_WRITECOMBINE   (1ULL << 4)
#define MEM_FLAG_HUGEPAGE       (1ULL << 5)
#define MEM_FLAG_DMA            (1ULL << 6)
#define MEM_FLAG_DMA32          (1ULL << 7)

/* Memory Zone Types */
#define ZONE_DMA                0
#define ZONE_DMA32              1
#define ZONE_NORMAL             2
#define ZONE_HIGHMEM            3
#define MAX_NR_ZONES            4

/*===========================================================================*/
/*                         MEMORY STRUCTURES                                 */
/*===========================================================================*/

/**
 * memory_region - Physical memory region
 */
struct memory_region {
    phys_addr_t start;
    phys_addr_t end;
    u32 type;
    u32 flags;
    char name[32];
};

/**
 * page - Page structure
 */
struct page {
    unsigned long flags;
    union {
        struct {
            struct page *next;
            struct page *prev;
        };
        struct {
            struct page *first;
        };
    };
    atomic_t refcount;
    atomic_t mapcount;
    unsigned long index;
    void *virtual;
    phys_addr_t physical;
};

/* Page Flags */
#define PG_locked           0
#define PG_error          1
#define PG_referenced     2
#define PG_uptodate       3
#define PG_dirty          4
#define PG_lru            5
#define PG_active         6
#define PG_slab           7
#define PG_reserved       8
#define PG_private        9
#define PG_writeback      10
#define PG_head           11
#define PG_tail           12
#define PG_compound       13
#define PG_swapcache      14
#define PG_mappedtodisk   15

/**
 * vm_area - Virtual memory area
 */
struct vm_area {
    virt_addr_t start;
    virt_addr_t end;
    u32 flags;
    u32 prot;
    struct vm_area *next;
    struct vm_area *prev;
    struct page *pages;
    void *private;
};

/* VMA Flags */
#define VMA_READ            0x00000001
#define VMA_WRITE           0x00000002
#define VMA_EXEC            0x00000004
#define VMA_USER            0x00000008
#define VMA_SHARED          0x00000010
#define VMA_ANONYMOUS       0x00000020
#define VMA_FILE_BACKED     0x00000040
#define VMA_GROWSDOWN       0x00000080
#define VMA_GROWSUP         0x00000100
#define VMA_DONTCOPY        0x00000200
#define VMA_LOCKED          0x00000400
#define VMA_IO              0x00000800

/**
 * address_space - Address space
 */
struct address_space {
    struct vm_area *vmas;
    u64 size;
    u32 flags;
    spinlock_t lock;
    void *private;
};

/*===========================================================================*/
/*                         PHYSICAL MEMORY MANAGER                           */
/*===========================================================================*/

/* Physical Memory Manager Functions */
void pmm_init(void);
void pmm_init_region(phys_addr_t base, size_t size);
void pmm_free_region(phys_addr_t base, size_t size);

phys_addr_t pmm_alloc_page(void);
phys_addr_t pmm_alloc_pages(size_t count);
void pmm_free_page(phys_addr_t page);
void pmm_free_pages(phys_addr_t pages, size_t count);

size_t pmm_get_free_pages(void);
size_t pmm_get_total_pages(void);
size_t pmm_get_used_pages(void);

struct page *pmm_get_page(phys_addr_t addr);
phys_addr_t pmm_get_phys_addr(struct page *page);

/*===========================================================================*/
/*                         VIRTUAL MEMORY MANAGER                            */
/*===========================================================================*/

/* Virtual Memory Manager Functions */
void vmm_init(void);

result_t vmm_map(virt_addr_t vaddr, phys_addr_t paddr, u32 flags);
result_t vmm_unmap(virt_addr_t vaddr);
result_t vmm_map_range(virt_addr_t vaddr, phys_addr_t paddr, size_t size, u32 flags);
result_t vmm_unmap_range(virt_addr_t vaddr, size_t size);

phys_addr_t vmm_virt_to_phys(virt_addr_t vaddr);
virt_addr_t vmm_phys_to_virt(phys_addr_t paddr);

struct address_space *vmm_create_as(void);
void vmm_destroy_as(struct address_space *as);
result_t vmm_switch_as(struct address_space *as);
struct address_space *vmm_get_current_as(void);

/* Page Table Operations */
void vmm_flush_tlb(void);
void vmm_flush_tlb_addr(virt_addr_t vaddr);
void vmm_flush_tlb_as(struct address_space *as);

/*===========================================================================*/
/*                         KERNEL HEAP                                       */
/*===========================================================================*/

/* Kernel Heap Functions */
void heap_init(void);
void heap_early_init(void);

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);

void *kmalloc_aligned(size_t size, size_t align);
void *kzalloc_aligned(size_t size, size_t align);

/* Slab Allocator */
void slab_init(void);

void *kmem_cache_alloc(void *cache);
void kmem_cache_free(void *cache, void *obj);

void *kmem_cache_create(const char *name, size_t size, size_t align, u32 flags);
void kmem_cache_destroy(void *cache);

/*===========================================================================*/
/*                         PAGE ALLOCATOR                                    */
/*===========================================================================*/

/* Page Allocator Functions */
void page_alloc_init(void);

struct page *alloc_page(u32 flags);
struct page *alloc_pages(u32 order, u32 flags);
void free_page(struct page *page);
void free_pages(struct page *page, u32 order);

/* GFP Flags */
#define GFP_NOWAIT          0x00000001
#define GFP_ATOMIC          0x00000002
#define GFP_KERNEL          0x00000004
#define GFP_USER            0x00000008
#define GFP_DMA             0x00000010
#define GFP_DMA32           0x00000020
#define GFP_HIGHMEM         0x00000040
#define GFP_ZERO            0x00000080
#define GFP_NORETRY         0x00000100
#define GFP_NOIO            0x00000200
#define GFP_NOFS            0x00000400

/* Buddy Allocator */
void *buddy_alloc(size_t size);
void buddy_free(void *ptr, size_t size);

/*===========================================================================*/
/*                         MEMORY MAPPING                                    */
/*===========================================================================*/

/* Memory Mapping Functions */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t length, int prot);
int msync(void *addr, size_t length, int flags);

/* mmap flags */
#define MAP_SHARED          0x01
#define MAP_PRIVATE         0x02
#define MAP_FIXED           0x10
#define MAP_ANONYMOUS       0x20
#define MAP_POPULATE        0x80
#define MAP_LOCKED          0x100
#define MAP_NORESERVE       0x200

/* mmap prot */
#define PROT_NONE           0x0
#define PROT_READ           0x1
#define PROT_WRITE          0x2
#define PROT_EXEC           0x4

/*===========================================================================*/
/*                         MEMORY UTILITY FUNCTIONS                          */
/*===========================================================================*/

/* Memory Utilities */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

/* String Functions */
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

/* Memory Information */
void mem_info(void);
size_t mem_get_total(void);
size_t mem_get_free(void);
size_t mem_get_used(void);

#endif /* _NEXUS_MM_H */
