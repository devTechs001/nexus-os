/*
 * NEXUS OS - Semaphore Implementation
 * kernel/ipc/semaphore.c
 */

#include "ipc.h"
#include "../sched/sched.h"
#include <stdarg.h>

/*===========================================================================*/
/*                         SEMAPHORE CONFIGURATION                           */
/*===========================================================================*/

#define SEM_MAGIC           0x53450001
#define SEM_HASH_SIZE       128
#define SEM_MIN_ID          0
#define SEM_MAX_ID          32767

/* Semaphore Operation Flags */
#define SEM_UNDO_FLAG       0x1000
#define SEM_NOWAIT_FLAG     0x2000

/*===========================================================================*/
/*                         SEMAPHORE GLOBAL DATA                             */
/*===========================================================================*/

static struct {
    spinlock_t lock;                  /* Global semaphore lock */
    struct sem_array *ids[SEM_MAX_ID]; /* ID table */
    atomic_t sem_count;               /* Active semaphore sets */
    atomic_t total_allocated;         /* Total allocations */
    atomic_t total_freed;             /* Total frees */
    atomic_t total_ops;               /* Total operations */
    struct list_head sem_list;        /* List of all sets */
} sem_global = {
    .lock = { .lock = 0 },
    .sem_count = ATOMIC_INIT(0),
    .total_allocated = ATOMIC_INIT(0),
    .total_freed = ATOMIC_INIT(0),
    .total_ops = ATOMIC_INIT(0),
};

/* Key to ID hash table */
static struct list_head sem_key_hash[SEM_HASH_SIZE];

/* Per-process undo list */
static struct {
    spinlock_t lock;
    struct list_head undo_list;
} sem_undo_global = {
    .lock = { .lock = 0 },
};

/*===========================================================================*/
/*                         HASH TABLE OPERATIONS                             */
/*===========================================================================*/

/**
 * sem_hash_key - Hash a key to bucket index
 * @key: IPC key to hash
 *
 * Returns: Hash bucket index
 */
static inline u32 sem_hash_key(key_t key)
{
    return (u32)key % SEM_HASH_SIZE;
}

/**
 * sem_hash_init - Initialize hash table
 */
static void sem_hash_init(void)
{
    u32 i;

    for (i = 0; i < SEM_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&sem_key_hash[i]);
    }
}

/**
 * sem_hash_insert - Insert semaphore set into hash table
 * @sma: Semaphore set to insert
 */
static void sem_hash_insert(struct sem_array *sma)
{
    u32 hash;

    if (!sma) {
        return;
    }

    hash = sem_hash_key(sma->sem_perm.key);
    list_add(&sma->undo_list, &sem_key_hash[hash]);
}

/**
 * sem_hash_remove - Remove semaphore set from hash table
 * @sma: Semaphore set to remove
 */
static void sem_hash_remove(struct sem_array *sma)
{
    if (!sma) {
        return;
    }

    list_del(&sma->undo_list);
    INIT_LIST_HEAD(&sma->undo_list);
}

/**
 * sem_hash_find - Find semaphore set by key
 * @key: IPC key to find
 *
 * Returns: Pointer to semaphore set or NULL
 */
static struct sem_array *sem_hash_find(key_t key)
{
    u32 hash;
    struct sem_array *sma;

    if (key == IPC_PRIVATE) {
        return NULL;
    }

    hash = sem_hash_key(key);

