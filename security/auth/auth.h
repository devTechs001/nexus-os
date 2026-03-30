/*
 * NEXUS OS - Security Framework
 * security/auth/auth.h
 *
 * Authentication Subsystem Header
 * User authentication, session management, and credential handling
 */

#ifndef _NEXUS_AUTH_H
#define _NEXUS_AUTH_H

#include "../../kernel/include/kernel.h"
#include "../../kernel/sync/sync.h"

/*===========================================================================*/
/*                         AUTH CONFIGURATION                                */
/*===========================================================================*/

#define AUTH_MAX_USERS          65536
#define AUTH_MAX_SESSIONS       4096
#define AUTH_SESSION_TIMEOUT    (30 * 60 * 1000)  /* 30 minutes */
#define AUTH_MAX_LOGIN_ATTEMPTS 5
#define AUTH_LOCKOUT_TIME       (5 * 60 * 1000)   /* 5 minutes */
#define AUTH_PASSWORD_MIN_LEN   8
#define AUTH_PASSWORD_MAX_LEN   128
#define AUTH_SALT_SIZE          16
#define AUTH_HASH_SIZE          32                /* SHA256_DIGEST_SIZE */
#define AUTH_TOKEN_SIZE         32

/* Authentication States */
#define AUTH_STATE_NONE         0
#define AUTH_STATE_PENDING      1
#define AUTH_STATE_AUTHENTICATED 2
#define AUTH_STATE_LOCKED       3
#define AUTH_STATE_DISABLED     4

/* User Flags */
#define USER_FLAG_NONE          0x00000000
#define USER_FLAG_ADMIN         0x00000001
#define USER_FLAG_SYSTEM        0x00000002
#define USER_FLAG_LOCKED        0x00000004
#define USER_FLAG_DISABLED      0x00000008
#define USER_FLAG_CHANGE_PASS   0x00000010
#define USER_FLAG_EXPIRED       0x00000020

/* Session Flags */
#define SESSION_FLAG_NONE       0x00000000
#define SESSION_FLAG_ACTIVE     0x00000001
#define SESSION_FLAG_ROOT       0x00000002
#define SESSION_FLAG_REMOTE     0x00000004
#define SESSION_FLAG_TTY        0x00000008

/* Magic Numbers */
#define AUTH_USER_MAGIC         0x4155544855535201ULL  /* AUTHUSR1 */
#define AUTH_SESSION_MAGIC      0x4155544853455301ULL  /* AUTHSES1 */

/*===========================================================================*/
/*                         AUTH DATA STRUCTURES                              */
/*===========================================================================*/

/**
 * auth_password_t - Stored password information
 */
typedef struct {
    u8 hash[AUTH_HASH_SIZE];      /* Password hash */
    u8 salt[AUTH_SALT_SIZE];      /* Password salt */
    u32 iterations;               /* KDF iterations */
    u64 last_changed;             /* Last password change */
    u64 expires;                  /* Password expiration */
} auth_password_t;

/**
 * auth_user_t - User account information
 */
typedef struct {
    u64 magic;                    /* Magic number */
    spinlock_t lock;              /* Protection lock */
    u32 uid;                      /* User ID */
    u32 gid;                      /* Primary group ID */
    u32 flags;                    /* User flags */
    u32 state;                    /* Authentication state */
    char username[64];            /* Username */
    auth_password_t password;     /* Password info */
    u32 login_attempts;           /* Failed login attempts */
    u64 lockout_until;            /* Lockout expiration */
    u64 last_login;               /* Last successful login */
    u64 last_failed;              /* Last failed login */
    char last_host[64];           /* Last login host */
    u32 session_count;            /* Active sessions */
    u64 created;                  /* Account creation time */
    void *private;                /* Private data */
} auth_user_t;

/**
 * auth_session_t - User session information
 */
typedef struct {
    u64 magic;                    /* Magic number */
    spinlock_t lock;              /* Protection lock */
    u32 sid;                      /* Session ID */
    u32 uid;                      /* User ID */
    u32 flags;                    /* Session flags */
    u8 token[AUTH_TOKEN_SIZE];    /* Session token */
    u64 created;                  /* Session creation time */
    u64 last_activity;            /* Last activity time */
    u64 expires;                  /* Session expiration */
    u32 pid;                      /* Associated process */
    char tty[32];                 /* Terminal device */
    char host[64];                /* Remote host */
    u32 login_count;              /* Commands executed */
    void *private;                /* Private data */
} auth_session_t;

/**
 * auth_context_t - Authentication context (used by GUI)
 */
typedef struct {
    auth_session_t *session;
    u32 auth_method;
    u32 flags;
} auth_context_t;

/*===========================================================================*/
/*                         AUTH API                                          */
/*===========================================================================*/

s32 auth_init(void);
void auth_shutdown(void);

s32 auth_user_verify(u32 uid, const char *password);
s32 auth_user_login(const char *username, const char *password, auth_session_t **session);
s32 auth_user_logout(auth_session_t *session);

s32 auth_session_create(u32 uid);
s32 auth_session_destroy(u32 sid);
void auth_destroy_user_sessions(u32 uid);
s32 auth_session_validate(auth_session_t *sess);
auth_session_t *auth_get_session(u32 sid);
auth_session_t *auth_get_session_by_token(const u8 *token, u32 length);

auth_user_t *auth_get_user(u32 uid);
auth_user_t *auth_get_user_by_name(const char *username);
s32 auth_user_update(auth_user_t *user);

/* Compatibility helper for GUI */
static inline int auth_login(auth_context_t *ctx, const char *username, const char *password) {
    if (!ctx) return -1;
    return auth_user_login(username, password, &ctx->session);
}

/* PAM-style API helpers */
s32 auth_pam_authenticate(const char *service, const char *username, const char *password);
s32 auth_pam_setcred(const char *service, const char *username, u32 flags);
s32 auth_pam_chauthtok(const char *service, const char *username, const char *oldpass, const char *newpass);

void auth_stats(void);
void auth_dump_users(void);
void auth_dump_sessions(void);

#endif /* _NEXUS_AUTH_H */
