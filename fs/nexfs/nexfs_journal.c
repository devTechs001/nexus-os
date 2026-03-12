/*
 * NEXUS OS - NexFS Filesystem
 * fs/nexfs/nexfs_journal.c
 *
 * NexFS Journaling - Transaction logging and recovery
 */

#include "nexfs.h"

/*===========================================================================*/
/*                         JOURNAL CONFIGURATION                             */
/*===========================================================================*/

#define JOURNAL_MIN_TRANSACTIONS    4
#define JOURNAL_MAX_TRANSACTIONS    1024
#define JOURNAL_FLUSH_INTERVAL      5000    /* 5 seconds */

/* Journal cache */
static void *journal_cache = NULL;
static void *transaction_cache = NULL;

/*===========================================================================*/
/*                         JOURNAL ALLOCATION                                */
/*===========================================================================*/

/**
 * nexfs_journal_alloc - Allocate journal structure
 *
 * Returns: New journal or NULL on failure
 */
static struct nexfs_journal *nexfs_journal_alloc(void)
{
    struct nexfs_journal *journal;

    if (!journal_cache) {
        journal_cache = kmem_cache_create("nexfs_journal",
                                           sizeof(struct nexfs_journal),
                                           __alignof__(struct nexfs_journal), 0);
        if (!journal_cache) {
            return NULL;
        }
    }

    if (!transaction_cache) {
        transaction_cache = kmem_cache_create("nexfs_transaction",
                                               sizeof(struct nexfs_transaction),
                                               __alignof__(struct nexfs_transaction), 0);
        if (!transaction_cache) {
            kmem_cache_destroy(journal_cache);
            journal_cache = NULL;
            return NULL;
        }
    }

    journal = (struct nexfs_journal *)kmem_cache_alloc(journal_cache);
    if (!journal) {
        return NULL;
    }

    memset(journal, 0, sizeof(struct nexfs_journal));

    journal->j_magic = NEXFS_JOURNAL_MAGIC;
    journal->j_state = 0;

    journal->j_inode = NULL;
    journal->j_dev = 0;

    journal->j_current_transaction = NULL;
    journal->j_committing_transaction = NULL;
    INIT_LIST_HEAD(&journal->j_completed_transactions);

    journal->j_sequence = 1;
    journal->j_tail_sequence = 0;
    journal->j_head_sequence = 0;

    journal->j_first_block = 0;
    journal->j_total_blocks = 0;
    journal->j_head = 0;
    journal->j_tail = 0;
    journal->j_free = 0;

    journal->j_max_transaction = 8192;
    journal->j_max_trans_data = 4096;

    spin_lock_init(&journal->j_lock);
    spin_lock_init(&journal->j_state_lock);

    journal->j_transactions = 0;
    journal->j_blocks_written = 0;
    journal->j_blocks_revoked = 0;

    journal->j_checksum_seed = 0;

    return journal;
}

/**
 * nexfs_journal_free - Free journal structure
 * @journal: Journal to free
 */
static void nexfs_journal_free(struct nexfs_journal *journal)
{
    if (!journal) {
        return;
    }

    journal->j_magic = 0;

    kmem_cache_free(journal_cache, journal);
}

/*===========================================================================*/
/*                         TRANSACTION ALLOCATION                            */
/*===========================================================================*/

/**
 * nexfs_transaction_alloc - Allocate transaction structure
 * @journal: Parent journal
 *
 * Returns: New transaction or NULL on failure
 */
static struct nexfs_transaction *nexfs_transaction_alloc(struct nexfs_journal *journal)
{
    struct nexfs_transaction *trans;

    if (!transaction_cache) {
        return NULL;
    }

    trans = (struct nexfs_transaction *)kmem_cache_alloc(transaction_cache);
    if (!trans) {
        return NULL;
    }

    memset(trans, 0, sizeof(struct nexfs_transaction));

    trans->t_transaction = journal->j_sequence++;
    trans->t_state = NEXUS_TRANS_RUNNING;

    trans->t_start = get_time_ms();
    trans->t_requested_commit = 0;
    trans->t_committing = 0;
    trans->t_committed = 0;

    INIT_LIST_HEAD(&trans->t_buffers);
    INIT_LIST_HEAD(&trans->t_locked_buffers);
    INIT_LIST_HEAD(&trans->t_meta_data);
    INIT_LIST_HEAD(&trans->t_log_buffers);

    trans->t_nr_buffers = 0;
    trans->t_nr_meta_data = 0;
    trans->t_nr_log_buffers = 0;

    trans->t_journal = journal;
    INIT_LIST_HEAD(&trans->t_list);

