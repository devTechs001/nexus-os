/*
 * NEXUS OS - Inter-Process Communication
 * kernel/ipc/ipc.h
 */

#ifndef _NEXUS_IPC_H
#define _NEXUS_IPC_H

#include "../include/kernel.h"

/*===========================================================================*/
/*                         IPC CONSTANTS                                     */
/*===========================================================================*/

/* IPC Keys */
#define IPC_PRIVATE         0
#define IPC_CREAT           0x00000200
#define IPC_EXCL            0x00000400
#define IPC_NOWAIT          0x00000800

/* IPC Permissions */
#define IPC_R               0x0004
#define IPC_W               0x0002
#define IPC_M               0x0001

/* IPC Control Commands */
#define IPC_RMID            0
#define IPC_SET             1
#define IPC_STAT            2
#define IPC_INFO            3

/* Pipe Constants */
#define PIPE_BUF            4096
#define PIPE_CAPACITY       (16 * PAGE_SIZE)
#define PIPE_MIN_READ       1

/* Shared Memory Constants */
#define SHM_MIN_SIZE        PAGE_SIZE
#define SHM_MAX_SIZE        (256 * MB(1))
#define SHM_ALIGN           PAGE_SIZE

/* Semaphore Constants */
#define SEMVMX              32767       /* Maximum semaphore value */
#define SEMAEM              16384       /* Adjust on exit max value */
#define SEMUSZ              20          /* Semaphore structure size */
#define SEMMNI              128         /* Maximum number of semaphore sets */
#define SEMMNS              256         /* Maximum number of semaphores */
#define SEMMNU              32          /* Maximum number of undo structures */
#define SEMOPM              10          /* Maximum number of operations */

/* Message Queue Constants */
#define MSGMNI              32          /* Maximum number of message queues */
#define MSGMNX              16          /* Maximum messages per queue */
#define MSGMAX              8192        /* Maximum message size */
#define MSGPOOL             (16 * KB(1)) /* Message pool size */

/* Message Flags */
#define MSG_NOERROR         0x1000      /* No error if message too large */
#define MSG_EXCEPT          0x2000      /* Receive all messages except type */

/*===========================================================================*/
/*                         IPC STRUCTURES                                    */
/*===========================================================================*/

/**
 * ipc_perm - IPC permission structure
 */
struct ipc_perm {
    key_t key;              /* Key */
    uid_t uid;              /* Owner UID */
    gid_t gid;              /* Owner GID */
    uid_t cuid;             /* Creator UID */
    gid_t cgid;             /* Creator GID */
    mode_t mode;            /* Permissions */
    u16 seq;                /* Sequence number */
};

/**
 * ipc_ids - IPC ID management
 */
struct ipc_ids {
    int max_id;             /* Maximum ID */
    int cur_id;             /* Current ID count */
    int max_seq;            /* Maximum sequence number */
    spinlock_t lock;        /* Protection lock */
    struct radix_tree_root entries; /* ID radix tree */
};

/**
 * pipe_inode_info - Pipe inode information
 */
struct pipe_inode_info {
    spinlock_t lock;        /* Pipe lock */
    wait_queue_t wait_read; /* Readers wait queue */
    wait_queue_t wait_write; /* Writers wait queue */

    struct page **buffers;  /* Pipe buffers */
    u32 nrbufs;             /* Number of buffers */
    u32 buffersize;         /* Buffer size */

    u32 head;               /* Write position */
    u32 tail;               /* Read position */
    u32 readers;            /* Number of readers */
    u32 writers;            /* Number of writers */
    u32 waiting_writers;    /* Waiting writers count */
    u32 waiting_readers;    /* Waiting readers count */

    u32 usage;              /* Reference count */
    bool orphan;            /* Orphaned pipe */
    bool was_full;          /* Was full flag */
};

/**
 * pipe_buf_operations - Pipe buffer operations
 */
struct pipe_buf_operations {
    int (*confirm)(struct pipe_inode_info *pipe, struct page *page);
    void (*release)(struct pipe_inode_info *pipe, struct page *page);
    void (*steal)(struct pipe_inode_info *pipe, struct page *page);
    void (*get)(struct pipe_inode_info *pipe, struct page *page);
};

/**
 * pipe_buffer - Pipe buffer descriptor
 */
struct pipe_buffer {
    struct page *page;      /* Page containing data */
    u32 offset;             /* Offset in page */
    u32 len;                /* Length of data */
    u32 flags;              /* Buffer flags */
    const struct pipe_buf_operations *ops; /* Operations */
};

/* Pipe Buffer Flags */
#define PIPE_BUF_FLAG_GIFT      0x01    /* Page is a gift */
#define PIPE_BUF_FLAG_CAN_MERGE 0x02    /* Page can be merged */

/**
 * shm_file_data - Shared memory file data
 */
struct shm_file_data {
    struct vm_area_struct *vma; /* VMA pointer */
    struct file *file;          /* File pointer */
    struct address_space *mapping; /* Address space */
    u32 flags;                  /* Flags */
};

/**
 * shmid_kernel - Kernel shared memory ID
 */
