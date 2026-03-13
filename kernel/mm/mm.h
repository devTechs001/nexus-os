/*
 * NEXUS OS - Memory Management Header
 * kernel/mm/mm.h
 */

#ifndef _NEXUS_MM_H
#define _NEXUS_MM_H

#include "../include/kernel.h"

/*===========================================================================*/
/*                         MEMORY MANAGEMENT                                 */
/*===========================================================================*/

/* Memory Region Types */
#define MEM_REGION_AVAILABLE    0
#define MEM_REGION_RESERVED     1
#define MEM_REGION_ACPI         2
#define MEM_REGION_NVS          3
#define MEM_REGION_UNUSABLE     4
#define MEM_REGION_KERNEL       5
#define MEM_REGION_INITRD       6

/* Memory Flags */
#define MEM_FLAG_PRESENT        (1ULL << 0)
#define MEM_FLAG_WRITABLE       (1ULL << 1)
#define MEM_FLAG_USER           (1ULL << 2)
#define MEM_FLAG_NOCACHE        (1ULL << 3)
#define MEM_FLAG_WRITECOMBINE   (1ULL << 4)
#define MEM_FLAG_HUGEPAGE       (1ULL << 5)
#define MEM_FLAG_DMA            (1ULL << 6)
#define MEM_FLAG_DMA32          (1ULL << 7)

/* Memory Zone Types */
#define ZONE_DMA                0
#define ZONE_DMA32              1
#define ZONE_NORMAL             2
#define ZONE_HIGHMEM            3
#define MAX_NR_ZONES            4

/*===========================================================================*/
/*                         MEMORY STRUCTURES                                 */
/*===========================================================================*/

/**
 * memory_region - Physical memory region
 */
struct memory_region {
    phys_addr_t start;
    phys_addr_t end;
    u32 type;
    u32 flags;
    char name[32];
};

/**
 * page - Page structure
 */
struct page {
    unsigned long flags;
    union {
        struct {
            struct page *next;
            struct page *prev;
        };
        struct {
            struct page *first;
        };
        struct list_head lru;       /* LRU list entry */
    };
    atomic_t refcount;
    atomic_t mapcount;
    unsigned long index;
    void *virtual;
    phys_addr_t physical;
    struct address_space *mapping;  /* Address space mapping */
    u32 order;                      /* Buddy allocator order */
    u32 zone;                       /* Memory zone */
};

/* Page Flags */
#define PG_locked           0
#define PG_error          1
#define PG_referenced     2
#define PG_uptodate       3
#define PG_dirty          4
#define PG_lru            5
#define PG_active         6
#define PG_slab           7
#define PG_reserved       8
#define PG_private        9
#define PG_writeback      10
#define PG_head           11
#define PG_tail           12
#define PG_compound       13
#define PG_swapcache      14
#define PG_mappedtodisk   15

/**
 * vm_area - Virtual memory area
 */
struct vm_area {
    virt_addr_t start;
    virt_addr_t end;
    u32 flags;
    u32 prot;
    struct vm_area *next;
    struct vm_area *prev;
    struct page *pages;
    void *private;
};

/**
 * vm_area_struct - Virtual memory area structure (Linux compatible)
 */
struct vm_area_struct {
    virt_addr_t vm_start;
    virt_addr_t vm_end;
    struct address_space *vm_file;
    void *vm_private_data;
    u32 vm_flags;
    u32 vm_page_prot;
    struct vm_area_struct *vm_next;
    struct rb_node rb;
    struct list_head anon_vma_node;
    struct mm_struct *vm_mm;
    unsigned long vm_pgoff;
};

/**
 * mm_struct - Memory descriptor
 */
struct mm_struct {
    struct vm_area_struct *mmap;
    struct rb_root mm_rb;
    struct vm_area_struct *mmap_cache;
    unsigned long pgd;
    atomic_t mm_users;
    atomic_t mm_count;
    int map_count;
    unsigned long total_vm;
    unsigned long locked_vm;
    spinlock_t page_table_lock;
    struct task_struct *owner;
};

/**
 * vm_fault - Page fault information
 */
