/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2024 NEXUS Development Team
 *
 * syscall_wrapper.c - System Call Wrappers
 *
 * This module provides user-space wrappers for kernel system calls.
 * It handles the low-level syscall interface and provides clean
 * C function interfaces for applications.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Basic types */
typedef int64_t ssize_t;
typedef uint64_t size_t;
typedef int32_t pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef int64_t off_t;
typedef uint64_t ino_t;
typedef uint64_t dev_t;
typedef uint32_t mode_t;
typedef int64_t time_t;
typedef int64_t clock_t;

/* File descriptor type */
typedef int fd_t;

/* Timeval structure */
struct timeval {
    time_t tv_sec;
    long tv_usec;
};

/* Timespec structure */
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

/* Stat structure */
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    uint32_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    long st_blksize;
    long st_blocks;
    struct timespec st_atime;
    struct timespec st_mtime;
    struct timespec st_ctime;
};

/* Directory entry structure */
struct dirent {
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

/* Process information */
struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};

/* File lock structure */
struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

/* Poll file descriptor */
struct pollfd {
    int fd;
    short events;
    short revents;
};

/* Select fd_set */
#define FD_SETSIZE 1024
typedef struct {
    unsigned long fds_bits[FD_SETSIZE / 64];
} fd_set;

/* Wait status */
typedef int wait_status_t;

/* Signal set */
typedef struct {
    unsigned long sig[64 / sizeof(unsigned long)];
} sigset_t;

/* Signal action */
struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, void *, void *);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

/* IPC structures */
struct ipc_perm {
    key_t key;
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    mode_t mode;
    unsigned short seq;
};

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t shm_segsz;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
    pid_t shm_cpid;
    pid_t shm_lpid;
    unsigned long shm_nattch;
};

/*===========================================================================*/
/*                         SYSCALL NUMBERS                                   */
/*===========================================================================*/

/* File operations */
#define SYS_OPEN        1
#define SYS_CLOSE       2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_LSEEK       5
#define SYS_STAT        6
#define SYS_FSTAT       7
#define SYS_LSTAT       8
#define SYS_POLL        9
#define SYS_ACCESS      10
#define SYS_PIPE        11
#define SYS_DUP         12
#define SYS_DUP2        13
#define SYS_FCNTL       14
#define SYS_FLOCK       15
#define SYS_FSYNC       16
#define SYS_FDATASYNC   17
#define SYS_TRUNCATE    18
#define SYS_FTRUNCATE   19
#define SYS_GETDENTS    20
#define SYS_GETCWD      21
#define SYS_CHDIR       22
#define SYS_FCHDIR      23
#define SYS_RENAME      24
#define SYS_MKDIR       25
#define SYS_RMDIR       26
#define SYS_CREAT       27
#define SYS_LINK        28
#define SYS_UNLINK      29
#define SYS_SYMLINK     30
#define SYS_READLINK    31
#define SYS_CHMOD       32
#define SYS_FCHMOD      33
#define SYS_CHOWN       34
#define SYS_FCHOWN      35
#define SYS_LCHOWN      36
#define SYS_UMASK       37
#define SYS_READV       38
#define SYS_WRITEV      39
#define SYS_PREAD       40
#define SYS_PWRITE      41

/* Process operations */
#define SYS_EXIT        50
#define SYS_EXIT_GROUP  51
#define SYS_FORK        52
#define SYS_VFORK       53
#define SYS_CLONE       54
#define SYS_EXECVE      55
#define SYS_WAIT4       56
#define SYS_KILL        57
#define SYS_GETPID      58
#define SYS_GETPPID     59
#define SYS_GETUID      60
#define SYS_GETEUID     61
#define SYS_GETGID      62
#define SYS_GETEGID     63
#define SYS_SETUID      64
#define SYS_SETEUID     65
#define SYS_SETGID      66
#define SYS_SETEGID     67
#define SYS_SETREUID    68
#define SYS_SETREGID    69
#define SYS_GETGROUPS   70
#define SYS_SETGROUPS   71
#define SYS_SETRESUID   72
#define SYS_GETRESUID   73
#define SYS_SETRESGID   74
#define SYS_GETRESGID   75
#define SYS_GETPGID     76
#define SYS_SETPGID     77
#define SYS_GETSID      78
#define SYS_SETSID      79
#define SYS_SETPRIORITY 80
#define SYS_GETPRIORITY 81
#define SYS_GETRUSAGE   82
#define SYS_TIMES       83
#define SYS_PTRACE      84

