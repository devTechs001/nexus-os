/*
 * NEXUS OS - Security Framework
 * security/sandbox/seccomp.c
 *
 * Seccomp Filter Support
 * System call filtering using Berkeley Packet Filter
 */

#include "../security.h"

/*===========================================================================*/
/*                         SECCOMP CONFIGURATION                             */
/*===========================================================================*/

#define SECCOMP_DEBUG           0
#define SECCOMP_MAX_FILTERS     256
#define SECCOMP_MAX_INSTRUCTIONS 4096
#define SECCOMP_MAX_RULES       512

/* Seccomp Modes */
#define SECCOMP_MODE_DISABLED   0
#define SECCOMP_MODE_STRICT     1
#define SECCOMP_MODE_FILTER     2

/* BPF Instruction Classes */
#define BPF_LD                  0x00
#define BPF_LDX                 0x01
#define BPF_ST                  0x02
#define BPF_STX                 0x03
#define BPF_ALU                 0x04
#define BPF_JMP                 0x05
#define BPF_RET                 0x06
#define BPF_MISC                0x07

/* BPF Load Sources */
#define BPF_IMM                 0x00
#define BPF_ABS                 0x20
#define BPF_IND                 0x40
#define BPF_MEM                 0x60
#define BPF_LEN                 0x80
#define BPF_MSH                 0xa0

/* BPF ALU Operations */
#define BPF_ADD                 0x00
#define BPF_SUB                 0x10
#define BPF_MUL                 0x20
#define BPF_DIV                 0x30
#define BPF_OR                  0x40
#define BPF_AND                 0x50
#define BPF_LSH                 0x60
#define BPF_RSH                 0x70
#define BPF_NEG                 0x80
#define BPF_MOD                 0x90
#define BPF_XOR                 0xa0

/* BPF Jump Operations */
#define BPF_JA                  0x00
#define BPF_JEQ                 0x10
#define BPF_JGT                 0x20
#define BPF_JGE                 0x30
#define BPF_JSET                0x40

/* BPF Return Values */
#define BPF_K                   0x00
#define BPF_X                   0x08

/* Seccomp Return Values */
#define SECCOMP_RET_KILL        0x00000000U
#define SECCOMP_RET_TRAP        0x00030000U
#define SECCOMP_RET_ERRNO       0x00050000U
#define SECCOMP_RET_TRACE       0x7ff00000U
#define SECCOMP_RET_LOG         0x7ffc0000U
#define SECCOMP_RET_ALLOW       0x7fff0000U

/* BPF Register Aliases */
#define BPF_REG_A               0
#define BPF_REG_X               1
#define BPF_REG_0               0
#define BPF_REG_1               1
#define BPF_REG_2               2
#define BPF_REG_3               3
#define BPF_REG_4               4
#define BPF_REG_5               5

/* Magic Numbers */
#define SECCOMP_FILTER_MAGIC    0x534543434F4D5001ULL  /* SECCOMP1 */
#define SECCOMP_RULE_MAGIC      0x53454352554C4501ULL  /* SECRULE1 */

/*===========================================================================*/
/*                         SECCOMP DATA STRUCTURES                           */
/*===========================================================================*/

/**
 * seccomp_bpf_insn - BPF instruction
 *
 * Represents a single Berkeley Packet Filter instruction.
 */
typedef struct {
    u16 code;                   /* Instruction code */
    u8 jt;                      /* Jump true offset */
    u8 jf;                      /* Jump false offset */
    u32 k;                      /* Constant operand */
} seccomp_bpf_insn;

/**
 * seccomp_rule_t - Seccomp rule
 *
 * Defines a single syscall filtering rule.
 */
typedef struct {
    u64 magic;                  /* Magic number */
    u32 id;                     /* Rule ID */
    u32 syscall;                /* Syscall number */
    u32 action;                 /* Action (ALLOW/KILL/ERRNO) */
    u32 errno_value;            /* Errno value for ERRNO action */
    seccomp_bpf_insn *filter;   /* BPF filter program */
    u32 filter_len;             /* Filter length */
    u64 created;                /* Creation time */
    u64 match_count;            /* Match count */
    bool enabled;               /* Rule enabled */
} seccomp_rule_t;

