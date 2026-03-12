/*
 * NEXUS OS - Virtual File System
 * fs/vfs/mount.c
 *
 * Mount Point Management - Filesystem mounting and unmounting
 */

#include "vfs.h"

/*===========================================================================*/
/*                         MOUNT CONFIGURATION                               */
/*===========================================================================*/

#define VFS_MOUNT_TABLE_SIZE    64
#define VFS_MOUNT_ID_START      1

/* Mount table */
static struct vfs_mount *mount_table[VFS_MOUNT_TABLE_SIZE];
static u32 next_mount_id = VFS_MOUNT_ID_START;
static spinlock_t mount_table_lock = { .lock = 0 };

/* Mount statistics */
static struct {
    atomic_t total_mounts;
    atomic_t total_umounts;
    atomic_t active_mounts;
    u64 total_mount_time;
} mount_stats;

/*===========================================================================*/
/*                         MOUNT INITIALIZATION                              */
/*===========================================================================*/

/**
 * vfs_mount_init - Initialize mount subsystem
 *
 * Initializes mount table and related structures.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_mount_init(void)
{
    u32 i;

    /* Initialize mount table */
    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        mount_table[i] = NULL;
    }

    next_mount_id = VFS_MOUNT_ID_START;

    /* Initialize lock */
    spin_lock_init(&mount_table_lock);

    /* Initialize statistics */
    atomic_set(&mount_stats.total_mounts, 0);
    atomic_set(&mount_stats.total_umounts, 0);
    atomic_set(&mount_stats.active_mounts, 0);
    mount_stats.total_mount_time = 0;

    pr_debug("VFS: Mount subsystem initialized (table size=%u)\n", VFS_MOUNT_TABLE_SIZE);

    return VFS_OK;
}

/*===========================================================================*/
/*                         MOUNT ALLOCATION                                  */
/*===========================================================================*/

/**
 * vfs_mount_alloc - Allocate a new mount structure
 *
 * Allocates and initializes a new mount structure.
 *
 * Returns: New mount or NULL on failure
 */
static struct vfs_mount *vfs_mount_alloc(void)
{
    struct vfs_mount *mount;
    u32 i;

    mount = (struct vfs_mount *)kzalloc(sizeof(struct vfs_mount));
    if (!mount) {
        return NULL;
    }

    /* Initialize mount */
    mount->magic = VFS_MAGIC;
    mount->mnt_id = 0;
    mount->mnt_flags = 0;
    mount->mnt_mountpoint = NULL;
    mount->mnt_root = NULL;
    mount->mnt_sb = NULL;
    mount->mnt_parent = NULL;
    mount->mnt_private = NULL;

    /* Initialize lists */
    INIT_LIST_HEAD(&mount->mnt_child);
    INIT_LIST_HEAD(&mount->mnt_mounts);
    INIT_LIST_HEAD(&mount->mnt_list);
    INIT_LIST_HEAD(&mount->mnt_expire);
    INIT_LIST_HEAD(&mount->mnt_share);

    /* Initialize reference count */
    atomic_set(&mount->mnt_count, 1);

    /* Initialize lock */
    spin_lock_init(&mount->mnt_lock);

    /* Find free slot in mount table */
    spin_lock(&mount_table_lock);

    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        if (!mount_table[i]) {
            mount->mnt_id = next_mount_id++;
            if (next_mount_id == 0) {
                next_mount_id = VFS_MOUNT_ID_START;
            }
            mount_table[i] = mount;
            spin_unlock(&mount_table_lock);
            break;
        }
    }

    if (i >= VFS_MOUNT_TABLE_SIZE) {
        spin_unlock(&mount_table_lock);
        kfree(mount);
        return NULL;
    }

    atomic_inc(&mount_stats.total_mounts);
    atomic_inc(&mount_stats.active_mounts);
    atomic_inc(&vfs_global.stats.total_mounts);

    pr_debug("VFS: Allocated mount %u\n", mount->mnt_id);

    return mount;
}

/**
 * vfs_mount_free - Free a mount structure
 * @mount: Mount to free
 *
 * Releases all resources associated with a mount.
 */
static void vfs_mount_free(struct vfs_mount *mount)
{
    u32 i;

    if (!mount) {
        return;
    }

    /* Remove from mount table */
    spin_lock(&mount_table_lock);

    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        if (mount_table[i] == mount) {
            mount_table[i] = NULL;
            break;
        }
    }

    spin_unlock(&mount_table_lock);

    /* Clear magic */
    mount->magic = 0;

    kfree(mount);

    atomic_inc(&mount_stats.total_umounts);
    atomic_dec(&mount_stats.active_mounts);
    atomic_dec(&vfs_global.stats.total_mounts);

    pr_debug("VFS: Freed mount\n");
}

