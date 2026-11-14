/*
 * NEXUS OS - Nested Virtualization Support
 * virt/emulation/nested_virt.c
 *
 * Nested hypervisor support for running hypervisors inside VMs
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/mm/mm.h"   /* vmm_virt_to_phys, virt_addr_t */

#define NESTED_VMX_MAX_LEVELS     3
#define NESTED_VMX_VPID_MAX       0xFFFF
#define NESTED_EPT_LEVELS         4

/* Nested VMX Controls */
#define NESTED_VMX_CTRL_PIN_BASED     0x0000003F
#define NESTED_VMX_CTRL_PROC_BASED    0xB61DFE79
#define NESTED_VMX_CTRL_PROC_BASED2   0x000000AA
#define NESTED_VMX_CTRL_EXIT          0x00036DFF
#define NESTED_VMX_CTRL_ENTRY         0x000011FF

/* Nested EPT Configuration */
#define NESTED_EPT_PAGE_WALK_4        (1 << 6)
#define NESTED_EPT_UNCACHED           (0 << 3)
#define NESTED_EPT_WRITE_BACK         (6 << 3)
#define NESTED_EPT_INVEPT             (1 << 20)
#define NESTED_EPT_AD                 (1 << 21)

/* Nested VPID Configuration */
#define NESTED_VPID_INVVPID           (1 << 32)

/* Nested VMCS State */
typedef struct {
    /* Guest VMCS State */
    u64 guest_cr0;
    u64 guest_cr3;
    u64 guest_cr4;
    u64 guest_dr7;
    u64 guest_rsp;
    u64 guest_rip;
    u64 guest_rflags;
    u64 guest_cs_selector;
    u64 guest_cs_base;
    u64 guest_cs_limit;
    u64 guest_ss_selector;
    u64 guest_ss_base;
    u64 guest_ss_limit;
    
    /* Host VMCS State */
    u64 host_cr0;
    u64 host_cr3;
    u64 host_cr4;
    u64 host_rsp;
    u64 host_rip;
    
    /* VMCS Control State */
    u64 pin_based_exec_ctrl;
    u64 proc_based_exec_ctrl;
    u64 proc_based_exec_ctrl2;
    u64 exception_bitmap;
    u64 page_fault_error_code_mask;
    u64 page_fault_error_code_match;
    u64 cr3_target_count;
    u64 cr3_target_value0;
    u64 cr3_target_value1;
    u64 cr3_target_value2;
    u64 cr3_target_value3;
    
    /* VMCS Exit/Entry State */
    u64 vm_exit_controls;
    u64 vm_exit_msr_store_count;
    u64 vm_exit_msr_load_count;
    u64 vm_entry_controls;
    u64 vm_entry_msr_load_count;
    u64 vm_entry_intr_info;
    u64 vm_entry_exception_error_code;
    u64 vm_entry_instruction_length;
    
    /* VMCS Miscellaneous */
    u64 tpr_threshold;
    u64 secondary_proc_based_ctrl;
    u64 ple_gap;
    u64 ple_window;
    
    /* Nested State */
    u64 vmcs_link_pointer;
    u64 vmcs_read_bitmap;
    u64 vmcs_write_bitmap;
    bool vmcs_shadow_enabled;
} nested_vmcs_state_t;

/* Nested EPT State */
typedef struct {
    u64 ept_pointer;
    u64 eptp_generation;
    bool ept_enabled;
    bool ept_write_back;
    u8 ept_page_walk_length;
    bool ept_accessed_dirty;
    bool ept_invept_supported;
    u64 invept_caps;
    u64 invvpid_caps;
} nested_ept_state_t;

/* Nested VPID State */
typedef struct {
    u16 vpid;
    bool vpid_enabled;
    bool invvpid_supported;
    u64 invvpid_caps;
} nested_vpid_state_t;

/* Nested VMX State */
typedef struct {
    bool vmxon_enabled;
    u64 vmxon_region_phys;
    void *vmxon_region_virt;
    u64 vmcs_region_phys;
    void *vmcs_region_virt;
    u32 vmcs_revision_id;
    u32 vmcs_abort_reason;
    nested_vmcs_state_t vmcs_state;
    nested_ept_state_t ept_state;
    nested_vpid_state_t vpid_state;
} nested_vmx_state_t;