struct shmid_kernel {
    struct ipc_perm shm_perm;   /* Permissions */
    size_t shm_segsz;           /* Segment size */
    time_t shm_atime;           /* Last attach time */
    time_t shm_dtime;           /* Last detach time */
    time_t shm_ctime;           /* Last change time */
    pid_t shm_cpid;             /* Creator PID */
    pid_t shm_lpid;             /* Last operation PID */
    u32 shm_nattch;             /* Attach count */
    u32 shm_flags;              /* Flags */

    struct file *file;          /* File pointer */
    void *addr;                 /* Virtual address */
    phys_addr_t paddr;          /* Physical address */

    struct list_head shm_clist; /* Attach list */
    atomic_t refcount;          /* Reference count */
};

/**
 * shmid_ds - Shared memory data structure (user visible)
 */
struct shmid_ds {
    struct ipc_perm shm_perm;   /* Permissions */
    size_t shm_segsz;           /* Segment size */
    time_t shm_atime;           /* Last attach time */
    time_t shm_dtime;           /* Last detach time */
    time_t shm_ctime;           /* Last change time */
    pid_t shm_cpid;             /* Creator PID */
    pid_t shm_lpid;             /* Last operation PID */
    u32 shm_nattch;             /* Attach count */
};

/* Shared Memory Flags */
#define SHM_RDONLY      010000  /* Read-only attach */
#define SHM_RND         020000  /* Round attach address */
#define SHM_REMAP       040000  /* Remap on attach */
#define SHM_EXEC        0100000 /* Execute permission */
#define SHM_LOCK        11      /* Lock segment */
#define SHM_UNLOCK      12      /* Unlock segment */
#define SHM_HUGETLB     04000   /* Segment uses huge pages */
#define SHM_NORESERVE   010000  /* No reserve check */
#define SHM_DEST        01000   /* Destroy on detach */
#define SHM_LOCKED      02000   /* Segment is locked */

/**
 * sem - Individual semaphore
 */
struct sem {
    s16 semval;             /* Semaphore value */
    s16 semadj;             /* Adjust on exit */
    u16 semopm;             /* Pending operations */
    pid_t sempid;           /* PID of last operation */

    struct list_head pending_alter; /* Pending alter operations */
    struct list_head pending_const; /* Pending constant operations */

    struct list_head list;  /* List entry */
};

/**
 * sem_undo - Semaphore undo structure
 */
struct sem_undo {
    struct sem_undo *next;  /* Next in process list */
    struct sem_undo *prev;  /* Previous in process list */
    struct sem_undo *next_id; /* Next in semaphore set list */
    struct sem_undo *prev_id; /* Previous in semaphore set list */

    int semid;              /* Semaphore set ID */
    s16 *semadj;            /* Adjustments */
    u16 nsemadj;            /* Number of adjustments */
};

/**
 * sem_array - Semaphore array (set)
 */
struct sem_array {
    struct ipc_perm sem_perm; /* Permissions */
    struct sem *sem_base;     /* Semaphore array */
    u16 sem_nsems;            /* Number of semaphores */
    time_t sem_otime;         /* Last operation time */
    pid_t sem_otime_pid;      /* PID of last operation */
    time_t sem_ctime;         /* Last change time */

    struct list_head pending_alter; /* Pending alter operations */
    struct list_head undo_list;   /* Undo list */

    spinlock_t lock;        /* Protection lock */
    atomic_t refcount;      /* Reference count */
    atomic_t unused;        /* Unused count */
};

/**
 * sembuf - Semaphore operation buffer
 */
struct sembuf {
    u16 sem_num;            /* Semaphore number */
    s16 sem_op;             /* Semaphore operation */
    s16 sem_flg;            /* Operation flags */
};

/* Semaphore Operation Flags */
#define SEM_UNDO        0x1000  /* Set undo attribute */
#define SEM_NOP         0x0000  /* No operation */

/**
 * seminfo - Semaphore information
 */
struct seminfo {
    int semmap;           /* Number of semaphore map entries */
    int semmni;           /* Maximum number of semaphore sets */
    int semmns;           /* Maximum number of semaphores */
    int semmnu;           /* Maximum number of undo structures */
    int semmsl;           /* Maximum semaphores per set */
    int semopm;           /* Maximum operations per semop call */
    int semume;           /* Maximum undo entries per process */
    int semusz;           /* Size of undo structure */
    int semvmx;           /* Maximum semaphore value */
    int semaem;           /* Adjust on exit max value */
};

/**
 * msg - Message structure
 */
struct msg {
    struct list_head list;  /* Message list entry */
    long mtype;             /* Message type */
    size_t msize;           /* Message size */
    time_t mtime;           /* Send time */

    char mtext[];           /* Message text (flexible array) */
};

/**
 * msqid_kernel - Kernel message queue ID
 */
struct msqid_kernel {
    struct ipc_perm msg_perm; /* Permissions */
    struct msg *msg_first;    /* First message */
    struct msg *msg_last;     /* Last message */
    size_t msg_cbytes;        /* Current bytes in queue */
    size_t msg_qnum;          /* Number of messages */
    size_t msg_qbytes;        /* Maximum bytes in queue */
    pid_t msg_lspid;          /* PID of last sender */
    pid_t msg_lrpid;          /* PID of last receiver */
    time_t msg_stime;         /* Last send time */
    time_t msg_rtime;         /* Last receive time */
    time_t msg_ctime;         /* Last change time */

