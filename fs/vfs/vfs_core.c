/*
 * NEXUS OS - Virtual File System
 * fs/vfs/vfs_core.c
 *
 * VFS Core Implementation - Main VFS initialization and core functions
 */

#include "vfs.h"

/*===========================================================================*/
/*                         VFS GLOBAL STATE                                  */
/*===========================================================================*/

static struct vfs_global vfs_global = {
    .initialized = false,
    .init_count = 0,
    .magic = VFS_MAGIC,
};

/* Registered Filesystems List */
static LIST_HEAD(vfs_filesystems);
static spinlock_t vfs_filesystems_lock = { .lock = 0 };

/* Root filesystem */
static struct vfs_mount *root_mount = NULL;
static struct vfs_dentry *root_dentry = NULL;

/*===========================================================================*/
/*                         HASH FUNCTIONS                                    */
/*===========================================================================*/

/**
 * vfs_hash_string - Hash a string for dentry/inode lookup
 * @str: String to hash
 * @len: Length of string
 *
 * Returns: Hash value
 *
 * Uses a variant of the djb2 hash algorithm for fast string hashing.
 */
u64 vfs_hash_string(const char *str, u64 len)
{
    u64 hash = 5381;
    u64 i;

    if (!str) {
        return 0;
    }

    if (len == 0) {
        len = strlen(str);
    }

    for (i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }

    return hash;
}

/**
 * vfs_inode_hash - Get hash bucket for inode
 * @sb: Superblock
 * @ino: Inode number
 *
 * Returns: Hash bucket index
 */
static inline u32 vfs_inode_hash(dev_t dev, u64 ino)
{
    u64 hash = ino ^ (u64)dev;
    return (u32)(hash & (VFS_INODE_HASH_SIZE - 1));
}

/**
 * vfs_dentry_hash - Get hash bucket for dentry
 * @parent: Parent dentry
 * @name: Entry name
 *
 * Returns: Hash bucket index
 */
static inline u32 vfs_dentry_hash(struct vfs_dentry *parent, const char *name)
{
    u64 hash = vfs_hash_string(name, 0);

    if (parent) {
        hash ^= (u64)(uintptr)parent;
    }

    return (u32)(hash & (VFS_DENTRY_HASH_SIZE - 1));
}

/*===========================================================================*/
/*                         VFS INITIALIZATION                                */
/*===========================================================================*/

/**
 * vfs_init_early - Early VFS initialization
 *
 * Performs early initialization of VFS data structures.
 * Called during kernel early boot.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 vfs_init_early(void)
{
    u32 i;

    pr_info("Initializing VFS (early)...\n");

    /* Initialize locks */
    spin_lock_init(&vfs_global.lock);
    spin_lock_init(&vfs_global.fs_lock);
    spin_lock_init(&vfs_global.mount_lock);
    spin_lock_init(&vfs_global.inode_lock);
    spin_lock_init(&vfs_global.dentry_lock);
    spin_lock_init(&vfs_global.file_lock);

    /* Initialize hash tables */
    for (i = 0; i < VFS_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&vfs_global.inode_hash[i]);
    }

    for (i = 0; i < VFS_DENTRY_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&vfs_global.dentry_hash[i]);
    }

    /* Initialize lists */
    INIT_LIST_HEAD(&vfs_global.inode_unused);
    INIT_LIST_HEAD(&vfs_global.inode_dirty);
    INIT_LIST_HEAD(&vfs_global.dentry_lru);
    INIT_LIST_HEAD(&vfs_global.filesystems);

    /* Initialize statistics */
    atomic_set(&vfs_global.stats.total_inodes, 0);
    atomic_set(&vfs_global.stats.total_dentries, 0);
    atomic_set(&vfs_global.stats.total_files, 0);
    atomic_set(&vfs_global.stats.total_mounts, 0);
    atomic_set(&vfs_global.stats.total_reads, 0);
    atomic_set(&vfs_global.stats.total_writes, 0);
    atomic_set(&vfs_global.stats.total_opens, 0);
    atomic_set(&vfs_global.stats.total_closes, 0);
    atomic_set(&vfs_global.stats.total_lookups, 0);
    vfs_global.stats.bytes_read = 0;
    vfs_global.stats.bytes_written = 0;
    vfs_global.stats.cache_hits = 0;
    vfs_global.stats.cache_misses = 0;

    /* Allocate file table */
    vfs_global.max_files = VFS_MAX_OPEN_FILES;
    vfs_global.files = (struct vfs_file **)kzalloc(
        sizeof(struct vfs_file *) * VFS_MAX_OPEN_FILES);

    if (!vfs_global.files) {
        pr_err("VFS: Failed to allocate file table\n");
        return VFS_ENOMEM;
    }

    pr_info("  File table: %u entries\n", VFS_MAX_OPEN_FILES);
    pr_info("  Inode hash: %u buckets\n", VFS_INODE_HASH_SIZE);
    pr_info("  Dentry hash: %u buckets\n", VFS_DENTRY_HASH_SIZE);

    return VFS_OK;
}