/**
 * seccomp_filter_t - Seccomp filter
 *
 * Contains a complete BPF filter program.
 */
typedef struct {
    u64 magic;                  /* Magic number */
    spinlock_t lock;            /* Protection lock */
    u32 id;                     /* Filter ID */
    u32 mode;                   /* Seccomp mode */
    seccomp_bpf_insn *program;  /* BPF program */
    u32 program_len;            /* Program length */
    seccomp_rule_t *rules;      /* Rule array */
    u32 rule_count;             /* Number of rules */
    u32 max_rules;              /* Maximum rules */
    u64 created;                /* Creation time */
    u64 eval_count;             /* Evaluation count */
    u64 allow_count;            /* Allow count */
    u64 deny_count;             /* Deny count */
    u64 kill_count;             /* Kill count */
    bool locked;                /* Filter locked */
} seccomp_filter_t;

/**
 * seccomp_context_t - Seccomp context for a process
 */
typedef struct {
    u32 pid;                    /* Process ID */
    u32 mode;                   /* Seccomp mode */
    seccomp_filter_t *filter;   /* Active filter */
    u64 syscalls_allowed;       /* Allowed syscalls */
    u64 syscalls_denied;        /* Denied syscalls */
    u64 last_syscall;           /* Last syscall number */
    u64 last_syscall_time;      /* Last syscall time */
} seccomp_context_t;

/**
 * seccomp_manager_t - Seccomp manager state
 */
typedef struct {
    spinlock_t lock;            /* Manager lock */
    u32 state;                  /* Manager state */
    seccomp_filter_t **filters; /* Filter table */
    u32 filter_count;           /* Active filters */
    u32 next_filter_id;         /* Next filter ID */
    u32 next_rule_id;           /* Next rule ID */
    u64 total_evaluations;      /* Total evaluations */
    u64 total_violations;       /* Total violations */
} seccomp_manager_t;

/*===========================================================================*/
/*                         SECCOMP GLOBAL DATA                               */
/*===========================================================================*/

static seccomp_manager_t g_seccomp_manager = {
    .lock = {
        .lock = SPINLOCK_UNLOCKED,
        .magic = SPINLOCK_MAGIC,
        .name = "seccomp_manager",
    },
    .state = 0,
    .filters = NULL,
    .filter_count = 0,
    .next_filter_id = 1,
    .next_rule_id = 1,
};

static seccomp_filter_t *seccomp_filter_table[SECCOMP_MAX_FILTERS];

/* Default allowed syscalls for strict mode */
static const u32 seccomp_strict_allowed[] = {
    0,   /* read */
    1,   /* write */
    2,   /* open */
    3,   /* close */
    4,   /* stat */
    5,   /* fstat */
    6,   /* lstat */
    9,   /* mmap */
    11,  /* munmap */
    12,  /* mprotect */
    20,  /* getpid */
    60,  /* exit */
    61,  /* wait4 */
    158, /* arch_prctl */
    231, /* exit_group */
    0xFFFFFFFF  /* Sentinel */
};

/*===========================================================================*/
/*                         SECCOMP INITIALIZATION                            */
/*===========================================================================*/

/**
 * seccomp_init - Initialize seccomp subsystem
 *
 * Initializes the seccomp filtering subsystem.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_init(void)
{
    u32 i;

    pr_info("Seccomp: Initializing seccomp subsystem...\n");

    spin_lock_init_named(&g_seccomp_manager.lock, "seccomp_manager");
    g_seccomp_manager.state = 1;

    /* Initialize filter table */
    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        seccomp_filter_table[i] = NULL;
    }

    g_seccomp_manager.filters = seccomp_filter_table;

    g_seccomp_manager.state = 2;

    pr_info("Seccomp: Subsystem initialized\n");
    pr_info("  Max filters: %u\n", SECCOMP_MAX_FILTERS);
    pr_info("  Max rules per filter: %u\n", SECCOMP_MAX_RULES);

    return OK;
}

