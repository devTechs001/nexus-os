/*
 * NEXUS OS - Slab Allocator
 * kernel/mm/slab.c
 */

#include "../include/kernel.h"
#include "mm.h"

/*===========================================================================*/
/*                         SLAB ALLOCATOR                                    */
/*===========================================================================*/

/* Slab Configuration */
#define SLAB_MAGIC          0xCAFEBABE
#define SLAB_MIN_SIZE       32
#define SLAB_MAX_SIZE       (128 * KB(1))
#define SLAB_MIN_ORDER      0
#define SLAB_MAX_ORDER      5
#define SLAB_COLORING       1

/* Slab Flags */
#define SLAB_FLAG_FREE      0x00000001
#define SLAB_FLAG_ACTIVE    0x00000002
#define SLAB_FLAG_FULL      0x00000004
#define SLAB_FLAG_EMPTY     0x00000008
#define SLAB_FLAG_PARTIAL   0x00000010

/* Object Debugging */
#define SLAB_DEBUG_FREE     0x00000001
#define SLAB_DEBUG_REDZONE  0x00000002
#define SLAB_DEBUG_POISON   0x00000004
#define SLAB_DEBUG_TRACK    0x00000008

/* Redzone Patterns */
#define SLAB_REDZONE_FREE   0xAA
#define SLAB_REDZONE_USED   0xBB
#define SLAB_POISON_FREE    0x6B
#define SLAB_POISON_USED    0xDC
#define SLAB_POISON_INIT    0x5A

/* Slab Object Header (for debugging) */
struct slab_object {
    struct slab_object *next;
    struct slab_object *prev;
    u32 magic;
    u32 flags;
    void *cache;
    void *slab;
};

/* Slab Descriptor */
struct slab {
    u32 magic;
    u32 flags;
    u32 inuse;              /* Number of objects in use */
    u32 total;              /* Total objects in slab */
    u32 order;              /* Allocation order (2^order pages) */
    void *mem;              /* Slab memory */
    void *free;             /* Pointer to first free object */
    struct slab_cache *cache; /* Parent cache */
    struct slab *next;      /* Next slab in list */
    struct slab *prev;      /* Previous slab in list */
    struct list_head list;  /* List entry */
};

/* Slab Cache */
struct slab_cache {
    u32 magic;
    char name[32];          /* Cache name */
    size_t size;            /* Object size */
    size_t align;           /* Object alignment */
    size_t total_size;      /* Total size with metadata */
    u32 flags;              /* Cache flags */
    u32 order;              /* Slab order */
    u32 num_objects;        /* Objects per slab */
    u32 color;              /* Color offset */
    u32 num_colors;         /* Number of colors */

    /* Slab Lists */
    struct slab *slabs_full;
    struct slab *slabs_partial;
    struct slab *slabs_empty;

    /* Free Object List */
    void *free_list;

    /* Statistics */
    atomic_t alloc_count;
    atomic_t free_count;
    atomic_t active_objects;

    /* Lock */
    spinlock_t lock;

    /* Constructor/Destructor */
    void (*ctor)(void *obj);
    void (*dtor)(void *obj);
};

/* Global Slab Caches */
#define MAX_SLAB_CACHES     64
static struct slab_cache *slab_caches[MAX_SLAB_CACHES];
static u32 num_slab_caches = 0;
static spinlock_t slab_caches_lock = { .lock = 0 };

/* General Purpose Caches */
static struct slab_cache *cache_32 = NULL;
static struct slab_cache *cache_64 = NULL;
static struct slab_cache *cache_128 = NULL;
static struct slab_cache *cache_256 = NULL;
static struct slab_cache *cache_512 = NULL;
static struct slab_cache *cache_1024 = NULL;
static struct slab_cache *cache_2048 = NULL;
static struct slab_cache *cache_4096 = NULL;

/*===========================================================================*/
/*                         OBJECT OPERATIONS                                 */
/*===========================================================================*/

/**
 * slab_get_object_size - Get actual object size
 */
static inline size_t slab_get_object_size(struct slab_cache *cache)
{
    return cache->size;
}

/**
 * slab_get_slab_size - Get slab memory size
 */
static inline size_t slab_get_slab_size(struct slab_cache *cache)
{
    return (PAGE_SIZE << cache->order);
}

/**
 * slab_get_object - Get object pointer from index
 */
static inline void *slab_get_object(struct slab *slab, u32 index)
{
    struct slab_cache *cache = slab->cache;
    u8 *base = (u8 *)slab->mem;

    return base + (index * cache->total_size);
}

