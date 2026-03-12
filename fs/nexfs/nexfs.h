/*
 * NEXUS OS - NexFS Filesystem
 * fs/nexfs/nexfs.h
 *
 * NexFS Header - Native filesystem for NEXUS OS
 */

#ifndef _NEXUS_NEXFS_H
#define _NEXUS_NEXFS_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "../vfs/vfs.h"

/*===========================================================================*/
/*                         NEXFS CONFIGURATION                               */
/*===========================================================================*/

#define NEXFS_MAGIC             0x4E4558465300    /* "NEXFS\0" */
#define NEXFS_VERSION           1
#define NEXFS_REVISION          0

/* Block Configuration */
#define NEXFS_BLOCK_SIZE        4096
#define NEXFS_BLOCK_SHIFT       12
#define NEXFS_MIN_BLOCK_SIZE    1024
#define NEXFS_MAX_BLOCK_SIZE    65536

/* Inode Configuration */
#define NEXFS_INODE_SIZE        256
#define NEXFS_INODES_PER_BLOCK  (NEXFS_BLOCK_SIZE / NEXFS_INODE_SIZE)
#define NEXFS_ROOT_INO          2
#define NEXFS_FIRST_INO         11

/* Direct/Indirect Blocks */
#define NEXFS_DIRECT_BLOCKS     12
#define NEXFS_INDIRECT_BLOCKS   1
#define NEXFS_DOUBLE_INDIRECT   1
#define NEXFS_TRIPLE_INDIRECT   1
#define NEXFS_MAX_BLOCKS        (NEXFS_DIRECT_BLOCKS + NEXFS_INDIRECT_BLOCKS + \
                                  NEXFS_DOUBLE_INDIRECT + NEXFS_TRIPLE_INDIRECT)

/* Journal Configuration */
#define NEXFS_JOURNAL_SIZE      (8 * 1024 * 1024)   /* 8 MB default */
#define NEXFS_JOURNAL_BLOCKS    (NEXFS_JOURNAL_SIZE / NEXFS_BLOCK_SIZE)
#define NEXFS_JOURNAL_MAGIC     0x4A4F55524E414C    /* "JOURNAL" */

/* Superblock Configuration */
#define NEXFS_SUPERBLOCK_OFFSET 1024
#define NEXFS_SUPERBLOCK_SIZE   NEXFS_BLOCK_SIZE
#define NEXFS_BOOT_BLOCK_SIZE   NEXFS_BLOCK_SIZE

/* Group Descriptor Configuration */
#define NEXFS_GROUPS_PER_DESC   (NEXFS_BLOCK_SIZE / sizeof(struct nexfs_group_desc))

/* Special Inode Numbers */
#define NEXFS_INO_BAD_BLOCKS    1
#define NEXFS_INO_ROOT          2
#define NEXFS_INO_JOURNAL       3
#define NEXFS_INO_RESERVED      4

/* File Type Codes */
#define NEXFS_TYPE_UNKNOWN      0
#define NEXFS_TYPE_REGULAR      1
#define NEXFS_TYPE_DIRECTORY    2
#define NEXFS_TYPE_CHAR_DEV     3
#define NEXFS_TYPE_BLOCK_DEV    4
#define NEXFS_TYPE_FIFO         5
#define NEXFS_TYPE_SOCKET       6
#define NEXFS_TYPE_SYMLINK      7

/* Journal Transaction Types */
#define NEXFS_JOURNAL_COMMIT    1
#define NEXFS_JOURNAL_REVOKE    2
#define NEXFS_JOURNAL_DESCRIPTOR 3
#define NEXFS_JOURNAL_DATA      4

/* Feature Flags */
#define NEXFS_FEATURE_COMPAT_RO_COMPAT    0x00000001
#define NEXFS_FEATURE_COMPAT_INCOMPAT     0x00000002
#define NEXFS_FEATURE_JOURNAL             0x00000004
#define NEXFS_FEATURE_EXTENTS             0x00000008
#define NEXFS_FEATURE_64BIT               0x00000010
#define NEXFS_FEATURE_FLEX_GROUPS         0x00000020