/**
 * seccomp_shutdown - Shutdown seccomp subsystem
 *
 * Cleans up all seccomp resources.
 */
void seccomp_shutdown(void)
{
    u32 i;

    pr_info("Seccomp: Shutting down...\n");

    spin_lock(&g_seccomp_manager.lock);

    /* Free all filters */
    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        if (seccomp_filter_table[i]) {
            seccomp_filter_free(seccomp_filter_table[i]->id);
        }
    }

    g_seccomp_manager.filter_count = 0;
    g_seccomp_manager.state = 0;

    spin_unlock(&g_seccomp_manager.lock);

    pr_info("Seccomp: Shutdown complete\n");
}

/*===========================================================================*/
/*                         BPF HELPER FUNCTIONS                              */
/*===========================================================================*/

/**
 * bpf_stmt - Create BPF statement
 * @code: Instruction code
 * @k: Constant operand
 *
 * Creates a BPF statement instruction.
 *
 * Returns: BPF instruction
 */
static inline seccomp_bpf_insn bpf_stmt(u16 code, u32 k)
{
    seccomp_bpf_insn insn;
    insn.code = code;
    insn.jt = 0;
    insn.jf = 0;
    insn.k = k;
    return insn;
}

/**
 * bpf_jump - Create BPF jump instruction
 * @code: Jump code
 * @k: Constant operand
 * @jt: Jump true offset
 * @jf: Jump false offset
 *
 * Creates a BPF jump instruction.
 *
 * Returns: BPF instruction
 */
static inline seccomp_bpf_insn bpf_jump(u16 code, u32 k, u8 jt, u8 jf)
{
    seccomp_bpf_insn insn;
    insn.code = code;
    insn.jt = jt;
    insn.jf = jf;
    insn.k = k;
    return insn;
}

/**
 * bpf_ret - Create BPF return instruction
 * @code: Return code
 * @k: Return value
 *
 * Creates a BPF return instruction.
 *
 * Returns: BPF instruction
 */
static inline seccomp_bpf_insn bpf_ret(u16 code, u32 k)
{
    return bpf_stmt(code | BPF_RET, k);
}

/**
 * bpf_load - Create BPF load instruction
 * @size: Load size (BPF_W, BPF_H, BPF_B)
 * @k: Offset
 *
 * Creates a BPF load instruction.
 *
 * Returns: BPF instruction
 */
static inline seccomp_bpf_insn bpf_load(u16 size, u32 k)
{
    return bpf_stmt(size | BPF_LD | BPF_ABS, k);
}

/*===========================================================================*/
/*                         FILTER MANAGEMENT                                 */
/*===========================================================================*/

/**
 * seccomp_filter_create - Create a new seccomp filter
 * @mode: Seccomp mode
 *
 * Creates a new seccomp filter.
 *
 * Returns: Filter ID on success, error code on failure
 */
s32 seccomp_filter_create(u32 mode)
{
    seccomp_filter_t *filter;
    u32 i;

    /* Find empty slot */
    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        if (seccomp_filter_table[i] == NULL) {
            break;
        }
    }

    if (i >= SECCOMP_MAX_FILTERS) {
        return ENOSPC;
    }

    /* Allocate filter */
    filter = kzalloc(sizeof(seccomp_filter_t));
    if (!filter) {
        return ENOMEM;
    }

    /* Initialize filter */
    filter->magic = SECCOMP_FILTER_MAGIC;
    spin_lock_init_named(&filter->lock, "seccomp_filter");
    filter->id = g_seccomp_manager.next_filter_id++;
    filter->mode = mode;
    filter->max_rules = SECCOMP_MAX_RULES;
    filter->created = get_time_ms();
    filter->locked = false;

    /* Allocate rule array */
    filter->rules = kzalloc(sizeof(seccomp_rule_t) * filter->max_rules);
    if (!filter->rules) {
        kfree(filter);
        return ENOMEM;
    }

    /* Store in table */
    seccomp_filter_table[i] = filter;
    g_seccomp_manager.filter_count++;

    pr_debug("Seccomp: Created filter %u (mode=%u)\n", filter->id, mode);

    return filter->id;
}

