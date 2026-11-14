/*
 * NEXUS OS - System call numbers
 * kernel/include/nexus_syscall_numbers.h
 *
 * Single source of truth for syscall numbering, shared by the kernel
 * dispatch table and userspace libsys.
 *
 * Before this existed the two sides disagreed. libsys carried its own
 * copy of the numbers using Linux x86_64 values (SYS_WRITE = 4), while the
 * kernel table used a numbering of its own (SYS_EXIT = 4, SYS_WRITE = 104).
 * A userspace write() therefore invoked the kernel's exit handler. Both
 * files compiled, and nothing caught it because neither user program nor
 * syscall path had ever run.
 *
 * Do not renumber anything in this file: the kernel table is indexed by
 * these values, and every built user program encodes them. Gaps are free
 * slots, not errors.
 */

#ifndef _NEXUS_SYSCALL_NUMBERS_H
#define _NEXUS_SYSCALL_NUMBERS_H

#define NR_SYSCALLS 256

/* ---- calls the kernel dispatch table implements ---- */
#define SYS_FORK                     0
#define SYS_VFORK                    1
#define SYS_CLONE                    2
#define SYS_EXECVE                   3
#define SYS_EXIT                     4
#define SYS_EXIT_GROUP               5
#define SYS_WAIT4                    6
#define SYS_WAITPID                  7
#define SYS_GETPID                   8
#define SYS_GETPPID                  9
#define SYS_GETTID                   10
#define SYS_GETUID                   11
#define SYS_GETEUID                  12
#define SYS_GETGID                   13
#define SYS_GETEGID                  14
#define SYS_SETUID                   15
#define SYS_SETEUID                  16
#define SYS_SETGID                   17
#define SYS_SETEGID                  18
#define SYS_SETPGID                  19
#define SYS_GETPGID                  20
#define SYS_SETSID                   21
#define SYS_GETSID                   22
#define SYS_GETPGRP                  23
#define SYS_SETREUID                 24
#define SYS_SETREGID                 25
#define SYS_GETGROUPS                26
#define SYS_SETGROUPS                27
#define SYS_SETPRIORITY              28
#define SYS_GETPRIORITY              29
#define SYS_NICE                     30
#define SYS_PRCTL                    31
#define SYS_ARCH_PRCTL               32
#define SYS_PTRACE                   33
#define SYS_MMAP                     50
#define SYS_MUNMAP                   51
#define SYS_MPROTECT                 52
#define SYS_MREMAP                   53
#define SYS_MSYNC                    54
#define SYS_MLOCK                    55
#define SYS_MUNLOCK                  56
#define SYS_MLOCKALL                 57
#define SYS_MUNLOCKALL               58
#define SYS_MINCORE                  59
#define SYS_MADVISE                  60
#define SYS_BRK                      61
#define SYS_SBRK                     62
#define SYS_MMAP2                    63
#define SYS_REMAP_FILE_PAGES         64
#define SYS_OPEN                     100
#define SYS_OPENAT                   101
#define SYS_CLOSE                    102
#define SYS_READ                     103
#define SYS_WRITE                    104
#define SYS_LSEEK                    105
#define SYS_PREAD64                  106
#define SYS_PWRITE64                 107
#define SYS_READV                    108
#define SYS_WRITEV                   109
#define SYS_PREADV                   110
#define SYS_PWRITEV                  111
#define SYS_DUP                      112
#define SYS_DUP2                     113
#define SYS_DUP3                     114
#define SYS_FCNTL                    115
#define SYS_IOCTL                    116
#define SYS_ACCESS                   117
#define SYS_FACCESSAT                118
#define SYS_STAT                     119
#define SYS_LSTAT                    120
#define SYS_FSTAT                    121
#define SYS_STATX                    122
#define SYS_GETDENTS                 123
#define SYS_GETDENTS64               124
#define SYS_READLINK                 125
#define SYS_READLINKAT               126
#define SYS_SYMLINK                  127
#define SYS_SYMLINKAT                128
#define SYS_LINK                     129
#define SYS_LINKAT                   130
#define SYS_UNLINK                   131
#define SYS_UNLINKAT                 132
#define SYS_RENAME                   133
#define SYS_RENAMEAT                 134
#define SYS_MKDIR                    135
#define SYS_MKDIRAT                  136
#define SYS_RMDIR                    137
#define SYS_CHMOD                    138
#define SYS_FCHMOD                   139
#define SYS_CHMODAT                  140
#define SYS_CHOWN                    141
#define SYS_FCHOWN                   142
#define SYS_LCHOWN                   143
#define SYS_TRUNCATE                 144
#define SYS_FTRUNCATE                145
#define SYS_FALLOCATE                146
#define SYS_FSYNC                    147
#define SYS_FDATASYNC                148
#define SYS_SYNC                     149
#define SYS_PIPE                     150
#define SYS_PIPE2                    151
#define SYS_SHMGET                   152
#define SYS_SHMAT                    153
#define SYS_SHMDT                    154
#define SYS_SHMCTL                   155
#define SYS_SEMGET                   156
#define SYS_SEMOP                    157
#define SYS_SEMTIMEDOP               158
#define SYS_SEMCTL                   159
#define SYS_MSGGET                   160
#define SYS_MSGSND                   161
#define SYS_MSGRCV                   162
#define SYS_MSGCTL                   163
#define SYS_SOCKET                   164
#define SYS_CONNECT                  165
#define SYS_ACCEPT                   166
#define SYS_BIND                     167
#define SYS_LISTEN                   168
#define SYS_SENDTO                   169
#define SYS_RECVFROM                 170
#define SYS_SENDMSG                  171
#define SYS_RECVMSG                  172
#define SYS_SHUTDOWN                 173
#define SYS_GETSOCKOPT               174
#define SYS_SETSOCKOPT               175
#define SYS_GETSOCKNAME              176
#define SYS_GETPEERNAME              177
#define SYS_SOCKETPAIR               178
#define SYS_EVENTFD                  179
#define SYS_SIGNALFD                 180
#define SYS_TIMERFD_CREATE           181
#define SYS_TIMERFD_SETTIME          182
#define SYS_TIMERFD_GETTIME          183
#define SYS_EPOLL_CREATE             184
#define SYS_EPOLL_CREATE1            185
#define SYS_EPOLL_CTL                186
#define SYS_EPOLL_WAIT               187
#define SYS_EPOLL_PWAIT              188
#define SYS_POLL                     189
#define SYS_PPOLL                    190
#define SYS_SELECT                   191
#define SYS_PSELECT6                 192
#define SYS_SCHED_YIELD              200
#define SYS_SCHED_SETAFFINITY        201
#define SYS_SCHED_GETAFFINITY        202
#define SYS_SCHED_SETATTR            203
#define SYS_SCHED_GETATTR            204
#define SYS_SCHED_SETPARAM           205
#define SYS_SCHED_GETPARAM           206
#define SYS_SCHED_SET_SCHEDULER      207
#define SYS_SCHED_GET_SCHEDULER      208
#define SYS_SCHED_GET_PRIORITY_MAX   209
#define SYS_SCHED_GET_PRIORITY_MIN   210
#define SYS_SCHED_RR_GET_INTERVAL    211
#define SYS_SCHED_GET_PRIORITY_CLASS 212
#define SYS_SCHED_SET_PRIORITY_CLASS 213
#define SYS_NANOSLEEP                214
#define SYS_CLOCK_NANOSLEEP          215
#define SYS_CLOCK_GETTIME            216
#define SYS_CLOCK_SETTIME            217
#define SYS_CLOCK_GETRES             218
#define SYS_CLOCK_GETCPUCLOCKID      219
#define SYS_GETRUSAGE                220
#define SYS_TIMES                    221
#define SYS_GETITIMER                222
#define SYS_SETITIMER                223
#define SYS_ALARM                    224
#define SYS_GETTIMEOFDAY             225
#define SYS_SETTIMEOFDAY             226
#define SYS_GETTIME                  227
#define SYS_SETTIME                  228
#define SYS_ADJTIMEX                 229
#define SYS_STIME                    230
#define SYS_TIME                     231
#define SYS_UTIME                    232
#define SYS_UTIMES                   233
#define SYS_FUTIMESAT                234
#define SYS_UTIMENSAT                235
#define SYS_CLOCK_ADJTIME            236
#define SYS_TIMER_CREATE             237
#define SYS_TIMER_SETTIME            238
#define SYS_TIMER_GETTIME            239
#define SYS_SIGNAL                   240
#define SYS_SIGACTION                241
#define SYS_SIGPROCMASK              242
#define SYS_SIGPENDING               243
#define SYS_SIGSUSPEND               244
#define SYS_SIGRETURN                245
#define SYS_RT_SIGACTION             246
#define SYS_RT_SIGPROCMASK           247
#define SYS_RT_SIGPENDING            248
#define SYS_RT_SIGSUSPEND            249
#define SYS_RT_SIGRETURN             250
#define SYS_RT_SIGTIMEDWAIT          251
#define SYS_RT_SIGQUEUEINFO          252
#define SYS_KILL                     253
#define SYS_TKILL                    254
#define SYS_TGKILL                   255

