/*
 * NEXUS OS - NexFS Filesystem
 * fs/nexfs/nexfs_core.c
 *
 * NexFS Core Implementation - Filesystem initialization and core operations
 */

#include "nexfs.h"

/*===========================================================================*/
/*                         NEXFS GLOBAL STATE                                */
/*===========================================================================*/

static struct {
    bool initialized;
    u32 mount_count;
    atomic_t total_inodes;
    atomic_t total_blocks;
    u64 total_reads;
    u64 total_writes;
} nexfs_global = {
    .initialized = false,
    .mount_count = 0,
};

/* CRC32C lookup table */
static u32 nexfs_crc32c_table[256];
static bool crc_table_initialized = false;

/*===========================================================================*/
/*                         CHECKSUM FUNCTIONS                                */
/*===========================================================================*/

/**
 * nexfs_init_crc32c_table - Initialize CRC32C lookup table
 *
 * Precomputes CRC32C table for fast checksum calculation.
 */
static void nexfs_init_crc32c_table(void)
{
    u32 crc;
    u32 i;
    u32 j;
    u32 poly = 0x82F63B78;  /* CRC32C polynomial */

    if (crc_table_initialized) {
        return;
    }

    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
        nexfs_crc32c_table[i] = crc;
    }

    crc_table_initialized = true;
}

/**
 * nexfs_crc32c - Calculate CRC32C checksum
 * @crc: Initial CRC value
 * @buf: Buffer to checksum
 * @len: Buffer length
 *
 * Returns: CRC32C checksum
 */
