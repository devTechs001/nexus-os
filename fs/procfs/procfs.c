/*
 * NEXUS OS - ProcFS Implementation
 * fs/procfs/procfs.c
 *
 * Process filesystem for kernel and process information
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         PROCFS CONFIGURATION                              */
/*===========================================================================*/

#define PROCFS_MAX_ENTRIES        512
#define PROCFS_MAX_NAME           64
#define PROCFS_MAX_DATA           4096

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct proc_entry proc_entry_t;

typedef s32 (*proc_read_fn)(proc_entry_t *entry, void *buffer, u32 size, u32 offset);
typedef s32 (*proc_write_fn)(proc_entry_t *entry, const void *buffer, u32 size);

struct proc_entry {
    char name[PROCFS_MAX_NAME];
    char *data;
    u32 size;
    u32 flags;
    proc_read_fn read;
    proc_write_fn write;
    void *private_data;
    proc_entry_t *parent;
    proc_entry_t *children;
    proc_entry_t *next;
    bool is_directory;
};

typedef struct {
    bool initialized;
    proc_entry_t *root;
    u32 entry_count;
    spinlock_t lock;
} procfs_t;

static procfs_t g_procfs;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int procfs_init(void)
{
    printk("[PROCFS] ========================================\n");
    printk("[PROCFS] NEXUS OS ProcFS\n");
    printk("[PROCFS] ========================================\n");
    
    memset(&g_procfs, 0, sizeof(procfs_t));
    g_procfs.initialized = true;
    spinlock_init(&g_procfs.lock);
    
    /* Create root directory */
    g_procfs.root = kmalloc(sizeof(proc_entry_t));
    if (!g_procfs.root) {
        return -ENOMEM;
    }
    
    memset(g_procfs.root, 0, sizeof(proc_entry_t));
    strcpy(g_procfs.root->name, "/");
    g_procfs.root->is_directory = true;
    g_procfs.entry_count = 1;
    
    /* Create standard entries */
    proc_create("version", 0444, NULL);
    proc_create("uptime", 0444, NULL);
    proc_create("meminfo", 0444, NULL);
    proc_create("cpuinfo", 0444, NULL);
    proc_create("loadavg", 0444, NULL);
    proc_create("cmdline", 0444, NULL);
    
    proc_entry_t *sys = proc_mkdir("sys");
    if (sys) {
        proc_create_child(sys, "kernel", 0555, NULL);
        proc_create_child(sys, "vm", 0555, NULL);
        proc_create_child(sys, "net", 0555, NULL);
    }
    
    proc_entry_t *net = proc_mkdir("net");
    if (net) {
        proc_create_child(net, "dev", 0444, NULL);
        proc_create_child(net, "route", 0444, NULL);
        proc_create_child(net, "tcp", 0444, NULL);
    }
    
    printk("[PROCFS] ProcFS initialized (%d entries)\n", g_procfs.entry_count);
    return 0;
}

/*===========================================================================*/
/*                         ENTRY CREATION                                    */
/*===========================================================================*/

proc_entry_t *proc_create(const char *name, u32 mode, proc_read_fn read)
{
    spinlock_lock(&g_procfs.lock);
    
    if (g_procfs.entry_count >= PROCFS_MAX_ENTRIES) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    proc_entry_t *entry = kmalloc(sizeof(proc_entry_t));
    if (!entry) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    memset(entry, 0, sizeof(proc_entry_t));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->flags = mode;
    entry->read = read;
    entry->parent = g_procfs.root;
    entry->next = g_procfs.root->children;
    g_procfs.root->children = entry;
    g_procfs.entry_count++;
    
    spinlock_unlock(&g_procfs.lock);
    return entry;
}

proc_entry_t *proc_mkdir(const char *name)
{
    spinlock_lock(&g_procfs.lock);
    
    if (g_procfs.entry_count >= PROCFS_MAX_ENTRIES) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    proc_entry_t *entry = kmalloc(sizeof(proc_entry_t));
    if (!entry) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    memset(entry, 0, sizeof(proc_entry_t));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->is_directory = true;
    entry->parent = g_procfs.root;
    entry->next = g_procfs.root->children;
    g_procfs.root->children = entry;
    g_procfs.entry_count++;
    
    spinlock_unlock(&g_procfs.lock);
    return entry;
}

proc_entry_t *proc_create_child(proc_entry_t *parent, const char *name, 
                                 u32 mode, proc_read_fn read)
{
    if (!parent || !parent->is_directory) return NULL;
    
    spinlock_lock(&g_procfs.lock);
    
    if (g_procfs.entry_count >= PROCFS_MAX_ENTRIES) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    proc_entry_t *entry = kmalloc(sizeof(proc_entry_t));
    if (!entry) {
        spinlock_unlock(&g_procfs.lock);
        return NULL;
    }
    
    memset(entry, 0, sizeof(proc_entry_t));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->flags = mode;
    entry->read = read;
    entry->parent = parent;
    entry->next = parent->children;
    parent->children = entry;
    g_procfs.entry_count++;
    
    spinlock_unlock(&g_procfs.lock);
    return entry;
}

/*===========================================================================*/
/*                         READ HANDLERS                                     */
/*===========================================================================*/

static s32 proc_read_version(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    (void)entry; (void)offset;
    
    const char *version = "NEXUS OS 1.0.0\n"
                          "Kernel: 1.0.0\n"
                          "Architecture: x86_64\n";
    
    u32 len = strlen(version);
    if (offset >= len) return 0;
    
    u32 copy = (size < (len - offset)) ? size : (len - offset);
    memcpy(buffer, version + offset, copy);
    
    return copy;
}

