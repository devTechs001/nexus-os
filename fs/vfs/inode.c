/*
 * NEXUS OS - Virtual File System
 * fs/vfs/inode.c
 *
 * Inode Management - VFS inode allocation, caching, and lifecycle
 */

#include "vfs.h"

/*===========================================================================*/
/*                         INODE CACHE                                       */
/*===========================================================================*/

/* Inode cache slab */
static void *inode_cache = NULL;

/* Inode statistics */
static struct {
    atomic_t allocated;
    atomic_t freed;
    atomic_t dirty;
    atomic_t unused;
    u64 total_reads;
    u64 total_writes;
} inode_stats;

/*===========================================================================*/
/*                         INODE CACHE INITIALIZATION                        */
/*===========================================================================*/

/**
 * vfs_inode_cache_init - Initialize inode cache
 *
 * Creates the slab cache for inode allocations.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_inode_cache_init(void)
{
    inode_cache = kmem_cache_create("vfs_inode", sizeof(struct vfs_inode),
                                     __alignof__(struct vfs_inode), 0);
    if (!inode_cache) {
        pr_err("VFS: Failed to create inode cache\n");
        return VFS_ENOMEM;
    }

    atomic_set(&inode_stats.allocated, 0);
    atomic_set(&inode_stats.freed, 0);
    atomic_set(&inode_stats.dirty, 0);
    atomic_set(&inode_stats.unused, 0);
    inode_stats.total_reads = 0;
    inode_stats.total_writes = 0;

    pr_debug("VFS: Inode cache created (size=%lu)\n", sizeof(struct vfs_inode));

    return VFS_OK;
}

/**
 * vfs_inode_cache_exit - Shutdown inode cache
 *
 * Destroys the inode slab cache.
 */
void vfs_inode_cache_exit(void)
{
    if (inode_cache) {
        kmem_cache_destroy(inode_cache);
        inode_cache = NULL;
    }

    pr_debug("VFS: Inode cache destroyed\n");
}

/*===========================================================================*/
/*                         INODE ALLOCATION                                  */
/*===========================================================================*/

/**
 * vfs_inode_alloc - Allocate a new inode structure
 *
 * Allocates a new inode from the slab cache and initializes it.
 *
 * Returns: New inode or NULL on failure
 */
static struct vfs_inode *vfs_inode_alloc(void)
{
    struct vfs_inode *inode;

    inode = (struct vfs_inode *)kmem_cache_alloc(inode_cache);
    if (!inode) {
        return NULL;
    }

    memset(inode, 0, sizeof(struct vfs_inode));

    /* Initialize basic fields */
    inode->magic = VFS_MAGIC;
    inode->ino = 0;
    inode->dev = 0;
    inode->generation = 0;

    /* Initialize type and mode */
    inode->type = VFS_TYPE_UNKNOWN;
    inode->mode = 0;
    inode->uid = 0;
    inode->gid = 0;

    /* Initialize size and links */
    inode->size = 0;
    inode->nlink = 1;
    inode->nblocks = 0;
    inode->blocks = 0;

    /* Initialize timestamps */
    inode->atime = 0;
    inode->mtime = 0;
    inode->ctime = 0;
    inode->btime = 0;

    /* Initialize device info */
    inode->rdev = 0;

    /* Initialize flags and state */
    inode->flags = VFS_INODE_NEW;
    inode->state = 0;
    atomic_set(&inode->refcount, 1);
    atomic_set(&inode->i_count, 1);

    /* Initialize lists */
    INIT_LIST_HEAD(&inode->hash_list);
    INIT_LIST_HEAD(&inode->dirty_list);
    INIT_LIST_HEAD(&inode->unused_list);
    inode->last_used = get_time_ms();

    /* Initialize locks */
    spin_lock_init(&inode->lock);
    spin_lock_init(&inode->i_lock);

    /* Initialize operations */
    inode->i_op = NULL;
    inode->i_fop = NULL;

    /* Initialize references */
    inode->i_sb = NULL;
    inode->i_private = NULL;
    inode->i_cache = NULL;

    atomic_inc(&inode_stats.allocated);
    atomic_inc(&vfs_global.stats.total_inodes);

    return inode;
}

/**
 * vfs_inode_free - Free an inode structure
 * @inode: Inode to free
 *
 * Returns the inode to the slab cache.
 */
