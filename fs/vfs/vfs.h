/*
 * NEXUS OS - Virtual File System
 * fs/vfs/vfs.h
 *
 * VFS Header - Core VFS structures and API definitions
 */

#ifndef _NEXUS_VFS_H
#define _NEXUS_VFS_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         VFS CONFIGURATION                                 */
/*===========================================================================*/

#define VFS_MAGIC               0x56465300      /* "VFS\0" */
#define VFS_NAME_MAX            255
#define VFS_PATH_MAX            4096
#define VFS_MAX_MOUNT_POINTS    64
#define VFS_MAX_FILESYSTEMS     16
#define VFS_MAX_OPEN_FILES      65536
#define VFS_INODE_HASH_BITS     10
#define VFS_INODE_HASH_SIZE     (1 << VFS_INODE_HASH_BITS)
#define VFS_DENTRY_HASH_BITS    12
#define VFS_DENTRY_HASH_SIZE    (1 << VFS_DENTRY_HASH_BITS)

/* Cache Configuration */
#define VFS_INODE_CACHE_SIZE    4096
#define VFS_DENTRY_CACHE_SIZE   8192
#define VFS_PAGE_CACHE_SIZE     16384

/* File Type Flags */
#define VFS_TYPE_MASK           0x000000FF
#define VFS_TYPE_UNKNOWN        0x00000000
#define VFS_TYPE_REGULAR        0x00000001
#define VFS_TYPE_DIRECTORY      0x00000002
#define VFS_TYPE_CHAR_DEVICE    0x00000003
#define VFS_TYPE_BLOCK_DEVICE   0x00000004
#define VFS_TYPE_FIFO           0x00000005
#define VFS_TYPE_SOCKET         0x00000006
#define VFS_TYPE_SYMLINK        0x00000007

/* File Mode Flags */
#define VFS_MODE_MASK           0x00000FFF
#define VFS_MODE_IRUSR          0x00000100      /* Owner read */
#define VFS_MODE_IWUSR          0x00000200      /* Owner write */
#define VFS_MODE_IXUSR          0x00000400      /* Owner execute */
#define VFS_MODE_IRGRP          0x00000040      /* Group read */
#define VFS_MODE_IWGRP          0x00000020      /* Group write */
#define VFS_MODE_IXGRP          0x00000010      /* Group execute */
#define VFS_MODE_IROTH          0x00000004      /* Other read */
#define VFS_MODE_IWOTH          0x00000002      /* Other write */
#define VFS_MODE_IXOTH          0x00000001      /* Other execute */
#define VFS_MODE_ISUID          0x00000800      /* Set UID */
#define VFS_MODE_ISGID          0x00000400      /* Set GID */
#define VFS_MODE_ISVTX          0x00000200      /* Sticky bit */

/* File Status Flags */
#define VFS_FO_READ             0x00000001
#define VFS_FO_WRITE            0x00000002
#define VFS_FO_RDWR             (VFS_FO_READ | VFS_FO_WRITE)
#define VFS_FO_APPEND           0x00000004
#define VFS_FO_CREAT            0x00000008
#define VFS_FO_EXCL             0x00000010
#define VFS_FO_TRUNC            0x00000020
#define VFS_FO_NOFOLLOW         0x00000040
#define VFS_FO_DIRECTORY        0x00000080
#define VFS_FO_SYNC             0x00000100
#define VFS_FO_NONBLOCK         0x00000200
#define VFS_FO_CLOEXEC          0x00000400

/* Inode Flags */
#define VFS_INODE_DIRTY         0x00000001
#define VFS_INODE_VALID         0x00000002
#define VFS_INODE_LOCKED        0x00000004
#define VFS_INODE_REFERENCED    0x00000008
#define VFS_INODE_FREEING       0x00000010
#define VFS_INODE_NEW           0x00000020
#define VFS_INODE_DELETED       0x00000040

/* Dentry Flags */
#define VFS_DENTRY_VALID        0x00000001
#define VFS_DENTRY_NEGATIVE     0x00000002
#define VFS_DENTRY_REFERENCED   0x00000004
#define VFS_DENTRY_LOCKED       0x00000008
#define VFS_DENTRY_FREEING      0x00000010

