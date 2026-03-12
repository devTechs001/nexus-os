/*
 * NEXUS OS - Security Framework
 * security/security_manager.c
 *
 * Security Manager Core Implementation
 * Central security management and coordination
 */

#include "security.h"

/*===========================================================================*/
/*                         SECURITY MANAGER CONFIGURATION                    */
/*===========================================================================*/

#define SECURITY_DEBUG              1
#define SECURITY_AUDIT_ENABLED      1
#define SECURITY_MAX_CONTEXTS       65536
#define SECURITY_MAX_POLICIES       1024
#define SECURITY_AUDIT_BUFFER_SIZE  4096

/* Security Manager States */
#define SECURITY_STATE_UNINITIALIZED    0
#define SECURITY_STATE_INITIALIZING     1
#define SECURITY_STATE_RUNNING          2
#define SECURITY_STATE_SHUTTING_DOWN    3
#define SECURITY_STATE_FAILED           4

/*===========================================================================*/
/*                         SECURITY MANAGER GLOBAL DATA                      */
/*===========================================================================*/

/**
 * g_security_manager - Global security manager instance
 *
 * Central structure managing all security subsystems and providing
 * the unified security API for the operating system.
 */
security_manager_t g_security_manager = {
    .lock = {
        .lock = SPINLOCK_UNLOCKED,
        .magic = SPINLOCK_MAGIC,
        .name = "security_manager",
        .owner_cpu = 0xFFFFFFFF,
    },
    .state = SECURITY_STATE_UNINITIALIZED,
    .security_level = SECURITY_LEVEL_BASIC,
    .contexts = NULL,
    .context_count = 0,
    .policies = NULL,
    .policy_count = 0,
    .total_events = 0,
    .violations = 0,
    .crypto_state = NULL,
    .tpm_state = NULL,
    .sandbox_state = NULL,
    .auth_state = NULL,
};

/* Context table */
static security_context_t *security_context_table[SECURITY_MAX_CONTEXTS];
static spinlock_t context_table_lock = {
    .lock = SPINLOCK_UNLOCKED,
    .magic = SPINLOCK_MAGIC,
    .name = "context_table",
};

/* Policy table */
static security_policy_t *security_policy_table[SECURITY_MAX_POLICIES];
static spinlock_t policy_table_lock = {
    .lock = SPINLOCK_UNLOCKED,
    .magic = SPINLOCK_MAGIC,
    .name = "policy_table",
};

/* Default security label for processes */
static const char *default_security_label = "system_u:system_r:default_t:s0";

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static s32 security_init_contexts(void);
static s32 security_init_policies(void);
static s32 security_init_audit(void);
static void security_cleanup_contexts(void);
static void security_cleanup_policies(void);

/*===========================================================================*/
/*                         SECURITY MANAGER INITIALIZATION                   */
/*===========================================================================*/

/**
 * security_init_contexts - Initialize security context table
 *
 * Allocates and initializes the security context table used to
 * track security contexts for all processes.
 *
 * Returns: OK on success, error code on failure
 */
static s32 security_init_contexts(void)
{
    u32 i;

    pr_info("Security: Initializing context table...\n");

    /* Initialize context table */
    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        security_context_table[i] = NULL;
    }

    g_security_manager.context_count = 0;

    pr_info("Security: Context table initialized (%d entries)\n", SECURITY_MAX_CONTEXTS);

    return OK;
}

/**
 * security_init_policies - Initialize security policy table
 *
 * Allocates and initializes the security policy table used to
 * store and manage security policies.
 *
 * Returns: OK on success, error code on failure
 */
static s32 security_init_policies(void)
{
    u32 i;

    pr_info("Security: Initializing policy table...\n");

    /* Initialize policy table */
    for (i = 0; i < SECURITY_MAX_POLICIES; i++) {
        security_policy_table[i] = NULL;
    }

    g_security_manager.policy_count = 0;

    pr_info("Security: Policy table initialized (%d entries)\n", SECURITY_MAX_POLICIES);

    return OK;
}

/**
 * security_init_audit - Initialize security audit subsystem
 *
 * Initializes the audit log buffer and configures audit settings.
 *
 * Returns: OK on success, error code on failure
 */
static s32 security_init_audit(void)
{
    pr_info("Security: Initializing audit subsystem...\n");

    return security_audit_init(&g_security_manager.audit_log, SECURITY_AUDIT_BUFFER_SIZE);
}

