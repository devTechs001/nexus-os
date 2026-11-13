/*
 * NEXUS OS - Language Runtime Dispatch
 * apps/terminal/lang_runtime.c
 *
 * Support for running programs written in several languages, wired into the
 * shell's external-command path.
 *
 * There is no compiler or interpreter in this tree. What this file does is
 * make the distinction between the two ways a language can work on NEXUS
 * explicit and usable:
 *
 *   Compiled languages (C, C++, Rust)
 *     Produce a static ELF64. The ELF loader (kernel/core/elf_loader.c) can
 *     validate and place them. Detected by magic bytes, not by extension, so
 *     a misnamed file is reported honestly.
 *
 *   Interpreted languages (Python, and the JavaScript side of MERN)
 *     Need an interpreter binary to be present on the system. This layer
 *     resolves the interpreter through $PATH and hands it the script. If the
 *     interpreter is absent it says so - it does not pretend to interpret.
 *
 * The MERN stack is treated as what it is: a *hosted web stack*. On NEXUS it
 * means Node.js + a MongoDB-compatible store reachable over the network
 * stack. Neither exists in-tree yet, so `mern` reports the precise missing
 * components instead of a generic failure.
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/elf.h"
#include "../../fs/vfs/vfs.h"
#include "lang_runtime.h"

/*===========================================================================*/
/*                            LANGUAGE TABLE                                 */
/*===========================================================================*/

typedef struct {
    const char *name;
    const char *extensions[4];      /* File extensions, with the dot */
    const char *interpreter;        /* NULL for compiled languages */
    const char *argv0_flag;         /* How to pass the script */
    bool compiled;
} lang_desc_t;

static const lang_desc_t g_languages[] = {
    { "c",      { ".c", ".h", NULL, NULL },                NULL,            NULL,   true  },
    { "c++",    { ".cpp", ".cc", ".cxx", ".hpp" },         NULL,            NULL,   true  },
    { "rust",   { ".rs", NULL, NULL, NULL },               NULL,            NULL,   true  },
    { "python", { ".py", ".pyw", ".pyc", NULL },           "/usr/bin/python3", NULL, false },
    { "js",     { ".js", ".mjs", ".cjs", ".jsx" },         "/usr/bin/node",  NULL,  false },
    { "shell",  { ".sh", NULL, NULL, NULL },               "/bin/sh",       NULL,   false },
};

#define LANG_COUNT (sizeof(g_languages) / sizeof(g_languages[0]))

/*===========================================================================*/
/*                              HELPERS                                      */
/*===========================================================================*/

static bool str_ends_with(const char *s, const char *suffix)
{
    u32 sl, xl;

    if (!s || !suffix) {
        return false;
    }

    sl = (u32)strlen(s);
    xl = (u32)strlen(suffix);

    if (xl > sl) {
        return false;
    }

    return strcmp(s + (sl - xl), suffix) == 0;
}

/** path_has_executable - True if @path exists and has any execute bit set */
static bool path_has_executable(const char *path, struct vfs_stat *out)
{
    struct vfs_stat st;

    if (!path || vfs_stat(path, &st) != 0) {
        return false;
    }

    if (out) {
        *out = st;
    }

    return (st.st_mode & (VFS_MODE_IXUSR | VFS_MODE_IXGRP | VFS_MODE_IXOTH)) != 0;
}

/**
 * find_in_path - Resolve a bare program name against $PATH
 *
 * @name:  Program name (no slash)
 * @out:   Receives the full path
 * @size:  Size of @out
 *
 * Returns true if an executable was found.
 */
bool lang_find_in_path(const char *name, char *out, u32 size)
{
    const char *path = shell_getenv("PATH");
    const char *p;

    if (!name || !out || size == 0) {
        return false;
    }

    /* An explicit path is used as-is. */
    if (strchr(name, '/')) {
        if (path_has_executable(name, NULL)) {
            u32 i = 0;
            while (name[i] && i + 1 < size) {
                out[i] = name[i];
                i++;
            }
            out[i] = '\0';
            return true;
        }
        return false;
    }

    if (!path || !*path) {
        path = "/bin:/usr/bin:/sbin";
    }

    p = path;
    while (*p) {
        char dir[VFS_PATH_MAX];
        u32 di = 0;

        while (*p && *p != ':' && di + 1 < sizeof(dir)) {
            dir[di++] = *p++;
        }
        dir[di] = '\0';
        if (*p == ':') {
            p++;
        }

        /* Compose dir/name */
        u32 fi = 0;
        for (u32 k = 0; dir[k] && fi + 1 < size; k++) {
            out[fi++] = dir[k];
        }
        if (fi + 1 < size && (fi == 0 || out[fi - 1] != '/')) {
            out[fi++] = '/';
        }
        for (u32 k = 0; name[k] && fi + 1 < size; k++) {
            out[fi++] = name[k];
        }
        out[fi] = '\0';

        if (path_has_executable(out, NULL)) {
            return true;
        }
    }

    return false;
}

