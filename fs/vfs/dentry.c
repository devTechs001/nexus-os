/*
 * NEXUS OS - Virtual File System
 * fs/vfs/dentry.c
 *
 * Dentry Management - Directory entry caching and lifecycle
 */

#include "vfs.h"

/*===========================================================================*/
/*                         DENTRY CACHE                                      */
/*===========================================================================*/

/* Dentry cache slab */
static void *dentry_cache = NULL;

/* Dentry name cache slab */
static void *dentry_name_cache = NULL;

/* Dentry statistics */
static struct {
    atomic_t allocated;
    atomic_t freed;
    atomic_t negative;
    atomic_t referenced;
    u64 total_lookups;
    u64 total_creates;
    u64 total_deletes;
} dentry_stats;

/*===========================================================================*/
/*                         DENTRY CACHE INITIALIZATION                       */
/*===========================================================================*/

/**
 * vfs_dentry_cache_init - Initialize dentry cache
 *
 * Creates slab caches for dentry allocations.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_dentry_cache_init(void)
{
    dentry_cache = kmem_cache_create("vfs_dentry", sizeof(struct vfs_dentry),
                                      __alignof__(struct vfs_dentry), 0);
    if (!dentry_cache) {
        pr_err("VFS: Failed to create dentry cache\n");
        return VFS_ENOMEM;
    }

    dentry_name_cache = kmem_cache_create("vfs_dentry_name", VFS_NAME_MAX + 1,
                                           8, 0);
    if (!dentry_name_cache) {
        kmem_cache_destroy(dentry_cache);
        dentry_cache = NULL;
        pr_err("VFS: Failed to create dentry name cache\n");
        return VFS_ENOMEM;
    }

    atomic_set(&dentry_stats.allocated, 0);
    atomic_set(&dentry_stats.freed, 0);
    atomic_set(&dentry_stats.negative, 0);
    atomic_set(&dentry_stats.referenced, 0);
    dentry_stats.total_lookups = 0;
    dentry_stats.total_creates = 0;
    dentry_stats.total_deletes = 0;

    pr_debug("VFS: Dentry cache created (size=%lu)\n", sizeof(struct vfs_dentry));

    return VFS_OK;
}

/**
 * vfs_dentry_cache_exit - Shutdown dentry cache
 *
 * Destroys dentry slab caches.
 */
void vfs_dentry_cache_exit(void)
{
    if (dentry_name_cache) {
        kmem_cache_destroy(dentry_name_cache);
        dentry_name_cache = NULL;
    }

    if (dentry_cache) {
        kmem_cache_destroy(dentry_cache);
        dentry_cache = NULL;
    }

    pr_debug("VFS: Dentry cache destroyed\n");
}

/*===========================================================================*/
/*                         DENTRY ALLOCATION                                 */
/*===========================================================================*/

/**
 * vfs_dentry_alloc - Allocate a new dentry
 * @parent: Parent dentry
 * @name: Entry name
 *
 * Allocates and initializes a new dentry structure.
 *
 * Returns: New dentry or NULL on failure
 */
struct vfs_dentry *vfs_d_alloc(struct vfs_dentry *parent, const char *name)
{
    struct vfs_dentry *dentry;
    size_t name_len;
    char *name_copy;

    if (!name) {
        return NULL;
    }

    name_len = strlen(name);
    if (name_len > VFS_NAME_MAX) {
        return NULL;
    }

    /* Allocate dentry structure */
    dentry = (struct vfs_dentry *)kmem_cache_alloc(dentry_cache);
    if (!dentry) {
        return NULL;
    }

    memset(dentry, 0, sizeof(struct vfs_dentry));

    /* Allocate and copy name */
    name_copy = (char *)kmem_cache_alloc(dentry_name_cache);
    if (!name_copy) {
        kmem_cache_free(dentry_cache, dentry);
        return NULL;
    }

    strncpy(name_copy, name, VFS_NAME_MAX);
    name_copy[VFS_NAME_MAX] = '\0';

