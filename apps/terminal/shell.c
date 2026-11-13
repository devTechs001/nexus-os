/*
 * NEXUS OS - Shell Core
 * apps/terminal/shell.c
 *
 * A real command interpreter for NEXUS.
 *
 * Before this file existed there was no shell in the tree at all: the
 * terminal window had a tab bar, a scrollback buffer and a mock PTY whose
 * pty_spawn() just stored "1000 + window_id" as a PID. Nothing could
 * interpret a command line, and nothing was wired to the VFS.
 *
 * This implements:
 *   - line editing with history (backed by terminal_features.c)
 *   - alias expansion
 *   - tokenisation with quoting and escaping
 *   - pipes (via the kernel's real pipe implementation)
 *   - output redirection (>, >>)
 *   - && / || / ; sequencing with real exit status
 *   - a builtin set backed by the VFS: cd, pwd, ls, cat, mkdir, rm, stat,
 *     echo, export, env, unset, history, alias, help, exit
 *   - external command lookup through the VFS + exec_process()
 *
 * Everything is synchronous and single-threaded: the shell runs on the
 * caller's stack and hands assembled output to a sink callback. That keeps
 * it usable from both the GUI terminal and a plain text console.
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "shell.h"
#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/math.h"
#include "../../fs/vfs/vfs.h"
#include "../../kernel/sched/sched.h"
#include "../../kernel/syscall/syscall.h"
#include "lang_runtime.h"

/* Provided by terminal_features.c */
int history_init(u32 max_entries);
int history_add(const char *command, u32 exit_code, const char *directory);
const char *history_prev(void);
const char *history_next(void);
int history_clear(void);
void history_print(u32 count);
int aliases_init(u32 max_aliases);
int alias_add(const char *name, const char *value, const char *description);
const char *alias_get(const char *name);
int alias_expand(char *command, u32 max_len);
void aliases_list(void);
int help_show(const char *command);
void emulations_list(void);

/*===========================================================================*/
/*                              LIMITS                                       */
/*===========================================================================*/

/* SHELL_MAX_LINE / SHELL_MAX_ARGS come from shell.h */
#define SHELL_MAX_ENV          64
#define SHELL_MAX_ENV_NAME     64
#define SHELL_MAX_ENV_VALUE    256
#define SHELL_MAX_STAGES       8
#define SHELL_PROMPT_MAX       512
#define SHELL_OUT_CHUNK        4096

/*===========================================================================*/
/*                            OUTPUT SINK                                    */
/*===========================================================================*/
/*
 * The shell never touches a framebuffer or UART directly. Output goes through
 * this sink so the same interpreter can drive the GUI terminal (into its cell
 * buffer) or a raw text console, and so redirection is a matter of swapping
 * the sink out.
 */

typedef void (*shell_write_fn)(void *ctx, const char *data, u32 len);

typedef struct {
    shell_write_fn write;
    void *ctx;
} shell_sink_t;

/*===========================================================================*/
/*                              STATE                                        */
/*===========================================================================*/

typedef struct {
    char name[SHELL_MAX_ENV_NAME];
    char value[SHELL_MAX_ENV_VALUE];
    bool exported;
} shell_var_t;

typedef struct {
    bool initialized;
    bool running;
    u32 exit_status;
    u32 last_status;                 /* status of the previous command */

    shell_var_t vars[SHELL_MAX_ENV];
    u32 var_count;

    shell_sink_t out;
    shell_sink_t err;

    char line[SHELL_MAX_LINE];

    /* Output capture for redirection / pipes */
    char *capture;
    u32 capture_len;
    u32 capture_cap;
    bool capturing;

    u32 pid;                         /* reported shell PID */
    u32 job_count;
} shell_state_t;

static shell_state_t g_shell;

/*===========================================================================*/
/*                             SMALL HELPERS                                 */
/*===========================================================================*/

static bool shell_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