    spin_lock_init(&trans->t_lock);

    return trans;
}

/**
 * nexfs_transaction_free - Free transaction structure
 * @trans: Transaction to free
 */
static void nexfs_transaction_free(struct nexfs_transaction *trans)
{
    if (!trans) {
        return;
    }

    kmem_cache_free(transaction_cache, trans);
}

/*===========================================================================*/
/*                         JOURNAL INITIALIZATION                            */
/*===========================================================================*/

/**
 * nexfs_journal_read_superblock - Read journal superblock
 * @journal: Journal structure
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_read_superblock(struct nexfs_journal *journal)
{
    struct nexfs_journal_superblock *jsb;
    u32 checksum;
    s32 ret;

    if (!journal || !journal->j_inode) {
        return VFS_EINVAL;
    }

    jsb = &journal->j_superblock;

    /* TODO: Read journal superblock from disk */
    /* ret = nexfs_read_block(journal->j_inode, 0, jsb, NEXFS_BLOCK_SIZE); */
    ret = VFS_OK;

    if (ret != VFS_OK) {
        return ret;
    }

    /* Validate magic */
    if (jsb->s_magic != NEXFS_JOURNAL_MAGIC) {
        pr_err("NexFS: Invalid journal magic (got 0x%08X)\n", jsb->s_magic);
        return VFS_EINVAL;
    }

    /* Validate checksum */
    checksum = nexfs_crc32c(0, jsb, sizeof(struct nexfs_journal_superblock) - 4);
    if (checksum != 0) {
        /* Checksum validation would go here */
    }

    /* Initialize journal parameters */
    journal->j_first_block = jsb->s_first_block;
    journal->j_total_blocks = jsb->s_total_blocks;
    journal->j_sequence = jsb->s_sequence;
    journal->j_head = jsb->s_start;
    journal->j_max_transaction = jsb->s_max_transaction;
    journal->j_max_trans_data = jsb->s_max_trans_data;

    pr_debug("NexFS: Journal loaded (blocks=%u, sequence=%u)\n",
             journal->j_total_blocks, journal->j_sequence);

    return VFS_OK;
}

/**
 * nexfs_journal_write_superblock - Write journal superblock
 * @journal: Journal structure
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_write_superblock(struct nexfs_journal *journal)
{
    struct nexfs_journal_superblock *jsb;
    s32 ret;

    if (!journal || !journal->j_inode) {
        return VFS_EINVAL;
    }

    jsb = &journal->j_superblock;

    /* Update fields */
    jsb->s_magic = NEXFS_JOURNAL_MAGIC;
    jsb->s_sequence = journal->j_sequence;
    jsb->s_start = journal->j_head;

    /* TODO: Write journal superblock to disk */
    /* ret = nexfs_write_block(journal->j_inode, 0, jsb, NEXFS_BLOCK_SIZE); */
    ret = VFS_OK;

    return ret;
}

/**
 * nexfs_journal_init - Initialize journal
 * @sb: Superblock
 *
 * Sets up the journal for the filesystem.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_init(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    struct nexfs_inode *journal_inode;
    s32 ret;

    if (!sb) {
        return VFS_EINVAL;
    }

    pr_info("NexFS: Initializing journal...\n");

    /* Allocate journal structure */
    journal = nexfs_journal_alloc();
    if (!journal) {
        return VFS_ENOMEM;
    }

    /* Get journal inode */
    if (sb->raw.s_journal_ino) {
        journal_inode = nexfs_iget(sb, sb->raw.s_journal_ino);
    } else {
        /* Create journal inode */
        journal_inode = nexfs_alloc_inode(sb);
        if (!journal_inode) {
            nexfs_journal_free(journal);
            return VFS_ENOMEM;
        }

        journal_inode->raw.i_mode = NEXFS_TYPE_REGULAR;
        journal_inode->raw.i_size = NEXFS_JOURNAL_SIZE;

        sb->raw.s_journal_ino = journal_inode->ino;
    }

    if (!journal_inode) {
        nexfs_journal_free(journal);
        return VFS_EIO;
    }

    journal->j_inode = journal_inode;
    journal->j_state = NEXFS_JOURNAL_RUNNING;

    /* Read journal superblock */
    ret = nexfs_journal_read_superblock(journal);
    if (ret != VFS_OK) {
        pr_err("NexFS: Failed to read journal superblock: %d\n", ret);
        nexfs_iput(journal_inode);
        nexfs_journal_free(journal);
        return ret;
    }

    /* Initialize current transaction */
    journal->j_current_transaction = nexfs_transaction_alloc(journal);
    if (!journal->j_current_transaction) {
        nexfs_iput(journal_inode);
        nexfs_journal_free(journal);
        return VFS_ENOMEM;
    }

    sb->journal = journal;

    pr_info("NexFS: Journal initialized (size=%u blocks)\n", journal->j_total_blocks);

    return VFS_OK;
}