/**
 * slab_get_index - Get object index
 */
static inline u32 slab_get_index(struct slab *slab, void *obj)
{
    struct slab_cache *cache = slab->cache;
    u8 *base = (u8 *)slab->mem;

    return ((u8 *)obj - base) / cache->total_size;
}

/**
 * slab_is_valid_object - Validate object
 */
static inline bool slab_is_valid_object(struct slab *slab, void *obj)
{
    struct slab_cache *cache = slab->cache;
    u8 *base = (u8 *)slab->mem;
    u8 *end = base + (cache->num_objects * cache->total_size);

    if ((u8 *)obj < base || (u8 *)obj >= end) {
        return false;
    }

    if (((u8 *)obj - base) % cache->total_size != 0) {
        return false;
    }

    return true;
}

/*===========================================================================*/
/*                         DEBUGGING OPERATIONS                              */
/*===========================================================================*/

/**
 * slab_poison - Poison object memory
 */
static void slab_poison(void *obj, size_t size, u8 pattern)
{
    memset(obj, pattern, size);
}

/**
 * slab_redzone_init - Initialize redzones
 */
static void slab_redzone_init(void *obj, size_t size)
{
    u8 *ptr = (u8 *)obj;

    /* Front redzone */
    memset(ptr - sizeof(size_t), SLAB_REDZONE_FREE, sizeof(size_t));

    /* Back redzone */
    memset(ptr + size, SLAB_REDZONE_FREE, sizeof(size_t));
}

/**
 * slab_redzone_check - Check redzones
 */
static bool slab_redzone_check(void *obj, size_t size)
{
    u8 *ptr = (u8 *)obj;
    u8 *front = ptr - sizeof(size_t);
    u8 *back = ptr + size;

    for (size_t i = 0; i < sizeof(size_t); i++) {
        if (front[i] != SLAB_REDZONE_FREE) {
            return false;
        }
        if (back[i] != SLAB_REDZONE_FREE) {
            return false;
        }
    }

    return true;
}

/**
 * slab_check_object - Check object integrity
 */
static bool slab_check_object(struct slab_cache *cache, void *obj)
{
    if (cache->flags & SLAB_DEBUG_REDZONE) {
        if (!slab_redzone_check(obj, cache->size)) {
            pr_err("Slab: Redzone corruption detected at %p (cache: %s)\n",
                   obj, cache->name);
            return false;
        }
    }

    return true;
}

/*===========================================================================*/
/*                         SLAB OPERATIONS                                   */
/*===========================================================================*/

/**
 * slab_create - Create new slab
 */
static struct slab *slab_create(struct slab_cache *cache, u32 order)
{
    struct slab *slab;
    phys_addr_t paddr;
    size_t slab_size;
    u32 i;

    /* Allocate pages for slab */
    slab_size = PAGE_SIZE << order;
    paddr = pmm_alloc_pages(order + 1);
    if (paddr == 0) {
        return NULL;
    }

    /* Allocate slab descriptor */
    slab = (struct slab *)kmalloc(sizeof(struct slab));
    if (!slab) {
        pmm_free_pages(paddr, order + 1);
        return NULL;
    }

    memset(slab, 0, sizeof(struct slab));

    /* Initialize slab */
    slab->magic = SLAB_MAGIC;
    slab->flags = SLAB_FLAG_EMPTY;
    slab->inuse = 0;
    slab->total = cache->num_objects;
    slab->order = order;
    slab->cache = cache;
    slab->next = NULL;
    slab->prev = NULL;
    INIT_LIST_HEAD(&slab->list);

    /* Map virtual address */
    slab->mem = (void *)vmm_phys_to_virt(paddr);

    /* Initialize free list */
    slab->free = slab->mem;

    /* Link objects in free list */
    for (i = 0; i < cache->num_objects - 1; i++) {
        void *obj = slab_get_object(slab, i);
        void *next_obj = slab_get_object(slab, i + 1);
        *(void **)obj = next_obj;
    }

    /* Last object points to NULL */
    *(void **)slab_get_object(slab, cache->num_objects - 1) = NULL;

    /* Apply coloring if enabled */
    if (SLAB_COLORING && cache->num_colors > 1) {
        slab->mem = (u8 *)slab->mem + (cache->color * cache->align);
    }

    return slab;
}

/**
 * slab_destroy - Destroy slab
 */