u32 nexfs_crc32c(u32 crc, const void *buf, size_t len)
{
    const u8 *p = (const u8 *)buf;
    size_t i;

    if (!crc_table_initialized) {
        nexfs_init_crc32c_table();
    }

    crc ^= 0xFFFFFFFF;

    for (i = 0; i < len; i++) {
        crc = nexfs_crc32c_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/**
 * nexfs_superblock_checksum - Calculate superblock checksum
 * @sb: Superblock to checksum
 *
 * Returns: Checksum value
 */
u32 nexfs_superblock_checksum(struct nexfs_superblock_raw *sb)
{
    u32 checksum;
    u32 saved_checksum;

    saved_checksum = sb->s_checksum;
    sb->s_checksum = 0;

    checksum = nexfs_crc32c(0, sb, sizeof(struct nexfs_superblock_raw));

    sb->s_checksum = saved_checksum;

    return checksum;
}

/**
 * nexfs_inode_checksum - Calculate inode checksum
 * @inode: Inode to checksum
 *
 * Returns: Checksum value
 */
u32 nexfs_inode_checksum(struct nexfs_inode_raw *inode)
{
    u32 checksum;
    u16 saved_checksum;

    saved_checksum = inode->i_checksum;
    inode->i_checksum = 0;

    checksum = nexfs_crc32c(0, inode, sizeof(struct nexfs_inode_raw));

    inode->i_checksum = saved_checksum;

    return checksum;
}

/**
 * nexfs_block_checksum - Calculate block checksum
 * @block: Block data
 * @blocknr: Block number
 *
 * Returns: Checksum value
 */
u32 nexfs_block_checksum(void *block, u32 blocknr)
{
    u32 crc;

    crc = nexfs_crc32c(blocknr, block, NEXFS_BLOCK_SIZE);

    return crc;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * nexfs_group_index - Get group index for a block
 * @sb: Superblock
 * @block: Block number
 *
 * Returns: Group index
 */
u32 nexfs_group_index(struct nexfs_superblock *sb, u64 block)
{
    return (u32)(block / sb->raw.s_blocks_per_group);
}

/**
 * nexfs_inode_to_block - Convert inode number to block number
 * @sb: Superblock
 * @ino: Inode number
 *
 * Returns: Block number containing inode
 */
u64 nexfs_inode_to_block(struct nexfs_superblock *sb, u64 ino)
{
    u32 group;
    u64 block;

    group = (u32)((ino - 1) / sb->raw.s_inodes_per_group);
    block = sb->groups[group].bg_inode_table;
    block += ((ino - 1) % sb->raw.s_inodes_per_group) / NEXFS_INODES_PER_BLOCK;

    return block;
}

/**
 * nexfs_block_to_inode - Convert block number to inode number
 * @sb: Superblock
 * @block: Block number
 *
 * Returns: Inode number or 0 if not an inode block
 */
u64 nexfs_block_to_inode(struct nexfs_superblock *sb, u64 block)
{
    u32 group;
    u32 i;

    for (group = 0; group < sb->groups_count; group++) {
        u64 itable_start = sb->groups[group].bg_inode_table;
        u64 itable_end = itable_start +
            (sb->raw.s_inodes_per_group / NEXFS_INODES_PER_BLOCK);

        if (block >= itable_start && block < itable_end) {
            u64 offset = block - itable_start;
            u64 ino = group * sb->raw.s_inodes_per_group;
            ino += offset * NEXFS_INODES_PER_BLOCK;
            ino += 1;
            return ino;
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         SUPERBLOCK OPERATIONS                             */
/*===========================================================================*/

/**
 * nexfs_read_superblock - Read superblock from disk
 * @sb: Superblock structure
 *
 * Reads and validates the filesystem superblock.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_read_superblock(struct nexfs_superblock *sb)
{
    struct nexfs_superblock_raw *raw;
    u32 checksum;
    s32 ret;

    if (!sb || !sb->bdev) {
        return VFS_EINVAL;
    }

    raw = &sb->raw;

    /* Read superblock from disk */
    /* TODO: Implement actual block device read */
    /* ret = block_device_read(sb->bdev, NEXFS_SUPERBLOCK_OFFSET / NEXFS_BLOCK_SIZE, raw, NEXFS_BLOCK_SIZE); */
    ret = VFS_OK;

    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to read superblock\n");
        return VFS_EIO;
    }

    /* Validate magic */
    if (raw->s_magic != NEXFS_MAGIC) {
        pr_err("NexFS: Invalid magic number (got 0x%08X, expected 0x%08X)\n",
               raw->s_magic, NEXFS_MAGIC);
        return VFS_EINVAL;
    }

    /* Validate checksum */
    checksum = nexfs_superblock_checksum(raw);
    if (checksum != raw->s_checksum) {
        pr_err("NexFS: Superblock checksum mismatch\n");
        return VFS_EIO;
    }

    /* Validate block size */
    if (raw->s_log_block_size < 10 || raw->s_log_block_size > 16) {
        pr_err("NexFS: Invalid block size\n");
        return VFS_EINVAL;
    }

    pr_info("NexFS: Superblock loaded (blocks=%llu, inodes=%llu)\n",
            (unsigned long long)raw->s_blocks_count,
            (unsigned long long)raw->s_inodes_count);

    return VFS_OK;
}

/**
 * nexfs_write_superblock - Write superblock to disk
 * @sb: Superblock structure
 *
 * Writes the superblock with updated checksum.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_write_superblock(struct nexfs_superblock *sb)
{
    struct nexfs_superblock_raw *raw;
    s32 ret;

    if (!sb || !sb->bdev) {
        return VFS_EINVAL;
    }

    raw = &sb->raw;

    /* Update checksum */
    raw->s_checksum = nexfs_superblock_checksum(raw);

    /* Update write time */
    raw->s_write_time = (u32)(get_time_ms() / 1000);

    /* Write to disk */
    /* TODO: Implement actual block device write */
    /* ret = block_device_write(sb->bdev, NEXFS_SUPERBLOCK_OFFSET / NEXFS_BLOCK_SIZE, raw, NEXFS_BLOCK_SIZE); */
    ret = VFS_OK;

    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to write superblock\n");
        return VFS_EIO;
    }

    return VFS_OK;
}

/**
 * nexfs_read_group_descriptors - Read group descriptors
 * @sb: Superblock
 *
 * Reads all group descriptors from disk.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_read_group_descriptors(struct nexfs_superblock *sb)
{
    u32 i;
    u64 block;
    s32 ret;

    if (!sb) {
        return VFS_EINVAL;
    }

    /* Allocate group descriptor array */
    sb->groups = (struct nexfs_group_desc *)kzalloc(
        sizeof(struct nexfs_group_desc) * sb->groups_count);

    if (!sb->groups) {
        return VFS_ENOMEM;
    }

    /* Read group descriptors */
    block = (NEXFS_SUPERBLOCK_OFFSET + NEXFS_BLOCK_SIZE) / NEXFS_BLOCK_SIZE;

    for (i = 0; i < sb->groups_count; i++) {
        /* TODO: Implement actual block device read */
        /* ret = block_device_read(sb->bdev, block + i, &sb->groups[i], sizeof(struct nexfs_group_desc)); */

        /* Validate checksum */
        /* u16 csum = nexfs_crc32c(0, &sb->groups[i], sizeof(struct nexfs_group_desc) - 2); */
        /* if (csum != sb->groups[i].bg_checksum) { ... } */
    }

    pr_debug("NexFS: Loaded %u group descriptors\n", sb->groups_count);

    return VFS_OK;
}

/**
 * nexfs_write_group_descriptors - Write group descriptors
 * @sb: Superblock
 *
 * Writes all group descriptors to disk.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_write_group_descriptors(struct nexfs_superblock *sb)
{
    u32 i;
    u64 block;
    s32 ret;

    if (!sb || !sb->groups) {
        return VFS_EINVAL;
    }

    block = (NEXFS_SUPERBLOCK_OFFSET + NEXFS_BLOCK_SIZE) / NEXFS_BLOCK_SIZE;

    for (i = 0; i < sb->groups_count; i++) {
        /* Update checksum */
        sb->groups[i].bg_checksum = nexfs_crc32c(0, &sb->groups[i],
                                                  sizeof(struct nexfs_group_desc) - 2);

        /* TODO: Implement actual block device write */
        /* ret = block_device_write(sb->bdev, block + i, &sb->groups[i], sizeof(struct nexfs_group_desc)); */
    }

    return VFS_OK;
}

