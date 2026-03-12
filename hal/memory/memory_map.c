/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/memory/memory_map.c - Physical Memory Map Management
 *
 * Implements physical memory map discovery, management, and allocation
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         MEMORY MAP DATA STRUCTURES                        */
/*===========================================================================*/

/**
 * memory_entry_t - Memory map entry
 */
typedef struct memory_entry {
    struct list_head list;          /* List entry */
    phys_addr_t base;               /* Base address */
    phys_addr_t size;               /* Size in bytes */
    phys_addr_t end;                /* End address (base + size) */
    u32 type;                       /* Memory type */
    u32 attributes;                 /* Memory attributes */
    u32 numa_node;                  /* NUMA node */
    char name[32];                  /* Region name */
    bool reserved;                  /* Is reserved */
    bool initialized;               /* Is initialized */
    spinlock_t lock;                /* Entry lock */
} memory_entry_t;

/**
 * memory_map_manager_t - Memory map manager global data
 */
typedef struct memory_map_manager {
    struct list_head regions;       /* Memory region list */
    u32 region_count;               /* Number of regions */
    spinlock_t lock;                /* Global lock */

    phys_addr_t total_memory;       /* Total memory */
    phys_addr_t available_memory;   /* Available memory */
    phys_addr_t reserved_memory;    /* Reserved memory */

    phys_addr_t low_memory_end;     /* End of low memory (< 4GB) */
    phys_addr_t high_memory_start;  /* Start of high memory (>= 4GB) */

    bool initialized;               /* Initialization flag */
} memory_map_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static memory_map_manager_t memory_map __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         MEMORY TYPE NAMES                                 */
/*===========================================================================*/

static const char *memory_type_names[] = {
    [MEM_TYPE_AVAILABLE] = "Available",
    [MEM_TYPE_RESERVED] = "Reserved",
    [MEM_TYPE_ACPI] = "ACPI",
    [MEM_TYPE_NVS] = "ACPI NVS",
    [MEM_TYPE_UNUSABLE] = "Unusable",
    [MEM_TYPE_PERSISTENT] = "Persistent",
};

/**
 * memory_get_type_name - Get memory type name
 * @type: Memory type
 *
 * Returns: Type name string
 */
static const char *memory_get_type_name(u32 type)
{
    if (type < ARRAY_SIZE(memory_type_names)) {
        return memory_type_names[type];
    }
    return "Unknown";
}

/*===========================================================================*/
/*                         INTERNAL FUNCTIONS                                */
/*===========================================================================*/

/**
 * memory_entry_create - Create a new memory entry
 * @base: Base address
 * @size: Size in bytes
 * @type: Memory type
 * @name: Region name
 *
 * Returns: Pointer to new entry, or NULL on failure
 */
static memory_entry_t *memory_entry_create(phys_addr_t base, phys_addr_t size,
                                            u32 type, const char *name)
{
    memory_entry_t *entry;

    entry = kzalloc(sizeof(memory_entry_t));
    if (!entry) {
        return NULL;
    }

    entry->base = base;
    entry->size = size;
    entry->end = base + size;
    entry->type = type;
    entry->attributes = 0;
    entry->numa_node = 0;
    entry->reserved = (type != MEM_TYPE_AVAILABLE);
    entry->initialized = true;

    if (name) {
        strncpy(entry->name, name, sizeof(entry->name) - 1);
    } else {
        snprintf(entry->name, sizeof(entry->name), "mem_%08X", (u32)base);
    }

    spin_lock_init_named(&entry->lock, entry->name);

    return entry;
}

/**
 * memory_entry_destroy - Destroy a memory entry
 * @entry: Entry to destroy
 */
static void memory_entry_destroy(memory_entry_t *entry)
{
    if (!entry) {
        return;
    }

    spin_lock(&entry->lock);
    entry->initialized = false;
    spin_unlock(&entry->lock);

    kfree(entry);
}

/**
 * memory_find_entry - Find memory entry by address
 * @addr: Address to find
 *
 * Returns: Pointer to entry, or NULL if not found
 */
