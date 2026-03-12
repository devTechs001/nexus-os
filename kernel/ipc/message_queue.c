/*
 * NEXUS OS - POSIX Message Queue Implementation
 * kernel/ipc/message_queue.c
 */

#include "ipc.h"

/*===========================================================================*/
/*                         MESSAGE QUEUE CONFIGURATION                       */
/*===========================================================================*/

#define MSGQ_MAGIC          0xMSG00001
#define MSGQ_HASH_SIZE      64
#define MSGQ_MIN_ID         0
#define MSGQ_MAX_ID         32767

/* Message Queue Flags (internal) */
#define MSGQ_FLAG_ALLOCATED 0x00000001
#define MSGQ_FLAG_DEST      0x00000002
#define MSGQ_FLAG_FULL      0x00000004

/*===========================================================================*/
/*                         MESSAGE QUEUE GLOBAL DATA                         */
/*===========================================================================*/

static struct {
    spinlock_t lock;                  /* Global message queue lock */
    struct msqid_kernel *ids[MSGQ_MAX_ID]; /* ID table */
    atomic_t msgq_count;              /* Active message queues */
    atomic_t total_allocated;         /* Total allocations */
    atomic_t total_freed;             /* Total frees */
    atomic_t total_sent;              /* Total messages sent */
    atomic_t total_received;          /* Total messages received */
    size_t total_messages;            /* Total messages in all queues */
    struct list_head msgq_list;       /* List of all queues */
} msgq_global = {
    .lock = { .lock = 0 },
    .msgq_count = ATOMIC_INIT(0),
    .total_allocated = ATOMIC_INIT(0),
    .total_freed = ATOMIC_INIT(0),
    .total_sent = ATOMIC_INIT(0),
    .total_received = ATOMIC_INIT(0),
    .total_messages = 0,
};

/* Key to ID hash table */
static struct list_head msgq_key_hash[MSGQ_HASH_SIZE];

/*===========================================================================*/
/*                         HASH TABLE OPERATIONS                             */
/*===========================================================================*/

/**
 * msgq_hash_key - Hash a key to bucket index
 * @key: IPC key to hash
 *
 * Returns: Hash bucket index
 */
static inline u32 msgq_hash_key(key_t key)
{
    return (u32)key % MSGQ_HASH_SIZE;
}

/**
 * msgq_hash_init - Initialize hash table
 */
static void msgq_hash_init(void)
{
    u32 i;

    for (i = 0; i < MSGQ_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&msgq_key_hash[i]);
    }
}

/**
 * msgq_hash_insert - Insert message queue into hash table
 * @msq: Message queue to insert
 */
static void msgq_hash_insert(struct msqid_kernel *msq)
{
    u32 hash;

    if (!msq) {
        return;
    }

    hash = msgq_hash_key(msq->msg_perm.key);
    list_add(&msq->msg_perm.key, &msgq_key_hash[hash]);
}

/**
 * msgq_hash_remove - Remove message queue from hash table
 * @msq: Message queue to remove
 */
static void msgq_hash_remove(struct msqid_kernel *msq)
{
    if (!msq) {
        return;
    }

    list_del(&msq->msg_perm.key);
    INIT_LIST_HEAD(&msq->msg_perm.key);
}

/**
 * msgq_hash_find - Find message queue by key
 * @key: IPC key to find
 *
 * Returns: Pointer to message queue or NULL
 */
static struct msqid_kernel *msgq_hash_find(key_t key)
{
    u32 hash;
    struct msqid_kernel *msq;

    if (key == IPC_PRIVATE) {
        return NULL;
    }

    hash = msgq_hash_key(key);