/* Memory operations */
#define SYS_BRK         90
#define SYS_MMAP        91
#define SYS_MUNMAP      92
#define SYS_MPROTECT    93
#define SYS_MREMAP      94
#define SYS_MADVISE     95
#define SYS_MLOCK       96
#define SYS_MUNLOCK     97
#define SYS_MLOCKALL    98
#define SYS_MUNLOCKALL  99

/* IPC operations */
#define SYS_SHMGET      100
#define SYS_SHMAT       101
#define SYS_SHMCTL      102
#define SYS_SHMDT       103
#define SYS_MSGGET      104
#define SYS_MSGSND      105
#define SYS_MSGRCV      106
#define SYS_MSGCTL      107
#define SYS_SEMGET      108
#define SYS_SEMOP       109
#define SYS_SEMCTL      110

/* Socket operations */
#define SYS_SOCKET      120
#define SYS_BIND        121
#define SYS_CONNECT     122
#define SYS_LISTEN      123
#define SYS_ACCEPT      124
#define SYS_GETSOCKNAME 125
#define SYS_GETPEERNAME 126
#define SYS_SOCKETPAIR  127
#define SYS_SEND        128
#define SYS_RECV        129
#define SYS_SENDTO      130
#define SYS_RECVFROM    131
#define SYS_SHUTDOWN    132
#define SYS_SETSOCKOPT  133
#define SYS_GETSOCKOPT  134
#define SYS_SENDMSG     135
#define SYS_RECVMSG     136
#define SYS_SENDMMSG    137

/* Time operations */
#define SYS_GETTIMEOFDAY 140
#define SYS_SETTIMEOFDAY 141
#define SYS_GETTIME     142
#define SYS_SETTIME     143
#define SYS_NANOSLEEP   144
#define SYS_CLOCK_GETTIME 145
#define SYS_CLOCK_SETTIME 146
#define SYS_CLOCK_GETRES 147
#define SYS_CLOCK_NANOSLEEP 148
#define SYS_ALARM       149
#define SYS_PAUSE       150

/* Signal operations */
#define SYS_RT_SIGACTION 160
#define SYS_RT_SIGPROCMASK 161
#define SYS_RT_SIGRETURN 162
#define SYS_RT_SIGPENDING 163
#define SYS_RT_SIGTIMEDWAIT 164
#define SYS_RT_SIGQUEUEINFO 165
#define SYS_RT_SIGSUSPEND 166
#define SYS_SIGALTSTACK 167
#define SYS_TGKILL      168
#define SYS_TKILL       169

/* Misc operations */
#define SYS_IOCTL       170
#define SYS_GETXATTR    171
#define SYS_SETXATTR    172
#define SYS_LISTXATTR   173
#define SYS_REMOVEXATTR 174
#define SYS_GETTID      180
#define SYS_YIELD       181
#define SYS_SCHED_GETAFFINITY 182
#define SYS_SCHED_SETAFFINITY 183
#define SYS_SYSINFO     184
#define SYS_UNAME       185
#define SYS_GETRLIMIT   186
#define SYS_SETRLIMIT   187
#define SYS_PRCTL       188
#define SYS_ARCH_PRCTL  189
#define SYS_CAPGET      190
#define SYS_CAPSET      191
#define SYS_REBOOT      192
#define SYS_MOUNT       193
#define SYS_UMOUNT2     194
#define SYS_SWAPON      195
#define SYS_SWAPOFF     196
#define SYS_INIT_MODULE 197
#define SYS_DELETE_MODULE 198
#define SYS_QUERY_MODULE 199

/* NEXUS OS specific */
#define SYS_NEXUS_CREATE_PROCESS 200
#define SYS_NEXUS_DESTROY_PROCESS 201
#define SYS_NEXUS_CREATE_THREAD 202
#define SYS_NEXUS_DESTROY_THREAD 203
#define SYS_NEXUS_SEND_MESSAGE 204
#define SYS_NEXUS_RECV_MESSAGE 205
#define SYS_NEXUS_CREATE_SERVICE 206
#define SYS_NEXUS_START_SERVICE 207
#define SYS_NEXUS_STOP_SERVICE 208
#define SYS_NEXUS_QUERY_SERVICE 209