/**
 * seccomp_filter_free - Free a seccomp filter
 * @id: Filter ID
 *
 * Frees a seccomp filter and all associated resources.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_filter_free(u32 id)
{
    seccomp_filter_t *filter;
    u32 i;

    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        if (seccomp_filter_table[i] && seccomp_filter_table[i]->id == id) {
            filter = seccomp_filter_table[i];
            break;
        }
    }

    if (i >= SECCOMP_MAX_FILTERS) {
        return ENOENT;
    }

    spin_lock(&filter->lock);

    /* Free BPF program */
    if (filter->program) {
        kfree(filter->program);
        filter->program = NULL;
    }

    /* Free rules */
    if (filter->rules) {
        for (i = 0; i < filter->rule_count; i++) {
            if (filter->rules[i].filter) {
                kfree(filter->rules[i].filter);
            }
        }
        kfree(filter->rules);
        filter->rules = NULL;
    }

    filter->magic = 0;

    spin_unlock(&filter->lock);

    seccomp_filter_table[i] = NULL;
    g_seccomp_manager.filter_count--;

    kfree(filter);

    pr_debug("Seccomp: Freed filter %u\n", id);

    return OK;
}

/**
 * seccomp_filter_get - Get filter by ID
 * @id: Filter ID
 *
 * Returns: Pointer to filter, or NULL if not found
 */
seccomp_filter_t *seccomp_filter_get(u32 id)
{
    u32 i;

    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        if (seccomp_filter_table[i] && seccomp_filter_table[i]->id == id) {
            return seccomp_filter_table[i];
        }
    }

    return NULL;
}

/**
 * seccomp_filter_lock - Lock a filter
 * @id: Filter ID
 *
 * Locks a filter to prevent further modifications.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_filter_lock(u32 id)
{
    seccomp_filter_t *filter;

    filter = seccomp_filter_get(id);
    if (!filter) {
        return ENOENT;
    }

    spin_lock(&filter->lock);
    filter->locked = true;
    spin_unlock(&filter->lock);

    return OK;
}

/*===========================================================================*/
/*                         RULE MANAGEMENT                                   */
/*===========================================================================*/