    list_for_each_entry(msq, &msgq_key_hash[hash], msg_perm.key) {
        if (msq->msg_perm.key == key) {
            return msq;
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         MESSAGE QUEUE ID MANAGEMENT                       */
/*===========================================================================*/

/**
 * msgq_alloc_id - Allocate a new message queue ID
 *
 * Returns: ID on success, -1 on failure
 */
static int msgq_alloc_id(void)
{
    static int next_id = MSGQ_MIN_ID;
    int id;
    int count = 0;

    while (count < MSGQ_MAX_ID) {
        id = next_id;
        next_id = (next_id + 1) % MSGQ_MAX_ID;

        if (!msgq_global.ids[id]) {
            return id;
        }

        count++;
    }

    return -1;
}

/**
 * msgq_free_id - Free a message queue ID
 * @id: ID to free
 */
static void msgq_free_id(int id)
{
    if (id >= 0 && id < MSGQ_MAX_ID) {
        msgq_global.ids[id] = NULL;
    }
}

/*===========================================================================*/
/*                         MESSAGE ALLOCATION                                */
/*===========================================================================*/

/**
 * msg_alloc - Allocate a new message
 * @size: Message body size
 *
 * Returns: Pointer to message or NULL
 */
struct msg *msg_alloc(size_t size)
{
    struct msg *msg;
    size_t total_size;

    if (size > MSGMAX) {
        return NULL;
    }

    total_size = sizeof(struct msg) + size;
    msg = kzalloc(total_size);
    if (!msg) {
        return NULL;
    }

    msg->msize = size;
    msg->mtime = get_time_ms() / 1000;
    INIT_LIST_HEAD(&msg->list);

    return msg;
}

/**
 * msg_free - Free a message
 * @msg: Message to free
 */
void msg_free(struct msg *msg)
{
    if (!msg) {
        return;
    }

    kfree(msg);
}

/*===========================================================================*/
/*                         MESSAGE QUEUE ALLOCATION                          */
/*===========================================================================*/

/**
 * msgq_alloc - Allocate a new message queue
 * @key: IPC key
 * @flags: Creation flags
 *
 * Returns: Pointer to msqid_kernel on success, NULL on failure
 */
struct msqid_kernel *msgq_alloc(key_t key, int flags)
{
    struct msqid_kernel *msq;
    struct msqid_kernel *existing;
    int id;

    /* Check for existing queue with same key */
    if (key != IPC_PRIVATE) {
        existing = msgq_hash_find(key);
        if (existing) {
            if (flags & IPC_EXCL) {
                return NULL;
            }

            /* Return existing queue */
            atomic_inc(&existing->refcount);
            return existing;
        }
    }

    /* Allocate ID */
    id = msgq_alloc_id();
    if (id < 0) {
        pr_err("MsgQ: No free IDs available\n");
        return NULL;
    }

    /* Allocate message queue structure */
    msq = kzalloc(sizeof(struct msqid_kernel));
    if (!msq) {
        msgq_free_id(id);
        return NULL;
    }

    /* Initialize message queue */
    msq->msg_perm.key = key;
    msq->msg_perm.uid = 0;
    msq->msg_perm.gid = 0;
    msq->msg_perm.cuid = 0;
    msq->msg_perm.cgid = 0;
    msq->msg_perm.mode = (flags & 0x1FF);
    msq->msg_perm.seq = (u16)id;

    msq->msg_first = NULL;
    msq->msg_last = NULL;
    msq->msg_cbytes = 0;
    msq->msg_qnum = 0;
    msq->msg_qbytes = MSGPOOL;
    msq->msg_lspid = 0;
    msq->msg_lrpid = 0;
    msq->msg_stime = 0;
    msq->msg_rtime = 0;
    msq->msg_ctime = get_time_ms() / 1000;

    spin_lock_init(&msq->lock);
    /* In real implementation: init_waitqueue_head */
    atomic_set(&msq->refcount, 1);

    /* Insert into ID table and hash */
    msgq_global.ids[id] = msq;
    if (key != IPC_PRIVATE) {
        msgq_hash_insert(msq);
    }

    /* Update global statistics */
    spin_lock(&msgq_global.lock);
    atomic_inc(&msgq_global.msgq_count);
    atomic_inc(&msgq_global.total_allocated);
    list_add(&msq->msg_perm.key, &msgq_global.msgq_list);
    spin_unlock(&msgq_global.lock);

    pr_debug("MsgQ: Allocated queue id=%d key=%x\n", id, key);

    return msq;
}

/**
 * msgq_free - Free a message queue
 * @msq: Message queue to free
 */
void msgq_free(struct msqid_kernel *msq)
{
    struct msg *msg;
    struct msg *next;
    int id;

    if (!msq) {
        return;
    }

    pr_debug("MsgQ: Freeing queue id=%d key=%x\n",
             msq->msg_perm.seq, msq->msg_perm.key);

    /* Free all messages */
    spin_lock(&msq->lock);
    msg = msq->msg_first;
    while (msg) {
        next = list_entry(msg->list.next, struct msg, list);
        msg_free(msg);
        msg = next;
    }
    msq->msg_first = NULL;
    msq->msg_last = NULL;
    msq->msg_cbytes = 0;
    msq->msg_qnum = 0;
    spin_unlock(&msq->lock);

    /* Remove from hash table */
    if (msq->msg_perm.key != IPC_PRIVATE) {
        msgq_hash_remove(msq);
    }

    /* Remove from ID table */
    id = msq->msg_perm.seq;
    if (id >= 0 && id < MSGQ_MAX_ID) {
        msgq_global.ids[id] = NULL;
    }

    /* Update global statistics */
    spin_lock(&msgq_global.lock);
    atomic_dec(&msgq_global.msgq_count);
    atomic_inc(&msgq_global.total_freed);
    list_del(&msq->msg_perm.key);
    spin_unlock(&msgq_global.lock);

    /* Free queue structure */
    kfree(msq);
}

/**
 * msgq_find - Find a message queue by ID
 * @msqid: Message queue ID
 *
 * Returns: Pointer to message queue or NULL
 */
struct msqid_kernel *msgq_find(int msqid)
{
    struct msqid_kernel *msq;

    if (msqid < 0 || msqid >= MSGQ_MAX_ID) {
        return NULL;
    }

    spin_lock(&msgq_global.lock);
    msq = msgq_global.ids[msqid];
    if (msq) {
        atomic_inc(&msq->refcount);
    }
    spin_unlock(&msgq_global.lock);

    return msq;
}

/*===========================================================================*/
/*                         MESSAGE SEND OPERATIONS                           */
/*===========================================================================*/

/**
 * msg_send - Send a message to queue
 * @msq: Message queue
 * @mtype: Message type
 * @msg: Message body
 * @size: Message size
 *
 * Returns: 0 on success, negative on error
 */
int msg_send(struct msqid_kernel *msq, long mtype, const void *msg, size_t size)
{
    struct msg *new_msg;
    struct msg *insert_pos;
    bool inserted = false;

    if (!msq || !msg || size == 0) {
        return -EINVAL;
    }

    if (mtype <= 0) {
        return -EINVAL;
    }

    if (size > MSGMAX) {
        return -E2BIG;
    }

    spin_lock(&msq->lock);

    /* Check queue capacity */
    if (msq->msg_cbytes + size > msq->msg_qbytes) {
        spin_unlock(&msq->lock);
        return -EAGAIN;
    }

    /* Allocate new message */
    new_msg = msg_alloc(size);
    if (!new_msg) {
        spin_unlock(&msq->lock);
        return -ENOMEM;
    }

    new_msg->mtype = mtype;
    memcpy(new_msg->mtext, msg, size);

    /* Insert message in type order */
    if (!msq->msg_first) {
        /* Empty queue */
        msq->msg_first = new_msg;
        msq->msg_last = new_msg;
        inserted = true;
    } else {
        /* Find insertion point */
        insert_pos = msq->msg_first;
        while (insert_pos) {
            if (insert_pos->mtype >= mtype) {
                /* Insert before this message */
                if (insert_pos == msq->msg_first) {
                    new_msg->list.next = &insert_pos->list;
                    new_msg->list.prev = insert_pos->list.prev;
                    msq->msg_first = new_msg;
                } else {
                    new_msg->list.next = &insert_pos->list;
                    new_msg->list.prev = insert_pos->list.prev;
                }
                inserted = true;
                break;
            }
            if (!insert_pos->list.next ||
                insert_pos->list.next == &msq->msg_first->list) {
                /* Insert at end */
                new_msg->list.next = &msq->msg_first->list;
                new_msg->list.prev = msq->msg_last->list.prev;
                msq->msg_last = new_msg;
                inserted = true;
                break;
            }
            insert_pos = list_entry(insert_pos->list.next, struct msg, list);
        }

        if (!inserted) {
            /* Append to end */
            new_msg->list.next = &msq->msg_first->list;
            new_msg->list.prev = msq->msg_last->list.prev;
            msq->msg_last = new_msg;
        }
    }

    /* Update queue statistics */
    msq->msg_cbytes += size;
    msq->msg_qnum++;
    msq->msg_stime = get_time_ms() / 1000;
    msq->msg_lspid = current_process() ? current_process()->pid : 0;

    spin_unlock(&msq->lock);

    /* Update global statistics */
    atomic_inc(&msgq_global.total_sent);
    atomic_inc(&msgq_global.total_messages);

    /* Wake up waiting receivers */
    /* In real implementation: wake_up_interruptible */

    pr_debug("MsgQ: Sent message type=%ld size=%zu to queue %d\n",
             mtype, size, msq->msg_perm.seq);

    return 0;
}

/**
 * msg_recv - Receive a message from queue
 * @msq: Message queue
 * @mtype: Message type to receive (0 for any)
 * @msg: Buffer for message body
 * @size: Buffer size
 *
 * Returns: Message size on success, negative on error
 */
int msg_recv(struct msqid_kernel *msq, long mtype, void *msg, size_t size)
{
    struct msg *found_msg = NULL;
    struct msg *msg_iter;
    int msg_size;

    if (!msq || !msg || size == 0) {
        return -EINVAL;
    }

    spin_lock(&msq->lock);

    /* Check for messages */
    if (!msq->msg_first || msq->msg_qnum == 0) {
        spin_unlock(&msq->lock);
        return -ENOMSG;
    }

    /* Find matching message */
    msg_iter = msq->msg_first;
    do {
        if (mtype == 0 || msg_iter->mtype == mtype) {
            found_msg = msg_iter;
            break;
        }
        if (msg_iter->list.next == &msq->msg_first->list) {
            break;
        }
        msg_iter = list_entry(msg_iter->list.next, struct msg, list);
    } while (msg_iter != msq->msg_first);

    if (!found_msg) {
        spin_unlock(&msq->lock);
        return -ENOMSG;
    }

    /* Check buffer size */
    if (size < found_msg->msize) {
        spin_unlock(&msq->lock);
        return -E2BIG;
    }

    /* Copy message */
    msg_size = found_msg->msize;
    memcpy(msg, found_msg->mtext, msg_size);

    /* Remove message from queue */
    if (found_msg == msq->msg_first) {
        if (found_msg == msq->msg_last) {
            msq->msg_first = NULL;
            msq->msg_last = NULL;
        } else {
            msq->msg_first = list_entry(found_msg->list.next, struct msg, list);
        }
    } else if (found_msg == msq->msg_last) {
        msq->msg_last = list_entry(found_msg->list.prev, struct msg, list);
    }

    msq->msg_cbytes -= found_msg->msize;
    msq->msg_qnum--;
    msq->msg_rtime = get_time_ms() / 1000;
    msq->msg_lrpid = current_process() ? current_process()->pid : 0;

    spin_unlock(&msq->lock);

    /* Free message */
    msg_free(found_msg);

    /* Update global statistics */
    atomic_inc(&msgq_global.total_received);
    atomic_dec(&msgq_global.total_messages);

    /* Wake up waiting senders */
    /* In real implementation: wake_up_interruptible */

    pr_debug("MsgQ: Received message type=%ld size=%d from queue %d\n",
             found_msg->mtype, msg_size, msq->msg_perm.seq);

    return msg_size;
}

/*===========================================================================*/
/*                         MSGGET SYSTEM CALL                                */
/*===========================================================================*/

/**
 * msgget - Create or access message queue
 * @key: IPC key
 * @flags: Creation flags and permissions
 *
 * Returns: Queue ID on success, negative on error
 */
int msgget(key_t key, int flags)
{
    struct msqid_kernel *msq;
    int msqid;

    /* Check if queue exists */
    if (key != IPC_PRIVATE && !(flags & IPC_CREAT)) {
        msq = msgq_hash_find(key);
        if (msq) {
            msqid = msq->msg_perm.seq;
            atomic_dec(&msq->refcount);
            return msqid;
        }
        return -ENOENT;
    }

    /* Create new queue */
    if (flags & IPC_CREAT) {
        msq = msgq_alloc(key, flags);
        if (!msq) {
            return -EEXIST;
        }

        msqid = msq->msg_perm.seq;
        atomic_dec(&msq->refcount);
        return msqid;
    }

    return -EINVAL;
}

/*===========================================================================*/
/*                         MSGSND SYSTEM CALL                                */
/*===========================================================================*/

/**
 * msgsnd - Send a message
 * @msqid: Message queue ID
 * @msgp: Message buffer
 * @msgsz: Message size
 * @msgflg: Flags
 *
 * Returns: 0 on success, negative on error
 */
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    struct msqid_kernel *msq;
    struct msgbuf *mbuf;
    int ret;

    if (!msgp || msgsz == 0) {
        return -EINVAL;
    }

    msq = msgq_find(msqid);
    if (!msq) {
        return -EINVAL;
    }

    /* Check permissions */
    /* In real implementation: check access rights */

    mbuf = (struct msgbuf *)msgp;

    /* Handle blocking */
    if (msgflg & IPC_NOWAIT) {
        ret = msg_send(msq, mbuf->mtype, mbuf->mtext, msgsz);
    } else {
        /* In real implementation: wait for space */
        ret = msg_send(msq, mbuf->mtype, mbuf->mtext, msgsz);
        while (ret == -EAGAIN) {
            if (signal_pending()) {
                atomic_dec(&msq->refcount);
                return -EINTR;
            }
            delay_ms(1);
            ret = msg_send(msq, mbuf->mtype, mbuf->mtext, msgsz);
        }
    }

    atomic_dec(&msq->refcount);

    return ret;
}

/*===========================================================================*/
/*                         MSGRCV SYSTEM CALL                                */
/*===========================================================================*/

/**
 * msgrcv - Receive a message
 * @msqid: Message queue ID
 * @msgp: Message buffer
 * @msgsz: Buffer size
 * @msgtyp: Message type (0 for any)
 * @msgflg: Flags
 *
 * Returns: Message size on success, negative on error
 */
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    struct msqid_kernel *msq;
    struct msgbuf *mbuf;
    int ret;

    if (!msgp || msgsz == 0) {
        return -EINVAL;
    }

    msq = msgq_find(msqid);
    if (!msq) {
        return -EINVAL;
    }

    /* Check permissions */
    /* In real implementation: check access rights */

    mbuf = (struct msgbuf *)msgp;

    /* Handle blocking */
    if (msgflg & IPC_NOWAIT) {
        ret = msg_recv(msq, msgtyp, mbuf->mtext, msgsz);
    } else {
        /* In real implementation: wait for message */
        ret = msg_recv(msq, msgtyp, mbuf->mtext, msgsz);
        while (ret == -ENOMSG) {
            if (signal_pending()) {
                atomic_dec(&msq->refcount);
                return -EINTR;
            }
            delay_ms(1);
            ret = msg_recv(msq, msgtyp, mbuf->mtext, msgsz);
        }
    }

    if (ret >= 0) {
        mbuf->mtype = msgtyp;
    }

    atomic_dec(&msq->refcount);

    return ret;
}

/*===========================================================================*/
/*                         MSGCTL SYSTEM CALL                                */
/*===========================================================================*/

/**
 * msgctl - Message queue control
 * @msqid: Message queue ID
 * @cmd: Command
 * @buf: Buffer for data
 *
 * Returns: 0 on success, negative on error
 */
int msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    struct msqid_kernel *msq;
    int ret = 0;

