/*
 * NEXUS OS - Kernel Module Loader
 * kernel/core/module_loader.c
 *
 * Loadable kernel module support with dependency resolution
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../include/types.h"
#include "../include/kernel.h"

/*===========================================================================*/
/*                         MODULE LOADER CONFIGURATION                       */
/*===========================================================================*/

#define MODULE_MAX_MODULES        256
#define MODULE_MAX_SYMBOLS        4096
#define MODULE_MAX_DEPS           32
#define MODULE_MAX_NAME           64
#define MODULE_MAGIC              0x4E58444D  /* "NXDM" */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct module_symbol {
    char name[MODULE_MAX_NAME];
    void *address;
    u32 size;
    bool exported;
    struct module_symbol *next;
} module_symbol_t;

typedef struct module {
    u32 magic;
    char name[MODULE_MAX_NAME];
    char version[32];
    char author[64];
    char description[256];
    char license[32];
    void *base;
    u32 size;
    u32 flags;
    s32 refcount;
    struct module *deps[MODULE_MAX_DEPS];
    u32 dep_count;
    module_symbol_t *symbols;
    u32 symbol_count;
    int (*init)(void);
    void (*exit)(void);
    bool loaded;
    bool init_called;
} module_t;

typedef struct {
    bool initialized;
    module_t modules[MODULE_MAX_MODULES];
    u32 module_count;
    module_symbol_t kernel_symbols[MODULE_MAX_SYMBOLS];
    u32 kernel_symbol_count;
    spinlock_t lock;
    char module_path[256];
} module_loader_t;

static module_loader_t g_mod_loader;

/*===========================================================================*/
/*                         SYMBOL TABLE                                      */
/*===========================================================================*/

int module_add_symbol(module_t *mod, const char *name, void *addr, u32 size)
{
    if (!mod || !name || !addr) return -EINVAL;
    if (mod->symbol_count >= MODULE_MAX_SYMBOLS) return -ENOMEM;
    
    module_symbol_t *sym = &mod->symbols[mod->symbol_count++];
    strncpy(sym->name, name, MODULE_MAX_NAME - 1);
    sym->address = addr;
    sym->size = size;
    sym->exported = true;
    sym->next = NULL;
    
    return 0;
}

void *module_find_symbol(const char *name)
{
    /* Search kernel symbols */
    for (u32 i = 0; i < g_mod_loader.kernel_symbol_count; i++) {
        if (strcmp(g_mod_loader.kernel_symbols[i].name, name) == 0) {
            return g_mod_loader.kernel_symbols[i].address;
        }
    }
    
    /* Search module symbols */
    for (u32 i = 0; i < g_mod_loader.module_count; i++) {
        module_t *mod = &g_mod_loader.modules[i];
        if (!mod->loaded) continue;
        
        for (u32 j = 0; j < mod->symbol_count; j++) {
            if (strcmp(mod->symbols[j].name, name) == 0) {
                return mod->symbols[j].address;
            }
        }
    }
    
    return NULL;
}

int module_export_kernel_symbol(const char *name, void *addr, u32 size)
{
    if (g_mod_loader.kernel_symbol_count >= MODULE_MAX_SYMBOLS) {
        return -ENOMEM;
    }
    
    module_symbol_t *sym = &g_mod_loader.kernel_symbols[g_mod_loader.kernel_symbol_count++];
    strncpy(sym->name, name, MODULE_MAX_NAME - 1);
    sym->address = addr;
    sym->size = size;
    sym->exported = true;
    
    return 0;
}

/*===========================================================================*/
/*                         ELF PARSING                                       */
/*===========================================================================*/

