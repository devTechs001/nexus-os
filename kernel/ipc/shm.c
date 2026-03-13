/*
 * NEXUS OS - Shared Memory Implementation
 * kernel/ipc/shm.c
 */

#include "ipc.h"
#include "../sched/sched.h"

/*===========================================================================*/
/*                         SHARED MEMORY CONFIGURATION                       */
/*===========================================================================*/

#define SHM_MAGIC           0xSHM00001
#define SHM_HASH_SIZE       256
#define SHM_MIN_ID          0
#define SHM_MAX_ID          32767

/* Shared Memory Flags (internal) */
#define SHM_FLAG_ALLOCATED  0x00000001
#define SHM_FLAG_LOCKED     0x00000002
#define SHM_FLAG_DEST       0x00000004
#define SHM_FLAG_HUGETLB    0x00000008

/*===========================================================================*/
/*                         SHARED MEMORY GLOBAL DATA                         */
/*===========================================================================*/

static struct {
    spinlock_t lock;                  /* Global shm lock */
    struct shmid_kernel *ids[SHM_MAX_ID]; /* ID table */
    atomic_t shm_count;               /* Active segments */
    atomic_t total_allocated;         /* Total allocations */
    atomic_t total_freed;             /* Total frees */
    size_t total_memory;              /* Total memory allocated */
    struct list_head shm_list;        /* List of all segments */
} shm_global = {
    .lock = { .lock = 0 },
    .shm_count = ATOMIC_INIT(0),
    .total_allocated = ATOMIC_INIT(0),
    .total_freed = ATOMIC_INIT(0),
    .total_memory = 0,
};

/* Key to ID hash table */
static struct list_head shm_key_hash[SHM_HASH_SIZE];

/*===========================================================================*/
/*                         HASH TABLE OPERATIONS                             */
/*===========================================================================*/

/**
 * shm_hash_key - Hash a key to bucket index
 * @key: IPC key to hash
 *
 * Returns: Hash bucket index
 */
static inline u32 shm_hash_key(key_t key)
{
    return (u32)key % SHM_HASH_SIZE;
}

/**
 * shm_hash_init - Initialize hash table
 */
static void shm_hash_init(void)
{
    u32 i;

    for (i = 0; i < SHM_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&shm_key_hash[i]);
    }
}

/**
 * shm_hash_insert - Insert segment into hash table
 * @shp: Segment to insert
 */
static void shm_hash_insert(struct shmid_kernel *shp)
{
    u32 hash;

    if (!shp) {
        return;
    }

    hash = shm_hash_key(shp->shm_perm.key);
    list_add(&shp->shm_clist, &shm_key_hash[hash]);
}

/**
 * shm_hash_remove - Remove segment from hash table
 * @shp: Segment to remove
 */
static void shm_hash_remove(struct shmid_kernel *shp)
{
    if (!shp) {
        return;
    }

    list_del(&shp->shm_clist);
    INIT_LIST_HEAD(&shp->shm_clist);
}

/**
 * shm_hash_find - Find segment by key
 * @key: IPC key to find
 *
 * Returns: Pointer to segment or NULL
 */
static struct shmid_kernel *shm_hash_find(key_t key)
{
    u32 hash;
    struct shmid_kernel *shp;

    if (key == IPC_PRIVATE) {
        return NULL;
    }

    hash = shm_hash_key(key);

