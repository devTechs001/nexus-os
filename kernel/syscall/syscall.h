/*
 * NEXUS OS - System Call Interface
 * kernel/syscall/syscall.h
 *
 * System call numbers, structures, and API definitions
 */

#ifndef _NEXUS_SYSCALL_H
#define _NEXUS_SYSCALL_H

#include "../include/kernel.h"

/*===========================================================================*/
/*                         SYSCALL CONFIGURATION                             */
/*===========================================================================*/

/* Maximum number of system calls */
#define NR_SYSCALLS         256

/* System call marker for validation */
#define SYSCALL_MARKER      0xDEADBEEF

/* Syscall calling convention (x86_64) */
#define SYSCALL_ARG1        rdi
#define SYSCALL_ARG2        rsi
#define SYSCALL_ARG3        rdx
#define SYSCALL_ARG4        r10
#define SYSCALL_ARG5        r8
#define SYSCALL_ARG6        r9

/* Syscall return value register */
#define SYSCALL_RET         rax

/*===========================================================================*/
/*                         SYSCALL NUMBERS                                   */
/*===========================================================================*/

/* Process Management Syscalls (0-49) */
#define SYS_FORK            0
#define SYS_VFORK           1
#define SYS_CLONE           2
#define SYS_EXECVE          3
#define SYS_EXIT            4
#define SYS_EXIT_GROUP      5
#define SYS_WAIT4           6
#define SYS_WAITPID         7
#define SYS_GETPID          8
#define SYS_GETPPID         9
#define SYS_GETTID          10
#define SYS_GETUID          11
#define SYS_GETEUID         12
#define SYS_GETGID          13
#define SYS_GETEGID         14
#define SYS_SETUID          15
#define SYS_SETEUID         16
#define SYS_SETGID          17
#define SYS_SETEGID         18
#define SYS_SETPGID         19
#define SYS_GETPGID         20
#define SYS_SETSID          21
#define SYS_GETSID          22
#define SYS_GETPGRP         23
#define SYS_SETREUID        24
#define SYS_SETREGID        25
#define SYS_GETGROUPS       26
#define SYS_SETGROUPS       27
#define SYS_SETPRIORITY     28
#define SYS_GETPRIORITY     29
#define SYS_NICE            30
#define SYS_PRCTL           31
#define SYS_ARCH_PRCTL      32
#define SYS_PTRACE          33

/* Memory Management Syscalls (50-99) */
#define SYS_MMAP            50
#define SYS_MUNMAP          51
#define SYS_MPROTECT        52
#define SYS_MREMAP          53
#define SYS_MSYNC           54
#define SYS_MLOCK           55
#define SYS_MUNLOCK         56
#define SYS_MLOCKALL        57
#define SYS_MUNLOCKALL      58
#define SYS_MINCORE         59
#define SYS_MADVISE         60
#define SYS_BRK             61
#define SYS_SBRK            62
#define SYS_MMAP2           63
#define SYS_REMAP_FILE_PAGES 64

/* File Operations Syscalls (100-149) */
#define SYS_OPEN            100
#define SYS_OPENAT          101
#define SYS_CLOSE           102
#define SYS_READ            103
#define SYS_WRITE           104
#define SYS_LSEEK           105
#define SYS_PREAD64         106
#define SYS_PWRITE64        107
#define SYS_READV           108
#define SYS_WRITEV          109
#define SYS_PREADV          110
#define SYS_PWRITEV         111
#define SYS_DUP             112
#define SYS_DUP2            113
#define SYS_DUP3            114
#define SYS_FCNTL           115
#define SYS_IOCTL           116
#define SYS_ACCESS          117
#define SYS_FACCESSAT       118
#define SYS_STAT            119
#define SYS_LSTAT           120
#define SYS_FSTAT           121
#define SYS_STATX           122
#define SYS_GETDENTS        123
#define SYS_GETDENTS64      124
#define SYS_READLINK        125
#define SYS_READLINKAT      126
#define SYS_SYMLINK         127
#define SYS_SYMLINKAT       128
#define SYS_LINK            129
#define SYS_LINKAT          130
#define SYS_UNLINK          131
#define SYS_UNLINKAT        132
#define SYS_RENAME          133
#define SYS_RENAMEAT        134
#define SYS_MKDIR           135
#define SYS_MKDIRAT         136
#define SYS_RMDIR           137
#define SYS_CHMOD           138
#define SYS_FCHMOD          139
#define SYS_CHMODAT         140
#define SYS_CHOWN           141
#define SYS_FCHOWN          142
#define SYS_LCHOWN          143
#define SYS_TRUNCATE        144
#define SYS_FTRUNCATE       145
#define SYS_FALLOCATE       146
#define SYS_FSYNC           147
#define SYS_FDATASYNC       148
#define SYS_SYNC            149