    list_for_each_entry(sma, &sem_key_hash[hash], undo_list) {
        if (sma->sem_perm.key == key) {
            return sma;
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         SEMAPHORE ID MANAGEMENT                           */
/*===========================================================================*/

/**
 * sem_alloc_id - Allocate a new semaphore ID
 *
 * Returns: ID on success, -1 on failure
 */
static int sem_alloc_id(void)
{
    static int next_id = SEM_MIN_ID;
    int id;
    int count = 0;

    while (count < SEM_MAX_ID) {
        id = next_id;
        next_id = (next_id + 1) % SEM_MAX_ID;

        if (!sem_global.ids[id]) {
            return id;
        }

        count++;
    }

    return -1;
}

/**
 * sem_free_id - Free a semaphore ID
 * @id: ID to free
 */
static void sem_free_id(int id)
{
    if (id >= 0 && id < SEM_MAX_ID) {
        sem_global.ids[id] = NULL;
    }
}

/*===========================================================================*/
/*                         SEMAPHORE ALLOCATION                              */
/*===========================================================================*/

/**
 * sem_alloc - Allocate a new semaphore set
 * @key: IPC key
 * @nsems: Number of semaphores
 * @flags: Creation flags
 *
 * Returns: Pointer to sem_array on success, NULL on failure
 */
struct sem_array *sem_alloc(key_t key, int nsems, int flags)
{
    struct sem_array *sma;
    struct sem_array *existing;
    int id;
    size_t size;
    int i;

    if (nsems <= 0 || nsems > SEMVMX) {
        pr_err("Sem: Invalid number of semaphores %d\n", nsems);
        return NULL;
    }

    /* Check for existing set with same key */
    if (key != IPC_PRIVATE) {
        existing = sem_hash_find(key);
        if (existing) {
            if (flags & IPC_EXCL) {
                return NULL;
            }

            /* Return existing set */
            atomic_inc(&existing->refcount);
            return existing;
        }
    }

    /* Allocate ID */
    id = sem_alloc_id();
    if (id < 0) {
        pr_err("Sem: No free IDs available\n");
        return NULL;
    }

    /* Allocate semaphore array structure */
    size = sizeof(struct sem_array) + (nsems * sizeof(struct sem));
    sma = kzalloc(size);
    if (!sma) {
        sem_free_id(id);
        return NULL;
    }

    /* Initialize semaphore array */
    sma->sem_perm.key = key;
    sma->sem_perm.uid = 0;
    sma->sem_perm.gid = 0;
    sma->sem_perm.cuid = 0;
    sma->sem_perm.cgid = 0;
    sma->sem_perm.mode = (flags & 0x1FF);
    sma->sem_perm.seq = (u16)id;

    sma->sem_base = (struct sem *)((u8 *)sma + sizeof(struct sem_array));
    sma->sem_nsems = (u16)nsems;
    sma->sem_otime = 0;
    sma->sem_otime_pid = 0;
    sma->sem_ctime = get_time_ms() / 1000;

    INIT_LIST_HEAD(&sma->pending_alter);
    INIT_LIST_HEAD(&sma->undo_list);

    spin_lock_init(&sma->lock);
    atomic_set(&sma->refcount, 1);
    atomic_set(&sma->unused, 0);

    /* Initialize individual semaphores */
    for (i = 0; i < nsems; i++) {
        sma->sem_base[i].semval = 1; /* Default to 1 */
        sma->sem_base[i].semadj = 0;
        sma->sem_base[i].semopm = 0;
        sma->sem_base[i].sempid = 0;
        INIT_LIST_HEAD(&sma->sem_base[i].pending_alter);
        INIT_LIST_HEAD(&sma->sem_base[i].pending_const);
        INIT_LIST_HEAD(&sma->sem_base[i].list);
    }

    /* Insert into ID table and hash */
    sem_global.ids[id] = sma;
    if (key != IPC_PRIVATE) {
        sem_hash_insert(sma);
    }

    /* Update global statistics */
    spin_lock(&sem_global.lock);
    atomic_inc(&sem_global.sem_count);
    atomic_inc(&sem_global.total_allocated);
    list_add(&sma->undo_list, &sem_global.sem_list);
    spin_unlock(&sem_global.lock);

    pr_debug("Sem: Allocated set id=%d key=%x nsems=%d\n", id, key, nsems);

    return sma;
}

/**
 * sem_free - Free a semaphore set
 * @sma: Semaphore set to free
 */
void sem_free(struct sem_array *sma)
{
    int id;

    if (!sma) {
        return;
    }

    pr_debug("Sem: Freeing set id=%d key=%x\n",
             sma->sem_perm.seq, sma->sem_perm.key);

    /* Remove from hash table */
    if (sma->sem_perm.key != IPC_PRIVATE) {
        sem_hash_remove(sma);
    }

    /* Remove from ID table */
    id = sma->sem_perm.seq;
    if (id >= 0 && id < SEM_MAX_ID) {
        sem_global.ids[id] = NULL;
    }

    /* Update global statistics */
    spin_lock(&sem_global.lock);
    atomic_dec(&sem_global.sem_count);
    atomic_inc(&sem_global.total_freed);
    list_del(&sma->undo_list);
    spin_unlock(&sem_global.lock);

    /* Free semaphore structure */
    kfree(sma);
}

/**
 * sem_find - Find a semaphore set by ID
 * @semid: Semaphore set ID
 *
 * Returns: Pointer to semaphore set or NULL
 */
struct sem_array *sem_find(int semid)
{
    struct sem_array *sma;

    if (semid < 0 || semid >= SEM_MAX_ID) {
        return NULL;
    }

    spin_lock(&sem_global.lock);
    sma = sem_global.ids[semid];
    if (sma) {
        atomic_inc(&sma->refcount);
    }
    spin_unlock(&sem_global.lock);

    return sma;
}

/*===========================================================================*/
/*                         SEMAPHORE UNDO OPERATIONS                         */
/*===========================================================================*/

/**
 * sem_undo_alloc - Allocate undo structure
 * @sma: Semaphore set
 * @semnum: Semaphore number
 *
 * Returns: Pointer to undo structure or NULL
 */
struct sem_undo *sem_undo_alloc(struct sem_array *sma, int semnum)
{
    struct sem_undo *un;

    if (!sma || semnum < 0 || semnum >= sma->sem_nsems) {
        return NULL;
    }

    un = kzalloc(sizeof(struct sem_undo));
    if (!un) {
        return NULL;
    }

    un->semid = sma->sem_perm.seq;
    un->nsemadj = 1;
    un->semadj = kzalloc(sizeof(s16));
    if (!un->semadj) {
        kfree(un);
        return NULL;
    }

    un->semadj[0] = 0;

    INIT_LIST_HEAD(&un->list);
    INIT_LIST_HEAD(&un->list_id);

    return un;
}

/**
 * sem_undo_free - Free undo structure
 * @un: Undo structure to free
 */
void sem_undo_free(struct sem_undo *un)
{
    if (!un) {
        return;
    }

    if (un->semadj) {
        kfree(un->semadj);
    }

    kfree(un);
}

/**
 * sem_undo_exit - Clean up undo structures on process exit
 * @task: Exiting task
 */
void sem_undo_exit(struct task_struct *task)
{
    struct sem_undo *un;
    struct sem_undo *next;
    struct sem_array *sma;
    int i;

    if (!task) {
        return;
    }

    spin_lock(&sem_undo_global.lock);

    /* Process all undo structures for this task */
    list_for_each_entry_safe(un, next, &sem_undo_global.undo_list, list_id) {
        sma = sem_find(un->semid);
        if (!sma) {
            continue;
        }

        /* Apply undo adjustments */
        spin_lock(&sma->lock);
        for (i = 0; i < un->nsemadj; i++) {
            if (i < sma->sem_nsems) {
                sma->sem_base[i].semval += un->semadj[i];

                /* Wake up waiting processes */
                /* In real implementation: wake_up_sem_waiters */
            }
        }
        spin_unlock(&sma->lock);

        atomic_dec(&sma->refcount);

        /* Remove from list and free */
        list_del(&un->list_id);
        sem_undo_free(un);
    }

    spin_unlock(&sem_undo_global.lock);
}

/*===========================================================================*/
/*                         SEMAPHORE OPERATIONS                              */
/*===========================================================================*/

/**
 * semop_single - Perform single semaphore operation
 * @sma: Semaphore set
 * @sop: Semaphore operation
 *
 * Returns: 0 on success, negative on error
 */
int semop_single(struct sem_array *sma, struct sembuf *sop)
{
    struct sem *sem;
    s16 new_val;

    if (!sma || !sop) {
        return -EINVAL;
    }

    if (sop->sem_num >= sma->sem_nsems) {
        return -ERANGE;
    }

    sem = &sma->sem_base[sop->sem_num];

    spin_lock(&sma->lock);

    new_val = sem->semval + sop->sem_op;

    /* Check for overflow/underflow */
    if (new_val > SEMVMX || new_val < -SEMVMX) {
        spin_unlock(&sma->lock);
        return -ERANGE;
    }

    /* Check if operation would block */
    if (new_val < 0) {
        if (sop->sem_flg & IPC_NOWAIT) {
            spin_unlock(&sma->lock);
            return -EAGAIN;
        }

        /* In real implementation: add to wait queue */
        spin_unlock(&sma->lock);

        /* Simulated wait */
        while (sem->semval + sop->sem_op < 0) {
            delay_ms(1);
            if (signal_pending_current()) {
                return -EINTR;
            }
        }

        spin_lock(&sma->lock);
        new_val = sem->semval + sop->sem_op;
    }

    /* Apply operation */
    sem->semval = new_val;
    sem->sempid = current ? current->pid : 0;

    /* Handle SEM_UNDO */
    if (sop->sem_flg & SEM_UNDO_FLAG) {
        /* In real implementation: update undo structure */
    }

    /* Update operation time */
    sma->sem_otime = get_time_ms() / 1000;
    sma->sem_otime_pid = current ? current->pid : 0;

    spin_unlock(&sma->lock);

    /* Update global statistics */
    atomic_inc(&sem_global.total_ops);

    pr_debug("Sem: Operation on set %d sem %d: val=%d\n",
             sma->sem_perm.seq, sop->sem_num, new_val);

    return 0;
}

/**
 * semop_multi - Perform multiple semaphore operations
 * @sma: Semaphore set
 * @sops: Array of semaphore operations
 * @nsops: Number of operations
 *
 * Returns: 0 on success, negative on error
 */
int semop_multi(struct sem_array *sma, struct sembuf *sops, size_t nsops)
{
    size_t i;
    int ret;

    if (!sma || !sops || nsops == 0) {
        return -EINVAL;
    }

    if (nsops > SEMOPM) {
        return -E2BIG;
    }

    /* First pass: check all operations */
    for (i = 0; i < nsops; i++) {
        if (sops[i].sem_num >= sma->sem_nsems) {
            return -ERANGE;
        }
    }

    /* Second pass: perform operations atomically */
    spin_lock(&sma->lock);

    for (i = 0; i < nsops; i++) {
        ret = semop_single(sma, &sops[i]);
        if (ret != 0) {
            /* Rollback previous operations */
            size_t j;
            for (j = 0; j < i; j++) {
                sops[j].sem_op = -sops[j].sem_op;
                semop_single(sma, &sops[j]);
            }
            spin_unlock(&sma->lock);
            return ret;
        }
    }

    spin_unlock(&sma->lock);

    return 0;
}

/*===========================================================================*/
/*                         SEMGET SYSTEM CALL                                */
/*===========================================================================*/

/**
 * semget - Create or access semaphore set
 * @key: IPC key
 * @nsems: Number of semaphores
 * @flags: Creation flags and permissions
 *
 * Returns: Semaphore set ID on success, negative on error
 */
int semget(key_t key, int nsems, int flags)
{
    struct sem_array *sma;
    int semid;

    /* Check if set exists */
    if (key != IPC_PRIVATE && !(flags & IPC_CREAT)) {
        sma = sem_hash_find(key);
        if (sma) {
            semid = sma->sem_perm.seq;
            atomic_dec(&sma->refcount);
            return semid;
        }
        return -ENOENT;
    }

    /* Create new set */
    if (flags & IPC_CREAT) {
        sma = sem_alloc(key, nsems, flags);
        if (!sma) {
            return -EEXIST;
        }

        semid = sma->sem_perm.seq;
        atomic_dec(&sma->refcount);
        return semid;
    }

    return -EINVAL;
}

/*===========================================================================*/
/*                         SEMOP SYSTEM CALL                                 */
/*===========================================================================*/

/**
 * semop - Perform semaphore operations
 * @semid: Semaphore set ID
 * @sops: Array of semaphore operations
 * @nsops: Number of operations
 *
 * Returns: 0 on success, negative on error
 */
int semop(int semid, struct sembuf *sops, size_t nsops)
{
    struct sem_array *sma;
    int ret;

    sma = sem_find(semid);
    if (!sma) {
        return -EINVAL;
    }

    /* Check permissions */
    /* In real implementation: check access rights */

    if (nsops == 1) {
        ret = semop_single(sma, sops);
    } else {
        ret = semop_multi(sma, sops, nsops);
    }

    atomic_dec(&sma->refcount);

    return ret;
}

/*===========================================================================*/
/*                         SEMCTL SYSTEM CALL                                */
/*===========================================================================*/

/**
 * semctl - Semaphore control operations
 * @semid: Semaphore set ID
 * @semnum: Semaphore number
 * @cmd: Command
 * @arg: Command argument (union semun)
 *
 * Returns: Result value or negative on error
 */
int semctl(int semid, int semnum, int cmd, ...)
{
    struct sem_array *sma;
    struct semid_ds *buf;
    struct seminfo *seminfo;
    u16 *array;
    int ret = 0;
    int val;
    va_list args;

    sma = sem_find(semid);
    if (!sma) {
        return -EINVAL;
    }

    va_start(args, cmd);

    switch (cmd) {
    case IPC_STAT:
        buf = va_arg(args, struct semid_ds *);
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&sma->lock);
        buf->sem_perm = sma->sem_perm;
        buf->sem_segsz = 0;
        buf->sem_nsems = sma->sem_nsems;
        buf->sem_otime = sma->sem_otime;
        buf->sem_ctime = sma->sem_ctime;
        spin_unlock(&sma->lock);
        break;

    case IPC_SET:
        buf = va_arg(args, struct semid_ds *);
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&sma->lock);
        sma->sem_perm.mode = buf->sem_perm.mode;
        sma->sem_perm.uid = buf->sem_perm.uid;
        sma->sem_perm.gid = buf->sem_perm.gid;
        sma->sem_ctime = get_time_ms() / 1000;
        spin_unlock(&sma->lock);
        break;

    case IPC_RMID:
        spin_lock(&sem_global.lock);
        sem_free(sma);
        spin_unlock(&sem_global.lock);
        sma = NULL; /* Don't decrement refcount */
        break;

    case GETVAL:
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            ret = -ERANGE;
            break;
        }
        spin_lock(&sma->lock);
        val = sma->sem_base[semnum].semval;
        spin_unlock(&sma->lock);
        ret = val;
        break;

    case SETVAL:
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            ret = -ERANGE;
            break;
        }
        val = va_arg(args, int);
        if (val < 0 || val > SEMVMX) {
            ret = -ERANGE;
            break;
        }
        spin_lock(&sma->lock);
        sma->sem_base[semnum].semval = (s16)val;
        sma->sem_ctime = get_time_ms() / 1000;
        spin_unlock(&sma->lock);
        break;

