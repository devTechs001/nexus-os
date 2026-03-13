/*
 * NEXUS OS - Page Frame Allocator (Buddy System)
 * kernel/mm/page_alloc.c
 */

#include "../include/kernel.h"
#include "mm.h"

/*===========================================================================*/
/*                         BUDDY SYSTEM ALLOCATOR                            */
/*===========================================================================*/

/* Buddy System Configuration */
#define MAX_ORDER           11              /* 2^11 * 4KB = 8MB max allocation */
#define MIN_ORDER           0               /* 4KB minimum allocation */
#define NR_ORDERS           (MAX_ORDER + 1)

/* Zone Configuration */
#define MAX_ZONES           4
#define ZONE_DMA            0
#define ZONE_DMA32          1
#define ZONE_NORMAL         2
#define ZONE_HIGHMEM        3

/* Page Flags */
#define PAGE_FLAG_FREE      0x00000001
#define PAGE_FLAG_RESERVED  0x00000002
#define PAGE_FLAG_COMPOUND  0x00000004
#define PAGE_FLAG_HEAD      0x00000008
#define PAGE_FLAG_TAIL      0x00000010
#define PAGE_FLAG_BUDDY     0x00000020

/* Free Area */
struct free_area {
    struct list_head free_list;
    size_t nr_free;
};

/* Zone Structure */
struct zone {
    struct free_area free_area[NR_ORDERS];
    struct page *pages;             /* Array of page structures */
    size_t nr_pages;                /* Total pages in zone */
    size_t nr_free;                 /* Free pages in zone */
    size_t nr_reserved;             /* Reserved pages */
    phys_addr_t start;              /* Physical start address */
    phys_addr_t end;                /* Physical end address */
    u32 zone_id;                    /* Zone identifier */
    spinlock_t lock;                /* Zone lock */
    char name[16];                  /* Zone name */
};

/* Node Structure (for NUMA support) */
struct node {
    struct zone zones[MAX_ZONES];
    u32 node_id;
    u32 nr_zones;
    spinlock_t lock;
};

/* Global Variables */
static struct node nodes[MAX_CPUS];
static u32 num_nodes = 1;

/* Default zone for allocations */
static struct zone *default_zone = NULL;

/* Total system memory tracking */
static size_t total_pages = 0;
static size_t num_free_pages = 0;
static size_t reserved_pages = 0;

/* Spinlock for global operations */
static spinlock_t page_alloc_lock = { .lock = 0 };

/*===========================================================================*/
/*                         PAGE OPERATIONS                                   */
/*===========================================================================*/

/**
 * page_init - Initialize page structure
 */
static inline void page_init(struct page *page, phys_addr_t paddr)
{
    if (!page) {
        return;
    }

    page->flags = 0;
    page->next = NULL;
    page->prev = NULL;
    page->first = NULL;
    atomic_set(&page->refcount, 0);
    atomic_set(&page->mapcount, 0);
    page->index = 0;
    page->virtual = (void *)vmm_phys_to_virt(paddr);
    page->physical = paddr;
}

/**
 * page_set_flag - Set page flag
 */
static inline void page_set_flag(struct page *page, u32 flag)
{
    if (page) {
        page->flags |= flag;
    }
}

/**
 * page_clear_flag - Clear page flag
 */
static inline void page_clear_flag(struct page *page, u32 flag)
{
    if (page) {
        page->flags &= ~flag;
    }
}

/**
 * page_test_flag - Test page flag
 */
static inline bool page_test_flag(struct page *page, u32 flag)
{
    return page && (page->flags & flag);
}

/**
 * page_count - Get page reference count
 */
static inline int page_count(struct page *page)
{
    return page ? atomic_read(&page->refcount) : 0;
}

/**
 * page_refcount_inc - Increment page reference count
 */
static inline void page_refcount_inc(struct page *page)
{
    if (page) {
        atomic_inc(&page->refcount);
    }
}

/**
 * page_refcount_dec - Decrement page reference count
 */
static inline int page_refcount_dec(struct page *page)
{
    if (page) {
        return atomic_add_return(-1, &page->refcount);
    }
    return 0;
}