/**
 * vfs_init - Initialize VFS subsystem
 *
 * Main VFS initialization function. Called during kernel initialization.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_init(void)
{
    s32 ret;

    /* Check if already initialized */
    spin_lock(&vfs_global.lock);

    if (vfs_global.init_count > 0) {
        vfs_global.init_count++;
        spin_unlock(&vfs_global.lock);
        return VFS_OK;
    }

    spin_unlock(&vfs_global.lock);

    pr_info("Initializing Virtual File System...\n");

    /* Early initialization */
    ret = vfs_init_early();
    if (ret != VFS_OK) {
        pr_err("VFS: Early initialization failed: %d\n", ret);
        return ret;
    }

    /* Initialize inode cache */
    ret = vfs_inode_cache_init();
    if (ret != VFS_OK) {
        pr_err("VFS: Inode cache initialization failed: %d\n", ret);
        goto err_inode;
    }
    pr_info("  Inode cache initialized\n");

    /* Initialize dentry cache */
    ret = vfs_dentry_cache_init();
    if (ret != VFS_OK) {
        pr_err("VFS: Dentry cache initialization failed: %d\n", ret);
        goto err_dentry;
    }
    pr_info("  Dentry cache initialized\n");

    /* Initialize mount subsystem */
    ret = vfs_mount_init();
    if (ret != VFS_OK) {
        pr_err("VFS: Mount subsystem initialization failed: %d\n", ret);
        goto err_mount;
    }
    pr_info("  Mount subsystem initialized\n");

    /* Mark as initialized */
    spin_lock(&vfs_global.lock);
    vfs_global.initialized = true;
    vfs_global.init_count = 1;
    spin_unlock(&vfs_global.lock);

    pr_info("Virtual File System initialized.\n");

    return VFS_OK;

err_mount:
    vfs_dentry_cache_exit();
err_dentry:
    vfs_inode_cache_exit();
err_inode:
    if (vfs_global.files) {
        kfree(vfs_global.files);
        vfs_global.files = NULL;
    }

    return ret;
}

/**
 * vfs_exit - Shutdown VFS subsystem
 *
 * Cleans up all VFS resources. Called during system shutdown.
 */
void vfs_exit(void)
{
    struct vfs_filesystem *fs;
    struct vfs_filesystem *fs_next;
    u32 i;

    spin_lock(&vfs_global.lock);

    if (!vfs_global.initialized || vfs_global.init_count > 1) {
        if (vfs_global.init_count > 0) {
            vfs_global.init_count--;
        }
        spin_unlock(&vfs_global.lock);
        return;
    }

    vfs_global.initialized = false;

    spin_unlock(&vfs_global.lock);

    pr_info("Shutting down Virtual File System...\n");

    /* Unmount all filesystems */
    vfs_umount_all();

    /* Unregister all filesystems */
    spin_lock(&vfs_filesystems_lock);

    list_for_each_entry_safe(fs, fs_next, &vfs_filesystems, fs_list) {
        if (fs->exit) {
            fs->exit(fs);
        }
        list_del(&fs->fs_list);
        kfree(fs->name);
        kfree(fs);
    }

    spin_unlock(&vfs_filesystems_lock);

    /* Free file table */
    if (vfs_global.files) {
        for (i = 0; i < vfs_global.max_files; i++) {
            if (vfs_global.files[i]) {
                vfs_put_file(vfs_global.files[i]);
            }
        }
        kfree(vfs_global.files);
        vfs_global.files = NULL;
    }

    /* Cleanup caches */
    vfs_dentry_cache_exit();
    vfs_inode_cache_exit();

    pr_info("Virtual File System shutdown complete.\n");
}