    msq = msgq_find(msqid);
    if (!msq) {
        return -EINVAL;
    }

    switch (cmd) {
    case IPC_STAT:
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&msq->lock);
        buf->msg_perm = msq->msg_perm;
        buf->msg_qbytes = msq->msg_qbytes;
        buf->msg_stime = msq->msg_stime;
        buf->msg_rtime = msq->msg_rtime;
        buf->msg_ctime = msq->msg_ctime;
        buf->msg_lspid = msq->msg_lspid;
        buf->msg_lrpid = msq->msg_lrpid;
        buf->msg_cbytes = msq->msg_cbytes;
        buf->msg_qnum = msq->msg_qnum;
        spin_unlock(&msq->lock);
        break;

    case IPC_SET:
        if (!buf) {
            ret = -EINVAL;
            break;
        }

        spin_lock(&msq->lock);
        msq->msg_perm.mode = buf->msg_perm.mode;
        msq->msg_perm.uid = buf->msg_perm.uid;
        msq->msg_perm.gid = buf->msg_perm.gid;
        msq->msg_qbytes = buf->msg_qbytes;
        msq->msg_ctime = get_time_ms() / 1000;
        spin_unlock(&msq->lock);
        break;

    case IPC_RMID:
        spin_lock(&msgq_global.lock);
        msgq_free(msq);
        spin_unlock(&msgq_global.lock);
        msq = NULL; /* Don't decrement refcount */
        break;