/**
 * security_init - Initialize the security framework
 *
 * Main initialization function for the security framework.
 * Initializes all security subsystems and prepares the
 * security manager for operation.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_init(void)
{
    s32 ret;

    pr_info("===============================================\n");
    pr_info("  NEXUS OS Security Framework v%s\n", SECURITY_VERSION_STRING);
    pr_info("===============================================\n");

    /* Check if already initialized */
    if (g_security_manager.state != SECURITY_STATE_UNINITIALIZED) {
        pr_err("Security: Already initialized (state=%u)\n", g_security_manager.state);
        return EBUSY;
    }

    g_security_manager.state = SECURITY_STATE_INITIALIZING;

    /* Initialize manager lock */
    spin_lock_init_named(&g_security_manager.lock, "security_manager");

    /* Initialize context table */
    ret = security_init_contexts();
    if (ret != OK) {
        pr_err("Security: Failed to initialize contexts (ret=%d)\n", ret);
        goto err_contexts;
    }

    /* Initialize policy table */
    ret = security_init_policies();
    if (ret != OK) {
        pr_err("Security: Failed to initialize policies (ret=%d)\n", ret);
        goto err_policies;
    }

    /* Initialize audit subsystem */
    ret = security_init_audit();
    if (ret != OK) {
        pr_err("Security: Failed to initialize audit (ret=%d)\n", ret);
        goto err_audit;
    }

    /* Initialize crypto subsystem */
    pr_info("Security: Initializing crypto subsystem...\n");
    ret = security_crypto_init();
    if (ret != OK) {
        pr_err("Security: Failed to initialize crypto (ret=%d)\n", ret);
        /* Continue without crypto - non-fatal */
    }

    /* Initialize authentication subsystem */
    pr_info("Security: Initializing auth subsystem...\n");
    ret = security_auth_init();
    if (ret != OK) {
        pr_err("Security: Failed to initialize auth (ret=%d)\n", ret);
        /* Continue without auth - non-fatal */
    }

    /* Initialize sandbox subsystem */
    pr_info("Security: Initializing sandbox subsystem...\n");
    ret = security_sandbox_init();
    if (ret != OK) {
        pr_err("Security: Failed to initialize sandbox (ret=%d)\n", ret);
        /* Continue without sandbox - non-fatal */
    }

    /* Initialize TPM subsystem */
    pr_info("Security: Initializing TPM subsystem...\n");
    ret = security_tpm_init();
    if (ret != OK) {
        pr_err("Security: Failed to initialize TPM (ret=%d)\n", ret);
        /* Continue without TPM - non-fatal */
    }

    /* Set default security level */
    g_security_manager.security_level = SECURITY_LEVEL_BASIC;

    /* Mark as running */
    g_security_manager.state = SECURITY_STATE_RUNNING;

    pr_info("Security: Framework initialized successfully\n");
    pr_info("  Security Level: %u\n", g_security_manager.security_level);
    pr_info("  Audit: %s\n", SECURITY_AUDIT_ENABLED ? "enabled" : "disabled");
    pr_info("  Debug: %s\n", SECURITY_DEBUG ? "enabled" : "disabled");

    return OK;

err_audit:
    security_cleanup_policies();
err_policies:
    security_cleanup_contexts();
err_contexts:
    g_security_manager.state = SECURITY_STATE_FAILED;
    return ret;
}

/**
 * security_subsystem_init - Initialize security as a subsystem
 *
 * Wrapper for security_init() called during kernel initialization.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_subsystem_init(void)
{
    return security_init();
}

/**
 * security_cleanup_contexts - Clean up security contexts
 *
 * Releases all resources associated with security contexts.
 */
static void security_cleanup_contexts(void)
{
    u32 i;

    spin_lock(&context_table_lock);

    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        if (security_context_table[i] != NULL) {
            security_destroy_context(security_context_table[i]);
            security_context_table[i] = NULL;
        }
    }

    spin_unlock(&context_table_lock);
}

/**
 * security_cleanup_policies - Clean up security policies
 *
 * Releases all resources associated with security policies.
 */
static void security_cleanup_policies(void)
{
    u32 i;

    spin_lock(&policy_table_lock);

    for (i = 0; i < SECURITY_MAX_POLICIES; i++) {
        if (security_policy_table[i] != NULL) {
            security_policy_unregister(i);
        }
    }

    spin_unlock(&policy_table_lock);
}

/**
 * security_shutdown - Shutdown the security framework
 *
 * Gracefully shuts down all security subsystems and releases
 * all resources.
 */
void security_shutdown(void)
{
    pr_info("Security: Shutting down...\n");

    g_security_manager.state = SECURITY_STATE_SHUTTING_DOWN;

    /* Shutdown subsystems */
    security_audit_shutdown(&g_security_manager.audit_log);
    security_cleanup_contexts();
    security_cleanup_policies();

    g_security_manager.state = SECURITY_STATE_UNINITIALIZED;

    pr_info("Security: Shutdown complete\n");
}

/*===========================================================================*/
/*                         MANAGER STATE OPERATIONS                          */
/*===========================================================================*/

/**
 * security_get_level - Get current security level
 *
 * Returns the current security level of the system.
 *
 * Returns: Current security level
 */
u32 security_get_level(void)
{
    return g_security_manager.security_level;
}

/**
 * security_set_level - Set security level
 * @level: New security level
 *
 * Sets the system security level. Higher levels enforce
 * stricter security policies.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_set_level(u32 level)
{
    if (level > SECURITY_LEVEL_MAX) {
        return EINVAL;
    }

    spin_lock(&g_security_manager.lock);
    g_security_manager.security_level = level;
    spin_unlock(&g_security_manager.lock);

    pr_info("Security: Level changed to %u\n", level);

    return OK;
}

/**
 * security_is_enabled - Check if security is enabled
 *
 * Returns: true if security framework is active, false otherwise
 */
bool security_is_enabled(void)
{
    return g_security_manager.state == SECURITY_STATE_RUNNING;
}

/*===========================================================================*/
/*                         CONTEXT MANAGEMENT                                */
/*===========================================================================*/