/*===========================================================================*/
/*                         FILESYSTEM REGISTRATION                           */
/*===========================================================================*/

/**
 * vfs_register_filesystem - Register a filesystem type
 * @fs: Filesystem structure to register
 *
 * Registers a new filesystem type with the VFS layer.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_register_filesystem(struct vfs_filesystem *fs)
{
    struct vfs_filesystem *existing;

    if (!fs || !fs->name) {
        return VFS_EINVAL;
    }

    if (fs->magic != VFS_MAGIC) {
        pr_err("VFS: Invalid filesystem magic for '%s'\n", fs->name);
        return VFS_EINVAL;
    }

    spin_lock(&vfs_filesystems_lock);

    /* Check for duplicate */
    list_for_each_entry(existing, &vfs_filesystems, fs_list) {
        if (strcmp(existing->name, fs->name) == 0) {
            spin_unlock(&vfs_filesystems_lock);
            pr_err("VFS: Filesystem '%s' already registered\n", fs->name);
            return VFS_EEXIST;
        }
    }

    /* Initialize filesystem */
    if (fs->init) {
        s32 ret = fs->init(fs);
        if (ret != VFS_OK) {
            spin_unlock(&vfs_filesystems_lock);
            pr_err("VFS: Filesystem '%s' init failed: %d\n", fs->name, ret);
            return ret;
        }
    }

    /* Add to list */
    list_add_tail(&fs->fs_list, &vfs_filesystems);

    spin_unlock(&vfs_filesystems_lock);

    pr_info("VFS: Registered filesystem '%s'\n", fs->name);

    return VFS_OK;
}

/**
 * vfs_unregister_filesystem - Unregister a filesystem type
 * @fs: Filesystem structure to unregister
 *
 * Removes a filesystem type from the VFS layer.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_unregister_filesystem(struct vfs_filesystem *fs)
{
    struct vfs_filesystem *existing;
    bool found = false;

    if (!fs) {
        return VFS_EINVAL;
    }

    spin_lock(&vfs_filesystems_lock);

    /* Find and remove */
    list_for_each_entry(existing, &vfs_filesystems, fs_list) {
        if (existing == fs) {
            found = true;
            break;
        }
    }

    if (!found) {
        spin_unlock(&vfs_filesystems_lock);
        return VFS_ENOENT;
    }

    /* Call exit handler */
    if (fs->exit) {
        fs->exit(fs);
    }

    list_del(&fs->fs_list);

    spin_unlock(&vfs_filesystems_lock);

    pr_info("VFS: Unregistered filesystem '%s'\n", fs->name);

    return VFS_OK;
}

/**
 * vfs_get_filesystem - Get filesystem type by name
 * @name: Filesystem name
 *
 * Returns: Filesystem structure or NULL if not found
 */
struct vfs_filesystem *vfs_get_filesystem(const char *name)
{
    struct vfs_filesystem *fs;

    if (!name) {
        return NULL;
    }

    spin_lock(&vfs_filesystems_lock);

    list_for_each_entry(fs, &vfs_filesystems, fs_list) {
        if (strcmp(fs->name, name) == 0) {
            spin_unlock(&vfs_filesystems_lock);
            return fs;
        }
    }

    spin_unlock(&vfs_filesystems_lock);

    return NULL;
}

/**
 * vfs_list_filesystems - List all registered filesystems
 *
 * Prints information about all registered filesystem types.
 */
void vfs_list_filesystems(void)
{
    struct vfs_filesystem *fs;

    printk("\n=== Registered Filesystems ===\n");

    spin_lock(&vfs_filesystems_lock);

    list_for_each_entry(fs, &vfs_filesystems, fs_list) {
        printk("  - %s (magic: 0x%08X)\n", fs->name, fs->fs_magic);
    }

    spin_unlock(&vfs_filesystems_lock);
}

/*===========================================================================*/
/*                         PATH LOOKUP                                       */
/*===========================================================================*/

/**
 * vfs_path_is_absolute - Check if path is absolute
 * @path: Path to check
 *
 * Returns: true if absolute, false otherwise
 */
bool vfs_path_is_absolute(const char *path)
{
    return (path && path[0] == '/');
}