/**
 * nexfs_journal_shutdown - Shutdown journal
 * @sb: Superblock
 *
 * Flushes and closes the journal.
 */
void nexfs_journal_shutdown(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;
    struct nexfs_transaction *next;

    if (!sb || !sb->journal) {
        return;
    }

    journal = sb->journal;

    pr_info("NexFS: Shutting down journal...\n");

    /* Commit current transaction */
    if (journal->j_current_transaction) {
        nexfs_journal_commit(sb);
    }

    /* Wait for committing transaction */
    /* TODO: Wait for j_committing_transaction */

    /* Free completed transactions */
    list_for_each_entry_safe(trans, next, &journal->j_completed_transactions, t_list) {
        list_del(&trans->t_list);
        nexfs_transaction_free(trans);
    }

    /* Free current transaction */
    if (journal->j_current_transaction) {
        nexfs_transaction_free(journal->j_current_transaction);
        journal->j_current_transaction = NULL;
    }

    /* Write journal superblock */
    journal->j_state = NEXFS_JOURNAL_SHUTDOWN;
    nexfs_journal_write_superblock(journal);

    /* Release journal inode */
    if (journal->j_inode) {
        nexfs_iput(journal->j_inode);
        journal->j_inode = NULL;
    }

    sb->journal = NULL;

    nexfs_journal_free(journal);

    pr_info("NexFS: Journal shutdown complete\n");
}

/*===========================================================================*/
/*                         TRANSACTION MANAGEMENT                            */
/*===========================================================================*/

/**
 * nexfs_journal_start - Start a journal transaction
 * @sb: Superblock
 * @blocks: Estimated blocks needed
 *
 * Starts a new journal transaction.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_start(struct nexfs_superblock *sb, u32 blocks)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    if (!(journal->j_state & NEXFS_JOURNAL_RUNNING)) {
        return VFS_EIO;
    }

    spin_lock(&journal->j_lock);

    /* Check if current transaction is full */
    if (journal->j_current_transaction &&
        journal->j_current_transaction->t_nr_buffers >= journal->j_max_transaction) {
        /* Commit current transaction */
        spin_unlock(&journal->j_lock);
        nexfs_journal_commit(sb);
        spin_lock(&journal->j_lock);
    }

    /* Get current transaction */
    trans = journal->j_current_transaction;

    if (!trans) {
        /* Create new transaction */
        trans = nexfs_transaction_alloc(journal);
        if (!trans) {
            spin_unlock(&journal->j_lock);
            return VFS_ENOMEM;
        }
        journal->j_current_transaction = trans;
    }

    atomic_inc(&trans->t_lock.counter);  /* Use as refcount */

    spin_unlock(&journal->j_lock);

    pr_debug("NexFS: Started transaction %u\n", trans->t_transaction);

    return VFS_OK;
}

/**
 * nexfs_journal_stop - Stop a journal transaction
 * @sb: Superblock
 *
 * Completes the current transaction.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_stop(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    trans = journal->j_current_transaction;

    if (trans) {
        atomic_dec(&trans->t_lock.counter);

        if (atomic_read(&trans->t_lock.counter) <= 0) {
            /* Transaction complete */
            trans->t_state = NEXUS_TRANS_LOCKED;
        }
    }

    spin_unlock(&journal->j_lock);

    return VFS_OK;
}