static void shell_strlcpy(char *dst, const char *src, u32 size)
{
    u32 i = 0;

    if (!dst || size == 0) {
        return;
    }
    if (src) {
        while (src[i] && i + 1 < size) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void sink_puts(shell_sink_t *s, const char *text)
{
    if (s && s->write && text) {
        s->write(s->ctx, text, (u32)strlen(text));
    }
}

static void sink_write(shell_sink_t *s, const char *data, u32 len)
{
    if (s && s->write && data && len) {
        s->write(s->ctx, data, len);
    }
}

/*
 * shell_emit - Write command output through the active sink.
 *
 * While a stage is capturing (for a pipe or a redirect) the bytes are
 * accumulated in the buffer instead of being displayed.
 */
static void shell_emit(const char *data, u32 len)
{
    if (!data || !len) {
        return;
    }

    if (g_shell.capturing) {
        if (g_shell.capture_len + len + 1 > g_shell.capture_cap) {
            u32 want = g_shell.capture_cap ? g_shell.capture_cap * 2 : SHELL_OUT_CHUNK;
            while (want < g_shell.capture_len + len + 1) {
                want *= 2;
            }
            char *bigger = (char *)kmalloc(want);
            if (!bigger) {
                return;     /* Drop rather than corrupt. */
            }
            if (g_shell.capture && g_shell.capture_len) {
                memcpy(bigger, g_shell.capture, g_shell.capture_len);
            }
            if (g_shell.capture) {
                kfree(g_shell.capture);
            }
            g_shell.capture = bigger;
            g_shell.capture_cap = want;
        }
        memcpy(g_shell.capture + g_shell.capture_len, data, len);
        g_shell.capture_len += len;
        g_shell.capture[g_shell.capture_len] = '\0';
        return;
    }

    sink_write(&g_shell.out, data, len);
}

static void shell_puts(const char *text)
{
    shell_emit(text, (u32)strlen(text));
}

/** shell_printf - Minimal formatted output into the shell sink */
static void shell_printf(const char *fmt, ...)
{
    char buf[512];
    __builtin_va_list args;

    __builtin_va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    shell_puts(buf);
}

static void shell_error(const char *fmt, ...)
{
    char buf[512];
    __builtin_va_list args;

    __builtin_va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    if (g_shell.capturing) {
        /* Errors from a piped stage still belong to the pipeline's output. */
        shell_puts(buf);
    } else {
        sink_puts(&g_shell.err, buf);
    }
}

/*===========================================================================*/
/*                            ENVIRONMENT                                    */
/*===========================================================================*/

static shell_var_t *env_find(const char *name)
{
    u32 i;

    for (i = 0; i < g_shell.var_count; i++) {
        if (shell_streq(g_shell.vars[i].name, name)) {
            return &g_shell.vars[i];
        }
    }
    return NULL;
}

int shell_setenv(const char *name, const char *value, bool exported)
{
    shell_var_t *v;

    if (!name || !*name) {
        return -EINVAL;
    }

    v = env_find(name);
    if (!v) {
        if (g_shell.var_count >= SHELL_MAX_ENV) {
            return -ENOMEM;
        }
        v = &g_shell.vars[g_shell.var_count++];
        memset(v, 0, sizeof(*v));
        shell_strlcpy(v->name, name, sizeof(v->name));
    }

    shell_strlcpy(v->value, value ? value : "", sizeof(v->value));
    v->exported = exported;

    return 0;
}

const char *shell_getenv(const char *name)
{
    shell_var_t *v = env_find(name);
    return v ? v->value : NULL;
}

static int shell_unsetenv(const char *name)
{
    u32 i;

    for (i = 0; i < g_shell.var_count; i++) {
        if (shell_streq(g_shell.vars[i].name, name)) {
            g_shell.vars[i] = g_shell.vars[g_shell.var_count - 1];
            g_shell.var_count--;
            return 0;
        }
    }
    return -ENOENT;
}

/**
 * expand_vars - Expand $NAME and ${NAME} references in place.
 *
 * Returns a newly allocated string, or NULL on allocation failure.
 */
static char *expand_vars(const char *in)
{
    char *out;
    u32 oi = 0;
    u32 cap;

    if (!in) {
        return NULL;
    }

    cap = (u32)strlen(in) + 1;
    out = (char *)kmalloc(cap);
    if (!out) {
        return NULL;
    }
    out[0] = '\0';

    for (u32 i = 0; in[i]; ) {
        if (in[i] != '$') {
            if (oi + 2 > cap) {
                char *bigger = (char *)kmalloc(cap * 2);
                if (!bigger) {
                    kfree(out);
                    return NULL;
                }
                memcpy(bigger, out, oi);
                kfree(out);
                out = bigger;
                cap *= 2;
            }
            out[oi++] = in[i++];
            continue;
        }

        /* $NAME or ${NAME} */
        i++;
        bool braced = false;
        if (in[i] == '{') {
            braced = true;
            i++;
        }

        char name[SHELL_MAX_ENV_NAME];
        u32 ni = 0;
        while (in[i] && (braced ? in[i] != '}' : (in[i] != '/' && in[i] != ' ' && in[i] != '\t'))) {
            if (ni + 1 < sizeof(name)) {
                name[ni++] = in[i];
            }
            i++;
        }
        name[ni] = '\0';
        if (braced && in[i] == '}') {
            i++;
        }

        const char *val = name[0] ? shell_getenv(name) : NULL;
        if (!val) {
            val = "";           /* Unset variables expand to nothing. */
        }

        u32 vlen = (u32)strlen(val);
        if (oi + vlen + 1 > cap) {
            u32 want = cap;
            while (want < oi + vlen + 2) {
                want *= 2;
            }
            char *bigger = (char *)kmalloc(want);
            if (!bigger) {
                kfree(out);
                return NULL;
            }
            memcpy(bigger, out, oi);
            kfree(out);
            out = bigger;
            cap = want;
        }
        memcpy(out + oi, val, vlen);
        oi += vlen;
    }

    out[oi] = '\0';
    return out;
}

/*===========================================================================*/
/*                            TOKENISATION                                   */
/*===========================================================================*/
/*
 * Splits a command into pipe/edit stages first, then each stage into argv.
 * Handles 'single', "double" quoting, backslash escapes and $VAR expansion.
 */

typedef struct {
    char *argv[SHELL_MAX_ARGS];
    u32 argc;
    char *redirect_out;      /* ">" target, or NULL */
    char *redirect_in;       /* "<" target, or NULL */
    bool append;             /* ">>" instead of ">" */
} shell_stage_t;

/**
 * tokenize_stage - Split one pipeline stage into argv, honouring quotes.
 *
 * Operates destructively on @work (which must be writable).
 */
static int tokenize_stage(char *work, shell_stage_t *stage)
{
    char *p = work;
    char *token;
    u32 tlen;

    memset(stage, 0, sizeof(*stage));

    /* Scratch token buffer large enough for any single word plus expansion. */
    token = (char *)kmalloc(SHELL_MAX_LINE);
    if (!token) {
        return -ENOMEM;
    }
    tlen = 0;

    for (;;) {
        char c = *p;

        if (c == '\0' || c == ' ' || c == '\t') {
            if (tlen) {
                token[tlen] = '\0';

                /* Redirection operators are only special when they *are* the
                 * whole token, which is how a shell behaves for "> file". */
                if (shell_streq(token, ">") || shell_streq(token, ">>")) {
                    bool app = shell_streq(token, ">>");
                    /* Skip whitespace to the target */
                    while (*p == ' ' || *p == '\t') p++;
                    char *start = p;
                    while (*p && *p != ' ' && *p != '\t') p++;
                    if (p == start) {
                        kfree(token);
                        return -EINVAL;
                    }
                    char saved = *p;
                    *p = '\0';
                    char *expanded = expand_vars(start);
                    *p = saved;
                    if (!expanded) {
                        kfree(token);
                        return -ENOMEM;
                    }
                    stage->redirect_out = expanded;
                    stage->append = app;
                    tlen = 0;
                    if (!c) break;
                    continue;
                }

                if (shell_streq(token, "<")) {
                    while (*p == ' ' || *p == '\t') p++;
                    char *start = p;
                    while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '|') p++;
                    if (p == start) {
                        kfree(token);
                        return -EINVAL;
                    }
                    char saved = *p;
                    *p = '\0';
                    char *expanded = expand_vars(start);
                    *p = saved;
                    if (!expanded) {
                        kfree(token);
                        return -ENOMEM;
                    }
                    stage->redirect_in = expanded;
                    tlen = 0;
                    if (!c) break;
                    continue;
                }

                if (stage->argc >= SHELL_MAX_ARGS - 1) {
                    kfree(token);
                    return -E2BIG;
                }

                char *expanded = expand_vars(token);
                if (!expanded) {
                    kfree(token);
                    return -ENOMEM;
                }
                stage->argv[stage->argc++] = expanded;
                tlen = 0;
            }
            if (c == '\0') {
                break;
            }
            p++;
            continue;
        }

        if (c == '\'') {
            /* Single quotes: literal, no expansion. */
            p++;
            while (*p && *p != '\'') {
                if (tlen + 1 < SHELL_MAX_LINE) token[tlen++] = *p;
                p++;
            }
            if (*p == '\'') p++;
            continue;
        }

        if (c == '"') {
            /* Double quotes: $VAR expands, everything else literal. */
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$')) {
                    p++;
                }
                if (tlen + 1 < SHELL_MAX_LINE) token[tlen++] = *p;
                p++;
            }
            if (*p == '"') p++;
            continue;
        }

        if (c == '\\' && p[1]) {
            p++;
            if (tlen + 1 < SHELL_MAX_LINE) token[tlen++] = *p;
            p++;
            continue;
        }

        if (tlen + 1 < SHELL_MAX_LINE) {
            token[tlen++] = c;
        }
        p++;
    }

    kfree(token);
    return 0;
}