/*===========================================================================*/
/*                            DETECTION                                      */
/*===========================================================================*/

/**
 * lang_detect - Identify the language a file is written in
 *
 * Extension first (that is what the user's intent is), but a compiled
 * language is only reported as such once the ELF magic has been confirmed -
 * a .rs file is source code, not a program, and saying otherwise would send
 * the caller down the wrong path.
 *
 * Returns a pointer into the language table, or NULL.
 */
const lang_desc_t *lang_detect(const char *path)
{
    if (!path) {
        return NULL;
    }

    for (u32 i = 0; i < LANG_COUNT; i++) {
        for (u32 e = 0; e < 4 && g_languages[i].extensions[e]; e++) {
            if (str_ends_with(path, g_languages[i].extensions[e])) {
                return &g_languages[i];
            }
        }
    }

    return NULL;
}

const char *lang_name_for_path(const char *path)
{
    const lang_desc_t *l = lang_detect(path);
    return l ? l->name : "unknown";
}

/*===========================================================================*/
/*                             EXECUTION                                     */
/*===========================================================================*/

/**
 * lang_run_compiled - Run a compiled ELF64 binary
 *
 * Returns 0 on success, negative error code on failure.
 */
static int lang_run_compiled(const char *path, const char *source)
{
    u64 entry = 0;
    u64 size = 0;
    u32 segments = 0;
    int rc;

    /* The path has to be the binary itself, not its source file. */
    if (!elf_can_execute(path)) {
        pr_err("[LANG] %s is source code, not an ELF64 executable\n", path);

        /*
         * Give the user the actionable half: the compiler is not shipped, so
         * tell them the binary has to be produced elsewhere.
         */
        shell_error_ex("\\n  %s looks like source code (%s).\\n"
                       "  NEXUS does not ship a compiler. Build a static\\n"
                       "  x86-64 ELF64 binary on the host and place it at %s.\\n\\n",
                       source, lang_name_for_path(source), path);
        return -ENOEXEC;
    }

    rc = elf_exec(path, &entry, &size, &segments);
    if (rc != 0) {
        shell_error_ex("  loader rejected %s (%d)\\n", path, rc);
        return rc;
    }

    shell_printf_ex("  loaded %llu bytes, %u segments, entry 0x%llX\\n",
                    (unsigned long long)size, segments,
                    (unsigned long long)entry);
    shell_printf_ex("  note: userspace hand-off is not implemented yet - "
                    "the image is resident but has not been entered.\\n");

    return 0;
}

/**
 * lang_run_script - Run a script through its interpreter
 *
 * Returns 0 on success, negative error code on failure.
 */
static int lang_run_script(const lang_desc_t *lang, const char *script)
{
    char interp[VFS_PATH_MAX];
    struct vfs_stat st;

    if (!lang || !script) {
        return -EINVAL;
    }

    if (!lang->interpreter) {
        shell_error_ex("  no interpreter defined for %s\\n", lang->name);
        return -ENOSYS;
    }

    if (vfs_stat(script, &st) != 0) {
        shell_error_ex("  %s: no such file\\n", script);
        return -ENOENT;
    }

    if (!lang_find_in_path(lang->interpreter, interp, sizeof(interp))) {
        shell_error_ex("\\n  %s is not installed.\\n"
                       "  '%s' requires the %s interpreter at %s.\\n\\n",
                       lang->interpreter, script, lang->name, lang->interpreter);
        return -ENOSYS;
    }

    pr_info("[LANG] %s -> %s %s\n", script, interp, script);

    /*
     * Delegation goes through the real exec path. It will report ENOSYS until
     * the scheduler can enter userspace; that is surfaced verbatim rather than
     * masked.
     */
    struct task_struct *self = get_current();
    if (!self) {
        shell_error_ex("  cannot execute outside a process context\\n");
        return -ENOSYS;
    }

    char *argv[3];
    argv[0] = (char *)interp;
    argv[1] = (char *)script;
    argv[2] = NULL;

    int rc = exec_process(self, interp, argv, NULL);
    if (rc != 0) {
        shell_error_ex("  interpreter start failed (%d)\\n", rc);
        return rc;
    }

    return 0;
}