/* Error codes */
#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define ENOTBLK         15
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define ETXTBSY         26
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         35
#define ENAMETOOLONG    36
#define ENOLCK          37
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define EWOULDBLOCK     EAGAIN
#define ENOMSG          42
#define EIDRM           43
#define ECHRNG          44
#define EL2NSYNC        45
#define EL3HLT          46
#define EL3RST          47
#define ELNRNG          48
#define EUNATCH         49
#define ENOCSI          50
#define EL2HLT          51
#define EBADE           52
#define EBADR           53
#define EXFULL          54
#define ENOANO          55
#define EBADRQC         56
#define EBADSLT         57
#define EDEADLOCK       EDEADLK
#define EBFONT          59
#define ENOSTR          60
#define ENODATA         61
#define ETIME           62
#define ENOSR           63
#define ENONET          64
#define ENOPKG          65
#define EREMOTE         66
#define ENOLINK         67
#define EADV            68
#define ESRMNT          69
#define ECOMM           70
#define EPROTO          71
#define EMULTIHOP       72
#define EDOTDOT         73
#define EBADMSG         74
#define EOVERFLOW       75
#define ENOTUNIQ        76
#define EBADFD          77
#define EREMCHG         78
#define ELIBACC         79
#define ELIBBAD         80
#define ELIBSCN         81
#define ELIBMAX         82
#define ELIBEXEC        83
#define EILSEQ          84
#define ERESTART        85
#define ESTRPIPE        86
#define EUSERS          87
#define ENOTSOCK        88
#define EDESTADDRREQ    89
#define EMSGSIZE        90
#define EPROTOTYPE      91
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define ESOCKTNOSUPPORT 94
#define EOPNOTSUPP      95
#define EPFNOSUPPORT    96
#define EAFNOSUPPORT    97
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99
#define ENETDOWN        100
#define ENETUNREACH     101
#define ENETRESET       102
#define ECONNABORTED    103
#define ECONNRESET      104
#define ENOBUFS         105
#define EISCONN         106
#define ENOTCONN        107
#define ESHUTDOWN       108
#define ETOOMANYREFS    109
#define ETIMEDOUT       110
#define ECONNREFUSED    111
#define EHOSTDOWN       112
#define EHOSTUNREACH    113
#define EALREADY        114
#define EINPROGRESS     115
#define ESTALE          116
#define EUCLEAN         117
#define ENOTNAM         118
#define ENAVAIL         119
#define EISNAM          120
#define EREMOTEIO       121
#define EDQUOT          122
#define ENOMEDIUM       123
#define EMEDIUMTYPE     124
#define ECANCELED       125
#define ENOKEY          126
#define EKEYEXPIRED     127
#define EKEYREVOKED     128
#define EKEYREJECTED    129
#define EOWNERDEAD      130
#define ENOTRECOVERABLE 131
#define ERFKILL         132
#define EHWPOISON       133

/* File access modes */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_CREAT         0x0040
#define O_EXCL          0x0080
#define O_NOCTTY        0x0100
#define O_TRUNC         0x0200
#define O_APPEND        0x0400
#define O_NONBLOCK      0x0800
#define O_DSYNC         0x1000
#define O_SYNC          0x4000
#define O_RSYNC         0x4000
#define O_DIRECTORY     0x10000
#define O_NOFOLLOW      0x20000
#define O_CLOEXEC       0x80000

/* Seek modes */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* File types */
#define S_IFMT          0170000
#define S_IFSOCK        0140000
#define S_IFLNK         0120000
#define S_IFREG         0100000
#define S_IFBLK         0060000
#define S_IFDIR         0040000
#define S_IFCHR         0020000
#define S_IFIFO         0010000
#define S_ISUID         0004000
#define S_ISGID         0002000
#define S_ISVTX         0001000

/* File permissions */
#define S_IRWXU         00700
#define S_IRUSR         00400
#define S_IWUSR         00200
#define S_IXUSR         00100
#define S_IRWXG         00070
#define S_IRGRP         00040
#define S_IWGRP         00020
#define S_IXGRP         00010
#define S_IRWXO         00007
#define S_IROTH         00004
#define S_IWOTH         00002
#define S_IXOTH         00001

/* Exit codes */
#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

/* Wait options */
#define WNOHANG         1
#define WUNTRACED       2
#define WCONTINUED      8

