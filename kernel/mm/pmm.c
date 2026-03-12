/*
 * NEXUS OS - Physical Memory Manager
 * kernel/mm/pmm.c
 */

#include "../include/kernel.h"
#include "mm.h"

/*===========================================================================*/
/*                         PHYSICAL MEMORY MANAGER                           */
/*===========================================================================*/

/* Memory Region Tracking */
#define MAX_MEMORY_REGIONS  1024

static struct memory_region memory_regions[MAX_MEMORY_REGIONS];
static u32 num_memory_regions = 0;

/* Bitmap for physical page tracking */
static u64 *pmm_bitmap = NULL;
static size_t bitmap_size = 0;

/* Memory statistics */
static size_t total_pages = 0;
static size_t free_pages = 0;
static size_t reserved_pages = 0;

/* Physical memory boundaries */
static phys_addr_t mem_start = 0;
static phys_addr_t mem_end = 0;

/* Spinlock for PMM */
static spinlock_t pmm_lock = { .lock = 0 };

/*===========================================================================*/
/*                         BITMAP OPERATIONS                                 */
/*===========================================================================*/

/**
 * bitmap_set - Set bit in bitmap
 */
static inline void bitmap_set(u64 *bitmap, size_t bit)
{
    bitmap[bit / 64] |= (1ULL << (bit % 64));
}

/**
 * bitmap_clear - Clear bit in bitmap
 */
static inline void bitmap_clear(u64 *bitmap, size_t bit)
{
    bitmap[bit / 64] &= ~(1ULL << (bit % 64));
}

/**
 * bitmap_test - Test bit in bitmap
 */
static inline bool bitmap_test(u64 *bitmap, size_t bit)
{
    return (bitmap[bit / 64] & (1ULL << (bit % 64))) != 0;
}

/**
 * bitmap_find_free - Find free region in bitmap
 */
static ssize_t bitmap_find_free(u64 *bitmap, size_t nbits, size_t count)
{
    size_t i, j;
    size_t found = 0;
    
    for (i = 0; i < nbits; i++) {
        if (!bitmap_test(bitmap, i)) {
            found++;
            if (found >= count) {
                return i - count + 1;
            }
        } else {
            found = 0;
        }
    }
    
    return -1;
}

/*===========================================================================*/
/*                         MEMORY REGION MANAGEMENT                          */
/*===========================================================================*/

/**
 * pmm_add_region - Add memory region
 */
static int pmm_add_region(phys_addr_t start, phys_addr_t end, u32 type)
{
    if (num_memory_regions >= MAX_MEMORY_REGIONS) {
        return -1;
    }
    
    memory_regions[num_memory_regions].start = start;
    memory_regions[num_memory_regions].end = end;
    memory_regions[num_memory_regions].type = type;
    memory_regions[num_memory_regions].flags = 0;
    num_memory_regions++;
    
    return 0;
}

/**
 * pmm_init_region - Initialize memory region
 */
void pmm_init_region(phys_addr_t base, size_t size)
{
    size_t start_page = (base - mem_start) / PAGE_SIZE;
    size_t num_pages = size / PAGE_SIZE;
    size_t i;
    
    spin_lock(&pmm_lock);
    
    for (i = 0; i < num_pages; i++) {
        bitmap_clear(pmm_bitmap, start_page + i);
    }
    
    free_pages += num_pages;
    
    spin_unlock(&pmm_lock);
}

/**
 * pmm_free_region - Free memory region
 */
void pmm_free_region(phys_addr_t base, size_t size)
{
    size_t start_page = (base - mem_start) / PAGE_SIZE;
    size_t num_pages = size / PAGE_SIZE;
    size_t i;
    
    spin_lock(&pmm_lock);
    
    for (i = 0; i < num_pages; i++) {
        bitmap_clear(pmm_bitmap, start_page + i);
    }
    
    free_pages += num_pages;
    
    spin_unlock(&pmm_lock);
}