/**
 * security_create_context - Create a new security context
 * @pid: Process ID
 * @uid: User ID
 * @gid: Group ID
 *
 * Creates a new security context for a process with the
 * specified credentials.
 *
 * Returns: Pointer to new context, or NULL on failure
 */
security_context_t *security_create_context(u32 pid, u32 uid, u32 gid)
{
    security_context_t *ctx;
    u32 i;

    if (!security_uid_valid(uid) || !security_gid_valid(gid)) {
        return NULL;
    }

    ctx = kzalloc(sizeof(security_context_t));
    if (!ctx) {
        return NULL;
    }

    /* Initialize context */
    ctx->magic = SECURITY_CTX_MAGIC;
    spin_lock_init_named(&ctx->lock, "security_context");
    ctx->pid = pid;
    ctx->uid = uid;
    ctx->gid = gid;
    ctx->flags = SECURITY_CTX_VALID;
    ctx->creation_time = get_time_ms();
    ctx->last_modified = get_time_ms();
    ctx->reference_count = 1;

    /* Initialize capabilities */
    security_cap_zero(&ctx->caps);

    /* Set default label */
    strncpy(ctx->label, default_security_label, SECURITY_MAX_LABEL_SIZE - 1);
    ctx->label[SECURITY_MAX_LABEL_SIZE - 1] = '\0';

    /* Find empty slot in context table */
    spin_lock(&context_table_lock);

    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        if (security_context_table[i] == NULL) {
            security_context_table[i] = ctx;
            g_security_manager.context_count++;
            spin_unlock(&context_table_lock);

            pr_debug("Security: Created context for PID %u (UID %u)\n", pid, uid);

            return ctx;
        }
    }

    spin_unlock(&context_table_lock);

    /* No space available */
    kfree(ctx);
    return NULL;
}

/**
 * security_destroy_context - Destroy a security context
 * @ctx: Context to destroy
 *
 * Destroys a security context and releases all associated resources.
 */
void security_destroy_context(security_context_t *ctx)
{
    u32 i;

    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return;
    }

    spin_lock(&ctx->lock);

    /* Check reference count */
    if (ctx->reference_count > 1) {
        ctx->reference_count--;
        spin_unlock(&ctx->lock);
        return;
    }

    /* Remove from context table */
    spin_lock(&context_table_lock);

    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        if (security_context_table[i] == ctx) {
            security_context_table[i] = NULL;
            g_security_manager.context_count--;
            break;
        }
    }

    spin_unlock(&context_table_lock);

    /* Free ACL if present */
    if (ctx->acl) {
        security_acl_destroy(ctx->acl);
        ctx->acl = NULL;
    }

    /* Clear magic to invalidate */
    ctx->magic = 0;

    spin_unlock(&ctx->lock);

    kfree(ctx);

    pr_debug("Security: Destroyed context for PID %u\n", ctx->pid);
}

/**
 * security_get_context - Get security context for a process
 * @pid: Process ID
 *
 * Retrieves the security context associated with a process.
 *
 * Returns: Pointer to context, or NULL if not found
 */
security_context_t *security_get_context(u32 pid)
{
    u32 i;
    security_context_t *ctx = NULL;

    spin_lock(&context_table_lock);

    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        if (security_context_table[i] != NULL &&
            security_context_table[i]->pid == pid) {
            ctx = security_context_table[i];
            ctx->reference_count++;
            break;
        }
    }

    spin_unlock(&context_table_lock);

    return ctx;
}

/**
 * security_set_context - Set the current security context
 * @ctx: Context to set
 *
 * Sets the security context for the current execution context.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_set_context(security_context_t *ctx)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return EINVAL;
    }

    /* In real implementation, this would set per-CPU current context */
    /* For now, just validate the context */

    return OK;
}

/**
 * security_put_context - Release a context reference
 * @ctx: Context to release
 *
 * Decrements the reference count on a context.
 */
void security_put_context(security_context_t *ctx)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return;
    }

    spin_lock(&ctx->lock);
    if (ctx->reference_count > 0) {
        ctx->reference_count--;
    }
    spin_unlock(&ctx->lock);
}

/*===========================================================================*/
/*                         CONTEXT OPERATIONS                                */
/*===========================================================================*/

/**
 * security_context_set_label - Set security label for a context
 * @ctx: Security context
 * @label: Label string
 *
 * Sets the security label (e.g., SELinux-style label) for a context.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_context_set_label(security_context_t *ctx, const char *label)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return EINVAL;
    }

    if (!label) {
        return EINVAL;
    }

    spin_lock(&ctx->lock);

    strncpy(ctx->label, label, SECURITY_MAX_LABEL_SIZE - 1);
    ctx->label[SECURITY_MAX_LABEL_SIZE - 1] = '\0';
    ctx->last_modified = get_time_ms();

    spin_unlock(&ctx->lock);

    return OK;
}

/**
 * security_context_get_label - Get security label for a context
 * @ctx: Security context
 *
 * Returns: Pointer to label string, or NULL on failure
 */
const char *security_context_get_label(security_context_t *ctx)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return NULL;
    }

    return ctx->label;
}