struct vm_fault {
    unsigned int flags;
    virt_addr_t address;
    struct vm_area_struct *vma;
    struct page *page;
    pte_t *pte;
    spinlock_t *ptl;
};

/* VMA Flags */
#define VMA_READ            0x00000001
#define VMA_WRITE           0x00000002
#define VMA_EXEC            0x00000004
#define VMA_USER            0x00000008
#define VMA_SHARED          0x00000010
#define VMA_ANONYMOUS       0x00000020
#define VMA_FILE_BACKED     0x00000040
#define VMA_GROWSDOWN       0x00000080
#define VMA_GROWSUP         0x00000100
#define VMA_DONTCOPY        0x00000200
#define VMA_LOCKED          0x00000400
#define VMA_IO              0x00000800

/**
 * address_space - Address space
 */
struct address_space {
    struct vm_area *vmas;
    u64 size;
    u32 flags;
    spinlock_t lock;
    void *private;
};

/**
 * fown_struct - File owner structure
 */
struct fown_struct {
    rwlock_t lock;
    pid_t pid;
    uid_t uid;
    int signum;
};

/**
 * file - File structure
 */
struct file {
    struct list_head f_list;
    struct dentry *f_dentry;
    struct vfsmount *f_vfsmnt;
    struct address_space *f_mapping;
    const struct file_operations *f_op;
    unsigned int f_flags;
    void *private_data;
    struct fown_struct f_owner;
    pid_t f_pid;
    unsigned long f_version;
    void *security;
};

/**
 * dentry - Directory entry
 */
struct dentry {
    const char *d_name;
    struct dentry *d_parent;
    struct inode *d_inode;
    struct list_head d_child;
    struct list_head d_subdirs;
    void *d_fsdata;
    unsigned int d_flags;
    spinlock_t d_lock;
};

/**
 * inode - Inode structure
 */
struct inode {
    unsigned long i_ino;
    unsigned int i_mode;
    unsigned int i_nlink;
    unsigned int i_uid;
    unsigned int i_gid;
    unsigned long i_size;
    unsigned long i_blocks;
    unsigned long i_atime;
    unsigned long i_mtime;
    unsigned long i_ctime;
    struct address_space *i_data;
    const struct inode_operations *i_op;
    void *i_private;
};

/**
 * vfsmount - Virtual filesystem mount
 */
struct vfsmount {
    struct list_head mnt_hash;
    struct vfsmount *mnt_parent;
    struct dentry *mnt_mountpoint;
    struct dentry *mnt_root;
    struct super_block *mnt_sb;
    struct list_head mnt_mounts;
    struct list_head mnt_child;
    int mnt_flags;
    const char *mnt_devname;
    struct list_head mnt_list;
};

/**
 * super_block - Superblock structure
 */
struct super_block {
    struct list_head s_list;
    unsigned int s_dev;
    unsigned long s_blocksize;
    unsigned char s_blocksize_bits;
    unsigned char s_dirt;
    u64 s_maxbytes;
    void *s_fs_info;
    const struct super_operations *s_op;
    struct dentry *s_root;
};

/**
 * file_operations - File operations
 */
struct file_operations {
    struct module *owner;
    ssize_t (*read)(struct file *, char *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
};

/**
 * inode_operations - Inode operations
 */
struct inode_operations {
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*mknod)(struct inode *, struct dentry *, umode_t, dev_t);
    int (*rename)(struct inode *, struct dentry *, struct inode *, struct dentry *, unsigned int);
};

/**
 * super_operations - Superblock operations
 */
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *);
    void (*destroy_inode)(struct inode *);
    void (*read_inode)(struct inode *);
    int (*write_inode)(struct inode *, int);
    void (*put_inode)(struct inode *);
    void (*delete_inode)(struct inode *);
};

/**
 * module - Kernel module
 */
struct module {
    const char *name;
    struct list_head list;
    void *module_init;
    void *module_core;
    unsigned int init_size;
    unsigned int core_size;
};

/*===========================================================================*/
/*                         PHYSICAL MEMORY MANAGER                           */
/*===========================================================================*/

/* Physical Memory Manager Functions */
void pmm_init(void);
void pmm_init_region(phys_addr_t base, size_t size);
void pmm_free_region(phys_addr_t base, size_t size);