/* Mount States */
#define NEXFS_STATE_CLEAN       0x0001
#define NEXFS_STATE_ERROR       0x0002
#define NEXFS_STATE_ORPHAN      0x0004

/* Error Behavior */
#define NEXFS_ERROR_CONTINUE    0
#define NEXFS_ERROR_RO          1
#define NEXFS_ERROR_PANIC       2

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

struct nexfs_superblock;
struct nexfs_group_desc;
struct nexfs_inode;
struct nexfs_extent;
struct nexfs_journal;
struct nexfs_journal_header;
struct nexfs_transaction;
struct nexfs_block_map;

/*===========================================================================*/
/*                         NEXFS SUPERBLOCK                                  */
/*===========================================================================*/

/**
 * struct nexfs_superblock_raw - On-disk superblock structure
 */
struct nexfs_superblock_raw {
    u32 s_magic;                    /* Magic number */
    u32 s_state;                    /* Filesystem state */
    u32 s_errors;                   /* Error behavior */
    u32 s_minor_rev;                /* Minor revision */

    u64 s_first_ino;                /* First non-reserved inode */
    u64 s_inode_size;               /* Inode size */
    u64 s_inodes_per_block;         /* Inodes per block */

    u64 s_blocks_count;             /* Total blocks */
    u64 s_free_blocks;              /* Free blocks */
    u64 s_reserved_blocks;          /* Reserved blocks */

    u64 s_inodes_count;             /* Total inodes */
    u64 s_free_inodes;              /* Free inodes */

    u64 s_first_data_block;         /* First data block */
    u32 s_log_block_size;           /* Block size exponent */
    u32 s_blocks_per_group;         /* Blocks per group */
    u32 s_inodes_per_group;         /* Inodes per group */
    u32 s_groups_count;             /* Number of groups */

    u32 s_mount_time;               /* Last mount time */
    u32 s_write_time;               /* Last write time */
    u16 s_mount_count;              /* Mount count since check */
    u16 s_max_mount_count;          /* Max mounts before check */

    u32 s_last_check;               /* Last filesystem check */
    u32 s_check_interval;           /* Check interval */
    u32 s_creator_os;               /* Creator OS */
    u32 s_rev_level;                /* Revision level */

    u16 s_def_resuid;               /* Default reserved UID */
    u16 s_def_resgid;               /* Default reserved GID */

    /* Extended fields */
    u64 s_journal_ino;              /* Journal inode */
    u64 s_journal_dev;              /* Journal device */
    u64 s_last_orphan;              /* Orphan list head */
    u32 s_feature_compat;           /* Compatible features */
    u32 s_feature_incompat;         /* Incompatible features */
    u32 s_feature_ro_compat;        /* Read-only features */

    u8 s_uuid[16];                  /* Filesystem UUID */
    char s_volume_name[16];         /* Volume name */
    char s_last_mount[64];          /* Last mount path */

    u32 s_commit_interval;          /* Journal commit interval */
    u64 s_journal_blocks;           /* Journal size in blocks */

    u8 s_reserved[880];             /* Reserved for future */
    u32 s_checksum;                 /* Superblock checksum */
} __packed;

/**
 * struct nexfs_superblock - In-memory superblock
 */
struct nexfs_superblock {
    struct vfs_superblock vfs_sb;   /* VFS superblock */

    /* Raw superblock (cached) */
    struct nexfs_superblock_raw raw;

    /* Block device */
    void *bdev;                     /* Block device */
    u64 bdev_size;                  /* Device size in bytes */

    /* Group descriptors */
    struct nexfs_group_desc *groups; /* Group descriptor array */
    u32 groups_count;

    /* Inode bitmap cache */
    u8 *inode_bitmap;               /* Cached inode bitmaps */
    u8 *block_bitmap;               /* Cached block bitmaps */

    /* Journal */
    struct nexfs_journal *journal;

    /* Statistics */
    u64 used_blocks;
    u64 used_inodes;