/*===========================================================================*/
/*                         PAGE ALLOCATION                                   */
/*===========================================================================*/

/**
 * pmm_alloc_page - Allocate single physical page
 */
phys_addr_t pmm_alloc_page(void)
{
    ssize_t page_idx;
    phys_addr_t addr;
    
    spin_lock(&pmm_lock);
    
    if (free_pages == 0) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    
    page_idx = bitmap_find_free(pmm_bitmap, total_pages, 1);
    if (page_idx < 0) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    
    bitmap_set(pmm_bitmap, page_idx);
    free_pages--;
    
    spin_unlock(&pmm_lock);
    
    addr = mem_start + (page_idx * PAGE_SIZE);
    memset((void *)addr, 0, PAGE_SIZE);
    
    return addr;
}

/**
 * pmm_alloc_pages - Allocate multiple physical pages
 */
phys_addr_t pmm_alloc_pages(size_t count)
{
    ssize_t page_idx;
    phys_addr_t addr;
    
    spin_lock(&pmm_lock);
    
    if (free_pages < count) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    
    page_idx = bitmap_find_free(pmm_bitmap, total_pages, count);
    if (page_idx < 0) {
        spin_unlock(&pmm_lock);
        return 0;
    }
    
    for (size_t i = 0; i < count; i++) {
        bitmap_set(pmm_bitmap, page_idx + i);
    }
    
    free_pages -= count;
    
    spin_unlock(&pmm_lock);
    
    addr = mem_start + (page_idx * PAGE_SIZE);
    memset((void *)addr, 0, count * PAGE_SIZE);
    
    return addr;
}

/**
 * pmm_free_page - Free single physical page
 */
void pmm_free_page(phys_addr_t page)
{
    size_t page_idx;
    
    if (page < mem_start || page >= mem_end) {
        return;
    }
    
    page_idx = (page - mem_start) / PAGE_SIZE;
    
    spin_lock(&pmm_lock);
    
    if (!bitmap_test(pmm_bitmap, page_idx)) {
        /* Page already free */
        spin_unlock(&pmm_lock);
        return;
    }
    
    bitmap_clear(pmm_bitmap, page_idx);
    free_pages++;
    
    spin_unlock(&pmm_lock);
}

/**
 * pmm_free_pages - Free multiple physical pages
 */
void pmm_free_pages(phys_addr_t pages, size_t count)
{
    size_t page_idx;
    size_t i;
    
    if (pages < mem_start || pages >= mem_end) {
        return;
    }
    
    page_idx = (pages - mem_start) / PAGE_SIZE;
    
    spin_lock(&pmm_lock);
    
    for (i = 0; i < count; i++) {
        if (bitmap_test(pmm_bitmap, page_idx + i)) {
            bitmap_clear(pmm_bitmap, page_idx + i);
            free_pages++;
        }
    }
    
    spin_unlock(&pmm_lock);
}

/*===========================================================================*/
/*                         PAGE INFO                                         */
/*===========================================================================*/

/**
 * pmm_get_page - Get page structure for physical address
 */
struct page *pmm_get_page(phys_addr_t addr)
{
    size_t page_idx;
    
    if (addr < mem_start || addr >= mem_end) {
        return NULL;
    }
    
    page_idx = (addr - mem_start) / PAGE_SIZE;
    
    /* In real implementation, return pointer to page array */
    return (struct page *)(KERNEL_VIRTUAL_BASE + addr);
}

/**
 * pmm_get_phys_addr - Get physical address from page
 */
phys_addr_t pmm_get_phys_addr(struct page *page)
{
    if (!page) {
        return 0;
    }
    
    return page->physical;
}

/*===========================================================================*/
/*                         MEMORY STATISTICS                                 */
/*===========================================================================*/

/**
 * pmm_get_free_pages - Get number of free pages
 */