static s32 proc_read_uptime(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    (void)entry; (void)offset;
    
    u64 uptime = ktime_get_sec();
    u64 idle = uptime / 4;  /* Mock idle time */
    
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%d.%02d %d.%02d\n",
                       (u32)(uptime / 100), (u32)(uptime % 100),
                       (u32)(idle / 100), (u32)(idle % 100));
    
    if (offset >= (u32)len) return 0;
    
    u32 copy = (size < ((u32)len - offset)) ? size : ((u32)len - offset);
    memcpy(buffer, buf + offset, copy);
    
    return copy;
}

static s32 proc_read_meminfo(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    (void)entry; (void)offset;
    
    /* Mock memory info */
    u64 total = 8 * 1024 * 1024 * 1024ULL;  /* 8GB */
    u64 free = 4 * 1024 * 1024 * 1024ULL;   /* 4GB free */
    u64 available = 5 * 1024 * 1024 * 1024ULL;
    u64 buffers = 256 * 1024 * 1024ULL;
    u64 cached = 1024 * 1024 * 1024ULL;
    
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "MemTotal:       %8d kB\n"
        "MemFree:        %8d kB\n"
        "MemAvailable:   %8d kB\n"
        "Buffers:        %8d kB\n"
        "Cached:         %8d kB\n",
        (u32)(total / 1024),
        (u32)(free / 1024),
        (u32)(available / 1024),
        (u32)(buffers / 1024),
        (u32)(cached / 1024));
    
    if (offset >= (u32)len) return 0;
    
    u32 copy = (size < ((u32)len - offset)) ? size : ((u32)len - offset);
    memcpy(buffer, buf + offset, copy);
    
    return copy;
}

static s32 proc_read_loadavg(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    (void)entry; (void)offset;
    
    /* Mock load average */
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "0.50 0.30 0.20 1/256 1234\n");
    
    if (offset >= (u32)len) return 0;
    
    u32 copy = (size < ((u32)len - offset)) ? size : ((u32)len - offset);
    memcpy(buffer, buf + offset, copy);
    
    return copy;
}

static s32 proc_read_net_dev(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    (void)entry; (void)offset;
    
    /* Mock network device stats */
    const char *dev = 
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
        "    lo:  100000     1000    0    0    0     0          0         0   100000     1000    0    0    0     0       0          0\n"
        "  eth0: 5000000    50000    0    0    0     0          0         0  2500000    25000    0    0    0     0       0          0\n";
    
    u32 len = strlen(dev);
    if (offset >= len) return 0;
    
    u32 copy = (size < (len - offset)) ? size : (len - offset);
    memcpy(buffer, dev + offset, copy);
    
    return copy;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

proc_entry_t *proc_lookup(const char *path)
{
    if (!path || !g_procfs.root) return NULL;
    
    /* Simple path lookup */
    proc_entry_t *current = g_procfs.root;
    
    /* Skip leading slash */
    if (path[0] == '/') path++;
    
    while (*path) {
        /* Find component */
        char component[PROCFS_MAX_NAME];
        u32 i = 0;
        
        while (*path && *path != '/' && i < PROCFS_MAX_NAME - 1) {
            component[i++] = *path++;
        }
        component[i] = '\0';
        
        /* Skip slashes */
        while (*path == '/') path++;
        
        /* Search children */
        proc_entry_t *found = NULL;
        for (proc_entry_t *child = current->children; child; child = child->next) {
            if (strcmp(child->name, component) == 0) {
                found = child;
                break;
            }
        }
        
        if (!found) return NULL;
        current = found;
    }
    
    return current;
}

s32 proc_read(proc_entry_t *entry, void *buffer, u32 size, u32 offset)
{
    if (!entry || !buffer || size == 0) return -EINVAL;
    
    /* Use custom read handler if available */
    if (entry->read) {
        return entry->read(entry, buffer, size, offset);
    }
    
    /* Read from data buffer */
    if (entry->data && entry->size > 0) {
        if (offset >= entry->size) return 0;
        
        u32 copy = (size < (entry->size - offset)) ? size : (entry->size - offset);
        memcpy(buffer, entry->data + offset, copy);
        
        return copy;
    }
    
    return 0;
}

s32 proc_write(proc_entry_t *entry, const void *buffer, u32 size)
{
    if (!entry || !buffer || size == 0) return -EINVAL;
    
    /* Check write permission */
    if (!(entry->flags & 0222)) {
        return -EACCES;
    }
    
    /* Use custom write handler if available */
    if (entry->write) {
        return entry->write(entry, buffer, size);
    }
    
    /* Write to data buffer */
    if (entry->data) {
        kfree(entry->data);
    }
    
    entry->data = kmalloc(size);
    if (!entry->data) return -ENOMEM;
    
    memcpy(entry->data, buffer, size);
    entry->size = size;
    
    return size;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int proc_list_entries(proc_entry_t *dir, proc_entry_t *entries, u32 *count)
{
    if (!dir || !dir->is_directory || !entries || !count) return -EINVAL;
    
    u32 i = 0;
    for (proc_entry_t *child = dir->children; child && i < *count; child = child->next) {
        entries[i++] = *child;
    }
    
    *count = i;
    return i;
}

procfs_t *procfs_get(void)
{
    return &g_procfs;
}