/* Mount Flags */
#define VFS_MOUNT_RDONLY        0x00000001
#define VFS_MOUNT_NOEXEC        0x00000002
#define VFS_MOUNT_NOSUID        0x00000004
#define VFS_MOUNT_NODEV         0x00000008
#define VFS_MOUNT_NOATIME       0x00000010
#define VFS_MOUNT_NODIRATIME    0x00000020
#define VFS_MOUNT_RELATIME      0x00000040
#define VFS_MOUNT_STRICTATIME   0x00000080
#define VFS_MOUNT_BIND          0x00000100
#define VFS_MOUNT_MOVE          0x00000200

/* Seek Positions */
#define VFS_SEEK_SET            0
#define VFS_SEEK_CUR            1
#define VFS_SEEK_END            2

/* VFS Error Codes */
#define VFS_OK                  OK
#define VFS_ERROR               ERROR
#define VFS_ENOENT              ENOENT
#define VFS_EEXIST              EEXIST
#define VFS_EINVAL              EINVAL
#define VFS_ENOMEM              ENOMEM
#define VFS_EIO                 EIO
#define VFS_EBUSY               EBUSY
#define VFS_EACCES              EACCES
#define VFS_EPERM               EPERM
#define VFS_ENODEV              ENODEV
#define VFS_ENOSYS              ENOSYS
#define VFS_ENOSPC              (-20)
#define VFS_EROFS               (-21)
#define VFS_ENOTDIR             (-22)
#define VFS_EISDIR              (-23)
#define VFS_ENOTEMPTY           (-24)
#define VFS_ELOOP               (-25)
#define VFS_ENAMETOOLONG        (-26)
#define VFS_EOVERFLOW           (-27)
#define VFS_EBADF               (-28)

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

struct vfs_inode;
struct vfs_dentry;
struct vfs_file;
struct vfs_mount;
struct vfs_superblock;
struct vfs_filesystem;
struct vfs_operations;
struct vfs_inode_operations;
struct vfs_dentry_operations;
struct vfs_file_operations;
struct vfs_superblock_operations;
struct vfs_page_cache;
struct vfs_buffer;

/*===========================================================================*/
/*                         VFS STATISTICS                                    */
/*===========================================================================*/

/**
 * struct vfs_stats - VFS global statistics
 */
struct vfs_stats {
    atomic_t total_inodes;            /* Total inodes allocated */
    atomic_t total_dentries;          /* Total dentries allocated */
    atomic_t total_files;             /* Total open files */
    atomic_t total_mounts;            /* Total mount points */
    atomic_t total_reads;             /* Total read operations */
    atomic_t total_writes;            /* Total write operations */
    atomic_t total_opens;             /* Total open operations */
    atomic_t total_closes;            /* Total close operations */
    atomic_t total_lookups;           /* Total path lookups */
    u64 bytes_read;                   /* Total bytes read */
    u64 bytes_written;                /* Total bytes written */
    u64 cache_hits;                   /* Cache hits */
    u64 cache_misses;                 /* Cache misses */
};

/*===========================================================================*/
/*                         INODE STRUCTURES                                  */
/*===========================================================================*/

/**
 * struct vfs_inode_operations - Inode operations
 *
 * Operations that can be performed on an inode.
 */
struct vfs_inode_operations {
    /* File operations */
    s32 (*create)(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode);
    s32 (*lookup)(struct vfs_inode *dir, struct vfs_dentry *dentry);
    s32 (*link)(struct vfs_inode *dir, struct vfs_dentry *dentry, struct vfs_inode *inode);
    s32 (*unlink)(struct vfs_inode *dir, struct vfs_dentry *dentry);
    s32 (*symlink)(struct vfs_inode *dir, struct vfs_dentry *dentry, const char *target);
    s32 (*mkdir)(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode);
    s32 (*rmdir)(struct vfs_inode *dir, struct vfs_dentry *dentry);
    s32 (*mknod)(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode, dev_t dev);
    s32 (*rename)(struct vfs_inode *old_dir, struct vfs_dentry *old_dentry,
                  struct vfs_inode *new_dir, struct vfs_dentry *new_dentry);

    /* Attribute operations */
    s32 (*setattr)(struct vfs_inode *inode, mode_t mode, uid_t uid, gid_t gid);
    s32 (*getattr)(struct vfs_inode *inode, mode_t *mode, uid_t *uid, gid_t *gid);