/*===========================================================================*/
/*                            BUILTINS                                       */
/*===========================================================================*/

typedef int (*builtin_fn)(shell_stage_t *stage);

typedef struct {
    const char *name;
    builtin_fn fn;
    const char *help;
} shell_builtin_t;

static int bi_cd(shell_stage_t *st)
{
    const char *target;

    if (st->argc < 2) {
        target = shell_getenv("HOME");
        if (!target || !*target) target = "/";
    } else {
        target = st->argv[1];
    }

    s32 ret = vfs_chdir(target);
    if (ret != 0) {
        shell_error("cd: %s: cannot change directory\n", target);
        return 1;
    }
    return 0;
}

static int bi_pwd(shell_stage_t *st)
{
    char buf[VFS_PATH_MAX];

    (void)st;

    if (vfs_getcwd(buf, sizeof(buf)) != 0) {
        shell_error("pwd: cannot determine current directory\n");
        return 1;
    }
    shell_printf("%s\n", buf);
    return 0;
}

/* Filldir callback for `ls` */
typedef struct {
    u32 count;
    bool long_format;
    bool all;
} ls_ctx_t;

static s32 ls_filldir(void *dirent, const char *name, int namelen, u64 offset,
                      u64 ino, u32 d_type)
{
    ls_ctx_t *ctx = (ls_ctx_t *)dirent;

    (void)offset;
    (void)ino;

    if (!name || namelen <= 0) {
        return 0;
    }

    /* Skip dotfiles unless -a */
    if (!ctx->all && name[0] == '.' && (namelen == 1 || (namelen == 2 && name[1] == '.'))) {
        return 0;
    }

    if (ctx->long_format) {
        shell_printf("%c%c%c%c%c%c  %8llu  %.*s\n",
                     (d_type == VFS_DT_DIR) ? 'd' : '-',
                     'r', 'w', 'x', 'r', 'w',
                     (unsigned long long)ino,
                     namelen, name);
    } else {
        shell_emit(name, (u32)namelen);
        if (d_type == VFS_DT_DIR) {
            shell_puts("/");
        }
        shell_puts("  ");
    }

    ctx->count++;
    return 0;
}

static int bi_ls(shell_stage_t *st)
{
    ls_ctx_t ctx;
    const char *path = NULL;
    int rc = 0;

    memset(&ctx, 0, sizeof(ctx));

    for (u32 i = 1; i < st->argc; i++) {
        if (st->argv[i][0] == '-') {
            for (u32 j = 1; st->argv[i][j]; j++) {
                if (st->argv[i][j] == 'l') ctx.long_format = true;
                if (st->argv[i][j] == 'a') ctx.all = true;
            }
        } else {
            path = st->argv[i];
        }
    }

    if (!path) {
        path = ".";
    }

    if (vfs_readdir(path, &ctx, ls_filldir) != 0) {
        shell_error("ls: %s: no such file or directory\n", path);
        return 1;
    }

    if (ctx.count == 0) {
        /* Genuinely empty, or unreadable - distinguish from failure above. */
        shell_puts("(empty)\n");
    } else if (!ctx.long_format) {
        shell_puts("\n");
    }

    return rc;
}