/*===========================================================================*/
/*                         BLOCK ALLOCATION                                  */
/*===========================================================================*/

/**
 * nexfs_alloc_block - Allocate a new block
 * @sb: Superblock
 * @group: Preferred group (or -1)
 *
 * Allocates a new data block from the filesystem.
 *
 * Returns: Block number or 0 on failure
 */
static u64 nexfs_alloc_block(struct nexfs_superblock *sb, s32 group)
{
    u32 g;
    u32 bit;
    u64 block = 0;

    if (!sb) {
        return 0;
    }

    spin_lock(&sb->bitmap_lock);

    /* Search groups */
    for (g = 0; g < sb->groups_count; g++) {
        struct nexfs_group_desc *gd = &sb->groups[g];

        if (gd->bg_free_blocks == 0) {
            continue;
        }

        /* Found group with free blocks */
        /* TODO: Search block bitmap for free block */
        /* For now, just return first block in group */
        block = g * sb->raw.s_blocks_per_group + sb->raw.s_first_data_block;

        /* Update group descriptor */
        gd->bg_free_blocks--;
        sb->raw.s_free_blocks--;

        break;
    }

    spin_unlock(&sb->bitmap_lock);

    if (block) {
        pr_debug("NexFS: Allocated block %llu\n", (unsigned long long)block);
    }

    return block;
}

/**
 * nexfs_free_block - Free a block
 * @sb: Superblock
 * @block: Block number to free
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_free_block(struct nexfs_superblock *sb, u64 block)
{
    u32 group;
    struct nexfs_group_desc *gd;

    if (!sb) {
        return VFS_EINVAL;
    }

    group = nexfs_group_index(sb, block);

    if (group >= sb->groups_count) {
        return VFS_EINVAL;
    }

    spin_lock(&sb->bitmap_lock);

    gd = &sb->groups[group];

    /* TODO: Clear bit in block bitmap */

    /* Update group descriptor */
    gd->bg_free_blocks++;
    sb->raw.s_free_blocks++;

    spin_unlock(&sb->bitmap_lock);

    pr_debug("NexFS: Freed block %llu\n", (unsigned long long)block);

    return VFS_OK;
}

