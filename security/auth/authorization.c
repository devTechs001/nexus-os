/*
 * NEXUS OS - Security Framework
 * security/auth/authorization.c
 *
 * Authorization Subsystem
 * Access control, permissions, and privilege management
 */

#include "../security.h"

/*===========================================================================*/
/*                         AUTHORIZATION CONFIGURATION                       */
/*===========================================================================*/

#define AUTHZ_DEBUG             0
#define AUTHZ_MAX_RULES         4096
#define AUTHZ_MAX_GROUPS        1024

/* Authorization Decision */
#define AUTHZ_DECISION_NONE     0
#define AUTHZ_DECISION_ALLOW    1
#define AUTHZ_DECISION_DENY     2
#define AUTHZ_DECISION_AUDIT    3

/* Rule Types */
#define RULE_TYPE_NONE          0
#define RULE_TYPE_ALLOW         1
#define RULE_TYPE_DENY          2
#define RULE_TYPE_AUDIT         3

/* Permission Types */
#define PERM_READ               0x01
#define PERM_WRITE              0x02
#define PERM_EXECUTE            0x04
#define PERM_ADMIN              0x08
#define PERM_ALL                0x0F

/* Magic Numbers */
#define AUTHZ_RULE_MAGIC        0x415554485A525501ULL  /* AUTHZRU1 */
#define AUTHZ_GROUP_MAGIC       0x415554485A475250ULL  /* AUTHZGRP */

/*===========================================================================*/
/*                         AUTHORIZATION DATA STRUCTURES                     */
/*===========================================================================*/

/**
 * authz_rule_t - Authorization rule
 *
 * Defines a single authorization rule for access control.
 */
typedef struct {
    u64 magic;                  /* Magic number */
    spinlock_t lock;            /* Protection lock */
    u32 id;                     /* Rule ID */
    u32 type;                   /* Rule type */
    u32 priority;               /* Rule priority (higher = evaluated first) */
    u32 uid;                    /* Target user ID (0 = all) */
    u32 gid;                    /* Target group ID (0 = all) */
    char resource[256];         /* Resource pattern */
    u32 permissions;            /* Required permissions */
    u32 conditions;             /* Additional conditions */
    u64 created;                /* Creation time */
    u64 expires;                /* Expiration time (0 = never) */
    bool enabled;               /* Rule enabled flag */
    u64 match_count;            /* Number of matches */
    u64 deny_count;             /* Number of denials */
} authz_rule_t;

/**
 * authz_group_t - Authorization group
 *
 * Groups users for collective permission management.
 */
typedef struct {
    u64 magic;                  /* Magic number */
    spinlock_t lock;            /* Protection lock */
    u32 id;                     /* Group ID */
    char name[64];              /* Group name */
    u32 member_count;           /* Number of members */
    u32 *members;               /* Member UIDs */
    u32 max_members;            /* Maximum members */
    u32 permissions;            /* Group permissions */
    u64 created;                /* Creation time */
} authz_group_t;

/**
 * authz_context_t - Authorization evaluation context
 *
 * Holds state during authorization evaluation.
 */
typedef struct {
    u32 uid;                    /* User ID */
    u32 gid;                    /* Group ID */
    u32 *groups;                /* Supplementary groups */
    u32 group_count;            /* Number of supplementary groups */
    u64 capabilities;           /* Effective capabilities */
    char *resource;             /* Target resource */
    u32 requested_perm;         /* Requested permissions */
    u32 decision;               /* Evaluation decision */
    u32 matched_rule;           /* ID of matched rule */
    char reason[128];           /* Decision reason */
} authz_context_t;

/**
 * authz_manager_t - Authorization manager state
 */
typedef struct {
    spinlock_t lock;            /* Manager lock */
    u32 state;                  /* Manager state */
    authz_rule_t **rules;       /* Rule table */
    u32 rule_count;             /* Active rules */
    authz_group_t **groups;     /* Group table */
    u32 group_count;            /* Active groups */
    u32 next_rule_id;           /* Next rule ID */
    u32 next_group_id;          /* Next group ID */
    u64 total_evaluations;      /* Total evaluations */
    u64 total_allowed;          /* Total allowed */
    u64 total_denied;           /* Total denied */
    u32 default_policy;         /* Default policy (allow/deny) */
} authz_manager_t;