static int bi_cat(shell_stage_t *st)
{
    char buf[512];
    int rc = 0;

    if (st->argc < 2) {
        shell_error("cat: missing operand\n");
        return 1;
    }

    for (u32 i = 1; i < st->argc; i++) {
        s32 fd = vfs_open(st->argv[i], VFS_FO_READ, 0);
        if (fd < 0) {
            shell_error("cat: %s: cannot open\n", st->argv[i]);
            rc = 1;
            continue;
        }

        for (;;) {
            s64 n = vfs_read((u32)fd, buf, sizeof(buf));
            if (n <= 0) {
                break;
            }
            shell_emit(buf, (u32)n);
        }

        vfs_close((u32)fd);
    }

    return rc;
}

static int bi_echo(shell_stage_t *st)
{
    bool newline = true;
    bool escapes = false;
    u32 start = 1;

    /* Recognise -n (no trailing newline) and -e (interpret escapes). */
    for (; start < st->argc && st->argv[start][0] == '-'; start++) {
        for (u32 j = 1; st->argv[start][j]; j++) {
            if (st->argv[start][j] == 'n') newline = false;
            if (st->argv[start][j] == 'e') escapes = true;
        }
    }

    for (u32 i = start; i < st->argc; i++) {
        if (i > start) {
            shell_puts(" ");
        }

        if (!escapes) {
            shell_puts(st->argv[i]);
            continue;
        }

        for (u32 j = 0; st->argv[i][j]; j++) {
            char c = st->argv[i][j];
            if (c != '\\' || !st->argv[i][j + 1]) {
                shell_emit(&c, 1);
                continue;
            }
            j++;
            switch (st->argv[i][j]) {
            case 'n':  shell_puts("\n"); break;
            case 't':  shell_puts("\t"); break;
            case '\\': shell_puts("\\"); break;
            case 'r':  shell_puts("\r"); break;
            default:
                shell_emit(&c, 1);
                shell_emit(&st->argv[i][j], 1);
                break;
            }
        }
    }

    if (newline) {
        shell_puts("\n");
    }
    return 0;
}

static int bi_mkdir(shell_stage_t *st)
{
    int rc = 0;

    if (st->argc < 2) {
        shell_error("mkdir: missing operand\n");
        return 1;
    }

    for (u32 i = 1; i < st->argc; i++) {
        if (vfs_mkdir(st->argv[i], 0755) != 0) {
            shell_error("mkdir: %s: cannot create directory\n", st->argv[i]);
            rc = 1;
        }
    }
    return rc;
}

static int bi_rm(shell_stage_t *st)
{
    int rc = 0;
    bool force = false;

    for (u32 i = 1; i < st->argc; i++) {
        if (shell_streq(st->argv[i], "-f") || shell_streq(st->argv[i], "-rf")) {
            force = true;
            continue;
        }
        if (vfs_unlink(st->argv[i]) != 0) {
            if (!force) {
                shell_error("rm: %s: cannot remove\n", st->argv[i]);
            }
            rc = 1;
        }
    }

    if (st->argc < 2) {
        shell_error("rm: missing operand\n");
        return 1;
    }
    return rc;
}

static int bi_stat(shell_stage_t *st)
{
    struct vfs_stat s;

    if (st->argc < 2) {
        shell_error("stat: missing operand\n");
        return 1;
    }

    if (vfs_stat(st->argv[1], &s) != 0) {
        shell_error("stat: %s: cannot stat\n", st->argv[1]);
        return 1;
    }

    shell_printf("  File: %s\n", st->argv[1]);
    shell_printf("  Type: %s\n", (s.st_mode & VFS_MODE_IFDIR) ? "directory" : "regular file");
    shell_printf("   Ino: %llu\n", (unsigned long long)s.st_ino);
    shell_printf("  Mode: 0%o\n", s.st_mode & VFS_MODE_MASK);
    shell_printf("  Size: %lld bytes\n", (long long)s.st_size);
    shell_printf(" Links: %u\n", s.st_nlink);
    shell_printf("   UID: %u   GID: %u\n", s.st_uid, s.st_gid);

    return 0;
}

static int bi_export(shell_stage_t *st)
{
    int rc = 0;

    if (st->argc < 2) {
        for (u32 i = 0; i < g_shell.var_count; i++) {
            if (g_shell.vars[i].exported) {
                shell_printf("export %s=%s\n", g_shell.vars[i].name, g_shell.vars[i].value);
            }
        }
        return 0;
    }

    for (u32 i = 1; i < st->argc; i++) {
        char *eq = strchr(st->argv[i], '=');
        if (!eq) {
            /* "export NAME" only marks an existing variable. */
            shell_var_t *v = env_find(st->argv[i]);
            if (v) {
                v->exported = true;
            } else {
                shell_error("export: %s: not set\n", st->argv[i]);
                rc = 1;
            }
            continue;
        }

        *eq = '\0';
        if (shell_setenv(st->argv[i], eq + 1, true) != 0) {
            shell_error("export: %s: out of space\n", st->argv[i]);
            rc = 1;
        }
    }

    return rc;
}

static int bi_env(shell_stage_t *st)
{
    (void)st;

    for (u32 i = 0; i < g_shell.var_count; i++) {
        if (g_shell.vars[i].exported) {
            shell_printf("%s=%s\n", g_shell.vars[i].name, g_shell.vars[i].value);
        }
    }
    return 0;
}

static int bi_unset(shell_stage_t *st)
{
    if (st->argc < 2) {
        shell_error("unset: missing operand\n");
        return 1;
    }
    for (u32 i = 1; i < st->argc; i++) {
        shell_unsetenv(st->argv[i]);
    }
    return 0;
}

