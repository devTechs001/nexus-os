/*
 * NEXUS OS - Security Framework
 * security/auth/auth.c
 *
 * Authentication Subsystem
 * User authentication, session management, and credential handling
 */

#include "../security.h"

/*===========================================================================*/
/*                         AUTH CONFIGURATION                                */
/*===========================================================================*/

#define AUTH_DEBUG              0
#define AUTH_MAX_USERS          65536
#define AUTH_MAX_SESSIONS       4096
#define AUTH_SESSION_TIMEOUT    (30 * 60 * 1000)  /* 30 minutes */
#define AUTH_MAX_LOGIN_ATTEMPTS 5
#define AUTH_LOCKOUT_TIME       (5 * 60 * 1000)   /* 5 minutes */
#define AUTH_PASSWORD_MIN_LEN   8
#define AUTH_PASSWORD_MAX_LEN   128
#define AUTH_SALT_SIZE          16
#define AUTH_HASH_SIZE          SHA256_DIGEST_SIZE
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
 * auth_manager_t - Authentication manager state
 */
typedef struct {
    spinlock_t lock;              /* Manager lock */
    u32 state;                    /* Manager state */
    auth_user_t **users;          /* User table */
    u32 user_count;               /* Active users */
    auth_session_t **sessions;    /* Session table */
    u32 session_count;            /* Active sessions */
    u32 next_uid;                 /* Next user ID */
    u32 next_sid;                 /* Next session ID */
    u64 total_logins;             /* Total successful logins */
    u64 total_failures;           /* Total failed logins */
    u64 total_sessions;           /* Total sessions created */
} auth_manager_t;

/*===========================================================================*/
/*                         AUTH GLOBAL DATA                                  */
/*===========================================================================*/

static auth_manager_t g_auth_manager = {
    .lock = {
        .lock = SPINLOCK_UNLOCKED,
        .magic = SPINLOCK_MAGIC,
        .name = "auth_manager",
    },
    .state = AUTH_STATE_NONE,
    .users = NULL,
    .user_count = 0,
    .sessions = NULL,
    .session_count = 0,
    .next_uid = 1000,
    .next_sid = 1,
};

/* Pre-allocated user table */
static auth_user_t *auth_user_table[AUTH_MAX_USERS];

/* Pre-allocated session table */
static auth_session_t *auth_session_table[AUTH_MAX_SESSIONS];

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static s32 auth_hash_password(const char *password, const u8 *salt,
                              u32 iterations, u8 *hash);
static s32 auth_verify_password(const auth_password_t *stored, const char *password);
static s32 auth_generate_token(u8 *token, u32 length);
static void auth_cleanup_expired_sessions(void);

/*===========================================================================*/
/*                         AUTH INITIALIZATION                               */
/*===========================================================================*/

/**
 * auth_init - Initialize authentication subsystem
 *
 * Initializes the authentication subsystem, including user and
 * session tables.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_init(void)
{
    u32 i;

    pr_info("Auth: Initializing authentication subsystem...\n");

    /* Initialize manager */
    spin_lock_init_named(&g_auth_manager.lock, "auth_manager");
    g_auth_manager.state = AUTH_STATE_PENDING;

    /* Initialize user table */
    for (i = 0; i < AUTH_MAX_USERS; i++) {
        auth_user_table[i] = NULL;
    }

    /* Initialize session table */
    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        auth_session_table[i] = NULL;
    }

    g_auth_manager.users = auth_user_table;
    g_auth_manager.sessions = auth_session_table;

    /* Create root/admin user (UID 0) */
    auth_user_t *root_user = kzalloc(sizeof(auth_user_t));
    if (root_user) {
        root_user->magic = AUTH_USER_MAGIC;
        spin_lock_init_named(&root_user->lock, "root_user");
        root_user->uid = 0;
        root_user->gid = 0;
        root_user->flags = USER_FLAG_ADMIN | USER_FLAG_SYSTEM;
        root_user->state = AUTH_STATE_AUTHENTICATED;
        strncpy(root_user->username, "root", sizeof(root_user->username) - 1);
        root_user->created = get_time_ms();

        auth_user_table[0] = root_user;
        g_auth_manager.user_count = 1;

        pr_info("Auth: Root user created\n");
    }

    g_auth_manager.state = AUTH_STATE_AUTHENTICATED;

    pr_info("Auth: Subsystem initialized\n");
    pr_info("  Max users: %u\n", AUTH_MAX_USERS);
    pr_info("  Max sessions: %u\n", AUTH_MAX_SESSIONS);
    pr_info("  Session timeout: %u ms\n", AUTH_SESSION_TIMEOUT);

    return OK;
}