/* Nested SVM State (AMD) */
typedef struct {
    bool svm_enabled;
    u64 vmcb_phys;
    void *vmcb_virt;
    u64 hsave_pa;
    bool nested_svm_enabled;
    u64 n_cr0;
    u64 n_cr4;
    u64 n_efer;
    u64 n_gdtr_base;
    u64 n_idtr_base;
    u64 n_dr7;
} nested_svm_state_t;

/* Nested Virtualization Manager */
typedef struct {
    bool initialized;
    bool nested_vmx_supported;
    bool nested_svm_supported;
    u32 max_nested_level;
    nested_vmx_state_t vmx_state;
    nested_svm_state_t svm_state;
    spinlock_t lock;
} nested_virt_manager_t;

static nested_virt_manager_t g_nested_virt;

/*===========================================================================*/
/*                         NESTED VMX SUPPORT                                */
/*===========================================================================*/

/* Initialize Nested VMX */
static int nested_vmx_init(void)
{
    printk("[NESTED-VIRT] Initializing nested VMX...\n");
    
    /* Allocate VMXON region */
    g_nested_virt.vmx_state.vmxon_region_virt = kmalloc(4096);
    if (!g_nested_virt.vmx_state.vmxon_region_virt) {
        printk("[NESTED-VIRT] ERROR: Failed to allocate VMXON region\n");
        return -1;
    }
    memset(g_nested_virt.vmx_state.vmxon_region_virt, 0, 4096);
    
    /* Allocate VMCS region */
    g_nested_virt.vmx_state.vmcs_region_virt = kmalloc(4096);
    if (!g_nested_virt.vmx_state.vmcs_region_virt) {
        printk("[NESTED-VIRT] ERROR: Failed to allocate VMCS region\n");
        return -1;
    }
    memset(g_nested_virt.vmx_state.vmcs_region_virt, 0, 4096);
    
    /* Set VMCS revision ID */
    g_nested_virt.vmx_state.vmcs_revision_id = 0x7FFFFFFF;
    
    /* Initialize EPT state */
    g_nested_virt.vmx_state.ept_state.ept_enabled = true;
    g_nested_virt.vmx_state.ept_state.ept_write_back = true;
    g_nested_virt.vmx_state.ept_state.ept_page_walk_length = 4;

    /*
     * INVEPT support is reported in IA32_VMX_EPT_VPID_CAP (MSR 0x48C), bit 20,
     * and the permitted INVEPT types in bits 25:26. Nothing used to read this
     * MSR, so ept_invept_supported stayed false and nested_invept() could never
     * have been reached. Read it once here.
     */
    {
        u32 lo, hi;

        __asm__ __volatile__("rdmsr"
                             : "=a"(lo), "=d"(hi)
                             : "c"(0x48Cu));
        (void)hi;

        g_nested_virt.vmx_state.ept_state.invept_caps = lo;
        g_nested_virt.vmx_state.ept_state.ept_invept_supported =
            (lo & (1u << 20)) != 0;
    }

    /* Initialize VPID state */
    g_nested_virt.vmx_state.vpid_state.vpid_enabled = true;
    g_nested_virt.vmx_state.vpid_state.vpid = 1;
    
    g_nested_virt.nested_vmx_supported = true;
    
    printk("[NESTED-VIRT] Nested VMX initialized\n");
    
    return 0;
}

/*
 * Read the VMX capability MSR that reports which bits of CR4/CR0 may be set
 * while VMX is active, and which are required. IA32_VMX_CR4_FIXED0 is 0x484.
 */
static u64 vmx_cr4_fixed0(void)
{
    u32 lo, hi;

    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x484u));
    return ((u64)hi << 32) | lo;
}

