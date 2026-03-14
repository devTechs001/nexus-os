/*
 * NEXUS OS - Terminal Enhancements
 * apps/terminal/terminal_features.c
 *
 * Advanced terminal features: history, aliases, emulations, help
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CONFIGURATION                                     */
/*===========================================================================*/

#define MAX_HISTORY             10000
#define MAX_ALIASES             256
#define MAX_EMULATIONS          32
#define MAX_HELP_TOPICS         128
#define MAX_ALIAS_VALUE         512
#define MAX_HELP_TEXT           2048

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Command History Entry */
typedef struct {
    char *command;
    u64 timestamp;
    u32 exit_code;
    char directory[256];
    u32 duration_ms;
} history_entry_t;

/* Command History */
typedef struct {
    history_entry_t *entries;
    u32 count;
    u32 max_count;
    u32 current_pos;
    bool search_mode;
    char search_query[64];
    u32 search_results[100];
    u32 search_count;
    bool timestamps_enabled;
    bool dedup_enabled;
    char history_file[256];
    bool save_to_file;
} command_history_t;

/* Alias Entry */
typedef struct {
    char name[64];
    char value[MAX_ALIAS_VALUE];
    char description[256];
    bool global;
    u64 created;
    u32 usage_count;
} alias_entry_t;

/* Alias Manager */
typedef struct {
    alias_entry_t *aliases;
    u32 count;
    u32 max_count;
    char alias_file[256];
    bool save_to_file;
} alias_manager_t;

/* Emulation Mode */
typedef struct {
    char name[32];
    char description[128];
    char prompt_template[64];
    u32 color_scheme;
    bool case_sensitive;
    bool expand_vars;
    char **init_commands;
    u32 init_command_count;
} emulation_mode_t;

/* Emulation Manager */
typedef struct {
    emulation_mode_t *modes;
    u32 count;
    u32 current_mode;
    char config_file[256];
} emulation_manager_t;

/* Help Topic */
typedef struct {
    char command[64];
    char short_desc[128];
    char full_desc[MAX_HELP_TEXT];
    char syntax[256];
    char examples[512];
    char see_also[256];
    u32 category;
} help_topic_t;

/* Help System */
typedef struct {
    help_topic_t *topics;
    u32 count;
    u32 max_count;
    bool color_enabled;
    u32 display_width;
    u32 display_height;
} help_system_t;

/* Global Terminal Features */
static command_history_t g_history;
static alias_manager_t g_aliases;
static emulation_manager_t g_emulations;
static help_system_t g_help;

/*===========================================================================*/
/*                         COMMAND HISTORY                                   */
/*===========================================================================*/

/* Initialize Command History */
int history_init(u32 max_entries)
{
    printk("[TERM-HISTORY] Initializing history (max: %u)\n", max_entries);
    
    memset(&g_history, 0, sizeof(command_history_t));
    
    g_history.entries = (history_entry_t *)kmalloc(sizeof(history_entry_t) * max_entries);
    if (!g_history.entries) {
        return -ENOMEM;
    }
    
    memset(g_history.entries, 0, sizeof(history_entry_t) * max_entries);
    g_history.count = 0;
    g_history.max_count = max_entries;
    g_history.current_pos = 0;
    g_history.search_mode = false;
    g_history.timestamps_enabled = true;
    g_history.dedup_enabled = true;
    g_history.save_to_file = true;
    strncpy(g_history.history_file, "/root/.nexus_history", 255);
    
    printk("[TERM-HISTORY] History initialized\n");
    printk("[TERM-HISTORY] Max entries: %u\n", max_entries);
    printk("[TERM-HISTORY] Timestamps: %s\n", g_history.timestamps_enabled ? "Enabled" : "Disabled");
    printk("[TERM-HISTORY] Dedup: %s\n", g_history.dedup_enabled ? "Enabled" : "Disabled");
    
    return 0;
}

/* Add Command to History */
int history_add(const char *command, u32 exit_code, const char *directory)
{
    if (!command || g_history.max_count == 0) {
        return -EINVAL;
    }
    
    /* Check for duplicates */
    if (g_history.dedup_enabled && g_history.count > 0) {
        history_entry_t *last = &g_history.entries[(g_history.count - 1) % g_history.max_count];
        if (strcmp(last->command, command) == 0) {
            return 0;  /* Skip duplicate */
        }
    }
    
    u32 idx = g_history.count % g_history.max_count;
    history_entry_t *entry = &g_history.entries[idx];
    
    /* Free old command if exists */
    if (entry->command) {
        kfree(entry->command);
    }
    
    /* Add new entry */
    entry->command = (char *)kmalloc(strlen(command) + 1);
    if (!entry->command) {
        return -ENOMEM;
    }
    
    strcpy(entry->command, command);
    entry->timestamp = 0;  /* Would get from kernel */
    entry->exit_code = exit_code;
    strncpy(entry->directory, directory ? directory : "/", 255);
    entry->duration_ms = 0;
    
    g_history.count++;
    g_history.current_pos = g_history.count;
    
    return 0;
}

