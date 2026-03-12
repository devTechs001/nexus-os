/*
 * NEXUS OS - Kernel Heap Allocator
 * kernel/mm/heap.c
 */

#include "../include/kernel.h"
#include "mm.h"

/*===========================================================================*/
/*                         KERNEL HEAP ALLOCATOR                             */
/*===========================================================================*/

/* Heap Configuration */
#define HEAP_MIN_SIZE       (4 * KB(1))       /* Minimum allocation size */
#define HEAP_ALIGN          16                 /* Alignment for allocations */
#define HEAP_MAGIC          0xDEADBEEF         /* Magic number for debugging */

/* Heap Block Header */
struct heap_block {
    u32 magic;              /* Magic number for validation */
    u32 flags;              /* Block flags */
    size_t size;            /* Size of user data */
    size_t total_size;      /* Total size including header */
    struct heap_block *next;/* Next block in free list */
    struct heap_block *prev;/* Previous block in free list */
    struct heap_block *next_free; /* Next free block */
    struct heap_block *prev_free; /* Previous free block */
};

#define HEAP_BLOCK_HEADER_SIZE  sizeof(struct heap_block)
#define HEAP_MIN_BLOCK_SIZE     (HEAP_MIN_SIZE - HEAP_BLOCK_HEADER_SIZE)

/* Block Flags */
#define HEAP_BLOCK_FREE       0x00000001
#define HEAP_BLOCK_USED       0x00000002
#define HEAP_BLOCK_COALESCED  0x00000004

/* Heap Arena */
struct heap_arena {
    virt_addr_t start;        /* Start of heap */
    virt_addr_t end;          /* End of heap */
    virt_addr_t brk;          /* Current break */
    size_t size;              /* Total heap size */
    size_t used;              /* Used bytes */
    size_t free;              /* Free bytes */
    struct heap_block *free_list; /* Free list head */
    spinlock_t lock;          /* Arena lock */
};

/* Global Heap Arena */
static struct heap_arena kernel_arena;

/* Early Heap State */
static bool heap_initialized = false;
static bool heap_early_mode = true;

/* Early Heap Buffer (used before full initialization) */
#define EARLY_HEAP_SIZE     (256 * KB(1))
static u8 early_heap_buffer[EARLY_HEAP_SIZE] __aligned(PAGE_SIZE);
static size_t early_heap_offset = 0;
static spinlock_t early_heap_lock = { .lock = 0 };

/*===========================================================================*/
/*                         BLOCK OPERATIONS                                  */
/*===========================================================================*/

/**
 * block_init - Initialize heap block
 */
static inline void block_init(struct heap_block *block, size_t size, bool free)
{
    block->magic = HEAP_MAGIC;
    block->flags = free ? HEAP_BLOCK_FREE : HEAP_BLOCK_USED;
    block->size = size;
    block->total_size = size + HEAP_BLOCK_HEADER_SIZE;
    block->next = NULL;
    block->prev = NULL;
    block->next_free = NULL;
    block->prev_free = NULL;
}

/**
 * block_get_data - Get data pointer from block
 */
static inline void *block_get_data(struct heap_block *block)
{
    return (void *)((u8 *)block + HEAP_BLOCK_HEADER_SIZE);
}

/**
 * block_get_header - Get block header from data pointer
 */
static inline struct heap_block *block_get_header(void *ptr)
{
    return (struct heap_block *)((u8 *)ptr - HEAP_BLOCK_HEADER_SIZE);
}

/**
 * block_get_next - Get next adjacent block
 */
static inline struct heap_block *block_get_next(struct heap_arena *arena,
                                                 struct heap_block *block)
{
    u8 *next_addr = (u8 *)block + block->total_size;

    if (next_addr >= arena->end) {
        return NULL;
    }

    return (struct heap_block *)next_addr;
}

/**
 * block_is_valid - Validate block
 */
static inline bool block_is_valid(struct heap_block *block)
{
    return block->magic == HEAP_MAGIC;
}