/**
 * vfs_path_normalize - Normalize a path string
 * @path: Path to normalize (modified in place)
 *
 * Removes redundant slashes, handles . and .. components.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_path_normalize(char *path)
{
    char *src;
    char *dst;
    bool is_absolute;

    if (!path || !path[0]) {
        return VFS_EINVAL;
    }

    is_absolute = (path[0] == '/');
    src = path;
    dst = path;

    while (*src) {
        /* Skip multiple slashes */
        if (*src == '/') {
            *dst++ = *src++;
            while (*src == '/') {
                src++;
            }
        }
        /* Handle . and .. */
        else if (*src == '.' && (src[1] == '/' || src[1] == '\0')) {
            /* Skip single dot */
            src += (src[1] == '/') ? 2 : 1;
        }
        else if (*src == '.' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
            /* Handle double dot - go back to previous component */
            if (dst > path + (is_absolute ? 1 : 0)) {
                dst--;
                while (dst > path && *dst != '/') {
                    dst--;
                }
                if (dst > path) {
                    dst++;
                }
            }
            src += (src[2] == '/') ? 3 : 2;
        }
        else {
            /* Copy regular character */
            *dst++ = *src++;
        }
    }

    /* Remove trailing slash (except for root) */
    if (dst > path + 1 && *(dst - 1) == '/') {
        dst--;
    }

    *dst = '\0';

    /* Handle empty path */
    if (dst == path) {
        if (is_absolute) {
            path[0] = '/';
            path[1] = '\0';
        } else {
            path[0] = '.';
            path[1] = '\0';
        }
    }

    return VFS_OK;
}

/**
 * vfs_path_split - Split path into directory and base name
 * @path: Full path
 * @dir: Output directory (caller must free)
 * @base: Output base name (caller must free)
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_path_split(const char *path, char **dir, char **base)
{
    const char *last_slash;
    size_t dir_len;
    size_t base_len;

    if (!path || !dir || !base) {
        return VFS_EINVAL;
    }

    /* Find last slash */
    last_slash = strrchr(path, '/');

    if (!last_slash || last_slash == path) {
        /* Root or no directory component */
        *dir = (char *)kmalloc(2);
        if (!*dir) {
            return VFS_ENOMEM;
        }
        strcpy(*dir, "/");

        *base = (char *)kmalloc(strlen(path) + 1);
        if (!*base) {
            kfree(*dir);
            *dir = NULL;
            return VFS_ENOMEM;
        }
        strcpy(*base, path[0] == '/' ? path + 1 : path);
    } else {
        /* Has directory component */
        dir_len = last_slash - path;
        base_len = strlen(last_slash + 1);

        *dir = (char *)kmalloc(dir_len + 1);
        if (!*dir) {
            return VFS_ENOMEM;
        }
        strncpy(*dir, path, dir_len);
        (*dir)[dir_len] = '\0';

        *base = (char *)kmalloc(base_len + 1);
        if (!*base) {
            kfree(*dir);
            *dir = NULL;
            return VFS_ENOMEM;
        }
        strcpy(*base, last_slash + 1);
    }

    return VFS_OK;
}