    /* Initialize dentry */
    dentry->magic = VFS_MAGIC;
    dentry->d_name = name_copy;
    dentry->d_name_len = (u32)name_len;
    dentry->d_hash = vfs_hash_string(name, name_len);

    /* Set parent */
    dentry->d_parent = parent;
    if (parent) {
        vfs_d_get(parent);
    }

    /* Initialize child list */
    INIT_LIST_HEAD(&dentry->d_children);
    INIT_LIST_HEAD(&dentry->d_child);

    /* Add to parent's children */
    if (parent) {
        spin_lock(&parent->d_lock);
        list_add(&dentry->d_child, &parent->d_children);
        spin_unlock(&parent->d_lock);
    }

    /* Initialize inode reference */
    dentry->d_inode = NULL;
    dentry->d_mount = NULL;

    /* Initialize flags */
    dentry->d_flags = VFS_DENTRY_VALID;
    dentry->d_seq = 0;
    atomic_set(&dentry->d_count, 1);

    /* Initialize lists */
    INIT_LIST_HEAD(&dentry->d_hash_list);
    INIT_LIST_HEAD(&dentry->d_lru);
    dentry->d_last_used = get_time_ms();

    /* Initialize lock */
    spin_lock_init(&dentry->d_lock);

    /* Initialize operations */
    dentry->d_op = NULL;

    /* Initialize superblock reference */
    dentry->d_sb = parent ? parent->d_sb : NULL;

    /* Initialize private data */
    dentry->d_private = NULL;

    atomic_inc(&dentry_stats.allocated);
    atomic_inc(&dentry_stats.total_creates);
    atomic_inc(&vfs_global.stats.total_dentries);

    pr_debug("VFS: Allocated dentry '%s' (parent=%p)\n", name, parent);

    return dentry;
}

/**
 * vfs_dentry_free - Free a dentry
 * @dentry: Dentry to free
 *
 * Releases all resources associated with a dentry.
 */
static void vfs_dentry_free(struct vfs_dentry *dentry)
{
    if (!dentry) {
        return;
    }

    /* Free name */
    if (dentry->d_name) {
        kmem_cache_free(dentry_name_cache, dentry->d_name);
        dentry->d_name = NULL;
    }

    /* Clear magic */
    dentry->magic = 0;

    kmem_cache_free(dentry_cache, dentry);

    atomic_inc(&dentry_stats.freed);
    atomic_inc(&dentry_stats.total_deletes);
    atomic_dec(&vfs_global.stats.total_dentries);
}

/*===========================================================================*/
/*                         DENTRY REFERENCE COUNTING                         */
/*===========================================================================*/

/**
 * vfs_d_get - Get reference to dentry
 * @dentry: Dentry to reference
 *
 * Increments the dentry reference count.
 */
void vfs_d_get(struct vfs_dentry *dentry)
{
    if (!dentry || dentry->magic != VFS_MAGIC) {
        return;
    }

    atomic_inc(&dentry->d_count);
    dentry->d_last_used = get_time_ms();
}

/**
 * vfs_d_put - Release dentry reference
 * @dentry: Dentry to release
 *
 * Decrements the dentry reference count. If count reaches zero,
 * the dentry may be freed.
 */
