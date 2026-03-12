/*
 * NEXUS OS - NexFS Filesystem
 * fs/nexfs/nexfs_inode.c
 *
 * NexFS Inode Operations - Inode management, file operations, and directory handling
 */

#include "nexfs.h"

/*===========================================================================*/
/*                         INODE CACHE                                       */
/*===========================================================================*/

static void *nexfs_inode_cache = NULL;

/**
 * nexfs_inode_cache_init - Initialize inode cache
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_cache_init(void)
{
    nexfs_inode_cache = kmem_cache_create("nexfs_inode", sizeof(struct nexfs_inode),
                                           __alignof__(struct nexfs_inode), 0);
    if (!nexfs_inode_cache) {
        pr_err("NexFS: Failed to create inode cache\n");
        return VFS_ENOMEM;
    }

    pr_debug("NexFS: Inode cache created\n");

    return VFS_OK;
}

/**
 * nexfs_inode_cache_exit - Destroy inode cache
 */
static void nexfs_inode_cache_exit(void)
{
    if (nexfs_inode_cache) {
        kmem_cache_destroy(nexfs_inode_cache);
        nexfs_inode_cache = NULL;
    }
}

/*===========================================================================*/
/*                         INODE ALLOCATION                                  */
/*===========================================================================*/

/**
 * nexfs_alloc_inode - Allocate a new inode
 * @sb: Superblock
 *
 * Allocates and initializes a new NexFS inode.
 *
 * Returns: New inode or NULL on failure
 */
struct nexfs_inode *nexfs_alloc_inode(struct nexfs_superblock *sb)
{
    struct nexfs_inode *inode;
    u64 ino;
    u32 group;

    if (!sb) {
        return NULL;
    }

    /* Allocate from cache */
    inode = (struct nexfs_inode *)kmem_cache_alloc(nexfs_inode_cache);
    if (!inode) {
        return NULL;
    }

    memset(inode, 0, sizeof(struct nexfs_inode));

    /* Initialize VFS inode part */
    struct vfs_inode *vfs_inode = &inode->vfs_inode;
    vfs_inode->magic = VFS_MAGIC;
    vfs_inode->i_sb = &sb->vfs_sb;
    vfs_inode->i_op = &nexfs_inode_ops;
    vfs_inode->i_fop = &nexfs_file_operations;

    atomic_set(&vfs_inode->refcount, 1);
    atomic_set(&vfs_inode->i_count, 1);

    spin_lock_init(&vfs_inode->lock);
    spin_lock_init(&vfs_inode->i_lock);

    INIT_LIST_HEAD(&vfs_inode->hash_list);
    INIT_LIST_HEAD(&vfs_inode->dirty_list);
    INIT_LIST_HEAD(&vfs_inode->unused_list);

    /* Initialize NexFS specific fields */
    inode->ino = 0;
    inode->generation = 0;
    inode->extent_cache = NULL;
    inode->extent_cache_size = 0;

    INIT_LIST_HEAD(&inode->dcache);
    spin_lock_init(&inode->dcache_lock);

    inode->last_alloc_block = 0;
    inode->last_alloc_group = 0;
    inode->checksum = 0;

    /* Find free inode number */
    spin_lock(&sb->bitmap_lock);

    for (group = 0; group < sb->groups_count; group++) {
        if (sb->groups[group].bg_free_inodes > 0) {
            /* TODO: Find free bit in inode bitmap */
            ino = group * sb->raw.s_inodes_per_group + NEXFS_FIRST_INO;

            /* Update counts */
            sb->groups[group].bg_free_inodes--;
            sb->raw.s_free_inodes--;
            sb->raw.s_inodes_count++;

            spin_unlock(&sb->bitmap_lock);

            inode->ino = ino;
            inode->generation = 1;

            /* Initialize raw inode */
            inode->raw.i_mode = 0;
            inode->raw.i_uid = 0;
            inode->raw.i_gid = 0;
            inode->raw.i_size = 0;
            inode->raw.i_links = 1;
            inode->raw.i_blocks_lo = 0;
            inode->raw.i_flags = 0;
            inode->raw.i_atime = (u32)(get_time_ms() / 1000);
            inode->raw.i_mtime = inode->raw.i_atime;
            inode->raw.i_ctime = inode->raw.i_atime;

            /* Set default flags */
            inode->raw.i_flags = NEXFS_FL_EXTENTS;

            atomic_inc(&nexfs_global.total_inodes);

            pr_debug("NexFS: Allocated inode %llu\n", (unsigned long long)ino);

            return inode;
        }
    }

    spin_unlock(&sb->bitmap_lock);

    /* No free inodes */
    kmem_cache_free(nexfs_inode_cache, inode);

    return NULL;
}

/**
 * nexfs_free_inode - Free an inode structure
 * @inode: Inode to free
 */
static void nexfs_free_inode(struct nexfs_inode *inode)
{
    if (!inode) {
        return;
    }

    /* Free extent cache */
    if (inode->extent_cache) {
        kfree(inode->extent_cache);
    }

    kmem_cache_free(nexfs_inode_cache, inode);
}