    /* Link operations */
    s32 (*readlink)(struct vfs_inode *inode, char *buf, size_t size);

    /* Extended attributes */
    s32 (*setxattr)(struct vfs_inode *inode, const char *name, const void *value, size_t size);
    s32 (*getxattr)(struct vfs_inode *inode, const char *name, void *value, size_t size);
    s32 (*listxattr)(struct vfs_inode *inode, char *list, size_t size);
    s32 (*removexattr)(struct vfs_inode *inode, const char *name);

    /* Inode lifecycle */
    void (*read_inode)(struct vfs_inode *inode);
    void (*write_inode)(struct vfs_inode *inode);
    void (*dirty_inode)(struct vfs_inode *inode);
    void (*evict_inode)(struct vfs_inode *inode);
};

/**
 * struct vfs_inode - Virtual inode structure
 *
 * Represents a file or directory in the VFS layer.
 */
struct vfs_inode {
    /* Identity */
    u64 ino;                          /* Inode number */
    dev_t dev;                        /* Device ID */
    u32 generation;                   /* Generation number */

    /* Type and Mode */
    u32 type;                         /* File type */
    mode_t mode;                      /* File mode/permissions */
    uid_t uid;                        /* Owner user ID */
    gid_t gid;                        /* Owner group ID */

    /* Size and Links */
    u64 size;                         /* File size in bytes */
    u32 nlink;                        /* Number of hard links */
    u32 nblocks;                      /* Number of blocks allocated */
    u64 blocks;                       /* Block count */

    /* Timestamps */
    time_t atime;                     /* Last access time */
    time_t mtime;                     /* Last modification time */
    time_t ctime;                     /* Last change time */
    time_t btime;                     /* Birth/creation time */

    /* Device Info (for special files) */
    dev_t rdev;                       /* Device number for char/block devices */

    /* Flags and State */
    u32 flags;                        /* Inode flags */
    u32 state;                        /* Inode state */
    atomic_t refcount;                /* Reference count */
    atomic_t i_count;                 /* Usage count */

    /* Caching */
    struct list_head hash_list;       /* Hash table list */
    struct list_head dirty_list;      /* Dirty inode list */
    struct list_head unused_list;     /* Unused inode list */
    u64 last_used;                    /* Last access timestamp */

    /* Locking */
    spinlock_t lock;                  /* Inode lock */
    spinlock_t i_lock;                /* Internal lock */

    /* Operations */
    struct vfs_inode_operations *i_op; /* Inode operations */
    struct vfs_file_operations *i_fop; /* File operations */

    /* Superblock reference */
    struct vfs_superblock *i_sb;      /* Parent superblock */

    /* Filesystem private data */
    void *i_private;                  /* Filesystem-specific data */

    /* Page cache */
    struct vfs_page_cache *i_cache;   /* Page cache */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         DENTRY STRUCTURES                                 */
/*===========================================================================*/

/**
 * struct vfs_dentry_operations - Dentry operations
 */
struct vfs_dentry_operations {
    s32 (*d_revalidate)(struct vfs_dentry *dentry, u32 flags);
    s32 (*d_hash)(const struct vfs_dentry *dentry, u64 *hash);
    s32 (*d_compare)(const struct vfs_dentry *dentry, const char *name1,
                     const char *name2, size_t len);
    void (*d_delete)(struct vfs_dentry *dentry);
    void (*d_release)(struct vfs_dentry *dentry);
    void (*d_iput)(struct vfs_dentry *dentry, struct vfs_inode *inode);
};

/**
 * struct vfs_dentry - Directory entry structure
 *
 * Represents a name in a directory, linking to an inode.
 */
struct vfs_dentry {
    /* Name Information */
    char *d_name;                     /* Entry name */
    u32 d_name_len;                   /* Name length */
    u64 d_hash;                       /* Hash of name */

    /* Parent and Child Relationships */
    struct vfs_dentry *d_parent;      /* Parent dentry */
    struct list_head d_children;      /* List of child dentries */
    struct list_head d_child;         /* Entry in parent's children list */

    /* Inode Reference */
    struct vfs_inode *d_inode;        /* Associated inode */

    /* Mount Point */
    struct vfs_mount *d_mount;        /* Mount point if any */

    /* Flags and State */
    u32 d_flags;                      /* Dentry flags */
    u32 d_seq;                        /* Sequence number for RCU */
    atomic_t d_count;                 /* Reference count */

