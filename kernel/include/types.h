/*
 * NEXUS OS - Type Definitions
 * kernel/include/types.h
 */

#ifndef _NEXUS_TYPES_H
#define _NEXUS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declarations */
struct task_struct;

/* Architecture Detection */
#if defined(__x86_64__)
    #define ARCH_X86_64     1
    #define ARCH_NAME       "x86_64"
    #define ARCH_BITS       64
#elif defined(__aarch64__)
    #define ARCH_ARM64      1
    #define ARCH_NAME       "arm64"
    #define ARCH_BITS       64
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define ARCH_RISCV64    1
    #define ARCH_NAME       "riscv64"
    #define ARCH_BITS       64
#endif

/* Standard Types */
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;

typedef intptr_t    sreg;
typedef uintptr_t   ureg;
typedef uintptr_t   uintptr;
typedef intptr_t    intptr;

typedef u64         size_t;
typedef s64         ssize_t;
typedef s64         off_t;
typedef u64         time_t;
typedef s64         clock_t;

typedef u32         pid_t;
typedef u32         tid_t;
typedef u32         uid_t;
typedef u32         gid_t;
typedef u32         mode_t;
typedef u32         dev_t;
typedef u64         ino_t;
typedef s64         blkcnt_t;
typedef s64         blksize_t;

typedef u64         phys_addr_t;
typedef u64         virt_addr_t;
typedef u64         dma_addr_t;

/* File offset type */
typedef s64         loff_t;

/* Mode type */
typedef u16         umode_t;

/* Boolean */
#ifndef NULL
#define NULL        ((void*)0)
#endif

/* Page Table Entry */
typedef u64 pte_t;
typedef u64 pmd_t;
typedef u64 pud_t;
typedef u64 pgd_t;

/* Limits */
#define S8_MIN      (-128)
#define S8_MAX      127
#define U8_MAX      255
#define S16_MIN     (-32768)
#define S16_MAX     32767
#define U16_MAX     65535
#define S32_MIN     (-2147483647 - 1)
#define S32_MAX     2147483647
#define U32_MAX     4294967295U
#define S64_MIN     (-9223372036854775807LL - 1)
#define S64_MAX     9223372036854775807LL
#define U64_MAX     18446744073709551615ULL

/* Page Constants */
#define PAGE_SHIFT_4K   12
#define PAGE_SIZE_4K    (1ULL << PAGE_SHIFT_4K)
#define PAGE_SIZE       PAGE_SIZE_4K
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x)   (((x) + PAGE_SIZE - 1) & PAGE_MASK)

/* Cache Line */
#define CACHE_LINE_SIZE   64
#define CACHE_LINE_ALIGN  __aligned(CACHE_LINE_SIZE)

/* Atomic Types */
typedef struct atomic {
    volatile s32 counter;
} atomic_t;

typedef struct atomic64 {
    volatile s64 counter;
} atomic64_t;

/* Preempt Count Type */
typedef struct preempt_count {
    s32 count;
    s32 softirq_count;
    s32 hardirq_count;
} preempt_count_t;

/* Result Type */
typedef s64 result_t;

/* Status Codes */
#define OK              0
#define ERROR           (-1)
#define ENOMEM          (-2)
#define EINVAL          (-3)
#define ENOENT          (-4)
#define EBUSY           (-5)
#define EACCES          (-6)
#define EIO             (-7)
#define ETIMEDOUT       (-8)
#define ENOSYS          (-9)
#define EEXIST          (-10)
#define EAGAIN          (-11)
#define ENODEV          (-12)
#define ENOTSUP         (-13)
#define EPERM           (-14)
#define ESRCH           (-15)
#define EINTR           (-16)
#define EFAULT          (-17)
#define ENOTCONN        (-18)
#define ENETDOWN        (-19)
#define EPIPE           (-25)

/* File Descriptor */
typedef s32 fd_t;
#define FD_INVALID      (-1)
#define FD_STDIN        0
#define FD_STDOUT       1
#define FD_STDERR       2

/* List Head */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/* Spinlock */
typedef struct {
    volatile u32 lock;
    u32 magic;
    const char *name;
    u32 owner_cpu;
    u64 acquire_time;
    u64 total_hold_time;
    u64 acquire_count;
    u64 contention_count;
} spinlock_t;

/* Read-write lock */
typedef struct {
    volatile u32 lock;
    spinlock_t write_lock;
    u32 magic;
    const char *name;
    u32 readers;
    u32 writer_cpu;
    u32 max_readers;
    u64 read_count;
    u64 write_count;
    u64 read_contentions;
    u64 write_contentions;
} rwlock_t;