    default:
        ret = -EINVAL;
        break;
    }

    if (msq) {
        atomic_dec(&msq->refcount);
    }

    return ret;
}

/*===========================================================================*/
/*                         MESSAGE QUEUE STATISTICS                          */
/*===========================================================================*/

/**
 * msg_stats - Print message queue statistics
 */
void msg_stats(void)
{
    struct msqid_kernel *msq;
    int i;
    int active_count = 0;
    size_t total_bytes = 0;

    printk("\n=== Message Queue Statistics ===\n");

    spin_lock(&msgq_global.lock);

    printk("Active Queues: %d\n", atomic_read(&msgq_global.msgq_count));
    printk("Total Allocated: %d\n", atomic_read(&msgq_global.total_allocated));
    printk("Total Freed: %d\n", atomic_read(&msgq_global.total_freed));
    printk("Total Messages Sent: %d\n", atomic_read(&msgq_global.total_sent));
    printk("Total Messages Received: %d\n", atomic_read(&msgq_global.total_received));
    printk("Current Messages: %zu\n", msgq_global.total_messages);

    printk("\nQueue Details:\n");

    for (i = 0; i < MSGQ_MAX_ID; i++) {
        msq = msgq_global.ids[i];
        if (msq) {
            active_count++;
            total_bytes += msq->msg_cbytes;

            printk("  Queue ID: %d, Key: 0x%x\n", i, msq->msg_perm.key);
            printk("    Messages: %zu, Bytes: %zu/%zu\n",
                   msq->msg_qnum, msq->msg_cbytes, msq->msg_qbytes);
            printk("    Last Sender PID: %d, Last Receiver PID: %d\n",
                   msq->msg_lspid, msq->msg_lrpid);
        }
    }

    printk("\nTotal Active: %d queues, %zu bytes\n", active_count, total_bytes);

    spin_unlock(&msgq_global.lock);
}