    /* Locks */
    spinlock_t bitmap_lock;         /* Bitmap update lock */
    spinlock_t group_lock;          /* Group descriptor lock */

    /* Orphan list */
    u64 orphan_list_head;
    spinlock_t orphan_lock;

    /* Mount options */
    u32 mount_options;
    u32 commit_interval;

    /* Checksum seed */
    u32 checksum_seed;
};

/*===========================================================================*/
/*                         NEXFS GROUP DESCRIPTOR                            */
/*===========================================================================*/

/**
 * struct nexfs_group_desc - Block group descriptor
 */
struct nexfs_group_desc {
    u64 bg_block_bitmap;            /* Block bitmap block */
    u64 bg_inode_bitmap;            /* Inode bitmap block */
    u64 bg_inode_table;             /* Inode table start block */

    u32 bg_free_blocks;             /* Free blocks in group */
    u32 bg_free_inodes;             /* Free inodes in group */
    u32 bg_used_dirs;               /* Directory count */
    u32 bg_flags;                   /* Group flags */

    u64 bg_exclude_bitmap;          /* Exclude bitmap */
    u16 bg_block_bitmap_csum;       /* Block bitmap checksum */
    u16 bg_inode_bitmap_csum;       /* Inode bitmap checksum */
    u32 bg_itable_unused;           /* Unused inodes in table */
    u16 bg_checksum;                /* Group descriptor checksum */

    u8 bg_reserved[42];             /* Reserved */
} __packed;

/* Group flags */
#define NEXFS_BG_INODE_UNINIT     0x0001
#define NEXFS_BG_BLOCK_UNINIT     0x0002
#define NEXFS_BG_INODE_ZEROED     0x0004

/*===========================================================================*/
/*                         NEXFS INODE                                       */
/*===========================================================================*/

/**
 * struct nexfs_inode_raw - On-disk inode structure
 */
struct nexfs_inode_raw {
    u16 i_mode;                     /* File mode */
    u16 i_uid;                      /* Owner UID */
    u64 i_size;                     /* File size */
    u32 i_atime;                    /* Access time */
    u32 i_ctime;                    /* Change time */
    u32 i_mtime;                    /* Modification time */
    u32 i_dtime;                    /* Deletion time */

    u16 i_gid;                      /* Owner GID */
    u16 i_links;                    /* Hard link count */
    u32 i_blocks_lo;                /* Block count (low) */
    u32 i_flags;                    /* File flags */

    union {
        struct {
            u32 i_block[NEXFS_DIRECT_BLOCKS]; /* Direct blocks */
            u32 i_indirect;         /* Indirect block */
            u32 i_dindirect;        /* Double indirect block */
            u32 i_tindirect;        /* Triple indirect block */
        } blocks;
        struct {
            u64 i_extent_head;      /* Extent tree head */
            u8 i_extent_reserved[44];
        } extents;
    } i_data;

    u32 i_generation;               /* File version */
    u64 i_file_acl;                 /* File ACL */
    u64 i_dir_acl;                  /* Directory ACL */
    u32 i_faddr;                    /* Fragment address */

    /* Extended fields */
    u8 i_osd1[12];                  /* OS dependent 1 */
    u32 i_uid_high;                 /* High 16 bits of UID */
    u32 i_gid_high;                 /* High 16 bits of GID */
    u32 i_reserved2;

    u64 i_blocks_hi;                /* Block count (high) */
    u16 i_extra_isize;              /* Extra inode size */
    u16 i_checksum;                 /* Inode checksum */
    u32 i_ctime_extra;              /* Extra change time */
    u32 i_mtime_extra;              /* Extra modification time */
    u32 i_atime_extra;              /* Extra access time */
    u32 i_crtime;                   /* Creation time */
    u32 i_crtime_extra;             /* Extra creation time */
    u32 i_version_hi;               /* High 32 bits of version */

    u8 i_reserved[92];              /* Reserved */
} __packed;

/**
 * struct nexfs_inode - In-memory inode
 */
struct nexfs_inode {
    struct vfs_inode vfs_inode;     /* VFS inode */