/*===========================================================================*/
/*                         AUTHORIZATION GLOBAL DATA                         */
/*===========================================================================*/

static authz_manager_t g_authz_manager = {
    .lock = {
        .lock = SPINLOCK_UNLOCKED,
        .magic = SPINLOCK_MAGIC,
        .name = "authz_manager",
    },
    .state = 0,
    .rules = NULL,
    .rule_count = 0,
    .groups = NULL,
    .group_count = 0,
    .next_rule_id = 1,
    .next_group_id = 1,
    .default_policy = AUTHZ_DECISION_DENY,
};

static authz_rule_t *authz_rule_table[AUTHZ_MAX_RULES];
static authz_group_t *authz_group_table[AUTHZ_MAX_GROUPS];

/*===========================================================================*/
/*                         AUTHORIZATION INITIALIZATION                      */
/*===========================================================================*/

/**
 * authorization_init - Initialize authorization subsystem
 *
 * Initializes the authorization subsystem with default settings.
 *
 * Returns: OK on success, error code on failure
 */
s32 authorization_init(void)
{
    u32 i;

    pr_info("AuthZ: Initializing authorization subsystem...\n");

    spin_lock_init_named(&g_authz_manager.lock, "authz_manager");
    g_authz_manager.state = 1;

    /* Initialize rule table */
    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        authz_rule_table[i] = NULL;
    }

    /* Initialize group table */
    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        authz_group_table[i] = NULL;
    }

    g_authz_manager.rules = authz_rule_table;
    g_authz_manager.groups = authz_group_table;

    /* Create default groups */
    authz_group_create("root", 0);
    authz_group_create("admin", 0);
    authz_group_create("users", 0);

    /* Create default rules */
    authz_rule_create(RULE_TYPE_ALLOW, 0, 0, "*", PERM_ALL, 100);
    authz_rule_create(RULE_TYPE_DENY, 0, 0, "/system/*", PERM_ALL, 50);

    g_authz_manager.state = 2;

    pr_info("AuthZ: Subsystem initialized\n");
    pr_info("  Max rules: %u\n", AUTHZ_MAX_RULES);
    pr_info("  Max groups: %u\n", AUTHZ_MAX_GROUPS);
    pr_info("  Default policy: %s\n",
            g_authz_manager.default_policy == AUTHZ_DECISION_ALLOW ? "allow" : "deny");

    return OK;
}

/**
 * authorization_shutdown - Shutdown authorization subsystem
 *
 * Cleans up all authorization resources.
 */
void authorization_shutdown(void)
{
    u32 i;

    pr_info("AuthZ: Shutting down...\n");

    spin_lock(&g_authz_manager.lock);

    /* Free all rules */
    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        if (authz_rule_table[i]) {
            kfree(authz_rule_table[i]);
            authz_rule_table[i] = NULL;
        }
    }

    /* Free all groups */
    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i]) {
            if (authz_group_table[i]->members) {
                kfree(authz_group_table[i]->members);
            }
            kfree(authz_group_table[i]);
            authz_group_table[i] = NULL;
        }
    }

    g_authz_manager.rule_count = 0;
    g_authz_manager.group_count = 0;
    g_authz_manager.state = 0;

    spin_unlock(&g_authz_manager.lock);

    pr_info("AuthZ: Shutdown complete\n");
}

/*===========================================================================*/
/*                         RULE MANAGEMENT                                   */
/*===========================================================================*/

/**
 * authz_rule_create - Create an authorization rule
 * @type: Rule type (ALLOW/DENY/AUDIT)
 * @uid: Target user ID (0 = all)
 * @gid: Target group ID (0 = all)
 * @resource: Resource pattern
 * @permissions: Required permissions
 * @priority: Rule priority
 *
 * Creates a new authorization rule.
 *
 * Returns: Rule ID on success, error code on failure
 */