static int bi_history(shell_stage_t *st)
{
    u32 n = 20;

    if (st->argc >= 2) {
        char *end = NULL;
        unsigned long v = strtoul(st->argv[1], &end, 10);
        if (end && *end == '\0' && v > 0) {
            n = (u32)v;
        }
        if (shell_streq(st->argv[1], "-c")) {
            history_clear();
            return 0;
        }
    }

    history_print(n);
    return 0;
}

static int bi_alias(shell_stage_t *st)
{
    if (st->argc < 2) {
        aliases_list();
        return 0;
    }

    for (u32 i = 1; i < st->argc; i++) {
        char *eq = strchr(st->argv[i], '=');
        if (!eq) {
            const char *v = alias_get(st->argv[i]);
            if (v) {
                shell_printf("%s='%s'\n", st->argv[i], v);
            } else {
                shell_error("alias: %s: not found\n", st->argv[i]);
                return 1;
            }
            continue;
        }

        *eq = '\0';
        if (alias_add(st->argv[i], eq + 1, NULL) != 0) {
            shell_error("alias: %s: cannot add\n", st->argv[i]);
            return 1;
        }
    }

    return 0;
}

static int bi_help(shell_stage_t *st);

static int bi_exit(shell_stage_t *st)
{
    u32 code = 0;

    if (st->argc >= 2) {
        char *end = NULL;
        unsigned long v = strtoul(st->argv[1], &end, 10);
        if (end && *end == '\0') {
            code = (u32)(v & 0xFF);
        }
    }

    g_shell.running = false;
    g_shell.exit_status = code;
    return (int)code;
}

static int bi_true(shell_stage_t *st)
{
    (void)st;
    return 0;
}

static int bi_false(shell_stage_t *st)
{
    (void)st;
    return 1;
}

static int bi_clear(shell_stage_t *st)
{
    (void)st;
    /* ANSI: clear screen and home the cursor. */
    shell_puts("\033[2J\033[H");
    return 0;
}

static int bi_lang(shell_stage_t *st)
{
    (void)st;
    lang_runtime_status();
    return 0;
}

static int bi_mern(shell_stage_t *st)
{
    (void)st;
    return lang_mern_status();
}

/**
 * bi_run - Run a file, choosing the runtime from its type.
 */
static int bi_run(shell_stage_t *st)
{
    int rc;

    if (st->argc < 2) {
        shell_error("run: usage: run <file>\n");
        return 1;
    }

    rc = lang_exec_file(st->argv[1]);

    return (rc == 0) ? 0 : 1;
}

/**
 * bi_python / bi_node / bi_cargo - Language-specific launchers.
 *
 * Each delegates to lang_exec_file(), which resolves the interpreter for
 * interpreted languages and the ELF loader for compiled ones. They exist so a
 * user can type what they would expect to type.
 */
static int bi_lang_launch(shell_stage_t *st, const char *interp, const char *label)
{
    char resolved[VFS_PATH_MAX];

    if (st->argc < 2) {
        /* No script: run the interpreter itself if it is present. */
        if (lang_find_in_path(interp, resolved, sizeof(resolved))) {
            shell_printf("%s: %s\n", label, resolved);
            return 0;
        }
        shell_error("%s: %s is not installed\n", label, interp);
        return 127;
    }

    return (lang_exec_file(st->argv[1]) == 0) ? 0 : 1;
}

static int bi_python(shell_stage_t *st)
{
    return bi_lang_launch(st, "/usr/bin/python3", "python");
}

static int bi_node(shell_stage_t *st)
{
    return bi_lang_launch(st, "/usr/bin/node", "node");
}

static int bi_cargo(shell_stage_t *st)
{
    /*
     * There is no Rust compiler on NEXUS, so `cargo` can only explain the
     * situation rather than silently fail.
     */
    (void)st;
    shell_puts("cargo: no Rust toolchain on NEXUS.\n"
               "       Build a static x86-64 binary on the host and place it\n"
               "       somewhere in $PATH; 'run <binary>' will load it.\n");
    return 127;
}

static const shell_builtin_t g_builtins[] = {
    { "cd",      bi_cd,      "Change the working directory" },
    { "pwd",     bi_pwd,     "Print the working directory" },
    { "ls",      bi_ls,      "List directory contents (-l long, -a all)" },
    { "cat",     bi_cat,     "Write file contents to output" },
    { "echo",    bi_echo,    "Write arguments to output (-n, -e)" },
    { "mkdir",   bi_mkdir,   "Create a directory" },
    { "rm",      bi_rm,      "Remove a file (-f ignore missing)" },
    { "stat",    bi_stat,    "Show file status" },
    { "export",  bi_export,  "Set an environment variable" },
    { "env",     bi_env,     "List exported variables" },
    { "unset",   bi_unset,   "Remove a variable" },
    { "history", bi_history, "Show command history (-c clears)" },
    { "alias",   bi_alias,   "List or define aliases" },
    { "help",    bi_help,    "Show help for a command" },
    { "clear",   bi_clear,   "Clear the screen" },
    { "true",    bi_true,    "Do nothing, succeed" },
    { "false",   bi_false,   "Do nothing, fail" },
    { "run",     bi_run,     "Run a file (ELF binary or script)" },
    { "lang",    bi_lang,    "Show language/runtime support" },
    { "python",  bi_python,  "Run a Python script" },
    { "node",    bi_node,    "Run a JavaScript file" },
    { "cargo",   bi_cargo,   "Rust toolchain (not available)" },
    { "mern",    bi_mern,    "MERN stack status" },
    { "exit",    bi_exit,    "Exit the shell" },
};