/**
 * block_is_free - Check if block is free
 */
static inline bool block_is_free(struct heap_block *block)
{
    return (block->flags & HEAP_BLOCK_FREE) != 0;
}

/**
 * block_is_used - Check if block is used
 */
static inline bool block_is_used(struct heap_block *block)
{
    return (block->flags & HEAP_BLOCK_USED) != 0;
}

/*===========================================================================*/
/*                         FREE LIST OPERATIONS                              */
/*===========================================================================*/

/**
 * free_list_insert - Insert block into free list
 */
static void free_list_insert(struct heap_arena *arena, struct heap_block *block)
{
    if (!arena->free_list) {
        arena->free_list = block;
        block->next_free = NULL;
        block->prev_free = NULL;
    } else {
        /* Insert at head for O(1) */
        block->next_free = arena->free_list;
        block->prev_free = NULL;
        arena->free_list->prev_free = block;
        arena->free_list = block;
    }
}

/**
 * free_list_remove - Remove block from free list
 */
static void free_list_remove(struct heap_arena *arena, struct heap_block *block)
{
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        arena->free_list = block->next_free;
    }

    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }

    block->next_free = NULL;
    block->prev_free = NULL;
}

/**
 * free_list_find - Find suitable free block
 */
static struct heap_block *free_list_find(struct heap_arena *arena, size_t size)
{
    struct heap_block *block;

    /* First-fit search */
    block = arena->free_list;
    while (block) {
        if (block->size >= size) {
            return block;
        }
        block = block->next_free;
    }

    return NULL;
}

/*===========================================================================*/
/*                         COALESCING                                        */
/*===========================================================================*/

/**
 * block_coalesce - Coalesce adjacent free blocks
 */
static void block_coalesce(struct heap_arena *arena, struct heap_block *block)
{
    struct heap_block *next;

    if (!block || !block_is_free(block)) {
        return;
    }

    /* Try to coalesce with next block */
    next = block_get_next(arena, block);
    if (next && block_is_free(next)) {
        /* Remove next from free list */
        free_list_remove(arena, next);

        /* Merge blocks */
        block->total_size += next->total_size;
        block->size = block->total_size - HEAP_BLOCK_HEADER_SIZE;
        block->flags |= HEAP_BLOCK_COALESCED;

        /* Clear merged block's magic */
        next->magic = 0;
    }
}

/*===========================================================================*/
/*                         ARENA OPERATIONS                                  */
/*===========================================================================*/

/**
 * arena_grow - Grow heap arena
 */