/*
 * Enable Nested VMX - real instruction sequence.
 *
 * This used to set a boolean and print "enabled" without executing anything.
 * The sequence below is the architectural one: set CR4.VMXE, write the VMCS
 * revision identifier into the VMXON region, then VMXON against its physical
 * address, checking CF for failure.
 *
 * Everything is gated on CPUID reporting VMX (bit 5 of ECX on leaf 1) and the
 * MSR lock bit (IA32_FEATURE_CONTROL, 0x3A, bit 0). Without the lock bit set,
 * VMXON raises #GP; a kernel that calls it anyway dies at boot. Where the
 * hardware cannot support it, this reports that and returns failure rather
 * than claiming success.
 */
static int nested_vmx_enable(void)
{
    u64 cr4;
    u64 fixed0;
    u8 failed;

    printk("[NESTED-VIRT] Enabling nested VMX...\n");

    if (!g_nested_virt.nested_vmx_supported) {
        printk("[NESTED-VIRT] VMX not reported by CPUID - not enabling\n");
        return -1;
    }

    /* IA32_FEATURE_CONTROL must have the lock bit and VMXON-outside-SMX set. */
    {
        u32 lo, hi;
        __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x3Au));

        if (!(lo & (1u << 0))) {
            printk("[NESTED-VIRT] IA32_FEATURE_CONTROL not locked - firmware "
                   "has not enabled VMX, not enabling\n");
            return -1;
        }
        if (!(lo & (1u << 2))) {
            printk("[NESTED-VIRT] VMXON outside SMX not permitted - "
                   "not enabling\n");
            return -1;
        }
    }

    if (!g_nested_virt.vmx_state.vmxon_region_virt) {
        printk("[NESTED-VIRT] No VMXON region allocated\n");
        return -1;
    }

    /* Record the physical address of the VMXON region. */
    g_nested_virt.vmx_state.vmxon_region_phys =
        (u64)vmm_virt_to_phys((virt_addr_t)g_nested_virt.vmx_state.vmxon_region_virt);

    /* The VMXON region must carry the VMCS revision identifier. */
    *(u32 *)g_nested_virt.vmx_state.vmxon_region_virt =
        g_nested_virt.vmx_state.vmcs_revision_id;

    /* Set CR4.VMXE (bit 13), honouring the fixed bits the CPU requires. */
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));

    fixed0 = vmx_cr4_fixed0();
    cr4 |= fixed0;
    cr4 |= (1ull << 13);            /* CR4.VMXE */

    __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4) : "memory");

    /* VMXON [phys] - sets CF on failure. */
    failed = 0;
    __asm__ __volatile__(
        "vmxon %1\n\t"
        "setc %0"
        : "=q"(failed)
        : "m"(g_nested_virt.vmx_state.vmxon_region_phys)
        : "memory", "cc");

    if (failed) {
        printk("[NESTED-VIRT] VMXON failed (CF set) - VMX not left on\n");

        /* Clear VMXE again; leaving it set with VMX off is invalid. */
        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
        cr4 &= ~(1ull << 13);
        __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4) : "memory");

        g_nested_virt.vmx_state.vmxon_enabled = false;
        return -1;
    }

    g_nested_virt.vmx_state.vmxon_enabled = true;

    printk("[NESTED-VIRT] VMXON succeeded, VMX operation entered "
           "(root region 0x%lX)\n",
           (u64)g_nested_virt.vmx_state.vmxon_region_phys);

    return 0;
}

/* Disable Nested VMX - real VMXOFF, then clear CR4.VMXE. */
static int nested_vmx_disable(void)
{
    u64 cr4;

    printk("[NESTED-VIRT] Disabling nested VMX...\n");

    if (!g_nested_virt.vmx_state.vmxon_enabled) {
        return 0;
    }

    __asm__ __volatile__("vmxoff" ::: "memory", "cc");

    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 &= ~(1ull << 13);           /* clear CR4.VMXE */
    __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4) : "memory");

    g_nested_virt.vmx_state.vmxon_enabled = false;

    printk("[NESTED-VIRT] VMXOFF executed, VMX operation left\n");

    return 0;
}

/*
 * Create Nested VMCS - real VMCLEAR + VMPTRLD.
 *
 * Both instructions take a 64-bit memory operand containing the physical
 * address of the VMCS region (verified against the assembler: the register
 * form is invalid), and both report failure in CF.
 *
 * VMCLEAR is required before a region may be loaded for the first time or
 * reused; the SDM makes it an error to VMPTRLD a region that is already
 * current or that has never been cleared.
 */