/* Get Previous Command */
const char *history_prev(void)
{
    if (g_history.count == 0 || g_history.current_pos == 0) {
        return NULL;
    }
    
    g_history.current_pos--;
    u32 idx = g_history.current_pos % g_history.max_count;
    return g_history.entries[idx].command;
}

/* Get Next Command */
const char *history_next(void)
{
    if (g_history.count == 0 || g_history.current_pos >= g_history.count) {
        g_history.current_pos = g_history.count;
        return "";
    }
    
    g_history.current_pos++;
    u32 idx = g_history.current_pos % g_history.max_count;
    return g_history.entries[idx].command;
}

/* Search History */
int history_search(const char *query, u32 *results, u32 max_results)
{
    if (!query || !results) {
        return -EINVAL;
    }

    u32 found = 0;

    /* Search backwards from current position */
    if (g_history.current_pos > 0) {
        for (s32 i = (s32)g_history.current_pos - 1; i >= 0 && found < max_results; i--) {
            u32 idx = (u32)i % g_history.max_count;
            if (strstr(g_history.entries[idx].command, query) != NULL) {
                results[found++] = idx;
            }
        }
    }

    g_history.search_count = found;
    return (int)found;
}

/* Clear History */
int history_clear(void)
{
    for (u32 i = 0; i < g_history.max_count; i++) {
        if (g_history.entries[i].command) {
            kfree(g_history.entries[i].command);
            g_history.entries[i].command = NULL;
        }
    }
    
    g_history.count = 0;
    g_history.current_pos = 0;
    
    printk("[TERM-HISTORY] History cleared\n");
    
    return 0;
}

/* Print History */
void history_print(u32 count)
{
    printk("\n[TERM-HISTORY] ====== COMMAND HISTORY ======\n");
    
    u32 start = (count < g_history.count) ? g_history.count - count : 0;
    
    for (u32 i = start; i < g_history.count; i++) {
        u32 idx = i % g_history.max_count;
        history_entry_t *entry = &g_history.entries[idx];
        
        printk("[TERM-HISTORY] %5u  ", i + 1);
        
        if (g_history.timestamps_enabled) {
            printk("%lu  ", entry->timestamp);
        }
        
        printk("%s\n", entry->command);
    }
    
    printk("[TERM-HISTORY] Total: %u / %u\n", g_history.count, g_history.max_count);
    printk("[TERM-HISTORY] ==============================\n\n");
}

/*===========================================================================*/
/*                         ALIASES                                           */
/*===========================================================================*/

/* Initialize Alias Manager */
int aliases_init(u32 max_aliases)
{
    printk("[TERM-ALIASES] Initializing aliases (max: %u)\n", max_aliases);
    
    memset(&g_aliases, 0, sizeof(alias_manager_t));
    
    g_aliases.aliases = (alias_entry_t *)kmalloc(sizeof(alias_entry_t) * max_aliases);
    if (!g_aliases.aliases) {
        return -ENOMEM;
    }
    
    memset(g_aliases.aliases, 0, sizeof(alias_entry_t) * max_aliases);
    g_aliases.count = 0;
    g_aliases.max_count = max_aliases;
    g_aliases.save_to_file = true;
    strncpy(g_aliases.alias_file, "/root/.nexus_aliases", 255);
    
    /* Add default aliases */
    alias_add("ll", "ls -la", "List files in long format");
    alias_add("la", "ls -A", "List all files except . and ..");
    alias_add("l", "ls -CF", "List files with classification");
    alias_add("gs", "git status", "Git status");
    alias_add("ga", "git add", "Git add");
    alias_add("gc", "git commit", "Git commit");
    alias_add("gp", "git push", "Git push");
    alias_add("gl", "git log", "Git log");
    alias_add("upd", "update", "Update system");
    alias_add("inst", "install", "Install package");
    alias_add("rm", "rm -i", "Remove with confirmation");
    alias_add("mv", "mv -i", "Move with confirmation");
    alias_add("cp", "cp -i", "Copy with confirmation");
    alias_add("..", "cd ..", "Go up one directory");
    alias_add("...", "cd ../..", "Go up two directories");
    alias_add("h", "history", "Show history");
    alias_add("help", "help", "Show help");
    alias_add("cls", "clear", "Clear screen");
    
    printk("[TERM-ALIASES] Aliases initialized (%u defaults)\n", g_aliases.count);
    
    return 0;
}

