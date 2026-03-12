# NEXUS-OS Kernel Design

## Overview

The NEXUS-OS kernel is a hybrid microkernel design that combines the performance benefits of monolithic kernels with the modularity and security of microkernels. This document describes the kernel architecture, design decisions, and implementation details.

## Design Philosophy

### Core Principles

1. **Minimal Trusted Computing Base**: Only essential code runs in kernel mode
2. **Performance**: Minimize context switches and memory copies
3. **Modularity**: Loadable kernel modules for extensibility
4. **Security**: Defense in depth with multiple security layers
5. **Portability**: Architecture-independent design with clean HAL interface

### Microkernel vs Monolithic Trade-offs

| Aspect | Microkernel Approach | NEXUS Hybrid |
|--------|---------------------|--------------|
| Drivers | Userspace | Critical in-kernel, others userspace |
| IPC | Message-based | Optimized shared memory + messages |
| Filesystem | Userspace | VFS in-kernel, FS drivers modular |
| Networking | Userspace | Stack in-kernel for performance |
| Performance | Higher overhead | Optimized paths for common operations |

## Kernel Memory Layout

```
Virtual Address Space (x86_64)
┌─────────────────────────────────────────┐  0xFFFFFFFFFFFFFFFF
│           Guard Page (Unmapped)         │
├─────────────────────────────────────────┤
│         Higher Half Kernel Space        │
│  ┌─────────────────────────────────┐    │
│  │      Kernel Dynamic Heap        │    │  Growable
│  ├─────────────────────────────────┤    │
│  │      Kernel Module Space        │    │
│  ├─────────────────────────────────┤    │
│  │      Kernel Stack Space         │    │  Per-CPU stacks
│  ├─────────────────────────────────┤    │
│  │      Direct Physical Map        │    │  Identity mapped
│  └─────────────────────────────────┘    │
├─────────────────────────────────────────┤  0xFFFFFFFF80000000
│           Kernel Code/Data              │  Fixed mapping
├─────────────────────────────────────────┤  0x0000000000000000
│           User Space                    │
│  ┌─────────────────────────────────┐    │
│  │         User Heap               │    │
│  ├─────────────────────────────────┤    │
│  │         User Stack              │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘  0x0000000000000000
```

### Memory Regions

| Region | Base Address | Size | Purpose |
|--------|-------------|------|---------|
| Kernel Code | 0xFFFFFFFF80000000 | 64 MB | Kernel text, data, rodata |
| Direct Map | 0xFFFF800000000000 | 64 TB | Direct physical memory mapping |
| Kernel Stacks | 0xFFFF880000000000 | 1 TB | Per-CPU and thread stacks |
| Kernel Modules | 0xFFFF900000000000 | 512 GB | Loadable kernel modules |
| Kernel Heap | 0xFFFFFFFF90000000 | 1 GB | Dynamic kernel allocations |

## Kernel Subsystems

### 1. Boot and Initialization

```
Boot Sequence:
┌─────────────┐
│  UEFI/BIOS  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Bootloader │  (NBL - NEXUS Bootloader)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Early Init │  (GDT, IDT, paging, console)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Kernel Init│  (subsystem initialization)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Scheduler  │  (start userspace)
└─────────────┘
```

**Key Files**:
- `/kernel/arch/*/boot/` - Architecture-specific boot code
- `/kernel/core/init.c` - Kernel initialization
- `/kernel/core/kernel.c` - Kernel main entry

### 2. Memory Management

#### Physical Memory Manager (PMM)

```c
// Physical page frame management
typedef struct {
    u64 total_pages;
    u64 free_pages;
    u64 reserved_pages;
    bitmap_t *page_bitmap;
} pmm_info_t;

// API
phys_addr_t pmm_alloc_page(void);
void pmm_free_page(phys_addr_t addr);
void pmm_alloc_pages(phys_addr_t *addrs, size_t count);
```

**Location**: `/kernel/mm/pmm.c`

#### Virtual Memory Manager (VMM)

```c
// Virtual address space
typedef struct {
    page_table_t *page_table;
    vma_t *vma_root;          // RB-tree of VMAs
    spinlock_t lock;
    u64 total_vm;
    u64 max_vm;
} address_space_t;

// API
nexus_result_t vmm_map(address_space_t *as, virt_addr_t vaddr, 
                       phys_addr_t paddr, size_t size, u64 flags);
nexus_result_t vmm_unmap(address_space_t *as, virt_addr_t vaddr, size_t size);
nexus_result_t vmm_alloc_vma(address_space_t *as, size_t size, u64 flags);
```

**Location**: `/kernel/mm/vmm.c`

#### Kernel Heap Allocator