typedef struct {
    u32 magic;
    u8 class;
    u8 data;
    u8 version;
    u8 osabi;
    u8 pad[8];
    u16 type;
    u16 machine;
    u32 elf_version;
    u64 entry;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct {
    u32 name;
    u32 type;
    u64 flags;
    u64 addr;
    u64 offset;
    u64 size;
    u32 link;
    u32 info;
    u64 addralign;
    u64 entsize;
} elf_section_t;

typedef struct {
    u32 name;
    u8 info;
    u8 other;
    u16 shndx;
    u64 value;
    u64 size;
} elf_symbol_t;

static int parse_elf(module_t *mod, const void *elf_data, u32 elf_size)
{
    elf_header_t *hdr = (elf_header_t *)elf_data;
    
    /* Validate ELF magic */
    if (hdr->magic != 0x464C457F) {  /* 0x7F ELF */
        printk("[MODULE] Invalid ELF magic\n");
        return -EINVAL;
    }
    
    /* Check for x86_64 */
    if (hdr->machine != 0x3E) {
        printk("[MODULE] Unsupported architecture\n");
        return -EINVAL;
    }
    
    /* Parse sections */
    elf_section_t *sections = (elf_section_t *)((u8 *)elf_data + hdr->shoff);
    char *shstrtab = (char *)((u8 *)elf_data + sections[hdr->shstrndx].offset);
    
    /* Find symbol table and string table */
    elf_symbol_t *symtab = NULL;
    char *strtab = NULL;
    u64 symtab_size = 0;
    u64 strtab_size = 0;
    
    for (u32 i = 0; i < hdr->shnum; i++) {
        char *name = &shstrtab[sections[i].name];
        
        if (sections[i].type == 2) {  /* SHT_SYMTAB */
            symtab = (elf_symbol_t *)((u8 *)elf_data + sections[i].offset);
            symtab_size = sections[i].size;
        } else if (sections[i].type == 3) {  /* SHT_STRTAB */
            if (strcmp(name, ".strtab") == 0) {
                strtab = (char *)((u8 *)elf_data + sections[i].offset);
                strtab_size = sections[i].size;
            }
        }
    }
    
    if (!symtab || !strtab) {
        printk("[MODULE] No symbol table found\n");
        return -EINVAL;
    }
    
    /* Export symbols */
    u32 sym_count = symtab_size / sizeof(elf_symbol_t);
    for (u32 i = 0; i < sym_count; i++) {
        if (symtab[i].value != 0 && symtab[i].size > 0) {
            char *name = &strtab[symtab[i].name];
            void *addr = (void *)((u8 *)mod->base + symtab[i].value);
            module_add_symbol(mod, name, addr, symtab[i].size);
        }
    }
    
    return 0;
}

/*===========================================================================*/
/*                         MODULE LOADING                                    */
/*===========================================================================*/

module_t *module_load(const char *path, const char *name)
{
    spinlock_lock(&g_mod_loader.lock);
    
    if (g_mod_loader.module_count >= MODULE_MAX_MODULES) {
        spinlock_unlock(&g_mod_loader.lock);
        printk("[MODULE] Maximum module count reached\n");
        return NULL;
    }
    
    /* Check if already loaded */
    for (u32 i = 0; i < g_mod_loader.module_count; i++) {
        if (strcmp(g_mod_loader.modules[i].name, name) == 0) {
            spinlock_unlock(&g_mod_loader.lock);
            printk("[MODULE] Module '%s' already loaded\n", name);
            return &g_mod_loader.modules[i];
        }
    }
    
    module_t *mod = &g_mod_loader.modules[g_mod_loader.module_count];
    memset(mod, 0, sizeof(module_t));
    strncpy(mod->name, name, MODULE_MAX_NAME - 1);
    mod->magic = MODULE_MAGIC;
    
    /* In real implementation, would read file from filesystem */
    /* For now, simulate loading */
    printk("[MODULE] Loading module '%s' from %s\n", name, path);
    
    /* Allocate memory for module */
    mod->size = 64 * 1024;  /* Mock size */
    mod->base = kmalloc(mod->size);
    if (!mod->base) {
        spinlock_unlock(&g_mod_loader.lock);
        printk("[MODULE] Failed to allocate memory\n");
        return NULL;
    }
    
    /* Parse ELF and export symbols */
    /* parse_elf(mod, mod->base, mod->size); */
    
    mod->loaded = true;
    g_mod_loader.module_count++;
    
    spinlock_unlock(&g_mod_loader.lock);
    
    printk("[MODULE] Module '%s' loaded successfully\n", name);
    return mod;
}

int module_init(module_t *mod)
{
    if (!mod || !mod->loaded) return -EINVAL;
    if (mod->init_called) return 0;
    
    printk("[MODULE] Initializing module '%s'...\n", mod->name);
    
    /* Check dependencies */
    for (u32 i = 0; i < mod->dep_count; i++) {
        if (!mod->deps[i]->loaded) {
            printk("[MODULE] Missing dependency: %s\n", mod->deps[i]->name);
            return -ENODEV;
        }
    }
    
    /* Call init function */
    if (mod->init) {
        int ret = mod->init();
        if (ret < 0) {
            printk("[MODULE] Module init failed: %d\n", ret);
            return ret;
        }
    }
    
    mod->init_called = true;
    printk("[MODULE] Module '%s' initialized\n", mod->name);
    
    return 0;
}

int module_unload(module_t *mod)
{
    if (!mod || !mod->loaded) return -EINVAL;
    if (mod->refcount > 0) return -EBUSY;
    
    spinlock_lock(&g_mod_loader.lock);
    
    printk("[MODULE] Unloading module '%s'...\n", mod->name);
    
    /* Call exit function */
    if (mod->exit) {
        mod->exit();
    }
    
    /* Free memory */
    if (mod->base) {
        kfree(mod->base);
        mod->base = NULL;
    }
    
    mod->loaded = false;
    mod->init_called = false;
    
    /* Remove from list */
    for (u32 i = 0; i < g_mod_loader.module_count; i++) {
        if (&g_mod_loader.modules[i] == mod) {
            /* Shift modules */
            for (u32 j = i; j < g_mod_loader.module_count - 1; j++) {
                g_mod_loader.modules[j] = g_mod_loader.modules[j + 1];
            }
            g_mod_loader.module_count--;
            break;
        }
    }
    
    spinlock_unlock(&g_mod_loader.lock);
    
    printk("[MODULE] Module '%s' unloaded\n", mod->name);
    return 0;
}

module_t *module_get(const char *name)
{
    for (u32 i = 0; i < g_mod_loader.module_count; i++) {
        if (strcmp(g_mod_loader.modules[i].name, name) == 0) {
            return &g_mod_loader.modules[i];
        }
    }
    return NULL;
}

int module_list(module_t *modules, u32 *count)
{
    if (!modules || !count) return -EINVAL;
    
    u32 copy = (*count < g_mod_loader.module_count) ? 
               *count : g_mod_loader.module_count;
    memcpy(modules, g_mod_loader.modules, sizeof(module_t) * copy);
    *count = copy;
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int module_loader_init(const char *module_path)
{
    printk("[MODULE] ========================================\n");
    printk("[MODULE] NEXUS OS Kernel Module Loader\n");
    printk("[MODULE] ========================================\n");
    
    memset(&g_mod_loader, 0, sizeof(module_loader_t));
    g_mod_loader.initialized = true;
    spinlock_init(&g_mod_loader.lock);
    strncpy(g_mod_loader.module_path, module_path, sizeof(g_mod_loader.module_path) - 1);
    
    /* Export essential kernel symbols */
    module_export_kernel_symbol("printk", (void *)printk, 0);
    module_export_kernel_symbol("kmalloc", (void *)kmalloc, 0);
    module_export_kernel_symbol("kfree", (void *)kfree, 0);
    module_export_kernel_symbol("memcpy", (void *)memcpy, 0);
    module_export_kernel_symbol("memset", (void *)memset, 0);
    module_export_kernel_symbol("strcmp", (void *)strcmp, 0);
    module_export_kernel_symbol("strcpy", (void *)strcpy, 0);
    
    printk("[MODULE] Module loader initialized\n");
    printk("[MODULE] Module path: %s\n", g_mod_loader.module_path);
    printk("[MODULE] Exported %d kernel symbols\n", g_mod_loader.kernel_symbol_count);
    
    return 0;
}

int module_loader_shutdown(void)
{
    printk("[MODULE] Shutting down module loader...\n");
    
    /* Unload all modules */
    for (s32 i = g_mod_loader.module_count - 1; i >= 0; i--) {
        module_unload(&g_mod_loader.modules[i]);
    }
    
    g_mod_loader.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int module_add_dependency(module_t *mod, const char *dep_name)
{
    if (!mod || mod->dep_count >= MODULE_MAX_DEPS) {
        return -EINVAL;
    }
    
    module_t *dep = module_get(dep_name);
    if (!dep) {
        printk("[MODULE] Dependency not found: %s\n", dep_name);
        return -ENOENT;
    }
    
    mod->deps[mod->dep_count++] = dep;
    return 0;
}

int module_set_info(module_t *mod, const char *version, const char *author,
                    const char *description, const char *license)
{
    if (!mod) return -EINVAL;
    
    if (version) strncpy(mod->version, version, sizeof(mod->version) - 1);
    if (author) strncpy(mod->author, author, sizeof(mod->author) - 1);
    if (description) strncpy(mod->description, description, sizeof(mod->description) - 1);
    if (license) strncpy(mod->license, license, sizeof(mod->license) - 1);
    
    return 0;
}

module_loader_t *module_loader_get(void)
{
    return &g_mod_loader;
}
