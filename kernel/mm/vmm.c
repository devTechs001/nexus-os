/*
 * NEXUS OS - Virtual Memory Manager
 * kernel/mm/vmm.c
 */

#include "../include/kernel.h"
#include "mm.h"

/*===========================================================================*/
/*                         VIRTUAL MEMORY MANAGER                            */
/*===========================================================================*/

/* Page Table Structure (x86_64 4-level paging) */
#define PT_LEVELS           4
#define PT_ENTRIES          512
#define PT_ENTRY_SIZE       8
#define PT_PAGE_SIZE        (PT_ENTRIES * PT_ENTRY_SIZE)

/* Page Table Shift Values */
#define PML4_SHIFT          39
#define PDPT_SHIFT          30
#define PD_SHIFT            21
#define PT_SHIFT            12

/* Page Table Masks */
#define PML4_MASK           0x0000FF8000000000ULL
#define PDPT_MASK           0x0000007FC0000000ULL
#define PD_MASK             0x000000003FE00000ULL
#define PT_MASK             0x00000000001FF000ULL
#define PAGE_OFFSET_MASK    0x0000000000000FFFULL

/* Page Table Entry Flags */
#define PTE_PRESENT         (1ULL << 0)
#define PTE_WRITABLE        (1ULL << 1)
#define PTE_USER            (1ULL << 2)
#define PTE_WRITETHROUGH    (1ULL << 3)
#define PTE_NOCACHE         (1ULL << 4)
#define PTE_ACCESSED        (1ULL << 5)
#define PTE_DIRTY           (1ULL << 6)
#define PTE_HUGE            (1ULL << 7)
#define PTE_GLOBAL          (1ULL << 8)
#define PTE_NX              (1ULL << 63)

/* Kernel Address Space */
static struct address_space *kernel_as = NULL;

/* Current Address Space */
static struct address_space *current_as = NULL;

/* Spinlock for VMM */
static spinlock_t vmm_lock = { .lock = 0 };

/* Page Table Cache */
static u64 *kernel_pml4 = NULL;

/*===========================================================================*/
/*                         PAGE TABLE OPERATIONS                             */
/*===========================================================================*/

/**
 * pte_create - Create page table entry
 */
static inline u64 pte_create(phys_addr_t paddr, u32 flags)
{
    u64 entry = (paddr & PAGE_MASK);

    if (flags & VMA_READ)
        entry |= PTE_PRESENT;
    if (flags & VMA_WRITE)
        entry |= PTE_WRITABLE;
    if (flags & VMA_USER)
        entry |= PTE_USER;
    if (flags & VMA_IO)
        entry |= PTE_NOCACHE;
    if (!(flags & VMA_EXEC))
        entry |= PTE_NX;

    entry |= PTE_ACCESSED;

    return entry;
}

/**
 * pte_get_addr - Get physical address from page table entry
 */
static inline phys_addr_t pte_get_addr(u64 entry)
{
    return (entry & PAGE_MASK);
}

/**
 * pte_is_present - Check if page table entry is present
 */
static inline bool pte_is_present(u64 entry)
{
    return (entry & PTE_PRESENT) != 0;
}

/**
 * pte_is_user - Check if page table entry is user accessible
 */
static inline bool pte_is_user(u64 entry)
{
    return (entry & PTE_USER) != 0;
}

/**
 * pte_is_writable - Check if page table entry is writable
 */
static inline bool pte_is_writable(u64 entry)
{
    return (entry & PTE_WRITABLE) != 0;
}

/**
 * get_pml4_index - Get PML4 index from virtual address
 */
static inline u64 get_pml4_index(virt_addr_t vaddr)
{
    return (vaddr & PML4_MASK) >> PML4_SHIFT;
}

/**
 * get_pdpt_index - Get PDPT index from virtual address
 */
static inline u64 get_pdpt_index(virt_addr_t vaddr)
{
    return (vaddr & PDPT_MASK) >> PDPT_SHIFT;
}

/**
 * get_pd_index - Get PD index from virtual address
 */
static inline u64 get_pd_index(virt_addr_t vaddr)
{
    return (vaddr & PD_MASK) >> PD_SHIFT;
}