    /* Caching */
    struct list_head d_hash_list;     /* Hash table list */
    struct list_head d_lru;           /* LRU list for reclaim */
    u64 d_last_used;                  /* Last access time */

    /* Locking */
    spinlock_t d_lock;                /* Dentry lock */

    /* Operations */
    struct vfs_dentry_operations *d_op; /* Dentry operations */

    /* Superblock reference */
    struct vfs_superblock *d_sb;      /* Parent superblock */

    /* Filesystem private data */
    void *d_private;                  /* Filesystem-specific data */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         FILE STRUCTURES                                   */
/*===========================================================================*/

/**
 * struct vfs_file_operations - File operations
 *
 * Operations that can be performed on an open file.
 */
struct vfs_file_operations {
    /* Basic I/O */
    s64 (*read)(struct vfs_file *file, char *buf, u64 count, u64 *pos);
    s64 (*write)(struct vfs_file *file, const char *buf, u64 count, u64 *pos);
    s64 (*read_iter)(struct vfs_file *file, struct iovec *iov, u64 count, u64 *pos);
    s64 (*write_iter)(struct vfs_file *file, const struct iovec *iov, u64 count, u64 *pos);

    /* Seek and Position */
    s64 (*llseek)(struct vfs_file *file, s64 offset, s32 whence);
    s64 (*tell)(struct vfs_file *file);

    /* I/O Control */
    s32 (*ioctl)(struct vfs_file *file, u32 cmd, void *arg);
    s32 (*fcntl)(struct vfs_file *file, u32 cmd, u64 arg);
    s32 (*flock)(struct vfs_file *file, u32 cmd, u64 arg);

    /* Memory Mapping */
    s32 (*mmap)(struct vfs_file *file, void *vma);
    s32 (*mremap)(struct vfs_file *file, void *vma);
    s32 (*fsync)(struct vfs_file *file, s32 datasync);
    s32 (*fdatasync)(struct vfs_file *file);

    /* Directory Operations */
    s32 (*iterate)(struct vfs_file *file, struct vfs_dirent *dirent);
    s32 (*readdir)(struct vfs_file *file, void *dirent, filldir_t filldir);

    /* File Lifecycle */
    s32 (*open)(struct vfs_inode *inode, struct vfs_file *file);
    s32 (*release)(struct vfs_inode *inode, struct vfs_file *file);
    s32 (*flush)(struct vfs_file *file);

    /* Async I/O */
    s32 (*aio_read)(struct vfs_file *file, struct vfs_aiocb *aiocb);
    s32 (*aio_write)(struct vfs_file *file, struct vfs_aiocb *aiocb);

    /* Splice Operations */
    s64 (*splice_read)(struct vfs_file *file, u64 *pos, struct vfs_pipe *pipe, u64 len);
    s64 (*splice_write)(struct vfs_file *file, struct vfs_pipe *pipe, u64 len);
};

/**
 * struct vfs_file - Open file structure
 *
 * Represents an open file descriptor in the VFS layer.
 */
struct vfs_file {
    /* File Identity */
    u32 f_fd;                         /* File descriptor number */
    u32 f_flags;                      /* Open flags */
    u32 f_mode;                       /* Access mode */
    u32 f_status;                     /* File status flags */

    /* Position and Count */
    u64 f_pos;                        /* Current file position */
    u64 f_count;                      /* Reference count */
    atomic_t f_usage;                 /* Usage counter */

    /* References */
    struct vfs_dentry *f_dentry;      /* Dentry */
    struct vfs_inode *f_inode;        /* Inode */
    struct vfs_mount *f_mount;        /* Mount point */

    /* File Operations */
    struct vfs_file_operations *f_op; /* File operations */

    /* Private Data */
    void *f_private;                  /* Filesystem-specific data */
    void *f_cred;                     /* File credentials */

    /* Locking */
    spinlock_t f_lock;                /* File lock */

    /* Async I/O */
    struct list_head f_async_list;    /* Async I/O list */