/**
 * security_context_copy - Copy security context
 * @dest: Destination context
 * @src: Source context
 *
 * Copies security attributes from source to destination context.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_context_copy(security_context_t *dest, security_context_t *src)
{
    if (!dest || !src) {
        return EINVAL;
    }

    if (src->magic != SECURITY_CTX_MAGIC) {
        return EINVAL;
    }

    spin_lock(&src->lock);
    spin_lock(&dest->lock);

    /* Copy capabilities */
    dest->caps = src->caps;

    /* Copy label */
    strncpy(dest->label, src->label, SECURITY_MAX_LABEL_SIZE - 1);
    dest->label[SECURITY_MAX_LABEL_SIZE - 1] = '\0';

    /* Copy flags (except VALID) */
    dest->flags = (dest->flags & SECURITY_CTX_VALID) |
                  (src->flags & ~SECURITY_CTX_VALID);

    dest->last_modified = get_time_ms();

    spin_unlock(&dest->lock);
    spin_unlock(&src->lock);

    return OK;
}

/*===========================================================================*/
/*                         CAPABILITY OPERATIONS                             */
/*===========================================================================*/

/**
 * security_cap_set - Set a capability bit
 * @caps: Capability set
 * @cap: Capability to set
 *
 * Sets the specified capability in the capability set.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_cap_set(security_capabilities_t *caps, u64 cap)
{
    if (!caps) {
        return EINVAL;
    }

    caps->effective |= cap;
    caps->permitted |= cap;

    return OK;
}

/**
 * security_cap_clear - Clear a capability bit
 * @caps: Capability set
 * @cap: Capability to clear
 *
 * Clears the specified capability from the capability set.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_cap_clear(security_capabilities_t *caps, u64 cap)
{
    if (!caps) {
        return EINVAL;
    }

    caps->effective &= ~cap;
    caps->permitted &= ~cap;

    return OK;
}

/**
 * security_cap_test - Test if a capability is set
 * @caps: Capability set
 * @cap: Capability to test
 *
 * Tests if the specified capability is set in the effective set.
 *
 * Returns: true if capability is set, false otherwise
 */
bool security_cap_test(security_capabilities_t *caps, u64 cap)
{
    if (!caps) {
        return false;
    }

    return (caps->effective & cap) != 0;
}

/**
 * security_cap_zero - Zero all capabilities
 * @caps: Capability set
 *
 * Clears all capabilities in the set.
 */
void security_cap_zero(security_capabilities_t *caps)
{
    if (!caps) {
        return;
    }

    caps->effective = 0;
    caps->permitted = 0;
    caps->inheritable = 0;
    caps->bounding = CAP_ALL;
    caps->ambient = 0;
}

/**
 * security_cap_full - Set all capabilities
 * @caps: Capability set
 *
 * Sets all capabilities in the set (full privileges).
 */
void security_cap_full(security_capabilities_t *caps)
{
    if (!caps) {
        return;
    }

    caps->effective = CAP_ALL;
    caps->permitted = CAP_ALL;
    caps->inheritable = CAP_ALL;
    caps->bounding = CAP_ALL;
    caps->ambient = CAP_ALL;
}

/*===========================================================================*/
/*                         PROCESS CAPABILITIES                              */
/*===========================================================================*/

/**
 * security_set_caps - Set capabilities for a context
 * @ctx: Security context
 * @caps: New capability set
 *
 * Sets the capabilities for a security context.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_set_caps(security_context_t *ctx, security_capabilities_t *caps)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return EINVAL;
    }

    if (!caps) {
        return EINVAL;
    }

    spin_lock(&ctx->lock);
    ctx->caps = *caps;
    ctx->last_modified = get_time_ms();
    spin_unlock(&ctx->lock);

    return OK;
}

/**
 * security_get_caps - Get capabilities for a context
 * @ctx: Security context
 * @caps: Output capability set
 *
 * Retrieves the capabilities for a security context.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_get_caps(security_context_t *ctx, security_capabilities_t *caps)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return EINVAL;
    }

    if (!caps) {
        return EINVAL;
    }

    spin_lock(&ctx->lock);
    *caps = ctx->caps;
    spin_unlock(&ctx->lock);

    return OK;
}

/**
 * security_capable - Check if context has a capability
 * @ctx: Security context
 * @cap: Capability to check
 *
 * Checks if a security context has the specified capability.
 *
 * Returns: true if capability is held, false otherwise
 */
bool security_capable(security_context_t *ctx, u64 cap)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return false;
    }

    return security_cap_test(&ctx->caps, cap);
}

/**
 * security_capable_current - Check if current context has a capability
 * @cap: Capability to check
 *
 * Checks if the current execution context has the specified capability.
 *
 * Returns: true if capability is held, false otherwise
 */
bool security_capable_current(u64 cap)
{
    /* In real implementation, would check current thread's context */
    /* For now, assume root has all capabilities */
    return (cap & CAP_ALL) != 0;
}

/**
 * security_capable_admin - Check for admin capability
 *
 * Returns: true if current context has SYS_ADMIN capability
 */
bool security_capable_admin(void)
{
    return security_capable_current(CAP_SYS_ADMIN);
}

/**
 * security_capable_sys - Check for system capabilities
 *
 * Returns: true if current context has system-level capabilities
 */
bool security_capable_sys(void)
{
    return security_capable_current(CAP_SYS_ADMIN | CAP_SYS_BOOT | CAP_SYS_MODULE);
}