s32 authz_rule_create(u32 type, u32 uid, u32 gid, const char *resource,
                      u32 permissions, u32 priority)
{
    authz_rule_t *rule;
    u32 i;

    if (!resource) {
        return EINVAL;
    }

    /* Find empty slot */
    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        if (authz_rule_table[i] == NULL) {
            break;
        }
    }

    if (i >= AUTHZ_MAX_RULES) {
        return ENOSPC;
    }

    /* Allocate rule */
    rule = kzalloc(sizeof(authz_rule_t));
    if (!rule) {
        return ENOMEM;
    }

    /* Initialize rule */
    rule->magic = AUTHZ_RULE_MAGIC;
    spin_lock_init_named(&rule->lock, "authz_rule");
    rule->id = g_authz_manager.next_rule_id++;
    rule->type = type;
    rule->priority = priority;
    rule->uid = uid;
    rule->gid = gid;
    strncpy(rule->resource, resource, sizeof(rule->resource) - 1);
    rule->permissions = permissions;
    rule->created = get_time_ms();
    rule->enabled = true;

    /* Store in table */
    authz_rule_table[i] = rule;
    g_authz_manager.rule_count++;

    pr_debug("AuthZ: Created rule %u (type=%u, priority=%u)\n",
             rule->id, type, priority);

    return rule->id;
}

/**
 * authz_rule_delete - Delete an authorization rule
 * @rule_id: Rule ID to delete
 *
 * Removes an authorization rule.
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_rule_delete(u32 rule_id)
{
    u32 i;

    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        if (authz_rule_table[i] && authz_rule_table[i]->id == rule_id) {
            spin_lock(&authz_rule_table[i]->lock);
            authz_rule_table[i]->magic = 0;
            spin_unlock(&authz_rule_table[i]->lock);

            kfree(authz_rule_table[i]);
            authz_rule_table[i] = NULL;
            g_authz_manager.rule_count--;

            return OK;
        }
    }

    return ENOENT;
}

/**
 * authz_rule_get - Get a rule by ID
 * @rule_id: Rule ID
 *
 * Returns: Pointer to rule, or NULL if not found
 */
authz_rule_t *authz_rule_get(u32 rule_id)
{
    u32 i;

    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        if (authz_rule_table[i] && authz_rule_table[i]->id == rule_id) {
            return authz_rule_table[i];
        }
    }

    return NULL;
}

/**
 * authz_rule_enable - Enable a rule
 * @rule_id: Rule ID
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_rule_enable(u32 rule_id)
{
    authz_rule_t *rule = authz_rule_get(rule_id);

    if (!rule) {
        return ENOENT;
    }

    spin_lock(&rule->lock);
    rule->enabled = true;
    spin_unlock(&rule->lock);

    return OK;
}

/**
 * authz_rule_disable - Disable a rule
 * @rule_id: Rule ID
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_rule_disable(u32 rule_id)
{
    authz_rule_t *rule = authz_rule_get(rule_id);

    if (!rule) {
        return ENOENT;
    }

    spin_lock(&rule->lock);
    rule->enabled = false;
    spin_unlock(&rule->lock);

    return OK;
}

/*===========================================================================*/
/*                         GROUP MANAGEMENT                                  */
/*===========================================================================*/

/**
 * authz_group_create - Create an authorization group
 * @name: Group name
 * @max_members: Maximum members
 *
 * Creates a new authorization group.
 *
 * Returns: Group ID on success, error code on failure
 */
s32 authz_group_create(const char *name, u32 max_members)
{
    authz_group_t *group;
    u32 i;

    if (!name || strlen(name) == 0) {
        return EINVAL;
    }

    /* Find empty slot */
    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i] == NULL) {
            break;
        }
    }

    if (i >= AUTHZ_MAX_GROUPS) {
        return ENOSPC;
    }

    /* Allocate group */
    group = kzalloc(sizeof(authz_group_t));
    if (!group) {
        return ENOMEM;
    }

    /* Initialize group */
    group->magic = AUTHZ_GROUP_MAGIC;
    spin_lock_init_named(&group->lock, "authz_group");
    group->id = g_authz_manager.next_group_id++;
    strncpy(group->name, name, sizeof(group->name) - 1);
    group->max_members = max_members > 0 ? max_members : 256;
    group->created = get_time_ms();

    /* Allocate member array */
    group->members = kzalloc(sizeof(u32) * group->max_members);
    if (!group->members) {
        kfree(group);
        return ENOMEM;
    }

    /* Store in table */
    authz_group_table[i] = group;
    g_authz_manager.group_count++;

    pr_debug("AuthZ: Created group '%s' (ID=%u)\n", name, group->id);

    return group->id;
}