static void slab_destroy(struct slab *slab)
{
    phys_addr_t paddr;
    size_t slab_size;

    if (!slab) {
        return;
    }

    /* Get physical address */
    paddr = vmm_virt_to_phys((virt_addr_t)slab->mem);

    /* Free pages */
    slab_size = PAGE_SIZE << slab->order;
    pmm_free_pages(paddr, slab->order + 1);

    /* Free descriptor */
    kfree(slab);
}

/**
 * slab_add_object - Add object to free list
 */
static void slab_add_object(struct slab *slab, void *obj)
{
    void *current_free = slab->free;

    *(void **)obj = current_free;
    slab->free = obj;
}

/**
 * slab_remove_object - Remove object from free list
 */
static void *slab_remove_object(struct slab *slab)
{
    void *obj = slab->free;

    if (obj) {
        slab->free = *(void **)obj;
    }

    return obj;
}

/*===========================================================================*/
/*                         CACHE OPERATIONS                                  */
/*===========================================================================*/

/**
 * cache_update_flags - Update cache flags based on slab state
 */
static void cache_update_flags(struct slab_cache *cache, struct slab *slab)
{
    if (slab->inuse == slab->total) {
        slab->flags &= ~SLAB_FLAG_PARTIAL;
        slab->flags &= ~SLAB_FLAG_EMPTY;
        slab->flags |= SLAB_FLAG_FULL;
    } else if (slab->inuse == 0) {
        slab->flags &= ~SLAB_FLAG_PARTIAL;
        slab->flags &= ~SLAB_FLAG_FULL;
        slab->flags |= SLAB_FLAG_EMPTY;
    } else {
        slab->flags &= ~SLAB_FLAG_FULL;
        slab->flags &= ~SLAB_FLAG_EMPTY;
        slab->flags |= SLAB_FLAG_PARTIAL;
    }
}

/**
 * cache_move_slab - Move slab between lists
 */
static void cache_move_slab(struct slab_cache *cache, struct slab *slab,
                            u32 from_flags, u32 to_flags)
{
    struct slab **from_list = NULL;
    struct slab **to_list = NULL;

    /* Determine source and destination lists */
    if (from_flags & SLAB_FLAG_FULL) {
        from_list = &cache->slabs_full;
    } else if (from_flags & SLAB_FLAG_PARTIAL) {
        from_list = &cache->slabs_partial;
    } else if (from_flags & SLAB_FLAG_EMPTY) {
        from_list = &cache->slabs_empty;
    }

    if (to_flags & SLAB_FLAG_FULL) {
        to_list = &cache->slabs_full;
    } else if (to_flags & SLAB_FLAG_PARTIAL) {
        to_list = &cache->slabs_partial;
    } else if (to_flags & SLAB_FLAG_EMPTY) {
        to_list = &cache->slabs_empty;
    }

    /* Remove from old list */
    if (from_list) {
        if (*from_list == slab) {
            *from_list = slab->next;
        }
        if (slab->prev) {
            slab->prev->next = slab->next;
        }
        if (slab->next) {
            slab->next->prev = slab->prev;
        }
    }

    /* Add to new list */
    if (to_list) {
        slab->next = *to_list;
        slab->prev = NULL;
        if (*to_list) {
            (*to_list)->prev = slab;
        }
        *to_list = slab;
    }

    slab->flags = to_flags;
}

/**
 * cache_find_slab - Find slab with free objects
 */
static struct slab *cache_find_slab(struct slab_cache *cache)
{
    struct slab *slab;

    /* First try partial slabs */
    slab = cache->slabs_partial;
    if (slab) {
        return slab;
    }

    /* Then try empty slabs */
    slab = cache->slabs_empty;
    if (slab) {
        return slab;
    }

    return NULL;
}

/**
 * cache_grow - Grow cache by adding new slab
 */
static int cache_grow(struct slab_cache *cache)
{
    struct slab *slab;

    slab = slab_create(cache, cache->order);
    if (!slab) {
        return -1;
    }

    /* Add to empty list */
    slab->next = cache->slabs_empty;
    if (cache->slabs_empty) {
        cache->slabs_empty->prev = slab;
    }
    cache->slabs_empty = slab;

    return 0;
}

/**
 * cache_reap - Reap empty slabs from cache
 */
