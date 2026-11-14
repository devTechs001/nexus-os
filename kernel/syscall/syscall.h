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
/* Syscall numbering lives in one place, shared with userspace libsys. */
#include "../include/nexus_syscall_numbers.h"


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
/* sigset_t is defined in types.h */

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

/* siginfo_t is defined in types.h */

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
#ifndef _NEXUS_IOVEC_DEFINED
#define _NEXUS_IOVEC_DEFINED
struct iovec {
    void *iov_base;       /* Starting address */
    size_t iov_len;       /* Number of bytes */
};
#endif

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
/*                     SYSCALL FUNCTION DECLARATIONS                         */
/*===========================================================================*/

/* Process Management Syscalls */
s64 sys_fork(struct syscall_args *args);
s64 sys_vfork(struct syscall_args *args);
s64 sys_clone(struct syscall_args *args);
s64 sys_execve(struct syscall_args *args);
s64 sys_exit(struct syscall_args *args);
s64 sys_exit_group(struct syscall_args *args);
s64 sys_wait4(struct syscall_args *args);
s64 sys_waitpid(struct syscall_args *args);
s64 sys_getpid(struct syscall_args *args);
s64 sys_getppid(struct syscall_args *args);
s64 sys_gettid(struct syscall_args *args);
s64 sys_getuid(struct syscall_args *args);
s64 sys_geteuid(struct syscall_args *args);
s64 sys_getgid(struct syscall_args *args);
s64 sys_getegid(struct syscall_args *args);
s64 sys_setuid(struct syscall_args *args);
s64 sys_seteuid(struct syscall_args *args);
s64 sys_setgid(struct syscall_args *args);
s64 sys_setegid(struct syscall_args *args);
s64 sys_setpgid(struct syscall_args *args);
s64 sys_getpgid(struct syscall_args *args);
s64 sys_setsid(struct syscall_args *args);
s64 sys_getsid(struct syscall_args *args);
s64 sys_getpgrp(struct syscall_args *args);
s64 sys_setreuid(struct syscall_args *args);
s64 sys_setregid(struct syscall_args *args);
s64 sys_getgroups(struct syscall_args *args);
s64 sys_setgroups(struct syscall_args *args);
s64 sys_setpriority(struct syscall_args *args);
s64 sys_getpriority(struct syscall_args *args);
s64 sys_nice(struct syscall_args *args);
s64 sys_prctl(struct syscall_args *args);
s64 sys_arch_prctl(struct syscall_args *args);
s64 sys_ptrace(struct syscall_args *args);

/* Memory Management Syscalls */
s64 sys_mmap(struct syscall_args *args);
s64 sys_munmap(struct syscall_args *args);
s64 sys_mprotect(struct syscall_args *args);
s64 sys_mremap(struct syscall_args *args);
s64 sys_msync(struct syscall_args *args);
s64 sys_mlock(struct syscall_args *args);
s64 sys_munlock(struct syscall_args *args);
s64 sys_mlockall(struct syscall_args *args);
s64 sys_munlockall(struct syscall_args *args);
s64 sys_mincore(struct syscall_args *args);
s64 sys_madvise(struct syscall_args *args);
s64 sys_brk(struct syscall_args *args);
s64 sys_sbrk(struct syscall_args *args);
s64 sys_mmap2(struct syscall_args *args);
s64 sys_remap_file_pages(struct syscall_args *args);

