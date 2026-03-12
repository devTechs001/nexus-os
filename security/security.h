/*
 * NEXUS OS - Security Framework
 * security/security.h
 *
 * Security Framework Header
 * Core security infrastructure for NEXUS OS
 */

#ifndef _NEXUS_SECURITY_H
#define _NEXUS_SECURITY_H

#include "../kernel/include/kernel.h"
#include "../kernel/sync/sync.h"

/*===========================================================================*/
/*                         SECURITY CONSTANTS                                */
/*===========================================================================*/

/* Security Framework Version */
#define SECURITY_VERSION_MAJOR      1
#define SECURITY_VERSION_MINOR      0
#define SECURITY_VERSION_PATCH      0
#define SECURITY_VERSION_STRING     "1.0.0"

/* Security Levels */
#define SECURITY_LEVEL_NONE         0
#define SECURITY_LEVEL_BASIC        1
#define SECURITY_LEVEL_ENHANCED     2
#define SECURITY_LEVEL_HIGH         3
#define SECURITY_LEVEL_MAX          4

/* Security Context Flags */
#define SECURITY_CTX_VALID          0x00000001
#define SECURITY_CTX_PRIVILEGED     0x00000002
#define SECURITY_CTX_RESTRICTED     0x00000003
#define SECURITY_CTX_SANDBOXED      0x00000004
#define SECURITY_CTX_SYSTEM         0x00000008
#define SECURITY_CTX_USER           0x00000010

/* Capability Flags (64-bit capability set) */
#define CAP_NONE                    0x0000000000000000ULL
#define CAP_SYS_ADMIN               0x0000000000000001ULL
#define CAP_SYS_BOOT                0x0000000000000002ULL
#define CAP_SYS_MODULE              0x0000000000000004ULL
#define CAP_SYS_RAWIO               0x0000000000000008ULL
#define CAP_SYS_PTRACE              0x0000000000000010ULL
#define CAP_NET_ADMIN               0x0000000000000020ULL
#define CAP_NET_RAW                 0x0000000000000040ULL
#define CAP_NET_BIND_SERVICE        0x0000000000000080ULL
#define CAP_DAC_OVERRIDE            0x0000000000000100ULL
#define CAP_DAC_READ_SEARCH         0x0000000000000200ULL
#define CAP_FOWNER                  0x0000000000000400ULL
#define CAP_FSETID                  0x0000000000000800ULL
#define CAP_KILL                    0x0000000000001000ULL
#define CAP_SETGID                  0x0000000000002000ULL
#define CAP_SETUID                  0x0000000000004000ULL
#define CAP_SETPCAP                 0x0000000000008000ULL
#define CAP_LINUX_IMMUTABLE         0x0000000000010000ULL
#define CAP_CHOWN                   0x0000000000020000ULL
#define CAP_MKNOD                   0x0000000000040000ULL
#define CAP_AUDIT_CONTROL           0x0000000000080000ULL
#define CAP_AUDIT_WRITE             0x0000000000100000ULL
#define CAP_MAC_ADMIN               0x0000000000200000ULL
#define CAP_MAC_OVERRIDE            0x0000000000400000ULL
#define CAP_IPC_LOCK                0x0000000000800000ULL
#define CAP_IPC_OWNER               0x0000000001000000ULL
#define CAP_SYS_RESOURCE            0x0000000002000000ULL
#define CAP_SYS_TIME                0x0000000004000000ULL
#define CAP_SYS_TTY_CONFIG          0x0000000008000000ULL
#define CAP_WAKE_ALARM              0x0000000010000000ULL
#define CAP_BLOCK_SUSPEND           0x0000000020000000ULL
#define CAP_LEASE                   0x0000000040000000ULL
#define CAP_ALL                     0xFFFFFFFFFFFFFFFFULL

/* Security Event Types */
#define SECURITY_EVENT_NONE         0
#define SECURITY_EVENT_AUTH         1
#define SECURITY_EVENT_ACCESS       2
#define SECURITY_EVENT_PRIVILEGE    3
#define SECURITY_EVENT_POLICY       4
#define SECURITY_EVENT_AUDIT        5
#define SECURITY_EVENT_CRYPTO       6
#define SECURITY_EVENT_SANDBOX      7
#define SECURITY_EVENT_TPM          8