    /* Raw inode data */
    struct nexfs_inode_raw raw;

    /* NexFS specific */
    u64 ino;                        /* Inode number */
    u32 generation;                 /* Generation number */

    /* Extent cache */
    struct nexfs_extent *extent_cache;
    u32 extent_cache_size;

    /* Directory cache */
    struct list_head dcache;        /* Directory entry cache */
    spinlock_t dcache_lock;

    /* Block allocation */
    u32 last_alloc_block;           /* Last allocated block */
    u32 last_alloc_group;           /* Last allocated group */

    /* Checksum */
    u32 checksum;
};

/* File flags */
#define NEXFS_FL_SECRM            0x00000001
#define NEXFS_FL_UNRM             0x00000002
#define NEXFS_FL_COMPR            0x00000004
#define NEXFS_FL_SYNC             0x00000008
#define NEXFS_FL_IMMUTABLE        0x00000010
#define NEXFS_FL_APPEND           0x00000020
#define NEXFS_FL_NODUMP           0x00000040
#define NEXFS_FL_NOATIME          0x00000080
#define NEXFS_FL_DIRTY            0x00000100
#define NEXFS_FL_COMPRBLK         0x00000200
#define NEXFS_FL_NOCOMPR          0x00000400
#define NEXFS_FL_ENCRYPT          0x00000800
#define NEXFS_FL_INDEX            0x00001000
#define NEXFS_FL_IMAGIC           0x00002000
#define NEXFS_FL_JOURNAL_DATA     0x00004000
#define NEXFS_FL_NOTAIL           0x00008000
#define NEXFS_FL_DIRSYNC          0x00010000
#define NEXFS_FL_TOPDIR           0x00020000
#define NEXFS_FL_HUGE_FILE        0x00040000
#define NEXFS_FL_EXTENTS          0x00080000

/*===========================================================================*/
/*                         NEXFS EXTENTS                                     */
/*===========================================================================*/

/**
 * struct nexfs_extent_header - Extent tree header
 */
struct nexfs_extent_header {
    u16 eh_magic;                   /* Magic number (0xF30A) */
    u16 eh_entries;                 /* Number of entries */
    u16 eh_max;                     /* Max entries */
    u16 eh_depth;                   /* Tree depth */
    u32 eh_generation;              /* Generation */
} __packed;

/**
 * struct nexfs_extent - Extent descriptor
 */
struct nexfs_extent {
    u32 ee_block;                   /* First logical block */
    u16 ee_len;                     /* Number of blocks */
    u16 ee_start_hi;                /* High 16 bits of physical block */
    u64 ee_start;                   /* Physical block address */
} __packed;

/**
 * struct nexfs_extent_idx - Extent tree index
 */
struct nexfs_extent_idx {
    u32 ei_block;                   /* Index of covered logical block */
    u64 ei_leaf;                    /* Pointer to child node */
    u16 ei_unused;
} __packed;

#define NEXFS_EXT_MAGIC           0xF30A
#define NEXFS_EXT_MAX_LEN         0x8000  /* 32K blocks */

/*===========================================================================*/
/*                         NEXFS DIRECTORY                                   */
/*===========================================================================*/

/**
 * struct nexfs_dir_entry - Directory entry
 */
struct nexfs_dir_entry {
    u32 inode;                      /* Inode number */
    u16 rec_len;                    /* Entry length */
    u8 name_len;                    /* Name length */
    u8 file_type;                   /* File type */
    char name[];                    /* File name */
} __packed;

/* Directory entry file types */
#define NEXFS_DE_UNKNOWN          0
#define NEXFS_DE_REG_FILE         1
#define NEXFS_DE_DIR              2
#define NEXFS_DE_CHRDEV           3
#define NEXFS_DE_BLKDEV           4
#define NEXFS_DE_FIFO             5
#define NEXFS_DE_SOCK             6
#define NEXFS_DE_SYMLINK          7

/* Directory entry maximum size */
#define NEXFS_DIR_REC_LEN(name_len) (((name_len) + 8 + 3) & ~3)
#define NEXFS_DIR_MAX_NAME_LEN    255