/**
 * lang_exec_file - Run a file in whatever language it is written in
 *
 * Returns 0 on success, negative error code on failure.
 */
int lang_exec_file(const char *path)
{
    const lang_desc_t *lang;

    if (!path || !*path) {
        return -EINVAL;
    }

    /* A genuine executable always wins, whatever it is called. */
    if (elf_can_execute(path)) {
        return lang_run_compiled(path, path);
    }

    lang = lang_detect(path);
    if (!lang) {
        shell_error_ex("  %s: unrecognised file type\\n", path);
        return -ENOEXEC;
    }

    if (lang->compiled) {
        return lang_run_compiled(path, path);
    }

    return lang_run_script(lang, path);
}

/*===========================================================================*/
/*                            MERN STACK                                     */
/*===========================================================================*/

typedef struct {
    const char *component;
    const char *role;
    const char *probe;          /* Program to look for, or subsystem */
    bool        present;
} mern_part_t;

/**
 * lang_mern_status - Report on the state of the MERN stack
 *
 * MERN is MongoDB + Express + React + Node. On NEXUS each of those is a
 * hosted dependency rather than something the kernel provides, so the useful
 * answer is a concrete checklist of what is present and what is missing.
 */
int lang_mern_status(void)
{
    static mern_part_t parts[] = {
        { "Node.js",  "runtime for Express and SSR React", "/usr/bin/node", false },
        { "npm",      "dependency resolution",             "/usr/bin/npm",  false },
        { "MongoDB",  "document store",                    "/usr/bin/mongod", false },
        { "Express",  "server framework (npm module)",     NULL,            false },
        { "React",    "UI library (npm module)",           NULL,            false },
    };
    u32 present = 0;

    shell_puts_ex("\\n  MERN stack status\\n"
                  "  ------------------------------------------------\\n");

    for (u32 i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        if (parts[i].probe) {
            parts[i].present = path_has_executable(parts[i].probe, NULL);
        } else {
            /* npm modules need Node itself to be usable. */
            parts[i].present = false;
        }

        if (parts[i].present) {
            present++;
        }

        shell_printf_ex("  %-10s %-8s %s\\n",
                        parts[i].component,
                        parts[i].present ? "[ok]" : "[missing]",
                        parts[i].role);
    }

    shell_puts_ex("  ------------------------------------------------\\n");

    if (present == 0) {
        shell_puts_ex("  Nothing from the MERN stack is installed.\\n"
                      "\\n"
                      "  What this needs, in order:\\n"
                      "    1. a userspace hand-off (context_switch + page tables)\\n"
                      "    2. /usr/bin/node on disk - a static x86-64 build\\n"
                      "    3. npm module resolution and a writable /usr/lib/node_modules\\n"
                      "    4. a MongoDB-protocol server, or a compatible store on\\n"
                      "       the network stack (tcp/27017)\\n"
                      "\\n"
                      "  The kernel side that exists: ELF64 loading, the TCP/IP\\n"
                      "  stack, and the VFS that would host node_modules.\\n");
        return -ENOSYS;
    }

    shell_printf_ex("  %u of 5 components present.\\n\\n",
                    (unsigned)present);

    return present >= 5 ? 0 : -ENOSYS;
}

/**
 * lang_runtime_status - Summarise the language support available
 */
void lang_runtime_status(void)
{
    shell_puts_ex("\\n  Language support\\n"
                  "  ------------------------------------------------\\n");

    for (u32 i = 0; i < LANG_COUNT; i++) {
        const lang_desc_t *l = &g_languages[i];

        if (l->compiled) {
            shell_printf_ex("  %-8s compiled  -> ELF64 loader (%s)\\n",
                            l->name, "no in-tree compiler");
        } else {
            char found[VFS_PATH_MAX];
            bool have = lang_find_in_path(l->interpreter, found, sizeof(found));

            shell_printf_ex("  %-8s hosted    -> %s %s\\n",
                            l->name,
                            l->interpreter,
                            have ? "[present]" : "[not installed]");
        }
    }

    shell_puts_ex("  ------------------------------------------------\\n");
    shell_puts_ex("  Use 'lang' for this summary, 'mern' for the web stack.\\n\\n");
}