/*===========================================================================*/
/*                         INODE LOOKUP                                      */
/*===========================================================================*/

/**
 * nexfs_iget - Get inode by number
 * @sb: Superblock
 * @ino: Inode number
 *
 * Looks up an inode, reading from disk if necessary.
 *
 * Returns: Inode or NULL on failure
 */
struct nexfs_inode *nexfs_iget(struct nexfs_superblock *sb, u64 ino)
{
    struct nexfs_inode *inode;
    struct vfs_inode *vfs_inode;
    s32 ret;

    if (!sb || ino == 0) {
        return NULL;
    }

    /* Check inode cache first */
    vfs_inode = vfs_iget_locked(&sb->vfs_sb, ino);
    if (vfs_inode) {
        return container_of(vfs_inode, struct nexfs_inode, vfs_inode);
    }

    /* Allocate new inode */
    inode = (struct nexfs_inode *)kmem_cache_alloc(nexfs_inode_cache);
    if (!inode) {
        return NULL;
    }

    memset(inode, 0, sizeof(struct nexfs_inode));

    /* Initialize VFS inode */
    vfs_inode = &inode->vfs_inode;
    vfs_inode->magic = VFS_MAGIC;
    vfs_inode->i_sb = &sb->vfs_sb;
    vfs_inode->ino = ino;
    vfs_inode->dev = sb->vfs_sb.s_dev;

    atomic_set(&vfs_inode->refcount, 1);
    atomic_set(&vfs_inode->i_count, 1);

    spin_lock_init(&vfs_inode->lock);
    spin_lock_init(&vfs_inode->i_lock);

    INIT_LIST_HEAD(&vfs_inode->hash_list);
    INIT_LIST_HEAD(&vfs_inode->dirty_list);
    INIT_LIST_HEAD(&vfs_inode->unused_list);
    vfs_inode->last_used = get_time_ms();

    /* Initialize NexFS fields */
    inode->ino = ino;
    inode->dcache_lock = (spinlock_t){ .lock = 0 };
    INIT_LIST_HEAD(&inode->dcache);

    /* Read inode from disk */
    ret = nexfs_read_inode(inode);
    if (ret != VFS_OK) {
        kmem_cache_free(nexfs_inode_cache, inode);
        return NULL;
    }

    /* Add to VFS inode hash */
    vfs_iget(vfs_inode);

    pr_debug("NexFS: Got inode %llu (mode=%04o, size=%llu)\n",
             (unsigned long long)ino, inode->raw.i_mode,
             (unsigned long long)inode->raw.i_size);

    return inode;
}

/**
 * nexfs_iput - Release inode reference
 * @inode: Inode to release
 */
void nexfs_iput(struct nexfs_inode *inode)
{
    struct vfs_inode *vfs_inode;

    if (!inode) {
        return;
    }

    vfs_inode = &inode->vfs_inode;
    vfs_iput(vfs_inode);
}

/*===========================================================================*/
/*                         INODE READ/WRITE                                  */
/*===========================================================================*/

/**
 * nexfs_read_inode - Read inode from disk
 * @inode: Inode to read
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_read_inode(struct nexfs_inode *inode)
{
    struct nexfs_superblock *sb;
    u64 block;
    u32 offset;
    u32 checksum;
    s32 ret;

    if (!inode) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    if (!sb) {
        return VFS_ENODEV;
    }

    /* Calculate inode location */
    block = nexfs_inode_to_block(sb, inode->ino);
    offset = ((inode->ino - 1) % NEXFS_INODES_PER_BLOCK) * NEXFS_INODE_SIZE;

    /* TODO: Read block from disk */
    /* ret = block_device_read(sb->bdev, block, buffer, NEXFS_BLOCK_SIZE); */
    ret = VFS_OK;

    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to read inode block %llu\n", (unsigned long long)block);
        return VFS_EIO;
    }

    /* Copy inode data from buffer */
    /* memcpy(&inode->raw, buffer + offset, NEXFS_INODE_SIZE); */

    /* Validate checksum */
    checksum = nexfs_inode_checksum(&inode->raw);
    if (checksum != inode->raw.i_checksum) {
        pr_err("NexFS: Inode %llu checksum mismatch\n", (unsigned long long)inode->ino);
        return VFS_EIO;
    }

    /* Update VFS inode fields */
    struct vfs_inode *vfs_inode = &inode->vfs_inode;

    vfs_inode->ino = inode->ino;
    vfs_inode->generation = inode->raw.i_generation;

    /* File type */
    switch (inode->raw.i_mode & VFS_TYPE_MASK) {
        case NEXFS_TYPE_REGULAR:
            vfs_inode->type = VFS_TYPE_REGULAR;
            vfs_inode->i_fop = &nexfs_file_operations;
            break;
        case NEXFS_TYPE_DIRECTORY:
            vfs_inode->type = VFS_TYPE_DIRECTORY;
            vfs_inode->i_fop = &nexfs_dir_operations;
            break;
        case NEXFS_TYPE_SYMLINK:
            vfs_inode->type = VFS_TYPE_SYMLINK;
            break;
        case NEXFS_TYPE_CHAR_DEV:
            vfs_inode->type = VFS_TYPE_CHAR_DEVICE;
            break;
        case NEXFS_TYPE_BLOCK_DEV:
            vfs_inode->type = VFS_TYPE_BLOCK_DEVICE;
            break;
        default:
            vfs_inode->type = VFS_TYPE_UNKNOWN;
            break;
    }

    vfs_inode->mode = inode->raw.i_mode;
    vfs_inode->uid = inode->raw.i_uid | (inode->raw.i_uid_high << 16);
    vfs_inode->gid = inode->raw.i_gid | (inode->raw.i_gid_high << 16);
    vfs_inode->size = inode->raw.i_size;
    vfs_inode->nlink = inode->raw.i_links;
    vfs_inode->nblocks = inode->raw.i_blocks_lo | ((u64)inode->raw.i_blocks_hi << 32);
    vfs_inode->atime = inode->raw.i_atime;
    vfs_inode->mtime = inode->raw.i_mtime;
    vfs_inode->ctime = inode->raw.i_ctime;
    vfs_inode->btime = inode->raw.i_crtime;

    vfs_inode->flags |= VFS_INODE_VALID;

    inode->generation = inode->raw.i_generation;
    inode->checksum = checksum;

    return VFS_OK;
}