phys_addr_t pmm_alloc_page(void);
phys_addr_t pmm_alloc_pages(size_t count);
void pmm_free_page(phys_addr_t page);
void pmm_free_pages(phys_addr_t pages, size_t count);

size_t pmm_get_free_pages(void);
size_t pmm_get_total_pages(void);
size_t pmm_get_used_pages(void);

struct page *pmm_get_page(phys_addr_t addr);
phys_addr_t pmm_get_phys_addr(struct page *page);

/*===========================================================================*/
/*                         VIRTUAL MEMORY MANAGER                            */
/*===========================================================================*/

/* Virtual Memory Manager Functions */
void vmm_init(void);

result_t vmm_map(virt_addr_t vaddr, phys_addr_t paddr, u32 flags);
result_t vmm_unmap(virt_addr_t vaddr);
result_t vmm_map_range(virt_addr_t vaddr, phys_addr_t paddr, size_t size, u32 flags);
result_t vmm_unmap_range(virt_addr_t vaddr, size_t size);

phys_addr_t vmm_virt_to_phys(virt_addr_t vaddr);
virt_addr_t vmm_phys_to_virt(phys_addr_t paddr);

struct address_space *vmm_create_as(void);
void vmm_destroy_as(struct address_space *as);
result_t vmm_switch_as(struct address_space *as);
struct address_space *vmm_get_current_as(void);

/* Page Table Operations */
void vmm_flush_tlb(void);
void vmm_flush_tlb_addr(virt_addr_t vaddr);
void vmm_flush_tlb_as(struct address_space *as);

/*===========================================================================*/
/*                         KERNEL HEAP                                       */
/*===========================================================================*/

/* Kernel Heap Functions */
void heap_init(void);
void heap_early_init(void);

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);

void *kmalloc_aligned(size_t size, size_t align);
void *kzalloc_aligned(size_t size, size_t align);

/* Slab Allocator */
void slab_init(void);

void *kmem_cache_alloc(void *cache);
void kmem_cache_free(void *cache, void *obj);

void *kmem_cache_create(const char *name, size_t size, size_t align, u32 flags);
void kmem_cache_destroy(void *cache);

/*===========================================================================*/
/*                         PAGE ALLOCATOR                                    */
/*===========================================================================*/

/* Page Allocator Functions */
void page_alloc_init(void);

struct page *alloc_page(u32 flags);
struct page *alloc_pages(u32 order, u32 flags);
void free_page(struct page *page);
void free_pages(struct page *page, u32 order);

/* GFP Flags */
#define GFP_NOWAIT          0x00000001
#define GFP_ATOMIC          0x00000002
#define GFP_KERNEL          0x00000004
#define GFP_USER            0x00000008
#define GFP_DMA             0x00000010
#define GFP_DMA32           0x00000020
#define GFP_HIGHMEM         0x00000040
#define GFP_ZERO            0x00000080
#define GFP_NORETRY         0x00000100
#define GFP_NOIO            0x00000200
#define GFP_NOFS            0x00000400

/* Buddy Allocator */
void *buddy_alloc(size_t size);
void buddy_free(void *ptr, size_t size);

/*===========================================================================*/
/*                         MEMORY MAPPING                                    */
/*===========================================================================*/

/* Memory Mapping Functions */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t length, int prot);
int msync(void *addr, size_t length, int flags);

/* mmap flags */
#define MAP_SHARED          0x01
#define MAP_PRIVATE         0x02
#define MAP_FIXED           0x10
#define MAP_ANONYMOUS       0x20
#define MAP_POPULATE        0x80
#define MAP_LOCKED          0x100
#define MAP_NORESERVE       0x200

/* mmap prot */
#define PROT_NONE           0x0
#define PROT_READ           0x1
#define PROT_WRITE          0x2
#define PROT_EXEC           0x4

/*===========================================================================*/
/*                         MEMORY UTILITY FUNCTIONS                          */
/*===========================================================================*/

/* Memory Utilities */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

/* String Functions */
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

/* Memory Information */
void mem_info(void);
size_t mem_get_total(void);
size_t mem_get_free(void);
size_t mem_get_used(void);

#endif /* _NEXUS_MM_H */