/* File Operations Syscalls */
s64 sys_open(struct syscall_args *args);
s64 sys_openat(struct syscall_args *args);
s64 sys_close(struct syscall_args *args);
s64 sys_read(struct syscall_args *args);
s64 sys_write(struct syscall_args *args);
s64 sys_lseek(struct syscall_args *args);
s64 sys_pread64(struct syscall_args *args);
s64 sys_pwrite64(struct syscall_args *args);
s64 sys_readv(struct syscall_args *args);
s64 sys_writev(struct syscall_args *args);
s64 sys_preadv(struct syscall_args *args);
s64 sys_pwritev(struct syscall_args *args);
s64 sys_dup(struct syscall_args *args);
s64 sys_dup2(struct syscall_args *args);
s64 sys_dup3(struct syscall_args *args);
s64 sys_fcntl(struct syscall_args *args);
s64 sys_ioctl(struct syscall_args *args);
s64 sys_access(struct syscall_args *args);
s64 sys_faccessat(struct syscall_args *args);
s64 sys_stat(struct syscall_args *args);
s64 sys_lstat(struct syscall_args *args);
s64 sys_fstat(struct syscall_args *args);
s64 sys_statx(struct syscall_args *args);
s64 sys_getdents(struct syscall_args *args);
s64 sys_getdents64(struct syscall_args *args);
s64 sys_readlink(struct syscall_args *args);
s64 sys_readlinkat(struct syscall_args *args);
s64 sys_symlink(struct syscall_args *args);
s64 sys_symlinkat(struct syscall_args *args);
s64 sys_link(struct syscall_args *args);
s64 sys_linkat(struct syscall_args *args);
s64 sys_unlink(struct syscall_args *args);
s64 sys_unlinkat(struct syscall_args *args);
s64 sys_rename(struct syscall_args *args);
s64 sys_renameat(struct syscall_args *args);
s64 sys_mkdir(struct syscall_args *args);
s64 sys_mkdirat(struct syscall_args *args);
s64 sys_rmdir(struct syscall_args *args);
s64 sys_chmod(struct syscall_args *args);
s64 sys_fchmod(struct syscall_args *args);
s64 sys_chmodat(struct syscall_args *args);
s64 sys_chown(struct syscall_args *args);
s64 sys_fchown(struct syscall_args *args);
s64 sys_lchown(struct syscall_args *args);
s64 sys_truncate(struct syscall_args *args);
s64 sys_ftruncate(struct syscall_args *args);
s64 sys_fallocate(struct syscall_args *args);
s64 sys_fsync(struct syscall_args *args);
s64 sys_fdatasync(struct syscall_args *args);
s64 sys_sync(struct syscall_args *args);

/* IPC Syscalls */
s64 sys_pipe(struct syscall_args *args);
s64 sys_pipe2(struct syscall_args *args);
s64 sys_shmget(struct syscall_args *args);
s64 sys_shmat(struct syscall_args *args);
s64 sys_shmdt(struct syscall_args *args);
s64 sys_shmctl(struct syscall_args *args);
s64 sys_semget(struct syscall_args *args);
s64 sys_semop(struct syscall_args *args);
s64 sys_semctl(struct syscall_args *args);
s64 sys_msgget(struct syscall_args *args);
s64 sys_msgsnd(struct syscall_args *args);
s64 sys_msgrcv(struct syscall_args *args);
s64 sys_msgctl(struct syscall_args *args);
s64 sys_socket(struct syscall_args *args);
s64 sys_bind(struct syscall_args *args);
s64 sys_connect(struct syscall_args *args);
s64 sys_listen(struct syscall_args *args);
s64 sys_accept(struct syscall_args *args);
s64 sys_getsockname(struct syscall_args *args);
s64 sys_getpeername(struct syscall_args *args);
s64 sys_socketpair(struct syscall_args *args);
s64 sys_send(struct syscall_args *args);
s64 sys_recv(struct syscall_args *args);
s64 sys_sendto(struct syscall_args *args);
s64 sys_recvfrom(struct syscall_args *args);
s64 sys_shutdown(struct syscall_args *args);
s64 sys_setsockopt(struct syscall_args *args);
s64 sys_getsockopt(struct syscall_args *args);
s64 sys_sendmsg(struct syscall_args *args);
s64 sys_recvmsg(struct syscall_args *args);

/* Signal Syscalls */
s64 sys_rt_sigaction(struct syscall_args *args);
s64 sys_rt_sigprocmask(struct syscall_args *args);
s64 sys_rt_sigpending(struct syscall_args *args);
s64 sys_rt_sigsuspend(struct syscall_args *args);
s64 sys_rt_sigreturn(struct syscall_args *args);
s64 sys_rt_sigtimedwait(struct syscall_args *args);
s64 sys_rt_sigqueueinfo(struct syscall_args *args);
s64 sys_kill(struct syscall_args *args);
s64 sys_tkill(struct syscall_args *args);
s64 sys_tgkill(struct syscall_args *args);
s64 sys_mknod(struct syscall_args *args);
s64 sys_getcmdline(struct syscall_args *args);
s64 sys_sigpending(struct syscall_args *args);
s64 sys_sigsuspend(struct syscall_args *args);
s64 sys_sigreturn(struct syscall_args *args);

/* Time Syscalls */
s64 sys_gettimeofday(struct syscall_args *args);
s64 sys_settimeofday(struct syscall_args *args);
s64 sys_getitimer(struct syscall_args *args);
s64 sys_setitimer(struct syscall_args *args);
s64 sys_clock_gettime(struct syscall_args *args);
s64 sys_clock_settime(struct syscall_args *args);
s64 sys_clock_getres(struct syscall_args *args);
s64 sys_clock_nanosleep(struct syscall_args *args);
s64 sys_nanosleep(struct syscall_args *args);
s64 sys_sysinfo(struct syscall_args *args);