    list_for_each_entry(shp, &shm_key_hash[hash], shm_clist) {
        if (shp->shm_perm.key == key) {
            return shp;
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         SHARED MEMORY ID MANAGEMENT                       */
/*===========================================================================*/

/**
 * shm_alloc_id - Allocate a new shared memory ID
 *
 * Returns: ID on success, -1 on failure
 */
static int shm_alloc_id(void)
{
    static int next_id = SHM_MIN_ID;
    int id;
    int start_id;
    int count = 0;

    start_id = next_id;

    while (count < SHM_MAX_ID) {
        id = next_id;
        next_id = (next_id + 1) % SHM_MAX_ID;

        if (!shm_global.ids[id]) {
            return id;
        }

        count++;
    }

    return -1; /* No free IDs */
}

/**
 * shm_free_id - Free a shared memory ID
 * @id: ID to free
 */
static void shm_free_id(int id)
{
    if (id >= 0 && id < SHM_MAX_ID) {
        shm_global.ids[id] = NULL;
    }
}

/*===========================================================================*/
/*                         SHARED MEMORY ALLOCATION                          */
/*===========================================================================*/

/**
 * shm_alloc - Allocate a new shared memory segment
 * @key: IPC key (IPC_PRIVATE for new key)
 * @size: Segment size
 * @flags: Creation flags
 *
 * Returns: Pointer to shmid_kernel on success, NULL on failure
 */
struct shmid_kernel *shm_alloc(key_t key, size_t size, int flags)
{
    struct shmid_kernel *shp;
    struct shmid_kernel *existing;
    int id;
    phys_addr_t paddr;
    void *addr;

    if (size == 0 || size > SHM_MAX_SIZE) {
        pr_err("Shm: Invalid size %zu\n", size);
        return NULL;
    }

    /* Align size to page boundary */
    size = ALIGN(size, SHM_ALIGN);

    /* Check for existing segment with same key */
    if (key != IPC_PRIVATE) {
        existing = shm_hash_find(key);
        if (existing) {
            if (flags & IPC_EXCL) {
                return NULL; /* EEXIST */
            }

            /* Return existing segment */
            atomic_inc(&existing->refcount);
            return existing;
        }
    }

    /* Allocate ID */
    id = shm_alloc_id();
    if (id < 0) {
        pr_err("Shm: No free IDs available\n");
        return NULL;
    }

    /* Allocate segment structure */
    shp = kzalloc(sizeof(struct shmid_kernel));
    if (!shp) {
        shm_free_id(id);
        return NULL;
    }

    /* Allocate physical memory */
    paddr = pmm_alloc_pages((size / PAGE_SIZE) - 1);
    if (paddr == 0) {
        kfree(shp);
        shm_free_id(id);
        return NULL;
    }

    /* Map to virtual address */
    addr = (void *)vmm_phys_to_virt(paddr);
    if (!addr) {
        pmm_free_pages(paddr, (size / PAGE_SIZE) - 1);
        kfree(shp);
        shm_free_id(id);
        return NULL;
    }

    /* Initialize segment */
    shp->shm_perm.key = key;
    shp->shm_perm.uid = 0; /* Current user */
    shp->shm_perm.gid = 0;
    shp->shm_perm.cuid = 0;
    shp->shm_perm.cgid = 0;
    shp->shm_perm.mode = (flags & 0x1FF); /* Lower 9 bits */
    shp->shm_perm.seq = (u16)id;

    shp->shm_segsz = size;
    shp->shm_atime = 0;
    shp->shm_dtime = 0;
    shp->shm_ctime = get_time_ms() / 1000;
    shp->shm_cpid = current_process() ? current_process()->pid : 0;
    shp->shm_lpid = 0;
    shp->shm_nattch = 0;
    shp->shm_flags = SHM_FLAG_ALLOCATED;

    shp->file = NULL;
    shp->addr = addr;
    shp->paddr = paddr;

    INIT_LIST_HEAD(&shp->shm_clist);
    atomic_set(&shp->refcount, 1);

    /* Insert into ID table and hash */
    shm_global.ids[id] = shp;
    if (key != IPC_PRIVATE) {
        shm_hash_insert(shp);
    }

    /* Update global statistics */
    spin_lock(&shm_global.lock);
    atomic_inc(&shm_global.shm_count);
    atomic_inc(&shm_global.total_allocated);
    shm_global.total_memory += size;
    list_add(&shp->shm_clist, &shm_global.shm_list);
    spin_unlock(&shm_global.lock);

    pr_debug("Shm: Allocated segment id=%d key=%x size=%zu addr=%p\n",
             id, key, size, addr);

    return shp;
}

/**
 * shm_free - Free a shared memory segment
 * @shp: Segment to free
 */
void shm_free(struct shmid_kernel *shp)
{
    size_t size;
    int id;

    if (!shp) {
        return;
    }

    pr_debug("Shm: Freeing segment id=%d key=%x\n",
             shp->shm_perm.seq, shp->shm_perm.key);

    /* Remove from hash table */
    if (shp->shm_perm.key != IPC_PRIVATE) {
        shm_hash_remove(shp);
    }

    /* Remove from ID table */
    id = shp->shm_perm.seq;
    if (id >= 0 && id < SHM_MAX_ID) {
        shm_global.ids[id] = NULL;
    }

    /* Free physical memory */
    size = shp->shm_segsz;
    if (shp->paddr != 0) {
        pmm_free_pages(shp->paddr, (size / PAGE_SIZE) - 1);
    }

    /* Update global statistics */
    spin_lock(&shm_global.lock);
    atomic_dec(&shm_global.shm_count);
    atomic_inc(&shm_global.total_freed);
    shm_global.total_memory -= size;
    list_del(&shp->shm_clist);
    spin_unlock(&shm_global.lock);

    /* Free segment structure */
    kfree(shp);
}

/**
 * shm_find - Find a shared memory segment by ID
 * @shmid: Shared memory ID
 *
 * Returns: Pointer to segment or NULL
 */
struct shmid_kernel *shm_find(int shmid)
{
    struct shmid_kernel *shp;

    if (shmid < 0 || shmid >= SHM_MAX_ID) {
        return NULL;
    }

    spin_lock(&shm_global.lock);
    shp = shm_global.ids[shmid];
    if (shp) {
        atomic_inc(&shp->refcount);
    }
    spin_unlock(&shm_global.lock);

    return shp;
}

/*===========================================================================*/
/*                         SHMGET SYSTEM CALL                                */
/*===========================================================================*/

/**
 * shmget - Create or access shared memory segment
 * @key: IPC key
 * @size: Segment size
 * @flags: Creation flags and permissions
 *
 * Returns: Segment ID on success, negative on error
 */
int shmget(key_t key, size_t size, int flags)
{
    struct shmid_kernel *shp;
    int shmid;

    /* Check if segment exists */
    if (key != IPC_PRIVATE && !(flags & IPC_CREAT)) {
        shp = shm_hash_find(key);
        if (shp) {
            shmid = shp->shm_perm.seq;
            atomic_dec(&shp->refcount);
            return shmid;
        }
        return -ENOENT;
    }

    /* Create new segment */
    if (flags & IPC_CREAT) {
        shp = shm_alloc(key, size, flags);
        if (!shp) {
            return -EEXIST;
        }

        shmid = shp->shm_perm.seq;
        atomic_dec(&shp->refcount);
        return shmid;
    }

    return -EINVAL;
}

/*===========================================================================*/
/*                         SHMAT SYSTEM CALL                                 */
/*===========================================================================*/

/**
 * shmat - Attach shared memory segment
 * @shmid: Shared memory ID
 * @addr: Suggested attach address
 * @flags: Attach flags
 *
 * Returns: Attach address on success, error pointer on failure
 */
void *shmat(int shmid, const void *addr, int flags)
{
    struct shmid_kernel *shp;
    void *attach_addr;

    shp = shm_find(shmid);
    if (!shp) {
        return (void *)(long)-EINVAL;
    }

    /* Check permissions */
    /* In real implementation: check access rights */

    /* Determine attach address */
    if (flags & SHM_RND) {
        /* Round address */
        attach_addr = (void *)((uintptr_t)addr & ~(SHM_ALIGN - 1));
    } else if (addr) {
        attach_addr = (void *)addr;
    } else {
        /* Let kernel choose address */
        attach_addr = shp->addr;
    }

    /* In real implementation: create VMA and map pages */

    /* Update segment info */
    spin_lock(&shm_global.lock);
    shp->shm_nattch++;
    shp->shm_atime = get_time_ms() / 1000;
    shp->shm_lpid = current_process() ? current_process()->pid : 0;
    spin_unlock(&shm_global.lock);

    atomic_dec(&shp->refcount);

    pr_debug("Shm: Attached segment %d at %p\n", shmid, attach_addr);

    return attach_addr;
}

/*===========================================================================*/
/*                         SHMDT SYSTEM CALL                                 */
/*===========================================================================*/

/**
 * shmdt - Detach shared memory segment
 * @addr: Address to detach
 *
 * Returns: 0 on success, negative on error
 */
int shmdt(const void *addr)
{
    struct shmid_kernel *shp;
    int found = 0;
    int i;

    if (!addr) {
        return -EINVAL;
    }

    spin_lock(&shm_global.lock);

    /* Find segment by address */
    for (i = 0; i < SHM_MAX_ID; i++) {
        shp = shm_global.ids[i];
        if (shp && shp->addr == addr) {
            if (shp->shm_nattch > 0) {
                shp->shm_nattch--;
            }
            found = 1;
            break;
        }
    }

    spin_unlock(&shm_global.lock);

    if (!found) {
        return -EINVAL;
    }

    /* In real implementation: unmap VMA */

    pr_debug("Shm: Detached segment at %p\n", addr);

    return 0;
}

/*===========================================================================*/
/*                         SHMCTL SYSTEM CALL                                */
/*===========================================================================*/

/**
 * shmctl - Shared memory control
 * @shmid: Shared memory ID
 * @cmd: Command
 * @buf: Buffer for data
 *
 * Returns: 0 on success, negative on error
 */
int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    struct shmid_kernel *shp;
    int ret = 0;

    shp = shm_find(shmid);
    if (!shp) {
        return -EINVAL;
    }

    switch (cmd) {
    case IPC_STAT:
        /* Get segment info */
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&shm_global.lock);
        buf->shm_perm = shp->shm_perm;
        buf->shm_segsz = shp->shm_segsz;
        buf->shm_atime = shp->shm_atime;
        buf->shm_dtime = shp->shm_dtime;
        buf->shm_ctime = shp->shm_ctime;
        buf->shm_cpid = shp->shm_cpid;
        buf->shm_lpid = shp->shm_lpid;
        buf->shm_nattch = shp->shm_nattch;
        spin_unlock(&shm_global.lock);
        break;

    case IPC_SET:
        /* Set segment info */
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&shm_global.lock);
        shp->shm_perm.mode = buf->shm_perm.mode;
        shp->shm_perm.uid = buf->shm_perm.uid;
        shp->shm_perm.gid = buf->shm_perm.gid;
        shp->shm_ctime = get_time_ms() / 1000;
        spin_unlock(&shm_global.lock);
        break;

    case IPC_RMID:
        /* Mark for removal */
        spin_lock(&shm_global.lock);
        shp->shm_flags |= SHM_FLAG_DEST;
        shp->shm_dtime = get_time_ms() / 1000;

        /* Free if no attachments */
        if (shp->shm_nattch == 0) {
            spin_unlock(&shm_global.lock);
            shm_free(shp);
        } else {
            spin_unlock(&shm_global.lock);
        }
        break;

    case SHM_LOCK:
        /* Lock segment in memory */
        spin_lock(&shm_global.lock);
        shp->shm_flags |= SHM_FLAG_LOCKED;
        spin_unlock(&shm_global.lock);
        break;

    case SHM_UNLOCK:
        /* Unlock segment */
        spin_lock(&shm_global.lock);
        shp->shm_flags &= ~SHM_FLAG_LOCKED;
        spin_unlock(&shm_global.lock);
        break;

    default:
        ret = -EINVAL;
        break;
    }