/* Add Alias */
int alias_add(const char *name, const char *value, const char *description)
{
    if (!name || !value || g_aliases.count >= g_aliases.max_count) {
        return -EINVAL;
    }
    
    /* Check if alias exists */
    for (u32 i = 0; i < g_aliases.count; i++) {
        if (strcmp(g_aliases.aliases[i].name, name) == 0) {
            /* Update existing */
            strncpy(g_aliases.aliases[i].value, value, MAX_ALIAS_VALUE - 1);
            if (description) {
                strncpy(g_aliases.aliases[i].description, description, 255);
            }
            g_aliases.aliases[i].usage_count = 0;
            printk("[TERM-ALIASES] Updated alias: %s\n", name);
            return 0;
        }
    }
    
    /* Add new alias */
    alias_entry_t *alias = &g_aliases.aliases[g_aliases.count++];
    strncpy(alias->name, name, 63);
    strncpy(alias->value, value, MAX_ALIAS_VALUE - 1);
    if (description) {
        strncpy(alias->description, description, 255);
    }
    alias->global = false;
    alias->created = 0;  /* Would get from kernel */
    alias->usage_count = 0;
    
    printk("[TERM-ALIASES] Added alias: %s = %s\n", name, value);
    
    return 0;
}

/* Remove Alias */
int alias_remove(const char *name)
{
    if (!name) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < g_aliases.count; i++) {
        if (strcmp(g_aliases.aliases[i].name, name) == 0) {
            /* Shift remaining aliases */
            for (u32 j = i; j < g_aliases.count - 1; j++) {
                memcpy(&g_aliases.aliases[j], &g_aliases.aliases[j + 1], sizeof(alias_entry_t));
            }
            g_aliases.count--;
            
            printk("[TERM-ALIASES] Removed alias: %s\n", name);
            return 0;
        }
    }
    
    return -ENOENT;
}

/* Get Alias Value */
const char *alias_get(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    for (u32 i = 0; i < g_aliases.count; i++) {
        if (strcmp(g_aliases.aliases[i].name, name) == 0) {
            g_aliases.aliases[i].usage_count++;
            return g_aliases.aliases[i].value;
        }
    }
    
    return NULL;
}

/* Check if Command is Alias */
bool alias_is_alias(const char *command)
{
    return alias_get(command) != NULL;
}

/* Expand Alias in Command */
int alias_expand(char *command, u32 max_len)
{
    if (!command) {
        return -EINVAL;
    }
    
    /* Get first word */
    char first_word[64];
    u32 i = 0;
    while (command[i] && command[i] != ' ' && i < 63) {
        first_word[i] = command[i];
        i++;
    }
    first_word[i] = '\0';
    
    /* Check if it's an alias */
    const char *alias_value = alias_get(first_word);
    if (!alias_value) {
        return 0;  /* Not an alias */
    }
    
    /* Replace command with alias value */
    char rest[max_len];
    strncpy(rest, command + i, max_len - 1);
    
    snprintf(command, max_len, "%s%s", alias_value, rest);
    
    return 1;  /* Was expanded */
}

/* List Aliases */
void aliases_list(void)
{
    printk("\n[TERM-ALIASES] ====== ALIASES ======\n");
    printk("[TERM-ALIASES] Total: %u / %u\n", g_aliases.count, g_aliases.max_count);
    
    for (u32 i = 0; i < g_aliases.count; i++) {
        alias_entry_t *alias = &g_aliases.aliases[i];
        printk("[TERM-ALIASES] %-12s = %s\n", alias->name, alias->value);
        if (alias->description[0]) {
            printk("[TERM-ALIASES]                # %s\n", alias->description);
        }
    }
    
    printk("[TERM-ALIASES] =======================\n\n");
}

/*===========================================================================*/
/*                         EMULATIONS                                        */
/*===========================================================================*/