/**
 * page_to_pfn - Convert page to page frame number
 */
static inline size_t page_to_pfn(struct page *page)
{
    if (!page) {
        return 0;
    }
    return (page->physical - default_zone->start) / PAGE_SIZE;
}

/**
 * pfn_to_page - Convert page frame number to page
 */
static inline struct page *pfn_to_page(size_t pfn)
{
    if (!default_zone || !default_zone->pages) {
        return NULL;
    }
    return &default_zone->pages[pfn];
}

/*===========================================================================*/
/*                         FREE LIST OPERATIONS                              */
/*===========================================================================*/

/**
 * free_list_init - Initialize free list
 */
static inline void free_list_init(struct free_area *area)
{
    INIT_LIST_HEAD(&area->free_list);
    area->nr_free = 0;
}

/**
 * free_list_add - Add page to free list
 */
static void free_list_add(struct free_area *area, struct page *page)
{
    list_add(&page->lru, &area->free_list);
    area->nr_free++;
    page_set_flag(page, PAGE_FLAG_FREE);
}

/**
 * free_list_del - Remove page from free list
 */
static void free_list_del(struct free_area *area, struct page *page)
{
    list_del(&page->lru);
    area->nr_free--;
    page_clear_flag(page, PAGE_FLAG_FREE);
}

/**
 * free_list_empty - Check if free list is empty
 */
static inline bool free_list_empty(struct free_area *area)
{
    return list_empty(&area->free_list);
}

/**
 * free_list_first - Get first page from free list
 */
static inline struct page *free_list_first(struct free_area *area)
{
    if (free_list_empty(area)) {
        return NULL;
    }
    return list_entry(area->free_list.next, struct page, lru);
}

/*===========================================================================*/
/*                         BUDDY OPERATIONS                                  */
/*===========================================================================*/

/**
 * get_buddy - Get buddy page
 */
static struct page *get_buddy(struct page *page, u32 order)
{
    size_t pfn = page_to_pfn(page);
    size_t buddy_pfn;

    /* XOR with bit at position 'order' to get buddy */
    buddy_pfn = pfn ^ (1UL << order);

    /* Check if buddy is within zone */
    if (buddy_pfn >= default_zone->nr_pages) {
        return NULL;
    }

    return pfn_to_page(buddy_pfn);
}

/**
 * expand - Expand larger block into smaller blocks
 */
static void expand(struct zone *zone, struct page *page, u32 order, u32 target_order)
{
    u32 size;

    while (order > target_order) {
        struct page *buddy;

        order--;
        size = 1 << order;

        buddy = page + size;

        /* Initialize buddy page */
        page_set_flag(buddy, PAGE_FLAG_BUDDY);
        free_list_add(&zone->free_area[order], buddy);

        page = buddy;
    }
}

/**
 * merge_buddies - Merge page with buddy if possible
 */
static struct page *merge_buddies(struct zone *zone, struct page *page, u32 order)
{
    struct page *buddy;

    while (order < MAX_ORDER) {
        buddy = get_buddy(page, order);

        /* Check if buddy exists and is free */
        if (!buddy || !page_test_flag(buddy, PAGE_FLAG_FREE)) {
            break;
        }

        /* Check if buddy is of same order */
        if (!page_test_flag(buddy, PAGE_FLAG_BUDDY)) {
            break;
        }

        /* Remove buddy from free list */
        free_list_del(&zone->free_area[order], buddy);
        page_clear_flag(buddy, PAGE_FLAG_BUDDY);

        /* Move to next order */
        order++;

        /* Use lower address as base */
        if (buddy < page) {
            page = buddy;
        }
    }

    return page;
}

/*===========================================================================*/
/*                         ZONE OPERATIONS                                   */
/*===========================================================================*/

/**
 * zone_init - Initialize memory zone
 */