/**
 * get_pt_index - Get PT index from virtual address
 */
static inline u64 get_pt_index(virt_addr_t vaddr)
{
    return (vaddr & PT_MASK) >> PT_SHIFT;
}

/**
 * get_page_offset - Get page offset from virtual address
 */
static inline u64 get_page_offset(virt_addr_t vaddr)
{
    return vaddr & PAGE_OFFSET_MASK;
}

/**
 * alloc_page_table - Allocate a new page table page
 */
static u64 *alloc_page_table(void)
{
    phys_addr_t paddr;
    u64 *pt;

    paddr = pmm_alloc_page();
    if (paddr == 0) {
        return NULL;
    }

    pt = (u64 *)vmm_phys_to_virt(paddr);
    memset(pt, 0, PAGE_SIZE);

    return pt;
}

/**
 * free_page_table - Free a page table page
 */
static void free_page_table(u64 *pt)
{
    phys_addr_t paddr;

    if (!pt) {
        return;
    }

    paddr = vmm_virt_to_phys((virt_addr_t)pt);
    pmm_free_page(paddr);
}

/*===========================================================================*/
/*                         PAGE TABLE WALKING                                */
/*===========================================================================*/

/**
 * vmm_get_pml4 - Get pointer to PML4 entry
 */
static u64 *vmm_get_pml4(virt_addr_t vaddr)
{
    u64 idx = get_pml4_index(vaddr);

    if (idx >= PT_ENTRIES) {
        return NULL;
    }

    return &kernel_pml4[idx];
}

/**
 * vmm_get_pdpt - Get pointer to PDPT entry
 */
static u64 *vmm_get_pdpt(u64 *pml4e, bool create)
{
    u64 *pdpt;
    phys_addr_t paddr;

    if (!pml4e) {
        return NULL;
    }

    if (!pte_is_present(*pml4e)) {
        if (!create) {
            return NULL;
        }

        pdpt = alloc_page_table();
        if (!pdpt) {
            return NULL;
        }

        paddr = vmm_virt_to_phys((virt_addr_t)pdpt);
        *pml4e = pte_create(paddr, VMA_READ | VMA_WRITE);

        return pdpt;
    }

    paddr = pte_get_addr(*pml4e);
    return (u64 *)vmm_phys_to_virt(paddr);
}

/**
 * vmm_get_pd - Get pointer to PD entry
 */
static u64 *vmm_get_pd(u64 *pdpte, bool create)
{
    u64 *pd;
    phys_addr_t paddr;

    if (!pdpte) {
        return NULL;
    }

    if (!pte_is_present(*pdpte)) {
        if (!create) {
            return NULL;
        }

        pd = alloc_page_table();
        if (!pd) {
            return NULL;
        }

        paddr = vmm_virt_to_phys((virt_addr_t)pd);
        *pdpte = pte_create(paddr, VMA_READ | VMA_WRITE);

        return pd;
    }

    paddr = pte_get_addr(*pdpte);
    return (u64 *)vmm_phys_to_virt(paddr);
}

/**
 * vmm_get_pt - Get pointer to PT entry
 */
static u64 *vmm_get_pt(u64 *pde, bool create)
{
    u64 *pt;
    phys_addr_t paddr;

    if (!pde) {
        return NULL;
    }

    if (!pte_is_present(*pde)) {
        if (!create) {
            return NULL;
        }

        pt = alloc_page_table();
        if (!pt) {
            return NULL;
        }

        paddr = vmm_virt_to_phys((virt_addr_t)pt);
        *pde = pte_create(paddr, VMA_READ | VMA_WRITE);

        return pt;
    }

    paddr = pte_get_addr(*pde);
    return (u64 *)vmm_phys_to_virt(paddr);
}

/**
 * vmm_walk_page_table - Walk page table to get PTE
 */
static u64 *vmm_walk_page_table(u64 *pml4, virt_addr_t vaddr, bool create)
{
    u64 *pml4e;
    u64 *pdpte;
    u64 *pde;
    u64 *pte;

    /* Get PML4 entry */
    pml4e = &pml4[get_pml4_index(vaddr)];

    /* Get PDPT */
    pdpte = vmm_get_pdpt(pml4e, create);
    if (!pdpte) {
        return NULL;
    }

    /* Get PD */
    pde = &pdpte[get_pd_index(vaddr)];
    pdpte = vmm_get_pd(pde, create);
    if (!pdpte) {
        return NULL;
    }

    /* Get PT */
    pte = &pdpte[get_pt_index(vaddr)];

    return pte;
}