static void cache_reap(struct slab_cache *cache)
{
    struct slab *slab;
    struct slab *next;
    u32 reap_count = 0;

    spin_lock(&cache->lock);

    /* Reap half of empty slabs */
    slab = cache->slabs_empty;
    while (slab && reap_count < (cache->num_objects / 2)) {
        next = slab->next;

        /* Remove from list */
        if (cache->slabs_empty == slab) {
            cache->slabs_empty = next;
        }
        if (next) {
            next->prev = NULL;
        }

        slab_destroy(slab);
        reap_count++;

        slab = next;
    }

    spin_unlock(&cache->lock);
}

/*===========================================================================*/
/*                         CACHE MANAGEMENT                                  */
/*===========================================================================*/

/**
 * kmem_cache_create - Create slab cache
 */
void *kmem_cache_create(const char *name, size_t size, size_t align, u32 flags)
{
    struct slab_cache *cache;
    size_t total_size;
    u32 order;
    u32 num_objects;

    if (!name || size == 0 || size > SLAB_MAX_SIZE) {
        return NULL;
    }

    /* Set minimum alignment */
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }

    /* Calculate total object size */
    total_size = size;
    if (flags & SLAB_DEBUG_REDZONE) {
        total_size += 2 * sizeof(size_t); /* Front and back redzone */
    }

    /* Align object size */
    total_size = ALIGN(total_size, align);

    /* Find optimal order */
    order = SLAB_MIN_ORDER;
    while (order <= SLAB_MAX_ORDER) {
        size_t slab_size = PAGE_SIZE << order;
        num_objects = slab_size / total_size;

        if (num_objects >= 1) {
            break;
        }
        order++;
    }

    if (order > SLAB_MAX_ORDER) {
        pr_err("Slab: Cannot create cache for size %lu\n", size);
        return NULL;
    }

    /* Allocate cache structure */
    cache = (struct slab_cache *)kzalloc(sizeof(struct slab_cache));
    if (!cache) {
        return NULL;
    }

    /* Initialize cache */
    cache->magic = SLAB_MAGIC;
    strncpy(cache->name, name, sizeof(cache->name) - 1);
    cache->size = size;
    cache->align = align;
    cache->total_size = total_size;
    cache->flags = flags;
    cache->order = order;
    cache->num_objects = (PAGE_SIZE << order) / total_size;
    cache->color = 0;
    cache->num_colors = 1;

    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
    cache->free_list = NULL;

    atomic_set(&cache->alloc_count, 0);
    atomic_set(&cache->free_count, 0);
    atomic_set(&cache->active_objects, 0);

    spin_lock_init(&cache->lock);

    cache->ctor = NULL;
    cache->dtor = NULL;

    /* Register cache */
    spin_lock(&slab_caches_lock);

    if (num_slab_caches >= MAX_SLAB_CACHES) {
        spin_unlock(&slab_caches_lock);
        kfree(cache);
        return NULL;
    }

    slab_caches[num_slab_caches++] = cache;

    spin_unlock(&slab_caches_lock);

    pr_debug("Slab: Created cache '%s' (size=%lu, align=%lu, order=%u, objects=%u)\n",
             name, size, align, order, cache->num_objects);

    return cache;
}

/**
 * kmem_cache_destroy - Destroy slab cache
 */
void kmem_cache_destroy(void *cache_ptr)
{
    struct slab_cache *cache = (struct slab_cache *)cache_ptr;
    struct slab *slab;
    struct slab *next;
    u32 i;

    if (!cache || cache->magic != SLAB_MAGIC) {
        return;
    }

    spin_lock(&cache->lock);

    /* Destroy all slabs */
    for (slab = cache->slabs_empty; slab; slab = next) {
        next = slab->next;
        slab_destroy(slab);
    }

    for (slab = cache->slabs_partial; slab; slab = next) {
        next = slab->next;
        slab_destroy(slab);
    }

    for (slab = cache->slabs_full; slab; slab = next) {
        next = slab->next;
        slab_destroy(slab);
    }

    spin_unlock(&cache->lock);

    /* Unregister cache */
    spin_lock(&slab_caches_lock);

    for (i = 0; i < num_slab_caches; i++) {
        if (slab_caches[i] == cache) {
            slab_caches[i] = slab_caches[--num_slab_caches];
            slab_caches[num_slab_caches] = NULL;
            break;
        }
    }

    spin_unlock(&slab_caches_lock);

    kfree(cache);

    pr_debug("Slab: Destroyed cache '%s'\n", cache->name);
}

/*===========================================================================*/
/*                         CACHE ALLOCATION                                  */
/*===========================================================================*/

/**
 * kmem_cache_alloc - Allocate object from cache
 */