/**
 * nexfs_get_block - Get block mapping for file
 * @inode: File inode
 * @iblock: Logical block number
 * @map: Output block mapping
 * @create: Create if not exists
 *
 * Maps a logical block to a physical block.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_get_block(struct nexfs_inode *inode, u64 iblock,
                    struct nexfs_block_map *map, s32 create)
{
    struct nexfs_superblock *sb;
    u64 phys_block;
    u32 block_index;

    if (!inode || !map) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    memset(map, 0, sizeof(struct nexfs_block_map));
    map->b_size = NEXFS_BLOCK_SIZE;

    /* Check if using extents */
    if (inode->raw.i_flags & NEXFS_FL_EXTENTS) {
        return nexfs_ext_get_block(inode, iblock, map);
    }

    /* Traditional block mapping */
    if (iblock < NEXFS_DIRECT_BLOCKS) {
        /* Direct block */
        phys_block = inode->raw.i_data.blocks.i_block[iblock];
    } else if (iblock < NEXFS_DIRECT_BLOCKS + NEXFS_INDIRECT_BLOCKS) {
        /* Indirect block - TODO: Implement */
        phys_block = 0;
    } else {
        /* Double/triple indirect - TODO: Implement */
        phys_block = 0;
    }

    if (phys_block) {
        map->b_blocknr = phys_block;
        map->b_state = NEXFS_BH_Mapped | NEXFS_BH_Read;
        return VFS_OK;
    }

    if (!create) {
        return VFS_ENOENT;
    }

    /* Allocate new block */
    phys_block = nexfs_alloc_block(sb, -1);
    if (!phys_block) {
        return VFS_ENOSPC;
    }

    /* Update inode */
    if (iblock < NEXFS_DIRECT_BLOCKS) {
        inode->raw.i_data.blocks.i_block[iblock] = (u32)phys_block;
    }

    /* Mark as new and dirty */
    nexfs_write_inode(inode, 0);

    map->b_blocknr = phys_block;
    map->b_state = NEXFS_BH_Mapped | NEXFS_BH_New | NEXFS_BH_Write;

    pr_debug("NexFS: Mapped logical %llu to physical %llu\n",
             (unsigned long long)iblock, (unsigned long long)phys_block);

    return VFS_OK;
}

/*===========================================================================*/
/*                         STATFS OPERATION                                  */
/*===========================================================================*/

/**
 * nexfs_statfs - Get filesystem statistics
 * @sb: Superblock
 * @stat: Output statistics
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_statfs(struct nexfs_superblock *sb, struct vfs_statfs *stat)
{
    if (!sb || !stat) {
        return VFS_EINVAL;
    }

    memset(stat, 0, sizeof(struct vfs_statfs));

    stat->f_type = NEXFS_MAGIC;
    stat->f_bsize = NEXFS_BLOCK_SIZE;
    stat->f_blocks = sb->raw.s_blocks_count;
    stat->f_bfree = sb->raw.s_free_blocks;
    stat->f_bavail = sb->raw.s_free_blocks;
    stat->f_files = sb->raw.s_inodes_count;
    stat->f_ffree = sb->raw.s_free_inodes;
    stat->f_namelen = NEXFS_DIR_MAX_NAME_LEN;

    return VFS_OK;
}

/*===========================================================================*/
/*                         FILESYSTEM TYPE                                   */
/*===========================================================================*/