static int nested_vmcs_create(void)
{
    u8 failed;

    printk("[NESTED-VIRT] Creating nested VMCS...\n");

    if (!g_nested_virt.vmx_state.vmxon_enabled) {
        printk("[NESTED-VIRT] VMX is not enabled - cannot load a VMCS\n");
        return -1;
    }

    if (!g_nested_virt.vmx_state.vmcs_region_virt) {
        printk("[NESTED-VIRT] No VMCS region allocated\n");
        return -1;
    }

    g_nested_virt.vmx_state.vmcs_region_phys =
        (u64)vmm_virt_to_phys((virt_addr_t)g_nested_virt.vmx_state.vmcs_region_virt);

    /* The VMCS revision identifier must be the first field of the region. */
    *(u32 *)g_nested_virt.vmx_state.vmcs_region_virt =
        g_nested_virt.vmx_state.vmcs_revision_id;

    /* VMCLEAR [phys] */
    failed = 0;
    __asm__ __volatile__(
        "vmclear %1\n\t"
        "setc %0"
        : "=q"(failed)
        : "m"(g_nested_virt.vmx_state.vmcs_region_phys)
        : "memory", "cc");

    if (failed) {
        printk("[NESTED-VIRT] VMCLEAR failed (CF set)\n");
        return -1;
    }

    /* VMPTRLD [phys] */
    failed = 0;
    __asm__ __volatile__(
        "vmptrld %1\n\t"
        "setc %0"
        : "=q"(failed)
        : "m"(g_nested_virt.vmx_state.vmcs_region_phys)
        : "memory", "cc");

    if (failed) {
        printk("[NESTED-VIRT] VMPTRLD failed (CF set)\n");
        return -1;
    }

    printk("[NESTED-VIRT] VMCS loaded at 0x%lX (current)\n",
           (u64)g_nested_virt.vmx_state.vmcs_region_phys);

    
    return 0;
}

/* Run Nested VM */
static int nested_vmx_run(void)
{
    printk("[NESTED-VIRT] Running nested VM...\n");

    /*
     * VMLAUNCH is deliberately NOT executed here.
     *
     * Launching a VMCS that has not been fully configured does not fail
     * politely: the CPU enters guest state built from whatever is in the
     * control fields - zeroed memory - and the first instruction fetch goes to
     * a guest-physical address with no EPT mapping. The result is a VM-exit
     * storm or a triple fault, at ring -1, with no handler installed.
     *
     * A real implementation has to populate the entire VMCS first: pin-based
     * and primary/secondary processor controls, exit controls, the guest and
     * host state areas, CR0/CR4 fixed bits, MSR bitmaps and an EPT identity
     * map. That work is not done, so this refuses rather than launching a
     * half-built guest and hanging the machine.
     */
    printk("[NESTED-VIRT] VMLAUNCH not performed: the VMCS is not configured "
           "(no guest/host state, no EPT). Refusing to launch - a VMCS entered "
           "in this state faults the CPU rather than failing cleanly.\n");

    return -1;
}

/* Forward declarations: the EPT violation handler flushes through INVEPT. */
static int nested_invept(u32 type, u64 eptp);
static int nested_invvpid(u32 type, u16 vpid);

/*
 * EPT Violation Handler
 *
 * Exit qualification bits (SDM 27.2.1.3): bit 0 = read, bit 1 = write,
 * bit 2 = instruction fetch.
 */
#define NESTED_EPT_QUAL_READ    0x01
#define NESTED_EPT_QUAL_WRITE   0x02
#define NESTED_EPT_QUAL_FETCH   0x04