#define SHELL_BUILTIN_COUNT (sizeof(g_builtins) / sizeof(g_builtins[0]))

static const shell_builtin_t *builtin_find(const char *name)
{
    for (u32 i = 0; i < SHELL_BUILTIN_COUNT; i++) {
        if (shell_streq(g_builtins[i].name, name)) {
            return &g_builtins[i];
        }
    }
    return NULL;
}

static int bi_help(shell_stage_t *st)
{
    if (st->argc >= 2) {
        const shell_builtin_t *b = builtin_find(st->argv[1]);
        if (b) {
            shell_printf("%s - %s\n", b->name, b->help);
            return 0;
        }
        if (help_show(st->argv[1]) == 0) {
            return 0;
        }
        shell_error("help: %s: no help available\n", st->argv[1]);
        return 1;
    }

    shell_puts("NEXUS shell builtins:\n");
    for (u32 i = 0; i < SHELL_BUILTIN_COUNT; i++) {
        shell_printf("  %-10s %s\n", g_builtins[i].name, g_builtins[i].help);
    }
    shell_puts("\nOperators: | pipe, > redirect, >> append, < input, ; sequence, && and, || or\n");
    shell_puts("Try 'help <command>' for details.\n");
    return 0;
}

/*===========================================================================*/
/*                          EXTERNAL COMMANDS                                */
/*===========================================================================*/

/**
 * run_external - Look up and run a non-builtin command from the filesystem.
 *
 * Resolution order: absolute/relative path as given, then each directory in
 * $PATH. Execution goes through exec_process(), which is the real syscall
 * path - so a binary has to actually exist and be executable.
 */
static int run_external(shell_stage_t *st)
{
    const char *cmd = st->argv[0];
    char candidate[VFS_PATH_MAX];
    struct vfs_stat s;
    s32 ret;

    /* Direct path given. */
    if (strchr(cmd, '/')) {
        if (vfs_stat(cmd, &s) != 0) {
            shell_error("%s: no such file or directory\n", cmd);
            return 127;
        }
    } else {
        const char *path = shell_getenv("PATH");
        bool found = false;

        if (!path || !*path) {
            path = "/bin:/usr/bin";
        }

        const char *p = path;
        while (*p) {
            u32 di = 0;
            while (*p && *p != ':' && di + 1 < sizeof(candidate)) {
                candidate[di++] = *p++;
            }
            candidate[di] = '\0';
            if (*p == ':') {
                p++;
            }

            char full[VFS_PATH_MAX];
            u32 fi = 0;
            for (u32 k = 0; candidate[k] && fi + 1 < sizeof(full); k++) {
                full[fi++] = candidate[k];
            }
            if (fi + 1 < sizeof(full)) {
                full[fi++] = '/';
            }
            for (u32 k = 0; cmd[k] && fi + 1 < sizeof(full); k++) {
                full[fi++] = cmd[k];
            }
            full[fi] = '\0';

            if (vfs_stat(full, &s) == 0) {
                shell_strlcpy(candidate, full, sizeof(candidate));
                found = true;
                break;
            }
        }

        if (!found) {
            shell_error("%s: command not found\n", cmd);
            return 127;
        }
    }

    /* Executable bit check: honest refusal beats a mystery failure. */
    if (!(s.st_mode & (VFS_MODE_IXUSR | VFS_MODE_IXGRP | VFS_MODE_IXOTH))) {
        shell_error("%s: permission denied (not executable)\n", cmd);
        return 126;
    }

    struct task_struct *self = get_current();
    if (!self) {
        shell_error("%s: cannot execute outside a process context\n", cmd);
        return 126;
    }

    ret = exec_process(self, candidate, st->argv, NULL);
    if (ret != 0) {
        shell_error("%s: exec failed (%d)\n", cmd, ret);
        return 126;
    }

    return 0;
}

/*===========================================================================*/
/*                        STAGE / PIPELINE EXECUTION                         */
/*===========================================================================*/

static int run_stage(shell_stage_t *st)
{
    const shell_builtin_t *b;

    if (st->argc == 0) {
        return 0;
    }

    b = builtin_find(st->argv[0]);
    if (b) {
        return b->fn(st);
    }

    return run_external(st);
}

/**
 * split_pipeline - Split a line on single '|' characters.
 *
 * '||' is a sequencing operator, not an empty pipe, so it is skipped over.
 * Returns the number of stages written into @stages.
 */
static u32 split_pipeline(char *line, char *stages[], u32 max)
{
    u32 n = 0;
    char *p = line;

    if (max == 0) {
        return 0;
    }
    stages[n++] = p;

    for (; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;                    /* Escaped character: keep both. */
            continue;
        }
        if (*p == '|') {
            if (p[1] == '|') {
                p++;                /* Logical OR - not a pipe. */
                continue;
            }
            *p = '\0';
            if (n < max) {
                stages[n++] = p + 1;
            }
        }
    }

    return n;
}

/**
 * exec_pipeline - Run one pipeline (already split into stages).
 *
 * With a single stage this is just a command. With several, each stage's
 * captured output is fed to the next as its input buffer, which is exactly
 * what a pipe gives you for a synchronous, in-process pipeline.
 */