/* Security Event Severity */
#define SECURITY_SEVERITY_INFO      0
#define SECURITY_SEVERITY_WARNING   1
#define SECURITY_SEVERITY_ERROR     2
#define SECURITY_SEVERITY_CRITICAL  3

/* Access Control Types */
#define ACL_TYPE_NONE               0
#define ACL_TYPE_ACCESS             1
#define ACL_TYPE_DEFAULT            2
#define ACL_TYPE_EXTENDED           3

/* ACL Entry Types */
#define ACL_ENTRY_USER              1
#define ACL_ENTRY_GROUP             2
#define ACL_ENTRY_MASK              3
#define ACL_ENTRY_OTHER             4

/* ACL Permissions */
#define ACL_PERM_READ               0x01
#define ACL_PERM_WRITE              0x02
#define ACL_PERM_EXECUTE            0x04
#define ACL_PERM_ADMIN              0x08
#define ACL_PERM_ALL                0x0F

/* Maximum Limits */
#define SECURITY_MAX_CAPABILITIES   64
#define SECURITY_MAX_ACL_ENTRIES    256
#define SECURITY_MAX_CONTEXTS       65536
#define SECURITY_MAX_POLICIES       1024
#define SECURITY_MAX_LABEL_SIZE     256
#define SECURITY_MAX_AUDIT_LOG      4096

/* Security Magic Numbers */
#define SECURITY_CTX_MAGIC          0x5345434354580001ULL  /* SECCTX01 */
#define SECURITY_POLICY_MAGIC       0x534543504F4C0001ULL  /* SECPOL01 */
#define SECURITY_ACL_MAGIC          0x53454341434C0001ULL  /* SECACL01 */

/*===========================================================================*/
/*                         SECURITY DATA STRUCTURES                          */
/*===========================================================================*/

/**
 * security_capabilities_t - Process capability set
 *
 * Represents the set of capabilities granted to a process.
 * Capabilities provide fine-grained privilege control.
 */
typedef struct {
    u64 effective;              /* Effective capabilities */
    u64 permitted;              /* Permitted capabilities */
    u64 inheritable;            /* Inheritable capabilities */
    u64 bounding;               /* Bounding set */
    u64 ambient;                /* Ambient capabilities */
} security_capabilities_t;

/**
 * security_acl_entry_t - Access Control List entry
 *
 * Represents a single ACL entry specifying permissions for
 * a user or group.
 */
typedef struct {
    u32 type;                   /* Entry type (user/group/other) */
    u32 id;                     /* User or group ID */
    u8 permissions;             /* Permission bits */
    u8 flags;                   /* Entry flags */
    u16 reserved;               /* Reserved for alignment */
} security_acl_entry_t;

/**
 * security_acl_t - Access Control List
 *
 * Contains a set of ACL entries defining access permissions
 * for a resource.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 entry_count;            /* Number of entries */
    u32 max_entries;            /* Maximum entries */
    security_acl_entry_t *entries;  /* Entry array */
    u32 owner_id;               /* Resource owner */
    u32 group_id;               /* Resource group */
    u16 mode;                   /* Traditional mode bits */
    u16 flags;                  /* ACL flags */
} security_acl_t;

/**
 * security_context_t - Security context for a process
 *
 * Contains all security-related information for a process,
 * including capabilities, labels, and access control info.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 pid;                    /* Process ID */
    u32 uid;                    /* User ID */
    u32 gid;                    /* Group ID */
    u32 flags;                  /* Context flags */
    security_capabilities_t caps;   /* Capabilities */
    security_acl_t *acl;        /* Access control list */
    char label[SECURITY_MAX_LABEL_SIZE];  /* Security label */
    u64 creation_time;          /* Context creation time */
    u64 last_modified;          /* Last modification time */
    u32 reference_count;        /* Reference count */
    void *private;              /* Private data */
} security_context_t;

/**
 * security_policy_t - Security policy definition
 *
 * Defines a security policy with rules and constraints
 * for access control and system behavior.
 */
typedef struct {
    u64 magic;                  /* Magic number for validation */
    spinlock_t lock;            /* Protection lock */
    u32 id;                     /* Policy ID */
    char name[SECURITY_MAX_LABEL_SIZE];  /* Policy name */
    u32 type;                   /* Policy type */
    u32 flags;                  /* Policy flags */
    u32 rule_count;             /* Number of rules */
    void *rules;                /* Policy rules */
    u64 creation_time;          /* Creation time */
    u64 last_modified;          /* Last modification time */
    bool enabled;               /* Policy enabled flag */
} security_policy_t;