```
Slab Allocator Hierarchy:
┌─────────────────────────────────────────┐
│            Kernel Heap                  │
│  ┌─────────┬─────────┬─────────┐       │
│  │  Slab   │  Slab   │  Slab   │       │  Large allocations
│  │ Cache   │ Cache   │ Cache   │       │
│  │ 64B     │ 128B    │ 256B    │       │
│  └─────────┴─────────┴─────────┘       │
│  ┌─────────────────────────────────┐   │
│  │         Buddy Allocator         │   │  Page-sized+ allocations
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

**Location**: `/kernel/mm/slab.c`, `/kernel/mm/heap.c`

### 3. Process and Thread Management

#### Process Structure

```c
typedef struct process {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    
    char name[TASK_COMM_LEN];
    
    address_space_t *mm;           // Address space
    struct thread *threads;        // Thread list
    spinlock_t thread_lock;
    
    struct file **files;           // Open files
    spinlock_t files_lock;
    
    struct process *parent;        // Parent process
    struct list_head children;     // Child processes
    
    // Credentials
    cred_t *cred;
    
    // Resource limits
    rlimit_t rlimits[RLIM_NLIMITS];
    
    // Signals
    signal_info_t *signal_info;
    
    // Statistics
    process_stats_t stats;
    
    // Synchronization
    wait_queue_t wait_queue;
    refcount_t refcount;
} process_t;
```

#### Thread Structure

```c
typedef struct thread {
    tid_t tid;
    pid_t pid;                     // Owner process
    
    char name[THREAD_NAME_LEN];
    
    // Execution context
    cpu_context_t context;
    void *stack;
    size_t stack_size;
    
    // Scheduling
    struct list_head run_list;
    u64 prio;
    u64 virt_runtime;
    u64 exec_start;
    
    // State
    thread_state_t state;
    wait_queue_entry_t *wait_entry;
    
    // TLS
    void *tls;
    
    // Statistics
    thread_stats_t stats;
    
    // Architecture specific
    arch_thread_t arch;
} thread_t;
```

**Location**: `/kernel/sched/process.c`, `/kernel/sched/thread.c`

### 4. Scheduler

#### Completely Fair Scheduler (CFS)

```
CFS Run Queue:
┌─────────────────────────────────────────┐
│              Run Queue                  │
│                                         │
│  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐   │
│  │ T1  │─>│ T2  │─>│ T3  │─>│ T4  │   │  Red-Black Tree
│  │vrt=0│  │vrt=5│  │vrt=8│  │vrt=12│  │  ordered by
│  └─────┘  └─────┘  └─────┘  └─────┘   │  virtual runtime
└─────────────────────────────────────────┘
```

**Scheduling Classes**:
1. **SCHED_NORMAL**: CFS for regular tasks
2. **SCHED_FIFO**: Real-time FIFO
3. **SCHED_RR**: Real-time round-robin
4. **SCHED_IDLE**: Low priority background tasks

**Location**: `/kernel/sched/scheduler.c`, `/kernel/sched/cfs.c`

### 5. Inter-Process Communication (IPC)

#### IPC Mechanisms

| Mechanism | Use Case | Performance |
|-----------|----------|-------------|
| Pipes | Stream data | Medium |
| Message Queues | Structured messages | Fast |
| Shared Memory | Large data transfer | Very Fast |
| Semaphores | Synchronization | Fast |
| Mutexes | Mutual exclusion | Fast |
| Signals | Notifications | Very Fast |

#### Message Passing

```c
// IPC Message
typedef struct {
    u64 sender;
    u64 receiver;
    u32 type;
    u32 flags;
    size_t size;
    void *data;
} ipc_message_t;

// API
nexus_result_t ipc_send(pid_t dest, ipc_message_t *msg);
nexus_result_t ipc_recv(pid_t src, ipc_message_t *msg);
nexus_result_t ipc_call(pid_t dest, ipc_message_t *req, ipc_message_t *resp);
```

**Location**: `/kernel/ipc/`

### 6. Synchronization Primitives

#### Spinlock

```c
typedef struct {
    volatile u32 lock;
#ifdef CONFIG_DEBUG_SPINLOCK
    const char *name;
    void *owner;
#endif
} spinlock_t;

#define spin_lock_irqsave(lock, flags)  \
    do { flags = irq_save(); spin_lock(lock); } while(0)
#define spin_unlock_irqrestore(lock, flags)  \
    do { spin_unlock(lock); irq_restore(flags); } while(0)
```

#### Mutex

```c
typedef struct {
    atomic_t count;
    spinlock_t wait_lock;
    struct list_head wait_list;
} mutex_t;
```

**Location**: `/kernel/sync/`

### 7. System Call Interface

#### Syscall Entry (x86_64)

```asm
; Fast syscall entry using SYSCALL instruction
.global syscall_entry
syscall_entry:
    swapgs                      ; Swap to kernel GS
    pushq   %rcx                ; Save user RIP
    pushq   %r11                ; Save user RFLAGS
    ; ... save registers
    call    syscall_handler
    ; ... restore registers
    swapgs                      ; Swap back to user GS
    sysretq                     ; Return to user