/**
 * nexfs_alloc_inode_vfs - VFS inode allocation wrapper
 * @sb: VFS superblock
 * @inode: VFS inode
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_alloc_inode_vfs(struct vfs_superblock *sb, struct vfs_inode *inode)
{
    struct nexfs_superblock *nexfs_sb;
    struct nexfs_inode *nexfs_inode;

    nexfs_sb = container_of(sb, struct nexfs_superblock, vfs_sb);

    nexfs_inode = nexfs_alloc_inode(nexfs_sb);
    if (!nexfs_inode) {
        return VFS_ENOMEM;
    }

    /* Copy to VFS inode */
    memcpy(inode, &nexfs_inode->vfs_inode, sizeof(struct vfs_inode));

    return VFS_OK;
}

/**
 * nexfs_destroy_inode_vfs - VFS inode destruction wrapper
 * @inode: VFS inode
 */
static void nexfs_destroy_inode_vfs(struct vfs_inode *inode)
{
    struct nexfs_inode *nexfs_inode;

    nexfs_inode = container_of(inode, struct nexfs_inode, vfs_inode);
    nexfs_iput(nexfs_inode);
}

/**
 * nexfs_read_inode_vfs - VFS inode read wrapper
 * @inode: VFS inode
 */
static void nexfs_read_inode_vfs(struct vfs_inode *inode)
{
    struct nexfs_inode *nexfs_inode;

    nexfs_inode = container_of(inode, struct nexfs_inode, vfs_inode);
    nexfs_read_inode(nexfs_inode);
}

/**
 * nexfs_write_inode_vfs - VFS inode write wrapper
 * @inode: VFS inode
 * @sync: Sync flag
 */
static void nexfs_write_inode_vfs(struct vfs_inode *inode, s32 sync)
{
    struct nexfs_inode *nexfs_inode;

    nexfs_inode = container_of(inode, struct nexfs_inode, vfs_inode);
    nexfs_write_inode(nexfs_inode, sync);
}

/**
 * nexfs_evict_inode_vfs - VFS inode eviction wrapper
 * @inode: VFS inode
 */
static void nexfs_evict_inode_vfs(struct vfs_inode *inode)
{
    struct nexfs_inode *nexfs_inode;

    nexfs_inode = container_of(inode, struct nexfs_inode, vfs_inode);
    nexfs_delete_inode(nexfs_inode);
}

/**
 * nexfs_put_super_vfs - VFS superblock release wrapper
 * @sb: VFS superblock
 */
static void nexfs_put_super_vfs(struct vfs_superblock *sb)
{
    struct nexfs_superblock *nexfs_sb;

    nexfs_sb = container_of(sb, struct nexfs_superblock, vfs_sb);

    /* Shutdown journal */
    if (nexfs_sb->journal) {
        nexfs_journal_shutdown(nexfs_sb);
    }

    /* Free group descriptors */
    if (nexfs_sb->groups) {
        kfree(nexfs_sb->groups);
    }

    kfree(nexfs_sb);
}

/**
 * nexfs_sync_fs_vfs - VFS filesystem sync wrapper
 * @sb: VFS superblock
 * @wait: Wait flag
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_sync_fs_vfs(struct vfs_superblock *sb, s32 wait)
{
    struct nexfs_superblock *nexfs_sb;

    nexfs_sb = container_of(sb, struct nexfs_superblock, vfs_sb);

    /* Write superblock */
    return nexfs_write_superblock(nexfs_sb);
}

/* VFS superblock operations */
struct vfs_superblock_operations nexfs_superblock_ops = {
    .alloc_inode = nexfs_alloc_inode_vfs,
    .destroy_inode = nexfs_destroy_inode_vfs,
    .read_inode = nexfs_read_inode_vfs,
    .write_inode = nexfs_write_inode_vfs,
    .evict_inode = nexfs_evict_inode_vfs,
    .put_super = nexfs_put_super_vfs,
    .statfs = NULL,  /* Handled separately */
    .sync_fs = nexfs_sync_fs_vfs,
};

/*===========================================================================*/
/*                         MOUNT/UNMOUNT                                     */
/*===========================================================================*/