/**
 * vfs_path_lookup - Lookup a path and return dentry
 * @path: Path to lookup
 * @flags: Lookup flags
 * @dentry: Output dentry pointer
 *
 * Performs a full path lookup, handling mount points and symlinks.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_path_lookup(const char *path, u32 flags, struct vfs_dentry **dentry)
{
    struct vfs_dentry *current;
    struct vfs_dentry *parent;
    struct vfs_dentry *child;
    char *path_copy;
    char *component;
    char *saveptr;
    char *token;
    s32 ret;

    if (!path || !dentry) {
        return VFS_EINVAL;
    }

    if (!vfs_global.initialized) {
        return VFS_ENODEV;
    }

    atomic_inc(&vfs_global.stats.total_lookups);

    /* Start from root or current directory */
    if (vfs_path_is_absolute(path)) {
        if (!root_dentry) {
            return VFS_ENOENT;
        }
        vfs_d_get(root_dentry);
        current = root_dentry;
    } else {
        if (!vfs_global.pwd) {
            return VFS_ENOENT;
        }
        vfs_d_get(vfs_global.pwd);
        current = vfs_global.pwd;
    }

    /* Make a copy of path for tokenization */
    path_copy = (char *)kmalloc(strlen(path) + 1);
    if (!path_copy) {
        vfs_d_put(current);
        return VFS_ENOMEM;
    }
    strcpy(path_copy, path);

    /* Skip leading slash for absolute paths */
    if (path_copy[0] == '/') {
        component = path_copy + 1;
    } else {
        component = path_copy;
    }

    /* Process each path component */
    token = strtok_r(component, "/", &saveptr);

    while (token) {
        parent = current;

        /* Lookup component in parent */
        ret = vfs_lookup(token, parent, &child);
        if (ret != VFS_OK) {
            vfs_d_put(parent);
            kfree(path_copy);
            return ret;
        }

        vfs_d_put(parent);
        current = child;

        /* Handle symlinks if not NOFOLLOW */
        if (!(flags & VFS_FO_NOFOLLOW) && current->d_inode &&
            (current->d_inode->type == VFS_TYPE_SYMLINK)) {
            /* TODO: Implement symlink resolution */
        }

        /* Handle mount points */
        if (current->d_mount) {
            struct vfs_dentry *mount_root = current->d_mount->mnt_root;
            vfs_d_get(mount_root);
            vfs_d_put(current);
            current = mount_root;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    kfree(path_copy);
    *dentry = current;

    return VFS_OK;
}

/**
 * vfs_lookup - Lookup a name in a directory
 * @name: Name to lookup
 * @parent: Parent dentry
 * @result: Output dentry
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_lookup(const char *name, struct vfs_dentry *parent, struct vfs_dentry **result)
{
    struct vfs_dentry *dentry;
    struct vfs_inode *inode;
    s32 ret;

    if (!name || !parent || !result) {
        return VFS_EINVAL;
    }

    if (!parent->d_inode) {
        return VFS_ENOTDIR;
    }

    if (!(parent->d_inode->type == VFS_TYPE_DIRECTORY)) {
        return VFS_ENOTDIR;
    }

    /* Try fast lookup first */
    ret = vfs_lookup_fast(name, parent, result);
    if (ret == VFS_OK) {
        return VFS_OK;
    }

    /* Slow path - call filesystem lookup */
    dentry = vfs_d_alloc(parent, name);
    if (!dentry) {
        return VFS_ENOMEM;
    }

    if (parent->d_inode->i_op && parent->d_inode->i_op->lookup) {
        ret = parent->d_inode->i_op->lookup(parent->d_inode, dentry);
        if (ret != VFS_OK) {
            vfs_d_put(dentry);
            return ret;
        }
    }

    /* Instantiate dentry with inode */
    if (dentry->d_inode) {
        vfs_d_get(dentry);
        *result = dentry;
        return VFS_OK;
    }

    vfs_d_put(dentry);
    return VFS_ENOENT;
}

/**
 * vfs_lookup_fast - Fast dentry cache lookup
 * @name: Name to lookup
 * @parent: Parent dentry
 * @result: Output dentry
 *
 * Returns: VFS_OK if found in cache, VFS_ENOENT otherwise
 */