size_t pmm_get_free_pages(void)
{
    size_t count;
    
    spin_lock(&pmm_lock);
    count = free_pages;
    spin_unlock(&pmm_lock);
    
    return count;
}

/**
 * pmm_get_total_pages - Get total number of pages
 */
size_t pmm_get_total_pages(void)
{
    return total_pages;
}

/**
 * pmm_get_used_pages - Get number of used pages
 */
size_t pmm_get_used_pages(void)
{
    size_t count;
    
    spin_lock(&pmm_lock);
    count = total_pages - free_pages;
    spin_unlock(&pmm_lock);
    
    return count;
}

/*===========================================================================*/
/*                         PMM INITIALIZATION                                */
/*===========================================================================*/

/**
 * pmm_early_init - Early PMM initialization
 */
void pmm_early_init(void)
{
    pr_info("Physical Memory Manager: Early initialization\n");
    
    /* Initialize lock */
    spin_lock_init(&pmm_lock);
}

/**
 * pmm_init - Initialize Physical Memory Manager
 */
void pmm_init(void)
{
    size_t bitmap_pages;
    phys_addr_t bitmap_addr;
    
    pr_info("Initializing Physical Memory Manager...\n");
    
    /* Set memory boundaries (from bootloader) */
    mem_start = 0x100000; /* Start after first MB */
    mem_end = 0x100000000ULL; /* 4GB - placeholder */
    
    /* Calculate total pages */
    total_pages = (mem_end - mem_start) / PAGE_SIZE;
    free_pages = total_pages;
    
    /* Calculate bitmap size */
    bitmap_size = (total_pages + 63) / 64;
    bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    pr_info("  Total Memory: %lu MB\n", (total_pages * PAGE_SIZE) / (1024 * 1024));
    pr_info("  Total Pages: %lu\n", total_pages);
    pr_info("  Bitmap Size: %lu bytes (%lu pages)\n", bitmap_size, bitmap_pages);
    
    /* Allocate bitmap */
    bitmap_addr = mem_start;
    pmm_bitmap = (u64 *)KERNEL_VIRTUAL_BASE + bitmap_addr;
    
    /* Clear bitmap (all pages free) */
    memset(pmm_bitmap, 0, bitmap_size);
    
    /* Mark bitmap pages as used */
    for (size_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(pmm_bitmap, i);
    }
    
    free_pages -= bitmap_pages;
    reserved_pages = bitmap_pages;
    
    /* Mark first MB as reserved */
    for (size_t i = 0; i < 256; i++) {
        bitmap_set(pmm_bitmap, i);
    }
    
    free_pages -= 256;
    reserved_pages += 256;
    
    pr_info("  Reserved Pages: %lu\n", reserved_pages);
    pr_info("  Free Pages: %lu\n", free_pages);
    
    pr_info("Physical Memory Manager initialized.\n");
}

/*===========================================================================*/
/*                         MEMORY REGION INFORMATION                         */
/*===========================================================================*/

/**
 * pmm_get_region_count - Get number of memory regions
 */
u32 pmm_get_region_count(void)
{
    return num_memory_regions;
}

/**
 * pmm_get_region - Get memory region info
 */
struct memory_region *pmm_get_region(u32 index)
{
    if (index >= num_memory_regions) {
        return NULL;
    }
    return &memory_regions[index];
}

/**
 * mem_info - Print memory information
 */
void mem_info(void)
{
    printk("\n=== Memory Information ===\n");
    printk("Total Memory: %lu MB\n", (total_pages * PAGE_SIZE) / (1024 * 1024));
    printk("Used Memory: %lu MB\n", ((total_pages - free_pages) * PAGE_SIZE) / (1024 * 1024));
    printk("Free Memory: %lu MB\n", (free_pages * PAGE_SIZE) / (1024 * 1024));
    printk("Reserved Memory: %lu MB\n", (reserved_pages * PAGE_SIZE) / (1024 * 1024));
    printk("\n");
}