/**
 * nexfs_mount - Mount a NexFS filesystem
 * @fs: Filesystem type
 * @flags: Mount flags
 * @dev_name: Device name
 * @data: Mount options
 * @mount: Output mount point
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_mount(struct vfs_filesystem *fs, u32 flags, const char *dev_name,
                void *data, struct vfs_mount **mount)
{
    struct nexfs_superblock *sb;
    struct vfs_superblock *vfs_sb;
    struct vfs_mount *mnt;
    s32 ret;

    pr_info("NexFS: Mounting '%s' (flags=0x%04X)\n", dev_name, flags);

    /* Allocate superblock */
    sb = (struct nexfs_superblock *)kzalloc(sizeof(struct nexfs_superblock));
    if (!sb) {
        return VFS_ENOMEM;
    }

    /* Initialize VFS superblock */
    vfs_sb = &sb->vfs_sb;
    vfs_sb->magic = VFS_MAGIC;
    vfs_sb->s_op = &nexfs_superblock_ops;
    vfs_sb->s_type = fs;

    INIT_LIST_HEAD(&vfs_sb->s_inodes);
    INIT_LIST_HEAD(&vfs_sb->s_dirty_inodes);
    INIT_LIST_HEAD(&vfs_sb->s_unused_inodes);
    spin_lock_init(&vfs_sb->s_lock);
    atomic_set(&vfs_sb->s_count, 1);

    /* Initialize NexFS superblock */
    spin_lock_init(&sb->bitmap_lock);
    spin_lock_init(&sb->group_lock);
    spin_lock_init(&sb->orphan_lock);

    /* TODO: Open block device */
    /* sb->bdev = block_device_open(dev_name); */
    sb->bdev = NULL;
    sb->bdev_size = 0;

    /* Read superblock */
    ret = nexfs_read_superblock(sb);
    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to read superblock: %d\n", ret);
        goto err_free_sb;
    }

    /* Read group descriptors */
    ret = nexfs_read_group_descriptors(sb);
    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to read group descriptors: %d\n", ret);
        goto err_free_sb;
    }

    /* Initialize journal */
    ret = nexfs_journal_init(sb);
    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to initialize journal: %d\n", ret);
        goto err_free_groups;
    }

    /* Recover journal if needed */
    if (sb->raw.s_state & NEXFS_STATE_ERROR) {
        pr_info("NexFS: Recovering journal...\n");
        ret = nexfs_journal_recovery(sb);
        if (ret != VFS_OK) {
            pr_err("NexFS: Journal recovery failed: %d\n", ret);
            goto err_journal;
        }
    }

    /* Get root inode */
    struct nexfs_inode *root_inode = nexfs_iget(sb, NEXFS_ROOT_INO);
    if (!root_inode) {
        pr_err("NexFS: Failed to read root inode\n");
        ret = VFS_EIO;
        goto err_journal;
    }

    /* Set VFS superblock fields */
    vfs_sb->s_dev = sb->raw.s_journal_dev;
    vfs_sb->s_magic = NEXFS_MAGIC;
    vfs_sb->s_size = sb->bdev_size;
    vfs_sb->s_blocks = sb->raw.s_blocks_count;
    vfs_sb->s_free_blocks = sb->raw.s_free_blocks;
    vfs_sb->s_bsize = NEXFS_BLOCK_SIZE;
    vfs_sb->s_inodes = sb->raw.s_inodes_count;
    vfs_sb->s_free_inodes = sb->raw.s_free_inodes;
    vfs_sb->s_root = &root_inode->vfs_inode;
    vfs_sb->s_flags = flags;
    vfs_sb->s_private = sb;

    /* Create root dentry */
    struct vfs_dentry *root_dentry = vfs_d_alloc(NULL, "/");
    if (!root_dentry) {
        ret = VFS_ENOMEM;
        goto err_root_inode;
    }

    vfs_d_instantiate(root_dentry, vfs_sb->s_root);
    vfs_sb->s_root_dentry = root_dentry;

    /* Update superblock state */
    sb->raw.s_state = NEXFS_STATE_CLEAN;
    sb->raw.s_mount_count++;
    nexfs_write_superblock(sb);

    nexfs_global.mount_count++;

    pr_info("NexFS: Mount successful\n");

    return VFS_OK;