void vfs_d_put(struct vfs_dentry *dentry)
{
    struct vfs_dentry *parent;
    struct vfs_inode *inode;

    if (!dentry || dentry->magic != VFS_MAGIC) {
        return;
    }

    if (!atomic_dec_and_test(&dentry->d_count)) {
        return;
    }

    pr_debug("VFS: Releasing dentry '%s'\n", dentry->d_name);

    /* Remove from parent's children list */
    parent = dentry->d_parent;
    if (parent) {
        spin_lock(&parent->d_lock);
        if (!list_empty(&dentry->d_child)) {
            list_del(&dentry->d_child);
        }
        spin_unlock(&parent->d_lock);
    }

    /* Release inode reference */
    inode = dentry->d_inode;
    if (inode) {
        /* Call d_iput if available */
        if (dentry->d_op && dentry->d_op->d_iput) {
            dentry->d_op->d_iput(dentry, inode);
        }
        vfs_iput(inode);
        dentry->d_inode = NULL;
    }

    /* Release mount reference */
    if (dentry->d_mount) {
        dentry->d_mount = NULL;
    }

    /* Call d_release if available */
    if (dentry->d_op && dentry->d_op->d_release) {
        dentry->d_op->d_release(dentry);
    }

    /* Remove from hash table */
    spin_lock(&vfs_global.dentry_hash_lock);
    if (!list_empty(&dentry->d_hash_list)) {
        list_del(&dentry->d_hash_list);
    }
    spin_unlock(&vfs_global.dentry_hash_lock);

    /* Remove from LRU */
    spin_lock(&vfs_global.dentry_lock);
    if (!list_empty(&dentry->d_lru)) {
        list_del(&dentry->d_lru);
    }
    spin_unlock(&vfs_global.dentry_lock);

    /* Release parent reference */
    if (parent) {
        vfs_d_put(parent);
    }

    /* Free the dentry */
    vfs_dentry_free(dentry);
}

/*===========================================================================*/
/*                         DENTRY HASH TABLE                                 */
/*===========================================================================*/

/**
 * vfs_d_instantiate - Connect dentry to inode
 * @dentry: Dentry to instantiate
 * @inode: Inode to connect
 *
 * Links a dentry with an inode and adds it to the hash table.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_d_instantiate(struct vfs_dentry *dentry, struct vfs_inode *inode)
{
    u32 bucket;

    if (!dentry || dentry->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    spin_lock(&dentry->d_lock);

    /* Set inode */
    dentry->d_inode = inode;
    if (inode) {
        vfs_iget(inode);
        dentry->d_sb = inode->i_sb;
    }

    /* Clear negative flag */
    dentry->d_flags &= ~VFS_DENTRY_NEGATIVE;

    spin_unlock(&dentry->d_lock);

    /* Add to hash table */
    bucket = vfs_dentry_hash(dentry->d_parent, dentry->d_name);

    spin_lock(&vfs_global.dentry_hash_lock);
    list_add(&dentry->d_hash_list, &vfs_global.dentry_hash[bucket]);
    spin_unlock(&vfs_global.dentry_hash_lock);

    pr_debug("VFS: Instantiated dentry '%s' with inode %p\n",
             dentry->d_name, inode);

    return VFS_OK;
}

/**
 * vfs_d_drop - Drop dentry from cache
 * @dentry: Dentry to drop
 *
 * Removes a dentry from the hash table but keeps it alive.
 */
void vfs_d_drop(struct vfs_dentry *dentry)
{
    if (!dentry || dentry->magic != VFS_MAGIC) {
        return;
    }

    spin_lock(&vfs_global.dentry_hash_lock);

    if (!list_empty(&dentry->d_hash_list)) {
        list_del(&dentry->d_hash_list);
        INIT_LIST_HEAD(&dentry->d_hash_list);
    }

    spin_unlock(&vfs_global.dentry_hash_lock);

    pr_debug("VFS: Dropped dentry '%s' from cache\n", dentry->d_name);
}

/**
 * vfs_d_delete - Mark dentry for deletion
 * @dentry: Dentry to delete
 *
 * Marks a dentry as negative and schedules it for deletion.
 */
void vfs_d_delete(struct vfs_dentry *dentry)
{
    if (!dentry || dentry->magic != VFS_MAGIC) {
        return;
    }

    spin_lock(&dentry->d_lock);

    /* Mark as negative */
    dentry->d_flags |= VFS_DENTRY_NEGATIVE;
    dentry->d_flags &= ~VFS_DENTRY_VALID;

    /* Disconnect from inode */
    if (dentry->d_inode) {
        vfs_iput(dentry->d_inode);
        dentry->d_inode = NULL;
    }

    spin_unlock(&dentry->d_lock);

    /* Call d_delete if available */
    if (dentry->d_op && dentry->d_op->d_delete) {
        dentry->d_op->d_delete(dentry);
    }

    /* Drop from hash */
    vfs_d_drop(dentry);

    pr_debug("VFS: Deleted dentry '%s'\n", dentry->d_name);
}