/* IPC Syscalls (150-199) */
#define SYS_PIPE            150
#define SYS_PIPE2           151
#define SYS_SHMGET          152
#define SYS_SHMAT           153
#define SYS_SHMDT           154
#define SYS_SHMCTL          155
#define SYS_SEMGET          156
#define SYS_SEMOP           157
#define SYS_SEMTIMEDOP      158
#define SYS_SEMCTL          159
#define SYS_MSGGET          160
#define SYS_MSGSND          161
#define SYS_MSGRCV          162
#define SYS_MSGCTL          163
#define SYS_SOCKET          164
#define SYS_CONNECT         165
#define SYS_ACCEPT          166
#define SYS_BIND            167
#define SYS_LISTEN          168
#define SYS_SENDTO          169
#define SYS_RECVFROM        170
#define SYS_SENDMSG         171
#define SYS_RECVMSG         172
#define SYS_SHUTDOWN        173
#define SYS_GETSOCKOPT      174
#define SYS_SETSOCKOPT      175
#define SYS_GETSOCKNAME     176
#define SYS_GETPEERNAME     177
#define SYS_SOCKETPAIR      178
#define SYS_EVENTFD         179
#define SYS_SIGNALFD        180
#define SYS_TIMERFD_CREATE  181
#define SYS_TIMERFD_SETTIME 182
#define SYS_TIMERFD_GETTIME 183
#define SYS_EPOLL_CREATE    184
#define SYS_EPOLL_CREATE1   185
#define SYS_EPOLL_CTL       186
#define SYS_EPOLL_WAIT      187
#define SYS_EPOLL_PWAIT     188
#define SYS_POLL            189
#define SYS_PPOLL           190
#define SYS_SELECT          191
#define SYS_PSELECT6        192

/* Scheduling Syscalls (200-224) */
#define SYS_SCHED_YIELD     200
#define SYS_SCHED_SETAFFINITY 201
#define SYS_SCHED_GETAFFINITY   202
#define SYS_SCHED_SETATTR       203
#define SYS_SCHED_GETATTR       204
#define SYS_SCHED_SETPARAM      205
#define SYS_SCHED_GETPARAM      206
#define SYS_SCHED_SET_SCHEDULER 207
#define SYS_SCHED_GET_SCHEDULER 208
#define SYS_SCHED_GET_PRIORITY_MAX 209
#define SYS_SCHED_GET_PRIORITY_MIN 210
#define SYS_SCHED_RR_GET_INTERVAL  211
#define SYS_SCHED_GET_PRIORITY_CLASS 212
#define SYS_SCHED_SET_PRIORITY_CLASS 213
#define SYS_NANOSLEEP         214
#define SYS_CLOCK_NANOSLEEP   215
#define SYS_CLOCK_GETTIME     216
#define SYS_CLOCK_SETTIME     217
#define SYS_CLOCK_GETRES      218
#define SYS_CLOCK_GETCPUCLOCKID 219
#define SYS_GETRUSAGE         220
#define SYS_TIMES             221
#define SYS_GETITIMER         222
#define SYS_SETITIMER         223
#define SYS_ALARM             224

/* Time Syscalls (225-239) */
#define SYS_GETTIMEOFDAY    225
#define SYS_SETTIMEOFDAY    226
#define SYS_GETTIME         227
#define SYS_SETTIME         228
#define SYS_ADJTIMEX        229
#define SYS_STIME           230
#define SYS_TIME            231
#define SYS_UTIME           232
#define SYS_UTIMES          233
#define SYS_FUTIMESAT       234
#define SYS_UTIMENSAT       235
#define SYS_CLOCK_ADJTIME   236
#define SYS_TIMER_CREATE    237
#define SYS_TIMER_SETTIME   238
#define SYS_TIMER_GETTIME   239