/**
 * security_capable_net - Check for network capabilities
 *
 * Returns: true if current context has network capabilities
 */
bool security_capable_net(void)
{
    return security_capable_current(CAP_NET_ADMIN | CAP_NET_RAW);
}

/*===========================================================================*/
/*                         ACCESS CONTROL LIST OPERATIONS                    */
/*===========================================================================*/

/**
 * security_acl_init - Initialize an ACL
 * @acl: ACL to initialize
 * @max_entries: Maximum number of entries
 *
 * Initializes an ACL structure with the specified capacity.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_acl_init(security_acl_t *acl, u32 max_entries)
{
    if (!acl) {
        return EINVAL;
    }

    acl->magic = SECURITY_ACL_MAGIC;
    spin_lock_init_named(&acl->lock, "security_acl");
    acl->entry_count = 0;
    acl->max_entries = max_entries;
    acl->owner_id = 0;
    acl->group_id = 0;
    acl->mode = 0;
    acl->flags = 0;

    /* Allocate entry array */
    if (max_entries > 0) {
        acl->entries = kzalloc(sizeof(security_acl_entry_t) * max_entries);
        if (!acl->entries) {
            return ENOMEM;
        }
    } else {
        acl->entries = NULL;
    }

    return OK;
}

/**
 * security_acl_destroy - Destroy an ACL
 * @acl: ACL to destroy
 *
 * Releases all resources associated with an ACL.
 */
void security_acl_destroy(security_acl_t *acl)
{
    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return;
    }

    spin_lock(&acl->lock);

    if (acl->entries) {
        kfree(acl->entries);
        acl->entries = NULL;
    }

    acl->magic = 0;
    acl->entry_count = 0;

    spin_unlock(&acl->lock);
}

/**
 * security_acl_add_entry - Add an entry to an ACL
 * @acl: ACL to modify
 * @type: Entry type (user/group/other)
 * @id: User or group ID
 * @perms: Permission bits
 *
 * Adds a new entry to the ACL.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_acl_add_entry(security_acl_t *acl, u32 type, u32 id, u8 perms)
{
    u32 i;

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return EINVAL;
    }

    if (!acl->entries || acl->entry_count >= acl->max_entries) {
        return ENOSPC;
    }

    spin_lock(&acl->lock);

    /* Check for duplicate */
    for (i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            spin_unlock(&acl->lock);
            return EEXIST;
        }
    }

    /* Add new entry */
    acl->entries[acl->entry_count].type = type;
    acl->entries[acl->entry_count].id = id;
    acl->entries[acl->entry_count].permissions = perms;
    acl->entries[acl->entry_count].flags = 0;
    acl->entry_count++;

    spin_unlock(&acl->lock);

    return OK;
}

/**
 * security_acl_remove_entry - Remove an entry from an ACL
 * @acl: ACL to modify
 * @type: Entry type
 * @id: User or group ID
 *
 * Removes an entry from the ACL.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_acl_remove_entry(security_acl_t *acl, u32 type, u32 id)
{
    u32 i, j;

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return EINVAL;
    }

    spin_lock(&acl->lock);

    /* Find entry */
    for (i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            /* Remove by shifting entries */
            for (j = i; j < acl->entry_count - 1; j++) {
                acl->entries[j] = acl->entries[j + 1];
            }
            acl->entry_count--;
            spin_unlock(&acl->lock);
            return OK;
        }
    }

    spin_unlock(&acl->lock);
    return ENOENT;
}

/**
 * security_acl_modify_entry - Modify an ACL entry
 * @acl: ACL to modify
 * @type: Entry type
 * @id: User or group ID
 * @perms: New permission bits
 *
 * Modifies the permissions for an existing ACL entry.
 *
 * Returns: OK on success, error code on failure
 */
s32 security_acl_modify_entry(security_acl_t *acl, u32 type, u32 id, u8 perms)
{
    u32 i;

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return EINVAL;
    }

    spin_lock(&acl->lock);

    /* Find entry */
    for (i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            acl->entries[i].permissions = perms;
            spin_unlock(&acl->lock);
            return OK;
        }
    }

    spin_unlock(&acl->lock);
    return ENOENT;
}

/**
 * security_acl_check - Check access against an ACL
 * @acl: ACL to check
 * @uid: User ID requesting access
 * @gid: Group ID requesting access
 * @perms: Permissions requested
 *
 * Checks if the specified user/group has the requested permissions.
 *
 * Returns: true if access granted, false otherwise
 */
bool security_acl_check(security_acl_t *acl, u32 uid, u32 gid, u8 perms)
{
    u32 i;
    u8 granted = 0;

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return false;
    }

    spin_lock(&acl->lock);

    /* Check owner */
    if (uid == acl->owner_id) {
        granted = acl->mode >> 6;  /* Owner bits */
        goto done;
    }

    /* Check group */
    if (gid == acl->group_id) {
        granted = (acl->mode >> 3) & 0x07;  /* Group bits */
        goto done;
    }

    /* Check ACL entries */
    for (i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == ACL_ENTRY_USER &&
            acl->entries[i].id == uid) {
            granted = acl->entries[i].permissions;
            goto done;
        }
        if (acl->entries[i].type == ACL_ENTRY_GROUP &&
            acl->entries[i].id == gid) {
            granted = acl->entries[i].permissions;
            goto done;
        }
    }

    /* Default to other permissions */
    granted = acl->mode & 0x07;