/**
 * auth_shutdown - Shutdown authentication subsystem
 *
 * Cleans up all authentication resources.
 */
void auth_shutdown(void)
{
    u32 i;

    pr_info("Auth: Shutting down...\n");

    spin_lock(&g_auth_manager.lock);

    /* Free all users */
    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i]) {
            crypto_memzero(auth_user_table[i], sizeof(auth_user_t));
            kfree(auth_user_table[i]);
            auth_user_table[i] = NULL;
        }
    }

    /* Free all sessions */
    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (auth_session_table[i]) {
            crypto_memzero(auth_session_table[i], sizeof(auth_session_t));
            kfree(auth_session_table[i]);
            auth_session_table[i] = NULL;
        }
    }

    g_auth_manager.user_count = 0;
    g_auth_manager.session_count = 0;
    g_auth_manager.state = AUTH_STATE_NONE;

    spin_unlock(&g_auth_manager.lock);

    pr_info("Auth: Shutdown complete\n");
}

/*===========================================================================*/
/*                         PASSWORD HANDLING                                 */
/*===========================================================================*/

/**
 * auth_hash_password - Hash a password with salt
 * @password: Plain text password
 * @salt: Salt value
 * @iterations: PBKDF2 iterations
 * @hash: Output hash
 *
 * Uses PBKDF2-HMAC-SHA256 for password hashing.
 *
 * Returns: OK on success, error code on failure
 */
static s32 auth_hash_password(const char *password, const u8 *salt,
                              u32 iterations, u8 *hash)
{
    if (!password || !salt || !hash) {
        return EINVAL;
    }

    return pbkdf2_sha256((const u8 *)password, strlen(password),
                         salt, AUTH_SALT_SIZE,
                         iterations, hash, AUTH_HASH_SIZE);
}

/**
 * auth_verify_password - Verify a password against stored hash
 * @stored: Stored password information
 * @password: Password to verify
 *
 * Returns: OK if password matches, error code otherwise
 */
static s32 auth_verify_password(const auth_password_t *stored, const char *password)
{
    u8 hash[AUTH_HASH_SIZE];
    s32 ret;

    if (!stored || !password) {
        return EINVAL;
    }

    ret = auth_hash_password(password, stored->salt, stored->iterations, hash);
    if (ret != OK) {
        return ret;
    }

    /* Constant-time comparison */
    if (crypto_memcmp(hash, stored->hash, AUTH_HASH_SIZE) == 0) {
        crypto_memzero(hash, sizeof(hash));
        return OK;
    }

    crypto_memzero(hash, sizeof(hash));
    return EPERM;
}

/**
 * auth_generate_salt - Generate random salt
 * @salt: Output salt buffer
 *
 * Generates a cryptographically secure random salt.
 */
static void auth_generate_salt(u8 *salt)
{
    crypto_random_bytes(salt, AUTH_SALT_SIZE);
}

/**
 * auth_set_password - Set user password
 * @user: User account
 * @password: New password
 *
 * Sets a new password for a user account.
 *
 * Returns: OK on success, error code on failure
 */
static s32 auth_set_password(auth_user_t *user, const char *password)
{
    u32 password_len;

    if (!user || user->magic != AUTH_USER_MAGIC) {
        return EINVAL;
    }

    if (!password) {
        return EINVAL;
    }

    password_len = strlen(password);

    /* Validate password length */
    if (password_len < AUTH_PASSWORD_MIN_LEN) {
        return EINVAL;
    }

    if (password_len > AUTH_PASSWORD_MAX_LEN) {
        return EINVAL;
    }

    spin_lock(&user->lock);

    /* Generate new salt */
    auth_generate_salt(user->password.salt);

    /* Set iteration count */
    user->password.iterations = 100000;

    /* Hash password */
    auth_hash_password(password, user->password.salt,
                       user->password.iterations,
                       user->password.hash);

    user->password.last_changed = get_time_ms();
    user->password.expires = 0;  /* No expiration */

    spin_unlock(&user->lock);

    pr_debug("Auth: Password set for user %s\n", user->username);

    return OK;
}

/*===========================================================================*/
/*                         USER MANAGEMENT                                   */
/*===========================================================================*/

/**
 * auth_create_user - Create a new user account
 * @username: Username
 * @password: Initial password
 * @uid: User ID (0 for auto-assign)
 * @gid: Primary group ID
 *
 * Creates a new user account with the specified credentials.
 *
 * Returns: Pointer to new user, or NULL on failure
 */
