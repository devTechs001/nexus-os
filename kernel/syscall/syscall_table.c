/*
 * NEXUS OS - System Call Table
 * kernel/syscall/syscall_table.c
 *
 * System call table containing all syscall function pointers
 * and metadata for dispatch.
 */

#include "syscall.h"

/* Forward declarations for missing syscalls */
long sys_semtimedop(struct syscall_args *args);
long sys_sched_setattr(struct syscall_args *args);
long sys_sched_getattr(struct syscall_args *args);
long sys_sched_setparam(struct syscall_args *args);
long sys_sched_getparam(struct syscall_args *args);
long sys_clock_getcpuclockid(struct syscall_args *args);
long sys_alarm(struct syscall_args *args);
long sys_signal(struct syscall_args *args);
long sys_sigaction(struct syscall_args *args);
long sys_sigprocmask(struct syscall_args *args);

/*===========================================================================*/
/*                         SYSCALL TABLE                                     */
/*===========================================================================*/

/*
 * System call table. Each entry contains:
 * - handler: Function pointer to the syscall implementation
 * - name: Human-readable name for debugging
 * - flags: Handler flags (native, compat, etc.)
 * - nargs: Number of arguments
 *
 * Unimplemented syscalls have NULL handlers.
 */
struct syscall_entry syscall_table[NR_SYSCALLS] = {
    /*=======================================================================*/
    /* Process Management Syscalls (0-49)                                    */
    /*=======================================================================*/