/**
 * authz_group_delete - Delete an authorization group
 * @group_id: Group ID
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_group_delete(u32 group_id)
{
    u32 i;

    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i] && authz_group_table[i]->id == group_id) {
            spin_lock(&authz_group_table[i]->lock);
            authz_group_table[i]->magic = 0;
            spin_unlock(&authz_group_table[i]->lock);

            if (authz_group_table[i]->members) {
                kfree(authz_group_table[i]->members);
            }
            kfree(authz_group_table[i]);
            authz_group_table[i] = NULL;
            g_authz_manager.group_count--;

            return OK;
        }
    }

    return ENOENT;
}

/**
 * authz_group_add_member - Add a member to a group
 * @group_id: Group ID
 * @uid: User ID to add
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_group_add_member(u32 group_id, u32 uid)
{
    authz_group_t *group;
    u32 i;

    group = authz_group_table[0];  /* Simplified lookup */
    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i] && authz_group_table[i]->id == group_id) {
            group = authz_group_table[i];
            break;
        }
    }

    if (!group || group->magic != AUTHZ_GROUP_MAGIC) {
        return ENOENT;
    }

    spin_lock(&group->lock);

    /* Check if already a member */
    for (i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            spin_unlock(&group->lock);
            return EEXIST;
        }
    }

    /* Check capacity */
    if (group->member_count >= group->max_members) {
        spin_unlock(&group->lock);
        return ENOSPC;
    }

    /* Add member */
    group->members[group->member_count++] = uid;

    spin_unlock(&group->lock);

    pr_debug("AuthZ: Added UID %u to group %u\n", uid, group_id);

    return OK;
}

/**
 * authz_group_remove_member - Remove a member from a group
 * @group_id: Group ID
 * @uid: User ID to remove
 *
 * Returns: OK on success, error code on failure
 */
s32 authz_group_remove_member(u32 group_id, u32 uid)
{
    authz_group_t *group;
    u32 i, j;

    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i] && authz_group_table[i]->id == group_id) {
            group = authz_group_table[i];
            break;
        }
    }

    if (!group || group->magic != AUTHZ_GROUP_MAGIC) {
        return ENOENT;
    }

    spin_lock(&group->lock);

    /* Find and remove member */
    for (i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            /* Shift remaining members */
            for (j = i; j < group->member_count - 1; j++) {
                group->members[j] = group->members[j + 1];
            }
            group->member_count--;

            spin_unlock(&group->lock);
            return OK;
        }
    }

    spin_unlock(&group->lock);
    return ENOENT;
}

/**
 * authz_group_is_member - Check if user is a group member
 * @group_id: Group ID
 * @uid: User ID
 *
 * Returns: true if member, false otherwise
 */
bool authz_group_is_member(u32 group_id, u32 uid)
{
    authz_group_t *group;
    u32 i;

    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        if (authz_group_table[i] && authz_group_table[i]->id == group_id) {
            group = authz_group_table[i];
            break;
        }
    }

    if (!group || group->magic != AUTHZ_GROUP_MAGIC) {
        return false;
    }

    spin_lock(&group->lock);

    for (i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            spin_unlock(&group->lock);
            return true;
        }
    }

    spin_unlock(&group->lock);
    return false;
}

/*===========================================================================*/
/*                         AUTHORIZATION EVALUATION                          */
/*===========================================================================*/

/**
 * authz_match_resource - Check if resource matches pattern
 * @pattern: Resource pattern (supports * wildcard)
 * @resource: Actual resource path
 *
 * Returns: true if matches, false otherwise
 */
static bool authz_match_resource(const char *pattern, const char *resource)
{
    if (!pattern || !resource) {
        return false;
    }

    /* Handle wildcard */
    if (strcmp(pattern, "*") == 0) {
        return true;
    }

    /* Handle prefix wildcard */
    if (pattern[strlen(pattern) - 1] == '*') {
        u32 prefix_len = strlen(pattern) - 1;
        return strncmp(pattern, resource, prefix_len) == 0;
    }

    /* Exact match */
    return strcmp(pattern, resource) == 0;
}