    spinlock_t lock;        /* Protection lock */
    wait_queue_t wait_send; /* Senders wait queue */
    wait_queue_t wait_recv; /* Receivers wait queue */

    atomic_t refcount;      /* Reference count */
};

/**
 * msqid_ds - Message queue data structure (user visible)
 */
struct msqid_ds {
    struct ipc_perm msg_perm; /* Permissions */
    size_t msg_qbytes;        /* Maximum bytes in queue */
    time_t msg_stime;         /* Last send time */
    time_t msg_rtime;         /* Last receive time */
    time_t msg_ctime;         /* Last change time */
    pid_t msg_lspid;          /* PID of last sender */
    pid_t msg_lrpid;          /* PID of last receiver */
    size_t msg_cbytes;        /* Current bytes in queue */
    size_t msg_qnum;          /* Number of messages */
};

/**
 * msgbuf - Message buffer (user visible)
 */
struct msgbuf {
    long mtype;             /* Message type */
    char mtext[];           /* Message text */
};

/*===========================================================================*/
/*                         IPC API                                           */
/*===========================================================================*/

/* IPC Initialization */
void ipc_init(void);

/*===========================================================================*/
/*                         PIPE API                                          */
/*===========================================================================*/

/* Pipe Creation */
struct pipe_inode_info *pipe_create(void);
void pipe_destroy(struct pipe_inode_info *pipe);

/* Pipe Operations */
ssize_t pipe_read(struct pipe_inode_info *pipe, char *buf, size_t count);
ssize_t pipe_write(struct pipe_inode_info *pipe, const char *buf, size_t count);

/* Pipe Control */
int pipe_ioctl(struct pipe_inode_info *pipe, int cmd, unsigned long arg);
int pipe_fcntl(struct pipe_inode_info *pipe, int cmd, unsigned long arg);

/* Pipe Reference */
void pipe_get(struct pipe_inode_info *pipe);
void pipe_put(struct pipe_inode_info *pipe);

/* Named Pipes (FIFO) */
struct pipe_inode_info *fifo_open(const char *path, int flags);
int fifo_close(struct pipe_inode_info *pipe);
int mkfifo(const char *path, mode_t mode);

/*===========================================================================*/
/*                         SHARED MEMORY API                                 */
/*===========================================================================*/

/* Shared Memory Creation */
int shmget(key_t key, size_t size, int flags);
void *shmat(int shmid, const void *addr, int flags);
int shmdt(const void *addr);
int shmctl(int shmid, int cmd, struct shmid_ds *buf);

/* Shared Memory Internal */
struct shmid_kernel *shm_find(int shmid);
struct shmid_kernel *shm_alloc(key_t key, size_t size, int flags);
void shm_free(struct shmid_kernel *shp);

/* Shared Memory Mapping */
int shm_mmap(struct file *file, struct vm_area_struct *vma);
int shm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

/*===========================================================================*/
/*                         SEMAPHORE API                                     */
/*===========================================================================*/

/* Semaphore Creation */
int semget(key_t key, int nsems, int flags);
int semop(int semid, struct sembuf *sops, size_t nsops);
int semctl(int semid, int semnum, int cmd, ...);

/* Semaphore Internal */
struct sem_array *sem_find(int semid);
struct sem_array *sem_alloc(key_t key, int nsems, int flags);
void sem_free(struct sem_array *sma);

/* Semaphore Operations */
int semop_single(struct sem_array *sma, struct sembuf *sop);
int semop_multi(struct sem_array *sma, struct sembuf *sops, size_t nsops);

/* Semaphore Undo */
struct sem_undo *sem_undo_alloc(struct sem_array *sma, int semnum);
void sem_undo_free(struct sem_undo *un);
void sem_undo_exit(struct task_struct *task);

/*===========================================================================*/
/*                         MESSAGE QUEUE API                                 */
/*===========================================================================*/

/* Message Queue Creation */
int msgget(key_t key, int flags);
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
int msgctl(int msqid, int cmd, struct msqid_ds *buf);

/* Message Queue Internal */
struct msqid_kernel *msgq_find(int msqid);
struct msqid_kernel *msgq_alloc(key_t key, int flags);
void msgq_free(struct msqid_kernel *msq);

/* Message Operations */
int msg_send(struct msqid_kernel *msq, long mtype, const void *msg, size_t size);
int msg_recv(struct msqid_kernel *msq, long mtype, void *msg, size_t size);
struct msg *msg_alloc(size_t size);
void msg_free(struct msg *msg);

/*===========================================================================*/
/*                         IPC STATISTICS                                    */
/*===========================================================================*/

void ipc_stats(void);
void pipe_stats(struct pipe_inode_info *pipe);
void shm_stats(void);
void sem_stats(void);
void msg_stats(void);

#endif /* _NEXUS_IPC_H */