/*===========================================================================*/
/*                         TLB OPERATIONS                                    */
/*===========================================================================*/

/**
 * vmm_flush_tlb - Flush entire TLB
 */
void vmm_flush_tlb(void)
{
    /* Reload CR3 to flush TLB */
    u64 cr3;

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(cr3));

    barrier();
}

/**
 * vmm_flush_tlb_addr - Flush TLB for specific address
 */
void vmm_flush_tlb_addr(virt_addr_t vaddr)
{
    /* Invalidate single page using INVLPG */
    __asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
}

/**
 * vmm_flush_tlb_as - Flush TLB for address space
 */
void vmm_flush_tlb_as(struct address_space *as)
{
    if (as == current_as) {
        vmm_flush_tlb();
    }
    /* Otherwise, TLB will be flushed on context switch */
}

/*===========================================================================*/
/*                         ADDRESS TRANSLATION                               */
/*===========================================================================*/

/**
 * vmm_virt_to_phys - Translate virtual address to physical
 */
phys_addr_t vmm_virt_to_phys(virt_addr_t vaddr)
{
    u64 *pte;
    phys_addr_t paddr;

    /* For kernel direct-mapped region */
    if (vaddr >= KERNEL_VIRTUAL_BASE) {
        return vaddr - KERNEL_VIRTUAL_BASE;
    }

    spin_lock(&vmm_lock);

    pte = vmm_walk_page_table(kernel_pml4, vaddr, false);
    if (!pte || !pte_is_present(*pte)) {
        spin_unlock(&vmm_lock);
        return 0;
    }

    paddr = pte_get_addr(*pte) + get_page_offset(vaddr);

    spin_unlock(&vmm_lock);

    return paddr;
}

/**
 * vmm_phys_to_virt - Translate physical address to virtual
 */
virt_addr_t vmm_phys_to_virt(phys_addr_t paddr)
{
    return paddr + KERNEL_VIRTUAL_BASE;
}

/*===========================================================================*/
/*                         PAGE MAPPING                                      */
/*===========================================================================*/

/**
 * vmm_map - Map virtual address to physical address
 */
result_t vmm_map(virt_addr_t vaddr, phys_addr_t paddr, u32 flags)
{
    u64 *pte;
    u64 entry;

    if (!IS_ALIGNED(vaddr, PAGE_SIZE)) {
        return EINVAL;
    }

    if (!IS_ALIGNED(paddr, PAGE_SIZE)) {
        return EINVAL;
    }

    spin_lock(&vmm_lock);

    pte = vmm_walk_page_table(kernel_pml4, vaddr, true);
    if (!pte) {
        spin_unlock(&vmm_lock);
        return ENOMEM;
    }

    entry = pte_create(paddr, flags);
    *pte = entry;

    /* Flush TLB for this address */
    vmm_flush_tlb_addr(vaddr);

    spin_unlock(&vmm_lock);

    return OK;
}

/**
 * vmm_unmap - Unmap virtual address
 */
result_t vmm_unmap(virt_addr_t vaddr)
{
    u64 *pte;

    if (!IS_ALIGNED(vaddr, PAGE_SIZE)) {
        return EINVAL;
    }

    spin_lock(&vmm_lock);

    pte = vmm_walk_page_table(kernel_pml4, vaddr, false);
    if (!pte || !pte_is_present(*pte)) {
        spin_unlock(&vmm_lock);
        return ENOENT;
    }

    *pte = 0;

    /* Flush TLB for this address */
    vmm_flush_tlb_addr(vaddr);

    spin_unlock(&vmm_lock);

    return OK;
}

/**
 * vmm_map_range - Map range of virtual addresses
 */