static void zone_init(struct zone *zone, phys_addr_t start, size_t nr_pages, u32 zone_id)
{
    u32 i;

    zone->start = start;
    zone->end = start + (nr_pages * PAGE_SIZE);
    zone->nr_pages = nr_pages;
    zone->nr_free = 0;
    zone->nr_reserved = 0;
    zone->zone_id = zone_id;
    zone->pages = NULL;

    spin_lock_init(&zone->lock);

    /* Initialize free lists for all orders */
    for (i = 0; i < NR_ORDERS; i++) {
        free_list_init(&zone->free_area[i]);
    }

    /* Set zone name */
    switch (zone_id) {
        case ZONE_DMA:
            strncpy(zone->name, "DMA", sizeof(zone->name));
            break;
        case ZONE_DMA32:
            strncpy(zone->name, "DMA32", sizeof(zone->name));
            break;
        case ZONE_NORMAL:
            strncpy(zone->name, "Normal", sizeof(zone->name));
            break;
        case ZONE_HIGHMEM:
            strncpy(zone->name, "HighMem", sizeof(zone->name));
            break;
        default:
            strncpy(zone->name, "Unknown", sizeof(zone->name));
            break;
    }
}

/**
 * zone_add_free_pages - Add free pages to zone
 */
static void zone_add_free_pages(struct zone *zone, phys_addr_t start, size_t count)
{
    size_t i;
    size_t start_pfn = (start - zone->start) / PAGE_SIZE;

    spin_lock(&zone->lock);

    for (i = 0; i < count; i++) {
        struct page *page = &zone->pages[start_pfn + i];

        page_init(page, start + (i * PAGE_SIZE));
        page_set_flag(page, PAGE_FLAG_FREE);

        /* Add to order-0 free list */
        free_list_add(&zone->free_area[0], page);
    }

    zone->nr_free += count;

    spin_unlock(&zone->lock);
}

/**
 * zone_reserve_pages - Reserve pages in zone
 */
static void zone_reserve_pages(struct zone *zone, phys_addr_t start, size_t count)
{
    size_t i;
    size_t start_pfn = (start - zone->start) / PAGE_SIZE;

    spin_lock(&zone->lock);

    for (i = 0; i < count; i++) {
        struct page *page = &zone->pages[start_pfn + i];

        page_set_flag(page, PAGE_FLAG_RESERVED);
        page_clear_flag(page, PAGE_FLAG_FREE);
    }

    zone->nr_reserved += count;

    spin_unlock(&zone->lock);
}

/**
 * find_zone - Find appropriate zone for allocation
 */
static struct zone *find_zone(u32 flags)
{
    struct zone *zone;
    u32 i;

    /* Check for specific zone requests */
    if (flags & GFP_DMA) {
        for (i = 0; i < num_nodes; i++) {
            zone = &nodes[i].zones[ZONE_DMA];
            if (zone->nr_free > 0) {
                return zone;
            }
        }
    }

    if (flags & GFP_DMA32) {
        for (i = 0; i < num_nodes; i++) {
            zone = &nodes[i].zones[ZONE_DMA32];
            if (zone->nr_free > 0) {
                return zone;
            }
        }
    }

    /* Default to normal zone */
    for (i = 0; i < num_nodes; i++) {
        zone = &nodes[i].zones[ZONE_NORMAL];
        if (zone->nr_free > 0) {
            return zone;
        }
    }