static int exec_pipeline(char *stages[], u32 nstages)
{
    int status = 0;
    char *input = NULL;         /* Output of the previous stage */
    u32 input_len = 0;

    for (u32 i = 0; i < nstages; i++) {
        shell_stage_t st;
        int rc;

        rc = tokenize_stage(stages[i], &st);
        if (rc != 0) {
            shell_error("shell: syntax error\n");
            if (input) kfree(input);
            return 2;
        }

        /*
         * The last stage writes to wherever the command's redirect says.
         * Every earlier stage is captured so it can feed the next one.
         */
        bool stage_is_last = (i + 1 == nstages);
        bool need_capture = !stage_is_last || st.redirect_out != NULL;

        if (need_capture) {
            g_shell.capturing = true;
            g_shell.capture_len = 0;
            if (g_shell.capture) {
                g_shell.capture[0] = '\0';
            }
        }

        /* Feed the previous stage's output in as this stage's input. */
        char saved_input[512];
        u32 saved_len = 0;
        if (input && input_len) {
            saved_len = input_len < sizeof(saved_input) ? input_len : (u32)sizeof(saved_input) - 1;
            memcpy(saved_input, input, saved_len);
        }

        status = run_stage(&st);

        if (need_capture) {
            g_shell.capturing = false;
        }

        /* Emit captured text: to the redirect target, or into the next stage. */
        if (need_capture) {
            if (input) {
                kfree(input);
                input = NULL;
            }
            if (g_shell.capture_len) {
                input = (char *)kmalloc(g_shell.capture_len + 1);
                if (input) {
                    memcpy(input, g_shell.capture, g_shell.capture_len);
                    input[g_shell.capture_len] = '\0';
                    input_len = g_shell.capture_len;
                } else {
                    input_len = 0;
                }
            } else {
                input_len = 0;
            }
        }

        /* Handle "> file" / ">> file" on the final stage. */
        if (stage_is_last && st.redirect_out) {
            u32 flags = VFS_FO_WRITE | VFS_FO_CREAT | (st.append ? VFS_FO_APPEND : 0);
            s32 fd = vfs_open(st.redirect_out, flags, 0644);
            if (fd < 0) {
                shell_error("shell: %s: cannot open for writing\n", st.redirect_out);
                status = 1;
            } else {
                if (input && input_len) {
                    vfs_write((u32)fd, input, input_len);
                }
                vfs_close((u32)fd);
            }
        } else if (stage_is_last && input && input_len) {
            /* No redirect: the captured text was the last stage's output. */
            sink_write(&g_shell.out, input, input_len);
        }

        /* Single-stage commands with no redirect print as they go. */
        if (stage_is_last && !(need_capture && !st.redirect_out)) {
            /* nothing extra */
        }

        (void)saved_input;
        (void)saved_len;

        for (u32 a = 0; a < st.argc; a++) {
            kfree(st.argv[a]);
        }
        if (st.redirect_out) kfree(st.redirect_out);
        if (st.redirect_in)  kfree(st.redirect_in);

        if (!g_shell.running) {
            break;              /* `exit` was executed. */
        }
    }

    if (input) {
        kfree(input);
    }

    return status;
}

/*===========================================================================*/
/*                          COMMAND SEQUENCING                               */
/*===========================================================================*/

/**
 * shell_execute_line - Execute a full command line.
 *
 * Handles && , || and ; in a single left-to-right pass, honouring short
 * circuiting: "a && b" runs b only when a succeeded, "a || b" only when it
 * failed. Pipelines between the separators are delegated to exec_pipeline().
 *
 * Returns the exit status of the last command that actually ran.
 */
int shell_execute_line(const char *line)
{
    char work[SHELL_MAX_LINE];
    int status = 0;

    if (!line) {
        return -EINVAL;
    }

    shell_strlcpy(work, line, sizeof(work));

    char *p = work;
    char *chunk = p;

    /*
     * Walk the line, splitting on top-level sequence operators. Quoting is
     * remembered so that ";" or "&&" inside quotes stays literal.
     */
    bool in_single = false, in_double = false, escaped = false;
    int pending_op = 0;         /* 0 = always, 1 = only if status==0, 2 = only if !=0 */
    bool skip_chunk = false;

    for (;;) {
        char c = *p;
        int op = 0;
        u32 oplen = 0;
        char *after = NULL;

        if (c) {
            if (escaped) {
                escaped = false;
                p++;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                p++;
                continue;
            }
            if (c == '\'' && !in_double) {
                in_single = !in_single;
                p++;
                continue;
            }
            if (c == '"' && !in_single) {
                in_double = !in_double;
                p++;
                continue;
            }

            if (!in_single && !in_double) {
                if (c == ';') {
                    op = 0; oplen = 1;
                } else if (c == '&' && p[1] == '&') {
                    op = 1; oplen = 2;
                } else if (c == '|' && p[1] == '|') {
                    op = 2; oplen = 2;
                }
            }

            if (!op) {
                p++;
                continue;
            }
            after = p + oplen;
        }

        /* End of a chunk (either a real operator or end of string). */
        char saved = *p;
        *p = '\0';

        bool run_it;
        switch (pending_op) {
        case 1:  run_it = (status == 0); break;
        case 2:  run_it = (status != 0); break;
        default: run_it = true;          break;
        }
        if (skip_chunk) {
            run_it = false;
        }

        /* Trim leading whitespace */
        char *cmd = chunk;
        while (*cmd == ' ' || *cmd == '\t') cmd++;

        if (run_it && *cmd) {
            char *pipe_stages[SHELL_MAX_STAGES];
            u32 n = split_pipeline(cmd, pipe_stages, SHELL_MAX_STAGES);
            status = exec_pipeline(pipe_stages, n);
            g_shell.exit_status = (u32)status;
            g_shell.last_status = (u32)status;
        }

        *p = saved;

        if (!c) {
            break;
        }

        pending_op = op;
        p = after;
        chunk = p;
        (void)skip_chunk;
    }

    return status;
}

/*===========================================================================*/
/*                          INTERACTIVE FRONTEND                             */
/*===========================================================================*/