/* Initialize Emulation Modes */
int emulations_init(void)
{
    printk("[TERM-EMULATION] Initializing emulation modes\n");
    
    memset(&g_emulations, 0, sizeof(emulation_manager_t));
    
    g_emulations.modes = (emulation_mode_t *)kmalloc(sizeof(emulation_mode_t) * MAX_EMULATIONS);
    if (!g_emulations.modes) {
        return -ENOMEM;
    }
    
    memset(g_emulations.modes, 0, sizeof(emulation_mode_t) * MAX_EMULATIONS);
    g_emulations.count = 0;
    g_emulations.current_mode = 0;
    strncpy(g_emulations.config_file, "/root/.nexus_emulation", 255);
    
    /* Add bash emulation */
    emulation_mode_t *bash = &g_emulations.modes[g_emulations.count++];
    strncpy(bash->name, "bash", 31);
    strncpy(bash->description, "Bash shell emulation", 127);
    strncpy(bash->prompt_template, "\\u@\\h:\\w$ ", 63);
    bash->color_scheme = 0;
    bash->case_sensitive = true;
    bash->expand_vars = true;
    
    /* Add sh emulation */
    emulation_mode_t *sh = &g_emulations.modes[g_emulations.count++];
    strncpy(sh->name, "sh", 31);
    strncpy(sh->description, "Bourne shell emulation", 127);
    strncpy(sh->prompt_template, "$ ", 63);
    sh->color_scheme = 0;
    sh->case_sensitive = true;
    sh->expand_vars = true;
    
    /* Add cmd (Windows) emulation */
    emulation_mode_t *cmd = &g_emulations.modes[g_emulations.count++];
    strncpy(cmd->name, "cmd", 31);
    strncpy(cmd->description, "Windows CMD emulation", 127);
    strncpy(cmd->prompt_template, "C:\\>", 63);
    cmd->color_scheme = 1;
    cmd->case_sensitive = false;
    cmd->expand_vars = false;
    
    /* Add powershell emulation */
    emulation_mode_t *ps = &g_emulations.modes[g_emulations.count++];
    strncpy(ps->name, "powershell", 31);
    strncpy(ps->description, "PowerShell emulation", 127);
    strncpy(ps->prompt_template, "PS C:\\>", 63);
    ps->color_scheme = 2;
    ps->case_sensitive = false;
    ps->expand_vars = true;
    
    printk("[TERM-EMULATION] Emulation modes initialized (%u modes)\n", g_emulations.count);
    
    return 0;
}

/* Set Emulation Mode */
int emulation_set(const char *name)
{
    for (u32 i = 0; i < g_emulations.count; i++) {
        if (strcmp(g_emulations.modes[i].name, name) == 0) {
            g_emulations.current_mode = i;
            printk("[TERM-EMULATION] Switched to %s mode\n", name);
            printk("[TERM-EMULATION] Prompt: %s\n", g_emulations.modes[i].prompt_template);
            return 0;
        }
    }
    
    return -ENOENT;
}

/* Get Current Emulation */
const char *emulation_get_current(void)
{
    if (g_emulations.count > 0 && g_emulations.current_mode < g_emulations.count) {
        return g_emulations.modes[g_emulations.current_mode].name;
    }
    return "unknown";
}

/* List Emulation Modes */
void emulations_list(void)
{
    printk("\n[TERM-EMULATION] ====== EMULATION MODES ======\n");

    for (u32 i = 0; i < g_emulations.count; i++) {
        emulation_mode_t *mode = &g_emulations.modes[i];
        const char *curr_marker = (i == g_emulations.current_mode) ? " (current)" : "";
        printk("[TERM-EMULATION] %u. %s%s\n", i + 1, mode->name, curr_marker);
        printk("[TERM-EMULATION]    %s\n", mode->description);
        printk("[TERM-EMULATION]    Prompt: %s\n", mode->prompt_template);
    }

    printk("[TERM-EMULATION] =============================\n\n");
}

/*===========================================================================*/
/*                         HELP SYSTEM                                       */
/*===========================================================================*/