/**
 * authz_evaluate_rule - Evaluate a single rule
 * @rule: Rule to evaluate
 * @ctx: Authorization context
 *
 * Returns: AUTHZ_DECISION_ALLOW, AUTHZ_DECISION_DENY, or AUTHZ_DECISION_NONE
 */
static u32 authz_evaluate_rule(authz_rule_t *rule, authz_context_t *ctx)
{
    if (!rule || !ctx) {
        return AUTHZ_DECISION_NONE;
    }

    spin_lock(&rule->lock);

    /* Check if rule is enabled */
    if (!rule->enabled) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    /* Check expiration */
    if (rule->expires > 0 && get_time_ms() > rule->expires) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    /* Check user match */
    if (rule->uid != 0 && rule->uid != ctx->uid) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    /* Check group match */
    if (rule->gid != 0 && rule->gid != ctx->gid) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    /* Check resource match */
    if (!authz_match_resource(rule->resource, ctx->resource)) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    /* Check permissions */
    if (!(rule->permissions & ctx->requested_perm)) {
        spin_unlock(&rule->lock);
        return AUTHZ_DECISION_NONE;
    }

    rule->match_count++;

    spin_unlock(&rule->lock);

    /* Return decision based on rule type */
    switch (rule->type) {
        case RULE_TYPE_ALLOW:
            return AUTHZ_DECISION_ALLOW;
        case RULE_TYPE_DENY:
            rule->deny_count++;
            return AUTHZ_DECISION_DENY;
        case RULE_TYPE_AUDIT:
            return AUTHZ_DECISION_AUDIT;
        default:
            return AUTHZ_DECISION_NONE;
    }
}

/**
 * authz_evaluate - Evaluate authorization request
 * @ctx: Authorization context
 *
 * Evaluates all rules and returns authorization decision.
 *
 * Returns: AUTHZ_DECISION_ALLOW or AUTHZ_DECISION_DENY
 */
static u32 authz_evaluate(authz_context_t *ctx)
{
    u32 i;
    u32 decision = g_authz_manager.default_policy;
    authz_rule_t *matched_rule = NULL;
    u32 highest_priority = 0;

    g_authz_manager.total_evaluations++;

    /* Evaluate rules in priority order */
    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        authz_rule_t *rule = authz_rule_table[i];
        if (!rule || rule->magic != AUTHZ_RULE_MAGIC) {
            continue;
        }

        u32 rule_decision = authz_evaluate_rule(rule, ctx);

        if (rule_decision != AUTHZ_DECISION_NONE) {
            if (rule->priority >= highest_priority) {
                highest_priority = rule->priority;
                decision = rule_decision;
                matched_rule = rule;
            }
        }
    }

    /* Update statistics */
    if (decision == AUTHZ_DECISION_ALLOW) {
        g_authz_manager.total_allowed++;
    } else {
        g_authz_manager.total_denied++;
    }

    /* Store result in context */
    ctx->decision = decision;
    if (matched_rule) {
        ctx->matched_rule = matched_rule->id;
        snprintf(ctx->reason, sizeof(ctx->reason), "Matched rule %u",
                 matched_rule->id);
    } else {
        snprintf(ctx->reason, sizeof(ctx->reason), "Default policy");
    }

    return decision;
}

/**
 * authz_check - Check if access is authorized
 * @uid: User ID
 * @gid: Group ID
 * @resource: Resource path
 * @permissions: Requested permissions
 *
 * Performs authorization check.
 *
 * Returns: OK if authorized, EACCES otherwise
 */
s32 authz_check(u32 uid, u32 gid, const char *resource, u32 permissions)
{
    authz_context_t ctx;
    u32 decision;

    if (!resource) {
        return EINVAL;
    }

    /* Initialize context */
    ctx.uid = uid;
    ctx.gid = gid;
    ctx.groups = NULL;
    ctx.group_count = 0;
    ctx.capabilities = 0;
    ctx.resource = (char *)resource;
    ctx.requested_perm = permissions;
    ctx.decision = AUTHZ_DECISION_NONE;
    ctx.matched_rule = 0;

    /* Evaluate */
    decision = authz_evaluate(&ctx);

    if (decision == AUTHZ_DECISION_ALLOW) {
        return OK;
    }

    security_log_access(uid, 0, false);

    return EACCES;
}