    /* 0 */ [SYS_FORK] = {
        .handler = sys_fork,
        .name = "fork",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 1 */ [SYS_VFORK] = {
        .handler = sys_vfork,
        .name = "vfork",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 2 */ [SYS_CLONE] = {
        .handler = sys_clone,
        .name = "clone",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 3 */ [SYS_EXECVE] = {
        .handler = sys_execve,
        .name = "execve",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 4 */ [SYS_EXIT] = {
        .handler = sys_exit,
        .name = "exit",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 5 */ [SYS_EXIT_GROUP] = {
        .handler = sys_exit_group,
        .name = "exit_group",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 6 */ [SYS_WAIT4] = {
        .handler = sys_wait4,
        .name = "wait4",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 7 */ [SYS_WAITPID] = {
        .handler = sys_waitpid,
        .name = "waitpid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 8 */ [SYS_GETPID] = {
        .handler = sys_getpid,
        .name = "getpid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 9 */ [SYS_GETPPID] = {
        .handler = sys_getppid,
        .name = "getppid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 10 */ [SYS_GETTID] = {
        .handler = sys_gettid,
        .name = "gettid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 11 */ [SYS_GETUID] = {
        .handler = sys_getuid,
        .name = "getuid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 12 */ [SYS_GETEUID] = {
        .handler = sys_geteuid,
        .name = "geteuid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 13 */ [SYS_GETGID] = {
        .handler = sys_getgid,
        .name = "getgid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 14 */ [SYS_GETEGID] = {
        .handler = sys_getegid,
        .name = "getegid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 15 */ [SYS_SETUID] = {
        .handler = sys_setuid,
        .name = "setuid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 16 */ [SYS_SETEUID] = {
        .handler = sys_seteuid,
        .name = "seteuid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 17 */ [SYS_SETGID] = {
        .handler = sys_setgid,
        .name = "setgid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 18 */ [SYS_SETEGID] = {
        .handler = sys_setegid,
        .name = "setegid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 19 */ [SYS_SETPGID] = {
        .handler = sys_setpgid,
        .name = "setpgid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 20 */ [SYS_GETPGID] = {
        .handler = sys_getpgid,
        .name = "getpgid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 21 */ [SYS_SETSID] = {
        .handler = sys_setsid,
        .name = "setsid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 22 */ [SYS_GETSID] = {
        .handler = sys_getsid,
        .name = "getsid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 23 */ [SYS_GETPGRP] = {
        .handler = sys_getpgrp,
        .name = "getpgrp",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 24 */ [SYS_SETREUID] = {
        .handler = sys_setreuid,
        .name = "setreuid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 25 */ [SYS_SETREGID] = {
        .handler = sys_setregid,
        .name = "setregid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 26 */ [SYS_GETGROUPS] = {
        .handler = sys_getgroups,
        .name = "getgroups",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 27 */ [SYS_SETGROUPS] = {
        .handler = sys_setgroups,
        .name = "setgroups",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 28 */ [SYS_SETPRIORITY] = {
        .handler = sys_setpriority,
        .name = "setpriority",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 29 */ [SYS_GETPRIORITY] = {
        .handler = sys_getpriority,
        .name = "getpriority",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 30 */ [SYS_NICE] = {
        .handler = sys_nice,
        .name = "nice",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 31 */ [SYS_PRCTL] = {
        .handler = sys_prctl,
        .name = "prctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 32 */ [SYS_ARCH_PRCTL] = {
        .handler = sys_arch_prctl,
        .name = "arch_prctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 33 */ [SYS_PTRACE] = {
        .handler = sys_ptrace,
        .name = "ptrace",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 34-49 */ /* Reserved for process management */

    /*=======================================================================*/
    /* Memory Management Syscalls (50-99)                                    */
    /*=======================================================================*/

    /* 50 */ [SYS_MMAP] = {
        .handler = sys_mmap,
        .name = "mmap",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 6
    },

    /* 51 */ [SYS_MUNMAP] = {
        .handler = sys_munmap,
        .name = "munmap",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 52 */ [SYS_MPROTECT] = {
        .handler = sys_mprotect,
        .name = "mprotect",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 53 */ [SYS_MREMAP] = {
        .handler = sys_mremap,
        .name = "mremap",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 54 */ [SYS_MSYNC] = {
        .handler = sys_msync,
        .name = "msync",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 55 */ [SYS_MLOCK] = {
        .handler = sys_mlock,
        .name = "mlock",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 56 */ [SYS_MUNLOCK] = {
        .handler = sys_munlock,
        .name = "munlock",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 57 */ [SYS_MLOCKALL] = {
        .handler = sys_mlockall,
        .name = "mlockall",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 58 */ [SYS_MUNLOCKALL] = {
        .handler = sys_munlockall,
        .name = "munlockall",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 59 */ [SYS_MINCORE] = {
        .handler = sys_mincore,
        .name = "mincore",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 60 */ [SYS_MADVISE] = {
        .handler = sys_madvise,
        .name = "madvise",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 61 */ [SYS_BRK] = {
        .handler = sys_brk,
        .name = "brk",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 62 */ [SYS_SBRK] = {
        .handler = sys_sbrk,
        .name = "sbrk",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 63 */ [SYS_MMAP2] = {
        .handler = sys_mmap2,
        .name = "mmap2",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 6
    },

    /* 64 */ [SYS_REMAP_FILE_PAGES] = {
        .handler = sys_remap_file_pages,
        .name = "remap_file_pages",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 65-99 */ /* Reserved for memory management */

    /*=======================================================================*/
    /* File Operations Syscalls (100-149)                                    */
    /*=======================================================================*/

    /* 100 */ [SYS_OPEN] = {
        .handler = sys_open,
        .name = "open",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 101 */ [SYS_OPENAT] = {
        .handler = sys_openat,
        .name = "openat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 102 */ [SYS_CLOSE] = {
        .handler = sys_close,
        .name = "close",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 103 */ [SYS_READ] = {
        .handler = sys_read,
        .name = "read",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 104 */ [SYS_WRITE] = {
        .handler = sys_write,
        .name = "write",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 105 */ [SYS_LSEEK] = {
        .handler = sys_lseek,
        .name = "lseek",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 106 */ [SYS_PREAD64] = {
        .handler = sys_pread64,
        .name = "pread64",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 107 */ [SYS_PWRITE64] = {
        .handler = sys_pwrite64,
        .name = "pwrite64",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 108 */ [SYS_READV] = {
        .handler = sys_readv,
        .name = "readv",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 109 */ [SYS_WRITEV] = {
        .handler = sys_writev,
        .name = "writev",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 110 */ [SYS_PREADV] = {
        .handler = sys_preadv,
        .name = "preadv",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 111 */ [SYS_PWRITEV] = {
        .handler = sys_pwritev,
        .name = "pwritev",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 112 */ [SYS_DUP] = {
        .handler = sys_dup,
        .name = "dup",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 113 */ [SYS_DUP2] = {
        .handler = sys_dup2,
        .name = "dup2",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 114 */ [SYS_DUP3] = {
        .handler = sys_dup3,
        .name = "dup3",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 115 */ [SYS_FCNTL] = {
        .handler = sys_fcntl,
        .name = "fcntl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 116 */ [SYS_IOCTL] = {
        .handler = sys_ioctl,
        .name = "ioctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 117 */ [SYS_ACCESS] = {
        .handler = sys_access,
        .name = "access",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 118 */ [SYS_FACCESSAT] = {
        .handler = sys_faccessat,
        .name = "faccessat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 119 */ [SYS_STAT] = {
        .handler = sys_stat,
        .name = "stat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 120 */ [SYS_LSTAT] = {
        .handler = sys_lstat,
        .name = "lstat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 121 */ [SYS_FSTAT] = {
        .handler = sys_fstat,
        .name = "fstat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 122 */ [SYS_STATX] = {
        .handler = sys_statx,
        .name = "statx",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 123 */ [SYS_GETDENTS] = {
        .handler = sys_getdents,
        .name = "getdents",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 124 */ [SYS_GETDENTS64] = {
        .handler = sys_getdents64,
        .name = "getdents64",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 125 */ [SYS_READLINK] = {
        .handler = sys_readlink,
        .name = "readlink",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 126 */ [SYS_READLINKAT] = {
        .handler = sys_readlinkat,
        .name = "readlinkat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 127 */ [SYS_SYMLINK] = {
        .handler = sys_symlink,
        .name = "symlink",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 128 */ [SYS_SYMLINKAT] = {
        .handler = sys_symlinkat,
        .name = "symlinkat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 129 */ [SYS_LINK] = {
        .handler = sys_link,
        .name = "link",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 130 */ [SYS_LINKAT] = {
        .handler = sys_linkat,
        .name = "linkat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 131 */ [SYS_UNLINK] = {
        .handler = sys_unlink,
        .name = "unlink",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 132 */ [SYS_UNLINKAT] = {
        .handler = sys_unlinkat,
        .name = "unlinkat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 133 */ [SYS_RENAME] = {
        .handler = sys_rename,
        .name = "rename",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 134 */ [SYS_RENAMEAT] = {
        .handler = sys_renameat,
        .name = "renameat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 135 */ [SYS_MKDIR] = {
        .handler = sys_mkdir,
        .name = "mkdir",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 136 */ [SYS_MKDIRAT] = {
        .handler = sys_mkdirat,
        .name = "mkdirat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 137 */ [SYS_RMDIR] = {
        .handler = sys_rmdir,
        .name = "rmdir",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 138 */ [SYS_CHMOD] = {
        .handler = sys_chmod,
        .name = "chmod",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 139 */ [SYS_FCHMOD] = {
        .handler = sys_fchmod,
        .name = "fchmod",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 140 */ [SYS_CHMODAT] = {
        .handler = sys_chmodat,
        .name = "chmodat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 141 */ [SYS_CHOWN] = {
        .handler = sys_chown,
        .name = "chown",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 142 */ [SYS_FCHOWN] = {
        .handler = sys_fchown,
        .name = "fchown",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 143 */ [SYS_LCHOWN] = {
        .handler = sys_lchown,
        .name = "lchown",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 144 */ [SYS_TRUNCATE] = {
        .handler = sys_truncate,
        .name = "truncate",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 145 */ [SYS_FTRUNCATE] = {
        .handler = sys_ftruncate,
        .name = "ftruncate",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 146 */ [SYS_FALLOCATE] = {
        .handler = sys_fallocate,
        .name = "fallocate",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 147 */ [SYS_FSYNC] = {
        .handler = sys_fsync,
        .name = "fsync",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 148 */ [SYS_FDATASYNC] = {
        .handler = sys_fdatasync,
        .name = "fdatasync",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 149 */ [SYS_SYNC] = {
        .handler = sys_sync,
        .name = "sync",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 150-149 */ /* Reserved for file operations */

    /*=======================================================================*/
    /* IPC Syscalls (150-199)                                                */
    /*=======================================================================*/

    /* 150 */ [SYS_PIPE] = {
        .handler = sys_pipe,
        .name = "pipe",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 151 */ [SYS_PIPE2] = {
        .handler = sys_pipe2,
        .name = "pipe2",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 152 */ [SYS_SHMGET] = {
        .handler = sys_shmget,
        .name = "shmget",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 153 */ [SYS_SHMAT] = {
        .handler = sys_shmat,
        .name = "shmat",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 154 */ [SYS_SHMDT] = {
        .handler = sys_shmdt,
        .name = "shmdt",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 155 */ [SYS_SHMCTL] = {
        .handler = sys_shmctl,
        .name = "shmctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 156 */ [SYS_SEMGET] = {
        .handler = sys_semget,
        .name = "semget",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 157 */ [SYS_SEMOP] = {
        .handler = sys_semop,
        .name = "semop",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 158 */ [SYS_SEMTIMEDOP] = {
        .handler = sys_semtimedop,
        .name = "semtimedop",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 159 */ [SYS_SEMCTL] = {
        .handler = sys_semctl,
        .name = "semctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 160 */ [SYS_MSGGET] = {
        .handler = sys_msgget,
        .name = "msgget",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 161 */ [SYS_MSGSND] = {
        .handler = sys_msgsnd,
        .name = "msgsnd",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 162 */ [SYS_MSGRCV] = {
        .handler = sys_msgrcv,
        .name = "msgrcv",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 5
    },

    /* 163 */ [SYS_MSGCTL] = {
        .handler = sys_msgctl,
        .name = "msgctl",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 164-199 */ /* Socket syscalls would go here */

    /*=======================================================================*/
    /* Scheduling Syscalls (200-224)                                         */
    /*=======================================================================*/

    /* 200 */ [SYS_SCHED_YIELD] = {
        .handler = sys_sched_yield,
        .name = "sched_yield",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 201 */ [SYS_SCHED_SETAFFINITY] = {
        .handler = sys_sched_setaffinity,
        .name = "sched_setaffinity",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 202 */ [SYS_SCHED_GETAFFINITY] = {
        .handler = sys_sched_getaffinity,
        .name = "sched_getaffinity",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 203 */ [SYS_SCHED_SETATTR] = {
        .handler = sys_sched_setattr,
        .name = "sched_setattr",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 204 */ [SYS_SCHED_GETATTR] = {
        .handler = sys_sched_getattr,
        .name = "sched_getattr",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 205 */ [SYS_SCHED_SETPARAM] = {
        .handler = sys_sched_setparam,
        .name = "sched_setparam",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 206 */ [SYS_SCHED_GETPARAM] = {
        .handler = sys_sched_getparam,
        .name = "sched_getparam",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 207 */ [SYS_SCHED_SET_SCHEDULER] = {
        .handler = sys_sched_setscheduler,
        .name = "sched_setscheduler",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 208 */ [SYS_SCHED_GET_SCHEDULER] = {
        .handler = sys_sched_getscheduler,
        .name = "sched_getscheduler",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 209 */ [SYS_SCHED_GET_PRIORITY_MAX] = {
        .handler = sys_sched_get_priority_max,
        .name = "sched_get_priority_max",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 210 */ [SYS_SCHED_GET_PRIORITY_MIN] = {
        .handler = sys_sched_get_priority_min,
        .name = "sched_get_priority_min",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 211 */ [SYS_SCHED_RR_GET_INTERVAL] = {
        .handler = sys_sched_rr_get_interval,
        .name = "sched_rr_get_interval",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 212-213 */ /* Reserved */

    /* 214 */ [SYS_NANOSLEEP] = {
        .handler = sys_nanosleep,
        .name = "nanosleep",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 215 */ [SYS_CLOCK_NANOSLEEP] = {
        .handler = sys_clock_nanosleep,
        .name = "clock_nanosleep",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 216 */ [SYS_CLOCK_GETTIME] = {
        .handler = sys_clock_gettime,
        .name = "clock_gettime",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 217 */ [SYS_CLOCK_SETTIME] = {
        .handler = sys_clock_settime,
        .name = "clock_settime",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 218 */ [SYS_CLOCK_GETRES] = {
        .handler = sys_clock_getres,
        .name = "clock_getres",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 219 */ [SYS_CLOCK_GETCPUCLOCKID] = {
        .handler = sys_clock_getcpuclockid,
        .name = "clock_getcpuclockid",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 220 */ [SYS_GETRUSAGE] = {
        .handler = sys_getrusage,
        .name = "getrusage",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 221 */ [SYS_TIMES] = {
        .handler = sys_times,
        .name = "times",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /* 222 */ [SYS_GETITIMER] = {
        .handler = sys_getitimer,
        .name = "getitimer",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 223 */ [SYS_SETITIMER] = {
        .handler = sys_setitimer,
        .name = "setitimer",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 224 */ [SYS_ALARM] = {
        .handler = sys_alarm,
        .name = "alarm",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 1
    },

    /*=======================================================================*/
    /* Time Syscalls (225-239)                                               */
    /*=======================================================================*/

    /* 225 */ [SYS_GETTIMEOFDAY] = {
        .handler = sys_gettimeofday,
        .name = "gettimeofday",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 226 */ [SYS_SETTIMEOFDAY] = {
        .handler = sys_settimeofday,
        .name = "settimeofday",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 227-239 */ /* Additional time syscalls */

    /*=======================================================================*/
    /* Signal Handling Syscalls (240-255)                                    */
    /*=======================================================================*/

    /* 240 */ [SYS_SIGNAL] = {
        .handler = sys_signal,
        .name = "signal",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 2
    },

    /* 241 */ [SYS_SIGACTION] = {
        .handler = sys_sigaction,
        .name = "sigaction",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 3
    },

    /* 242 */ [SYS_SIGPROCMASK] = {
        .handler = sys_sigprocmask,
        .name = "sigprocmask",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 3
    },

    /* 243 */ [SYS_SIGPENDING] = {
        .handler = sys_sigpending,
        .name = "sigpending",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 1
    },

    /* 244 */ [SYS_SIGSUSPEND] = {
        .handler = sys_sigsuspend,
        .name = "sigsuspend",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 1
    },

    /* 245 */ [SYS_SIGRETURN] = {
        .handler = sys_sigreturn,
        .name = "sigreturn",
        .flags = SYSCALL_F_NATIVE | SYSCALL_F_DEPRECATED,
        .nargs = 0
    },

    /* 246 */ [SYS_RT_SIGACTION] = {
        .handler = sys_rt_sigaction,
        .name = "rt_sigaction",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 247 */ [SYS_RT_SIGPROCMASK] = {
        .handler = sys_rt_sigprocmask,
        .name = "rt_sigprocmask",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 248 */ [SYS_RT_SIGPENDING] = {
        .handler = sys_rt_sigpending,
        .name = "rt_sigpending",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 249 */ [SYS_RT_SIGSUSPEND] = {
        .handler = sys_rt_sigsuspend,
        .name = "rt_sigsuspend",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 250 */ [SYS_RT_SIGRETURN] = {
        .handler = sys_rt_sigreturn,
        .name = "rt_sigreturn",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 0
    },

    /* 251 */ [SYS_RT_SIGTIMEDWAIT] = {
        .handler = sys_rt_sigtimedwait,
        .name = "rt_sigtimedwait",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 4
    },

    /* 252 */ [SYS_RT_SIGQUEUEINFO] = {
        .handler = sys_rt_sigqueueinfo,
        .name = "rt_sigqueueinfo",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },

    /* 253 */ [SYS_KILL] = {
        .handler = sys_kill,
        .name = "kill",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 254 */ [SYS_TKILL] = {
        .handler = sys_tkill,
        .name = "tkill",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 2
    },

    /* 255 */ [SYS_TGKILL] = {
        .handler = sys_tgkill,
        .name = "tgkill",
        .flags = SYSCALL_F_NATIVE,
        .nargs = 3
    },
};

/*===========================================================================*/
/*                         SYSCALL TABLE ACCESS                              */
/*===========================================================================*/

/**
 * syscall_get_handler - Get handler for syscall number
 * @nr: Syscall number
 *
 * Returns: Handler function pointer, or NULL if not implemented
 */
syscall_handler_t syscall_get_handler(u64 nr)
{
    if (nr >= NR_SYSCALLS) {
        return NULL;
    }

    return syscall_table[nr].handler;
}

/**
 * syscall_get_name - Get name for syscall number
 * @nr: Syscall number
 *
 * Returns: Syscall name string, or "unknown" if invalid
 */
const char *syscall_get_name(u64 nr)
{
    if (nr >= NR_SYSCALLS) {
        return "unknown";
    }

    return syscall_table[nr].name ? syscall_table[nr].name : "unknown";
}

/**
 * syscall_table_init - Initialize syscall table
 *
 * Sets up the syscall table with all registered handlers.
 * Called during kernel initialization.
 */
void syscall_table_init(void)
{
    u64 i;
    u64 implemented = 0;

    pr_info("Initializing syscall table...\n");

    /* Count implemented syscalls */
    for (i = 0; i < NR_SYSCALLS; i++) {
        if (syscall_table[i].handler != NULL) {
            implemented++;
        }
    }

    pr_info("Syscall table initialized: %lu/%d syscalls implemented\n",
            implemented, NR_SYSCALLS);
}