static int arena_grow(struct heap_arena *arena, size_t size)
{
    virt_addr_t new_brk;
    struct heap_block *new_block;
    size_t grow_size;

    /* Align growth to page size */
    grow_size = ALIGN(size, PAGE_SIZE);

    new_brk = arena->brk + grow_size;

    /* Check if we're within bounds */
    if (new_brk > arena->end) {
        /* Need to allocate more pages */
        size_t pages_needed = (grow_size + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t paddr;
        virt_addr_t vaddr;

        for (size_t i = 0; i < pages_needed; i++) {
            paddr = pmm_alloc_page();
            if (paddr == 0) {
                return -1;
            }
            vaddr = arena->brk + (i * PAGE_SIZE);
            vmm_map(vaddr, paddr, VMA_READ | VMA_WRITE);
        }
    }

    /* Create new free block */
    new_block = (struct heap_block *)arena->brk;
    block_init(new_block, grow_size - HEAP_BLOCK_HEADER_SIZE, true);

    /* Add to free list */
    free_list_insert(arena, new_block);

    arena->brk = new_brk;
    arena->free += grow_size;
    arena->size += grow_size;

    return 0;
}

/**
 * arena_init - Initialize heap arena
 */
static void arena_init(struct heap_arena *arena, virt_addr_t start, size_t size)
{
    struct heap_block *initial_block;

    arena->start = start;
    arena->end = start + size;
    arena->brk = start;
    arena->size = 0;
    arena->used = 0;
    arena->free = 0;
    arena->free_list = NULL;

    spin_lock_init(&arena->lock);

    /* Create initial free block */
    initial_block = (struct heap_block *)start;
    block_init(initial_block, size - HEAP_BLOCK_HEADER_SIZE, true);
    free_list_insert(arena, initial_block);

    arena->brk = start + size;
    arena->size = size;
    arena->free = size - HEAP_BLOCK_HEADER_SIZE;
}

/*===========================================================================*/
/*                         EARLY HEAP ALLOCATION                             */
/*===========================================================================*/

/**
 * heap_early_init - Initialize early heap
 */
void heap_early_init(void)
{
    pr_info("Heap: Early initialization\n");

    memset(early_heap_buffer, 0, EARLY_HEAP_SIZE);
    early_heap_offset = 0;
    spin_lock_init(&early_heap_lock);

    heap_early_mode = true;
    heap_initialized = false;
}

/**
 * early_alloc - Allocate from early heap
 */
static void *early_alloc(size_t size)
{
    void *ptr;

    spin_lock(&early_heap_lock);

    /* Align size */
    size = ALIGN(size, HEAP_ALIGN);

    if (early_heap_offset + size > EARLY_HEAP_SIZE) {
        spin_unlock(&early_heap_lock);
        return NULL;
    }

    ptr = &early_heap_buffer[early_heap_offset];
    early_heap_offset += size;

    spin_unlock(&early_heap_lock);

    return ptr;
}

/*===========================================================================*/
/*                         KERNEL HEAP ALLOCATION                            */
/*===========================================================================*/

/**
 * kmalloc - Allocate memory from kernel heap
 */
void *kmalloc(size_t size)
{
    struct heap_block *block;
    struct heap_block *remainder;
    void *data;
    size_t aligned_size;

    if (size == 0) {
        return NULL;
    }

    /* Use early heap if not initialized */
    if (heap_early_mode) {
        return early_alloc(size);
    }

    /* Align size */
    aligned_size = ALIGN(size, HEAP_ALIGN);
    if (aligned_size < HEAP_MIN_BLOCK_SIZE) {
        aligned_size = HEAP_MIN_BLOCK_SIZE;
    }

    spin_lock(&kernel_arena.lock);

    /* Find suitable free block */
    block = free_list_find(&kernel_arena, aligned_size);

    /* If no suitable block, grow heap */
    if (!block) {
        if (arena_grow(&kernel_arena, aligned_size + PAGE_SIZE) != 0) {
            spin_unlock(&kernel_arena.lock);
            return NULL;
        }
        block = free_list_find(&kernel_arena, aligned_size);
    }

    if (!block) {
        spin_unlock(&kernel_arena.lock);
        return NULL;
    }

    /* Remove from free list */
    free_list_remove(&kernel_arena, block);

    /* Split block if remainder is large enough */
    if (block->size >= aligned_size + HEAP_MIN_BLOCK_SIZE + HEAP_BLOCK_HEADER_SIZE) {
        remainder = (struct heap_block *)((u8 *)block + HEAP_BLOCK_HEADER_SIZE + aligned_size);
        block_init(remainder, block->size - aligned_size - HEAP_BLOCK_HEADER_SIZE, true);

        block->size = aligned_size;
        block->total_size = aligned_size + HEAP_BLOCK_HEADER_SIZE;

        /* Add remainder to free list */
        free_list_insert(&kernel_arena, remainder);
    }

    /* Mark block as used */
    block->flags = HEAP_BLOCK_USED;
    block->flags &= ~HEAP_BLOCK_FREE;

    kernel_arena.used += block->size;
    kernel_arena.free -= block->size;

    data = block_get_data(block);

    spin_unlock(&kernel_arena.lock);

    return data;
}

/**
 * kzalloc - Allocate zeroed memory from kernel heap
 */
void *kzalloc(size_t size)
{
    void *ptr;

    ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}

/**
 * krealloc - Reallocate memory
 */
void *krealloc(void *ptr, size_t new_size)
{
    struct heap_block *block;
    struct heap_block *next;
    void *new_ptr;
    size_t copy_size;

    if (!ptr) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    block = block_get_header(ptr);

    if (!block_is_valid(block) || !block_is_used(block)) {
        return NULL;
    }

    /* If new size fits, just return */
    if (block->size >= new_size) {
        return ptr;
    }

    /* Try to expand by coalescing with next block */
    spin_lock(&kernel_arena.lock);

    next = block_get_next(&kernel_arena, block);
    if (next && block_is_free(next)) {
        size_t combined_size = block->size + HEAP_BLOCK_HEADER_SIZE + next->size;

        if (combined_size >= new_size) {
            /* Remove next from free list */
            free_list_remove(&kernel_arena, next);

            /* Merge blocks */
            block->total_size += next->total_size;
            block->size = block->total_size - HEAP_BLOCK_HEADER_SIZE;
            next->magic = 0;

            kernel_arena.free -= next->total_size;

            spin_unlock(&kernel_arena.lock);
            return ptr;
        }
    }

    spin_unlock(&kernel_arena.lock);

    /* Need to allocate new block and copy */
    new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }

    copy_size = MIN(block->size, new_size);
    memcpy(new_ptr, ptr, copy_size);

    kfree(ptr);

    return new_ptr;
}