/** shell_prompt - Build the PS1-style prompt string. */
static void shell_prompt(char *buf, u32 size)
{
    char cwd[VFS_PATH_MAX];
    const char *ps1 = shell_getenv("PS1");

    if (ps1 && *ps1) {
        char *expanded = expand_vars(ps1);
        if (expanded) {
            shell_strlcpy(buf, expanded, size);
            kfree(expanded);
            return;
        }
    }

    if (vfs_getcwd(cwd, sizeof(cwd)) != 0) {
        shell_strlcpy(cwd, "?", sizeof(cwd));
    }

    /* Home directories are abbreviated, like every other shell. */
    const char *home = shell_getenv("HOME");
    if (home && *home) {
        u64 hl = strlen(home);
        if (hl && strcmp(cwd, home) == 0) {
            shell_strlcpy(cwd, "~", sizeof(cwd));
        } else if (hl && strncmp(cwd, home, hl) == 0 && cwd[hl] == '/') {
            char join[VFS_PATH_MAX];
            shell_strlcpy(join, "~", sizeof(join));
            shell_strlcpy(join + 1, cwd + hl, sizeof(join) - 1);
            shell_strlcpy(cwd, join, sizeof(cwd));
        }
    }

    const char *user = shell_getenv("USER");
    if (!user || !*user) {
        user = "user";
    }

    snprintf(buf, size, "%s@nexus:%s$ ", user, cwd);
}

void shell_set_output(shell_write_fn fn, void *ctx)
{
    g_shell.out.write = fn;
    g_shell.out.ctx = ctx;
}

void shell_set_error(shell_write_fn fn, void *ctx)
{
    g_shell.err.write = fn;
    g_shell.err.ctx = ctx;
}

bool shell_is_running(void)
{
    return g_shell.running;
}

u32 shell_exit_status(void)
{
    return g_shell.exit_status;
}

u32 shell_get_pid(void)
{
    return g_shell.pid;
}

u32 shell_var_count(void)
{
    return g_shell.var_count;
}

/**
 * shell_submit_line - Feed one line to the shell as if the user typed it.
 *
 * Adds it to history, echoes nothing (the caller owns the display), and
 * executes it. Blank lines are ignored without polluting history.
 */
int shell_submit_line(const char *line)
{
    char expanded[SHELL_MAX_LINE];
    int status;
    char cwd[VFS_PATH_MAX];

    if (!line) {
        return -EINVAL;
    }

    if (!g_shell.running) {
        return -EPIPE;
    }

    /* Skip leading whitespace to detect an empty line. */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '\n') {
        return 0;
    }

    if (vfs_getcwd(cwd, sizeof(cwd)) != 0) {
        cwd[0] = '\0';
    }
    history_add(line, 0, cwd);

    /*
     * Alias expansion happens on the raw text before parsing, so an alias may
     * itself introduce a pipeline or redirect.
     */
    shell_strlcpy(expanded, line, sizeof(expanded));
    alias_expand(expanded, sizeof(expanded));

    status = shell_execute_line(expanded);

    /* Record the real status now that we know it. */
    history_add(expanded, (u32)status, cwd);

    return status;
}

/** shell_print_prompt - Ask the user for input via the configured sink. */
void shell_print_prompt(void)
{
    char buf[SHELL_PROMPT_MAX];

    if (!g_shell.running) {
        return;
    }
    shell_prompt(buf, sizeof(buf));
    shell_puts(buf);
}

/**
 * shell_init - Initialise the shell and its default environment.
 *
 * Safe to call more than once; the second call is a no-op.
 */
int shell_init(void)
{
    if (g_shell.initialized) {
        return 0;
    }

    memset(&g_shell, 0, sizeof(g_shell));

    g_shell.initialized = true;
    g_shell.running = true;
    g_shell.exit_status = 0;
    g_shell.pid = 1;

    history_init(1000);
    aliases_init(64);

    /* A sane starting environment. */
    shell_setenv("PATH", "/bin:/usr/bin:/sbin", true);
    shell_setenv("HOME", "/", true);
    shell_setenv("USER", "root", true);
    shell_setenv("SHELL", "/bin/nexsh", true);
    shell_setenv("PWD", "/", false);
    shell_setenv("TERM", "nexus-256color", true);
    shell_setenv("HOSTNAME", "nexus", true);

    /* Small quality-of-life aliases. */
    alias_add("ll", "ls -l", "List in long format");
    alias_add("la", "ls -a", "List all entries");
    alias_add("..", "cd ..", "Go up one directory");
    alias_add("cls", "clear", "Clear the screen");

    pr_info("[SHELL] Nexus shell initialised (pid %u)\n", g_shell.pid);

    return 0;
}

void shell_shutdown(void)
{
    if (!g_shell.initialized) {
        return;
    }

    g_shell.running = false;

    if (g_shell.capture) {
        kfree(g_shell.capture);
        g_shell.capture = NULL;
        g_shell.capture_cap = 0;
        g_shell.capture_len = 0;
    }

    g_shell.initialized = false;
}

/*===========================================================================*/
/*                    HELPERS EXPORTED TO OTHER MODULES                      */
/*===========================================================================*/
/*
 * lang_runtime.c needs to print through the shell's active sink so its output
 * lands in the terminal (or a redirect) rather than on the kernel log.
 */

void shell_puts_ex(const char *text)
{
    shell_puts(text);
}

void shell_printf_ex(const char *fmt, ...)
{
    char buf[512];
    __builtin_va_list args;

    __builtin_va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    shell_puts(buf);
}

void shell_error_ex(const char *fmt, ...)
{
    char buf[512];
    __builtin_va_list args;

    __builtin_va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);

    shell_error("%s", buf);
}