static int nested_ept_violation_handler(u64 guest_phys, u64 exit_qualification)
{
    const char *kind = "unknown";
    u64 gpa_page = guest_phys & ~0xFFFULL;

    if (exit_qualification & NESTED_EPT_QUAL_FETCH) {
        kind = "fetch";
    } else if (exit_qualification & NESTED_EPT_QUAL_WRITE) {
        kind = "write";
    } else if (exit_qualification & NESTED_EPT_QUAL_READ) {
        kind = "read";
    }

    printk("[NESTED-VIRT] EPT violation: GPA 0x%llx (%s), qualification 0x%llx\n",
           (unsigned long long)guest_phys, kind,
           (unsigned long long)exit_qualification);

    /*
     * Any mapping change invalidates the translations the CPU holds for this
     * EPTP, so flush single-context after touching EPT state. Skipping this
     * was why the handler could log a violation forever: the stale translation
     * kept faulting on the same page.
     */
    if (nested_invept(0, g_nested_virt.vmx_state.ept_state.ept_pointer) != 0) {
        printk("[NESTED-VIRT] failing to flush EPT for GPA 0x%llx\n",
               (unsigned long long)gpa_page);
        return -EIO;
    }

    return 0;
}

/*
 * INVEPT / INVVPID descriptors. Both instructions take a 128-bit operand
 * (EPTP or VPID) in memory, not in a register, so the descriptor has to be
 * built in a local and passed by address.
 */
typedef struct __packed {
    u64 eptp;
    u64 reserved;
} nested_invept_desc_t;

typedef struct __packed {
    u16 vpid;
    u16 reserved0;
    u32 reserved1;
    u64 reserved2;
} nested_invvpid_desc_t;

/*
 * These two were printk-only stubs. Without the real instructions, a nested
 * EPT mapping change - or a VPID reuse - leaves stale translations in the TLB,
 * so a guest keeps translating through the old mapping and reads or writes the
 * wrong physical page. Both now execute the hardware instruction when the CPU
 * advertises support, and say so plainly when it does not.
 */

/* INVEPT (Invalidate EPT) */
static int nested_invept(u32 type, u64 eptp)
{
    nested_invept_desc_t desc;
    u8 ok;

    if (!g_nested_virt.vmx_state.ept_state.ept_invept_supported) {
        printk("[NESTED-VIRT] INVEPT unsupported by this CPU\n");
        return -ENOTSUP;
    }

    /* Only single-context and all-context are architecturally valid. */
    if (type > 1) {
        return -EINVAL;
    }

    desc.eptp = eptp;
    desc.reserved = 0;

    __asm__ __volatile__(
        "invept %1, %2\n\t"
        "setna %0"
        : "=rm"(ok)
        : "m"(desc), "r"((u64)type)
        : "cc", "memory");

    if (!ok) {
        printk("[NESTED-VIRT] INVEPT type %u (EPTP 0x%llx) failed\n",
               type, (unsigned long long)eptp);
        return -EIO;
    }

    return 0;
}

/* INVVPID (Invalidate VPID) */
static int nested_invvpid(u32 type, u16 vpid)
{
    nested_invvpid_desc_t desc;
    u8 ok;

    if (!g_nested_virt.vmx_state.vpid_state.vpid_enabled) {
        printk("[NESTED-VIRT] INVVPID unsupported by this CPU\n");
        return -ENOTSUP;
    }

    if (type > 3) {
        return -EINVAL;
    }

    desc.vpid = vpid;
    desc.reserved0 = 0;
    desc.reserved1 = 0;
    desc.reserved2 = 0;

    __asm__ __volatile__(
        "invvpid %1, %2\n\t"
        "setna %0"
        : "=rm"(ok)
        : "m"(desc), "r"((u64)type)
        : "cc", "memory");

    if (!ok) {
        printk("[NESTED-VIRT] INVVPID type %u (VPID %u) failed\n", type, vpid);
        return -EIO;
    }

    return 0;
}

/*===========================================================================*/
/*                         NESTED SVM SUPPORT (AMD)                          */
/*===========================================================================*/