/* Signal Handling Syscalls (240-255) */
#define SYS_SIGNAL          240
#define SYS_SIGACTION       241
#define SYS_SIGPROCMASK     242
#define SYS_SIGPENDING      243
#define SYS_SIGSUSPEND      244
#define SYS_SIGRETURN       245
#define SYS_RT_SIGACTION    246
#define SYS_RT_SIGPROCMASK  247
#define SYS_RT_SIGPENDING   248
#define SYS_RT_SIGSUSPEND   249
#define SYS_RT_SIGRETURN    250
#define SYS_RT_SIGTIMEDWAIT 251
#define SYS_RT_SIGQUEUEINFO 252
#define SYS_KILL            253
#define SYS_TKILL           254
#define SYS_TGKILL          255

/*===========================================================================*/
/*                         SYSCALL STRUCTURES                                */
/*===========================================================================*/

/**
 * syscall_frame - CPU register state during syscall
 *
 * This structure represents the saved CPU state when a syscall is invoked.
 * It is populated by the architecture-specific syscall entry code.
 */
struct syscall_frame {
    /* General Purpose Registers */
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rax;

    /* Error Code and Interrupt Number */
    u64 error_code;
    u64 interrupt_nr;

    /* Instruction Pointer and Code Segment */
    u64 rip;
    u64 cs;

    /* Flags and Stack Pointer */
    u64 rflags;
    u64 rsp;

    /* Stack Segment */
    u64 ss;
} __packed;

/**
 * syscall_args - System call arguments container
 */
struct syscall_args {
    u64 nr;         /* System call number */
    u64 arg1;       /* First argument */
    u64 arg2;       /* Second argument */
    u64 arg3;       /* Third argument */
    u64 arg4;       /* Fourth argument */
    u64 arg5;       /* Fifth argument */
    u64 arg6;       /* Sixth argument */
};

/**
 * syscall_handler - System call handler function type
 */
typedef s64 (*syscall_handler_t)(struct syscall_args *args);

/**
 * syscall_entry - System call table entry
 */
struct syscall_entry {
    syscall_handler_t handler;    /* Handler function */
    const char *name;             /* System call name */
    u32 flags;                    /* Handler flags */
    u32 nargs;                    /* Number of arguments */
};

/* Syscall Entry Flags */
#define SYSCALL_F_NATIVE      0x00000001  /* Native 64-bit syscall */
#define SYSCALL_F_COMPAT      0x00000002  /* 32-bit compatibility */
#define SYSCALL_F_NOSMP       0x00000004  /* Not SMP safe */
#define SYSCALL_F_NEEDLOCK    0x00000008  /* Needs big kernel lock */
#define SYSCALL_F_DEPRECATED  0x00000010  /* Deprecated syscall */

/**
 * sigset_t - Signal set
 */
#define _NSIG_WORDS   4
typedef struct {
    unsigned long sig[_NSIG_WORDS];
} sigset_t;

/**
 * sigaction - Signal action structure
 */
struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
};

/* Signal Action Flags */
#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_ONSTACK      0x08000000
#define SA_RESTART      0x10000000
#define SA_NODEFER      0x40000000
#define SA_RESETHAND    0x80000000

/**
 * siginfo_t - Signal information
 */
struct siginfo {
    s32 si_signo;       /* Signal number */
    s32 si_errno;       /* Error number */
    s32 si_code;        /* Signal code */
    union {
        s32 _pad[128 / sizeof(s32) - 3];
        struct {
            pid_t _pid;     /* Sender's PID */
            uid_t _uid;     /* Sender's UID */
        } _kill;
        struct {
            void *_addr;    /* Faulting address */
        } _sigfault;
    } _sifields;
};

/**
 * timespec - Time specification
 */
struct timespec {
    s64 tv_sec;     /* Seconds */
    s64 tv_nsec;    /* Nanoseconds */
};

/**
 * timeval - Time value
 */
struct timeval {
    s64 tv_sec;     /* Seconds */
    s64 tv_usec;    /* Microseconds */
};

/**
 * timezone - Timezone information
 */
struct timezone {
    s32 tz_minuteswest;
    s32 tz_dsttime;
};

/**
 * rusage - Resource usage
 */