done:
    spin_unlock(&acl->lock);

    return (granted & perms) == perms;
}

/**
 * security_acl_get_perms - Get effective permissions for a user
 * @acl: ACL to check
 * @uid: User ID
 * @gid: Group ID
 *
 * Returns: Effective permission bits for the user
 */
u8 security_acl_get_perms(security_acl_t *acl, u32 uid, u32 gid)
{
    u32 i;

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return 0;
    }

    spin_lock(&acl->lock);

    /* Check owner */
    if (uid == acl->owner_id) {
        spin_unlock(&acl->lock);
        return acl->mode >> 6;
    }

    /* Check group */
    if (gid == acl->group_id) {
        spin_unlock(&acl->lock);
        return (acl->mode >> 3) & 0x07;
    }

    /* Check ACL entries */
    for (i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == ACL_ENTRY_USER &&
            acl->entries[i].id == uid) {
            spin_unlock(&acl->lock);
            return acl->entries[i].permissions;
        }
        if (acl->entries[i].type == ACL_ENTRY_GROUP &&
            acl->entries[i].id == gid) {
            spin_unlock(&acl->lock);
            return acl->entries[i].permissions;
        }
    }

    spin_unlock(&acl->lock);

    /* Default to other permissions */
    return acl->mode & 0x07;
}

/**
 * security_acl_set_owner - Set ACL owner
 * @acl: ACL to modify
 * @uid: New owner UID
 * @gid: New owner GID
 *
 * Returns: OK on success, error code on failure
 */
s32 security_acl_set_owner(security_acl_t *acl, u32 uid, u32 gid)
{
    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return EINVAL;
    }

    spin_lock(&acl->lock);
    acl->owner_id = uid;
    acl->group_id = gid;
    spin_unlock(&acl->lock);

    return OK;
}

/*===========================================================================*/
/*                         ACCESS CONTROL OPERATIONS                         */
/*===========================================================================*/

/**
 * security_check_access - Check access for a context against an ACL
 * @ctx: Security context
 * @acl: ACL to check
 * @perms: Permissions requested
 *
 * Returns: OK if access granted, error code otherwise
 */
s32 security_check_access(security_context_t *ctx, security_acl_t *acl, u8 perms)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return EPERM;
    }

    if (!acl || acl->magic != SECURITY_ACL_MAGIC) {
        return EPERM;
    }

    /* Check if user has capability to bypass ACL */
    if (security_capable(ctx, CAP_DAC_OVERRIDE)) {
        return OK;
    }

    /* Check ACL */
    if (security_acl_check(acl, ctx->uid, ctx->gid, perms)) {
        return OK;
    }

    return EACCES;
}

/**
 * security_validate_access - Validate access between users
 * @uid: Requesting user ID
 * @gid: Requesting group ID
 * @target_uid: Target user ID
 * @target_gid: Target group ID
 * @perms: Permissions requested
 *
 * Returns: OK if access valid, error code otherwise
 */
s32 security_validate_access(u32 uid, u32 gid, u32 target_uid, u32 target_gid, u8 perms)
{
    /* Root can access anything */
    if (uid == 0) {
        return OK;
    }

    /* Same user */
    if (uid == target_uid) {
        return OK;
    }

    /* Same group */
    if (gid == target_gid) {
        return OK;
    }

    /* No access */
    return EACCES;
}

/*===========================================================================*/
/*                         SECURITY AUDITING                                 */
/*===========================================================================*/

/**
 * security_audit_init - Initialize audit log
 * @log: Audit log to initialize
 * @max_events: Maximum events to buffer
 *
 * Returns: OK on success, error code on failure
 */
s32 security_audit_init(security_audit_log_t *log, u32 max_events)
{
    if (!log) {
        return EINVAL;
    }

    spin_lock_init_named(&log->lock, "audit_log");
    log->head = 0;
    log->tail = 0;
    log->count = 0;
    log->max_events = max_events;
    log->total_events = 0;
    log->dropped_events = 0;

    /* Allocate event buffer */
    log->events = kzalloc(sizeof(security_event_t) * max_events);
    if (!log->events) {
        return ENOMEM;
    }

    return OK;
}

/**
 * security_audit_shutdown - Shutdown audit log
 * @log: Audit log to shutdown
 */
void security_audit_shutdown(security_audit_log_t *log)
{
    if (!log) {
        return;
    }

    spin_lock(&log->lock);

    if (log->events) {
        kfree(log->events);
        log->events = NULL;
    }

    log->max_events = 0;
    log->count = 0;

    spin_unlock(&log->lock);
}

/**
 * security_audit_log - Log a security event
 * @log: Audit log
 * @event: Event to log
 *
 * Returns: OK on success, error code on failure
 */
s32 security_audit_log(security_audit_log_t *log, security_event_t *event)
{
    if (!log || !event) {
        return EINVAL;
    }

    spin_lock(&log->lock);

    /* Check if buffer is full */
    if (log->count >= log->max_events) {
        /* Drop oldest event */
        log->tail = (log->tail + 1) % log->max_events;
        log->dropped_events++;
        log->count--;
    }

    /* Copy event to buffer */
    log->events[log->head] = *event;
    log->head = (log->head + 1) % log->max_events;
    log->count++;
    log->total_events++;

    spin_unlock(&log->lock);

    return OK;
}