/* ---- calls libsys exposes; reserved slots, no kernel handler yet ---- */
#define SYS_FLOCK                    34
#define SYS_GETCWD                   35
#define SYS_CHDIR                    36
#define SYS_FCHDIR                   37
#define SYS_CREAT                    38
#define SYS_UMASK                    39
#define SYS_PREAD                    40
#define SYS_PWRITE                   41
#define SYS_SETRESUID                42
#define SYS_GETRESUID                43
#define SYS_SETRESGID                44
#define SYS_GETRESGID                45
#define SYS_SEND                     46
#define SYS_RECV                     47
#define SYS_SENDMMSG                 48
#define SYS_PAUSE                    49
#define SYS_SIGALTSTACK              65
#define SYS_GETXATTR                 66
#define SYS_SETXATTR                 67
#define SYS_LISTXATTR                68
#define SYS_REMOVEXATTR              69
#define SYS_YIELD                    70
#define SYS_SYSINFO                  71
#define SYS_UNAME                    72
#define SYS_GETRLIMIT                73
#define SYS_SETRLIMIT                74
#define SYS_CAPGET                   75
#define SYS_CAPSET                   76
#define SYS_REBOOT                   77
#define SYS_MOUNT                    78
#define SYS_UMOUNT2                  79
#define SYS_SWAPON                   80
#define SYS_SWAPOFF                  81
#define SYS_INIT_MODULE              82
#define SYS_DELETE_MODULE            83
#define SYS_QUERY_MODULE             84
#define SYS_NEXUS_CREATE_PROCESS     85
#define SYS_NEXUS_DESTROY_PROCESS    86
#define SYS_NEXUS_CREATE_THREAD      87
#define SYS_NEXUS_DESTROY_THREAD     88
#define SYS_NEXUS_SEND_MESSAGE       89
#define SYS_NEXUS_RECV_MESSAGE       90
#define SYS_NEXUS_CREATE_SERVICE     91
#define SYS_NEXUS_START_SERVICE      92
#define SYS_NEXUS_STOP_SERVICE       93
#define SYS_NEXUS_QUERY_SERVICE      94

/*
 * mknod is used by userspace init to create the /dev nodes it opens, so it
 * needs a number even though no kernel handler is registered for it yet. The
 * dispatcher answers ENOSYS for unregistered slots, which is the honest
 * response and is what init already has to tolerate: device nodes are created
 * by the kernel from its own table, not on request.
 */
#define SYS_MKNOD                    95

/*
 * getcmdline - copy this boot's kernel command line into a user buffer.
 *
 * Needed by userspace init: it parses the same option string the kernel parsed
 * (quiet, splash, virt=, terminal, ...), and until this call existed it had no
 * way to obtain it.
 */
#define SYS_GET_CMDLINE              96

#endif /* _NEXUS_SYSCALL_NUMBERS_H */