/**
 * security_event_t - Security audit event
 *
 * Represents a security-related event for auditing
 * and logging purposes.
 */
typedef struct {
    u32 id;                     /* Event ID */
    u32 type;                   /* Event type */
    u32 severity;               /* Event severity */
    u32 pid;                    /* Process ID */
    u32 uid;                    /* User ID */
    u64 timestamp;              /* Event timestamp */
    char message[SECURITY_MAX_AUDIT_LOG];  /* Event message */
    char source[64];            /* Event source */
    u64 data;                   /* Additional data */
    u32 result;                 /* Event result */
} security_event_t;

/**
 * security_audit_log_t - Security audit log buffer
 *
 * Circular buffer for storing security audit events.
 */
typedef struct {
    spinlock_t lock;            /* Protection lock */
    u32 head;                   /* Write position */
    u32 tail;                   /* Read position */
    u32 count;                  /* Event count */
    u32 max_events;             /* Maximum events */
    security_event_t *events;   /* Event buffer */
    u64 total_events;           /* Total events logged */
    u64 dropped_events;         /* Dropped events count */
} security_audit_log_t;

/**
 * security_manager_t - Main security manager structure
 *
 * Central structure managing all security subsystems
 * and providing the security API.
 */
typedef struct {
    spinlock_t lock;            /* Manager lock */
    u32 state;                  /* Manager state */
    u32 security_level;         /* Current security level */
    security_context_t *contexts;   /* Context table */
    u32 context_count;          /* Active contexts */
    security_policy_t *policies;    /* Policy table */
    u32 policy_count;           /* Active policies */
    security_audit_log_t audit_log; /* Audit log */
    u64 total_events;           /* Total security events */
    u64 violations;             /* Security violations */
    void *crypto_state;         /* Crypto subsystem state */
    void *tpm_state;            /* TPM subsystem state */
    void *sandbox_state;        /* Sandbox subsystem state */
    void *auth_state;           /* Auth subsystem state */
} security_manager_t;

/*===========================================================================*/
/*                         SECURITY MANAGER API                              */
/*===========================================================================*/

/* Initialization */
s32 security_init(void);
s32 security_subsystem_init(void);
void security_shutdown(void);

/* Manager State */
u32 security_get_level(void);
s32 security_set_level(u32 level);
bool security_is_enabled(void);

/* Context Management */
security_context_t *security_create_context(u32 pid, u32 uid, u32 gid);
void security_destroy_context(security_context_t *ctx);
security_context_t *security_get_context(u32 pid);
s32 security_set_context(security_context_t *ctx);
void security_put_context(security_context_t *ctx);

/* Context Operations */
s32 security_context_set_label(security_context_t *ctx, const char *label);
const char *security_context_get_label(security_context_t *ctx);
s32 security_context_copy(security_context_t *dest, security_context_t *src);

/*===========================================================================*/
/*                         CAPABILITY API                                    */
/*===========================================================================*/

/* Capability Operations */
s32 security_cap_set(security_capabilities_t *caps, u64 cap);
s32 security_cap_clear(security_capabilities_t *caps, u64 cap);
bool security_cap_test(security_capabilities_t *caps, u64 cap);
void security_cap_zero(security_capabilities_t *caps);
void security_cap_full(security_capabilities_t *caps);

/* Process Capabilities */
s32 security_set_caps(security_context_t *ctx, security_capabilities_t *caps);
s32 security_get_caps(security_context_t *ctx, security_capabilities_t *caps);
bool security_capable(security_context_t *ctx, u64 cap);
bool security_capable_current(u64 cap);

/* Capability Helpers */
bool security_capable_admin(void);
bool security_capable_sys(void);
bool security_capable_net(void);

/*===========================================================================*/
/*                         ACCESS CONTROL API                                */
/*===========================================================================*/

/* ACL Operations */
s32 security_acl_init(security_acl_t *acl, u32 max_entries);
void security_acl_destroy(security_acl_t *acl);
s32 security_acl_add_entry(security_acl_t *acl, u32 type, u32 id, u8 perms);
s32 security_acl_remove_entry(security_acl_t *acl, u32 type, u32 id);
s32 security_acl_modify_entry(security_acl_t *acl, u32 type, u32 id, u8 perms);