/*===========================================================================*/
/*                         MOUNT REFERENCE COUNTING                          */
/*===========================================================================*/

/**
 * vfs_mount_get - Get reference to mount
 * @mount: Mount to reference
 *
 * Increments the mount reference count.
 */
static void vfs_mount_get(struct vfs_mount *mount)
{
    if (!mount || mount->magic != VFS_MAGIC) {
        return;
    }

    atomic_inc(&mount->mnt_count);
}

/**
 * vfs_mount_put - Release mount reference
 * @mount: Mount to release
 *
 * Decrements the mount reference count.
 */
static void vfs_mount_put(struct vfs_mount *mount)
{
    if (!mount || mount->magic != VFS_MAGIC) {
        return;
    }

    if (atomic_dec_and_test(&mount->mnt_count)) {
        vfs_mount_free(mount);
    }
}

/*===========================================================================*/
/*                         MOUNT OPERATIONS                                  */
/*===========================================================================*/

/**
 * vfs_do_mount - Perform filesystem mount
 * @fs: Filesystem type
 * @flags: Mount flags
 * @dev_name: Device name
 * @data: Filesystem-specific data
 * @mount: Output mount pointer
 *
 * Internal mount function that creates the mount structure.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 vfs_do_mount(struct vfs_filesystem *fs, u32 flags, const char *dev_name,
                        void *data, struct vfs_mount **mount)
{
    struct vfs_mount *mnt;
    struct vfs_superblock *sb;
    u64 start_time;
    s32 ret;

    if (!fs || !mount) {
        return VFS_EINVAL;
    }

    start_time = get_time_ms();

    /* Allocate mount structure */
    mnt = vfs_mount_alloc();
    if (!mnt) {
        return VFS_ENOMEM;
    }

    mnt->mnt_flags = flags;

    /* Call filesystem mount function */
    if (fs->mount) {
        ret = fs->mount(fs, flags, dev_name, data, &mnt);
        if (ret != VFS_OK) {
            vfs_mount_free(mnt);
            return ret;
        }
    } else {
        /* Default mount implementation */
        sb = (struct vfs_superblock *)kzalloc(sizeof(struct vfs_superblock));
        if (!sb) {
            vfs_mount_free(mnt);
            return VFS_ENOMEM;
        }

        /* Initialize superblock */
        sb->magic = VFS_MAGIC;
        sb->s_dev = 0; /* TODO: Get from dev_name */
        sb->s_type = fs->fs_type;
        sb->s_flags = flags;
        sb->s_op = fs->fs_op;
        sb->s_mount = mnt;
        sb->s_time = get_time_ms() / 1000;

        if (dev_name) {
            sb->s_dev_name = (char *)kmalloc(strlen(dev_name) + 1);
            if (sb->s_dev_name) {
                strcpy(sb->s_dev_name, dev_name);
            }
        }

        /* Initialize inode lists */
        INIT_LIST_HEAD(&sb->s_inodes);
        INIT_LIST_HEAD(&sb->s_dirty_inodes);
        INIT_LIST_HEAD(&sb->s_unused_inodes);

        spin_lock_init(&sb->s_lock);
        atomic_set(&sb->s_count, 1);

        mnt->mnt_sb = sb;
    }

    mnt->mnt_time = get_time_ms() / 1000;

    *mount = mnt;

    mount_stats.total_mount_time += get_time_ms() - start_time;

    pr_info("VFS: Mounted filesystem '%s' (id=%u)\n", fs->name, mnt->mnt_id);

    return VFS_OK;
}