/* Miscellaneous Syscalls */
s64 sys_uname(struct syscall_args *args);
s64 sys_getrlimit(struct syscall_args *args);
s64 sys_setrlimit(struct syscall_args *args);
s64 sys_getrusage(struct syscall_args *args);
s64 sys_times(struct syscall_args *args);
s64 sys_syslog(struct syscall_args *args);
s64 sys_gettid(struct syscall_args *args);
s64 sys_futex(struct syscall_args *args);
s64 sys_sched_yield(struct syscall_args *args);
s64 sys_sched_getaffinity(struct syscall_args *args);
s64 sys_sched_setaffinity(struct syscall_args *args);
s64 sys_sched_getscheduler(struct syscall_args *args);
s64 sys_sched_setscheduler(struct syscall_args *args);
s64 sys_sched_get_priority_max(struct syscall_args *args);
s64 sys_sched_get_priority_min(struct syscall_args *args);
s64 sys_sched_rr_get_interval(struct syscall_args *args);
s64 sys_mbind(struct syscall_args *args);
s64 sys_migrate_pages(struct syscall_args *args);
s64 sys_move_pages(struct syscall_args *args);
s64 sys_getcpu(struct syscall_args *args);
s64 sys_epoll_create(struct syscall_args *args);
s64 sys_epoll_create1(struct syscall_args *args);
s64 sys_epoll_ctl(struct syscall_args *args);
s64 sys_epoll_wait(struct syscall_args *args);
s64 sys_epoll_pwait(struct syscall_args *args);
s64 sys_select(struct syscall_args *args);
s64 sys_pselect6(struct syscall_args *args);
s64 sys_poll(struct syscall_args *args);
s64 sys_ppoll(struct syscall_args *args);
s64 sys_inotify_init(struct syscall_args *args);
s64 sys_inotify_init1(struct syscall_args *args);
s64 sys_inotify_add_watch(struct syscall_args *args);
s64 sys_inotify_rm_watch(struct syscall_args *args);
s64 sys_eventfd(struct syscall_args *args);
s64 sys_eventfd2(struct syscall_args *args);
s64 sys_signalfd(struct syscall_args *args);
s64 sys_signalfd4(struct syscall_args *args);
s64 sys_timerfd_create(struct syscall_args *args);
s64 sys_timerfd_settime(struct syscall_args *args);
s64 sys_timerfd_gettime(struct syscall_args *args);
s64 sys_getrandom(struct syscall_args *args);
s64 sys_memfd_create(struct syscall_args *args);
s64 sys_copy_file_range(struct syscall_args *args);
s64 sys_statfs(struct syscall_args *args);
s64 sys_fstatfs(struct syscall_args *args);
s64 sys_getxattr(struct syscall_args *args);
s64 sys_lgetxattr(struct syscall_args *args);
s64 sys_fgetxattr(struct syscall_args *args);
s64 sys_listxattr(struct syscall_args *args);
s64 sys_llistxattr(struct syscall_args *args);
s64 sys_flistxattr(struct syscall_args *args);
s64 sys_removexattr(struct syscall_args *args);
s64 sys_lremovexattr(struct syscall_args *args);
s64 sys_fremovexattr(struct syscall_args *args);
s64 sys_setxattr(struct syscall_args *args);
s64 sys_lsetxattr(struct syscall_args *args);
s64 sys_fsetxattr(struct syscall_args *args);
s64 sys_acct(struct syscall_args *args);
s64 sys_capget(struct syscall_args *args);
s64 sys_capset(struct syscall_args *args);
s64 sys_personality(struct syscall_args *args);
s64 sys_reboot(struct syscall_args *args);
s64 sys_sethostname(struct syscall_args *args);
s64 sys_setdomainname(struct syscall_args *args);
s64 sys_ioperm(struct syscall_args *args);
s64 sys_iopl(struct syscall_args *args);
s64 sys_init_module(struct syscall_args *args);
s64 sys_delete_module(struct syscall_args *args);
s64 sys_quotactl(struct syscall_args *args);
s64 sys_nfsservctl(struct syscall_args *args);
s64 sys_lookup_dcookie(struct syscall_args *args);
s64 sys_perf_event_open(struct syscall_args *args);
s64 sys_seccomp(struct syscall_args *args);
s64 sys_bpf(struct syscall_args *args);

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
void syscall_exit(struct syscall_args *args, s64 ret, u64 duration_ns);

/* Syscall Statistics */
void syscall_stats_print(void);
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