/**
 * nexfs_journal_commit - Commit current transaction
 * @sb: Superblock
 *
 * Commits the current transaction to the journal.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_commit(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;
    struct nexfs_journal_header *header;
    struct nexfs_journal_commit *commit;
    u32 block;
    s32 ret;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    trans = journal->j_current_transaction;

    if (!trans || trans->t_nr_buffers == 0) {
        spin_unlock(&journal->j_lock);
        return VFS_OK;
    }

    /* Mark as committing */
    trans->t_state = NEXUS_TRANS_COMMITTING;
    trans->t_committing = get_time_ms();

    spin_unlock(&journal->j_lock);

    pr_debug("NexFS: Committing transaction %u (%u blocks)\n",
             trans->t_transaction, trans->t_nr_buffers);

    /* Write descriptor block */
    /* TODO: Write descriptor block with block tags */

    /* Write data blocks */
    /* TODO: Write all modified blocks to journal */

    /* Write commit block */
    block = journal->j_head;
    /* commit = (struct nexfs_journal_commit *)buffer; */
    /* commit->c_header.h_magic = NEXFS_JOURNAL_MAGIC; */
    /* commit->c_header.h_blocktype = NEXFS_J_COMMIT_BLOCK; */
    /* commit->c_header.h_sequence = trans->t_transaction; */
    /* commit->c_sequence = trans->t_transaction; */
    /* commit->c_nr_blocks = trans->t_nr_buffers; */

    /* TODO: Write commit block to disk */

    /* Update journal head */
    journal->j_head = (journal->j_head + trans->t_nr_buffers + 1) % journal->j_total_blocks;
    journal->j_head_sequence = trans->t_transaction;

    /* Mark transaction as committed */
    trans->t_state = NEXUS_TRANS_COMMITTED;
    trans->t_committed = get_time_ms();

    journal->j_transactions++;
    journal->j_blocks_written += trans->t_nr_buffers;

    /* Move to completed list */
    spin_lock(&journal->j_lock);

    list_add_tail(&trans->t_list, &journal->j_completed_transactions);

    /* Create new current transaction */
    journal->j_current_transaction = nexfs_transaction_alloc(journal);

    spin_unlock(&journal->j_lock);

    /* Write superblock periodically */
    if (journal->j_transactions % 100 == 0) {
        nexfs_journal_write_superblock(journal);
    }

    pr_debug("NexFS: Transaction %u committed\n", trans->t_transaction);

    return VFS_OK;
}

/*===========================================================================*/
/*                         BUFFER MANAGEMENT                                 */
/*===========================================================================*/

/**
 * nexfs_journal_get_write_access - Get write access to a block
 * @sb: Superblock
 * @block: Block number
 *
 * Prepares a block for modification within a transaction.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_get_write_access(struct nexfs_superblock *sb, u64 block)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    trans = journal->j_current_transaction;

    if (!trans) {
        spin_unlock(&journal->j_lock);
        return VFS_EINVAL;
    }

    /* TODO: Add block to transaction's buffer list */
    /* TODO: Copy original block content for potential rollback */

    trans->t_nr_buffers++;

    spin_unlock(&journal->j_lock);

    return VFS_OK;
}

/**
 * nexfs_journal_dirty_metadata - Mark metadata as dirty
 * @sb: Superblock
 * @block: Block number
 *
 * Marks a metadata block as modified in the current transaction.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_dirty_metadata(struct nexfs_superblock *sb, u64 block)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    trans = journal->j_current_transaction;

    if (!trans) {
        spin_unlock(&journal->j_lock);
        return VFS_EINVAL;
    }

    /* TODO: Add block to metadata list */

    trans->t_nr_meta_data++;

    spin_unlock(&journal->j_lock);

    return VFS_OK;
}

/**
 * nexfs_journal_revoke - Revoke a block
 * @sb: Superblock
 * @block: Block number
 *
 * Revokes a block from the journal (used for freed blocks).
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_revoke(struct nexfs_superblock *sb, u64 block)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    trans = journal->j_current_transaction;

    if (!trans) {
        spin_unlock(&journal->j_lock);
        return VFS_EINVAL;
    }

    /* TODO: Add block to revoke list */

    journal->j_blocks_revoked++;

    spin_unlock(&journal->j_lock);

    pr_debug("NexFS: Revoked block %llu\n", (unsigned long long)block);

    return VFS_OK;
}

/*===========================================================================*/
/*                         JOURNAL RECOVERY                                  */
/*===========================================================================*/

/**
 * nexfs_journal_scan - Scan journal for valid transactions
 * @journal: Journal structure
 *
 * Scans the journal to find valid transactions.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_scan(struct nexfs_journal *journal)
{
    u32 block;
    u32 sequence;
    struct nexfs_journal_header header;
    s32 ret;

    if (!journal) {
        return VFS_EINVAL;
    }

    block = journal->j_first_block;
    sequence = journal->j_tail_sequence;

    /* Scan journal blocks */
    while (block < journal->j_total_blocks) {
        /* TODO: Read block header */
        /* ret = nexfs_read_block(journal->j_inode, block, &header, sizeof(header)); */
        ret = VFS_OK;

        if (ret != VFS_OK) {
            break;
        }

        /* Check magic */
        if (header.h_magic != NEXFS_JOURNAL_MAGIC) {
            break;
        }

        /* Check sequence */
        if (header.h_sequence != sequence) {
            break;
        }

        /* Process block based on type */
        switch (header.h_blocktype) {
            case NEXFS_J_DESC_BLOCK:
                /* Descriptor block - start of transaction */
                break;

            case NEXFS_J_COMMIT_BLOCK:
                /* Commit block - transaction complete */
                sequence++;
                break;

            case NEXFS_J_REVOKE_BLOCK:
                /* Revoke block */
                break;

            default:
                /* Data block */
                break;
        }

        block++;
    }

    journal->j_head_sequence = sequence - 1;

    return VFS_OK;
}