/**
 * nexfs_write_inode - Write inode to disk
 * @inode: Inode to write
 * @sync: Sync flag
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_write_inode(struct nexfs_inode *inode, s32 sync)
{
    struct nexfs_superblock *sb;
    u64 block;
    u32 offset;
    s32 ret;

    if (!inode) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    if (!sb) {
        return VFS_ENODEV;
    }

    /* Update raw inode from VFS inode */
    struct vfs_inode *vfs_inode = &inode->vfs_inode;

    inode->raw.i_mode = vfs_inode->mode;
    inode->raw.i_uid = vfs_inode->uid & 0xFFFF;
    inode->raw.i_uid_high = (vfs_inode->uid >> 16) & 0xFFFF;
    inode->raw.i_gid = vfs_inode->gid & 0xFFFF;
    inode->raw.i_gid_high = (vfs_inode->gid >> 16) & 0xFFFF;
    inode->raw.i_size = vfs_inode->size;
    inode->raw.i_links = vfs_inode->nlink;
    inode->raw.i_blocks_lo = vfs_inode->nblocks & 0xFFFFFFFF;
    inode->raw.i_blocks_hi = (vfs_inode->nblocks >> 32) & 0xFFFFFFFF;
    inode->raw.i_atime = (u32)vfs_inode->atime;
    inode->raw.i_mtime = (u32)vfs_inode->mtime;
    inode->raw.i_ctime = (u32)vfs_inode->ctime;
    inode->raw.i_crtime = (u32)vfs_inode->btime;
    inode->raw.i_generation = vfs_inode->generation;

    /* Update checksum */
    inode->raw.i_checksum = nexfs_inode_checksum(&inode->raw);
    inode->checksum = inode->raw.i_checksum;

    /* Calculate inode location */
    block = nexfs_inode_to_block(sb, inode->ino);
    offset = ((inode->ino - 1) % NEXFS_INODES_PER_BLOCK) * NEXFS_INODE_SIZE;

    /* Start journal transaction */
    ret = nexfs_journal_start(sb, 1);
    if (ret != VFS_OK) {
        return ret;
    }

    /* Get write access to block */
    ret = nexfs_journal_get_write_access(sb, block);
    if (ret != VFS_OK) {
        nexfs_journal_stop(sb);
        return ret;
    }

    /* TODO: Write to disk */
    /* ret = block_device_write(sb->bdev, block, buffer, NEXFS_BLOCK_SIZE); */

    /* Dirty metadata */
    ret = nexfs_journal_dirty_metadata(sb, block);

    /* Stop transaction */
    nexfs_journal_stop(sb);

    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to write inode %llu\n", (unsigned long long)inode->ino);
        return ret;
    }

    if (sync) {
        /* Force commit */
        nexfs_journal_commit(sb);
    }

    pr_debug("NexFS: Wrote inode %llu\n", (unsigned long long)inode->ino);

    return VFS_OK;
}

/**
 * nexfs_delete_inode - Delete an inode
 * @inode: Inode to delete
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_delete_inode(struct nexfs_inode *inode)
{
    struct nexfs_superblock *sb;
    u32 group;

    if (!inode) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    if (!sb) {
        return VFS_ENODEV;
    }

    pr_debug("NexFS: Deleting inode %llu\n", (unsigned long long)inode->ino);

    /* Free all blocks */
    /* TODO: Free all data blocks */

    /* Update inode bitmap */
    group = (u32)((inode->ino - 1) / sb->raw.s_inodes_per_group);

    spin_lock(&sb->bitmap_lock);

    /* TODO: Clear bit in inode bitmap */
    sb->groups[group].bg_free_inodes++;
    sb->raw.s_free_inodes++;
    sb->raw.s_inodes_count--;

    spin_unlock(&sb->bitmap_lock);

    /* Write inode with deletion time */
    inode->raw.i_dtime = (u32)(get_time_ms() / 1000);
    nexfs_write_inode(inode, 1);

    atomic_dec(&nexfs_global.total_inodes);

    /* Free inode structure */
    nexfs_free_inode(inode);

    return VFS_OK;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