/*===========================================================================*/
/*                         DENTRY LOOKUP                                     */
/*===========================================================================*/

/**
 * vfs_d_lookup - Lookup dentry by name
 * @name: Name to lookup
 * @parent: Parent dentry
 *
 * Searches the dentry cache for a matching entry.
 *
 * Returns: Dentry or NULL if not found
 */
struct vfs_dentry *vfs_d_lookup(const char *name, struct vfs_dentry *parent)
{
    struct vfs_dentry *dentry;
    u64 hash;
    u32 bucket;

    if (!name || !parent) {
        return NULL;
    }

    hash = vfs_hash_string(name, 0);
    if (parent) {
        hash ^= (u64)(uintptr)parent;
    }
    bucket = (u32)(hash & (VFS_DENTRY_HASH_SIZE - 1));

    dentry_stats.total_lookups++;

    spin_lock(&vfs_global.dentry_hash_lock);

    list_for_each_entry(dentry, &vfs_global.dentry_hash[bucket], d_hash_list) {
        /* Check parent */
        if (dentry->d_parent != parent) {
            continue;
        }

        /* Check hash */
        if (dentry->d_hash != hash) {
            continue;
        }

        /* Check name */
        if (dentry->d_name_len != strlen(name)) {
            continue;
        }

        if (strcmp(dentry->d_name, name) == 0) {
            /* Found - increment refcount */
            atomic_inc(&dentry->d_count);
            dentry->d_last_used = get_time_ms();

            spin_unlock(&vfs_global.dentry_hash_lock);

            vfs_global.stats.cache_hits++;

            pr_debug("VFS: Lookup hit for '%s'\n", name);

            return dentry;
        }
    }

    spin_unlock(&vfs_global.dentry_hash_lock);

    vfs_global.stats.cache_misses++;

    return NULL;
}

/**
 * vfs_d_lookup_hash - Lookup dentry by hash
 * @parent: Parent dentry
 * @name: Name
 * @hash: Precomputed hash
 *
 * Fast lookup using precomputed hash.
 *
 * Returns: Dentry or NULL if not found
 */
struct vfs_dentry *vfs_d_lookup_hash(struct vfs_dentry *parent, const char *name, u64 hash)
{
    struct vfs_dentry *dentry;
    u32 bucket;

    if (!name || !parent) {
        return NULL;
    }

    bucket = (u32)(hash & (VFS_DENTRY_HASH_SIZE - 1));

    dentry_stats.total_lookups++;

    spin_lock(&vfs_global.dentry_hash_lock);

    list_for_each_entry(dentry, &vfs_global.dentry_hash[bucket], d_hash_list) {
        if (dentry->d_parent != parent) {
            continue;
        }

        if (dentry->d_hash != hash) {
            continue;
        }

        if (strcmp(dentry->d_name, name) == 0) {
            atomic_inc(&dentry->d_count);
            dentry->d_last_used = get_time_ms();

            spin_unlock(&vfs_global.dentry_hash_lock);

            vfs_global.stats.cache_hits++;

            return dentry;
        }
    }

    spin_unlock(&vfs_global.dentry_hash_lock);

    vfs_global.stats.cache_misses++;

    return NULL;
}

/*===========================================================================*/
/*                         DENTRY OPERATIONS                                 */
/*===========================================================================*/

/**
 * vfs_d_revalidate - Revalidate a dentry
 * @dentry: Dentry to revalidate
 * @flags: Revalidation flags
 *
 * Checks if a cached dentry is still valid.
 *
 * Returns: VFS_OK if valid, error code otherwise
 */