/**
 * seccomp_rule_add - Add a rule to a filter
 * @filter_id: Filter ID
 * @syscall: Syscall number
 * @action: Action to take
 *
 * Adds a syscall rule to a filter.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_rule_add(u32 filter_id, u32 syscall, u32 action)
{
    seccomp_filter_t *filter;
    seccomp_rule_t *rule;

    filter = seccomp_filter_get(filter_id);
    if (!filter) {
        return ENOENT;
    }

    spin_lock(&filter->lock);

    if (filter->locked) {
        spin_unlock(&filter->lock);
        return EPERM;
    }

    if (filter->rule_count >= filter->max_rules) {
        spin_unlock(&filter->lock);
        return ENOSPC;
    }

    rule = &filter->rules[filter->rule_count];
    rule->magic = SECCOMP_RULE_MAGIC;
    rule->id = g_seccomp_manager.next_rule_id++;
    rule->syscall = syscall;
    rule->action = action;
    rule->errno_value = 0;
    rule->created = get_time_ms();
    rule->enabled = true;

    filter->rule_count++;

    spin_unlock(&filter->lock);

    pr_debug("Seccomp: Added rule for syscall %u (action=%u) to filter %u\n",
             syscall, action, filter_id);

    return OK;
}

/**
 * seccomp_rule_add_errno - Add a rule with errno return
 * @filter_id: Filter ID
 * @syscall: Syscall number
 * @errno_val: Errno value to return
 *
 * Adds a syscall rule that returns a specific errno.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_rule_add_errno(u32 filter_id, u32 syscall, u32 errno_val)
{
    seccomp_filter_t *filter;
    seccomp_rule_t *rule;

    filter = seccomp_filter_get(filter_id);
    if (!filter) {
        return ENOENT;
    }

    spin_lock(&filter->lock);

    if (filter->locked) {
        spin_unlock(&filter->lock);
        return EPERM;
    }

    if (filter->rule_count >= filter->max_rules) {
        spin_unlock(&filter->lock);
        return ENOSPC;
    }

    rule = &filter->rules[filter->rule_count];
    rule->magic = SECCOMP_RULE_MAGIC;
    rule->id = g_seccomp_manager.next_rule_id++;
    rule->syscall = syscall;
    rule->action = SECCOMP_RET_ERRNO;
    rule->errno_value = errno_val;
    rule->created = get_time_ms();
    rule->enabled = true;

    filter->rule_count++;

    spin_unlock(&filter->lock);

    return OK;
}

/**
 * seccomp_rule_remove - Remove a rule from a filter
 * @filter_id: Filter ID
 * @syscall: Syscall number
 *
 * Removes a syscall rule from a filter.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_rule_remove(u32 filter_id, u32 syscall)
{
    seccomp_filter_t *filter;
    u32 i;

    filter = seccomp_filter_get(filter_id);
    if (!filter) {
        return ENOENT;
    }

    spin_lock(&filter->lock);

    if (filter->locked) {
        spin_unlock(&filter->lock);
        return EPERM;
    }

    for (i = 0; i < filter->rule_count; i++) {
        if (filter->rules[i].syscall == syscall) {
            /* Shift remaining rules */
            for (; i < filter->rule_count - 1; i++) {
                filter->rules[i] = filter->rules[i + 1];
            }
            filter->rule_count--;
            spin_unlock(&filter->lock);
            return OK;
        }
    }

    spin_unlock(&filter->lock);
    return ENOENT;
}

/*===========================================================================*/
/*                         FILTER PROGRAM GENERATION                         */
/*===========================================================================*/

/**
 * seccomp_filter_compile - Compile filter rules to BPF
 * @filter_id: Filter ID
 *
 * Compiles the filter rules into a BPF program.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_filter_compile(u32 filter_id)
{
    seccomp_filter_t *filter;
    seccomp_bpf_insn *program;
    u32 program_len;
    u32 i;

    filter = seccomp_filter_get(filter_id);
    if (!filter) {
        return ENOENT;
    }

    spin_lock(&filter->lock);

    if (filter->locked) {
        spin_unlock(&filter->lock);
        return EPERM;
    }

    /* Calculate program size */
    /* Each rule needs: load syscall, compare, jump, return */
    program_len = filter->rule_count * 4 + 2;  /* Rules + default allow + return */

    if (program_len > SECCOMP_MAX_INSTRUCTIONS) {
        spin_unlock(&filter->lock);
        return E2BIG;
    }

    /* Allocate program */
    if (filter->program) {
        kfree(filter->program);
    }

    program = kzalloc(sizeof(seccomp_bpf_insn) * program_len);
    if (!program) {
        spin_unlock(&filter->lock);
        return ENOMEM;
    }

    /* Generate BPF program */
    u32 pc = 0;

    for (i = 0; i < filter->rule_count; i++) {
        seccomp_rule_t *rule = &filter->rules[i];
        u32 jump_offset = filter->rule_count - i;

        if (!rule->enabled) {
            continue;
        }

        /* Load syscall number */
        program[pc++] = bpf_load(BPF_W, 0);

        /* Compare with syscall number */
        program[pc++] = bpf_jump(BPF_JMP | BPF_JEQ | BPF_K,
                                  rule->syscall, 0, jump_offset);

        /* Return action */
        program[pc++] = bpf_ret(BPF_K, rule->action);
    }

    /* Default: allow */
    program[pc++] = bpf_ret(BPF_K, SECCOMP_RET_ALLOW);

    filter->program = program;
    filter->program_len = pc;

    spin_unlock(&filter->lock);

    pr_debug("Seccomp: Compiled filter %u (%u instructions)\n", filter_id, pc);

    return OK;
}