/**
 * nexfs_file_read - Read from a file
 * @file: Open file
 * @buf: Buffer to read into
 * @count: Bytes to read
 * @pos: File position
 *
 * Returns: Bytes read or error code
 */
static s64 nexfs_file_read(struct vfs_file *file, char *buf, u64 count, u64 *pos)
{
    struct nexfs_inode *inode;
    struct nexfs_superblock *sb;
    struct nexfs_block_map map;
    u64 offset;
    u64 bytes_read = 0;
    u64 block;
    u32 block_offset;
    u32 to_read;
    s32 ret;

    if (!file || !buf || !pos) {
        return VFS_EINVAL;
    }

    inode = container_of(file->f_inode, struct nexfs_inode, vfs_inode);
    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    offset = *pos;

    /* Check EOF */
    if (offset >= inode->raw.i_size) {
        return 0;
    }

    /* Limit read to EOF */
    if (offset + count > inode->raw.i_size) {
        count = inode->raw.i_size - offset;
    }

    while (count > 0) {
        block = offset / NEXFS_BLOCK_SIZE;
        block_offset = offset % NEXFS_BLOCK_SIZE;

        /* Get block mapping */
        ret = nexfs_get_block(inode, block, &map, 0);
        if (ret != VFS_OK) {
            if (ret == VFS_ENOENT) {
                /* Hole - return zeros */
                to_read = MIN(count, NEXFS_BLOCK_SIZE - block_offset);
                memset(buf + bytes_read, 0, to_read);
                bytes_read += to_read;
                offset += to_read;
                count -= to_read;
                continue;
            }
            return ret;
        }

        /* Calculate bytes to read from this block */
        to_read = MIN(count, NEXFS_BLOCK_SIZE - block_offset);
        to_read = MIN(to_read, (u32)(inode->raw.i_size - offset));

        /* TODO: Read from block device */
        /* ret = block_device_read(sb->bdev, map.b_blocknr, buf + bytes_read, to_read); */

        /* For now, zero-fill */
        memset(buf + bytes_read, 0, to_read);

        bytes_read += to_read;
        offset += to_read;
        count -= to_read;

        nexfs_global.total_reads++;
    }

    *pos = offset;

    return (s64)bytes_read;
}

/**
 * nexfs_file_write - Write to a file
 * @file: Open file
 * @buf: Buffer to write from
 * @count: Bytes to write
 * @pos: File position
 *
 * Returns: Bytes written or error code
 */
static s64 nexfs_file_write(struct vfs_file *file, const char *buf, u64 count, u64 *pos)
{
    struct nexfs_inode *inode;
    struct nexfs_superblock *sb;
    struct nexfs_block_map map;
    u64 offset;
    u64 bytes_written = 0;
    u64 block;
    u32 block_offset;
    u32 to_write;
    s32 ret;

    if (!file || !buf || !pos) {
        return VFS_EINVAL;
    }

    inode = container_of(file->f_inode, struct nexfs_inode, vfs_inode);
    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    offset = *pos;

    while (count > 0) {
        block = offset / NEXFS_BLOCK_SIZE;
        block_offset = offset % NEXFS_BLOCK_SIZE;

        /* Get or allocate block */
        ret = nexfs_get_block(inode, block, &map, 1);
        if (ret != VFS_OK) {
            if (bytes_written > 0) {
                break;
            }
            return ret;
        }

        /* Calculate bytes to write to this block */
        to_write = MIN(count, NEXFS_BLOCK_SIZE - block_offset);

        /* TODO: Write to block device */
        /* ret = block_device_write(sb->bdev, map.b_blocknr, buf + bytes_written, to_write); */

        bytes_written += to_write;
        offset += to_write;
        count -= to_write;

        nexfs_global.total_writes++;
    }

    /* Update file size */
    if (offset > inode->raw.i_size) {
        inode->raw.i_size = offset;
        inode->vfs_inode.size = offset;
        nexfs_write_inode(inode, 0);
    }

    *pos = offset;

    return (s64)bytes_written;
}

/**
 * nexfs_file_llseek - Seek in file
 * @file: Open file
 * @offset: Offset
 * @whence: Seek type
 *
 * Returns: New position or error code
 */