auth_user_t *auth_create_user(const char *username, const char *password,
                              u32 uid, u32 gid)
{
    auth_user_t *user;
    u32 i;

    if (!username || strlen(username) == 0) {
        return NULL;
    }

    if (uid == 0) {
        /* Auto-assign UID */
        uid = g_auth_manager.next_uid++;
    }

    /* Check if UID already exists */
    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i] && auth_user_table[i]->uid == uid) {
            return NULL;
        }
    }

    /* Find empty slot */
    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i] == NULL) {
            break;
        }
    }

    if (i >= AUTH_MAX_USERS) {
        return NULL;  /* No space */
    }

    /* Allocate user structure */
    user = kzalloc(sizeof(auth_user_t));
    if (!user) {
        return NULL;
    }

    /* Initialize user */
    user->magic = AUTH_USER_MAGIC;
    spin_lock_init_named(&user->lock, "auth_user");
    user->uid = uid;
    user->gid = gid;
    user->flags = USER_FLAG_NONE;
    user->state = AUTH_STATE_PENDING;
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->created = get_time_ms();

    /* Set password if provided */
    if (password) {
        auth_set_password(user, password);
    }

    /* Store in table */
    auth_user_table[i] = user;
    g_auth_manager.user_count++;

    pr_info("Auth: Created user '%s' (UID %u)\n", username, uid);

    return user;
}

/**
 * auth_delete_user - Delete a user account
 * @user: User to delete
 *
 * Removes a user account and terminates all sessions.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_delete_user(auth_user_t *user)
{
    u32 i;

    if (!user || user->magic != AUTH_USER_MAGIC) {
        return EINVAL;
    }

    /* Cannot delete system users */
    if (user->flags & USER_FLAG_SYSTEM) {
        return EPERM;
    }

    spin_lock(&g_auth_manager.lock);
    spin_lock(&user->lock);

    /* Find and remove from table */
    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i] == user) {
            auth_user_table[i] = NULL;
            g_auth_manager.user_count--;
            break;
        }
    }

    user->magic = 0;

    spin_unlock(&user->lock);
    spin_unlock(&g_auth_manager.lock);

    /* Securely free */
    crypto_memzero(user, sizeof(auth_user_t));
    kfree(user);

    pr_info("Auth: Deleted user (UID %u)\n", user->uid);

    return OK;
}

/**
 * auth_get_user - Get user by UID
 * @uid: User ID
 *
 * Returns: Pointer to user, or NULL if not found
 */
auth_user_t *auth_get_user(u32 uid)
{
    u32 i;
    auth_user_t *user = NULL;

    spin_lock(&g_auth_manager.lock);

    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i] && auth_user_table[i]->uid == uid) {
            user = auth_user_table[i];
            break;
        }
    }

    spin_unlock(&g_auth_manager.lock);

    return user;
}

/**
 * auth_get_user_by_name - Get user by username
 * @username: Username
 *
 * Returns: Pointer to user, or NULL if not found
 */
auth_user_t *auth_get_user_by_name(const char *username)
{
    u32 i;
    auth_user_t *user = NULL;

    if (!username) {
        return NULL;
    }

    spin_lock(&g_auth_manager.lock);

    for (i = 0; i < AUTH_MAX_USERS; i++) {
        if (auth_user_table[i] &&
            strcmp(auth_user_table[i]->username, username) == 0) {
            user = auth_user_table[i];
            break;
        }
    }

    spin_unlock(&g_auth_manager.lock);

    return user;
}

/*===========================================================================*/
/*                         AUTHENTICATION                                    */
/*===========================================================================*/