static void vfs_inode_free(struct vfs_inode *inode)
{
    if (!inode) {
        return;
    }

    /* Verify inode is not in use */
    if (atomic_read(&inode->i_count) > 0) {
        pr_err("VFS: Freeing inode with active references (count=%d)\n",
               atomic_read(&inode->i_count));
    }

    /* Clear magic to prevent use-after-free */
    inode->magic = 0;

    /* Free page cache if present */
    if (inode->i_cache) {
        /* TODO: Free page cache */
        inode->i_cache = NULL;
    }

    kmem_cache_free(inode_cache, inode);

    atomic_inc(&inode_stats.freed);
    atomic_dec(&vfs_global.stats.total_inodes);
}

/*===========================================================================*/
/*                         INODE LIFECYCLE                                   */
/*===========================================================================*/

/**
 * vfs_alloc_inode - Allocate inode for a superblock
 * @sb: Superblock
 *
 * Allocates a new inode and associates it with the superblock.
 *
 * Returns: New inode or NULL on failure
 */
struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb)
{
    struct vfs_inode *inode;

    if (!sb) {
        return NULL;
    }

    inode = vfs_inode_alloc();
    if (!inode) {
        return NULL;
    }

    /* Associate with superblock */
    inode->i_sb = sb;
    inode->dev = sb->s_dev;

    /* Call filesystem alloc_inode if available */
    if (sb->s_op && sb->s_op->alloc_inode) {
        s32 ret = sb->s_op->alloc_inode(sb, inode);
        if (ret != VFS_OK) {
            vfs_inode_free(inode);
            return NULL;
        }
    }

    pr_debug("VFS: Allocated inode %llu for sb %p\n",
             (unsigned long long)inode->ino, sb);

    return inode;
}

/**
 * vfs_destroy_inode - Destroy an inode
 * @inode: Inode to destroy
 *
 * Frees all resources associated with an inode.
 */
void vfs_destroy_inode(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;

    if (!inode) {
        return;
    }

    sb = inode->i_sb;

    pr_debug("VFS: Destroying inode %llu\n", (unsigned long long)inode->ino);

    /* Call filesystem destroy_inode if available */
    if (sb && sb->s_op && sb->s_op->destroy_inode) {
        sb->s_op->destroy_inode(inode);
    }

    /* Remove from superblock lists */
    if (sb) {
        spin_lock(&sb->s_lock);
        if (!list_empty(&inode->hash_list)) {
            list_del(&inode->hash_list);
        }
        if (!list_empty(&inode->dirty_list)) {
            list_del(&inode->dirty_list);
            atomic_dec(&inode_stats.dirty);
        }
        if (!list_empty(&inode->unused_list)) {
            list_del(&inode->unused_list);
            atomic_dec(&inode_stats.unused);
        }
        spin_unlock(&sb->s_lock);
    }

    /* Free the inode */
    vfs_inode_free(inode);
}

/**
 * vfs_iget - Get reference to inode
 * @inode: Inode to reference
 *
 * Increments the inode reference count.
 */
void vfs_iget(struct vfs_inode *inode)
{
    if (!inode || inode->magic != VFS_MAGIC) {
        return;
    }

    atomic_inc(&inode->i_count);
    atomic_inc(&inode->refcount);
    inode->last_used = get_time_ms();
}

/**
 * vfs_iput - Release inode reference
 * @inode: Inode to release
 *
 * Decrements the inode reference count. If count reaches zero,
 * the inode may be evicted.
 */
void vfs_iput(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;

    if (!inode || inode->magic != VFS_MAGIC) {
        return;
    }

    sb = inode->i_sb;

    if (atomic_dec_and_test(&inode->i_count)) {
        /* No more users - check if we can evict */
        if (atomic_read(&inode->refcount) <= 1) {
            /* Can evict */
            if (inode->flags & VFS_INODE_DIRTY) {
                /* Write back first */
                vfs_write_inode(inode);
            }

            /* Call evict if available */
            if (sb && sb->s_op && sb->s_op->evict_inode) {
                sb->s_op->evict_inode(inode);
            }

            vfs_destroy_inode(inode);
            return;
        }

        /* Move to unused list */
        if (sb) {
            spin_lock(&sb->s_lock);
            list_del(&inode->hash_list);
            list_add(&inode->unused_list, &sb->s_unused_inodes);
            atomic_inc(&inode_stats.unused);
            spin_unlock(&sb->s_lock);
        }
    }

    atomic_dec(&inode->refcount);
}

/*===========================================================================*/
/*                         INODE HASH TABLE                                  */
/*===========================================================================*/

/**
 * vfs_iget_locked - Get or create locked inode
 * @sb: Superblock
 * @ino: Inode number
 *
 * Looks up an inode in the hash table. If not found, creates a new one.
 * The returned inode is locked and referenced.
 *
 * Returns: Inode or NULL on failure
 */