static s64 nexfs_file_llseek(struct vfs_file *file, s64 offset, s32 whence)
{
    struct nexfs_inode *inode;
    s64 new_pos;

    if (!file) {
        return VFS_EINVAL;
    }

    inode = container_of(file->f_inode, struct nexfs_inode, vfs_inode);

    switch (whence) {
        case VFS_SEEK_SET:
            new_pos = offset;
            break;
        case VFS_SEEK_CUR:
            new_pos = (s64)file->f_pos + offset;
            break;
        case VFS_SEEK_END:
            new_pos = (s64)inode->raw.i_size + offset;
            break;
        default:
            return VFS_EINVAL;
    }

    if (new_pos < 0) {
        return VFS_EINVAL;
    }

    file->f_pos = (u64)new_pos;

    return new_pos;
}

/**
 * nexfs_file_open - Open a file
 * @inode: Inode to open
 * @file: File structure
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_file_open(struct vfs_inode *inode, struct vfs_file *file)
{
    struct nexfs_inode *nexfs_inode;

    if (!inode || !file) {
        return VFS_EINVAL;
    }

    nexfs_inode = container_of(inode, struct nexfs_inode, vfs_inode);

    /* Check file type */
    if (nexfs_inode->raw.i_mode & VFS_TYPE_MASK != NEXFS_TYPE_REGULAR) {
        return VFS_EINVAL;
    }

    /* Check permissions */
    if ((file->f_flags & VFS_FO_WRITE) && !(inode->mode & VFS_MODE_IWUSR)) {
        return VFS_EACCES;
    }

    /* Check immutable flag */
    if ((nexfs_inode->raw.i_flags & NEXFS_FL_IMMUTABLE) && (file->f_flags & VFS_FO_WRITE)) {
        return VFS_EPERM;
    }

    /* Check append-only flag */
    if (nexfs_inode->raw.i_flags & NEXFS_FL_APPEND) {
        if (file->f_flags & VFS_FO_WRITE) {
            file->f_flags |= VFS_FO_APPEND;
        }
    }

    pr_debug("NexFS: Opened file inode %llu\n", (unsigned long long)nexfs_inode->ino);

    return VFS_OK;
}

/**
 * nexfs_file_release - Release a file
 * @inode: Inode
 * @file: File structure
 *
 * Returns: VFS_OK on success
 */
static s32 nexfs_file_release(struct vfs_inode *inode, struct vfs_file *file)
{
    pr_debug("NexFS: Released file inode %llu\n", (unsigned long long)inode->ino);
    return VFS_OK;
}

/**
 * nexfs_file_fsync - Sync file to disk
 * @file: Open file
 * @datasync: Data sync only flag
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_file_fsync(struct vfs_file *file, s32 datasync)
{
    struct nexfs_inode *inode;
    struct nexfs_superblock *sb;

    if (!file) {
        return VFS_EINVAL;
    }

    inode = container_of(file->f_inode, struct nexfs_inode, vfs_inode);
    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    /* Commit journal */
    return nexfs_journal_commit(sb);
}

/* NexFS file operations */
struct vfs_file_operations nexfs_file_operations = {
    .read = nexfs_file_read,
    .write = nexfs_file_write,
    .llseek = nexfs_file_llseek,
    .open = nexfs_file_open,
    .release = nexfs_file_release,
    .fsync = nexfs_file_fsync,
};

/*===========================================================================*/
/*                         DIRECTORY OPERATIONS                              */
/*===========================================================================*/

/**
 * nexfs_add_entry - Add entry to directory
 * @dir: Directory inode
 * @name: Entry name
 * @ino: Inode number
 * @type: File type
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_add_entry(struct nexfs_inode *dir, const char *name, u64 ino, u8 type)
{
    struct nexfs_superblock *sb;
    struct nexfs_dir_entry *de;
    u64 block;
    u32 offset;
    u16 rec_len;
    u16 name_len;
    s32 ret;

    if (!dir || !name || !ino) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)dir->vfs_inode.i_sb;
    name_len = (u16)strlen(name);

    if (name_len > NEXFS_DIR_MAX_NAME_LEN) {
        return VFS_ENAMETOOLONG;
    }

    rec_len = NEXFS_DIR_REC_LEN(name_len);

    /* Find space in directory */
    /* TODO: Search directory blocks for space */

    /* For now, allocate new block */
    struct nexfs_block_map map;
    u64 dir_block = dir->raw.i_size / NEXFS_BLOCK_SIZE;

    ret = nexfs_get_block(dir, dir_block, &map, 1);
    if (ret != VFS_OK) {
        return ret;
    }

    /* Create directory entry */
    /* de = (struct nexfs_dir_entry *)(buffer + offset); */
    /* de->inode = ino; */
    /* de->rec_len = rec_len; */
    /* de->name_len = name_len; */
    /* de->file_type = type; */
    /* memcpy(de->name, name, name_len); */

    /* Update directory size */
    dir->raw.i_size += rec_len;
    dir->vfs_inode.size = dir->raw.i_size;

    /* Update directory mtime */
    dir->raw.i_mtime = (u32)(get_time_ms() / 1000);

    nexfs_write_inode(dir, 0);

    pr_debug("NexFS: Added entry '%s' (ino=%llu) to dir %llu\n",
             name, (unsigned long long)ino, (unsigned long long)dir->ino);

    return VFS_OK;
}