/*===========================================================================*/
/*                         NEXFS JOURNAL                                     */
/*===========================================================================*/

/**
 * struct nexfs_journal_header - Journal block header
 */
struct nexfs_journal_header {
    u32 h_magic;                    /* Journal magic */
    u32 h_blocktype;                /* Block type */
    u32 h_sequence;                 /* Sequence number */
} __packed;

/**
 * struct nexfs_journal_superblock - Journal superblock
 */
struct nexfs_journal_superblock {
    u32 s_magic;                    /* Journal magic */
    u32 s_blocksize;                /* Block size */
    u32 s_total_blocks;             /* Total blocks */
    u32 s_first_block;              /* First log block */

    u32 s_sequence;                 /* Start sequence */
    u32 s_start;                    /* Start block */

    u32 s_errno;                    /* Error status */
    u32 s_feature_compat;           /* Compatible features */
    u32 s_feature_incompat;         /* Incompatible features */
    u32 s_feature_ro_compat;        /* Read-only features */

    u8 s_uuid[16];                  /* Journal UUID */
    u64 s_shared_bmap_blocks;       /* Shared bitmap blocks */

    u32 s_nr_users;                 /* Number of users */
    u32 s_dynsuper;                 /* Dynamic superblock */

    u32 s_max_transaction;          /* Max transaction size */
    u32 s_max_trans_data;           /* Max transaction data */

    u8 s_reserved[180];             /* Reserved */
} __packed;

/**
 * struct nexfs_journal_commit - Journal commit block
 */
struct nexfs_journal_commit {
    struct nexfs_journal_header c_header;
    u32 c_sequence;                 /* Sequence number */
    u32 c_nr_blocks;                /* Number of blocks */
    u64 c_checksum;                 /* Checksum */
    u8 c_reserved[44];
} __packed;

/**
 * struct nexfs_journal_revoke - Journal revoke block
 */
struct nexfs_journal_revoke {
    struct nexfs_journal_header r_header;
    u32 r_count;                    /* Number of entries */
    u32 r_data[];                   /* Revoked blocks */
} __packed;

/**
 * struct nexfs_journal_descriptor - Journal descriptor block
 */
struct nexfs_journal_descriptor {
    struct nexfs_journal_header d_header;
    u32 d_nr_blocks;                /* Number of blocks */
    u8 d_reserved[52];
} __packed;

/**
 * struct nexfs_journal_block_tag - Block tag in descriptor
 */
struct nexfs_journal_block_tag {
    u64 t_blocknr;                  /* Block number */
    u32 t_flags;                    /* Tag flags */
    u32 t_blocknr_high;             /* High 32 bits */
    u64 t_checksum;                 /* Block checksum */
} __packed;

/* Journal block types */
#define NEXFS_J_DESC_BLOCK        1
#define NEXFS_J_COMMIT_BLOCK      2
#define NEXFS_J_REVOKE_BLOCK      3
#define NEXFS_J_SUPERBLOCK_V2     4

/* Journal feature flags */
#define NEXFS_J_FEATURE_COMPAT_CHECKSUM   0x00000001

/* Journal incompatible flags */
#define NEXFS_J_FEATURE_INCOMPAT_REVOKE   0x00000001
#define NEXFS_J_FEATURE_INCOMPAT_64BIT    0x00000002
#define NEXFS_J_FEATURE_INCOMPAT_ASYNC    0x00000004

/* Block tag flags */
#define NEXFS_J_FLAG_ESCAPED      0x00000001
#define NEXFS_J_FLAG_SAME_UUID    0x00000002
#define NEXFS_J_FLAG_DELETED      0x00000004
#define NEXFS_J_FLAG_LAST_TAG     0x00000008

/**
 * struct nexfs_journal - In-memory journal
 */
struct nexfs_journal {
    /* Journal identity */
    u32 j_magic;
    u32 j_state;

    /* Journal file */
    struct nexfs_inode *j_inode;    /* Journal inode */
    u64 j_dev;                      /* Journal device */