    /* List Entry */
    struct list_head f_list;          /* List of open files */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         MOUNT STRUCTURES                                  */
/*===========================================================================*/

/**
 * struct vfs_superblock_operations - Superblock operations
 */
struct vfs_superblock_operations {
    /* Superblock lifecycle */
    s32 (*alloc_inode)(struct vfs_superblock *sb, struct vfs_inode *inode);
    void (*destroy_inode)(struct vfs_inode *inode);
    void (*read_inode)(struct vfs_inode *inode);
    void (*write_inode)(struct vfs_inode *inode, s32 sync);
    void (*evict_inode)(struct vfs_inode *inode);
    void (*put_super)(struct vfs_superblock *sb);

    /* Filesystem operations */
    s32 (*statfs)(struct vfs_superblock *sb, struct vfs_statfs *stat);
    s32 (*remount)(struct vfs_superblock *sb, u32 flags, char *data);
    s32 (*sync_fs)(struct vfs_superblock *sb, s32 wait);

    /* Show options */
    s32 (*show_options)(struct seq_file *seq, struct vfs_mount *mount);
};

/**
 * struct vfs_superblock - Superblock structure
 *
 * Represents a mounted filesystem instance.
 */
struct vfs_superblock {
    /* Identity */
    dev_t s_dev;                      /* Device ID */
    u64 s_magic;                      /* Filesystem magic number */
    u32 s_type;                       /* Filesystem type */

    /* Size Information */
    u64 s_size;                       /* Total size in bytes */
    u64 s_blocks;                     /* Total blocks */
    u64 s_free_blocks;                /* Free blocks */
    u64 s_bsize;                      /* Block size */

    /* Inode Information */
    u64 s_inodes;                     /* Total inodes */
    u64 s_free_inodes;                /* Free inodes */
    u64 s_ino;                        /* Next inode number */

    /* Root */
    struct vfs_inode *s_root;         /* Root inode */
    struct vfs_dentry *s_root_dentry; /* Root dentry */

    /* Mount Information */
    struct vfs_mount *s_mount;        /* Mount point */
    char *s_dev_name;                 /* Device name */

    /* Flags and State */
    u32 s_flags;                      /* Mount flags */
    u32 s_state;                      /* Filesystem state */
    atomic_t s_count;                 /* Reference count */

    /* Locking */
    spinlock_t s_lock;                /* Superblock lock */

    /* Inode Lists */
    struct list_head s_inodes;        /* List of inodes */
    struct list_head s_dirty_inodes;  /* Dirty inodes */
    struct list_head s_unused_inodes; /* Unused inodes */

    /* Operations */
    struct vfs_superblock_operations *s_op; /* Superblock operations */

    /* Filesystem type */
    struct vfs_filesystem *s_type;    /* Filesystem type */

    /* Time */
    time_t s_time;                    /* Last mount time */

    /* Private Data */
    void *s_private;                  /* Filesystem-specific data */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/**
 * struct vfs_mount - Mount point structure
 *
 * Represents a filesystem mount point.
 */
struct vfs_mount {
    /* Mount Identity */
    u32 mnt_id;                       /* Mount ID */
    u32 mnt_flags;                    /* Mount flags */

    /* Mount Point References */
    struct vfs_dentry *mnt_mountpoint; /* Dentry of mount point */
    struct vfs_dentry *mnt_root;      /* Root dentry of mounted fs */
    struct vfs_superblock *mnt_sb;    /* Superblock */

    /* Parent Mount */
    struct vfs_mount *mnt_parent;     /* Parent mount */
    struct list_head mnt_child;       /* Child mounts list */
    struct list_head mnt_mounts;      /* List of mounts under this */

    /* Namespace */
    struct list_head mnt_list;        /* Mount list */
    struct list_head mnt_expire;      /* Expire list */
    struct list_head mnt_share;       /* Share list */

    /* Reference Count */
    atomic_t mnt_count;               /* Reference count */

    /* Locking */
    spinlock_t mnt_lock;              /* Mount lock */

    /* Private Data */
    void *mnt_private;                /* Filesystem-specific data */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         FILESYSTEM TYPE STRUCTURES                        */
/*===========================================================================*/

/**
 * struct vfs_filesystem - Filesystem type structure
 *
 * Represents a registered filesystem type.
 */
struct vfs_filesystem {
    /* Identity */
    char *name;                       /* Filesystem name */
    u32 fs_magic;                     /* Filesystem magic */
    u32 fs_flags;                     /* Filesystem flags */

    /* Operations */
    struct vfs_superblock_operations *fs_op; /* Superblock operations */

