/*
 * NEXUS OS - libsys syscall_* compatibility layer
 * userspace/libsys/syscall_glue.c
 *
 * Why this file exists
 * --------------------
 * libsys historically exposed the POSIX names (open, read, write, exit, ...)
 * and nothing else. libc and init, however, call a second set of names -
 * syscall_open(), syscall_write(), syscall_exit() and so on - which were
 * declared `extern` at each call site and defined nowhere in the tree.
 *
 * Both halves compiled. Nothing linked them, so nothing noticed that every
 * buffered stdio write, every heap expansion, and every process exit in the
 * userspace tree resolved to a symbol that did not exist. The first attempt to
 * link userspace produced ~40 undefined references from these names alone.
 *
 * This file is the bridge. Each syscall_* entry issues the same syscall number
 * the matching POSIX wrapper does, so there is one implementation of the ABI
 * and two spellings of it.
 *
 * Return convention
 * -----------------
 * The POSIX wrappers here (syscall_wrapper.c) translate a negative kernel
 * result into -1 and stash the code in errno. The syscall_* names deliberately
 * do NOT: callers in libc/init test the raw value against -1 or against 0 and
 * several of them discard it entirely. Passing the kernel result straight
 * through keeps those checks meaningful. libc's own `syscall_abort` caller, for
 * instance, never looks at the return value at all.
 */

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>   /* pid_t, uid_t, gid_t, mode_t, dev_t, ssize_t */

/* Shared numbering. This is the one definition both sides agree on. */
#include "../../kernel/include/nexus_syscall_numbers.h"

typedef int64_t nxs_ret_t;

/*===========================================================================*/
/*                          RAW SYSCALL PRIMITIVES                           */
/*===========================================================================*/

/*
 * The kernel side (kernel/arch/x86_64/syscall/syscall_entry.S) takes the number
 * in rax and the arguments in rdi, rsi, rdx, r10 - the 4th argument in r10
 * because the syscall instruction clobbers rcx. These primitives match that
 * exactly; they are duplicated from syscall_wrapper.c rather than shared
 * because they are `static inline` there and pulling in a header for them
 * would mean moving code this change does not need to touch.
 */