struct vfs_inode *vfs_iget_locked(struct vfs_superblock *sb, u64 ino)
{
    struct vfs_inode *inode;
    u32 bucket;

    if (!sb) {
        return NULL;
    }

    bucket = vfs_inode_hash(sb->s_dev, ino);

    spin_lock(&vfs_global.inode_hash_lock);

    /* Search hash table */
    list_for_each_entry(inode, &vfs_global.inode_hash[bucket], hash_list) {
        if (inode->i_sb != sb) {
            continue;
        }
        if (inode->ino != ino) {
            continue;
        }

        /* Found - increment refcount */
        vfs_iget(inode);
        spin_unlock(&vfs_global.inode_hash_lock);

        return inode;
    }

    spin_unlock(&vfs_global.inode_hash_lock);

    /* Not found - allocate new inode */
    inode = vfs_alloc_inode(sb);
    if (!inode) {
        return NULL;
    }

    inode->ino = ino;

    /* Read inode from disk */
    if (sb->s_op && sb->s_op->read_inode) {
        sb->s_op->read_inode(inode);
    }

    /* Add to hash table */
    spin_lock(&vfs_global.inode_hash_lock);

    /* Double-check it wasn't added while we were allocating */
    list_for_each_entry(inode, &vfs_global.inode_hash[bucket], hash_list) {
        if (inode->i_sb == sb && inode->ino == ino) {
            vfs_iget(inode);
            spin_unlock(&vfs_global.inode_hash_lock);
            vfs_destroy_inode(inode);
            return inode;
        }
    }

    /* Add new inode to hash */
    list_add(&inode->hash_list, &vfs_global.inode_hash[bucket]);

    spin_unlock(&vfs_global.inode_hash_lock);

    return inode;
}

/**
 * vfs_find_inode - Find inode by number
 * @sb: Superblock
 * @ino: Inode number
 *
 * Returns: Inode or NULL if not found
 */
static struct vfs_inode *vfs_find_inode(struct vfs_superblock *sb, u64 ino)
{
    struct vfs_inode *inode;
    u32 bucket;

    if (!sb) {
        return NULL;
    }

    bucket = vfs_inode_hash(sb->s_dev, ino);

    spin_lock(&vfs_global.inode_hash_lock);

    list_for_each_entry(inode, &vfs_global.inode_hash[bucket], hash_list) {
        if (inode->i_sb == sb && inode->ino == ino) {
            vfs_iget(inode);
            spin_unlock(&vfs_global.inode_hash_lock);
            return inode;
        }
    }

    spin_unlock(&vfs_global.inode_hash_lock);

    return NULL;
}

/*===========================================================================*/
/*                         INODE OPERATIONS                                  */
/*===========================================================================*/

/**
 * vfs_read_inode - Read inode from disk
 * @inode: Inode to read
 *
 * Populates inode fields from the underlying filesystem.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_read_inode(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;

    if (!inode || inode->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    sb = inode->i_sb;

    if (!sb) {
        return VFS_ENODEV;
    }

    spin_lock(&inode->lock);

    /* Call filesystem read_inode */
    if (sb->s_op && sb->s_op->read_inode) {
        sb->s_op->read_inode(inode);
    } else if (inode->i_op && inode->i_op->read_inode) {
        inode->i_op->read_inode(inode);
    }

    inode->flags |= VFS_INODE_VALID;
    inode->last_used = get_time_ms();

    spin_unlock(&inode->lock);

    inode_stats.total_reads++;

    return VFS_OK;
}

/**
 * vfs_write_inode - Write inode to disk
 * @inode: Inode to write
 *
 * Flushes inode changes to the underlying filesystem.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_write_inode(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;

    if (!inode || inode->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    if (!(inode->flags & VFS_INODE_DIRTY)) {
        return VFS_OK;
    }

    sb = inode->i_sb;

    spin_lock(&inode->lock);

    /* Call filesystem write_inode */
    if (sb && sb->s_op && sb->s_op->write_inode) {
        sb->s_op->write_inode(inode, 1);
    } else if (inode->i_op && inode->i_op->write_inode) {
        inode->i_op->write_inode(inode);
    }

    inode->flags &= ~VFS_INODE_DIRTY;

    spin_unlock(&inode->lock);

    /* Remove from dirty list */
    if (sb) {
        spin_lock(&sb->s_lock);
        if (!list_empty(&inode->dirty_list)) {
            list_del(&inode->dirty_list);
            atomic_dec(&inode_stats.dirty);
        }
        spin_unlock(&sb->s_lock);
    }

    inode_stats.total_writes++;

    return VFS_OK;
}