s32 vfs_lookup_fast(const char *name, struct vfs_dentry *parent, struct vfs_dentry **result)
{
    struct vfs_dentry *dentry;
    u64 hash;
    u32 bucket;

    if (!name || !parent || !result) {
        return VFS_EINVAL;
    }

    hash = vfs_hash_string(name, 0);
    if (parent) {
        hash ^= (u64)(uintptr)parent;
    }
    bucket = (u32)(hash & (VFS_DENTRY_HASH_SIZE - 1));

    spin_lock(&vfs_global.dentry_hash_lock);

    list_for_each_entry(dentry, &vfs_global.dentry_hash[bucket], d_hash_list) {
        if (dentry->d_parent != parent) {
            continue;
        }
        if (dentry->d_hash != hash) {
            continue;
        }
        if (dentry->d_name_len != strlen(name)) {
            continue;
        }
        if (strcmp(dentry->d_name, name) == 0) {
            /* Found in cache */
            vfs_d_get(dentry);
            spin_unlock(&vfs_global.dentry_hash_lock);
            vfs_global.stats.cache_hits++;
            *result = dentry;
            return VFS_OK;
        }
    }

    spin_unlock(&vfs_global.dentry_hash_lock);
    vfs_global.stats.cache_misses++;

    return VFS_ENOENT;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

/**
 * vfs_open - Open a file
 * @pathname: Path to file
 * @flags: Open flags
 * @mode: File mode (for creation)
 *
 * Returns: File descriptor on success, error code on failure
 */
s32 vfs_open(const char *pathname, u32 flags, mode_t mode)
{
    struct vfs_dentry *dentry;
    struct vfs_file *file;
    struct vfs_inode *inode;
    s32 fd;
    s32 ret;
    u32 i;

    if (!pathname) {
        return VFS_EINVAL;
    }

    if (!vfs_global.initialized) {
        return VFS_ENODEV;
    }

    /* Lookup path */
    ret = vfs_path_lookup(pathname, flags, &dentry);
    if (ret != VFS_OK) {
        /* Try to create if O_CREAT */
        if ((flags & VFS_FO_CREAT) && dentry == NULL) {
            /* TODO: Implement file creation */
            return VFS_ENOENT;
        }
        return ret;
    }

    if (!dentry->d_inode) {
        vfs_d_put(dentry);
        return VFS_ENOENT;
    }

    inode = dentry->d_inode;

    /* Check permissions */
    if ((flags & VFS_FO_WRITE) && !(inode->mode & VFS_MODE_IWUSR)) {
        vfs_d_put(dentry);
        return VFS_EACCES;
    }

    /* Find free file descriptor */
    spin_lock(&vfs_global.file_lock);

    for (i = 0; i < vfs_global.max_files; i++) {
        if (!vfs_global.files[i]) {
            fd = (s32)i;
            break;
        }
    }

    if (i >= vfs_global.max_files) {
        spin_unlock(&vfs_global.file_lock);
        vfs_d_put(dentry);
        return VFS_ENFILE;
    }

    /* Allocate file structure */
    file = (struct vfs_file *)kzalloc(sizeof(struct vfs_file));
    if (!file) {
        spin_unlock(&vfs_global.file_lock);
        vfs_d_put(dentry);
        return VFS_ENOMEM;
    }

    /* Initialize file */
    file->f_fd = (u32)fd;
    file->f_flags = flags;
    file->f_mode = flags & VFS_FO_RDWR;
    file->f_pos = 0;
    file->f_count = 1;
    atomic_set(&file->f_usage, 1);
    file->f_dentry = dentry;
    file->f_inode = inode;
    file->f_mount = dentry->d_mount;
    file->f_op = inode->i_fop;
    file->magic = VFS_MAGIC;
    spin_lock_init(&file->f_lock);
    INIT_LIST_HEAD(&file->f_list);
    INIT_LIST_HEAD(&file->f_async_list);

    /* Call filesystem open */
    if (file->f_op && file->f_op->open) {
        ret = file->f_op->open(inode, file);
        if (ret != VFS_OK) {
            kfree(file);
            spin_unlock(&vfs_global.file_lock);
            vfs_d_put(dentry);
            return ret;
        }
    }

    /* Install file descriptor */
    vfs_global.files[fd] = file;
    atomic_inc(&vfs_global.stats.total_files);
    atomic_inc(&vfs_global.stats.total_opens);

    spin_unlock(&vfs_global.file_lock);

    pr_debug("VFS: Opened '%s' (fd=%d)\n", pathname, fd);

    return fd;
}

/**
 * vfs_close - Close a file descriptor
 * @fd: File descriptor
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 vfs_close(u32 fd)
{
    struct vfs_file *file;
    s32 ret;

    if (fd >= vfs_global.max_files) {
        return VFS_EBADF;
    }

    spin_lock(&vfs_global.file_lock);

    file = vfs_global.files[fd];
    if (!file) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EBADF;
    }

    /* Call filesystem release */
    if (file->f_op && file->f_op->release) {
        ret = file->f_op->release(file->f_inode, file);
        if (ret != VFS_OK) {
            pr_err("VFS: Filesystem release failed: %d\n", ret);
        }
    }

    /* Remove from table */
    vfs_global.files[fd] = NULL;

    /* Release resources */
    vfs_d_put(file->f_dentry);
    atomic_dec(&vfs_global.stats.total_files);
    atomic_inc(&vfs_global.stats.total_closes);

    kfree(file);

    spin_unlock(&vfs_global.file_lock);

    pr_debug("VFS: Closed fd=%d\n", fd);

    return VFS_OK;
}