result_t vmm_map_range(virt_addr_t vaddr, phys_addr_t paddr, size_t size, u32 flags)
{
    size_t num_pages;
    size_t i;
    result_t ret;

    if (!IS_ALIGNED(vaddr, PAGE_SIZE)) {
        return EINVAL;
    }

    if (!IS_ALIGNED(paddr, PAGE_SIZE)) {
        return EINVAL;
    }

    num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    spin_lock(&vmm_lock);

    for (i = 0; i < num_pages; i++) {
        ret = vmm_map(vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags);
        if (ret != OK) {
            /* Unwind on error */
            while (i > 0) {
                i--;
                vmm_unmap(vaddr + (i * PAGE_SIZE));
            }
            spin_unlock(&vmm_lock);
            return ret;
        }
    }

    spin_unlock(&vmm_lock);

    return OK;
}

/**
 * vmm_unmap_range - Unmap range of virtual addresses
 */
result_t vmm_unmap_range(virt_addr_t vaddr, size_t size)
{
    size_t num_pages;
    size_t i;
    result_t ret;

    if (!IS_ALIGNED(vaddr, PAGE_SIZE)) {
        return EINVAL;
    }

    num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    spin_lock(&vmm_lock);

    for (i = 0; i < num_pages; i++) {
        ret = vmm_unmap(vaddr + (i * PAGE_SIZE));
        if (ret != OK && ret != ENOENT) {
            spin_unlock(&vmm_lock);
            return ret;
        }
    }

    spin_unlock(&vmm_lock);

    return OK;
}

/*===========================================================================*/
/*                         VMA MANAGEMENT                                    */
/*===========================================================================*/

/**
 * vma_create - Create new VMA
 */
static struct vm_area *vma_create(virt_addr_t start, virt_addr_t end, u32 flags)
{
    struct vm_area *vma;

    vma = (struct vm_area *)kmalloc(sizeof(struct vm_area));
    if (!vma) {
        return NULL;
    }

    memset(vma, 0, sizeof(struct vm_area));

    vma->start = start;
    vma->end = end;
    vma->flags = flags;
    vma->next = NULL;
    vma->prev = NULL;

    return vma;
}

/**
 * vma_destroy - Destroy VMA
 */
static void vma_destroy(struct vm_area *vma)
{
    if (!vma) {
        return;
    }

    kfree(vma);
}

/**
 * vma_find - Find VMA containing address
 */
static struct vm_area *vma_find(struct address_space *as, virt_addr_t addr)
{
    struct vm_area *vma;

    if (!as) {
        return NULL;
    }

    vma = as->vmas;

    while (vma) {
        if (addr >= vma->start && addr < vma->end) {
            return vma;
        }
        vma = vma->next;
    }

    return NULL;
}

/**
 * vma_insert - Insert VMA into address space
 */
static result_t vma_insert(struct address_space *as, struct vm_area *vma)
{
    struct vm_area *curr;
    struct vm_area *prev;

    if (!as || !vma) {
        return EINVAL;
    }

    spin_lock(&as->lock);

    /* Check for overlaps */
    curr = as->vmas;
    while (curr) {
        if ((vma->start < curr->end) && (vma->end > curr->start)) {
            spin_unlock(&as->lock);
            return EEXIST;
        }
        curr = curr->next;
    }

    /* Insert in sorted order */
    prev = NULL;
    curr = as->vmas;

    while (curr && curr->start < vma->start) {
        prev = curr;
        curr = curr->next;
    }

    vma->next = curr;
    vma->prev = prev;

    if (prev) {
        prev->next = vma;
    } else {
        as->vmas = vma;
    }

    if (curr) {
        curr->prev = vma;
    }

    as->size += (vma->end - vma->start);

    spin_unlock(&as->lock);

    return OK;
}

/**
 * vma_remove - Remove VMA from address space
 */
static result_t vma_remove(struct address_space *as, struct vm_area *vma)
{
    if (!as || !vma) {
        return EINVAL;
    }

    spin_lock(&as->lock);

    if (vma->prev) {
        vma->prev->next = vma->next;
    } else {
        as->vmas = vma->next;
    }

    if (vma->next) {
        vma->next->prev = vma->prev;
    }

    as->size -= (vma->end - vma->start);

    spin_unlock(&as->lock);

    vma_destroy(vma);

    return OK;
}

/*===========================================================================*/
/*                         ADDRESS SPACE MANAGEMENT                          */
/*===========================================================================*/