/**
 * nexfs_remove_entry - Remove entry from directory
 * @dir: Directory inode
 * @name: Entry name
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_remove_entry(struct nexfs_inode *dir, const char *name)
{
    struct nexfs_dir_entry *de;
    void *result;
    s32 ret;

    if (!dir || !name) {
        return VFS_EINVAL;
    }

    /* Find entry */
    de = nexfs_find_entry(dir, name, &result);
    if (!de) {
        return VFS_ENOENT;
    }

    /* TODO: Remove entry from directory block */

    kfree(result);

    /* Update directory mtime */
    dir->raw.i_mtime = (u32)(get_time_ms() / 1000);
    nexfs_write_inode(dir, 0);

    pr_debug("NexFS: Removed entry '%s' from dir %llu\n",
             name, (unsigned long long)dir->ino);

    return VFS_OK;
}

/**
 * nexfs_find_entry - Find entry in directory
 * @dir: Directory inode
 * @name: Entry name
 * @result: Output buffer pointer
 *
 * Returns: Directory entry or NULL
 */
struct nexfs_dir_entry *nexfs_find_entry(struct nexfs_inode *dir,
                                          const char *name, void **result)
{
    struct nexfs_superblock *sb;
    struct nexfs_dir_entry *de;
    u64 block;
    u32 offset;
    u16 rec_len;
    u32 name_len;

    if (!dir || !name || !result) {
        return NULL;
    }

    sb = (struct nexfs_superblock *)dir->vfs_inode.i_sb;
    name_len = strlen(name);

    /* Search directory blocks */
    for (offset = 0; offset < dir->raw.i_size; ) {
        block = offset / NEXFS_BLOCK_SIZE;

        /* TODO: Read directory block */
        /* ret = nexfs_get_block(dir, block, &map, 0); */

        /* de = (struct nexfs_dir_entry *)(buffer + (offset % NEXFS_BLOCK_SIZE)); */

        /* For now, return NULL */
        de = NULL;

        if (de) {
            rec_len = de->rec_len;

            if (rec_len == 0) {
                break;
            }

            if (de->name_len == name_len &&
                memcmp(de->name, name, name_len) == 0) {
                *result = de;
                return de;
            }

            offset += rec_len;
        } else {
            break;
        }
    }

    return NULL;
}

/**
 * nexfs_iterate_dir - Iterate directory entries
 * @dir: Directory inode
 * @dirent: User dirent buffer
 * @filldir: Fill callback
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_iterate_dir(struct nexfs_inode *dir, void *dirent, filldir_t filldir)
{
    /* TODO: Implement directory iteration */
    return VFS_OK;
}

/**
 * nexfs_dir_open - Open a directory
 * @inode: Inode to open
 * @file: File structure
 *
 * Returns: VFS_OK on success
 */
static s32 nexfs_dir_open(struct vfs_inode *inode, struct vfs_file *file)
{
    if (!inode || !file) {
        return VFS_EINVAL;
    }

    pr_debug("NexFS: Opened directory inode %llu\n", (unsigned long long)inode->ino);

    return VFS_OK;
}

/**
 * nexfs_dir_release - Release a directory
 * @inode: Inode
 * @file: File structure
 *
 * Returns: VFS_OK on success
 */
static s32 nexfs_dir_release(struct vfs_inode *inode, struct vfs_file *file)
{
    pr_debug("NexFS: Released directory inode %llu\n", (unsigned long long)inode->ino);
    return VFS_OK;
}

/* NexFS directory operations */
struct vfs_file_operations nexfs_dir_operations = {
    .open = nexfs_dir_open,
    .release = nexfs_dir_release,
};

/*===========================================================================*/
/*                         INODE OPERATIONS                                  */
/*===========================================================================*/

/**
 * nexfs_inode_create - Create a file
 * @dir: Directory inode
 * @dentry: Dentry for new file
 * @mode: File mode
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_create(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode)
{
    struct nexfs_inode *dir_inode;
    struct nexfs_inode *new_inode;
    struct nexfs_superblock *sb;
    s32 ret;

    if (!dir || !dentry) {
        return VFS_EINVAL;
    }

    dir_inode = container_of(dir, struct nexfs_inode, vfs_inode);
    sb = (struct nexfs_superblock *)dir->i_sb;

    /* Allocate new inode */
    new_inode = nexfs_alloc_inode(sb);
    if (!new_inode) {
        return VFS_ENOMEM;
    }

    /* Set file type and mode */
    new_inode->raw.i_mode = (mode & VFS_MODE_MASK) | NEXFS_TYPE_REGULAR;
    new_inode->vfs_inode.type = VFS_TYPE_REGULAR;
    new_inode->vfs_inode.mode = mode;

    /* Write new inode */
    ret = nexfs_write_inode(new_inode, 1);
    if (ret != VFS_OK) {
        nexfs_iput(new_inode);
        return ret;
    }

    /* Add entry to directory */
    ret = nexfs_add_entry(dir_inode, dentry->d_name, new_inode->ino, NEXFS_TYPE_REGULAR);
    if (ret != VFS_OK) {
        nexfs_delete_inode(new_inode);
        return ret;
    }

    /* Instantiate dentry */
    vfs_d_instantiate(dentry, &new_inode->vfs_inode);

    /* Update directory */
    dir->ctime = dir->mtime = get_time_ms() / 1000;
    nexfs_write_inode(dir_inode, 0);

    pr_debug("NexFS: Created file '%s' (ino=%llu)\n",
             dentry->d_name, (unsigned long long)new_inode->ino);

    return VFS_OK;
}