/**
 * nexfs_journal_do_recovery - Perform journal recovery
 * @journal: Journal structure
 *
 * Replays committed transactions from the journal.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_do_recovery(struct nexfs_journal *journal)
{
    u32 block;
    u32 sequence;
    struct nexfs_journal_header header;
    s32 ret;

    if (!journal) {
        return VFS_EINVAL;
    }

    pr_info("NexFS: Starting journal recovery...\n");

    /* Scan journal */
    ret = nexfs_journal_scan(journal);
    if (ret != VFS_OK) {
        return ret;
    }

    /* Replay transactions */
    block = journal->j_first_block;
    sequence = journal->j_tail_sequence + 1;

    while (sequence <= journal->j_head_sequence) {
        /* TODO: Read and replay each transaction */

        sequence++;
    }

    pr_info("NexFS: Journal recovery complete\n");

    return VFS_OK;
}

/**
 * nexfs_journal_recovery - Recover filesystem from journal
 * @sb: Superblock
 *
 * Recovers the filesystem state from the journal after a crash.
 *
 * Returns: VFS_OK on success, error code on failure
 */
s32 nexfs_journal_recovery(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    s32 ret;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    /* Perform recovery */
    ret = nexfs_journal_do_recovery(journal);
    if (ret != VFS_OK) {
        pr_err("NexFS: Journal recovery failed: %d\n", ret);
        return ret;
    }

    /* Update superblock state */
    sb->raw.s_state = NEXFS_STATE_CLEAN;
    nexfs_write_superblock(sb);

    return VFS_OK;
}

/*===========================================================================*/
/*                         CHECKPOINTING                                     */
/*===========================================================================*/

/**
 * nexfs_journal_checkpoint - Checkpoint journal
 * @sb: Superblock
 *
 * Writes committed transaction data to their final locations.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_checkpoint(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;
    struct nexfs_transaction *trans;
    struct nexfs_transaction *next;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    spin_lock(&journal->j_lock);

    /* Process completed transactions */
    list_for_each_entry_safe(trans, next, &journal->j_completed_transactions, t_list) {
        if (trans->t_state != NEXUS_TRANS_COMMITTED) {
            continue;
        }

        /* TODO: Write transaction data to final locations */

        /* Remove from completed list */
        list_del(&trans->t_list);
        nexfs_transaction_free(trans);

        /* Update tail */
        journal->j_tail_sequence++;
    }

    spin_unlock(&journal->j_lock);

    return VFS_OK;
}

/**
 * nexfs_journal_flush - Flush journal to disk
 * @sb: Superblock
 *
 * Forces all pending journal data to disk.
 *
 * Returns: VFS_OK on success, error code on failure
 */
static s32 nexfs_journal_flush(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;

    if (!sb || !sb->journal) {
        return VFS_EINVAL;
    }

    journal = sb->journal;

    /* Commit current transaction */
    nexfs_journal_commit(sb);

    /* Checkpoint */
    nexfs_journal_checkpoint(sb);

    /* Write superblock */
    nexfs_journal_write_superblock(journal);

    return VFS_OK;
}

/*===========================================================================*/
/*                         JOURNAL STATISTICS                                */
/*===========================================================================*/

/**
 * nexfs_journal_stats - Print journal statistics
 * @sb: Superblock
 */
void nexfs_journal_stats(struct nexfs_superblock *sb)
{
    struct nexfs_journal *journal;

    if (!sb || !sb->journal) {
        return;
    }

    journal = sb->journal;

    printk("\n=== Journal Statistics ===\n");
    printk("State:             0x%04X\n", journal->j_state);
    printk("Sequence:          %u\n", journal->j_sequence);
    printk("Tail Sequence:     %u\n", journal->j_tail_sequence);
    printk("Head Sequence:     %u\n", journal->j_head_sequence);
    printk("Total Blocks:      %u\n", journal->j_total_blocks);
    printk("Free Blocks:       %u\n", journal->j_free);
    printk("Transactions:      %llu\n", (unsigned long long)journal->j_transactions);
    printk("Blocks Written:    %llu\n", (unsigned long long)journal->j_blocks_written);
    printk("Blocks Revoked:    %llu\n", (unsigned long long)journal->j_blocks_revoked);
}