/**
 * auth_user_verify - Verify user credentials
 * @uid: User ID
 * @password: Password to verify
 *
 * Authenticates a user with their password.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_user_verify(u32 uid, const char *password)
{
    auth_user_t *user;
    s32 ret;
    u64 now;

    if (!password) {
        return EINVAL;
    }

    user = auth_get_user(uid);
    if (!user) {
        g_auth_manager.total_failures++;
        return ENOENT;
    }

    spin_lock(&user->lock);

    now = get_time_ms();

    /* Check account state */
    if (user->flags & USER_FLAG_DISABLED) {
        spin_unlock(&user->lock);
        g_auth_manager.total_failures++;
        return EACCES;
    }

    if (user->flags & USER_FLAG_LOCKED) {
        if (now < user->lockout_until) {
            spin_unlock(&user->lock);
            g_auth_manager.total_failures++;
            return EACCES;
        }
        /* Lockout expired, reset */
        user->flags &= ~USER_FLAG_LOCKED;
        user->login_attempts = 0;
    }

    /* Verify password */
    ret = auth_verify_password(&user->password, password);

    if (ret == OK) {
        /* Successful login */
        user->state = AUTH_STATE_AUTHENTICATED;
        user->last_login = now;
        user->login_attempts = 0;
        user->flags &= ~USER_FLAG_LOCKED;
        g_auth_manager.total_logins++;

        strncpy(user->last_host, "local", sizeof(user->last_host) - 1);

        pr_debug("Auth: User '%s' authenticated\n", user->username);
    } else {
        /* Failed login */
        user->last_failed = now;
        user->login_attempts++;
        g_auth_manager.total_failures++;

        strncpy(user->last_host, "local", sizeof(user->last_host) - 1);

        /* Check for lockout */
        if (user->login_attempts >= AUTH_MAX_LOGIN_ATTEMPTS) {
            user->flags |= USER_FLAG_LOCKED;
            user->lockout_until = now + AUTH_LOCKOUT_TIME;
            pr_warn("Auth: User '%s' locked out\n", user->username);
        }

        pr_debug("Auth: Failed login for user '%s' (attempt %u)\n",
                 user->username, user->login_attempts);
    }

    spin_unlock(&user->lock);

    return ret;
}

/**
 * auth_user_login - User login
 * @username: Username
 * @password: Password
 * @session: Output session pointer
 *
 * Performs user login and creates a session.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_user_login(const char *username, const char *password,
                    auth_session_t **session)
{
    auth_user_t *user;
    auth_session_t *sess;
    s32 ret;

    if (!username || !password || !session) {
        return EINVAL;
    }

    *session = NULL;

    user = auth_get_user_by_name(username);
    if (!user) {
        return ENOENT;
    }

    ret = auth_user_verify(user->uid, password);
    if (ret != OK) {
        return ret;
    }

    /* Create session */
    ret = auth_session_create(user->uid);
    if (ret < 0) {
        return ret;
    }

    sess = auth_get_session(ret);
    if (!sess) {
        return ENOMEM;
    }

    *session = sess;

    security_log_auth(user->uid, true, "login successful");

    return OK;
}

/**
 * auth_user_logout - User logout
 * @session: Session to terminate
 *
 * Terminates a user session.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_user_logout(auth_session_t *session)
{
    if (!session || session->magic != AUTH_SESSION_MAGIC) {
        return EINVAL;
    }

    return auth_session_destroy(session->sid);
}

/*===========================================================================*/
/*                         SESSION MANAGEMENT                                */
/*===========================================================================*/

/**
 * auth_generate_token - Generate session token
 * @token: Output token buffer
 * @length: Token length
 *
 * Generates a cryptographically secure session token.
 *
 * Returns: OK on success, error code on failure
 */
static s32 auth_generate_token(u8 *token, u32 length)
{
    if (!token || length == 0) {
        return EINVAL;
    }

    return crypto_random_bytes(token, length);
}

/**
 * auth_cleanup_expired_sessions - Clean up expired sessions
 *
 * Removes sessions that have exceeded their timeout.
 */
static void auth_cleanup_expired_sessions(void)
{
    u32 i;
    u64 now = get_time_ms();

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        auth_session_t *sess = auth_session_table[i];
        if (sess && sess->magic == AUTH_SESSION_MAGIC) {
            if (now > sess->expires ||
                (now - sess->last_activity) > AUTH_SESSION_TIMEOUT) {
                auth_session_destroy(sess->sid);
            }
        }
    }
}

/**
 * auth_session_create - Create a new session
 * @uid: User ID
 *
 * Creates a new authenticated session for a user.
 *
 * Returns: Session ID on success, error code on failure
 */