/**
 * kfree - Free memory allocated by kmalloc
 */
void kfree(void *ptr)
{
    struct heap_block *block;

    if (!ptr) {
        return;
    }

    /* Ignore early heap allocations */
    if (heap_early_mode) {
        return;
    }

    block = block_get_header(ptr);

    if (!block_is_valid(block) || !block_is_used(block)) {
        return;
    }

    spin_lock(&kernel_arena.lock);

    /* Mark block as free */
    block->flags = HEAP_BLOCK_FREE;
    block->flags &= ~HEAP_BLOCK_USED;

    kernel_arena.used -= block->size;
    kernel_arena.free += block->size;

    /* Add to free list */
    free_list_insert(&kernel_arena, block);

    /* Try to coalesce */
    block_coalesce(&kernel_arena, block);

    spin_unlock(&kernel_arena.lock);
}

/*===========================================================================*/
/*                         ALIGNED ALLOCATION                                */
/*===========================================================================*/

/**
 * kmalloc_aligned - Allocate aligned memory
 */
void *kmalloc_aligned(size_t size, size_t align)
{
    void *ptr;
    uintptr_t aligned_ptr;
    size_t padding;

    /* Align must be power of 2 */
    if (!IS_ALIGNED(align, align) || align < HEAP_ALIGN) {
        align = HEAP_ALIGN;
    }

    /* Allocate extra space for alignment */
    ptr = kmalloc(size + align);
    if (!ptr) {
        return NULL;
    }

    /* Calculate aligned address */
    aligned_ptr = ALIGN((uintptr_t)ptr, align);
    padding = aligned_ptr - (uintptr_t)ptr;

    /* Store original pointer just before aligned address */
    ((void **)aligned_ptr)[-1] = ptr;

    return (void *)aligned_ptr;
}

/**
 * kzalloc_aligned - Allocate zeroed aligned memory
 */
void *kzalloc_aligned(size_t size, size_t align)
{
    void *ptr;

    ptr = kmalloc_aligned(size, align);
    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}

/*===========================================================================*/
/*                         HEAP INITIALIZATION                               */
/*===========================================================================*/

/**
 * heap_init - Initialize kernel heap
 */