/**
 * vfs_mount - Mount a filesystem
 * @dev_name: Device name
 * @dir_name: Directory to mount on
 * @type: Filesystem type name
 * @flags: Mount flags
 * @data: Filesystem-specific data
 *
 * Mounts a filesystem at the specified directory.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_mount(const char *dev_name, const char *dir_name, const char *type,
              u32 flags, void *data)
{
    struct vfs_filesystem *fs;
    struct vfs_mount *mnt;
    struct vfs_dentry *dir_dentry;
    s32 ret;

    if (!dir_name || !type) {
        return VFS_EINVAL;
    }

    if (!vfs_global.initialized) {
        return VFS_ENODEV;
    }

    pr_info("VFS: Mounting '%s' on '%s' (type=%s)\n",
            dev_name ? dev_name : "none", dir_name, type);

    /* Get filesystem type */
    fs = vfs_get_filesystem(type);
    if (!fs) {
        pr_err("VFS: Unknown filesystem type '%s'\n", type);
        return VFS_ENODEV;
    }

    /* Lookup mount point directory */
    ret = vfs_path_lookup(dir_name, 0, &dir_dentry);
    if (ret != VFS_OK) {
        pr_err("VFS: Mount point '%s' not found: %d\n", dir_name, ret);
        return ret;
    }

    /* Verify it's a directory */
    if (!dir_dentry->d_inode ||
        dir_dentry->d_inode->type != VFS_TYPE_DIRECTORY) {
        vfs_d_put(dir_dentry);
        pr_err("VFS: '%s' is not a directory\n", dir_name);
        return VFS_ENOTDIR;
    }

    /* Check if already mounted */
    if (dir_dentry->d_mount) {
        vfs_d_put(dir_dentry);
        pr_err("VFS: '%s' is already a mount point\n", dir_name);
        return VFS_EBUSY;
    }

    /* Perform mount */
    ret = vfs_do_mount(fs, flags, dev_name, data, &mnt);
    if (ret != VFS_OK) {
        vfs_d_put(dir_dentry);
        pr_err("VFS: Mount failed: %d\n", ret);
        return ret;
    }

    /* Connect mount to dentry */
    mnt->mnt_mountpoint = dir_dentry;
    mnt->mnt_parent = vfs_global.root_mount;
    dir_dentry->d_mount = mnt;

    /* Set root dentry */
    if (mnt->mnt_sb && mnt->mnt_sb->s_root_dentry) {
        mnt->mnt_root = mnt->mnt_sb->s_root_dentry;
    }

    /* Handle root mount */
    if (strcmp(dir_name, "/") == 0) {
        vfs_global.root_mount = mnt;
        vfs_global.root_dentry = mnt->mnt_root;
        vfs_global.pwd = mnt->mnt_root;
        vfs_d_get(vfs_global.root_dentry);
    }

    vfs_mount_get(mnt);

    pr_info("VFS: Mount successful (id=%u)\n", mnt->mnt_id);

    return VFS_OK;
}

/**
 * vfs_do_umount - Perform filesystem unmount
 * @mount: Mount to unmount
 *
 * Internal unmount function.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 vfs_do_umount(struct vfs_mount *mount)
{
    struct vfs_superblock *sb;
    s32 ret;

    if (!mount || mount->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    sb = mount->mnt_sb;

    /* Check for busy mount */
    if (atomic_read(&mount->mnt_count) > 1) {
        pr_err("VFS: Mount %u is busy\n", mount->mnt_id);
        return VFS_EBUSY;
    }

    /* Check for child mounts */
    if (!list_empty(&mount->mnt_mounts)) {
        pr_err("VFS: Mount %u has child mounts\n", mount->mnt_id);
        return VFS_EBUSY;
    }

    /* Sync filesystem */
    if (sb && sb->s_op && sb->s_op->sync_fs) {
        sb->s_op->sync_fs(sb, 1);
    }

    /* Call filesystem umount */
    if (mount->mnt_sb && mount->mnt_sb->s_type &&
        mount->mnt_sb->s_type->umount) {
        ret = mount->mnt_sb->s_type->umount(mount);
        if (ret != VFS_OK) {
            return ret;
        }
    }

    /* Evict all inodes */
    if (sb) {
        vfs_evict_inodes(sb);
    }

    /* Release superblock */
    if (sb) {
        if (sb->s_op && sb->s_op->put_super) {
            sb->s_op->put_super(sb);
        }

        if (sb->s_dev_name) {
            kfree(sb->s_dev_name);
        }

        kfree(sb);
        mount->mnt_sb = NULL;
    }

    /* Disconnect from mount point */
    if (mount->mnt_mountpoint) {
        mount->mnt_mountpoint->d_mount = NULL;
        vfs_d_put(mount->mnt_mountpoint);
        mount->mnt_mountpoint = NULL;
    }

    return VFS_OK;
}