s32 auth_session_create(u32 uid)
{
    auth_session_t *sess;
    auth_user_t *user;
    u32 i;

    user = auth_get_user(uid);
    if (!user) {
        return ENOENT;
    }

    /* Find empty slot */
    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (auth_session_table[i] == NULL) {
            break;
        }
    }

    if (i >= AUTH_MAX_SESSIONS) {
        /* Clean up expired sessions and retry */
        auth_cleanup_expired_sessions();

        for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
            if (auth_session_table[i] == NULL) {
                break;
            }
        }

        if (i >= AUTH_MAX_SESSIONS) {
            return ENOSPC;
        }
    }

    /* Allocate session */
    sess = kzalloc(sizeof(auth_session_t));
    if (!sess) {
        return ENOMEM;
    }

    /* Initialize session */
    sess->magic = AUTH_SESSION_MAGIC;
    spin_lock_init_named(&sess->lock, "auth_session");
    sess->sid = g_auth_manager.next_sid++;
    sess->uid = uid;
    sess->flags = SESSION_FLAG_ACTIVE;
    sess->created = get_time_ms();
    sess->last_activity = sess->created;
    sess->expires = sess->created + AUTH_SESSION_TIMEOUT;

    /* Generate session token */
    auth_generate_token(sess->token, AUTH_TOKEN_SIZE);

    /* Store in table */
    auth_session_table[i] = sess;
    g_auth_manager.session_count++;
    g_auth_manager.total_sessions++;

    /* Update user session count */
    spin_lock(&user->lock);
    user->session_count++;
    spin_unlock(&user->lock);

    pr_debug("Auth: Created session %u for user %u\n", sess->sid, uid);

    return sess->sid;
}

/**
 * auth_session_destroy - Destroy a session
 * @sid: Session ID
 *
 * Terminates and removes a session.
 *
 * Returns: OK on success, error code on failure
 */
s32 auth_session_destroy(u32 sid)
{
    auth_session_t *sess;
    auth_user_t *user;
    u32 i;

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (auth_session_table[i] && auth_session_table[i]->sid == sid) {
            sess = auth_session_table[i];
            break;
        }
    }

    if (i >= AUTH_MAX_SESSIONS) {
        return ENOENT;
    }

    spin_lock(&sess->lock);

    /* Get user to update session count */
    user = auth_get_user(sess->uid);

    auth_session_table[i] = NULL;
    g_auth_manager.session_count--;

    sess->magic = 0;
    sess->flags &= ~SESSION_FLAG_ACTIVE;

    spin_unlock(&sess->lock);

    /* Update user */
    if (user) {
        spin_lock(&user->lock);
        if (user->session_count > 0) {
            user->session_count--;
        }
        spin_unlock(&user->lock);
    }

    /* Securely free */
    crypto_memzero(sess, sizeof(auth_session_t));
    kfree(sess);

    pr_debug("Auth: Destroyed session %u\n", sid);

    return OK;
}

/**
 * auth_get_session - Get session by ID
 * @sid: Session ID
 *
 * Returns: Pointer to session, or NULL if not found
 */
auth_session_t *auth_get_session(u32 sid)
{
    u32 i;
    auth_session_t *sess = NULL;

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (auth_session_table[i] && auth_session_table[i]->sid == sid) {
            sess = auth_session_table[i];
            break;
        }
    }

    return sess;
}

/**
 * auth_get_session_by_token - Get session by token
 * @token: Session token
 * @length: Token length
 *
 * Returns: Pointer to session, or NULL if not found
 */
auth_session_t *auth_get_session_by_token(const u8 *token, u32 length)
{
    u32 i;
    auth_session_t *sess = NULL;

    if (!token || length != AUTH_TOKEN_SIZE) {
        return NULL;
    }

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        if (auth_session_table[i] &&
            auth_session_table[i]->magic == AUTH_SESSION_MAGIC &&
            (auth_session_table[i]->flags & SESSION_FLAG_ACTIVE)) {
            if (memcmp(auth_session_table[i]->token, token, length) == 0) {
                sess = auth_session_table[i];
                /* Update last activity */
                sess->last_activity = get_time_ms();
                break;
            }
        }
    }

    return sess;
}

/**
 * auth_session_validate - Validate a session
 * @sess: Session to validate
 *
 * Checks if a session is still valid.
 *
 * Returns: OK if valid, error code otherwise
 */
s32 auth_session_validate(auth_session_t *sess)
{
    u64 now;

    if (!sess || sess->magic != AUTH_SESSION_MAGIC) {
        return EINVAL;
    }

    if (!(sess->flags & SESSION_FLAG_ACTIVE)) {
        return EPERM;
    }

    now = get_time_ms();

    if (now > sess->expires) {
        return ETIMEDOUT;
    }

    if ((now - sess->last_activity) > AUTH_SESSION_TIMEOUT) {
        return ETIMEDOUT;
    }

    /* Update activity */
    sess->last_activity = now;

    return OK;
}

/*===========================================================================*/
/*                         PAM INTEGRATION                                   */
/*===========================================================================*/

