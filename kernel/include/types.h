/*
 * NEXUS OS - Type Definitions
 * kernel/include/types.h
 */

#ifndef _NEXUS_TYPES_H
#define _NEXUS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

/* Boolean */
#ifndef NULL
#define NULL        ((void*)0)
#endif

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
#define EPERM           (-14)
#define ESRCH           (-15)
#define EINTR           (-16)
#define EFAULT          (-17)
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
} spinlock_t;

/* Mutex */
typedef struct {
    atomic_t count;
    spinlock_t wait_lock;
    struct list_head wait_list;
} mutex_t;

#endif /* _NEXUS_TYPES_H */