/**
 * authz_check_capability - Check if user has capability
 * @uid: User ID
 * @capability: Capability to check
 *
 * Returns: OK if capable, EPERM otherwise
 */
s32 authz_check_capability(u32 uid, u64 capability)
{
    /* Root has all capabilities */
    if (uid == 0) {
        return OK;
    }

    /* Check against capabilities */
    if (capability & CAP_ALL) {
        return OK;
    }

    return EPERM;
}

/*===========================================================================*/
/*                         CONVENIENCE FUNCTIONS                             */
/*===========================================================================*/

/**
 * authz_check_read - Check read permission
 * @uid: User ID
 * @resource: Resource path
 *
 * Returns: OK if authorized, error code otherwise
 */
s32 authz_check_read(u32 uid, const char *resource)
{
    return authz_check(uid, 0, resource, PERM_READ);
}

/**
 * authz_check_write - Check write permission
 * @uid: User ID
 * @resource: Resource path
 *
 * Returns: OK if authorized, error code otherwise
 */
s32 authz_check_write(u32 uid, const char *resource)
{
    return authz_check(uid, 0, resource, PERM_WRITE);
}

/**
 * authz_check_execute - Check execute permission
 * @uid: User ID
 * @resource: Resource path
 *
 * Returns: OK if authorized, error code otherwise
 */
s32 authz_check_execute(u32 uid, const char *resource)
{
    return authz_check(uid, 0, resource, PERM_EXECUTE);
}

/**
 * authz_check_admin - Check admin permission
 * @uid: User ID
 * @resource: Resource path
 *
 * Returns: OK if authorized, error code otherwise
 */
s32 authz_check_admin(u32 uid, const char *resource)
{
    /* Only root can perform admin operations */
    if (uid != 0) {
        return EPERM;
    }

    return authz_check(uid, 0, resource, PERM_ADMIN);
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * authz_stats - Print authorization statistics
 */
void authz_stats(void)
{
    printk("\n=== Authorization Statistics ===\n");
    printk("State: %u\n", g_authz_manager.state);
    printk("Active Rules: %u\n", g_authz_manager.rule_count);
    printk("Active Groups: %u\n", g_authz_manager.group_count);
    printk("Total Evaluations: %llu\n",
           (unsigned long long)g_authz_manager.total_evaluations);
    printk("Total Allowed: %llu\n",
           (unsigned long long)g_authz_manager.total_allowed);
    printk("Total Denied: %llu\n",
           (unsigned long long)g_authz_manager.total_denied);

    if (g_authz_manager.total_evaluations > 0) {
        u32 allow_rate = (g_authz_manager.total_allowed * 100) /
                         g_authz_manager.total_evaluations;
        printk("Allow Rate: %u%%\n", allow_rate);
    }
}

/**
 * authz_dump_rules - Dump all rules
 */
void authz_dump_rules(void)
{
    u32 i;
    authz_rule_t *rule;

    printk("\n=== Authorization Rules ===\n");

    for (i = 0; i < AUTHZ_MAX_RULES; i++) {
        rule = authz_rule_table[i];
        if (rule && rule->magic == AUTHZ_RULE_MAGIC) {
            printk("  [%u] ID=%u Type=%u Priority=%u UID=%u GID=%u "
                   "Resource=%s Enabled=%s\n",
                   i, rule->id, rule->type, rule->priority,
                   rule->uid, rule->gid, rule->resource,
                   rule->enabled ? "yes" : "no");
        }
    }
}

/**
 * authz_dump_groups - Dump all groups
 */
void authz_dump_groups(void)
{
    u32 i;
    authz_group_t *group;

    printk("\n=== Authorization Groups ===\n");

    for (i = 0; i < AUTHZ_MAX_GROUPS; i++) {
        group = authz_group_table[i];
        if (group && group->magic == AUTHZ_GROUP_MAGIC) {
            printk("  [%u] ID=%u Name=%s Members=%u\n",
                   i, group->id, group->name, group->member_count);
        }
    }
}