s32 vfs_d_revalidate(struct vfs_dentry *dentry, u32 flags)
{
    if (!dentry || dentry->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    /* Check if dentry is valid */
    if (dentry->d_flags & VFS_DENTRY_NEGATIVE) {
        return VFS_ENOENT;
    }

    if (!(dentry->d_flags & VFS_DENTRY_VALID)) {
        return VFS_ENOENT;
    }

    /* Call filesystem revalidate if available */
    if (dentry->d_op && dentry->d_op->d_revalidate) {
        return dentry->d_op->d_revalidate(dentry, flags);
    }

    return VFS_OK;
}

/**
 * vfs_d_compare - Compare two dentry names
 * @dentry: Dentry context
 * @name1: First name
 * @name2: Second name
 * @len: Name length
 *
 * Returns: 0 if equal, non-zero otherwise
 */
s32 vfs_d_compare(const struct vfs_dentry *dentry, const char *name1,
                  const char *name2, size_t len)
{
    /* Call filesystem compare if available */
    if (dentry && dentry->d_op && dentry->d_op->d_compare) {
        return dentry->d_op->d_compare(dentry, name1, name2, len);
    }

    /* Default: case-sensitive comparison */
    return strncmp(name1, name2, len);
}

/**
 * vfs_d_hash - Compute dentry hash
 * @dentry: Dentry context
 * @hash: Output hash
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_d_hash(const struct vfs_dentry *dentry, u64 *hash)
{
    if (!hash) {
        return VFS_EINVAL;
    }

    /* Call filesystem hash if available */
    if (dentry && dentry->d_op && dentry->d_op->d_hash) {
        return dentry->d_op->d_hash(dentry, hash);
    }

    /* Default hash */
    *hash = 0;

    return VFS_OK;
}

/*===========================================================================*/
/*                         DENTRY LRU MANAGEMENT                             */
/*===========================================================================*/

/**
 * vfs_dentry_add_lru - Add dentry to LRU list
 * @dentry: Dentry to add
 */
static void vfs_dentry_add_lru(struct vfs_dentry *dentry)
{
    spin_lock(&vfs_global.dentry_lock);

    if (list_empty(&dentry->d_lru)) {
        list_add_tail(&dentry->d_lru, &vfs_global.dentry_lru);
    }

    spin_unlock(&vfs_global.dentry_lock);
}

/**
 * vfs_dentry_remove_lru - Remove dentry from LRU list
 * @dentry: Dentry to remove
 */
static void vfs_dentry_remove_lru(struct vfs_dentry *dentry)
{
    spin_lock(&vfs_global.dentry_lock);

    if (!list_empty(&dentry->d_lru)) {
        list_del(&dentry->d_lru);
        INIT_LIST_HEAD(&dentry->d_lru);
    }

    spin_unlock(&vfs_global.dentry_lock);
}

/**
 * vfs_dentry_shrink - Shrink dentry cache
 *
 * Reclaims unused dentries from the cache.
 *
 * Returns: Number of dentries reclaimed
 */
u32 vfs_dentry_shrink(void)
{
    struct vfs_dentry *dentry;
    struct vfs_dentry *next;
    u32 count = 0;
    u64 now;

    pr_debug("VFS: Shrinking dentry cache...\n");

    now = get_time_ms();

    spin_lock(&vfs_global.dentry_lock);

    /* Scan LRU list for old dentries */
    list_for_each_entry_safe(dentry, next, &vfs_global.dentry_lru, d_lru) {
        /* Check if dentry is unused */
        if (atomic_read(&dentry->d_count) > 0) {
            continue;
        }

        /* Check age (older than 30 seconds) */
        if (now - dentry->d_last_used < 30000) {
            continue;
        }

        /* Get reference to prevent race */
        atomic_inc(&dentry->d_count);
        list_del_init(&dentry->d_lru);

        spin_unlock(&vfs_global.dentry_lock);

        /* Delete the dentry */
        vfs_d_delete(dentry);
        vfs_d_put(dentry);
        count++;

        /* Limit reclamation per pass */
        if (count >= 128) {
            return count;
        }

        spin_lock(&vfs_global.dentry_lock);
    }

    spin_unlock(&vfs_global.dentry_lock);

    pr_debug("VFS: Reclaimed %u dentries\n", count);

    return count;
}