```

#### Syscall Table

```c
// System call numbers
#define SYS_READ            0
#define SYS_WRITE           1
#define SYS_OPEN            2
#define SYS_CLOSE           3
#define SYS_STAT            4
#define SYS_FSTAT           5
#define SYS_MMAP            9
#define SYS_MPROTECT        10
#define SYS_MUNMAP          11
#define SYS_BRK             12
#define SYS_IOCTL           16
#define SYS_FORK            57
#define SYS_EXECVE          59
#define SYS_EXIT            60
#define SYS_WAIT4           61
#define SYS_KILL            62
#define SYS_GETPID          39
#define SYS_SOCKET          41
#define SYS_CONNECT         42
// ... total ~400 syscalls

// Syscall handler
typedef long (*syscall_fn)(const syscall_args_t *args);
static syscall_fn syscall_table[NR_SYSCALLS];
```

**Location**: `/kernel/syscall/`

### 8. Interrupt Handling

#### Interrupt Descriptor Table

```c
typedef struct {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} __packed idt_entry_t;

// Interrupt handler type
typedef void (*irq_handler_t)(int irq, void *dev_id);

// API
nexus_result_t request_irq(int irq, irq_handler_t handler, 
                           u64 flags, const char *name, void *dev_id);
void free_irq(int irq, void *dev_id);
```

**Location**: `/kernel/arch/*/interrupts/`

## Kernel Configuration

### Build-time Options

```
CONFIG_SMP=y                  # Symmetric multiprocessing
CONFIG_PREEMPT=y              # Preemptible kernel
CONFIG_DEBUG=y                # Debug features
CONFIG_KASAN=y                # Address sanitizer
CONFIG_KCOV=y                 # Coverage-guided fuzzing
CONFIG_MODULES=y              # Loadable modules
CONFIG_KPROBES=y              # Kernel probes
CONFIG_FTRACE=y               # Function tracer
```

### Runtime Tunables

```
kernel.sched_latency_ns = 6000000
kernel.sched_min_granularity_ns = 750000
kernel.sched_wakeup_granularity_ns = 1000000
kernel.pid_max = 65536
kernel.threads-max = 125829
```

## Error Handling

### Kernel Panic

```c
void kernel_panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // Disable interrupts
    local_irq_disable();
    
    // Print panic message
    vprintk(fmt, args);
    
    // Dump registers and stack trace
    dump_registers();
    dump_stack_trace();
    
    // Halt or reboot
    if (panic_timeout > 0) {
        delay_ms(panic_timeout * 1000);
        machine_restart();
    } else {
        machine_halt();
    }
}
```

### Error Codes

```c
typedef enum {
    NEXUS_OK        = 0,
    NEXUS_ERROR     = -1,
    NEXUS_ENOMEM    = -2,    /* Out of memory */
    NEXUS_EINVAL    = -3,    /* Invalid argument */
    NEXUS_ENOENT    = -4,    /* No such entry */
    NEXUS_EBUSY     = -5,    /* Device busy */
    NEXUS_EACCES    = -6,    /* Access denied */
    NEXUS_EIO       = -7,    /* I/O error */
    NEXUS_ETIMEDOUT = -8,    /* Timeout */
    NEXUS_ENOSYS    = -9,    /* Not implemented */
    NEXUS_EEXIST    = -10,   /* Already exists */
    NEXUS_EAGAIN    = -11,   /* Try again */
    NEXUS_ENODEV    = -12,   /* No such device */
    NEXUS_ENOTSUPP  = -13,   /* Not supported */
} nexus_result_t;
```

## Debugging Support

### Kernel Debugging Features

1. **Printk**: Kernel logging with log levels
2. **BUG/BUG_ON**: Kernel bug detection
3. **WARN/WARN_ON**: Warning macros
4. **KASAN**: Memory error detection
5. **KMSAN**: Uninitialized memory detection
6. **UBSAN**: Undefined behavior detection
7. **Lockdep**: Lock dependency tracking
8. **Kprobes**: Dynamic instrumentation

### Debug Output

```
[    0.000000] NEXUS-OS v1.0.0-alpha "Genesis"
[    0.000000] Booting on CPU 0 (Intel Core i9-13900K)
[    0.000000] Memory: 32768 MB total, 31744 MB available
[    0.001234] SMP: Booting 24 CPUs...
[    0.002456] Scheduler: CFS initialized
[    0.003678] VFS: Mounted root filesystem
[    0.004890] Init: Starting userspace...
```

## Performance Optimizations

### Key Optimizations

1. **Per-CPU Data**: Avoid cache line bouncing
2. **RCU**: Read-copy-update for read-heavy data
3. **Lock-free Algorithms**: Where possible
4. **CPU Affinity**: Pin threads to CPUs
5. **Huge Pages**: Reduce TLB misses
6. **Prefetching**: Software prefetch hints
7. **Inline Functions**: Reduce call overhead
8. **Branch Prediction**: Likely/unlikely hints

## Related Documents

- [Architecture Overview](overview.md)
- [Virtualization Architecture](virtualization.md)
- [Kernel API](../api/kernel_api.md)

---

*Last Updated: March 2026*
*NEXUS-OS Version 1.0.0-alpha*