err_root_inode:
    nexfs_iput(root_inode);
err_journal:
    nexfs_journal_shutdown(sb);
err_free_groups:
    kfree(sb->groups);
err_free_sb:
    kfree(sb);
    return ret;
}

/**
 * nexfs_umount - Unmount a NexFS filesystem
 * @mount: Mount point
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_umount(struct vfs_mount *mount)
{
    struct nexfs_superblock *sb;

    if (!mount || !mount->mnt_sb) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)mount->mnt_sb->s_private;

    pr_info("NexFS: Unmounting...\n");

    /* Sync filesystem */
    nexfs_write_superblock(sb);

    /* Shutdown journal */
    nexfs_journal_shutdown(sb);

    /* Update superblock state */
    sb->raw.s_state = NEXFS_STATE_CLEAN;
    nexfs_write_superblock(sb);

    nexfs_global.mount_count--;

    pr_info("NexFS: Unmount complete\n");

    return VFS_OK;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * nexfs_init - Initialize NexFS module
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_init(void)
{
    s32 ret;

    pr_info("NexFS: Initializing...\n");

    /* Initialize CRC table */
    nexfs_init_crc32c_table();

    /* Initialize global state */
    nexfs_global.initialized = true;
    nexfs_global.mount_count = 0;
    atomic_set(&nexfs_global.total_inodes, 0);
    atomic_set(&nexfs_global.total_blocks, 0);
    nexfs_global.total_reads = 0;
    nexfs_global.total_writes = 0;

    /* Register filesystem */
    ret = vfs_register_filesystem(&nexfs_filesystem);
    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to register filesystem: %d\n", ret);
        nexfs_global.initialized = false;
        return ret;
    }

    pr_info("NexFS: Initialized successfully\n");

    return VFS_OK;
}

/**
 * nexfs_exit - Shutdown NexFS module
 */
void nexfs_exit(void)
{
    pr_info("NexFS: Shutting down...\n");

    /* Unregister filesystem */
    vfs_unregister_filesystem(&nexfs_filesystem);

    nexfs_global.initialized = false;

    pr_info("NexFS: Shutdown complete\n");
}

/*===========================================================================*/
/*                         FILESYSTEM TYPE DEFINITION                        */
/*===========================================================================*/

/**
 * nexfs_fs_init - NexFS filesystem initialization
 * @fs: Filesystem structure
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_fs_init(struct vfs_filesystem *fs)
{
    pr_debug("NexFS: Filesystem type initialized\n");
    return VFS_OK;
}

/**
 * nexfs_fs_exit - NexFS filesystem cleanup
 * @fs: Filesystem structure
 */
static void nexfs_fs_exit(struct vfs_filesystem *fs)
{
    pr_debug("NexFS: Filesystem type cleaned up\n");
}

/* NexFS filesystem type */
struct vfs_filesystem nexfs_filesystem = {
    .name = "nexfs",
    .fs_magic = NEXFS_MAGIC,
    .fs_flags = 0,
    .fs_op = &nexfs_superblock_ops,
    .mount = nexfs_mount,
    .umount = nexfs_umount,
    .init = nexfs_fs_init,
    .exit = nexfs_fs_exit,
    .magic = VFS_MAGIC,
};

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * nexfs_stats_print - Print NexFS statistics
 */
void nexfs_stats_print(void)
{
    printk("\n=== NexFS Statistics ===\n");
    printk("Initialized:       %s\n", nexfs_global.initialized ? "Yes" : "No");
    printk("Mount Count:       %u\n", nexfs_global.mount_count);
    printk("Total Inodes:      %d\n", atomic_read(&nexfs_global.total_inodes));
    printk("Total Blocks:      %d\n", atomic_read(&nexfs_global.total_blocks));
    printk("Total Reads:       %llu\n", (unsigned long long)nexfs_global.total_reads);
    printk("Total Writes:      %llu\n", (unsigned long long)nexfs_global.total_writes);
}