void *kmem_cache_alloc(void *cache_ptr)
{
    struct slab_cache *cache = (struct slab_cache *)cache_ptr;
    struct slab *slab;
    void *obj;

    if (!cache || cache->magic != SLAB_MAGIC) {
        return NULL;
    }

    spin_lock(&cache->lock);

    /* Find slab with free objects */
    slab = cache_find_slab(cache);

    /* If no slab available, grow cache */
    if (!slab) {
        if (cache_grow(cache) != 0) {
            spin_unlock(&cache->lock);
            return NULL;
        }
        slab = cache_find_slab(cache);
    }

    if (!slab) {
        spin_unlock(&cache->lock);
        return NULL;
    }

    /* Allocate object from slab */
    obj = slab_remove_object(slab);

    if (!obj) {
        spin_unlock(&cache->lock);
        return NULL;
    }

    /* Update slab statistics */
    slab->inuse++;
    cache_update_flags(cache, slab);

    /* Move slab to appropriate list */
    if (slab->inuse == 1) {
        cache_move_slab(cache, slab, SLAB_FLAG_EMPTY, SLAB_FLAG_PARTIAL);
    } else if (slab->inuse == slab->total) {
        cache_move_slab(cache, slab, SLAB_FLAG_PARTIAL, SLAB_FLAG_FULL);
    }

    /* Update cache statistics */
    atomic_inc(&cache->alloc_count);
    atomic_inc(&cache->active_objects);

    spin_unlock(&cache->lock);

    /* Initialize object */
    if (cache->flags & SLAB_DEBUG_POISON) {
        slab_poison(obj, cache->size, SLAB_POISON_USED);
    }

    if (cache->ctor) {
        cache->ctor(obj);
    }

    if (cache->flags & SLAB_DEBUG_REDZONE) {
        slab_redzone_init(obj, cache->size);
        obj = (u8 *)obj + sizeof(size_t); /* Skip front redzone */
    }

    return obj;
}

/**
 * kmem_cache_free - Free object back to cache
 */
void kmem_cache_free(void *cache_ptr, void *obj)
{
    struct slab_cache *cache = (struct slab_cache *)cache_ptr;
    struct slab *slab;
    u32 index;

    if (!cache || cache->magic != SLAB_MAGIC || !obj) {
        return;
    }

    /* Adjust for redzone */
    if (cache->flags & SLAB_DEBUG_REDZONE) {
        obj = (u8 *)obj - sizeof(size_t);
    }

    spin_lock(&cache->lock);

    /* Find slab containing object */
    /* In real implementation, use page-to-slab mapping */
    /* For now, search through slabs */
    slab = cache->slabs_partial;
    while (slab) {
        if (slab_is_valid_object(slab, obj)) {
            break;
        }
        slab = slab->next;
    }

    if (!slab) {
        slab = cache->slabs_full;
        while (slab) {
            if (slab_is_valid_object(slab, obj)) {
                break;
            }
            slab = slab->next;
        }
    }

    if (!slab) {
        pr_err("Slab: Object %p not found in cache %s\n", obj, cache->name);
        spin_unlock(&cache->lock);
        return;
    }

    /* Check object integrity */
    if (cache->flags & SLAB_DEBUG_REDZONE) {
        if (!slab_check_object(cache, obj)) {
            spin_unlock(&cache->lock);
            return;
        }
    }

    /* Free object */
    if (cache->dtor) {
        cache->dtor(obj);
    }

    if (cache->flags & SLAB_DEBUG_POISON) {
        slab_poison(obj, cache->size, SLAB_POISON_FREE);
    }

    slab_add_object(slab, obj);

    /* Update slab statistics */
    slab->inuse--;
    cache_update_flags(cache, slab);

    /* Move slab to appropriate list */
    if (slab->inuse == slab->total - 1) {
        cache_move_slab(cache, slab, SLAB_FLAG_FULL, SLAB_FLAG_PARTIAL);
    } else if (slab->inuse == 0) {
        cache_move_slab(cache, slab, SLAB_FLAG_PARTIAL, SLAB_FLAG_EMPTY);
    }

    /* Update cache statistics */
    atomic_inc(&cache->free_count);
    atomic_dec(&cache->active_objects);

    spin_unlock(&cache->lock);
}

/*===========================================================================*/
/*                         GENERAL PURPOSE ALLOCATION                        */
/*===========================================================================*/

/**
 * slab_alloc - Allocate from general purpose caches
 */