static inline nxs_ret_t nxs_syscall0(uint64_t num)
{
    nxs_ret_t ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(num)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline nxs_ret_t nxs_syscall1(uint64_t num, uint64_t a1)
{
    nxs_ret_t ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(num), "D"(a1)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline nxs_ret_t nxs_syscall2(uint64_t num, uint64_t a1, uint64_t a2)
{
    nxs_ret_t ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(num), "D"(a1), "S"(a2)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline nxs_ret_t nxs_syscall3(uint64_t num, uint64_t a1, uint64_t a2,
                                     uint64_t a3)
{
    nxs_ret_t ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(num), "D"(a1), "S"(a2), "d"(a3)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline nxs_ret_t nxs_syscall4(uint64_t num, uint64_t a1, uint64_t a2,
                                     uint64_t a3, uint64_t a4)
{
    register uint64_t r10 __asm__("r10") = a4;
    nxs_ret_t ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                      : "rcx", "r11", "memory");
    return ret;
}

/*===========================================================================*/
/*                            FILE AND FD CALLS                              */
/*===========================================================================*/

int syscall_open(const char *pathname, int flags, int mode)
{
    return (int)nxs_syscall3(SYS_OPEN, (uint64_t)pathname,
                             (uint64_t)(int64_t)flags, (uint64_t)(int64_t)mode);
}

int syscall_close(int fd)
{
    return (int)nxs_syscall1(SYS_CLOSE, (uint64_t)(int64_t)fd);
}

ssize_t syscall_read(int fd, void *buf, size_t count)
{
    return (ssize_t)nxs_syscall3(SYS_READ, (uint64_t)(int64_t)fd,
                                 (uint64_t)buf, (uint64_t)count);
}

ssize_t syscall_write(int fd, const void *buf, size_t count)
{
    return (ssize_t)nxs_syscall3(SYS_WRITE, (uint64_t)(int64_t)fd,
                                 (uint64_t)buf, (uint64_t)count);
}

long syscall_lseek(int fd, long offset, int whence)
{
    return (long)nxs_syscall3(SYS_LSEEK, (uint64_t)(int64_t)fd,
                              (uint64_t)offset, (uint64_t)(int64_t)whence);
}

int syscall_dup2(int oldfd, int newfd)
{
    return (int)nxs_syscall2(SYS_DUP2, (uint64_t)(int64_t)oldfd,
                             (uint64_t)(int64_t)newfd);
}

int syscall_pipe(int pipefd[2])
{
    return (int)nxs_syscall1(SYS_PIPE, (uint64_t)pipefd);
}

int syscall_stat(const char *pathname, void *statbuf)
{
    return (int)nxs_syscall2(SYS_STAT, (uint64_t)pathname, (uint64_t)statbuf);
}

int syscall_unlink(const char *pathname)
{
    return (int)nxs_syscall1(SYS_UNLINK, (uint64_t)pathname);
}

int syscall_chdir(const char *path)
{
    return (int)nxs_syscall1(SYS_CHDIR, (uint64_t)path);
}

int syscall_mkdir(const char *pathname, mode_t mode)
{
    return (int)nxs_syscall2(SYS_MKDIR, (uint64_t)pathname,
                             (uint64_t)(int64_t)mode);
}

int syscall_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    return (int)nxs_syscall3(SYS_MKNOD, (uint64_t)pathname,
                             (uint64_t)(int64_t)mode, (uint64_t)dev);
}

int syscall_symlink(const char *target, const char *linkpath)
{
    return (int)nxs_syscall2(SYS_SYMLINK, (uint64_t)target, (uint64_t)linkpath);
}

int syscall_chmod(const char *pathname, mode_t mode)
{
    return (int)nxs_syscall2(SYS_CHMOD, (uint64_t)pathname,
                             (uint64_t)(int64_t)mode);
}

int syscall_chown(const char *pathname, uid_t owner, gid_t group)
{
    return (int)nxs_syscall3(SYS_CHOWN, (uint64_t)pathname,
                             (uint64_t)(int64_t)owner, (uint64_t)(int64_t)group);
}

/*===========================================================================*/
/*                            PROCESS AND MEMORY                             */
/*===========================================================================*/

int syscall_exit(int status)
{
    nxs_syscall1(SYS_EXIT, (uint64_t)(int64_t)status);
    return 0;  /* SYS_EXIT does not return; keep the compiler satisfied. */
}

void syscall_abort(void)
{
    /*
     * There is no SYS_ABORT in the number table. An abort is a forced exit
     * with a non-zero status, which is what the callers rely on.
     */
    nxs_syscall1(SYS_EXIT, 134);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

int syscall_fork(void)
{
    return (int)nxs_syscall0(SYS_FORK);
}

int syscall_getpid(void)
{
    return (int)nxs_syscall0(SYS_GETPID);
}

int syscall_getppid(void)
{
    return (int)nxs_syscall0(SYS_GETPPID);
}

int syscall_setsid(void)
{
    return (int)nxs_syscall0(SYS_SETSID);
}

int syscall_kill(int pid, int sig)
{
    return (int)nxs_syscall2(SYS_KILL, (uint64_t)(int64_t)pid,
                             (uint64_t)(int64_t)sig);
}

int syscall_execve(const char *filename, char *const argv[],
                   char *const envp[])
{
    return (int)nxs_syscall3(SYS_EXECVE, (uint64_t)filename,
                             (uint64_t)argv, (uint64_t)envp);
}

int syscall_wait4(int pid, int *wstatus, int options, void *rusage)
{
    return (int)nxs_syscall4(SYS_WAIT4, (uint64_t)(int64_t)pid,
                             (uint64_t)wstatus, (uint64_t)(int64_t)options,
                             (uint64_t)rusage);
}

void *syscall_brk(void *addr)
{
    return (void *)nxs_syscall1(SYS_BRK, (uint64_t)addr);
}

/*
 * syscall_sbrk - grow (or shrink) the heap by `increment`, return old break
 *
 * libc/stdlib.c treats the return as the start of the newly available region
 * and writes a block header there, so this must return the previous break on
 * success and (void *)-1 on failure. SYS_SBRK is asked to do the adjustment;
 * the previous break is recovered with SYS_BRK(0), which is the standard
 * "query only" form.
 */
void *syscall_sbrk(intptr_t increment)
{
    void *old_brk = (void *)nxs_syscall1(SYS_BRK, 0);
    if ((int64_t)old_brk < 0) {
        return (void *)-1;
    }

    if (increment == 0) {
        return old_brk;
    }

    int64_t ret = nxs_syscall1(SYS_SBRK, (uint64_t)increment);
    if (ret < 0) {
        return (void *)-1;
    }

    return old_brk;
}

/*===========================================================================*/
/*                          SIGNALS AND TIME                                 */
/*===========================================================================*/

int syscall_sigaction(int signum, const void *act, void *oldact)
{
    return (int)nxs_syscall3(SYS_SIGACTION, (uint64_t)(int64_t)signum,
                             (uint64_t)act, (uint64_t)oldact);
}

int syscall_gettimeofday(void *tv, void *tz)
{
    return (int)nxs_syscall2(SYS_GETTIMEOFDAY, (uint64_t)tv, (uint64_t)tz);
}

int syscall_nanosleep(const void *req, void *rem)
{
    return (int)nxs_syscall2(SYS_NANOSLEEP, (uint64_t)req, (uint64_t)rem);
}

/*===========================================================================*/
/*                        SYSTEM AND ENVIRONMENT                             */
/*===========================================================================*/

int syscall_mount(const char *source, const char *target,
                  const char *filesystemtype, unsigned long mountflags,
                  const void *data)
{
    return (int)nxs_syscall4(SYS_MOUNT, (uint64_t)source, (uint64_t)target,
                             (uint64_t)filesystemtype, (uint64_t)mountflags);
}

int syscall_reboot(int magic)
{
    /*
     * SYS_REBOOT is a reserved slot with no kernel handler yet. It returns
     * ENOSYS from the dispatcher, which is the correct answer for a program
     * that is not privileged to reboot the machine anyway.
     */
    return (int)nxs_syscall1(SYS_REBOOT, (uint64_t)(int64_t)magic);
}

/*
 * Environment. There is no environment block in the userspace ABI yet - crt0
 * passes an empty one - so these report "nothing set" rather than reaching for
 * a syscall that does not exist. libc's getenv() consults its own in-process
 * table first and falls back to syscall_getenv(), so returning NULL here means
 * "not found", which is accurate.
 */
ssize_t syscall_getcmdline(char *buf, size_t size)
{
    return (ssize_t)nxs_syscall2(SYS_GET_CMDLINE, (uint64_t)buf,
                                 (uint64_t)size);
}

char *syscall_getenv(const char *name)
{
    (void)name;
    return NULL;
}

int syscall_setenv(const char *name, const char *value, int overwrite)
{
    (void)name; (void)value; (void)overwrite;
    return -1;
}

int syscall_unsetenv(const char *name)
{
    (void)name;
    return -1;
}