/* ACL Queries */
bool security_acl_check(security_acl_t *acl, u32 uid, u32 gid, u8 perms);
u8 security_acl_get_perms(security_acl_t *acl, u32 uid, u32 gid);
s32 security_acl_set_owner(security_acl_t *acl, u32 uid, u32 gid);

/* Access Control */
s32 security_check_access(security_context_t *ctx, security_acl_t *acl, u8 perms);
s32 security_validate_access(u32 uid, u32 gid, u32 target_uid, u32 target_gid, u8 perms);

/*===========================================================================*/
/*                         SECURITY LABELING                                 */
/*===========================================================================*/

/* Label Operations */
s32 security_label_create(const char *name, u32 level);
s32 security_label_destroy(const char *name);
s32 security_label_assign(security_context_t *ctx, const char *label);
const char *security_label_get(security_context_t *ctx);

/* Label Comparison */
s32 security_label_compare(const char *label1, const char *label2);
bool security_label_dominates(const char *high, const char *low);

/*===========================================================================*/
/*                         SECURITY POLICIES                                 */
/*===========================================================================*/

/* Policy Management */
s32 security_policy_register(security_policy_t *policy);
s32 security_policy_unregister(u32 policy_id);
security_policy_t *security_policy_get(u32 policy_id);
s32 security_policy_enable(u32 policy_id);
s32 security_policy_disable(u32 policy_id);

/* Policy Evaluation */
bool security_policy_evaluate(security_context_t *ctx, u32 action);
s32 security_policy_check(security_context_t *ctx, const char *resource, u32 access);

/*===========================================================================*/
/*                         SECURITY AUDITING                                 */
/*===========================================================================*/

/* Audit Log Operations */
s32 security_audit_init(security_audit_log_t *log, u32 max_events);
void security_audit_shutdown(security_audit_log_t *log);
s32 security_audit_log(security_audit_log_t *log, security_event_t *event);
s32 security_audit_read(security_audit_log_t *log, security_event_t *event);

/* Event Logging */
void security_log_event(u32 type, u32 severity, const char *fmt, ...);
void security_log_auth(u32 uid, bool success, const char *reason);
void security_log_access(u32 uid, u32 resource, bool granted);
void security_log_violation(u32 uid, const char *reason);

/* Audit Queries */
u32 security_audit_count(security_audit_log_t *log);
u64 security_audit_total(security_audit_log_t *log);
u64 security_audit_dropped(security_audit_log_t *log);

/*===========================================================================*/
/*                         SECURITY UTILITIES                                */
/*===========================================================================*/

/* Security Checks */
bool security_is_root(u32 uid);
bool security_is_privileged(security_context_t *ctx);
bool security_is_sandboxed(security_context_t *ctx);

/* UID/GID Operations */
bool security_uid_valid(u32 uid);
bool security_gid_valid(u32 gid);
bool security_uid_equal(u32 uid1, u32 uid2);

/* Permission Helpers */
u8 security_perm_from_mode(u32 mode);
u32 security_mode_from_perm(u8 perm, bool is_dir);

/* Statistics */
void security_stats(void);
void security_dump_contexts(void);
void security_dump_policies(void);

/*===========================================================================*/
/*                         EXTERNAL DECLARATIONS                             */
/*===========================================================================*/

/* Global security manager instance */
extern security_manager_t g_security_manager;

/* Security subsystem initialization */
extern s32 security_crypto_init(void);
extern s32 security_auth_init(void);
extern s32 security_sandbox_init(void);
extern s32 security_tpm_init(void);

/* Crypto API (from crypto subsystem) */
extern s32 crypto_init(void);
extern void *crypto_alloc_context(void);
extern void crypto_free_context(void *ctx);

/* Auth API (from auth subsystem) */
extern s32 auth_init(void);
extern s32 auth_user_verify(u32 uid, const char *password);
extern s32 auth_session_create(u32 uid);

/* Sandbox API (from sandbox subsystem) */
extern s32 sandbox_init(void);
extern s32 sandbox_create(u32 pid, u32 flags);
extern s32 sandbox_destroy(u32 pid);

/* TPM API (from TPM subsystem) */
extern s32 tpm_init(void);
extern s32 tpm_probe(void);
extern void tpm_shutdown(void);

#endif /* _NEXUS_SECURITY_H */