/* Signal numbers */
#define SIGHUP          1
#define SIGINT          2
#define SIGQUIT         3
#define SIGILL          4
#define SIGTRAP         5
#define SIGABRT         6
#define SIGBUS          7
#define SIGFPE          8
#define SIGKILL         9
#define SIGUSR1         10
#define SIGSEGV         11
#define SIGUSR2         12
#define SIGPIPE         13
#define SIGALRM         14
#define SIGTERM         15
#define SIGCHLD         17
#define SIGCONT         18
#define SIGSTOP         19
#define SIGTSTP         20
#define SIGTTIN         21
#define SIGTTOU         22
#define SIGURG          23
#define SIGXCPU         24
#define SIGXFSZ         25
#define SIGVTALRM       26
#define SIGPROF         27
#define SIGWINCH        28
#define SIGIO           29
#define SIGPWR          30
#define SIGSYS          31
#define SIGRTMIN        34
#define SIGRTMAX        64

/* Mmap flags */
#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_FAILED      ((void *)-1)

/* Fcntl commands */
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9

/* Fcntl flags */
#define FD_CLOEXEC      1

/* Access modes */
#define F_OK            0
#define X_OK            1
#define W_OK            2
#define R_OK            4

/* Uname structure */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* Resource limits */
#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_RSS      5
#define RLIMIT_NPROC    6
#define RLIMIT_NOFILE   7
#define RLIMIT_MEMLOCK  8
#define RLIMIT_AS       9
#define RLIMIT_LOCKS    10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE     13
#define RLIMIT_RTPRIO   14
#define RLIMIT_RTTIME   15
#define RLIM_NLIMITS    16

#define RLIM_INFINITY   (~0ULL)
#define RLIM_SAVED_CUR  RLIM_INFINITY
#define RLIM_SAVED_MAX  RLIM_INFINITY

struct rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

/* Sysinfo structure */
struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

/* errno - Thread-local error number */
static __thread int errno = 0;

/*===========================================================================*/
/*                         SYSCALL MACRO                                     */
/*===========================================================================*/

/**
 * syscall0 - System call with no arguments
 */
static inline int64_t syscall0(uint64_t num)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall1 - System call with one argument
 */
static inline int64_t syscall1(uint64_t num, uint64_t arg1)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall2 - System call with two arguments
 */
static inline int64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall3 - System call with three arguments
 */
static inline int64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall4 - System call with four arguments
 */