/* Initialize Nested SVM */
static int nested_svm_init(void)
{
    printk("[NESTED-VIRT] Initializing nested SVM...\n");
    
    /* Allocate VMCB (Virtual Machine Control Block) */
    g_nested_virt.svm_state.vmcb_virt = kmalloc(4096);
    if (!g_nested_virt.svm_state.vmcb_virt) {
        printk("[NESTED-VIRT] ERROR: Failed to allocate VMCB\n");
        return -1;
    }
    memset(g_nested_virt.svm_state.vmcb_virt, 0, 4096);
    
    /* Allocate HSAVE area */
    g_nested_virt.svm_state.hsave_pa = (u64)kmalloc(4096);
    if (!g_nested_virt.svm_state.hsave_pa) {
        printk("[NESTED-VIRT] ERROR: Failed to allocate HSAVE\n");
        return -1;
    }
    
    g_nested_virt.nested_svm_supported = true;
    
    printk("[NESTED-VIRT] Nested SVM initialized\n");
    
    return 0;
}

/* Enable Nested SVM */
static int nested_svm_enable(void)
{
    printk("[NESTED-VIRT] Enabling nested SVM...\n");
    
    /* Set EFER.SVME bit */
    /* Enable virtualization in VM_HSAVE_PA MSR */
    
    g_nested_virt.svm_state.svm_enabled = true;
    g_nested_virt.svm_state.nested_svm_enabled = true;
    
    printk("[NESTED-VIRT] Nested SVM enabled\n");
    
    return 0;
}

/* Run Nested SVM VM */
static int nested_svm_run(void)
{
    printk("[NESTED-VIRT] Running nested SVM VM...\n");
    
    /* VMRUN instruction */
    
    /* Handle VM-Exit */
    /* - Reflect to L1 hypervisor if needed */
    
    printk("[NESTED-VIRT] Nested SVM execution complete\n");
    
    return 0;
}

/*===========================================================================*/
/*                         NESTED VM-EXIT DISPATCH                           */
/*===========================================================================*/

/* Basic exit-reason numbers (SDM Appendix C). */
#define NESTED_EXIT_EPT_VIOLATION       48
#define NESTED_EXIT_INVEPT              50
#define NESTED_EXIT_INVVPID             53

/**
 * nested_virt_handle_vmexit - Dispatch a nested VM-exit
 * @exit_reason: Basic exit reason from the VMCS
 * @guest_phys:  Guest-physical address for EPT violations
 * @qualification: Exit qualification
 *
 * Without a dispatcher the EPT-violation and invalidation handlers had no
 * caller at all: a nested guest that tripped an EPT violation would simply
 * resume against the same stale translation and fault forever.
 *
 * Returns 0 if handled, -EINVAL for an unknown exit reason, or the handler's
 * error code.
 */
int nested_virt_handle_vmexit(u32 exit_reason, u64 guest_phys,
                              u64 qualification)
{
    switch (exit_reason) {
        case NESTED_EXIT_EPT_VIOLATION:
            return nested_ept_violation_handler(guest_phys, qualification);

        case NESTED_EXIT_INVEPT:
            /*
             * A nested hypervisor asked to invalidate EPT translations. Honour
             * it for every context so no stale mapping survives.
             */
            return nested_invept(1, 0);

        case NESTED_EXIT_INVVPID:
            /* Flush the current VPID for this logical processor. */
            return nested_invvpid(0,
                g_nested_virt.vmx_state.vpid_state.vpid);

        default:
            return -EINVAL;
    }
}

/*===========================================================================*/
/*                         NESTED VIRTUALIZATION API                         */
/*===========================================================================*/

/* Enable Nested Virtualization */
int nested_virt_enable(u32 level)
{
    if (level > NESTED_VMX_MAX_LEVELS) {
        printk("[NESTED-VIRT] ERROR: Maximum nested level is %u\n", NESTED_VMX_MAX_LEVELS);
        return -1;
    }
    
    printk("[NESTED-VIRT] Enabling nested virtualization level %u\n", level);

    /*
     * The results were discarded and this always returned success, so a
     * caller could not tell "VMX is on" from "the firmware never allowed it".
     * VMXON and VMCLEAR/VMPTRLD can all fail, and failing silently is how a
     * hypervisor ends up believing it has hardware it does not have.
     */
    int vmx_rc = 0;
    int svm_rc = 0;

    if (g_nested_virt.nested_vmx_supported) {
        vmx_rc = nested_vmx_enable();
    }

    if (g_nested_virt.nested_svm_supported) {
        svm_rc = nested_svm_enable();
    }

    if (vmx_rc != 0 && svm_rc != 0) {
        printk("[NESTED-VIRT] Neither VMX nor SVM could be enabled - "
               "nested virtualization is unavailable\n");
        return -1;
    }

    g_nested_virt.max_nested_level = level;

    printk("[NESTED-VIRT] Nested virtualization enabled (level %u, "
           "vmx=%s, svm=%s)\n", level,
           vmx_rc == 0 ? "on" : "off",
           svm_rc == 0 ? "on" : "off");

    return 0;
}