/**
 * vfs_dirty_inode - Mark inode as dirty
 * @inode: Inode to mark dirty
 *
 * Marks an inode as modified and adds it to the dirty list.
 */
void vfs_dirty_inode(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;

    if (!inode || inode->magic != VFS_MAGIC) {
        return;
    }

    spin_lock(&inode->lock);

    if (inode->flags & VFS_INODE_DIRTY) {
        spin_unlock(&inode->lock);
        return;
    }

    inode->flags |= VFS_INODE_DIRTY;

    sb = inode->i_sb;

    /* Add to dirty list */
    if (sb) {
        spin_lock(&sb->s_lock);
        list_add(&inode->dirty_list, &sb->s_dirty_inodes);
        atomic_inc(&inode_stats.dirty);
        spin_unlock(&sb->s_lock);
    }

    /* Call filesystem dirty_inode if available */
    if (inode->i_op && inode->i_op->dirty_inode) {
        inode->i_op->dirty_inode(inode);
    }

    spin_unlock(&inode->lock);
}

/**
 * vfs_evict_inode - Evict an inode
 * @inode: Inode to evict
 *
 * Removes an inode from all caches and frees it.
 */
void vfs_evict_inode(struct vfs_inode *inode)
{
    struct vfs_superblock *sb;
    u32 bucket;

    if (!inode || inode->magic != VFS_MAGIC) {
        return;
    }

    sb = inode->i_sb;

    pr_debug("VFS: Evicting inode %llu\n", (unsigned long long)inode->ino);

    /* Write back if dirty */
    if (inode->flags & VFS_INODE_DIRTY) {
        vfs_write_inode(inode);
    }

    /* Call filesystem evict */
    if (sb && sb->s_op && sb->s_op->evict_inode) {
        sb->s_op->evict_inode(inode);
    }

    /* Remove from hash table */
    bucket = vfs_inode_hash(inode->dev, inode->ino);

    spin_lock(&vfs_global.inode_hash_lock);

    if (!list_empty(&inode->hash_list)) {
        list_del(&inode->hash_list);
    }

    spin_unlock(&vfs_global.inode_hash_lock);

    /* Remove from other lists */
    if (sb) {
        spin_lock(&sb->s_lock);

        if (!list_empty(&inode->dirty_list)) {
            list_del(&inode->dirty_list);
            atomic_dec(&inode_stats.dirty);
        }

        if (!list_empty(&inode->unused_list)) {
            list_del(&inode->unused_list);
            atomic_dec(&inode_stats.unused);
        }

        spin_unlock(&sb->s_lock);
    }

    /* Free the inode */
    vfs_destroy_inode(inode);
}

/**
 * vfs_evict_inodes - Evict all inodes for a superblock
 * @sb: Superblock
 *
 * Evicts all inodes belonging to a filesystem.
 */
void vfs_evict_inodes(struct vfs_superblock *sb)
{
    struct vfs_inode *inode;
    struct vfs_inode *next;
    u32 i;

    if (!sb) {
        return;
    }

    pr_debug("VFS: Evicting all inodes for sb %p\n", sb);

    /* Evict from all hash buckets */
    for (i = 0; i < VFS_INODE_HASH_SIZE; i++) {
        spin_lock(&vfs_global.inode_hash_lock);

        list_for_each_entry_safe(inode, next, &vfs_global.inode_hash[i], hash_list) {
            if (inode->i_sb == sb) {
                /* Get reference to prevent premature free */
                vfs_iget(inode);
                spin_unlock(&vfs_global.inode_hash_lock);

                vfs_evict_inode(inode);

                spin_lock(&vfs_global.inode_hash_lock);
            }
        }

        spin_unlock(&vfs_global.inode_hash_lock);
    }
}

/*===========================================================================*/
/*                         INODE ATTRIBUTE OPERATIONS                        */
/*===========================================================================*/