static void *slab_alloc(size_t size)
{
    struct slab_cache *cache = NULL;

    /* Select appropriate cache based on size */
    if (size <= 32) {
        cache = cache_32;
    } else if (size <= 64) {
        cache = cache_64;
    } else if (size <= 128) {
        cache = cache_128;
    } else if (size <= 256) {
        cache = cache_256;
    } else if (size <= 512) {
        cache = cache_512;
    } else if (size <= 1024) {
        cache = cache_1024;
    } else if (size <= 2048) {
        cache = cache_2048;
    } else if (size <= 4096) {
        cache = cache_4096;
    }

    if (cache) {
        return kmem_cache_alloc(cache);
    }

    /* Fall back to kmalloc for larger sizes */
    return kmalloc(size);
}

/**
 * slab_free - Free to general purpose caches
 */
static void slab_free(void *cache, void *obj)
{
    if (cache && ((struct slab_cache *)cache)->magic == SLAB_MAGIC) {
        kmem_cache_free(cache, obj);
    } else {
        kfree(obj);
    }
}

/*===========================================================================*/
/*                         SLAB INITIALIZATION                               */
/*===========================================================================*/

/**
 * slab_init - Initialize slab allocator
 */
void slab_init(void)
{
    pr_info("Initializing Slab Allocator...\n");

    spin_lock_init(&slab_caches_lock);

    /* Create general purpose caches */
    cache_32 = kmem_cache_create("kmem_cache-32", 32, 8, 0);
    cache_64 = kmem_cache_create("kmem_cache-64", 64, 8, 0);
    cache_128 = kmem_cache_create("kmem_cache-128", 128, 16, 0);
    cache_256 = kmem_cache_create("kmem_cache-256", 256, 16, 0);
    cache_512 = kmem_cache_create("kmem_cache-512", 512, 32, 0);
    cache_1024 = kmem_cache_create("kmem_cache-1024", 1024, 64, 0);
    cache_2048 = kmem_cache_create("kmem_cache-2048", 2048, 64, 0);
    cache_4096 = kmem_cache_create("kmem_cache-4096", 4096, 64, 0);

    pr_info("  General purpose caches created\n");
    pr_info("Slab Allocator initialized.\n");
}

/*===========================================================================*/
/*                         CACHE STATISTICS                                  */
/*===========================================================================*/

/**
 * kmem_cache_info - Get cache statistics
 */
void kmem_cache_info(void *cache_ptr, size_t *active, size_t *total)
{
    struct slab_cache *cache = (struct slab_cache *)cache_ptr;

    if (!cache || cache->magic != SLAB_MAGIC) {
        return;
    }

    if (active) {
        *active = atomic_read(&cache->active_objects);
    }

    if (total) {
        u32 slab_count = 0;
        struct slab *slab;

        spin_lock(&cache->lock);

        for (slab = cache->slabs_full; slab; slab = slab->next) {
            slab_count++;
        }
        for (slab = cache->slabs_partial; slab; slab = slab->next) {
            slab_count++;
        }
        for (slab = cache->slabs_empty; slab; slab = slab->next) {
            slab_count++;
        }

        *total = slab_count * cache->num_objects;

        spin_unlock(&cache->lock);
    }
}

/**
 * slab_info - Print slab allocator statistics
 */
void slab_info(void)
{
    u32 i;
    struct slab_cache *cache;

    printk("\n=== Slab Allocator Information ===\n");
    printk("Total Caches: %u\n", num_slab_caches);
    printk("\n");

    spin_lock(&slab_caches_lock);

    for (i = 0; i < num_slab_caches; i++) {
        cache = slab_caches[i];

        printk("Cache: %s\n", cache->name);
        printk("  Object Size: %lu bytes\n", cache->size);
        printk("  Alignment: %lu bytes\n", cache->align);
        printk("  Order: %u\n", cache->order);
        printk("  Objects/Slab: %u\n", cache->num_objects);
        printk("  Active Objects: %d\n", atomic_read(&cache->active_objects));
        printk("  Alloc Count: %d\n", atomic_read(&cache->alloc_count));
        printk("  Free Count: %d\n", atomic_read(&cache->free_count));
        printk("\n");
    }

    spin_unlock(&slab_caches_lock);
}

/**
 * slab_reap_all - Reap all caches
 */
void slab_reap_all(void)
{
    u32 i;

    spin_lock(&slab_caches_lock);

    for (i = 0; i < num_slab_caches; i++) {
        cache_reap(slab_caches[i]);
    }

    spin_unlock(&slab_caches_lock);
}