/**
 * vfs_umount - Unmount a filesystem
 * @dir_name: Directory mounted on
 *
 * Unmounts the filesystem at the specified directory.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_umount(const char *dir_name)
{
    struct vfs_dentry *dir_dentry;
    struct vfs_mount *mount;
    s32 ret;

    if (!dir_name) {
        return VFS_EINVAL;
    }

    if (!vfs_global.initialized) {
        return VFS_ENODEV;
    }

    pr_info("VFS: Unmounting '%s'\n", dir_name);

    /* Lookup mount point */
    ret = vfs_path_lookup(dir_name, VFS_FO_DIRECTORY, &dir_dentry);
    if (ret != VFS_OK) {
        pr_err("VFS: Path '%s' not found\n", dir_name);
        return ret;
    }

    /* Get mount */
    mount = dir_dentry->d_mount;
    if (!mount) {
        vfs_d_put(dir_dentry);
        pr_err("VFS: '%s' is not a mount point\n", dir_name);
        return VFS_EINVAL;
    }

    vfs_d_put(dir_dentry);

    /* Perform unmount */
    ret = vfs_do_umount(mount);
    if (ret != VFS_OK) {
        pr_err("VFS: Unmount failed: %d\n", ret);
        return ret;
    }

    /* Release mount */
    vfs_mount_put(mount);

    pr_info("VFS: Unmount successful\n");

    return VFS_OK;
}

/**
 * vfs_umount_all - Unmount all filesystems
 *
 * Unmounts all mounted filesystems. Called during shutdown.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_umount_all(void)
{
    struct vfs_mount *mount;
    u32 i;
    s32 ret;
    s32 error = VFS_OK;

    pr_info("VFS: Unmounting all filesystems...\n");

    spin_lock(&mount_table_lock);

    /* Unmount in reverse order */
    for (i = VFS_MOUNT_TABLE_SIZE; i > 0; i--) {
        mount = mount_table[i - 1];
        if (mount) {
            spin_unlock(&mount_table_lock);

            ret = vfs_do_umount(mount);
            if (ret != VFS_OK) {
                pr_err("VFS: Failed to unmount %u: %d\n", mount->mnt_id, ret);
                error = ret;
            } else {
                vfs_mount_put(mount);
            }

            spin_lock(&mount_table_lock);
        }
    }

    spin_unlock(&mount_table_lock);

    /* Clear root mount */
    vfs_global.root_mount = NULL;
    vfs_global.root_dentry = NULL;
    vfs_global.pwd = NULL;

    pr_info("VFS: All filesystems unmounted\n");

    return error;
}

/*===========================================================================*/
/*                         MOUNT LOOKUP                                      */
/*===========================================================================*/

/**
 * vfs_lookup_mount - Find mount for a path
 * @path: Path to lookup
 *
 * Returns: Mount structure or NULL
 */
struct vfs_mount *vfs_lookup_mount(const char *path)
{
    struct vfs_dentry *dentry;
    struct vfs_mount *mount;
    s32 ret;

    if (!path) {
        return NULL;
    }

    ret = vfs_path_lookup(path, VFS_FO_DIRECTORY, &dentry);
    if (ret != VFS_OK) {
        return NULL;
    }

    mount = dentry->d_mount;

    vfs_d_put(dentry);

    return mount;
}

/**
 * vfs_find_mount_by_id - Find mount by ID
 * @id: Mount ID
 *
 * Returns: Mount structure or NULL
 */
static struct vfs_mount *vfs_find_mount_by_id(u32 id)
{
    struct vfs_mount *mount;
    u32 i;

    spin_lock(&mount_table_lock);

    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        mount = mount_table[i];
        if (mount && mount->mnt_id == id) {
            spin_unlock(&mount_table_lock);
            return mount;
        }
    }

    spin_unlock(&mount_table_lock);

    return NULL;
}

/*===========================================================================*/
/*                         MOUNT ITERATION                                   */
/*===========================================================================*/

/**
 * vfs_for_each_mount - Iterate over all mounts
 * @func: Callback function
 * @data: User data
 *
 * Calls func for each mounted filesystem.
 */
void vfs_for_each_mount(s32 (*func)(struct vfs_mount *, void *), void *data)
{
    struct vfs_mount *mount;
    u32 i;

    if (!func) {
        return;
    }

    spin_lock(&mount_table_lock);

    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        mount = mount_table[i];
        if (mount) {
            spin_unlock(&mount_table_lock);

            if (func(mount, data) != 0) {
                return;
            }

            spin_lock(&mount_table_lock);
        }
    }

    spin_unlock(&mount_table_lock);
}

/*===========================================================================*/
/*                         MOUNT FLAGS                                       */
/*===========================================================================*/

