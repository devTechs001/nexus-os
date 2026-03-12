/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2024 NEXUS Development Team
 *
 * kernel.h - Core Kernel Definitions
 */

#ifndef _NEXUS_KERNEL_H
#define _NEXUS_KERNEL_H

#include "types.h"
#include "config.h"
#include "version.h"

/*===========================================================================*/
/*                            KERNEL CONFIGURATION                           */
/*===========================================================================*/

#define NEXUS_VERSION_MAJOR     1
#define NEXUS_VERSION_MINOR     0
#define NEXUS_VERSION_PATCH     0
#define NEXUS_VERSION_STRING    "1.0.0-alpha"
#define NEXUS_CODENAME          "Genesis"

/* Kernel Memory Layout (x86_64) */
#define KERNEL_VIRTUAL_BASE     0xFFFFFFFF80000000ULL
#define KERNEL_PHYSICAL_BASE    0x0000000000100000ULL
#define KERNEL_STACK_SIZE       (64 * 1024)
#define KERNEL_HEAP_START       0xFFFFFFFF90000000ULL
#define KERNEL_HEAP_SIZE        (1024 * 1024 * 1024)

/* Page Sizes */
#define PAGE_SIZE_4K            0x1000
#define PAGE_SIZE_2M            0x200000
#define PAGE_SIZE_1G            0x40000000
#define PAGE_SIZE               PAGE_SIZE_4K
#define PAGE_SHIFT              12
#define PAGE_MASK               (~(PAGE_SIZE - 1))

/* Page Table Flags */
#define PAGE_PRESENT            (1ULL << 0)
#define PAGE_WRITABLE           (1ULL << 1)
#define PAGE_USER               (1ULL << 2)
#define PAGE_WRITETHROUGH       (1ULL << 3)
#define PAGE_NOCACHE            (1ULL << 4)
#define PAGE_ACCESSED           (1ULL << 5)
#define PAGE_DIRTY              (1ULL << 6)
#define PAGE_HUGE               (1ULL << 7)
#define PAGE_GLOBAL             (1ULL << 8)

/* Maximum Limits */
#define MAX_CPUS                256
#define MAX_PROCESSES           65536
#define MAX_THREADS_PER_PROCESS 4096
#define MAX_OPEN_FILES          65536
#define MAX_MEMORY_REGIONS      1024
#define MAX_PATH                4096

/* Scheduling */
#define SCHED_PRIORITY_MIN      0
#define SCHED_PRIORITY_DEFAULT  50
#define SCHED_PRIORITY_MAX      99
#define SCHED_TIME_SLICE_MS     10

/* Utility Macros */
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN(x, a)             (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)        ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)        (((x) & ((a) - 1)) == 0)
#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)        MIN(MAX(x, lo), hi)
#define BIT(n)                  (1ULL << (n))
#define KB(x)                   ((x) * 1024ULL)
#define MB(x)                   ((x) * 1024ULL * 1024ULL)
#define GB(x)                   ((x) * 1024ULL * 1024ULL * 1024ULL)

/* Compiler Attributes */
#define __packed                __attribute__((packed))
#define __aligned(x)            __attribute__((aligned(x)))
#define __section(x)            __attribute__((section(x)))
#define __unused                __attribute__((unused))
#define __noreturn              __attribute__((noreturn))
#define __weak                  __attribute__((weak))
#define __always_inline         inline __attribute__((always_inline))
#define __noinline              __attribute__((noinline))
#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)

/* Memory Barriers */
#define barrier()               __asm__ __volatile__("" ::: "memory")
#define mb()                    __asm__ __volatile__("mfence" ::: "memory")
#define rmb()                   __asm__ __volatile__("lfence" ::: "memory")
#define wmb()                   __asm__ __volatile__("sfence" ::: "memory")

/* Container Of */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member)); })

#define offsetof(type, member) ((size_t)&((type *)0)->member)

/* List Operations */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    head->next->prev = new;
    new->next = head->next;
    new->prev = head;
    head->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head) {
    head->prev->next = new;
    new->prev = head->prev;
    new->next = head;
    head->prev = new;
}

static inline void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

static inline bool list_empty(const struct list_head *head) {
    return head->next == head;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* Atomic Operations */
static inline s32 atomic_read(const atomic_t *v) {
    return v->counter;
}

static inline void atomic_set(atomic_t *v, s32 i) {
    v->counter = i;
}

static inline void atomic_inc(atomic_t *v) {
    __asm__ __volatile__("lock; incl %0" : "+m"(v->counter));
}

static inline void atomic_dec(atomic_t *v) {
    __asm__ __volatile__("lock; decl %0" : "+m"(v->counter));
}

static inline s32 atomic_add_return(s32 i, atomic_t *v) {
    return __sync_add_and_fetch(&v->counter, i);
}

static inline s32 atomic_cmpxchg(atomic_t *v, s32 old, s32 new) {
    return __sync_val_compare_and_swap(&v->counter, old, new);
}

/* Forward Declarations */
struct process;
struct thread;
struct percpu_data;

/* Per-CPU Data */
typedef struct percpu_data {
    u32 cpu_id;
    u32 apic_id;
    struct thread *current_thread;
    struct process *current_process;
    void *kernel_stack;
    u64 context_switches;
    u64 interrupts;
} percpu_data_t;

/* Kernel Core Functions */
void kernel_main(void);
void kernel_early_init(void);
void kernel_init(void);
void kernel_start_scheduler(void) __noreturn;
void kernel_panic(const char *fmt, ...) __noreturn;

/* Logging */
int printk(const char *fmt, ...);

#define pr_info(fmt, ...)    printk("[INFO] " fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     printk("[ERROR] " fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   printk("[DEBUG] " fmt, ##__VA_ARGS__)

/* Memory */
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);

/* String */
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

/* Per-CPU */
percpu_data_t *get_percpu(void);
u32 get_cpu_id(void);
u32 get_num_cpus(void);

#define current_thread()    (get_percpu()->current_thread)
#define current_process()   (get_percpu()->current_process)

/* Interrupts */
void local_irq_enable(void);
void local_irq_disable(void);

/* Time */
u64 get_ticks(void);
u64 get_time_ms(void);
void delay_ms(u64 ms);

#endif /* _NEXUS_KERNEL_H */