    /* Journal superblock */
    struct nexfs_journal_superblock j_superblock;

    /* Transaction management */
    struct nexfs_transaction *j_current_transaction;
    struct nexfs_transaction *j_committing_transaction;
    struct list_head j_completed_transactions;

    /* Sequence numbers */
    u32 j_sequence;
    u32 j_tail_sequence;
    u32 j_head_sequence;

    /* Block pointers */
    u32 j_first_block;
    u32 j_total_blocks;
    u32 j_head;
    u32 j_tail;
    u32 j_free;

    /* Transaction limits */
    u32 j_max_transaction;
    u32 j_max_trans_data;

    /* Wait queue */
    wait_queue_head_t j_wait_transaction;
    wait_queue_head_t j_wait_checkpoint;

    /* Locks */
    spinlock_t j_lock;
    spinlock_t j_state_lock;

    /* Statistics */
    u64 j_transactions;
    u64 j_blocks_written;
    u64 j_blocks_revoked;

    /* Checksum */
    u32 j_checksum_seed;
};

/* Journal states */
#define NEXFS_JOURNAL_RUNNING     0x0001
#define NEXFS_JOURNAL_FLUSHING    0x0002
#define NEXFS_JOURNAL_ABORTED     0x0004
#define NEXFS_JOURNAL_SHUTDOWN    0x0008

/**
 * struct nexfs_transaction - Journal transaction
 */
struct nexfs_transaction {
    u32 t_transaction;              /* Transaction ID */
    u32 t_state;                    /* Transaction state */

    /* Timing */
    u64 t_start;
    u64 t_requested_commit;
    u64 t_committing;
    u64 t_committed;

    /* Block lists */
    struct list_head t_buffers;     /* Buffers to write */
    struct list_head t_locked_buffers; /* Locked buffers */
    struct list_head t_meta_data;   /* Metadata buffers */
    struct list_head t_log_buffers; /* Log buffers */

    /* Block counts */
    u32 t_nr_buffers;
    u32 t_nr_meta_data;
    u32 t_nr_log_buffers;

    /* Journal reference */
    struct nexfs_journal *t_journal;

    /* List entry */
    struct list_head t_list;

    /* Wait queue */
    wait_queue_head_t t_wait;

    /* Lock */
    spinlock_t t_lock;
};

/* Transaction states */
#define NEXUS_TRANS_RUNNING       0
#define NEXUS_TRANS_LOCKED        1
#define NEXUS_TRANS_COMMITTING    2
#define NEXUS_TRANS_COMMITTED     3
#define NEXUS_TRANS_FINISHED      4

/*===========================================================================*/
/*                         NEXFS BLOCK MAP                                   */
/*===========================================================================*/

/**
 * struct nexfs_block_map - Block mapping for I/O
 */
struct nexfs_block_map {
    u64 b_blocknr;                  /* Physical block number */
    u64 b_size;                     /* Block size */
    u32 b_state;                    /* Block state */
    u32 b_flags;                    /* Mapping flags */
};

/* Block state */
#define NEXFS_BH_New              0x0001
#define NEXFS_BH_Mapped           0x0002
#define NEXFS_BH_Delay            0x0004
#define NEXFS_BH_Unwritten        0x0008
#define NEXFS_BH_Boundary         0x0010
#define NEXFS_BH_Write            0x0020
#define NEXFS_BH_Read             0x0040
#define NEXFS_BH_Req              0x0080
#define NEXFS_BH_Touched          0x0100
#define NEXFS_BH_Expired          0x0200
#define NEXFS_BH_Verified         0x0400
#define NEXFS_BH_Meta             0x0800
#define NEXFS_BH_Eopnotsupp       0x1000

/*===========================================================================*/
/*                         NEXFS OPERATIONS                                  */
/*===========================================================================*/

/* Superblock operations */
extern struct vfs_superblock_operations nexfs_superblock_ops;

/* Inode operations */
extern struct vfs_inode_operations nexfs_inode_ops;
extern struct vfs_inode_operations nexfs_dir_ops;
extern struct vfs_inode_operations nexfs_symlink_ops;
extern struct vfs_inode_operations nexfs_file_ops;