    case GETALL:
        array = va_arg(args, u16 *);
        if (!array) {
            ret = -EINVAL;
            break;
        }
        spin_lock(&sma->lock);
        for (val = 0; val < sma->sem_nsems; val++) {
            array[val] = sma->sem_base[val].semval;
        }
        spin_unlock(&sma->lock);
        break;

    case SETALL:
        array = va_arg(args, u16 *);
        if (!array) {
            ret = -EINVAL;
            break;
        }
        spin_lock(&sma->lock);
        for (val = 0; val < sma->sem_nsems; val++) {
            sma->sem_base[val].semval = array[val];
        }
        sma->sem_ctime = get_time_ms() / 1000;
        spin_unlock(&sma->lock);
        break;

    case GETPID:
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            ret = -ERANGE;
            break;
        }
        spin_lock(&sma->lock);
        val = sma->sem_base[semnum].sempid;
        spin_unlock(&sma->lock);
        ret = val;
        break;

    case GETNCNT:
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            ret = -ERANGE;
            break;
        }
        spin_lock(&sma->lock);
        val = sma->sem_base[semnum].semopm;
        spin_unlock(&sma->lock);
        ret = val;
        break;

    case GETZCNT:
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            ret = -ERANGE;
            break;
        }
        spin_lock(&sma->lock);
        val = (sma->sem_base[semnum].semval == 0) ? 1 : 0;
        spin_unlock(&sma->lock);
        ret = val;
        break;

    case IPC_INFO:
    case SEM_INFO:
        seminfo = va_arg(args, struct seminfo *);
        if (!seminfo) {
            ret = -EINVAL;
            break;
        }

        seminfo->semmap = SEM_HASH_SIZE;
        seminfo->semmni = SEMMNI;
        seminfo->semmns = SEMMNS;
        seminfo->semmnu = SEMMNU;
        seminfo->semmsl = SEMVMX;
        seminfo->semopm = SEMOPM;
        seminfo->semume = 32;
        seminfo->semusz = sizeof(struct sem_undo);
        seminfo->semvmx = SEMVMX;
        seminfo->semaem = SEMAEM;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    va_end(args);

    if (sma) {
        atomic_dec(&sma->refcount);
    }

    return ret;
}