/**
 * vfs_set_mount_flags - Set mount flags
 * @mount: Mount to modify
 * @flags: New flags
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_set_mount_flags(struct vfs_mount *mount, u32 flags)
{
    if (!mount || mount->magic != VFS_MAGIC) {
        return VFS_EINVAL;
    }

    spin_lock(&mount->mnt_lock);
    mount->mnt_flags = flags;
    spin_unlock(&mount->mnt_lock);

    if (mount->mnt_sb) {
        mount->mnt_sb->s_flags = flags;
    }

    return VFS_OK;
}

/**
 * vfs_get_mount_flags - Get mount flags
 * @mount: Mount to query
 *
 * Returns: Mount flags
 */
u32 vfs_get_mount_flags(struct vfs_mount *mount)
{
    u32 flags;

    if (!mount || mount->magic != VFS_MAGIC) {
        return 0;
    }

    spin_lock(&mount->mnt_lock);
    flags = mount->mnt_flags;
    spin_unlock(&mount->mnt_lock);

    return flags;
}

/*===========================================================================*/
/*                         MOUNT STATISTICS                                  */
/*===========================================================================*/

/**
 * vfs_mount_stats_print - Print mount statistics
 */
void vfs_mount_stats_print(void)
{
    printk("\n=== Mount Statistics ===\n");
    printk("Total Mounts:    %d\n", atomic_read(&mount_stats.total_mounts));
    printk("Total Umounts:   %d\n", atomic_read(&mount_stats.total_umounts));
    printk("Active Mounts:   %d\n", atomic_read(&mount_stats.active_mounts));
    printk("Total Mount Time: %llu ms\n", (unsigned long long)mount_stats.total_mount_time);

    if (atomic_read(&mount_stats.total_mounts) > 0) {
        u32 avg_time = (u32)(mount_stats.total_mount_time /
                             atomic_read(&mount_stats.total_mounts));
        printk("Avg Mount Time:  %u ms\n", avg_time);
    }
}

/**
 * vfs_mount_dump - Dump all mounts
 */
void vfs_mount_dump(void)
{
    struct vfs_mount *mount;
    u32 i;
    u32 count = 0;

    printk("\n=== Mount Dump ===\n");

    spin_lock(&mount_table_lock);

    for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++) {
        mount = mount_table[i];
        if (mount) {
            printk("Mount %3u: flags=0x%04X, sb=%p, root=%p, mountpoint=%p\n",
                   mount->mnt_id,
                   mount->mnt_flags,
                   mount->mnt_sb,
                   mount->mnt_root,
                   mount->mnt_mountpoint);

            if (mount->mnt_sb) {
                printk("         Superblock: dev=%u, type=%u, inodes=%llu/%llu\n",
                       (u32)mount->mnt_sb->s_dev,
                       mount->mnt_sb->s_type,
                       (unsigned long long)(mount->mnt_sb->s_inodes - mount->mnt_sb->s_free_inodes),
                       (unsigned long long)mount->mnt_sb->s_inodes);
            }

            count++;
        }
    }

    spin_unlock(&mount_table_lock);

    printk("Total: %u mounts\n", count);
}

/*===========================================================================*/
/*                         MOUNT INFO                                        */
/*===========================================================================*/

/**
 * vfs_mount_info - Get mount information
 * @mount: Mount to query
 * @info: Output info structure
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_mount_info(struct vfs_mount *mount, struct vfs_mount_info *info)
{
    if (!mount || mount->magic != VFS_MAGIC || !info) {
        return VFS_EINVAL;
    }

    memset(info, 0, sizeof(struct vfs_mount_info));

    info->mnt_id = mount->mnt_id;
    info->mnt_flags = mount->mnt_flags;
    info->mnt_count = atomic_read(&mount->mnt_count);

    if (mount->mnt_sb) {
        info->s_dev = mount->mnt_sb->s_dev;
        info->s_size = mount->mnt_sb->s_size;
        info->s_free = mount->mnt_sb->s_free_blocks;
        info->s_bsize = mount->mnt_sb->s_bsize;
        info->s_inodes = mount->mnt_sb->s_inodes;
        info->s_free_inodes = mount->mnt_sb->s_free_inodes;

        if (mount->mnt_sb->s_dev_name) {
            strncpy(info->s_dev_name, mount->mnt_sb->s_dev_name,
                    sizeof(info->s_dev_name) - 1);
        }
    }

    if (mount->mnt_mountpoint && mount->mnt_mountpoint->d_name) {
        strncpy(info->mnt_point, mount->mnt_mountpoint->d_name,
                sizeof(info->mnt_point) - 1);
    }

    return VFS_OK;
}