    /* Fall back to any zone with free pages */
    for (i = 0; i < num_nodes; i++) {
        u32 j;
        for (j = 0; j < MAX_ZONES; j++) {
            zone = &nodes[i].zones[j];
            if (zone->nr_free > 0) {
                return zone;
            }
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         PAGE ALLOCATION                                   */
/*===========================================================================*/

/**
 * __alloc_pages - Internal page allocation
 */
static struct page *__alloc_pages(struct zone *zone, u32 order, u32 flags)
{
    struct free_area *area;
    struct page *page;
    u32 current_order;

    if (!zone) {
        return NULL;
    }

    spin_lock(&zone->lock);

    /* Search from requested order upward */
    for (current_order = order; current_order < NR_ORDERS; current_order++) {
        area = &zone->free_area[current_order];

        if (free_list_empty(area)) {
            continue;
        }

        /* Get first free page */
        page = free_list_first(area);
        if (!page) {
            continue;
        }

        /* Remove from free list */
        free_list_del(area, page);
        page_clear_flag(page, PAGE_FLAG_BUDDY);

        /* Expand if we found a larger block */
        if (current_order > order) {
            expand(zone, page, current_order, order);
        }

        /* Mark page as allocated */
        page_clear_flag(page, PAGE_FLAG_FREE);
        page_refcount_inc(page);

        /* Zero page if requested */
        if (flags & GFP_ZERO) {
            memset(page->virtual, 0, PAGE_SIZE << order);
        }

        zone->nr_free -= (1 << order);

        spin_unlock(&zone->lock);

        return page;
    }

    spin_unlock(&zone->lock);

    return NULL;
}

/**
 * alloc_pages - Allocate contiguous pages
 */
struct page *alloc_pages(u32 order, u32 flags)
{
    struct zone *zone;
    struct page *page;

    if (order > MAX_ORDER) {
        pr_err("PageAlloc: Order %u exceeds maximum %d\n", order, MAX_ORDER);
        return NULL;
    }

    /* Find suitable zone */
    zone = find_zone(flags);
    if (!zone) {
        return NULL;
    }

    page = __alloc_pages(zone, order, flags);

    /* Handle compound pages for order > 0 */
    if (page && order > 0) {
        u32 i;

        page_set_flag(page, PAGE_FLAG_HEAD);
        page_set_flag(page, PAGE_FLAG_COMPOUND);
        page->first = page;

        /* Initialize tail pages */
        for (i = 1; i < (1 << order); i++) {
            struct page *tail = page + i;
            page_set_flag(tail, PAGE_FLAG_TAIL);
            page_set_flag(tail, PAGE_FLAG_COMPOUND);
            tail->first = page;
            atomic_set(&tail->refcount, 0);
        }
    }

    return page;
}

/**
 * alloc_page - Allocate single page
 */
struct page *alloc_page(u32 flags)
{
    return alloc_pages(0, flags);
}

/**
 * alloc_page_vma - Allocate page for VMA
 */
struct page *alloc_page_vma(u32 flags, struct vm_area *vma)
{
    /* For now, same as alloc_page */
    /* In real implementation, consider NUMA locality */
    return alloc_page(flags);
}

/*===========================================================================*/
/*                         PAGE FREE                                         */
/*===========================================================================*/

/**
 * __free_pages - Internal page free
 */
static void __free_pages(struct zone *zone, struct page *page, u32 order)
{
    struct page *buddy;

    if (!zone || !page) {
        return;
    }

    spin_lock(&zone->lock);

    /* Merge with buddies */
    page = merge_buddies(zone, page, order);

    /* Add to appropriate free list */
    free_list_add(&zone->free_area[order], page);

    zone->nr_free += (1 << order);

    spin_unlock(&zone->lock);
}

/**
 * free_pages - Free contiguous pages
 */
void free_pages(struct page *page, u32 order)
{
    struct zone *zone;
    u32 i;

    if (!page) {
        return;
    }

    /* Find zone containing page */
    zone = default_zone;

    /* Clear compound page flags */
    if (order > 0) {
        page_clear_flag(page, PAGE_FLAG_HEAD);
        page_clear_flag(page, PAGE_FLAG_COMPOUND);

        for (i = 1; i < (1 << order); i++) {
            struct page *tail = page + i;
            page_clear_flag(tail, PAGE_FLAG_TAIL);
            page_clear_flag(tail, PAGE_FLAG_COMPOUND);
        }
    }

    /* Clear reference count */
    atomic_set(&page->refcount, 0);

    __free_pages(zone, page, order);
}

/**
 * free_page - Free single page
 */
void free_page(struct page *page)
{
    free_pages(page, 0);
}

/*===========================================================================*/
/*                         GFP FLAG HANDLING                                 */
/*===========================================================================*/

/**
 * gfp_to_zone - Convert GFP flags to zone
 */
static u32 gfp_to_zone(u32 flags)
{
    if (flags & GFP_DMA) {
        return ZONE_DMA;
    }
    if (flags & GFP_DMA32) {
        return ZONE_DMA32;
    }
    if (flags & GFP_HIGHMEM) {
        return ZONE_HIGHMEM;
    }
    return ZONE_NORMAL;
}

/**
 * gfp_check - Validate GFP flags
 */
static bool gfp_check(u32 flags)
{
    /* Check for invalid flag combinations */
    if ((flags & GFP_DMA) && (flags & GFP_HIGHMEM)) {
        return false;
    }

    return true;
}

/*===========================================================================*/
/*                         PAGE ALLOCATOR INIT                               */
/*===========================================================================*/

/**
 * page_alloc_init - Initialize page allocator
 */
void page_alloc_init(void)
{
    phys_addr_t mem_start;
    phys_addr_t mem_end;
    size_t total_mem;
    size_t pages_per_zone;
    u32 i;

    pr_info("Initializing Page Frame Allocator (Buddy System)...\n");

    spin_lock_init(&page_alloc_lock);

    /* Initialize nodes */
    for (i = 0; i < MAX_CPUS; i++) {
        nodes[i].node_id = i;
        nodes[i].nr_zones = MAX_ZONES;
        spin_lock_init(&nodes[i].lock);
    }

    num_nodes = 1; /* Single node for now */

    /* Set memory boundaries */
    mem_start = 0x100000; /* 1MB */
    mem_end = 0x100000000ULL; /* 4GB - placeholder */
    total_mem = mem_end - mem_start;
    total_pages = total_mem / PAGE_SIZE;

    pr_info("  Total Memory: %lu MB\n", total_mem / MB(1));
    pr_info("  Total Pages: %lu\n", total_pages);

    /* Initialize zones */
    /* Divide memory into zones */
    pages_per_zone = total_pages / 3; /* DMA, DMA32, Normal */

    /* ZONE_DMA: First 16MB */
    zone_init(&nodes[0].zones[ZONE_DMA], mem_start,
              (16 * MB(1)) / PAGE_SIZE, ZONE_DMA);

    /* ZONE_DMA32: Next ~3.9GB */
    zone_init(&nodes[0].zones[ZONE_DMA32],
              mem_start + (16 * MB(1)),
              pages_per_zone, ZONE_DMA32);

    /* ZONE_NORMAL: Remaining */
    zone_init(&nodes[0].zones[ZONE_NORMAL],
              mem_start + (16 * MB(1)) + (pages_per_zone * PAGE_SIZE),
              total_pages - pages_per_zone - ((16 * MB(1)) / PAGE_SIZE),
              ZONE_NORMAL);

    /* Allocate page structures */
    /* In real implementation, allocate from reserved memory */
    default_zone = &nodes[0].zones[ZONE_NORMAL];
    default_zone->pages = (struct page *)kmalloc(default_zone->nr_pages * sizeof(struct page));

    if (!default_zone->pages) {
        pr_err("PageAlloc: Failed to allocate page structures\n");
        return;
    }

    memset(default_zone->pages, 0, default_zone->nr_pages * sizeof(struct page));

    /* Initialize all pages */
    for (i = 0; i < default_zone->nr_pages; i++) {
        phys_addr_t paddr = default_zone->start + (i * PAGE_SIZE);
        page_init(&default_zone->pages[i], paddr);
    }

    /* Add all pages as free */
    zone_add_free_pages(default_zone, default_zone->start, default_zone->nr_pages);

    /* Reserve kernel memory */
    zone_reserve_pages(default_zone, mem_start, 256); /* First 1MB */

    reserved_pages = default_zone->nr_reserved;

    pr_info("  Zones initialized:\n");
    pr_info("    DMA: %lu pages\n", nodes[0].zones[ZONE_DMA].nr_pages);
    pr_info("    DMA32: %lu pages\n", nodes[0].zones[ZONE_DMA32].nr_pages);
    pr_info("    Normal: %lu pages\n", nodes[0].zones[ZONE_NORMAL].nr_pages);
    pr_info("  Free Pages: %lu\n", default_zone->nr_free);
    pr_info("  Reserved Pages: %lu\n", reserved_pages);

    pr_info("Page Frame Allocator initialized.\n");
}

/*===========================================================================*/
/*                         BUDDY ALLOCATOR (Legacy)                          */
/*===========================================================================*/

/**
 * buddy_alloc - Allocate memory using buddy system
 */
void *buddy_alloc(size_t size)
{
    u32 order;
    struct page *page;
    u32 flags = GFP_KERNEL | GFP_ZERO;

    /* Calculate order needed */
    order = 0;
    while ((PAGE_SIZE << order) < size) {
        order++;
    }

    if (order > MAX_ORDER) {
        return NULL;
    }

    page = alloc_pages(order, flags);
    if (!page) {
        return NULL;
    }

    return page->virtual;
}

/**
 * buddy_free - Free memory allocated by buddy_alloc
 */
void buddy_free(void *ptr, size_t size)
{
    struct page *page;
    phys_addr_t paddr;
    u32 order;

    if (!ptr) {
        return;
    }

    /* Calculate order */
    order = 0;
    while ((PAGE_SIZE << order) < size) {
        order++;
    }

    /* Get physical address */
    paddr = vmm_virt_to_phys((virt_addr_t)ptr);

    /* Get page structure */
    page = pmm_get_page(paddr);
    if (!page) {
        return;
    }

    free_pages(page, order);
}

/*===========================================================================*/
/*                         PAGE STATISTICS                                   */
/*===========================================================================*/

/**
 * get_free_pages - Get number of free pages
 */
size_t get_free_pages(void)
{
    size_t count = 0;
    u32 i;

    spin_lock(&page_alloc_lock);

    for (i = 0; i < num_nodes; i++) {
        u32 j;
        for (j = 0; j < MAX_ZONES; j++) {
            count += nodes[i].zones[j].nr_free;
        }
    }

    spin_unlock(&page_alloc_lock);

    return count;
}

/**
 * get_total_pages - Get total number of pages
 */
size_t get_total_pages(void)
{
    return total_pages;
}

/**
 * get_used_pages - Get number of used pages
 */
size_t get_used_pages(void)
{
    return total_pages - get_free_pages() - reserved_pages;
}

/**
 * page_alloc_info - Print page allocator statistics
 */
void page_alloc_info(void)
{
    u32 i, j;
    struct zone *zone;

    printk("\n=== Page Frame Allocator Information ===\n");
    printk("Total Pages: %lu\n", total_pages);
    printk("Free Pages: %lu\n", get_free_pages());
    printk("Used Pages: %lu\n", get_used_pages());
    printk("Reserved Pages: %lu\n", reserved_pages);
    printk("\n");

    for (i = 0; i < num_nodes; i++) {
        printk("Node %u:\n", i);

        for (j = 0; j < MAX_ZONES; j++) {
            u32 k;
            zone = &nodes[i].zones[j];

            if (zone->nr_pages == 0) {
                continue;
            }

            printk("  Zone %s:\n", zone->name);
            printk("    Total: %lu pages\n", zone->nr_pages);
            printk("    Free: %lu pages\n", zone->nr_free);
            printk("    Reserved: %lu pages\n", zone->nr_reserved);

            printk("    Free by Order:\n");
            for (k = 0; k < NR_ORDERS; k++) {
                if (zone->free_area[k].nr_free > 0) {
                    printk("      Order %2u: %lu blocks (%lu KB)\n",
                           k, zone->free_area[k].nr_free,
                           (zone->free_area[k].nr_free * (PAGE_SIZE << k)) / KB(1));
                }
            }
            printk("\n");
        }
    }
}

/**
 * page_alloc_dump - Dump page allocator state
 */
void page_alloc_dump(void)
{
    u32 i;
    struct zone *zone = default_zone;

    if (!zone) {
        return;
    }

    printk("\n=== Page Allocator Dump ===\n");

    spin_lock(&zone->lock);

    for (i = 0; i < NR_ORDERS; i++) {
        struct free_area *area = &zone->free_area[i];
        struct page *page;

        if (free_list_empty(area)) {
            continue;
        }

        printk("Order %u (%lu KB blocks): %lu free\n",
               i, (PAGE_SIZE << i) / KB(1), area->nr_free);

        list_for_each_entry(page, &area->free_list, lru) {
            printk("  Page at 0x%lx (PFN: %lu)\n",
                   page->physical, page_to_pfn(page));
        }
    }

    spin_unlock(&zone->lock);

    printk("\n");
}