/**
 * vfs_read - Read from a file descriptor
 * @fd: File descriptor
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 *
 * Returns: Number of bytes read, or error code
 */
s64 vfs_read(u32 fd, char *buf, u64 count)
{
    struct vfs_file *file;
    s64 ret;

    if (!buf || count == 0) {
        return VFS_EINVAL;
    }

    if (fd >= vfs_global.max_files) {
        return VFS_EBADF;
    }

    spin_lock(&vfs_global.file_lock);
    file = vfs_global.files[fd];
    if (!file) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EBADF;
    }

    if (!(file->f_flags & VFS_FO_READ)) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EACCES;
    }

    vfs_d_get(file->f_dentry);
    spin_unlock(&vfs_global.file_lock);

    /* Call filesystem read */
    if (file->f_op && file->f_op->read) {
        u64 pos = file->f_pos;
        ret = file->f_op->read(file, buf, count, &pos);
        if (ret > 0) {
            file->f_pos = pos;
            atomic_inc(&vfs_global.stats.total_reads);
            vfs_global.stats.bytes_read += (u64)ret;
        }
    } else {
        ret = VFS_EINVAL;
    }

    vfs_d_put(file->f_dentry);

    return ret;
}

/**
 * vfs_write - Write to a file descriptor
 * @fd: File descriptor
 * @buf: Buffer to write from
 * @count: Number of bytes to write
 *
 * Returns: Number of bytes written, or error code
 */
s64 vfs_write(u32 fd, const char *buf, u64 count)
{
    struct vfs_file *file;
    s64 ret;

    if (!buf || count == 0) {
        return VFS_EINVAL;
    }

    if (fd >= vfs_global.max_files) {
        return VFS_EBADF;
    }

    spin_lock(&vfs_global.file_lock);
    file = vfs_global.files[fd];
    if (!file) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EBADF;
    }

    if (!(file->f_flags & VFS_FO_WRITE)) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EACCES;
    }

    vfs_d_get(file->f_dentry);
    spin_unlock(&vfs_global.file_lock);

    /* Call filesystem write */
    if (file->f_op && file->f_op->write) {
        u64 pos = file->f_pos;
        ret = file->f_op->write(file, buf, count, &pos);
        if (ret > 0) {
            file->f_pos = pos;
            atomic_inc(&vfs_global.stats.total_writes);
            vfs_global.stats.bytes_written += (u64)ret;
        }
    } else {
        ret = VFS_EINVAL;
    }

    vfs_d_put(file->f_dentry);

    return ret;
}

/**
 * vfs_lseek - Seek to position in file
 * @fd: File descriptor
 * @offset: Offset
 * @whence: Seek type (SEEK_SET, SEEK_CUR, SEEK_END)
 *
 * Returns: New position on success, error code on failure
 */
s64 vfs_lseek(u32 fd, s64 offset, s32 whence)
{
    struct vfs_file *file;
    s64 new_pos;

    if (fd >= vfs_global.max_files) {
        return VFS_EBADF;
    }

    spin_lock(&vfs_global.file_lock);
    file = vfs_global.files[fd];
    if (!file) {
        spin_unlock(&vfs_global.file_lock);
        return VFS_EBADF;
    }
    spin_unlock(&vfs_global.file_lock);

    switch (whence) {
        case VFS_SEEK_SET:
            new_pos = offset;
            break;
        case VFS_SEEK_CUR:
            new_pos = (s64)file->f_pos + offset;
            break;
        case VFS_SEEK_END:
            new_pos = (s64)file->f_inode->size + offset;
            break;
        default:
            return VFS_EINVAL;
    }

    if (new_pos < 0) {
        return VFS_EINVAL;
    }

    /* Call filesystem llseek if available */
    if (file->f_op && file->f_op->llseek) {
        s64 ret = file->f_op->llseek(file, offset, whence);
        if (ret >= 0) {
            file->f_pos = (u64)ret;
            return ret;
        }
        return ret;
    }

    file->f_pos = (u64)new_pos;
    return new_pos;
}

/**
 * vfs_get_file - Get file structure by fd
 * @fd: File descriptor
 *
 * Returns: File structure or NULL
 */