/**
 * pam_authenticate - PAM authentication
 * @service: PAM service name
 * @username: Username
 * @password: Password
 *
 * PAM-style authentication interface.
 *
 * Returns: OK on success, error code on failure
 */
s32 pam_authenticate(const char *service, const char *username, const char *password)
{
    auth_user_t *user;
    s32 ret;

    if (!service || !username || !password) {
        return EINVAL;
    }

    pr_debug("PAM: Authenticating user '%s' for service '%s'\n", username, service);

    user = auth_get_user_by_name(username);
    if (!user) {
        return ENOENT;
    }

    ret = auth_user_verify(user->uid, password);

    if (ret == OK) {
        security_log_auth(user->uid, true, "PAM authentication successful");
    } else {
        security_log_auth(user->uid, false, "PAM authentication failed");
    }

    return ret;
}

/**
 * pam_setcred - PAM set credentials
 * @service: PAM service name
 * @username: Username
 * @flags: Credential flags
 *
 * PAM-style credential management.
 *
 * Returns: OK on success, error code on failure
 */
s32 pam_setcred(const char *service, const char *username, u32 flags)
{
    auth_user_t *user;

    if (!service || !username) {
        return EINVAL;
    }

    user = auth_get_user_by_name(username);
    if (!user) {
        return ENOENT;
    }

    /* Handle credential flags */
    if (flags & 0x01) {  /* PAM_ESTABLISH_CRED */
        user->state = AUTH_STATE_AUTHENTICATED;
    }

    if (flags & 0x02) {  /* PAM_DELETE_CRED */
        user->state = AUTH_STATE_NONE;
    }

    if (flags & 0x04) {  /* PAM_REINITIALIZE_CRED */
        user->state = AUTH_STATE_AUTHENTICATED;
    }

    if (flags & 0x08) {  /* PAM_REFRESH_CRED */
        /* Refresh credentials */
    }

    return OK;
}

/**
 * pam_chauthtok - PAM change authentication token
 * @service: PAM service name
 * @username: Username
 * @old_password: Old password
 * @new_password: New password
 *
 * PAM-style password change.
 *
 * Returns: OK on success, error code on failure
 */
s32 pam_chauthtok(const char *service, const char *username,
                  const char *old_password, const char *new_password)
{
    auth_user_t *user;
    s32 ret;

    if (!service || !username || !old_password || !new_password) {
        return EINVAL;
    }

    user = auth_get_user_by_name(username);
    if (!user) {
        return ENOENT;
    }

    /* Verify old password */
    ret = auth_verify_password(&user->password, old_password);
    if (ret != OK) {
        return ret;
    }

    /* Set new password */
    return auth_set_password(user, new_password);
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * auth_stats - Print authentication statistics
 */
void auth_stats(void)
{
    printk("\n=== Authentication Statistics ===\n");
    printk("State: %u\n", g_auth_manager.state);
    printk("Active Users: %u\n", g_auth_manager.user_count);
    printk("Active Sessions: %u\n", g_auth_manager.session_count);
    printk("Total Logins: %llu\n", (unsigned long long)g_auth_manager.total_logins);
    printk("Total Failures: %llu\n", (unsigned long long)g_auth_manager.total_failures);
    printk("Total Sessions: %llu\n", (unsigned long long)g_auth_manager.total_sessions);
}

/**
 * auth_dump_users - Dump all users
 */
void auth_dump_users(void)
{
    u32 i;
    auth_user_t *user;

    printk("\n=== User Accounts ===\n");

    for (i = 0; i < AUTH_MAX_USERS; i++) {
        user = auth_user_table[i];
        if (user && user->magic == AUTH_USER_MAGIC) {
            printk("  [%u] UID=%u GID=%u Name=%s Flags=0x%x State=%u\n",
                   i, user->uid, user->gid, user->username,
                   user->flags, user->state);
        }
    }
}

/**
 * auth_dump_sessions - Dump all sessions
 */
void auth_dump_sessions(void)
{
    u32 i;
    auth_session_t *sess;

    printk("\n=== Active Sessions ===\n");

    for (i = 0; i < AUTH_MAX_SESSIONS; i++) {
        sess = auth_session_table[i];
        if (sess && sess->magic == AUTH_SESSION_MAGIC) {
            printk("  [%u] SID=%u UID=%u Flags=0x%x Active=%s\n",
                   i, sess->sid, sess->uid, sess->flags,
                   (sess->flags & SESSION_FLAG_ACTIVE) ? "yes" : "no");
        }
    }
}