/*===========================================================================*/
/*                         DENTRY CHILD MANAGEMENT                           */
/*===========================================================================*/

/**
 * vfs_d_add_child - Add child dentry to parent
 * @parent: Parent dentry
 * @child: Child dentry
 */
void vfs_d_add_child(struct vfs_dentry *parent, struct vfs_dentry *child)
{
    if (!parent || !child) {
        return;
    }

    spin_lock(&parent->d_lock);

    if (list_empty(&child->d_child)) {
        list_add(&child->d_child, &parent->d_children);
    }

    spin_unlock(&parent->d_lock);
}

/**
 * vfs_d_remove_child - Remove child dentry from parent
 * @child: Child dentry to remove
 */
void vfs_d_remove_child(struct vfs_dentry *child)
{
    struct vfs_dentry *parent;

    if (!child) {
        return;
    }

    parent = child->d_parent;
    if (!parent) {
        return;
    }

    spin_lock(&parent->d_lock);

    if (!list_empty(&child->d_child)) {
        list_del(&child->d_child);
        INIT_LIST_HEAD(&child->d_child);
    }

    spin_unlock(&parent->d_lock);
}

/**
 * vfs_d_find_child - Find child dentry by name
 * @parent: Parent dentry
 * @name: Child name
 *
 * Returns: Child dentry or NULL
 */
struct vfs_dentry *vfs_d_find_child(struct vfs_dentry *parent, const char *name)
{
    struct vfs_dentry *child;

    if (!parent || !name) {
        return NULL;
    }

    spin_lock(&parent->d_lock);

    list_for_each_entry(child, &parent->d_children, d_child) {
        if (strcmp(child->d_name, name) == 0) {
            vfs_d_get(child);
            spin_unlock(&parent->d_lock);
            return child;
        }
    }

    spin_unlock(&parent->d_lock);

    return NULL;
}

/*===========================================================================*/
/*                         DENTRY STATISTICS                                 */
/*===========================================================================*/

/**
 * vfs_dentry_stats_print - Print dentry statistics
 */
void vfs_dentry_stats_print(void)
{
    printk("\n=== Dentry Statistics ===\n");
    printk("Allocated:       %d\n", atomic_read(&dentry_stats.allocated));
    printk("Freed:           %d\n", atomic_read(&dentry_stats.freed));
    printk("Active:          %d\n", atomic_read(&dentry_stats.allocated) - atomic_read(&dentry_stats.freed));
    printk("Negative:        %d\n", atomic_read(&dentry_stats.negative));
    printk("Referenced:      %d\n", atomic_read(&dentry_stats.referenced));
    printk("Total Lookups:   %llu\n", (unsigned long long)dentry_stats.total_lookups);
    printk("Total Creates:   %llu\n", (unsigned long long)dentry_stats.total_creates);
    printk("Total Deletes:   %llu\n", (unsigned long long)dentry_stats.total_deletes);
}

/**
 * vfs_dentry_dump - Dump all dentries
 */
void vfs_dentry_dump(void)
{
    struct vfs_dentry *dentry;
    u32 i;
    u32 count = 0;

    printk("\n=== Dentry Dump ===\n");

    for (i = 0; i < VFS_DENTRY_HASH_SIZE; i++) {
        spin_lock(&vfs_global.dentry_hash_lock);

        list_for_each_entry(dentry, &vfs_global.dentry_hash[i], d_hash_list) {
            printk("Dentry '%s': parent=%p, inode=%p, flags=0x%02X, count=%d\n",
                   dentry->d_name,
                   dentry->d_parent,
                   dentry->d_inode,
                   dentry->d_flags,
                   atomic_read(&dentry->d_count));
            count++;
        }

        spin_unlock(&vfs_global.dentry_hash_lock);
    }

    printk("Total: %u dentries\n", count);
}