/* File operations */
extern struct vfs_file_operations nexfs_file_operations;
extern struct vfs_file_operations nexfs_dir_operations;

/* Dentry operations */
extern struct vfs_dentry_operations nexfs_dentry_operations;

/* Filesystem type */
extern struct vfs_filesystem nexfs_filesystem;

/*===========================================================================*/
/*                         NEXFS API FUNCTIONS                               */
/*===========================================================================*/

/* Core functions */
s32 nexfs_init(void);
void nexfs_exit(void);
s32 nexfs_mount(struct vfs_filesystem *fs, u32 flags, const char *dev_name,
                void *data, struct vfs_mount **mount);
s32 nexfs_umount(struct vfs_mount *mount);

/* Superblock functions */
s32 nexfs_read_superblock(struct nexfs_superblock *sb);
s32 nexfs_write_superblock(struct nexfs_superblock *sb);
s32 nexfs_statfs(struct nexfs_superblock *sb, struct vfs_statfs *stat);

/* Inode functions */
struct nexfs_inode *nexfs_iget(struct nexfs_superblock *sb, u64 ino);
void nexfs_iput(struct nexfs_inode *inode);
s32 nexfs_read_inode(struct nexfs_inode *inode);
s32 nexfs_write_inode(struct nexfs_inode *inode, s32 sync);
struct nexfs_inode *nexfs_alloc_inode(struct nexfs_superblock *sb);
s32 nexfs_delete_inode(struct nexfs_inode *inode);

/* Block allocation */
s32 nexfs_get_block(struct nexfs_inode *inode, u64 iblock,
                    struct nexfs_block_map *map, s32 create);
s32 nexfs_alloc_blocks(struct nexfs_inode *inode, u64 start, u64 count);
s32 nexfs_free_blocks(struct nexfs_inode *inode, u64 start, u64 count);

/* Directory functions */
s32 nexfs_add_entry(struct nexfs_inode *dir, const char *name, u64 ino, u8 type);
s32 nexfs_remove_entry(struct nexfs_inode *dir, const char *name);
struct nexfs_dir_entry *nexfs_find_entry(struct nexfs_inode *dir,
                                          const char *name, void **result);
s32 nexfs_iterate_dir(struct nexfs_inode *dir, void *dirent, filldir_t filldir);

/* Extent functions */
s32 nexfs_ext_get_block(struct nexfs_inode *inode, u64 iblock,
                        struct nexfs_block_map *map);
s32 nexfs_ext_alloc_block(struct nexfs_inode *inode, u64 iblock, u64 *phys);
s32 nexfs_ext_free_block(struct nexfs_inode *inode, u64 iblock);

/* Journal functions */
s32 nexfs_journal_init(struct nexfs_superblock *sb);
void nexfs_journal_shutdown(struct nexfs_superblock *sb);
s32 nexfs_journal_start(struct nexfs_superblock *sb, u32 blocks);
s32 nexfs_journal_stop(struct nexfs_superblock *sb);
s32 nexfs_journal_get_write_access(struct nexfs_superblock *sb, u64 block);
s32 nexfs_journal_dirty_metadata(struct nexfs_superblock *sb, u64 block);
s32 nexfs_journal_commit(struct nexfs_superblock *sb);
s32 nexfs_journal_recovery(struct nexfs_superblock *sb);

/* Checksum functions */
u32 nexfs_crc32c(u32 crc, const void *buf, size_t len);
u32 nexfs_inode_checksum(struct nexfs_inode_raw *inode);
u32 nexfs_block_checksum(void *block, u32 blocknr);
u32 nexfs_superblock_checksum(struct nexfs_superblock_raw *sb);

/* Utility functions */
u32 nexfs_group_index(struct nexfs_superblock *sb, u64 block);
u64 nexfs_inode_to_block(struct nexfs_superblock *sb, u64 ino);
u64 nexfs_block_to_inode(struct nexfs_superblock *sb, u64 block);

#endif /* _NEXUS_NEXFS_H */