static memory_entry_t *memory_find_entry(phys_addr_t addr)
{
    memory_entry_t *entry;

    list_for_each_entry(entry, &memory_map.regions, list) {
        if (addr >= entry->base && addr < entry->end) {
            return entry;
        }
    }

    return NULL;
}

/**
 * memory_find_entry_by_range - Find memory entry by range
 * @base: Base address
 * @size: Size
 *
 * Returns: Pointer to entry, or NULL if not found
 */
static memory_entry_t *memory_find_entry_by_range(phys_addr_t base, phys_addr_t size)
{
    memory_entry_t *entry;
    phys_addr_t end = base + size;

    list_for_each_entry(entry, &memory_map.regions, list) {
        if (base >= entry->base && end <= entry->end) {
            return entry;
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         MEMORY MAP INITIALIZATION                         */
/*===========================================================================*/

/**
 * memory_map_init - Initialize memory map manager
 */
static void memory_map_init(void)
{
    pr_info("Memory: Initializing memory map manager...\n");

    INIT_LIST_HEAD(&memory_map.regions);
    spin_lock_init_named(&memory_map.lock, "memory_map");

    memory_map.region_count = 0;
    memory_map.total_memory = 0;
    memory_map.available_memory = 0;
    memory_map.reserved_memory = 0;

    memory_map.low_memory_end = 0;
    memory_map.high_memory_start = 0;

    memory_map.initialized = false;
}

/**
 * memory_map_add_region_internal - Add region to memory map
 * @base: Base address
 * @size: Size in bytes
 * @type: Memory type
 * @name: Region name
 *
 * Returns: 0 on success, negative error code on failure
 */
static int memory_map_add_region_internal(phys_addr_t base, phys_addr_t size,
                                           u32 type, const char *name)
{
    memory_entry_t *entry;

    if (size == 0) {
        return -EINVAL;
    }

    /* Create new entry */
    entry = memory_entry_create(base, size, type, name);
    if (!entry) {
        return -ENOMEM;
    }

    /* Add to list */
    spin_lock(&memory_map.lock);
    list_add_tail(&entry->list, &memory_map.regions);
    memory_map.region_count++;

    /* Update statistics */
    memory_map.total_memory += size;

    if (type == MEM_TYPE_AVAILABLE) {
        memory_map.available_memory += size;
    } else {
        memory_map.reserved_memory += size;
    }

    /* Update low/high memory boundaries */
    if (base < 0x100000000ULL) { /* Below 4GB */
        if (base + size > memory_map.low_memory_end) {
            memory_map.low_memory_end = base + size;
        }
    } else {
        if (memory_map.high_memory_start == 0 || base < memory_map.high_memory_start) {
            memory_map.high_memory_start = base;
        }
    }

    spin_unlock(&memory_map.lock);

    pr_debug("Memory: Added region %s: 0x%016X - 0x%016X (%u MB) [%s]\n",
             entry->name, base, base + size, (u32)(size / MB(1)),
             memory_get_type_name(type));

    return 0;
}

/*===========================================================================*/
/*                         MEMORY MAP API                                    */
/*===========================================================================*/

/**
 * memory_early_init - Early memory initialization
 *
 * Called during early boot to set up basic memory map.
 */
void memory_early_init(void)
{
    memory_map_init();

    /* Add conventional memory (0-640KB) */
    memory_map_add_region_internal(0x00000000, 0x000A0000,
                                    MEM_TYPE_AVAILABLE, "conventional");

    /* Add BIOS/firmware area (640KB-1MB) */
    memory_map_add_region_internal(0x000A0000, 0x00060000,
                                    MEM_TYPE_RESERVED, "bios_area");

    /* Add high memory area (1MB+) - will be refined later */
    memory_map_add_region_internal(0x00100000, 0x7FE00000,
                                    MEM_TYPE_AVAILABLE, "high_memory");

    pr_info("Memory: Early initialization complete\n");
}

/**
 * memory_init - Main memory initialization
 *
 * Called during kernel initialization to set up complete memory map.
 */
void memory_init(void)
{
    pr_info("Memory: Initializing memory subsystem...\n");

    /* Platform-specific memory map discovery */
#if defined(ARCH_X86_64)
    x86_memory_map_init();
#elif defined(ARCH_ARM64)
    arm_memory_map_init();
#endif

    memory_map.initialized = true;

    pr_info("Memory: Total: %llu MB, Available: %llu MB, Reserved: %llu MB\n",
            memory_map.total_memory / MB(1),
            memory_map.available_memory / MB(1),
            memory_map.reserved_memory / MB(1));
}

/**
 * memory_add_region - Add a memory region
 * @base: Base address
 * @size: Size in bytes
 * @type: Memory type
 *
 * Returns: 0 on success, negative error code on failure
 */
int memory_add_region(phys_addr_t base, phys_addr_t size, u32 type)
{
    if (!memory_map.initialized) {
        return -ENODEV;
    }

    return memory_map_add_region_internal(base, size, type, NULL);
}

/**
 * memory_remove_region - Remove a memory region
 * @base: Base address of region to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int memory_remove_region(phys_addr_t base)
{
    memory_entry_t *entry;
    int ret = -ENOENT;

    if (!memory_map.initialized) {
        return -ENODEV;
    }

    spin_lock(&memory_map.lock);

    entry = memory_find_entry(base);
    if (entry) {
        list_del(&entry->list);
        memory_map.region_count--;

        /* Update statistics */
        memory_map.total_memory -= entry->size;
        if (entry->type == MEM_TYPE_AVAILABLE) {
            memory_map.available_memory -= entry->size;
        } else {
            memory_map.reserved_memory -= entry->size;
        }

        ret = 0;
    }

    spin_unlock(&memory_map.lock);

    if (ret == 0) {
        memory_entry_destroy(entry);
    }

    return ret;
}

/**
 * memory_get_region - Get memory region by index
 * @index: Region index
 *
 * Returns: Pointer to memory_region_t, or NULL on failure
 */
memory_region_t *memory_get_region(u32 index)
{
    memory_entry_t *entry;
    static memory_region_t region;
    u32 i = 0;

    if (!memory_map.initialized) {
        return NULL;
    }

    spin_lock(&memory_map.lock);

    list_for_each_entry(entry, &memory_map.regions, list) {
        if (i == index) {
            region.base = entry->base;
            region.size = entry->size;
            region.type = entry->type;
            region.attributes = entry->attributes;
            strncpy(region.name, entry->name, sizeof(region.name) - 1);

            spin_unlock(&memory_map.lock);
            return &region;
        }
        i++;
    }

    spin_unlock(&memory_map.lock);
    return NULL;
}

/**
 * memory_get_region_count - Get number of memory regions
 *
 * Returns: Number of regions
 */
u32 memory_get_region_count(void)
{
    return memory_map.region_count;
}

/**
 * memory_get_total - Get total memory size
 *
 * Returns: Total memory in bytes
 */
phys_addr_t memory_get_total(void)
{
    return memory_map.total_memory;
}

/**
 * memory_get_available - Get available memory size
 *
 * Returns: Available memory in bytes
 */
phys_addr_t memory_get_available(void)
{
    return memory_map.available_memory;
}

/**
 * memory_get_reserved - Get reserved memory size
 *
 * Returns: Reserved memory in bytes
 */
phys_addr_t memory_get_reserved(void)
{
    return memory_map.reserved_memory;
}

/*===========================================================================*/
/*                         MEMORY RESERVATION                                */
/*===========================================================================*/

/**
 * memory_reserve - Reserve a memory region
 * @base: Base address
 * @size: Size in bytes
 *
 * Returns: 0 on success, negative error code on failure
 */
int memory_reserve(phys_addr_t base, phys_addr_t size)
{
    memory_entry_t *entry;
    int ret = -ENOENT;

    if (!memory_map.initialized) {
        return -ENODEV;
    }

    if (size == 0) {
        return -EINVAL;
    }

    spin_lock(&memory_map.lock);

    entry = memory_find_entry(base);
    if (entry) {
        spin_lock(&entry->lock);

        if (!entry->reserved) {
            entry->reserved = true;
            memory_map.available_memory -= size;
            memory_map.reserved_memory += size;
            ret = 0;
        }

        spin_unlock(&entry->lock);
    }

    spin_unlock(&memory_map.lock);

    if (ret == 0) {
        pr_debug("Memory: Reserved 0x%016X - 0x%016X (%u bytes)\n",
                 base, base + size, (u32)size);
    }

    return ret;
}

/**
 * memory_free - Free a reserved memory region
 * @base: Base address
 * @size: Size in bytes
 *
 * Returns: 0 on success, negative error code on failure
 */
int memory_free(phys_addr_t base, phys_addr_t size)
{
    memory_entry_t *entry;
    int ret = -ENOENT;

    if (!memory_map.initialized) {
        return -ENODEV;
    }

    if (size == 0) {
        return -EINVAL;
    }

    spin_lock(&memory_map.lock);

    entry = memory_find_entry(base);
    if (entry) {
        spin_lock(&entry->lock);

        if (entry->reserved) {
            entry->reserved = false;
            memory_map.available_memory += size;
            memory_map.reserved_memory -= size;
            ret = 0;
        }

        spin_unlock(&entry->lock);
    }

    spin_unlock(&memory_map.lock);

    if (ret == 0) {
        pr_debug("Memory: Freed 0x%016X - 0x%016X (%u bytes)\n",
                 base, base + size, (u32)size);
    }

    return ret;
}

/**
 * memory_reserve_range - Reserve a physical address range
 * @start: Start address
 * @end: End address
 *
 * Returns: 0 on success, negative error code on failure
 */
int memory_reserve_range(phys_addr_t start, phys_addr_t end)
{
    return memory_reserve(start, end - start);
}

/**
 * memory_alloc_low - Allocate low memory region
 * @size: Size to allocate
 * @align: Alignment requirement
 *
 * Returns: Base address, or 0 on failure
 */
phys_addr_t memory_alloc_low(phys_addr_t size, phys_addr_t align)
{
    memory_entry_t *entry;
    phys_addr_t addr = 0;

    if (!memory_map.initialized) {
        return 0;
    }

    spin_lock(&memory_map.lock);

    list_for_each_entry(entry, &memory_map.regions, list) {
        phys_addr_t aligned_base;

        if (entry->type != MEM_TYPE_AVAILABLE || entry->reserved) {
            continue;
        }

        if (entry->end > 0x100000000ULL) {
            continue; /* Not low memory */
        }

        /* Align the base address */
        aligned_base = ALIGN(entry->base, align);

        if (aligned_base + size <= entry->end) {
            addr = aligned_base;
            entry->base = aligned_base + size;
            entry->size -= size;
            memory_map.available_memory -= size;
            break;
        }
    }

    spin_unlock(&memory_map.lock);

    if (addr != 0) {
        pr_debug("Memory: Allocated low memory 0x%016X (%u bytes)\n",
                 addr, (u32)size);
    }

    return addr;
}

/**
 * memory_alloc_high - Allocate high memory region
 * @size: Size to allocate
 * @align: Alignment requirement
 *
 * Returns: Base address, or 0 on failure
 */
phys_addr_t memory_alloc_high(phys_addr_t size, phys_addr_t align)
{
    memory_entry_t *entry;
    phys_addr_t addr = 0;

    if (!memory_map.initialized) {
        return 0;
    }

    spin_lock(&memory_map.lock);

    list_for_each_entry(entry, &memory_map.regions, list) {
        phys_addr_t aligned_base;

        if (entry->type != MEM_TYPE_AVAILABLE || entry->reserved) {
            continue;
        }

        if (entry->base < 0x100000000ULL) {
            continue; /* Not high memory */
        }

        /* Align the base address */
        aligned_base = ALIGN(entry->base, align);

        if (aligned_base + size <= entry->end) {
            addr = aligned_base;
            entry->base = aligned_base + size;
            entry->size -= size;
            memory_map.available_memory -= size;
            break;
        }
    }

    spin_unlock(&memory_map.lock);

    return addr;
}

/*===========================================================================*/
/*                         MEMORY MAP DEBUGGING                              */
/*===========================================================================*/

/**
 * memory_map_dump - Dump memory map for debugging
 */
void memory_map_dump(void)
{
    memory_entry_t *entry;

    pr_info("Memory Map:\n");
    pr_info("  Total: %llu MB\n", memory_map.total_memory / MB(1));
    pr_info("  Available: %llu MB\n", memory_map.available_memory / MB(1));
    pr_info("  Reserved: %llu MB\n", memory_map.reserved_memory / MB(1));
    pr_info("  Low Memory End: 0x%016X\n", memory_map.low_memory_end);
    pr_info("  High Memory Start: 0x%016X\n", memory_map.high_memory_start);
    pr_info("  Regions: %u\n", memory_map.region_count);
    pr_info("\n");

    spin_lock(&memory_map.lock);

    list_for_each_entry(entry, &memory_map.regions, list) {
        pr_info("  %s: 0x%016X - 0x%016X (%6u MB) [%s]%s\n",
                entry->name,
                entry->base,
                entry->end,
                (u32)(entry->size / MB(1)),
                memory_get_type_name(entry->type),
                entry->reserved ? " [RESERVED]" : "");
    }

    spin_unlock(&memory_map.lock);
}

/**
 * memory_map_validate - Validate memory map consistency
 *
 * Returns: 0 if valid, negative error code if invalid
 */
int memory_map_validate(void)
{
    memory_entry_t *entry, *prev;
    phys_addr_t total = 0, available = 0, reserved = 0;

    if (!memory_map.initialized) {
        return -ENODEV;
    }

    spin_lock(&memory_map.lock);

    prev = NULL;
    list_for_each_entry(entry, &memory_map.regions, list) {
        /* Check for overlapping regions */
        if (prev && entry->base < prev->end) {
            pr_err("Memory: Overlapping regions detected!\n");
            pr_err("  %s: 0x%016X - 0x%016X\n", prev->name, prev->base, prev->end);
            pr_err("  %s: 0x%016X - 0x%016X\n", entry->name, entry->base, entry->end);
            spin_unlock(&memory_map.lock);
            return -EINVAL;
        }

        /* Accumulate statistics */
        total += entry->size;
        if (entry->type == MEM_TYPE_AVAILABLE && !entry->reserved) {
            available += entry->size;
        } else {
            reserved += entry->size;
        }

        prev = entry;
    }

    /* Verify statistics match */
    if (total != memory_map.total_memory ||
        available != memory_map.available_memory ||
        reserved != memory_map.reserved_memory) {
        pr_err("Memory: Statistics mismatch!\n");
        spin_unlock(&memory_map.lock);
        return -EINVAL;
    }

    spin_unlock(&memory_map.lock);

    pr_info("Memory: Map validation successful\n");

    return 0;
}

/*===========================================================================*/
/*                         ARCHITECTURE-SPECIFIC INIT                        */
/*===========================================================================*/

#if defined(ARCH_X86_64)
/**
 * x86_memory_map_init - x86_64 memory map initialization
 *
 * Discovers memory map using E820 or EFI.
 */
void x86_memory_map_init(void)
{
    /* Placeholder for x86_64 E820/EFI memory map discovery */
    pr_info("Memory: x86_64 memory map initialization (placeholder)\n");
}
#endif

#if defined(ARCH_ARM64)
/**
 * arm_memory_map_init - ARM64 memory map initialization
 *
 * Discovers memory map using Device Tree or EFI.
 */
void arm_memory_map_init(void)
{
    /* Placeholder for ARM64 DTB/EFI memory map discovery */
    pr_info("Memory: ARM64 memory map initialization (placeholder)\n");
}
#endif