/*===========================================================================*/
/*                         SEMAPHORE STATISTICS                              */
/*===========================================================================*/

/**
 * sem_stats - Print semaphore statistics
 */
void sem_stats(void)
{
    struct sem_array *sma;
    int i;
    int j;
    int active_count = 0;

    printk("\n=== Semaphore Statistics ===\n");

    spin_lock(&sem_global.lock);

    printk("Active Sets: %d\n", atomic_read(&sem_global.sem_count));
    printk("Total Allocated: %d\n", atomic_read(&sem_global.total_allocated));
    printk("Total Freed: %d\n", atomic_read(&sem_global.total_freed));
    printk("Total Operations: %d\n", atomic_read(&sem_global.total_ops));

    printk("\nSet Details:\n");

    for (i = 0; i < SEM_MAX_ID; i++) {
        sma = sem_global.ids[i];
        if (sma) {
            active_count++;

            printk("  Set ID: %d, Key: 0x%x, Semaphores: %d\n",
                   i, sma->sem_perm.key, sma->sem_nsems);

            for (j = 0; j < sma->sem_nsems; j++) {
                printk("    Sem %d: value=%d, pid=%d\n",
                       j, sma->sem_base[j].semval, sma->sem_base[j].sempid);
            }
        }
    }

    printk("\nTotal Active Sets: %d\n", active_count);

    spin_unlock(&sem_global.lock);
}

/*===========================================================================*/
/*                         SEMAPHORE INITIALIZATION                          */
/*===========================================================================*/

/**
 * sem_init - Initialize semaphore subsystem
 */
void sem_init(void)
{
    u32 i;

    pr_info("Initializing Semaphore Subsystem...\n");

    spin_lock_init(&sem_global.lock);
    spin_lock_init(&sem_undo_global.lock);
    INIT_LIST_HEAD(&sem_global.sem_list);
    INIT_LIST_HEAD(&sem_undo_global.undo_list);

    /* Initialize ID table */
    for (i = 0; i < SEM_MAX_ID; i++) {
        sem_global.ids[i] = NULL;
    }

    /* Initialize hash table */
    sem_hash_init();

    /* Initialize counters */
    atomic_set(&sem_global.sem_count, 0);
    atomic_set(&sem_global.total_allocated, 0);
    atomic_set(&sem_global.total_freed, 0);
    atomic_set(&sem_global.total_ops, 0);

    pr_info("  Maximum sets: %d\n", SEM_MAX_ID);
    pr_info("  Maximum semaphores per set: %d\n", SEMVMX);
    pr_info("  Maximum operations per call: %d\n", SEMOPM);
    pr_info("Semaphore Subsystem initialized.\n");
}