/*===========================================================================*/
/*                         MESSAGE QUEUE INITIALIZATION                      */
/*===========================================================================*/

/**
 * msgq_init - Initialize message queue subsystem
 */
void msgq_init(void)
{
    u32 i;

    pr_info("Initializing Message Queue Subsystem...\n");

    spin_lock_init(&msgq_global.lock);
    INIT_LIST_HEAD(&msgq_global.msgq_list);

    /* Initialize ID table */
    for (i = 0; i < MSGQ_MAX_ID; i++) {
        msgq_global.ids[i] = NULL;
    }

    /* Initialize hash table */
    msgq_hash_init();

    /* Initialize counters */
    atomic_set(&msgq_global.msgq_count, 0);
    atomic_set(&msgq_global.total_allocated, 0);
    atomic_set(&msgq_global.total_freed, 0);
    atomic_set(&msgq_global.total_sent, 0);
    atomic_set(&msgq_global.total_received, 0);
    msgq_global.total_messages = 0;

    pr_info("  Maximum queues: %d\n", MSGQ_MAX_ID);
    pr_info("  Maximum message size: %d bytes\n", MSGMAX);
    pr_info("  Default queue capacity: %d bytes\n", MSGPOOL);
    pr_info("Message Queue Subsystem initialized.\n");
}