/**
 * vmm_create_as - Create new address space
 */
struct address_space *vmm_create_as(void)
{
    struct address_space *as;
    u64 *pml4;
    phys_addr_t pml4_paddr;

    as = (struct address_space *)kmalloc(sizeof(struct address_space));
    if (!as) {
        return NULL;
    }

    memset(as, 0, sizeof(struct address_space));

    /* Allocate PML4 table */
    pml4 = alloc_page_table();
    if (!pml4) {
        kfree(as);
        return NULL;
    }

    pml4_paddr = vmm_virt_to_phys((virt_addr_t)pml4);

    /* Copy kernel mappings to new page table */
    /* Kernel mappings are in the higher half */
    for (u64 i = 256; i < PT_ENTRIES; i++) {
        pml4[i] = kernel_pml4[i];
    }

    as->vmas = NULL;
    as->size = 0;
    as->flags = 0;
    as->private = pml4;
    spin_lock_init(&as->lock);

    return as;
}

/**
 * vmm_destroy_as - Destroy address space
 */
void vmm_destroy_as(struct address_space *as)
{
    struct vm_area *vma;
    struct vm_area *next;
    u64 *pml4;

    if (!as) {
        return;
    }

    spin_lock(&vmm_lock);

    /* Free all VMAs */
    vma = as->vmas;
    while (vma) {
        next = vma->next;
        vma_destroy(vma);
        vma = next;
    }

    /* Free page tables (except kernel mappings) */
    pml4 = (u64 *)as->private;
    if (pml4) {
        for (u64 i = 0; i < 256; i++) {
            if (pte_is_present(pml4[i])) {
                u64 *pdpt = (u64 *)vmm_phys_to_virt(pte_get_addr(pml4[i]));
                free_page_table(pdpt);
            }
        }
        free_page_table(pml4);
    }

    spin_unlock(&vmm_lock);

    kfree(as);
}

/**
 * vmm_switch_as - Switch to different address space
 */
result_t vmm_switch_as(struct address_space *as)
{
    u64 *pml4;
    phys_addr_t pml4_paddr;

    if (!as) {
        return EINVAL;
    }

    spin_lock(&vmm_lock);

    pml4 = (u64 *)as->private;
    pml4_paddr = vmm_virt_to_phys((virt_addr_t)pml4);

    /* Load new CR3 */
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pml4_paddr));

    current_as = as;

    spin_unlock(&vmm_lock);

    return OK;
}

/**
 * vmm_get_current_as - Get current address space
 */
struct address_space *vmm_get_current_as(void)
{
    return current_as;
}

/*===========================================================================*/
/*                         KERNEL ADDRESS SPACE                              */
/*===========================================================================*/

/**
 * vmm_init_kernel_as - Initialize kernel address space
 */
static void vmm_init_kernel_as(void)
{
    phys_addr_t paddr;
    u64 entry;

    kernel_as = (struct address_space *)kmalloc(sizeof(struct address_space));
    if (!kernel_as) {
        pr_err("VMM: Failed to allocate kernel address space\n");
        return;
    }

    memset(kernel_as, 0, sizeof(struct address_space));
    spin_lock_init(&kernel_as->lock);

    /* Allocate PML4 table */
    kernel_pml4 = alloc_page_table();
    if (!kernel_pml4) {
        pr_err("VMM: Failed to allocate PML4 table\n");
        return;
    }

    kernel_as->private = kernel_pml4;
    kernel_as->vmas = NULL;
    kernel_as->size = 0;

    /* Map kernel code and data */
    paddr = KERNEL_PHYSICAL_BASE;

    /* Create identity mapping for kernel */
    for (virt_addr_t vaddr = KERNEL_VIRTUAL_BASE;
         vaddr < KERNEL_VIRTUAL_BASE + (64 * MB(1));
         vaddr += PAGE_SIZE) {

        entry = pte_create(paddr, VMA_READ | VMA_WRITE | VMA_EXEC);
        kernel_pml4[get_pml4_index(vaddr)] = entry;

        paddr += PAGE_SIZE;
    }

    current_as = kernel_as;

    pr_info("VMM: Kernel address space initialized\n");
}