/* Initialize Help System */
int help_init(void)
{
    printk("[TERM-HELP] Initializing help system\n");
    
    memset(&g_help, 0, sizeof(help_system_t));
    
    g_help.topics = (help_topic_t *)kmalloc(sizeof(help_topic_t) * MAX_HELP_TOPICS);
    if (!g_help.topics) {
        return -ENOMEM;
    }
    
    memset(g_help.topics, 0, sizeof(help_topic_t) * MAX_HELP_TOPICS);
    g_help.count = 0;
    g_help.max_count = MAX_HELP_TOPICS;
    g_help.color_enabled = true;
    g_help.display_width = 80;
    g_help.display_height = 24;
    
    /* Add help topics */
    help_add("sudo", "Execute command as another user",
             "sudo [options] command\n\n"
             "Execute a command as another user (default: root).\n\n"
             "Options:\n"
             "  -u user    Run as specified user\n"
             "  -i         Run login shell\n"
             "  -s         Run shell\n"
             "  -k         Reset timestamp\n\n"
             "Examples:\n"
             "  sudo ls /root\n"
             "  sudo -u www-data service apache2 restart",
             "sudo -l, visudo");
    
    help_add("history", "Show command history",
             "history [options]\n\n"
             "Display or manipulate command history.\n\n"
             "Options:\n"
             "  -c         Clear history\n"
             "  -d offset  Delete entry at offset\n"
             "  -a         Append to history file\n"
             "  -r         Read history file\n"
             "  -w         Write history file\n\n"
             "Examples:\n"
             "  history           # Show all history\n"
             "  history 20        # Show last 20 commands\n"
             "  history -c        # Clear history",
             "alias, savehist");
    
    help_add("alias", "Create command aliases",
             "alias [name[=value]]\n\n"
             "Define or display command aliases.\n\n"
             "Examples:\n"
             "  alias ll='ls -la'     # Create alias\n"
             "  alias                 # List all aliases\n"
             "  unalias ll            # Remove alias",
             "unalias, history");
    
    help_add("clear", "Clear terminal screen",
             "clear\n\n"
             "Clear the terminal screen and scrollback.\n\n"
             "Shortcut: Ctrl+L",
             "reset, cls");
    
    help_add("cd", "Change directory",
             "cd [directory]\n\n"
             "Change the current working directory.\n\n"
             "Examples:\n"
             "  cd /home          # Go to /home\n"
             "  cd ..             # Go up one level\n"
             "  cd ~              # Go to home directory\n"
             "  cd -              # Go to previous directory",
             "pwd, ls, pushd, popd");
    
    help_add("ls", "List directory contents",
             "ls [options] [path]\n\n"
             "List information about files and directories.\n\n"
             "Options:\n"
             "  -l    Long format\n"
             "  -a    Show hidden files\n"
             "  -h    Human readable sizes\n"
             "  -R    Recursive\n"
             "  -t    Sort by time\n\n"
             "Examples:\n"
             "  ls -la            # List all files in long format\n"
             "  ls -lh /var       # List /var with human sizes",
             "dir, vdir, ll");
    
    help_add("cat", "Concatenate and display files",
             "cat [options] [file...]\n\n"
             "Read and display file contents.\n\n"
             "Options:\n"
             "  -n    Number lines\n"
             "  -E    Show line ends\n"
             "  -T    Show tabs\n\n"
             "Examples:\n"
             "  cat file.txt      # Display file\n"
             "  cat -n file.txt   # Display with line numbers",
             "less, more, head, tail");
    
    help_add("grep", "Search text patterns",
             "grep [options] pattern [file...]\n\n"
             "Search for patterns in text using regular expressions.\n\n"
             "Options:\n"
             "  -i    Case insensitive\n"
             "  -r    Recursive\n"
             "  -n    Show line numbers\n"
             "  -v    Invert match\n"
             "  -c    Count matches\n\n"
             "Examples:\n"
             "  grep 'error' log.txt\n"
             "  grep -ri 'TODO' src/",
             "egrep, fgrep, sed, awk");
    
    help_add("ps", "Display process status",
             "ps [options]\n\n"
             "Display information about running processes.\n\n"
             "Options:\n"
             "  -a    All processes\n"
             "  -u    User format\n"
             "  -x    Include processes without terminal\n"
             "  -f    Full format\n\n"
             "Examples:\n"
             "  ps aux            # All processes detailed\n"
             "  ps -ef            # Full format",
             "top, htop, kill, pgrep");
    
    help_add("kill", "Terminate processes",
             "kill [options] pid...\n\n"
             "Send signals to processes.\n\n"
             "Options:\n"
             "  -l    List signals\n"
             "  -9    SIGKILL (force)\n"
             "  -15   SIGTERM (default)\n\n"
             "Examples:\n"
             "  kill 1234         # Send SIGTERM\n"
             "  kill -9 1234      # Force kill",
             "killall, pkill, signal");
    
    printk("[TERM-HELP] Help system initialized (%u topics)\n", g_help.count);
    
    return 0;
}