/**
 * nexfs_inode_lookup - Lookup entry in directory
 * @dir: Directory inode
 * @dentry: Dentry to lookup
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_lookup(struct vfs_inode *dir, struct vfs_dentry *dentry)
{
    struct nexfs_inode *dir_inode;
    struct nexfs_dir_entry *de;
    void *result;
    struct nexfs_inode *inode;

    if (!dir || !dentry) {
        return VFS_EINVAL;
    }

    dir_inode = container_of(dir, struct nexfs_inode, vfs_inode);

    /* Find entry */
    de = nexfs_find_entry(dir_inode, dentry->d_name, &result);
    if (!de) {
        return VFS_ENOENT;
    }

    /* Get inode */
    inode = nexfs_iget((struct nexfs_superblock *)dir->i_sb, de->inode);
    if (!inode) {
        kfree(result);
        return VFS_EIO;
    }

    kfree(result);

    /* Instantiate dentry */
    vfs_d_instantiate(dentry, &inode->vfs_inode);

    return VFS_OK;
}

/**
 * nexfs_inode_unlink - Unlink a file
 * @dir: Directory inode
 * @dentry: Dentry to unlink
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_unlink(struct vfs_inode *dir, struct vfs_dentry *dentry)
{
    struct nexfs_inode *dir_inode;
    struct nexfs_inode *file_inode;
    s32 ret;

    if (!dir || !dentry || !dentry->d_inode) {
        return VFS_EINVAL;
    }

    dir_inode = container_of(dir, struct nexfs_inode, vfs_inode);
    file_inode = container_of(dentry->d_inode, struct nexfs_inode, vfs_inode);

    /* Remove from directory */
    ret = nexfs_remove_entry(dir_inode, dentry->d_name);
    if (ret != VFS_OK) {
        return ret;
    }

    /* Decrement link count */
    file_inode->raw.i_links--;
    file_inode->vfs_inode.nlink--;

    if (file_inode->raw.i_links == 0) {
        /* Schedule for deletion */
        file_inode->raw.i_dtime = (u32)(get_time_ms() / 1000);
    }

    nexfs_write_inode(file_inode, 0);

    /* Update directory */
    dir->ctime = dir->mtime = get_time_ms() / 1000;
    nexfs_write_inode(dir_inode, 0);

    /* Drop dentry */
    vfs_d_delete(dentry);

    pr_debug("NexFS: Unlinked '%s' (ino=%llu)\n",
             dentry->d_name, (unsigned long long)file_inode->ino);

    return VFS_OK;
}

/**
 * nexfs_inode_mkdir - Create a directory
 * @dir: Directory inode
 * @dentry: Dentry for new directory
 * @mode: Directory mode
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_mkdir(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode)
{
    struct nexfs_inode *dir_inode;
    struct nexfs_inode *new_inode;
    struct nexfs_superblock *sb;
    s32 ret;

    if (!dir || !dentry) {
        return VFS_EINVAL;
    }

    dir_inode = container_of(dir, struct nexfs_inode, vfs_inode);
    sb = (struct nexfs_superblock *)dir->i_sb;

    /* Allocate new inode */
    new_inode = nexfs_alloc_inode(sb);
    if (!new_inode) {
        return VFS_ENOMEM;
    }

    /* Set directory type and mode */
    new_inode->raw.i_mode = (mode & VFS_MODE_MASK) | NEXFS_TYPE_DIRECTORY;
    new_inode->vfs_inode.type = VFS_TYPE_DIRECTORY;
    new_inode->vfs_inode.mode = mode | VFS_MODE_IXUSR | VFS_MODE_IXGRP | VFS_MODE_IXOTH;
    new_inode->vfs_inode.i_fop = &nexfs_dir_operations;

    /* Initialize . and .. entries */
    /* TODO: Add . and .. entries */

    /* Write new inode */
    ret = nexfs_write_inode(new_inode, 1);
    if (ret != VFS_OK) {
        nexfs_iput(new_inode);
        return ret;
    }

    /* Add entry to parent directory */
    ret = nexfs_add_entry(dir_inode, dentry->d_name, new_inode->ino, NEXFS_TYPE_DIRECTORY);
    if (ret != VFS_OK) {
        nexfs_delete_inode(new_inode);
        return ret;
    }

    /* Instantiate dentry */
    vfs_d_instantiate(dentry, &new_inode->vfs_inode);

    /* Update parent directory */
    dir->ctime = dir->mtime = get_time_ms() / 1000;
    nexfs_write_inode(dir_inode, 0);

    pr_debug("NexFS: Created directory '%s' (ino=%llu)\n",
             dentry->d_name, (unsigned long long)new_inode->ino);

    return VFS_OK;
}