    /* Mount/Unmount */
    s32 (*mount)(struct vfs_filesystem *fs, u32 flags, const char *dev_name,
                 void *data, struct vfs_mount **mount);
    s32 (*umount)(struct vfs_mount *mount);

    /* Initialization */
    s32 (*init)(struct vfs_filesystem *fs);
    void (*exit)(struct vfs_filesystem *fs);

    /* Owner */
    struct module *owner;             /* Module owner */

    /* List Entry */
    struct list_head fs_list;         /* List of filesystems */

    /* Magic number for validation */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         PAGE CACHE STRUCTURES                             */
/*===========================================================================*/

/**
 * struct vfs_page - Page cache page
 */
struct vfs_page {
    u64 index;                        /* Page index in file */
    u32 flags;                        /* Page flags */
    u32 refcount;                     /* Reference count */
    void *data;                       /* Page data */
    struct vfs_inode *inode;          /* Owner inode */
    struct list_head list;            /* LRU list */
    struct list_head hash_list;       /* Hash list */
    spinlock_t lock;                  /* Page lock */
};

/**
 * struct vfs_page_cache - Page cache for an inode
 */
struct vfs_page_cache {
    struct vfs_inode *inode;          /* Owner inode */
    struct radix_tree_root tree;      /* Radix tree of pages */
    u64 nr_pages;                     /* Number of pages */
    u64 nr_dirty;                     /* Number of dirty pages */
    u64 nr_writeback;                 /* Number of writeback pages */
    spinlock_t lock;                  /* Cache lock */
    struct list_head lru;             /* LRU list */
};

/*===========================================================================*/
/*                         BUFFER CACHE STRUCTURES                           */
/*===========================================================================*/

/**
 * struct vfs_buffer - Buffer cache buffer
 */
struct vfs_buffer {
    u64 b_blocknr;                    /* Block number */
    u32 b_size;                       /* Buffer size */
    u32 b_flags;                      /* Buffer flags */
    u32 b_count;                      /* Reference count */
    void *b_data;                     /* Buffer data */
    struct vfs_superblock *b_sb;      /* Superblock */
    struct list_head b_list;          /* Hash/queue list */
    spinlock_t b_lock;                /* Buffer lock */
};

/*===========================================================================*/
/*                         VFS GLOBAL STATE                                  */
/*===========================================================================*/

/**
 * struct vfs_global - Global VFS state
 */
struct vfs_global {
    /* Initialization */
    bool initialized;                 /* VFS initialized flag */
    u32 init_count;                   /* Initialization count */

    /* Statistics */
    struct vfs_stats stats;           /* VFS statistics */

    /* Filesystems */
    struct list_head filesystems;     /* List of registered filesystems */
    spinlock_t fs_lock;               /* Filesystems list lock */

    /* Mount Points */
    struct vfs_mount *mounts;         /* Array of mount points */
    u32 num_mounts;                   /* Number of mount points */
    spinlock_t mount_lock;            /* Mount points lock */

    /* Inode Cache */
    struct list_head inode_hash[VFS_INODE_HASH_SIZE];
    spinlock_t inode_hash_lock;
    struct list_head inode_unused;
    struct list_head inode_dirty;
    spinlock_t inode_lock;

    /* Dentry Cache */
    struct list_head dentry_hash[VFS_DENTRY_HASH_SIZE];
    spinlock_t dentry_hash_lock;
    struct list_head dentry_lru;
    spinlock_t dentry_lock;

    /* File Table */
    struct vfs_file **files;          /* Open file table */
    u32 max_files;                    /* Maximum open files */
    spinlock_t file_lock;             /* File table lock */

    /* Root Filesystem */
    struct vfs_mount *root_mount;     /* Root mount point */
    struct vfs_dentry *root_dentry;   /* Root dentry */

    /* Working Directory */
    struct vfs_dentry *pwd;           /* Process working directory */

    /* Locking */
    spinlock_t lock;                  /* Global VFS lock */