/**
 * seccomp_filter_load - Load a filter for a process
 * @filter_id: Filter ID
 * @pid: Process ID
 *
 * Loads a filter for the specified process.
 *
 * Returns: OK on success, error code on failure
 */
s32 seccomp_filter_load(u32 filter_id, u32 pid)
{
    seccomp_filter_t *filter;

    filter = seccomp_filter_get(filter_id);
    if (!filter) {
        return ENOENT;
    }

    if (!filter->program) {
        return EINVAL;
    }

    /* In real implementation, would use ptrace or prctl */
    /* to load the filter into the process */

    pr_info("Seccomp: Loaded filter %u for PID %u\n", filter_id, pid);

    return OK;
}

/*===========================================================================*/
/*                         SYSCALL EVALUATION                                */
/*===========================================================================*/

/**
 * seccomp_check_syscall - Check if syscall is allowed
 * @filter: Filter to check
 * @syscall: Syscall number
 *
 * Evaluates a syscall against the filter.
 *
 * Returns: SECCOMP_RET_* action
 */
u32 seccomp_check_syscall(seccomp_filter_t *filter, u32 syscall)
{
    u32 i;
    u32 action = SECCOMP_RET_ALLOW;

    if (!filter || !filter->program) {
        return SECCOMP_RET_ALLOW;
    }

    spin_lock(&filter->lock);

    filter->eval_count++;

    /* Simple linear search through rules */
    for (i = 0; i < filter->rule_count; i++) {
        seccomp_rule_t *rule = &filter->rules[i];

        if (!rule->enabled) {
            continue;
        }

        if (rule->syscall == syscall) {
            action = rule->action;
            rule->match_count++;

            switch (action) {
                case SECCOMP_RET_ALLOW:
                    filter->allow_count++;
                    break;
                case SECCOMP_RET_KILL:
                    filter->kill_count++;
                    break;
                default:
                    filter->deny_count++;
                    break;
            }

            break;
        }
    }

    spin_unlock(&filter->lock);

    return action;
}

/**
 * seccomp_strict_check - Check syscall in strict mode
 * @syscall: Syscall number
 *
 * Checks if syscall is allowed in strict mode.
 *
 * Returns: SECCOMP_RET_* action
 */
u32 seccomp_strict_check(u32 syscall)
{
    u32 i;

    for (i = 0; seccomp_strict_allowed[i] != 0xFFFFFFFF; i++) {
        if (seccomp_strict_allowed[i] == syscall) {
            return SECCOMP_RET_ALLOW;
        }
    }

    return SECCOMP_RET_KILL;
}

/*===========================================================================*/
/*                         PREDEFINED POLICIES                               */
/*===========================================================================*/

/**
 * seccomp_policy_default - Create default policy
 *
 * Creates a default seccomp policy that allows common syscalls.
 *
 * Returns: Filter ID on success, error code on failure
 */