struct rusage {
    struct timeval ru_utime;    /* User time */
    struct timeval ru_stime;    /* System time */
    s64 ru_maxrss;              /* Maximum resident set size */
    s64 ru_ixrss;               /* Integral shared memory size */
    s64 ru_idrss;               /* Integral unshared data size */
    s64 ru_isrss;               /* Integral unshared stack size */
    s64 ru_minflt;              /* Page reclaims */
    s64 ru_majflt;              /* Page faults */
    s64 ru_nswap;               /* Swaps */
    s64 ru_inblock;             /* Block input operations */
    s64 ru_oublock;             /* Block output operations */
    s64 ru_msgsnd;              /* Messages sent */
    s64 ru_msgrcv;              /* Messages received */
    s64 ru_nsignals;            /* Signals received */
    s64 ru_nvcsw;               /* Voluntary context switches */
    s64 ru_nivcsw;              /* Involuntary context switches */
};

/**
 * stat - File status
 */
struct stat {
    dev_t st_dev;           /* Device */
    ino_t st_ino;           /* Inode */
    mode_t st_mode;         /* Protection */
    u32 st_nlink;           /* Link count */
    uid_t st_uid;           /* Owner UID */
    gid_t st_gid;           /* Owner GID */
    dev_t st_rdev;          /* Device type */
    off_t st_size;          /* Total size */
    blksize_t st_blksize;   /* Block size */
    blkcnt_t st_blocks;     /* Number of blocks */
    struct timespec st_atime;   /* Last access time */
    struct timespec st_mtime;   /* Last modification time */
    struct timespec st_ctime;   /* Last status change time */
};

/**
 * iovec - I/O vector
 */
struct iovec {
    void *iov_base;       /* Starting address */
    size_t iov_len;       /* Number of bytes */
};

/**
 * pollfd - Poll file descriptor
 */
struct pollfd {
    s32 fd;           /* File descriptor */
    s16 events;       /* Requested events */
    s16 revents;      /* Returned events */
};

/**
 * flock - File lock
 */
struct flock {
    s16 l_type;     /* Lock type */
    s16 l_whence;   /* Starting offset */
    off_t l_start;  /* Offset */
    off_t l_len;    /* Length */
    pid_t l_pid;    /* PID holding lock */
};

/* File Lock Types */
#define F_RDLCK     0   /* Read lock */
#define F_WRLCK     1   /* Write lock */
#define F_UNLCK     2   /* Unlock */

/**
 * utsname - System information
 */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/*===========================================================================*/
/*                         SYSCALL API                                       */
/*===========================================================================*/

/* Syscall Initialization */
void syscall_init(void);

/* Syscall Handling */
s64 syscall_dispatch(struct syscall_frame *frame);
s64 syscall_handler(struct syscall_args *args);

/* Syscall Table */
void syscall_table_init(void);
syscall_handler_t syscall_get_handler(u64 nr);
const char *syscall_get_name(u64 nr);

/* Syscall Registration */
s64 syscall_register(u64 nr, syscall_handler_t handler, const char *name, u32 nargs);
s64 syscall_unregister(u64 nr);

/* User Space Memory Access */
s64 copy_from_user(void *to, const void *from, size_t n);
s64 copy_to_user(void *to, const void *from, size_t n);
s64 strncpy_from_user(char *dest, const char *src, size_t count);
s64 put_user(u64 x, u64 *ptr);
s64 get_user(u64 *x, const u64 *ptr);
bool access_ok(const void *addr, size_t size);

/* Architecture-specific syscall entry */
void syscall_entry(void);
void syscall_exit(void);

/* Syscall Statistics */
void syscall_stats(void);
u64 syscall_get_count(u64 nr);
void syscall_reset_count(u64 nr);

/*===========================================================================*/
/*                         SYSCALL MACROS                                    */
/*===========================================================================*/

/* Validate syscall number */
#define SYSCALL_VALID(nr)   ((nr) < NR_SYSCALLS)

/* Check if syscall is implemented */
#define SYSCALL_IMPLEMENTED(nr)   (syscall_table[nr].handler != NULL)

/* Get syscall argument */
#define SYSCALL_ARG(args, n)    ((args)->arg##n)

/* Return from syscall */
#define SYSCALL_RETURN(ret)     return (ret)

/* Error return from syscall */
#define SYSCALL_ERROR(err)      return (-(err))

/* Success return from syscall */
#define SYSCALL_SUCCESS(ret)    return (ret)

#endif /* _NEXUS_SYSCALL_H */