struct vfs_file *vfs_get_file(u32 fd)
{
    struct vfs_file *file;

    if (fd >= vfs_global.max_files) {
        return NULL;
    }

    spin_lock(&vfs_global.file_lock);
    file = vfs_global.files[fd];
    if (file) {
        atomic_inc(&file->f_usage);
    }
    spin_unlock(&vfs_global.file_lock);

    return file;
}

/**
 * vfs_put_file - Release file structure
 * @file: File structure
 */
void vfs_put_file(struct vfs_file *file)
{
    if (!file) {
        return;
    }

    if (atomic_dec_and_test(&file->f_usage)) {
        kfree(file);
    }
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * vfs_stats_print - Print VFS statistics
 */
void vfs_stats_print(void)
{
    printk("\n=== VFS Statistics ===\n");
    printk("Total Inodes:    %d\n", atomic_read(&vfs_global.stats.total_inodes));
    printk("Total Dentries:  %d\n", atomic_read(&vfs_global.stats.total_dentries));
    printk("Total Files:     %d\n", atomic_read(&vfs_global.stats.total_files));
    printk("Total Mounts:    %d\n", atomic_read(&vfs_global.stats.total_mounts));
    printk("\n");
    printk("Total Reads:     %d\n", atomic_read(&vfs_global.stats.total_reads));
    printk("Total Writes:    %d\n", atomic_read(&vfs_global.stats.total_writes));
    printk("Total Opens:     %d\n", atomic_read(&vfs_global.stats.total_opens));
    printk("Total Closes:    %d\n", atomic_read(&vfs_global.stats.total_closes));
    printk("Total Lookups:   %d\n", atomic_read(&vfs_global.stats.total_lookups));
    printk("\n");
    printk("Bytes Read:      %llu\n", (unsigned long long)vfs_global.stats.bytes_read);
    printk("Bytes Written:   %llu\n", (unsigned long long)vfs_global.stats.bytes_written);
    printk("Cache Hits:      %llu\n", (unsigned long long)vfs_global.stats.cache_hits);
    printk("Cache Misses:    %llu\n", (unsigned long long)vfs_global.stats.cache_misses);

    if (vfs_global.stats.cache_hits + vfs_global.stats.cache_misses > 0) {
        u64 total = vfs_global.stats.cache_hits + vfs_global.stats.cache_misses;
        u32 hit_rate = (u32)(vfs_global.stats.cache_hits * 100 / total);
        printk("Cache Hit Rate:  %u%%\n", hit_rate);
    }
}

/**
 * vfs_dump_info - Dump complete VFS information
 */
void vfs_dump_info(void)
{
    struct vfs_filesystem *fs;

    printk("\n===========================================\n");
    printk("       NEXUS OS VFS Information\n");
    printk("===========================================\n\n");

    printk("VFS Version: 1.0.0\n");
    printk("Initialized: %s\n", vfs_global.initialized ? "Yes" : "No");
    printk("Init Count:  %u\n", vfs_global.init_count);
    printk("\n");

    vfs_stats_print();

    printk("\n=== Registered Filesystems ===\n");
    spin_lock(&vfs_filesystems_lock);
    list_for_each_entry(fs, &vfs_filesystems, fs_list) {
        printk("  - %s\n", fs->name);
    }
    spin_unlock(&vfs_filesystems_lock);

    printk("\n===========================================\n");
}

/*===========================================================================*/
/*                         CACHE SHRINKING                                   */
/*===========================================================================*/

/**
 * vfs_shrink_inode_cache - Shrink inode cache
 *
 * Reclaims unused inodes from the cache.
 */
void vfs_shrink_inode_cache(void)
{
    vfs_inode_shrink();
}

/**
 * vfs_shrink_dentry_cache - Shrink dentry cache
 *
 * Reclaims unused dentries from the cache.
 */
void vfs_shrink_dentry_cache(void)
{
    vfs_dentry_shrink();
}

/**
 * vfs_drop_caches - Drop all caches
 *
 * Frees all cached inodes and dentries.
 */
void vfs_drop_caches(void)
{
    pr_info("VFS: Dropping caches...\n");

    vfs_shrink_inode_cache();
    vfs_shrink_dentry_cache();

    pr_info("VFS: Cache drop complete.\n");
}