/*===========================================================================*/
/*                         VMM INITIALIZATION                                */
/*===========================================================================*/

/**
 * vmm_init - Initialize Virtual Memory Manager
 */
void vmm_init(void)
{
    pr_info("Initializing Virtual Memory Manager...\n");

    /* Initialize lock */
    spin_lock_init(&vmm_lock);

    /* Initialize kernel address space */
    vmm_init_kernel_as();

    /* Flush TLB */
    vmm_flush_tlb();

    pr_info("VMM: 4-level paging enabled\n");
    pr_info("VMM: PML4 at %p\n", kernel_pml4);
    pr_info("Virtual Memory Manager initialized.\n");
}

/*===========================================================================*/
/*                         MEMORY MAPPING (mmap)                             */
/*===========================================================================*/

/**
 * mmap - Map files or devices into memory
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    struct vm_area *vma;
    virt_addr_t vaddr;
    u32 vma_flags = 0;

    /* Convert prot to VMA flags */
    if (prot & PROT_READ)
        vma_flags |= VMA_READ;
    if (prot & PROT_WRITE)
        vma_flags |= VMA_WRITE;
    if (prot & PROT_EXEC)
        vma_flags |= VMA_EXEC;

    if (flags & MAP_SHARED)
        vma_flags |= VMA_SHARED;
    if (flags & MAP_ANONYMOUS)
        vma_flags |= VMA_ANONYMOUS;

    /* Find free virtual address region */
    if (addr) {
        vaddr = (virt_addr_t)addr;
    } else {
        /* Allocate new region */
        vaddr = KERNEL_HEAP_START;
        /* In real implementation, find free hole in address space */
    }

    vaddr = PAGE_ALIGN(vaddr);

    /* Create VMA */
    vma = vma_create(vaddr, vaddr + length, vma_flags);
    if (!vma) {
        return NULL;
    }

    /* Insert VMA into address space */
    if (vma_insert(current_as, vma) != OK) {
        vma_destroy(vma);
        return NULL;
    }

    /* Map pages for anonymous mappings */
    if (flags & MAP_ANONYMOUS) {
        for (virt_addr_t va = vaddr; va < vaddr + length; va += PAGE_SIZE) {
            phys_addr_t pa = pmm_alloc_page();
            if (pa == 0) {
                /* Cleanup on failure */
                vma_remove(current_as, vma);
                return NULL;
            }
            vmm_map(va, pa, vma_flags);
        }
    }

    return (void *)vaddr;
}

/**
 * munmap - Unmap memory region
 */
int munmap(void *addr, size_t length)
{
    struct vm_area *vma;
    virt_addr_t vaddr = (virt_addr_t)addr;

    vma = vma_find(current_as, vaddr);
    if (!vma) {
        return -1;
    }

    /* Unmap pages */
    vmm_unmap_range(vaddr, length);

    /* Remove VMA */
    vma_remove(current_as, vma);

    return 0;
}

/**
 * mprotect - Change protection of memory region
 */
int mprotect(void *addr, size_t length, int prot)
{
    struct vm_area *vma;
    virt_addr_t vaddr = (virt_addr_t)addr;
    u32 flags = 0;

    vma = vma_find(current_as, vaddr);
    if (!vma) {
        return -1;
    }

    /* Convert prot to flags */
    if (prot & PROT_READ)
        flags |= VMA_READ;
    if (prot & PROT_WRITE)
        flags |= VMA_WRITE;
    if (prot & PROT_EXEC)
        flags |= VMA_EXEC;

    vma->flags = flags;

    /* Update page table entries */
    for (virt_addr_t va = vaddr; va < vaddr + length; va += PAGE_SIZE) {
        u64 *pte = vmm_walk_page_table(kernel_pml4, va, false);
        if (pte) {
            phys_addr_t paddr = pte_get_addr(*pte);
            *pte = pte_create(paddr, flags);
        }
    }

    vmm_flush_tlb();

    return 0;
}

/**
 * msync - Synchronize memory with physical storage
 */
int msync(void *addr, size_t length, int flags)
{
    /* For now, just flush caches */
    __asm__ __volatile__("wbinvd" ::: "memory");
    return 0;
}