    atomic_dec(&shp->refcount);

    return ret;
}

/*===========================================================================*/
/*                         MEMORY MAPPING                                    */
/*===========================================================================*/

/**
 * shm_mmap - Map shared memory to user space
 * @file: File pointer
 * @vma: Virtual memory area
 *
 * Returns: 0 on success, negative on error
 */
int shm_mmap(struct file *file, struct vm_area_struct *vma)
{
    /* In real implementation: map pages to user space */
    return 0;
}

/**
 * shm_fault - Handle page fault in shared memory
 * @vma: Virtual memory area
 * @vmf: VM fault info
 *
 * Returns: 0 on success, negative on error
 */
int shm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    /* In real implementation: handle page fault */
    return 0;
}

/*===========================================================================*/
/*                         SHARED MEMORY STATISTICS                          */
/*===========================================================================*/

/**
 * shm_stats - Print shared memory statistics
 */
void shm_stats(void)
{
    struct shmid_kernel *shp;
    int i;
    int active_count = 0;
    size_t total_size = 0;

    printk("\n=== Shared Memory Statistics ===\n");

    spin_lock(&shm_global.lock);

    printk("Active Segments: %d\n", atomic_read(&shm_global.shm_count));
    printk("Total Allocated: %d\n", atomic_read(&shm_global.total_allocated));
    printk("Total Freed: %d\n", atomic_read(&shm_global.total_freed));
    printk("Total Memory: %zu bytes (%zu MB)\n",
           shm_global.total_memory,
           shm_global.total_memory / MB(1));

    printk("\nSegment Details:\n");

    for (i = 0; i < SHM_MAX_ID; i++) {
        shp = shm_global.ids[i];
        if (shp) {
            active_count++;
            total_size += shp->shm_segsz;

            printk("  ID: %d, Key: 0x%x, Size: %zu, Attachments: %u\n",
                   i, shp->shm_perm.key, shp->shm_segsz, shp->shm_nattch);
        }
    }

    printk("\nTotal Active: %d segments, %zu bytes\n", active_count, total_size);

    spin_unlock(&shm_global.lock);
}

/*===========================================================================*/
/*                         SHARED MEMORY INITIALIZATION                      */
/*===========================================================================*/

/**
 * shm_init - Initialize shared memory subsystem
 */
void shm_init(void)
{
    u32 i;

    pr_info("Initializing Shared Memory Subsystem...\n");

    spin_lock_init(&shm_global.lock);
    INIT_LIST_HEAD(&shm_global.shm_list);

    /* Initialize ID table */
    for (i = 0; i < SHM_MAX_ID; i++) {
        shm_global.ids[i] = NULL;
    }

    /* Initialize hash table */
    shm_hash_init();

    /* Initialize counters */
    atomic_set(&shm_global.shm_count, 0);
    atomic_set(&shm_global.total_allocated, 0);
    atomic_set(&shm_global.total_freed, 0);
    shm_global.total_memory = 0;

    pr_info("  Maximum segments: %d\n", SHM_MAX_ID);
    pr_info("  Maximum size: %zu MB\n", SHM_MAX_SIZE / MB(1));
    pr_info("  Hash table size: %d buckets\n", SHM_HASH_SIZE);
    pr_info("Shared Memory Subsystem initialized.\n");
}