/**
 * nexfs_inode_rmdir - Remove a directory
 * @dir: Directory inode
 * @dentry: Dentry to remove
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_inode_rmdir(struct vfs_inode *dir, struct vfs_dentry *dentry)
{
    struct nexfs_inode *dir_inode;
    struct nexfs_inode *rmdir_inode;

    if (!dir || !dentry || !dentry->d_inode) {
        return VFS_EINVAL;
    }

    dir_inode = container_of(dir, struct nexfs_inode, vfs_inode);
    rmdir_inode = container_of(dentry->d_inode, struct nexfs_inode, vfs_inode);

    /* Check if directory is empty */
    /* TODO: Check for entries other than . and .. */

    /* Remove from parent */
    nexfs_remove_entry(dir_inode, dentry->d_name);

    /* Decrement link count */
    rmdir_inode->raw.i_links--;
    rmdir_inode->vfs_inode.nlink--;

    nexfs_write_inode(rmdir_inode, 0);

    /* Update parent */
    dir->ctime = dir->mtime = get_time_ms() / 1000;
    nexfs_write_inode(dir_inode, 0);

    vfs_d_delete(dentry);

    pr_debug("NexFS: Removed directory '%s'\n", dentry->d_name);

    return VFS_OK;
}

/* NexFS inode operations */
struct vfs_inode_operations nexfs_inode_ops = {
    .create = nexfs_inode_create,
    .lookup = nexfs_inode_lookup,
    .unlink = nexfs_inode_unlink,
    .mkdir = nexfs_inode_mkdir,
    .rmdir = nexfs_inode_rmdir,
};

struct vfs_inode_operations nexfs_dir_ops = {
    .lookup = nexfs_inode_lookup,
    .mkdir = nexfs_inode_mkdir,
    .rmdir = nexfs_inode_rmdir,
    .create = nexfs_inode_create,
    .unlink = nexfs_inode_unlink,
};

struct vfs_inode_operations nexfs_file_ops = {
};

struct vfs_inode_operations nexfs_symlink_ops = {
};

/*===========================================================================*/
/*                         EXTENT OPERATIONS                                 */
/*===========================================================================*/

/**
 * nexfs_ext_get_block - Get block mapping using extents
 * @inode: File inode
 * @iblock: Logical block number
 * @map: Output block mapping
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_ext_get_block(struct nexfs_inode *inode, u64 iblock,
                        struct nexfs_block_map *map)
{
    struct nexfs_extent_header *eh;
    struct nexfs_extent *extent;
    u32 i;

    if (!inode || !map) {
        return VFS_EINVAL;
    }

    memset(map, 0, sizeof(struct nexfs_block_map));
    map->b_size = NEXFS_BLOCK_SIZE;

    /* Get extent header */
    eh = (struct nexfs_extent_header *)inode->raw.i_data.extents.i_extent_reserved;

    if (eh->eh_magic != NEXFS_EXT_MAGIC) {
        return VFS_EINVAL;
    }

    /* Search extents */
    extent = (struct nexfs_extent *)(eh + 1);

    for (i = 0; i < eh->eh_entries; i++) {
        if (iblock >= extent[i].ee_block &&
            iblock < extent[i].ee_block + extent[i].ee_len) {
            /* Found extent */
            u64 offset = iblock - extent[i].ee_block;
            map->b_blocknr = extent[i].ee_start + offset;
            map->b_state = NEXFS_BH_Mapped | NEXFS_BH_Read;
            return VFS_OK;
        }
    }

    return VFS_ENOENT;
}

/**
 * nexfs_ext_alloc_block - Allocate block using extents
 * @inode: File inode
 * @iblock: Logical block number
 * @phys: Output physical block number
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_ext_alloc_block(struct nexfs_inode *inode, u64 iblock, u64 *phys)
{
    struct nexfs_superblock *sb;
    u64 block;

    if (!inode || !phys) {
        return VFS_EINVAL;
    }

    sb = (struct nexfs_superblock *)inode->vfs_inode.i_sb;

    /* Allocate new block */
    block = nexfs_alloc_block(sb, inode->last_alloc_group);
    if (!block) {
        return VFS_ENOSPC;
    }

    /* TODO: Add to extent tree */

    *phys = block;
    inode->last_alloc_block = (u32)iblock;

    return VFS_OK;
}

/**
 * nexfs_ext_free_block - Free block from extents
 * @inode: File inode
 * @iblock: Logical block number
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_ext_free_block(struct nexfs_inode *inode, u64 iblock)
{
    /* TODO: Remove from extent tree and free block */
    return VFS_OK;
}