/**
 * security_audit_read - Read next event from audit log
 * @log: Audit log
 * @event: Buffer for event
 *
 * Returns: OK on success, ENOENT if no events
 */
s32 security_audit_read(security_audit_log_t *log, security_event_t *event)
{
    if (!log || !event) {
        return EINVAL;
    }

    spin_lock(&log->lock);

    if (log->count == 0) {
        spin_unlock(&log->lock);
        return ENOENT;
    }

    *event = log->events[log->tail];
    log->tail = (log->tail + 1) % log->max_events;
    log->count--;

    spin_unlock(&log->lock);

    return OK;
}

/**
 * security_log_event - Log a security event with formatting
 * @type: Event type
 * @severity: Event severity
 * @fmt: Format string
 */
void security_log_event(u32 type, u32 severity, const char *fmt, ...)
{
    security_event_t event;
    __builtin_va_list args;

    event.id = (u32)g_security_manager.total_events;
    event.type = type;
    event.severity = severity;
    event.pid = 0;  /* Would be current->pid */
    event.uid = 0;  /* Would be current->uid */
    event.timestamp = get_time_ms();
    event.data = 0;
    event.result = OK;

    __builtin_va_start(args, fmt);
    /* vsnprintf would be used here */
    strncpy(event.message, fmt, SECURITY_MAX_AUDIT_LOG - 1);
    __builtin_va_end(args);

    strncpy(event.source, "security", sizeof(event.source) - 1);

    security_audit_log(&g_security_manager.audit_log, &event);
    g_security_manager.total_events++;
}

/**
 * security_log_auth - Log authentication event
 * @uid: User ID
 * @success: Authentication result
 * @reason: Reason string
 */
void security_log_auth(u32 uid, bool success, const char *reason)
{
    security_event_t event;

    event.id = (u32)g_security_manager.total_events;
    event.type = SECURITY_EVENT_AUTH;
    event.severity = success ? SECURITY_SEVERITY_INFO : SECURITY_SEVERITY_WARNING;
    event.uid = uid;
    event.timestamp = get_time_ms();
    event.result = success ? OK : EPERM;

    strncpy(event.message, reason ? reason : "auth event", SECURITY_MAX_AUDIT_LOG - 1);
    strncpy(event.source, "auth", sizeof(event.source) - 1);

    security_audit_log(&g_security_manager.audit_log, &event);
    g_security_manager.total_events++;

    if (!success) {
        g_security_manager.violations++;
    }
}

/**
 * security_log_access - Log access control event
 * @uid: User ID
 * @resource: Resource identifier
 * @granted: Access granted flag
 */
void security_log_access(u32 uid, u32 resource, bool granted)
{
    security_event_t event;

    event.id = (u32)g_security_manager.total_events;
    event.type = SECURITY_EVENT_ACCESS;
    event.severity = granted ? SECURITY_SEVERITY_INFO : SECURITY_SEVERITY_WARNING;
    event.uid = uid;
    event.timestamp = get_time_ms();
    event.data = resource;
    event.result = granted ? OK : EACCES;

    if (granted) {
        strncpy(event.message, "access granted", SECURITY_MAX_AUDIT_LOG - 1);
    } else {
        strncpy(event.message, "access denied", SECURITY_MAX_AUDIT_LOG - 1);
        g_security_manager.violations++;
    }

    strncpy(event.source, "acl", sizeof(event.source) - 1);

    security_audit_log(&g_security_manager.audit_log, &event);
    g_security_manager.total_events++;
}

/**
 * security_log_violation - Log security violation
 * @uid: User ID
 * @reason: Violation reason
 */
void security_log_violation(u32 uid, const char *reason)
{
    security_event_t event;

    event.id = (u32)g_security_manager.total_events;
    event.type = SECURITY_EVENT_AUDIT;
    event.severity = SECURITY_SEVERITY_ERROR;
    event.uid = uid;
    event.timestamp = get_time_ms();
    event.result = EPERM;

    strncpy(event.message, reason ? reason : "security violation", SECURITY_MAX_AUDIT_LOG - 1);
    strncpy(event.source, "violation", sizeof(event.source) - 1);

    security_audit_log(&g_security_manager.audit_log, &event);
    g_security_manager.total_events++;
    g_security_manager.violations++;
}

/**
 * security_audit_count - Get number of events in log
 * @log: Audit log
 *
 * Returns: Number of buffered events
 */
u32 security_audit_count(security_audit_log_t *log)
{
    u32 count;

    if (!log) {
        return 0;
    }

    spin_lock(&log->lock);
    count = log->count;
    spin_unlock(&log->lock);

    return count;
}

/**
 * security_audit_total - Get total events logged
 * @log: Audit log
 *
 * Returns: Total events logged
 */
u64 security_audit_total(security_audit_log_t *log)
{
    if (!log) {
        return 0;
    }

    return log->total_events;
}

/**
 * security_audit_dropped - Get dropped events count
 * @log: Audit log
 *
 * Returns: Number of dropped events
 */
u64 security_audit_dropped(security_audit_log_t *log)
{
    if (!log) {
        return 0;
    }

    return log->dropped_events;
}