/* Add Help Topic */
int help_add(const char *command, const char *short_desc, const char *full_desc, const char *see_also)
{
    if (!command || !short_desc || g_help.count >= g_help.max_count) {
        return -EINVAL;
    }
    
    help_topic_t *topic = &g_help.topics[g_help.count++];
    strncpy(topic->command, command, 63);
    strncpy(topic->short_desc, short_desc, 127);
    strncpy(topic->full_desc, full_desc, MAX_HELP_TEXT - 1);
    strncpy(topic->see_also, see_also, 255);
    
    return 0;
}

/* Show Help for Command */
int help_show(const char *command)
{
    if (!command) {
        /* Show all topics */
        printk("\n[TERM-HELP] ====== AVAILABLE COMMANDS ======\n");
        for (u32 i = 0; i < g_help.count; i++) {
            printk("[TERM-HELP] %-16s - %s\n",
                   g_help.topics[i].command, g_help.topics[i].short_desc);
        }
        printk("[TERM-HELP] Total: %u commands\n", g_help.count);
        printk("[TERM-HELP] Use 'help <command>' for details\n");
        printk("[TERM-HELP] ==================================\n\n");
        return 0;
    }
    
    /* Find specific topic */
    for (u32 i = 0; i < g_help.count; i++) {
        if (strcmp(g_help.topics[i].command, command) == 0) {
            help_topic_t *topic = &g_help.topics[i];
            
            printk("\n");
            printk("═══════════════════════════════════════════════════════\n");
            printk("  %s\n", topic->command);
            printk("═══════════════════════════════════════════════════════\n\n");
            printk("  %s\n\n", topic->short_desc);
            printk("  SYNOPSIS:\n");
            printk("  ─────────\n");
            printk("  %s\n\n", topic->syntax[0] ? topic->syntax : topic->command);
            printk("  DESCRIPTION:\n");
            printk("  ────────────\n");
            printk("  %s\n\n", topic->full_desc);
            
            if (topic->examples[0]) {
                printk("  EXAMPLES:\n");
                printk("  ─────────\n");
                printk("  %s\n\n", topic->examples);
            }
            
            if (topic->see_also[0]) {
                printk("  SEE ALSO:\n");
                printk("  ─────────\n");
                printk("  %s\n\n", topic->see_also);
            }
            
            printk("═══════════════════════════════════════════════════════\n\n");
            
            return 0;
        }
    }
    
    printk("[TERM-HELP] No help found for '%s'\n", command);
    printk("[TERM-HELP] Use 'help' to see all commands\n");
    
    return -ENOENT;
}

/* Search Help */
int help_search(const char *query)
{
    if (!query) {
        return -EINVAL;
    }
    
    printk("\n[TERM-HELP] Search results for '%s':\n", query);
    printk("────────────────────────────────────────\n");
    
    u32 found = 0;
    for (u32 i = 0; i < g_help.count; i++) {
        help_topic_t *topic = &g_help.topics[i];
        if (strstr(topic->command, query) ||
            strstr(topic->short_desc, query) ||
            strstr(topic->full_desc, query)) {
            printk("  %-16s - %s\n", topic->command, topic->short_desc);
            found++;
        }
    }
    
    if (found == 0) {
        printk("  No results found\n");
    } else {
        printk("────────────────────────────────────────\n");
        printk("  Found %u topic(s)\n", found);
    }
    printk("\n");
    
    return found;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int terminal_features_init(void)
{
    printk("[TERMINAL] ========================================\n");
    printk("[TERMINAL] NEXUS OS Terminal Features\n");
    printk("[TERMINAL] ========================================\n");
    
    /* Initialize history */
    history_init(MAX_HISTORY);
    
    /* Initialize aliases */
    aliases_init(MAX_ALIASES);
    
    /* Initialize emulations */
    emulations_init();
    
    /* Initialize help */
    help_init();
    
    printk("[TERMINAL] All features initialized\n");
    printk("[TERMINAL] ========================================\n");
    
    return 0;
}

/* Print Terminal Features Status */
void terminal_features_status(void)
{
    printk("\n[TERMINAL] ====== FEATURES STATUS ======\n");
    printk("[TERMINAL] History: %u / %u entries\n", g_history.count, g_history.max_count);
    printk("[TERMINAL] Aliases: %u / %u defined\n", g_aliases.count, g_aliases.max_count);
    printk("[TERMINAL] Emulations: %u modes\n", g_emulations.count);
    printk("[TERMINAL] Help: %u topics\n", g_help.count);
    printk("[TERMINAL] Current emulation: %s\n", emulation_get_current());
    printk("[TERMINAL] =============================\n\n");
}