static inline int64_t syscall4(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4)
{
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall5 - System call with five arguments
 */
static inline int64_t syscall5(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8 __asm__("r8") = arg5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * syscall6 - System call with six arguments
 */
static inline int64_t syscall6(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4, uint64_t arg5,
                                uint64_t arg6)
{
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8 __asm__("r8") = arg5;
    register uint64_t r9 __asm__("r9") = arg6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

/**
 * open - Open a file
 */
int open(const char *pathname, int flags, mode_t mode)
{
    int64_t ret = syscall3(SYS_OPEN, (uint64_t)pathname, (uint64_t)flags, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * close - Close a file descriptor
 */
int close(int fd)
{
    int64_t ret = syscall1(SYS_CLOSE, fd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * read - Read from a file descriptor
 */
ssize_t read(int fd, void *buf, size_t count)
{
    int64_t ret = syscall3(SYS_READ, fd, (uint64_t)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (ssize_t)ret;
}

/**
 * write - Write to a file descriptor
 */
ssize_t write(int fd, const void *buf, size_t count)
{
    int64_t ret = syscall3(SYS_WRITE, fd, (uint64_t)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (ssize_t)ret;
}

/**
 * lseek - Reposition read/write file offset
 */
off_t lseek(int fd, off_t offset, int whence)
{
    int64_t ret = syscall3(SYS_LSEEK, fd, offset, whence);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (off_t)ret;
}

/**
 * stat - Get file status
 */
int stat(const char *pathname, struct stat *statbuf)
{
    int64_t ret = syscall2(SYS_STAT, (uint64_t)pathname, (uint64_t)statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * fstat - Get file status by file descriptor
 */
int fstat(int fd, struct stat *statbuf)
{
    int64_t ret = syscall2(SYS_FSTAT, fd, (uint64_t)statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * lstat - Get file status (don't follow symlinks)
 */
int lstat(const char *pathname, struct stat *statbuf)
{
    int64_t ret = syscall2(SYS_LSTAT, (uint64_t)pathname, (uint64_t)statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * access - Check file accessibility
 */
int access(const char *pathname, int mode)
{
    int64_t ret = syscall2(SYS_ACCESS, (uint64_t)pathname, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * pipe - Create pipe
 */
int pipe(int pipefd[2])
{
    int64_t ret = syscall1(SYS_PIPE, (uint64_t)pipefd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * dup - Duplicate file descriptor
 */
int dup(int oldfd)
{
    int64_t ret = syscall1(SYS_DUP, oldfd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * dup2 - Duplicate file descriptor to specific number
 */
int dup2(int oldfd, int newfd)
{
    int64_t ret = syscall2(SYS_DUP2, oldfd, newfd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * fcntl - File control
 */
int fcntl(int fd, int cmd, void *arg)
{
    int64_t ret = syscall3(SYS_FCNTL, fd, cmd, (uint64_t)arg);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * fsync - Synchronize file to disk
 */
int fsync(int fd)
{
    int64_t ret = syscall1(SYS_FSYNC, fd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * fdatasync - Synchronize file data to disk
 */
int fdatasync(int fd)
{
    int64_t ret = syscall1(SYS_FDATASYNC, fd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * truncate - Truncate a file
 */
int truncate(const char *path, off_t length)
{
    int64_t ret = syscall2(SYS_TRUNCATE, (uint64_t)path, length);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * ftruncate - Truncate a file by descriptor
 */
int ftruncate(int fd, off_t length)
{
    int64_t ret = syscall2(SYS_FTRUNCATE, fd, length);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getcwd - Get current working directory
 */
char *getcwd(char *buf, size_t size)
{
    int64_t ret = syscall2(SYS_GETCWD, (uint64_t)buf, size);
    if (ret < 0) {
        errno = -ret;
        return NULL;
    }
    return buf;
}

/**
 * chdir - Change working directory
 */
int chdir(const char *path)
{
    int64_t ret = syscall1(SYS_CHDIR, (uint64_t)path);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * mkdir - Create directory
 */
int mkdir(const char *pathname, mode_t mode)
{
    int64_t ret = syscall2(SYS_MKDIR, (uint64_t)pathname, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * rmdir - Remove directory
 */
int rmdir(const char *pathname)
{
    int64_t ret = syscall1(SYS_RMDIR, (uint64_t)pathname);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * unlink - Delete file
 */
int unlink(const char *pathname)
{
    int64_t ret = syscall1(SYS_UNLINK, (uint64_t)pathname);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * rename - Rename file
 */
int rename(const char *oldpath, const char *newpath)
{
    int64_t ret = syscall2(SYS_RENAME, (uint64_t)oldpath, (uint64_t)newpath);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * chmod - Change file permissions
 */
int chmod(const char *pathname, mode_t mode)
{
    int64_t ret = syscall2(SYS_CHMOD, (uint64_t)pathname, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * fchmod - Change file permissions by descriptor
 */
int fchmod(int fd, mode_t mode)
{
    int64_t ret = syscall2(SYS_FCHMOD, fd, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * umask - Set file mode creation mask
 */
mode_t umask(mode_t mask)
{
    int64_t ret = syscall1(SYS_UMASK, mask);
    return (mode_t)ret;
}

/**
 * link - Create hard link
 */
int link(const char *oldpath, const char *newpath)
{
    int64_t ret = syscall2(SYS_LINK, (uint64_t)oldpath, (uint64_t)newpath);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * symlink - Create symbolic link
 */
int symlink(const char *target, const char *linkpath)
{
    int64_t ret = syscall2(SYS_SYMLINK, (uint64_t)target, (uint64_t)linkpath);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * readlink - Read symbolic link
 */
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
    int64_t ret = syscall3(SYS_READLINK, (uint64_t)pathname, (uint64_t)buf, bufsiz);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (ssize_t)ret;
}

/**
 * chown - Change file owner
 */
int chown(const char *pathname, uid_t owner, gid_t group)
{
    int64_t ret = syscall3(SYS_CHOWN, (uint64_t)pathname, owner, group);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/*===========================================================================*/
/*                         PROCESS OPERATIONS                                */
/*===========================================================================*/

/**
 * exit - Terminate process
 */
void exit(int status)
{
    syscall1(SYS_EXIT, status & 0xFF);
    for (;;);  /* Should never return */
}

/**
 * _exit - Terminate process (no cleanup)
 */
void _exit(int status)
{
    syscall1(SYS_EXIT, status & 0xFF);
    for (;;);  /* Should never return */
}

/**
 * fork - Create child process
 */
pid_t fork(void)
{
    int64_t ret = syscall0(SYS_FORK);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (pid_t)ret;
}

/**
 * execve - Execute program
 */
int execve(const char *filename, char *const argv[], char *const envp[])
{
    int64_t ret = syscall3(SYS_EXECVE, (uint64_t)filename,
                            (uint64_t)argv, (uint64_t)envp);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * wait4 - Wait for process to change state
 */
pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage)
{
    int64_t ret = syscall4(SYS_WAIT4, pid, (uint64_t)wstatus, options,
                            (uint64_t)rusage);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (pid_t)ret;
}

/**
 * waitpid - Wait for specific process
 */
pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    return wait4(pid, wstatus, options, NULL);
}

/**
 * wait - Wait for any child process
 */
pid_t wait(int *wstatus)
{
    return wait4(-1, wstatus, 0, NULL);
}

/**
 * kill - Send signal to process
 */
int kill(pid_t pid, int sig)
{
    int64_t ret = syscall2(SYS_KILL, pid, sig);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getpid - Get process ID
 */
pid_t getpid(void)
{
    return (pid_t)syscall0(SYS_GETPID);
}

/**
 * getppid - Get parent process ID
 */
pid_t getppid(void)
{
    return (pid_t)syscall0(SYS_GETPPID);
}

/**
 * getuid - Get user ID
 */
uid_t getuid(void)
{
    return (uid_t)syscall0(SYS_GETUID);
}

/**
 * geteuid - Get effective user ID
 */
uid_t geteuid(void)
{
    return (uid_t)syscall0(SYS_GETEUID);
}

/**
 * getgid - Get group ID
 */
gid_t getgid(void)
{
    return (gid_t)syscall0(SYS_GETGID);
}

/**
 * getegid - Get effective group ID
 */
gid_t getegid(void)
{
    return (gid_t)syscall0(SYS_GETEGID);
}

/**
 * setuid - Set user ID
 */
int setuid(uid_t uid)
{
    int64_t ret = syscall1(SYS_SETUID, uid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * setgid - Set group ID
 */
int setgid(gid_t gid)
{
    int64_t ret = syscall1(SYS_SETGID, gid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getpgid - Get process group ID
 */
pid_t getpgid(pid_t pid)
{
    int64_t ret = syscall1(SYS_GETPGID, pid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (pid_t)ret;
}

/**
 * setpgid - Set process group ID
 */
int setpgid(pid_t pid, pid_t pgid)
{
    int64_t ret = syscall2(SYS_SETPGID, pid, pgid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getsid - Get session ID
 */
pid_t getsid(pid_t pid)
{
    int64_t ret = syscall1(SYS_GETSID, pid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (pid_t)ret;
}

/**
 * setsid - Create new session
 */
pid_t setsid(void)
{
    int64_t ret = syscall0(SYS_SETSID);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (pid_t)ret;
}

/**
 * gettid - Get thread ID
 */
pid_t gettid(void)
{
    return (pid_t)syscall0(SYS_GETTID);
}

/**
 * sched_yield - Yield processor
 */
int sched_yield(void)
{
    return (int)syscall0(SYS_YIELD);
}

/*===========================================================================*/
/*                         MEMORY OPERATIONS                                 */
/*===========================================================================*/

/**
 * brk - Change data segment size
 */
void *brk(void *addr)
{
    return (void *)syscall1(SYS_BRK, (uint64_t)addr);
}

/**
 * sbrk - Change data segment size (incremental)
 */
void *sbrk(intptr_t increment)
{
    static void *current_brk = NULL;

    if (current_brk == NULL) {
        current_brk = brk(NULL);
    }

    void *old_brk = current_brk;

    if (increment != 0) {
        void *new_brk = brk((void *)((uintptr_t)current_brk + increment));
        if (new_brk == (void *)-1) {
            errno = ENOMEM;
            return (void *)-1;
        }
        current_brk = new_brk;
    }

    return old_brk;
}

/**
 * mmap - Map files or devices into memory
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    int64_t ret = syscall6(SYS_MMAP, (uint64_t)addr, length, prot, flags, fd, offset);
    if ((uint64_t)ret >= (uint64_t)-4095) {
        errno = -ret;
        return MAP_FAILED;
    }
    return (void *)ret;
}

/**
 * munmap - Unmap memory region
 */
int munmap(void *addr, size_t length)
{
    int64_t ret = syscall2(SYS_MUNMAP, (uint64_t)addr, length);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * mprotect - Set protection on memory region
 */
int mprotect(void *addr, size_t len, int prot)
{
    int64_t ret = syscall3(SYS_MPROTECT, (uint64_t)addr, len, prot);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * mremap - Remap memory region
 */
void *mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
    int64_t ret = syscall4(SYS_MREMAP, (uint64_t)old_address, old_size, new_size, flags);
    if ((uint64_t)ret >= (uint64_t)-4095) {
        errno = -ret;
        return MAP_FAILED;
    }
    return (void *)ret;
}

/**
 * madvise - Give advice about memory usage
 */
int madvise(void *addr, size_t length, int advice)
{
    int64_t ret = syscall3(SYS_MADVISE, (uint64_t)addr, length, advice);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/*===========================================================================*/
/*                         TIME OPERATIONS                                   */
/*===========================================================================*/

/**
 * gettimeofday - Get time of day
 */
int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;  /* Timezone is obsolete */
    int64_t ret = syscall1(SYS_GETTIMEOFDAY, (uint64_t)tv);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * nanosleep - High-resolution sleep
 */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
    int64_t ret = syscall2(SYS_NANOSLEEP, (uint64_t)req, (uint64_t)rem);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * clock_gettime - Get clock time
 */
int clock_gettime(int clock_id, struct timespec *tp)
{
    int64_t ret = syscall2(SYS_CLOCK_GETTIME, clock_id, (uint64_t)tp);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * alarm - Set alarm clock
 */
unsigned int alarm(unsigned int seconds)
{
    return (unsigned int)syscall1(SYS_ALARM, seconds);
}

/**
 * sleep - Sleep for specified seconds
 */
unsigned int sleep(unsigned int seconds)
{
    struct timespec req, rem;
    req.tv_sec = seconds;
    req.tv_nsec = 0;

    if (nanosleep(&req, &rem) == 0) {
        return 0;
    }

    if (errno == EINTR) {
        return rem.tv_sec + (rem.tv_nsec > 0 ? 1 : 0);
    }

    return seconds;
}

/**
 * usleep - Sleep for specified microseconds
 */
int usleep(unsigned long usec)
{
    struct timespec req;
    req.tv_sec = usec / 1000000;
    req.tv_nsec = (usec % 1000000) * 1000;

    return nanosleep(&req, NULL);
}

/*===========================================================================*/
/*                         SIGNAL OPERATIONS                                 */
/*===========================================================================*/

/**
 * sigaction - Examine and change signal action
 */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    int64_t ret = syscall3(SYS_RT_SIGACTION, signum, (uint64_t)act, (uint64_t)oldact);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * sigprocmask - Examine and change blocked signals
 */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    int64_t ret = syscall3(SYS_RT_SIGPROCMASK, how, (uint64_t)set, (uint64_t)oldset);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * signal - Set signal handler (simplified)
 */
void (*signal(int signum, void (*handler)(int)))(int)
{
    struct sigaction sa, old_sa;

    sa.sa_handler = handler;
    sa.sa_flags = 0;

    if (sigaction(signum, &sa, &old_sa) < 0) {
        return (void (*)(int))-1;
    }

    return old_sa.sa_handler;
}

/**
 * raise - Send signal to current process
 */
int raise(int sig)
{
    return kill(getpid(), sig);
}

/*===========================================================================*/
/*                         MISC OPERATIONS                                   */
/*===========================================================================*/

/**
 * ioctl - Device control
 */
int ioctl(int fd, unsigned long request, void *arg)
{
    int64_t ret = syscall3(SYS_IOCTL, fd, request, (uint64_t)arg);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * uname - Get system information
 */
int uname(struct utsname *buf)
{
    int64_t ret = syscall1(SYS_UNAME, (uint64_t)buf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * sysinfo - Get system information
 */
int sysinfo(struct sysinfo *info)
{
    int64_t ret = syscall1(SYS_SYSINFO, (uint64_t)info);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getrlimit - Get resource limits
 */
int getrlimit(int resource, struct rlimit *rlim)
{
    int64_t ret = syscall2(SYS_GETRLIMIT, resource, (uint64_t)rlim);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * setrlimit - Set resource limits
 */
int setrlimit(int resource, const struct rlimit *rlim)
{
    int64_t ret = syscall2(SYS_SETRLIMIT, resource, (uint64_t)rlim);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * prctl - Process control
 */
int prctl(int option, unsigned long arg2, unsigned long arg3,
          unsigned long arg4, unsigned long arg5)
{
    int64_t ret = syscall5(SYS_PRCTL, option, arg2, arg3, arg4, arg5);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * reboot - Reboot system
 */
int reboot(int magic)
{
    int64_t ret = syscall1(SYS_REBOOT, magic);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * mount - Mount filesystem
 */
int mount(const char *source, const char *target, const char *filesystemtype,
          unsigned long mountflags, const void *data)
{
    int64_t ret = syscall5(SYS_MOUNT, (uint64_t)source, (uint64_t)target,
                            (uint64_t)filesystemtype, mountflags, (uint64_t)data);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * umount2 - Unmount filesystem
 */
int umount2(const char *target, int flags)
{
    int64_t ret = syscall2(SYS_UMOUNT2, (uint64_t)target, flags);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * getdents - Get directory entries
 */
int getdents(int fd, struct dirent *dirp, unsigned int count)
{
    int64_t ret = syscall3(SYS_GETDENTS, fd, (uint64_t)dirp, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * poll - Wait for events on file descriptors
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int64_t ret = syscall3(SYS_POLL, (uint64_t)fds, nfds, timeout);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * select - Synchronous I/O multiplexing
 */
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout)
{
    int64_t ret = syscall5(SYS_POLL, nfds, (uint64_t)readfds,
                            (uint64_t)writefds, (uint64_t)exceptfds,
                            (uint64_t)timeout);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/*===========================================================================*/
/*                         NEXUS OS SPECIFIC                                 */
/*===========================================================================*/

/**
 * nexus_create_process - Create a new process (NEXUS specific)
 */
int nexus_create_process(const char *path, char *const argv[])
{
    int64_t ret = syscall2(SYS_NEXUS_CREATE_PROCESS, (uint64_t)path, (uint64_t)argv);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_destroy_process - Destroy a process (NEXUS specific)
 */
int nexus_destroy_process(pid_t pid)
{
    int64_t ret = syscall1(SYS_NEXUS_DESTROY_PROCESS, pid);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

/**
 * nexus_create_thread - Create a new thread (NEXUS specific)
 */
int nexus_create_thread(void *(*start_routine)(void *), void *arg)
{
    int64_t ret = syscall2(SYS_NEXUS_CREATE_THREAD, (uint64_t)start_routine, (uint64_t)arg);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_send_message - Send IPC message (NEXUS specific)
 */
int nexus_send_message(pid_t dest, void *msg, size_t size)
{
    int64_t ret = syscall3(SYS_NEXUS_SEND_MESSAGE, dest, (uint64_t)msg, size);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_recv_message - Receive IPC message (NEXUS specific)
 */
int nexus_recv_message(pid_t *src, void *msg, size_t size)
{
    int64_t ret = syscall3(SYS_NEXUS_RECV_MESSAGE, (uint64_t)src, (uint64_t)msg, size);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_create_service - Create a service (NEXUS specific)
 */
int nexus_create_service(const char *name, const char *path)
{
    int64_t ret = syscall2(SYS_NEXUS_CREATE_SERVICE, (uint64_t)name, (uint64_t)path);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_start_service - Start a service (NEXUS specific)
 */
int nexus_start_service(const char *name)
{
    int64_t ret = syscall1(SYS_NEXUS_START_SERVICE, (uint64_t)name);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_stop_service - Stop a service (NEXUS specific)
 */
int nexus_stop_service(const char *name)
{
    int64_t ret = syscall1(SYS_NEXUS_STOP_SERVICE, (uint64_t)name);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/**
 * nexus_query_service - Query service status (NEXUS specific)
 */
int nexus_query_service(const char *name, void *status)
{
    int64_t ret = syscall2(SYS_NEXUS_QUERY_SERVICE, (uint64_t)name, (uint64_t)status);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

/*===========================================================================*/
/*                         ERROR HANDLING                                    */
/*===========================================================================*/

/**
 * strerror - Get error message string
 */
char *strerror(int errnum)
{
    static const char *error_messages[] = {
        "Success",                  /* 0 */
        "Operation not permitted",  /* 1 */
        "No such file or directory",/* 2 */
        "No such process",          /* 3 */
        "Interrupted system call",  /* 4 */
        "I/O error",                /* 5 */
        "No such device or address",/* 6 */
        "Argument list too long",   /* 7 */
        "Exec format error",        /* 8 */
        "Bad file number",          /* 9 */
        "No child processes",       /* 10 */
        "Try again",                /* 11 */
        "Out of memory",            /* 12 */
        "Permission denied",        /* 13 */
        "Bad address",              /* 14 */
        /* ... more error messages ... */
    };

    if (errnum < 0 || errnum >= (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return "Unknown error";
    }

    return (char *)error_messages[errnum];
}

/**
 * perror - Print error message
 */
void perror(const char *s)
{
    if (s && *s) {
        write(STDERR_FILENO, s, strlen(s));
        write(STDERR_FILENO, ": ", 2);
    }
    write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
    write(STDERR_FILENO, "\n", 1);
}