/*===========================================================================*/
/*                         SECURITY UTILITIES                                */
/*===========================================================================*/

/**
 * security_is_root - Check if UID is root
 * @uid: User ID to check
 *
 * Returns: true if UID is 0 (root)
 */
bool security_is_root(u32 uid)
{
    return uid == 0;
}

/**
 * security_is_privileged - Check if context is privileged
 * @ctx: Security context
 *
 * Returns: true if context has privileged status
 */
bool security_is_privileged(security_context_t *ctx)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return false;
    }

    return (ctx->flags & SECURITY_CTX_PRIVILEGED) != 0 ||
           ctx->uid == 0 ||
           security_cap_test(&ctx->caps, CAP_SYS_ADMIN);
}

/**
 * security_is_sandboxed - Check if context is sandboxed
 * @ctx: Security context
 *
 * Returns: true if context is sandboxed
 */
bool security_is_sandboxed(security_context_t *ctx)
{
    if (!ctx || ctx->magic != SECURITY_CTX_MAGIC) {
        return false;
    }

    return (ctx->flags & SECURITY_CTX_SANDBOXED) != 0;
}

/**
 * security_uid_valid - Validate a UID
 * @uid: UID to validate
 *
 * Returns: true if UID is valid
 */
bool security_uid_valid(u32 uid)
{
    return uid != (u32)-1;
}

/**
 * security_gid_valid - Validate a GID
 * @gid: GID to validate
 *
 * Returns: true if GID is valid
 */
bool security_gid_valid(u32 gid)
{
    return gid != (u32)-1;
}

/**
 * security_uid_equal - Compare two UIDs
 * @uid1: First UID
 * @uid2: Second UID
 *
 * Returns: true if UIDs are equal
 */
bool security_uid_equal(u32 uid1, u32 uid2)
{
    return uid1 == uid2;
}

/**
 * security_perm_from_mode - Extract permissions from mode
 * @mode: File mode
 *
 * Returns: Permission bits
 */
u8 security_perm_from_mode(u32 mode)
{
    return (u8)(mode & 0x1FF);
}

/**
 * security_mode_from_perm - Create mode from permissions
 * @perm: Permission bits
 * @is_dir: Is directory flag
 *
 * Returns: File mode
 */
u32 security_mode_from_perm(u8 perm, bool is_dir)
{
    u32 mode = perm & 0x1FF;

    if (is_dir) {
        mode |= 0x4000;  /* S_IFDIR */
    } else {
        mode |= 0x8000;  /* S_IFREG */
    }

    return mode;
}

/*===========================================================================*/
/*                         STATISTICS AND DEBUGGING                          */
/*===========================================================================*/

/**
 * security_stats - Print security statistics
 */
void security_stats(void)
{
    printk("\n=== Security Framework Statistics ===\n");
    printk("State: %u\n", g_security_manager.state);
    printk("Security Level: %u\n", g_security_manager.security_level);
    printk("Active Contexts: %u\n", g_security_manager.context_count);
    printk("Active Policies: %u\n", g_security_manager.policy_count);
    printk("Total Events: %llu\n", (unsigned long long)g_security_manager.total_events);
    printk("Violations: %llu\n", (unsigned long long)g_security_manager.violations);
    printk("Audit Buffer: %u/%u events\n",
           security_audit_count(&g_security_manager.audit_log),
           g_security_manager.audit_log.max_events);
    printk("Audit Dropped: %llu\n",
           (unsigned long long)g_security_manager.audit_log.dropped_events);
}

/**
 * security_dump_contexts - Dump all security contexts
 */
void security_dump_contexts(void)
{
    u32 i;
    security_context_t *ctx;

    printk("\n=== Security Contexts ===\n");

    spin_lock(&context_table_lock);

    for (i = 0; i < SECURITY_MAX_CONTEXTS; i++) {
        ctx = security_context_table[i];
        if (ctx && ctx->magic == SECURITY_CTX_MAGIC) {
            printk("  [%u] PID=%u UID=%u GID=%u Label=%s\n",
                   i, ctx->pid, ctx->uid, ctx->gid, ctx->label);
        }
    }

    spin_unlock(&context_table_lock);
}

/**
 * security_dump_policies - Dump all security policies
 */
void security_dump_policies(void)
{
    u32 i;
    security_policy_t *policy;

    printk("\n=== Security Policies ===\n");

    spin_lock(&policy_table_lock);

    for (i = 0; i < SECURITY_MAX_POLICIES; i++) {
        policy = security_policy_table[i];
        if (policy && policy->magic == SECURITY_POLICY_MAGIC) {
            printk("  [%u] ID=%u Name=%s Enabled=%s\n",
                   i, policy->id, policy->name,
                   policy->enabled ? "yes" : "no");
        }
    }

    spin_unlock(&policy_table_lock);
}

/*===========================================================================*/
/*                         MODULE INITIALIZATION                             */
/*===========================================================================*/

/**
 * security_module_init - Security module initialization
 *
 * Called during kernel boot to initialize the security framework.
 */
void security_module_init(void)
{
    s32 ret;

    pr_info("Security: Loading security module...\n");

    ret = security_init();
    if (ret != OK) {
        pr_err("Security: Failed to initialize (ret=%d)\n", ret);
        return;
    }

    pr_info("Security: Module loaded successfully\n");
}