/* Mutex */
typedef struct {
    atomic_t count;               /* Lock count */
    spinlock_t wait_lock;         /* Waiter list lock */
    struct list_head wait_list;   /* Waiter list */
    u32 magic;                    /* Magic number */
    u32 flags;                    /* Mutex flags */
    struct task_struct *owner;    /* Owner task */
    u32 spin_count;               /* Spin count before sleep */
    struct list_head global_list; /* Global list of mutexes */
    u32 recursion_count;          /* Recursion count for recursive mutexes */
} mutex_t;

/* RT Mutex (for priority inheritance) */
typedef struct {
    spinlock_t wait_lock;         /* Waiter list lock */
    struct task_struct *owner;    /* Owner task (with PI bits) */
    struct list_head wait_list;   /* Waiter list */
    s32 pi_boosted_prio;          /* Priority inheritance boosted priority */
} rt_mutex_t;

/*===========================================================================*/
/*                         ADDITIONAL TYPES                                  */
/*===========================================================================*/

/* Red-Black Tree Node (for scheduler and IPC) */
struct rb_node {
    unsigned long rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
    struct rb_node *rb_parent;  /* Parent node (for some implementations) */
    int rb_color;               /* Node color (0=red, 1=black) */
};

/* RB Tree Colors */
#define RB_RED      0
#define RB_BLACK    1

/* Red-Black Tree Root */
struct rb_root {
    struct rb_node *rb_node;
};

#define RB_ROOT     (struct rb_root){ NULL }
#define RB_ROOT_CACHED  (struct rb_root_cached){ RB_ROOT, NULL }

/* IPC Key Type */
typedef s32 key_t;

/* Signal Information Type */
typedef struct siginfo {
    s32 si_signo;
    s32 si_errno;
    s32 si_code;
    union {
        s32 _pad[128 / sizeof(s32) - 3];
        struct {
            pid_t _pid;
            uid_t _uid;
        } _kill;
        struct {
            void *_addr;
        } _sigfault;
    } _sifields;
} siginfo_t;

/* Clock ID Type */
typedef s32 clockid_t;

/* Timer ID Type */
typedef void *timer_t;

/* Radix Tree (for IPC) */
struct radix_tree_node {
    void *slots[64];
    unsigned long tags[3][2];
};

struct radix_tree_root {
    u32 height;
    u32 gfp_mask;
    struct radix_tree_node *rnode;
};

/* Additional Error Codes (POSIX-like) - only new ones not defined above */
#ifndef ECHILD
#define ECHILD      (-10)
#endif
#ifndef ENOTTY
#define ENOTTY      (-25)
#endif
#ifndef EDEADLK
#define EDEADLK     (-35)
#endif
#ifndef ENOTRECOVERABLE
#define ENOTRECOVERABLE (-36)
#endif
#ifndef EOWNERDEAD
#define EOWNERDEAD  (-37)
#endif
#ifndef ERANGE
#define ERANGE      (-34)
#endif
#ifndef E2BIG
#define E2BIG       (-7)
#endif
#ifndef ENXIO
#define ENXIO       (-6)
#endif
#ifndef ENOTDIR
#define ENOTDIR     (-20)
#endif
#ifndef EISDIR
#define EISDIR      (-21)
#endif
#ifndef ENFILE
#define ENFILE      (-23)
#endif
#ifndef EMFILE
#define EMFILE      (-24)
#endif
#ifndef EOVERFLOW
#define EOVERFLOW   (-79)
#endif
#ifndef ENOMSG
#define ENOMSG      (-35)
#endif
#ifndef EBADMSG
#define EBADMSG     (-77)
#endif
#ifndef ENODATA
#define ENODATA     (-61)
#endif
#ifndef ETIME
#define ETIME       (-62)
#endif
#ifndef EMSGSIZE
#define EMSGSIZE    (-90)
#endif

/* Signal Numbers */
#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGIOT      SIGABRT
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALRM   26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGIO       29
#define SIGPOLL     SIGIO
#define SIGPWR      30
#define SIGSYS      31

/* Signal Set */
typedef struct {
    u64 sig[8];
} sigset_t;

/* File descriptor set */
typedef struct {
    u64 fds_bits[1024 / 64];
} fd_set;

#define FD_SETSIZE 1024

#define FD_SET(fd, fdsetp)      ((fdsetp)->fds_bits[(fd)/64] |= (1ULL << ((fd) % 64)))
#define FD_CLR(fd, fdsetp)      ((fdsetp)->fds_bits[(fd)/64] &= ~(1ULL << ((fd) % 64)))
#define FD_ISSET(fd, fdsetp)    (((fdsetp)->fds_bits[(fd)/64] & (1ULL << ((fd) % 64))) != 0)
#define FD_ZERO(fdsetp)         memset((fdsetp), 0, sizeof(*(fdsetp)))

#endif /* _NEXUS_TYPES_H */