/**
 * vfs_inode_setattr - Set inode attributes
 * @inode: Inode to modify
 * @mode: New mode (or 0 to keep)
 * @uid: New UID (or -1 to keep)
 * @gid: New GID (or -1 to keep)
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_inode_setattr(struct vfs_inode *inode, mode_t mode, uid_t uid, gid_t gid)
{
    if (!inode || inode->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    spin_lock(&inode->lock);

    if (mode != 0) {
        inode->mode = mode;
    }

    if (uid != (uid_t)-1) {
        inode->uid = uid;
    }

    if (gid != (gid_t)-1) {
        inode->gid = gid;
    }

    inode->ctime = get_time_ms() / 1000;

    spin_unlock(&inode->lock);

    /* Call filesystem setattr if available */
    if (inode->i_op && inode->i_op->setattr) {
        return inode->i_op->setattr(inode, mode, uid, gid);
    }

    vfs_dirty_inode(inode);

    return VFS_OK;
}

/**
 * vfs_inode_getattr - Get inode attributes
 * @inode: Inode to query
 * @mode: Output mode (or NULL)
 * @uid: Output UID (or NULL)
 * @gid: Output GID (or NULL)
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_inode_getattr(struct vfs_inode *inode, mode_t *mode, uid_t *uid, gid_t *gid)
{
    if (!inode || inode->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    spin_lock(&inode->lock);

    if (mode) {
        *mode = inode->mode;
    }

    if (uid) {
        *uid = inode->uid;
    }

    if (gid) {
        *gid = inode->gid;
    }

    spin_unlock(&inode->lock);

    /* Call filesystem getattr if available */
    if (inode->i_op && inode->i_op->getattr) {
        return inode->i_op->getattr(inode, mode, uid, gid);
    }

    return VFS_OK;
}

/*===========================================================================*/
/*                         INODE CACHE SHRINKING                             */
/*===========================================================================*/

/**
 * vfs_inode_shrink - Shrink inode cache
 *
 * Reclaims unused inodes from the cache.
 *
 * Returns: Number of inodes reclaimed
 */
u32 vfs_inode_shrink(void)
{
    struct vfs_inode *inode;
    struct vfs_inode *next;
    u32 count = 0;
    u32 i;

    pr_debug("VFS: Shrinking inode cache...\n");

    /* Scan hash buckets for unused inodes */
    for (i = 0; i < VFS_INODE_HASH_SIZE; i++) {
        spin_lock(&vfs_global.inode_hash_lock);

        list_for_each_entry_safe(inode, next, &vfs_global.inode_hash[i], hash_list) {
            /* Check if inode is unused */
            if (atomic_read(&inode->i_count) == 0) {
                /* Get reference to prevent race */
                vfs_iget(inode);
                spin_unlock(&vfs_global.inode_hash_lock);

                /* Evict the inode */
                vfs_evict_inode(inode);
                count++;

                /* Limit reclamation per pass */
                if (count >= 64) {
                    return count;
                }

                spin_lock(&vfs_global.inode_hash_lock);
            }
        }

        spin_unlock(&vfs_global.inode_hash_lock);
    }

    pr_debug("VFS: Reclaimed %u inodes\n", count);

    return count;
}

/*===========================================================================*/
/*                         INODE STATISTICS                                  */
/*===========================================================================*/

/**
 * vfs_inode_stats_print - Print inode statistics
 */
void vfs_inode_stats_print(void)
{
    printk("\n=== Inode Statistics ===\n");
    printk("Allocated:       %d\n", atomic_read(&inode_stats.allocated));
    printk("Freed:           %d\n", atomic_read(&inode_stats.freed));
    printk("Active:          %d\n", atomic_read(&inode_stats.allocated) - atomic_read(&inode_stats.freed));
    printk("Dirty:           %d\n", atomic_read(&inode_stats.dirty));
    printk("Unused:          %d\n", atomic_read(&inode_stats.unused));
    printk("Total Reads:     %llu\n", (unsigned long long)inode_stats.total_reads);
    printk("Total Writes:    %llu\n", (unsigned long long)inode_stats.total_writes);
}

/**
 * vfs_inode_dump - Dump all inodes
 */
void vfs_inode_dump(void)
{
    struct vfs_inode *inode;
    u32 i;
    u32 count = 0;

    printk("\n=== Inode Dump ===\n");

    for (i = 0; i < VFS_INODE_HASH_SIZE; i++) {
        spin_lock(&vfs_global.inode_hash_lock);

        list_for_each_entry(inode, &vfs_global.inode_hash[i], hash_list) {
            printk("Inode %5llu: dev=%u, type=%u, mode=%04o, nlink=%u, size=%llu\n",
                   (unsigned long long)inode->ino,
                   (u32)inode->dev,
                   inode->type,
                   inode->mode,
                   inode->nlink,
                   (unsigned long long)inode->size);
            count++;
        }

        spin_unlock(&vfs_global.inode_hash_lock);
    }

    printk("Total: %u inodes\n", count);
}