s32 seccomp_policy_default(void)
{
    s32 filter_id;
    u32 allowed_syscalls[] = {
        0,   /* read */
        1,   /* write */
        2,   /* open */
        3,   /* close */
        4,   /* stat */
        5,   /* fstat */
        6,   /* lstat */
        8,   /* lseek */
        9,   /* mmap */
        11,  /* munmap */
        12,  /* mprotect */
        19,  /* brk */
        20,  /* getpid */
        21,  /* getppid */
        23,  /* getuid */
        24,  /* geteuid */
        25,  /* getgid */
        26,  /* getegid */
        33,  /* access */
        41,  /* pipe */
        42,  /* select */
        56,  /* openat */
        57,  /* mkdirat */
        58,  /* unlinkat */
        59,  /* execve */
        60,  /* exit */
        61,  /* wait4 */
        62,  /* kill */
        83,  /* symlink */
        85,  /* readlink */
        87,  /* rename */
        89,  /* mkdir */
        90,  /* rmdir */
        91,  /* creat */
        93,  /* link */
        94,  /* unlink */
        95,  /* symlinkat */
        96,  /* readlinkat */
        157, /* prctl */
        158, /* arch_prctl */
        218, /* set_tid_address */
        228, /* clock_gettime */
        231, /* exit_group */
        257, /* openat */
        262, /* newfstatat */
        272, /* unlinkat */
        288, /* accept4 */
        302, /* prlimit64 */
        318, /* getrandom */
        334, /* rseq */
        0xFFFFFFFF
    };
    u32 i;

    filter_id = seccomp_filter_create(SECCOMP_MODE_FILTER);
    if (filter_id < 0) {
        return filter_id;
    }

    /* Add allow rules for common syscalls */
    for (i = 0; allowed_syscalls[i] != 0xFFFFFFFF; i++) {
        seccomp_rule_add(filter_id, allowed_syscalls[i], SECCOMP_RET_ALLOW);
    }

    /* Compile the filter */
    seccomp_filter_compile(filter_id);

    /* Lock the filter */
    seccomp_filter_lock(filter_id);

    return filter_id;
}

/**
 * seccomp_policy_restrictive - Create restrictive policy
 *
 * Creates a restrictive seccomp policy.
 *
 * Returns: Filter ID on success, error code on failure
 */
s32 seccomp_policy_restrictive(void)
{
    s32 filter_id;
    u32 allowed_syscalls[] = {
        0,   /* read */
        1,   /* write */
        3,   /* close */
        5,   /* fstat */
        9,   /* mmap */
        11,  /* munmap */
        12,  /* mprotect */
        19,  /* brk */
        21,  /* getppid */
        231, /* exit_group */
        0xFFFFFFFF
    };
    u32 i;

    filter_id = seccomp_filter_create(SECCOMP_MODE_FILTER);
    if (filter_id < 0) {
        return filter_id;
    }

    /* Only allow minimal syscalls */
    for (i = 0; allowed_syscalls[i] != 0xFFFFFFFF; i++) {
        seccomp_rule_add(filter_id, allowed_syscalls[i], SECCOMP_RET_ALLOW);
    }

    /* Kill all other syscalls */
    seccomp_filter_compile(filter_id);
    seccomp_filter_lock(filter_id);

    return filter_id;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * seccomp_stats - Print seccomp statistics
 */
void seccomp_stats(void)
{
    u32 i;
    seccomp_filter_t *filter;

    printk("\n=== Seccomp Statistics ===\n");
    printk("State: %u\n", g_seccomp_manager.state);
    printk("Active Filters: %u\n", g_seccomp_manager.filter_count);
    printk("Total Evaluations: %llu\n",
           (unsigned long long)g_seccomp_manager.total_evaluations);
    printk("Total Violations: %llu\n",
           (unsigned long long)g_seccomp_manager.total_violations);

    printk("\n=== Active Filters ===\n");
    for (i = 0; i < SECCOMP_MAX_FILTERS; i++) {
        filter = seccomp_filter_table[i];
        if (filter && filter->magic == SECCOMP_FILTER_MAGIC) {
            printk("  [%u] ID=%u Mode=%u Rules=%u Eval=%llu Allow=%llu "
                   "Deny=%llu Kill=%llu Locked=%s\n",
                   i, filter->id, filter->mode, filter->rule_count,
                   (unsigned long long)filter->eval_count,
                   (unsigned long long)filter->allow_count,
                   (unsigned long long)filter->deny_count,
                   (unsigned long long)filter->kill_count,
                   filter->locked ? "yes" : "no");
        }
    }
}