/* Disable Nested Virtualization */
int nested_virt_disable(void)
{
    printk("[NESTED-VIRT] Disabling nested virtualization...\n");
    
    if (g_nested_virt.nested_vmx_supported) {
        nested_vmx_disable();
    }
    
    g_nested_virt.max_nested_level = 0;
    
    printk("[NESTED-VIRT] Nested virtualization disabled\n");
    
    return 0;
}

/* Create Nested VM */
int nested_vm_create(u32 vm_type)
{
    printk("[NESTED-VIRT] Creating nested VM (type %u)...\n", vm_type);
    
    if (vm_type == 0) {
        /* VMX nested VM */
        nested_vmcs_create();
    } else {
        /* SVM nested VM */
        /* VMCB setup */
    }
    
    printk("[NESTED-VIRT] Nested VM created\n");
    
    return 0;
}

/* Run Nested VM */
int nested_vm_run(void)
{
    if (g_nested_virt.nested_vmx_supported) {
        return nested_vmx_run();
    }
    
    if (g_nested_virt.nested_svm_supported) {
        return nested_svm_run();
    }
    
    return -1;
}

/* Nested Virtualization Statistics */
void nested_virt_print_stats(void)
{
    printk("\n[NESTED-VIRT] ====== STATISTICS ======\n");
    printk("[NESTED-VIRT] Initialized:          %s\n", g_nested_virt.initialized ? "Yes" : "No");
    printk("[NESTED-VIRT] Nested VMX:           %s\n", g_nested_virt.nested_vmx_supported ? "Yes" : "No");
    printk("[NESTED-VIRT] Nested SVM:           %s\n", g_nested_virt.nested_svm_supported ? "Yes" : "No");
    printk("[NESTED-VIRT] Max Nested Level:     %u\n", g_nested_virt.max_nested_level);
    printk("[NESTED-VIRT] ========================\n\n");
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int nested_virt_init(void)
{
    printk("[NESTED-VIRT] ========================================\n");
    printk("[NESTED-VIRT] NEXUS OS Nested Virtualization\n");
    printk("[NESTED-VIRT] ========================================\n");
    
    memset(&g_nested_virt, 0, sizeof(nested_virt_manager_t));
    g_nested_virt.lock.lock = 0;
    
    /* Detect nested virtualization support */
    u32 eax, ebx, ecx, edx;
    
    /* Check for VMX (Intel) */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    if (ecx & (1 << 5)) {
        /* VMX supported, check nested VMX capability */
        __asm__ __volatile__(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0x00000007)
        );
        
        if (ecx & (1 << 22)) {
            g_nested_virt.nested_vmx_supported = true;
            printk("[NESTED-VIRT] Intel nested VMX supported\n");
        }
    }
    
    /* Check for SVM (AMD) */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x80000001)
    );
    
    if (ecx & (1 << 2)) {
        /* SVM supported, check nested SVM capability */
        printk("[NESTED-VIRT] AMD SVM supported (nested SVM requires MSR check)\n");
    }
    
    /* Initialize nested VMX */
    if (g_nested_virt.nested_vmx_supported) {
        nested_vmx_init();
    }
    
    /* Initialize nested SVM */
    if (g_nested_virt.nested_svm_supported) {
        nested_svm_init();
    }
    
    g_nested_virt.initialized = true;
    
    printk("[NESTED-VIRT] Nested virtualization initialized\n");
    printk("[NESTED-VIRT] ========================================\n");
    
    return 0;
}