    /* Magic number */
    u32 magic;                        /* Must be VFS_MAGIC */
};

/*===========================================================================*/
/*                         VFS API FUNCTIONS                                 */
/*===========================================================================*/

/* VFS Core Functions */
s32 vfs_init(void);
void vfs_exit(void);
s32 vfs_register_filesystem(struct vfs_filesystem *fs);
s32 vfs_unregister_filesystem(struct vfs_filesystem *fs);
struct vfs_filesystem *vfs_get_filesystem(const char *name);

/* Mount Functions */
s32 vfs_mount(const char *dev_name, const char *dir_name, const char *type,
              u32 flags, void *data);
s32 vfs_umount(const char *dir_name);
s32 vfs_umount_all(void);
struct vfs_mount *vfs_lookup_mount(const char *path);

/* Path Lookup Functions */
s32 vfs_path_lookup(const char *path, u32 flags, struct vfs_dentry **dentry);
s32 vfs_lookup(const char *name, struct vfs_dentry *parent, struct vfs_dentry **result);
s32 vfs_lookup_fast(const char *name, struct vfs_dentry *parent, struct vfs_dentry **result);

/* Inode Functions */
struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb);
void vfs_destroy_inode(struct vfs_inode *inode);
void vfs_iget(struct vfs_inode *inode);
void vfs_iput(struct vfs_inode *inode);
struct vfs_inode *vfs_iget_locked(struct vfs_superblock *sb, u64 ino);
s32 vfs_read_inode(struct vfs_inode *inode);
s32 vfs_write_inode(struct vfs_inode *inode);
void vfs_dirty_inode(struct vfs_inode *inode);
void vfs_evict_inodes(struct vfs_superblock *sb);

/* Dentry Functions */
struct vfs_dentry *vfs_d_alloc(struct vfs_dentry *parent, const char *name);
void vfs_d_put(struct vfs_dentry *dentry);
void vfs_d_get(struct vfs_dentry *dentry);
s32 vfs_d_instantiate(struct vfs_dentry *dentry, struct vfs_inode *inode);
void vfs_d_drop(struct vfs_dentry *dentry);
void vfs_d_delete(struct vfs_dentry *dentry);
struct vfs_dentry *vfs_d_lookup(const char *name, struct vfs_dentry *parent);
struct vfs_dentry *vfs_d_lookup_hash(struct vfs_dentry *parent, const char *name, u64 hash);

/* File Functions */
s32 vfs_open(const char *pathname, u32 flags, mode_t mode);
s32 vfs_close(u32 fd);
s64 vfs_read(u32 fd, char *buf, u64 count);
s64 vfs_write(u32 fd, const char *buf, u64 count);
s64 vfs_lseek(u32 fd, s64 offset, s32 whence);
s32 vfs_fsync(u32 fd);
s32 vfs_fcntl(u32 fd, u32 cmd, u64 arg);
s32 vfs_ioctl(u32 fd, u32 cmd, void *arg);
struct vfs_file *vfs_get_file(u32 fd);
void vfs_put_file(struct vfs_file *file);

/* File Operations */
s32 vfs_create(const char *pathname, mode_t mode);
s32 vfs_unlink(const char *pathname);
s32 vfs_mkdir(const char *pathname, mode_t mode);
s32 vfs_rmdir(const char *pathname);
s32 vfs_rename(const char *oldpath, const char *newpath);
s32 vfs_symlink(const char *target, const char *linkpath);
s32 vfs_readlink(const char *pathname, char *buf, u64 size);
s32 vfs_stat(const char *pathname, struct vfs_stat *stat);
s32 vfs_chmod(const char *pathname, mode_t mode);
s32 vfs_chown(const char *pathname, uid_t uid, gid_t gid);
s32 vfs_truncate(const char *pathname, u64 length);

/* Directory Operations */
s32 vfs_readdir(const char *pathname, void *dirent, filldir_t filldir);
s32 vfs_getcwd(char *buf, u64 size);

/* Superblock Functions */
s32 vfs_sync_fs(struct vfs_superblock *sb, s32 wait);
s32 vfs_statfs(const char *pathname, struct vfs_statfs *stat);

/* Cache Functions */
void vfs_shrink_inode_cache(void);
void vfs_shrink_dentry_cache(void);
void vfs_drop_caches(void);

/* Utility Functions */
s32 vfs_path_split(const char *path, char **dir, char **base);
bool vfs_path_is_absolute(const char *path);
s32 vfs_path_normalize(char *path);
u64 vfs_hash_string(const char *str, u64 len);

/* Statistics Functions */
void vfs_stats_print(void);
void vfs_dump_info(void);

#endif /* _NEXUS_VFS_H */