void heap_init(void)
{
    virt_addr_t heap_start;
    size_t heap_size;

    pr_info("Initializing Kernel Heap...\n");

    /* Set up kernel heap region */
    heap_start = KERNEL_HEAP_START;
    heap_size = KERNEL_HEAP_SIZE;

    /* Map initial heap pages */
    for (virt_addr_t addr = heap_start;
         addr < heap_start + (4 * MB(1));
         addr += PAGE_SIZE) {
        phys_addr_t paddr = pmm_alloc_page();
        if (paddr == 0) {
            pr_err("Heap: Failed to allocate initial pages\n");
            return;
        }
        vmm_map(addr, paddr, VMA_READ | VMA_WRITE);
    }

    /* Initialize arena */
    arena_init(&kernel_arena, heap_start, 4 * MB(1));

    heap_initialized = true;
    heap_early_mode = false;

    pr_info("  Heap Start: 0x%lx\n", heap_start);
    pr_info("  Heap Size: %lu MB\n", heap_size / MB(1));
    pr_info("  Initial Arena: %lu MB\n", (4 * MB(1)) / MB(1));
    pr_info("Kernel Heap initialized.\n");
}

/*===========================================================================*/
/*                         HEAP STATISTICS                                   */
/*===========================================================================*/

/**
 * heap_get_info - Get heap statistics
 */
void heap_get_info(size_t *total, size_t *used, size_t *free)
{
    if (heap_early_mode) {
        if (total) *total = EARLY_HEAP_SIZE;
        if (used) *used = early_heap_offset;
        if (free) *free = EARLY_HEAP_SIZE - early_heap_offset;
        return;
    }

    spin_lock(&kernel_arena.lock);

    if (total) *total = kernel_arena.size;
    if (used) *used = kernel_arena.used;
    if (free) *free = kernel_arena.free;

    spin_unlock(&kernel_arena.lock);
}

/**
 * heap_check - Check heap integrity
 */
int heap_check(void)
{
    struct heap_block *block;
    int errors = 0;

    if (heap_early_mode) {
        return 0;
    }

    spin_lock(&kernel_arena.lock);

    block = (struct heap_block *)kernel_arena.start;

    while ((virt_addr_t)block < kernel_arena.brk) {
        if (!block_is_valid(block)) {
            pr_err("Heap: Invalid block at %p\n", block);
            errors++;
            break;
        }

        if (block->total_size == 0 || block->total_size > kernel_arena.size) {
            pr_err("Heap: Invalid block size at %p\n", block);
            errors++;
            break;
        }

        block = block_get_next(&kernel_arena, block);
    }

    spin_unlock(&kernel_arena.lock);

    return errors;
}

/**
 * heap_dump - Dump heap information
 */
void heap_dump(void)
{
    struct heap_block *block;
    u32 block_count = 0;
    u32 free_count = 0;
    size_t free_size = 0;

    if (heap_early_mode) {
        printk("Heap: Early mode active\n");
        printk("  Used: %lu / %lu bytes\n", early_heap_offset, EARLY_HEAP_SIZE);
        return;
    }

    spin_lock(&kernel_arena.lock);

    printk("\n=== Heap Information ===\n");
    printk("Start: 0x%lx\n", kernel_arena.start);
    printk("End: 0x%lx\n", kernel_arena.end);
    printk("Break: 0x%lx\n", kernel_arena.brk);
    printk("Size: %lu bytes\n", kernel_arena.size);
    printk("Used: %lu bytes\n", kernel_arena.used);
    printk("Free: %lu bytes\n", kernel_arena.free);
    printk("\n");

    block = (struct heap_block *)kernel_arena.start;

    while ((virt_addr_t)block < kernel_arena.brk && block_count < 100) {
        if (!block_is_valid(block)) {
            printk("  [INVALID] at %p\n", block);
            break;
        }

        if (block_is_free(block)) {
            printk("  [FREE]  %p: %lu bytes\n", block, block->size);
            free_count++;
            free_size += block->size;
        } else {
            printk("  [USED]  %p: %lu bytes\n", block, block->size);
        }

        block = block_get_next(&kernel_arena, block);
        block_count++;
    }

    printk("\nTotal Blocks: %u\n", block_count);
    printk("Free Blocks: %u\n", free_count);
    printk("Free Size: %lu bytes\n", free_size);
    printk("\n");

    spin_unlock(&kernel_arena.lock);
}
