/*
 * NEXUS OS - User Feedback System
 * system/feedback/feedback_manager.c
 *
 * Feedback mechanism for users to send bug reports, feature requests, and suggestions to developers
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         FEEDBACK CONFIGURATION                            */
/*===========================================================================*/

#define FEEDBACK_MAX_ENTRIES        1000
#define FEEDBACK_MAX_ATTACHMENTS    10
#define FEEDBACK_MAX_COMMENT_LENGTH 4096
#define FEEDBACK_CATEGORIES_MAX     32

/* Feedback Types */
#define FEEDBACK_TYPE_BUG_REPORT        0
#define FEEDBACK_TYPE_FEATURE_REQUEST   1
#define FEEDBACK_TYPE_SUGGESTION        2
#define FEEDBACK_TYPE_FEEDBACK          3
#define FEEDBACK_TYPE_UPDATE_REQUEST    4
#define FEEDBACK_TYPE_UPGRADE_REQUEST   5
#define FEEDBACK_TYPE_COMPATIBILITY     6
#define FEEDBACK_TYPE_PERFORMANCE       7

/* Feedback Priority */
#define FEEDBACK_PRIORITY_LOW           0
#define FEEDBACK_PRIORITY_MEDIUM        1
#define FEEDBACK_PRIORITY_HIGH          2
#define FEEDBACK_PRIORITY_CRITICAL      3

/* Feedback Status */
#define FEEDBACK_STATUS_NEW             0
#define FEEDBACK_STATUS_ACKNOWLEDGED    1
#define FEEDBACK_STATUS_IN_REVIEW       2
#define FEEDBACK_STATUS_ACCEPTED        3
#define FEEDBACK_STATUS_PLANNED         4
#define FEEDBACK_STATUS_IN_PROGRESS     5
#define FEEDBACK_STATUS_RESOLVED        6
#define FEEDBACK_STATUS_CLOSED          7
#define FEEDBACK_STATUS_REJECTED        8

/* Feedback Categories */
#define FEEDBACK_CAT_SYSTEM             0
#define FEEDBACK_CAT_GUI                1
#define FEEDBACK_CAT_TERMINAL           2
#define FEEDBACK_CAT_VIRTUALIZATION     3
#define FEEDBACK_CAT_AUDIO              4
#define FEEDBACK_CAT_NETWORK            5
#define FEEDBACK_CAT_SECURITY           6
#define FEEDBACK_CAT_PERFORMANCE        7
#define FEEDBACK_CAT_COMPATIBILITY      8
#define FEEDBACK_CAT_DOCUMENTATION      9

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Feedback Attachment */
typedef struct {
    char filename[128];
    char path[256];
    u32 size_bytes;
    u32 type;  /* 0=log, 1=screenshot, 2=config, 3=other */
    u64 created_time;
} feedback_attachment_t;

/* Feedback Entry */
typedef struct {
    u32 id;
    u32 type;
    u32 priority;
    u32 status;
    u32 category;
    
    /* User Info */
    u32 user_id;
    char username[64];
    char email[128];
    
    /* Content */
    char title[128];
    char description[FEEDBACK_MAX_COMMENT_LENGTH];
    char steps_to_reproduce[1024];
    char expected_behavior[512];
    char actual_behavior[512];
    
    /* System Info */
    char os_version[32];
    char kernel_version[32];
    char hardware_info[256];
    
    /* Attachments */
    feedback_attachment_t attachments[FEEDBACK_MAX_ATTACHMENTS];
    u32 attachment_count;
    
    /* Log References */
    char log_files[10][256];
    u32 log_file_count;
    
    /* Timestamps */
    u64 created_time;
    u64 updated_time;
    
    /* Developer Response */
    char developer_response[1024];
    char response_author[64];
    u64 response_time;
    
    /* Voting */
    u32 upvotes;
    u32 downvotes;
    
    /* Metadata */
    bool anonymous;
    bool allow_contact;
    bool include_logs;
    u32 session_id;
} feedback_entry_t;

/* Feedback Category */
typedef struct {
    u32 id;
    char name[64];
    char description[256];
    u32 feedback_count;
    bool enabled;
} feedback_category_t;

/* Feedback Statistics */
typedef struct {
    u32 total_submissions;
    u32 bug_reports;
    u32 feature_requests;
    u32 suggestions;
    u32 by_priority[4];
    u32 by_status[9];
    u32 by_category[10];
    u32 resolved_count;
    u32 avg_resolution_time_hours;
    u32 user_satisfaction_percent;
} feedback_statistics_t;

/* Feedback Manager */
typedef struct {
    bool initialized;
    
    /* Feedback Entries */
    feedback_entry_t *entries;
    u32 entry_count;
    u32 max_entries;
    u32 next_id;
    
    /* Categories */
    feedback_category_t categories[FEEDBACK_CATEGORIES_MAX];
    u32 category_count;
    
    /* Statistics */
    feedback_statistics_t stats;
    
    /* Settings */
    bool auto_include_logs;
    bool allow_anonymous;
    bool email_notifications;
    bool developer_responses;
    char developer_email[128];
    char feedback_server[256];
    
    /* Log Integration */
    bool attach_system_logs;
    bool attach_session_logs;
    u32 max_log_size_kb;
    
    /* Submission Queue */
    feedback_entry_t *submission_queue;
    u32 queue_count;
    bool submitting;
    
    spinlock_t lock;
} feedback_manager_t;

static feedback_manager_t g_feedback;

/*===========================================================================*/
/*                         FEEDBACK CATEGORIES                               */
/*===========================================================================*/

/* Initialize Feedback Categories */
void feedback_init_categories(void)
{
    printk("[FEEDBACK] Initializing categories...\n");
    
    feedback_category_t *cat;
    
    /* System */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_SYSTEM;
    strncpy(cat->name, "System", 63);
    strncpy(cat->description, "Core system issues and improvements", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* GUI */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_GUI;
    strncpy(cat->name, "Graphical Interface", 63);
    strncpy(cat->description, "Desktop environment, themes, icons, display", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Terminal */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_TERMINAL;
    strncpy(cat->name, "Terminal", 63);
    strncpy(cat->description, "Terminal emulator, shell, commands", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Virtualization */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_VIRTUALIZATION;
    strncpy(cat->name, "Virtualization", 63);
    strncpy(cat->description, "VMs, containers, hypervisor", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Audio */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_AUDIO;
    strncpy(cat->name, "Audio", 63);
    strncpy(cat->description, "Sound system, equalizer, effects", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Network */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_NETWORK;
    strncpy(cat->name, "Network", 63);
    strncpy(cat->description, "Networking, connectivity, protocols", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Security */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_SECURITY;
    strncpy(cat->name, "Security", 63);
    strncpy(cat->description, "Security features, vulnerabilities", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Performance */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_PERFORMANCE;
    strncpy(cat->name, "Performance", 63);
    strncpy(cat->description, "Speed, optimization, resource usage", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Compatibility */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_COMPATIBILITY;
    strncpy(cat->name, "Compatibility", 63);
    strncpy(cat->description, "Hardware/software compatibility", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    /* Documentation */
    cat = &g_feedback.categories[g_feedback.category_count++];
    cat->id = FEEDBACK_CAT_DOCUMENTATION;
    strncpy(cat->name, "Documentation", 63);
    strncpy(cat->description, "Docs, guides, help content", 255);
    cat->feedback_count = 0;
    cat->enabled = true;
    
    printk("[FEEDBACK] %u categories initialized\n", g_feedback.category_count);
}

/*===========================================================================*/
/*                         FEEDBACK SUBMISSION                               */
/*===========================================================================*/

/* Create Feedback Entry */
u32 feedback_create(u32 type, u32 priority, u32 category, const char *title, const char *description)
{
    if (g_feedback.entry_count >= g_feedback.max_entries) {
        printk("[FEEDBACK] ERROR: Maximum feedback entries reached\n");
        return 0;
    }
    
    feedback_entry_t *entry = &g_feedback.entries[g_feedback.entry_count++];
    memset(entry, 0, sizeof(feedback_entry_t));
    
    entry->id = ++g_feedback.next_id;
    entry->type = type;
    entry->priority = priority;
    entry->status = FEEDBACK_STATUS_NEW;
    entry->category = category;
    entry->created_time = 0;  /* Would get from kernel */
    entry->updated_time = 0;
    entry->upvotes = 0;
    entry->downvotes = 0;
    entry->anonymous = false;
    entry->allow_contact = true;
    entry->include_logs = g_feedback.auto_include_logs;
    entry->session_id = 0;  /* Would get from session manager */
    
    strncpy(entry->title, title, 127);
    strncpy(entry->description, description, FEEDBACK_MAX_COMMENT_LENGTH - 1);
    
    /* Get system info */
    strncpy(entry->os_version, "1.0.0", 31);
    strncpy(entry->kernel_version, "1.0.0", 31);
    strncpy(entry->hardware_info, "x86_64 QEMU VM", 255);
    
    /* Auto-attach logs if enabled */
    if (entry->include_logs && g_feedback.attach_system_logs) {
        feedback_attach_logs(entry);
    }
    
    /* Update category count */
    if (category < g_feedback.category_count) {
        g_feedback.categories[category].feedback_count++;
    }
    
    /* Update statistics */
    g_feedback.stats.total_submissions++;
    if (type == FEEDBACK_TYPE_BUG_REPORT) g_feedback.stats.bug_reports++;
    else if (type == FEEDBACK_TYPE_FEATURE_REQUEST) g_feedback.stats.feature_requests++;
    else if (type == FEEDBACK_TYPE_SUGGESTION) g_feedback.stats.suggestions++;
    
    g_feedback.stats.by_priority[priority]++;
    g_feedback.stats.by_status[FEEDBACK_STATUS_NEW]++;
    g_feedback.stats.by_category[category]++;
    
    printk("[FEEDBACK] Created feedback #%u: %s\n", entry->id, title);
    
    /* Add to submission queue */
    feedback_queue_for_submission(entry);
    
    return entry->id;
}

/* Attach System Logs */
void feedback_attach_logs(feedback_entry_t *entry)
{
    printk("[FEEDBACK] Attaching system logs to feedback #%u...\n", entry->id);
    
    /* Attach recent system log */
    if (entry->log_file_count < 10) {
        strncpy(entry->log_files[entry->log_file_count++], "/var/log/system.log", 255);
    }
    
    /* Attach kernel log */
    if (entry->log_file_count < 10) {
        strncpy(entry->log_files[entry->log_file_count++], "/var/log/kernel.log", 255);
    }
    
    /* Attach boot log */
    if (entry->log_file_count < 10) {
        strncpy(entry->log_files[entry->log_file_count++], "/var/log/boot.log", 255);
    }
    
    /* Attach session log if available */
    if (g_feedback.attach_session_logs && entry->session_id > 0) {
        char session_log[256];
        snprintf(session_log, 256, "/var/log/sessions/session_%u.log", entry->session_id);
        if (entry->log_file_count < 10) {
            strncpy(entry->log_files[entry->log_file_count++], session_log, 255);
        }
    }
    
    printk("[FEEDBACK] Attached %u log files\n", entry->log_file_count);
}

/* Queue Feedback for Submission */
void feedback_queue_for_submission(feedback_entry_t *entry)
{
    if (!g_feedback.submission_queue) {
        return;
    }
    
    memcpy(&g_feedback.submission_queue[g_feedback.queue_count++], entry, sizeof(feedback_entry_t));
    g_feedback.submitting = true;
    
    printk("[FEEDBACK] Queued feedback #%u for submission\n", entry->id);
}

/* Submit Feedback to Developer */
int feedback_submit_to_developer(u32 feedback_id)
{
    for (u32 i = 0; i < g_feedback.entry_count; i++) {
        feedback_entry_t *entry = &g_feedback.entries[i];
        if (entry->id == feedback_id) {
            printk("[FEEDBACK] Submitting feedback #%u to developer...\n", feedback_id);
            printk("[FEEDBACK] Type: %u, Priority: %u, Category: %u\n",
                   entry->type, entry->priority, entry->category);
            printk("[FEEDBACK] Title: %s\n", entry->title);
            printk("[FEEDBACK] Logs attached: %u files\n", entry->log_file_count);
            
            /* In production, this would send to a server */
            /* send_to_server(g_feedback.feedback_server, entry); */
            
            entry->status = FEEDBACK_STATUS_ACKNOWLEDGED;
            g_feedback.stats.by_status[FEEDBACK_STATUS_ACKNOWLEDGED]++;
            g_feedback.stats.by_status[FEEDBACK_STATUS_NEW]--;
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         FEEDBACK TYPES                                    */
/*===========================================================================*/

/* Submit Bug Report */
u32 feedback_submit_bug(const char *title, const char *description, const char *steps,
                        const char *expected, const char *actual, u32 priority)
{
    u32 id = feedback_create(FEEDBACK_TYPE_BUG_REPORT, priority, FEEDBACK_CAT_SYSTEM, title, description);
    
    if (id > 0) {
        feedback_entry_t *entry = &g_feedback.entries[g_feedback.entry_count - 1];
        strncpy(entry->steps_to_reproduce, steps, 1023);
        strncpy(entry->expected_behavior, expected, 511);
        strncpy(entry->actual_behavior, actual, 511);
    }
    
    return id;
}

/* Submit Feature Request */
u32 feedback_submit_feature(const char *title, const char *description, u32 category)
{
    return feedback_create(FEEDBACK_TYPE_FEATURE_REQUEST, FEEDBACK_PRIORITY_MEDIUM,
                          category, title, description);
}

/* Submit Suggestion */
u32 feedback_submit_suggestion(const char *title, const char *description)
{
    return feedback_create(FEEDBACK_TYPE_SUGGESTION, FEEDBACK_PRIORITY_LOW,
                          FEEDBACK_CAT_SYSTEM, title, description);
}

/* Submit Update Request */
u32 feedback_submit_update_request(const char *component, const char *reason)
{
    char title[256];
    snprintf(title, 256, "Update Request: %s", component);
    
    return feedback_create(FEEDBACK_TYPE_UPDATE_REQUEST, FEEDBACK_PRIORITY_MEDIUM,
                          FEEDBACK_CAT_SYSTEM, title, reason);
}

/* Submit Upgrade Request */
u32 feedback_submit_upgrade_request(const char *component, const char *reason)
{
    char title[256];
    snprintf(title, 256, "Upgrade Request: %s", component);
    
    return feedback_create(FEEDBACK_TYPE_UPGRADE_REQUEST, FEEDBACK_PRIORITY_MEDIUM,
                          FEEDBACK_CAT_SYSTEM, title, reason);
}

/*===========================================================================*/
/*                         DEVELOPER RESPONSE                                */
/*===========================================================================*/

/* Add Developer Response */
int feedback_add_response(u32 feedback_id, const char *response, const char *author)
{
    for (u32 i = 0; i < g_feedback.entry_count; i++) {
        feedback_entry_t *entry = &g_feedback.entries[i];
        if (entry->id == feedback_id) {
            strncpy(entry->developer_response, response, 1023);
            strncpy(entry->response_author, author, 63);
            entry->response_time = 0;  /* Would get from kernel */
            entry->status = FEEDBACK_STATUS_IN_REVIEW;
            
            printk("[FEEDBACK] Developer response added to feedback #%u by %s\n",
                   feedback_id, author);
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/* Vote on Feedback */
void feedback_vote(u32 feedback_id, bool upvote)
{
    for (u32 i = 0; i < g_feedback.entry_count; i++) {
        feedback_entry_t *entry = &g_feedback.entries[i];
        if (entry->id == feedback_id) {
            if (upvote) {
                entry->upvotes++;
            } else {
                entry->downvotes++;
            }
            return;
        }
    }
}

/*===========================================================================*/
/*                         FEEDBACK VIEWER                                   */
/*===========================================================================*/

/* Show Feedback Statistics */
void feedback_show_statistics(void)
{
    feedback_statistics_t *stats = &g_feedback.stats;
    
    printk("\n[FEEDBACK] ====== FEEDBACK STATISTICS ======\n");
    printk("[FEEDBACK] Total Submissions: %u\n", stats->total_submissions);
    printk("[FEEDBACK]   Bug Reports: %u\n", stats->bug_reports);
    printk("[FEEDBACK]   Feature Requests: %u\n", stats->feature_requests);
    printk("[FEEDBACK]   Suggestions: %u\n", stats->suggestions);
    
    printk("[FEEDBACK]\nBy Priority:\n");
    printk("[FEEDBACK]   Low: %u\n", stats->by_priority[FEEDBACK_PRIORITY_LOW]);
    printk("[FEEDBACK]   Medium: %u\n", stats->by_priority[FEEDBACK_PRIORITY_MEDIUM]);
    printk("[FEEDBACK]   High: %u\n", stats->by_priority[FEEDBACK_PRIORITY_HIGH]);
    printk("[FEEDBACK]   Critical: %u\n", stats->by_priority[FEEDBACK_PRIORITY_CRITICAL]);
    
    printk("[FEEDBACK]\nBy Status:\n");
    const char *status_names[] = {
        "New", "Acknowledged", "In Review", "Accepted",
        "Planned", "In Progress", "Resolved", "Closed", "Rejected"
    };
    for (u32 i = 0; i < 9; i++) {
        printk("[FEEDBACK]   %s: %u\n", status_names[i], stats->by_status[i]);
    }
    
    printk("[FEEDBACK]\nBy Category:\n");
    for (u32 i = 0; i < g_feedback.category_count; i++) {
        printk("[FEEDBACK]   %s: %u\n",
               g_feedback.categories[i].name,
               stats->by_category[i]);
    }
    
    printk("[FEEDBACK]\nResolution:\n");
    printk("[FEEDBACK]   Resolved: %u\n", stats->resolved_count);
    printk("[FEEDBACK]   Avg Resolution Time: %u hours\n", stats->avg_resolution_time_hours);
    printk("[FEEDBACK]   User Satisfaction: %u%%\n", stats->user_satisfaction_percent);
    printk("[FEEDBACK] =================================\n\n");
}

/* Show Recent Feedback */
void feedback_show_recent(u32 count)
{
    printk("\n[FEEDBACK] ====== RECENT FEEDBACK (last %u) ======\n", count);
    
    const char *type_names[] = {
        "Bug Report", "Feature Request", "Suggestion",
        "Feedback", "Update Request", "Upgrade Request",
        "Compatibility", "Performance"
    };
    
    const char *priority_names[] = { "Low", "Medium", "High", "Critical" };
    const char *status_names[] = {
        "New", "Acknowledged", "In Review", "Accepted",
        "Planned", "In Progress", "Resolved", "Closed", "Rejected"
    };
    
    u32 start = (g_feedback.entry_count > count) ? g_feedback.entry_count - count : 0;
    
    for (u32 i = start; i < g_feedback.entry_count; i++) {
        feedback_entry_t *entry = &g_feedback.entries[i];
        
        printk("[FEEDBACK] #%u: %s\n", entry->id, entry->title);
        printk("[FEEDBACK]   Type: %s, Priority: %s, Status: %s\n",
               type_names[entry->type],
               priority_names[entry->priority],
               status_names[entry->status]);
        printk("[FEEDBACK]   Category: %s\n",
               g_feedback.categories[entry->category].name);
        printk("[FEEDBACK]   Votes: +%u/-%u\n", entry->upvotes, entry->downvotes);
        printk("[FEEDBACK]   Logs: %u files\n", entry->log_file_count);
        if (entry->developer_response[0] != '\0') {
            printk("[FEEDBACK]   Response by %s: %s\n",
                   entry->response_author, entry->developer_response);
        }
        printk("[FEEDBACK]\n");
    }
    
    printk("[FEEDBACK] =========================================\n\n");
}

/* Show Feedback by Category */
void feedback_show_by_category(u32 category_id)
{
    if (category_id >= g_feedback.category_count) {
        return;
    }
    
    printk("\n[FEEDBACK] ====== %s FEEDBACK ======\n", g_feedback.categories[category_id].name);
    
    u32 count = 0;
    for (u32 i = 0; i < g_feedback.entry_count; i++) {
        feedback_entry_t *entry = &g_feedback.entries[i];
        if (entry->category == category_id) {
            printk("[FEEDBACK] #%u: %s [%s]\n",
                   entry->id, entry->title,
                   entry->priority == FEEDBACK_PRIORITY_CRITICAL ? "CRITICAL" :
                   entry->priority == FEEDBACK_PRIORITY_HIGH ? "HIGH" : "NORMAL");
            count++;
        }
    }
    
    printk("[FEEDBACK] Total: %u entries\n", count);
    printk("[FEEDBACK] =================================\n\n");
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/* Initialize Feedback Manager */
int feedback_manager_init(void)
{
    printk("[FEEDBACK] ========================================\n");
    printk("[FEEDBACK] NEXUS OS User Feedback System\n");
    printk("[FEEDBACK] ========================================\n");
    
    memset(&g_feedback, 0, sizeof(feedback_manager_t));
    g_feedback.lock.lock = 0;
    
    /* Allocate feedback entries */
    g_feedback.max_entries = FEEDBACK_MAX_ENTRIES;
    g_feedback.entries = (feedback_entry_t *)kmalloc(sizeof(feedback_entry_t) * FEEDBACK_MAX_ENTRIES);
    if (!g_feedback.entries) {
        printk("[FEEDBACK] ERROR: Failed to allocate feedback entries\n");
        return -ENOMEM;
    }
    memset(g_feedback.entries, 0, sizeof(feedback_entry_t) * FEEDBACK_MAX_ENTRIES);
    
    /* Allocate submission queue */
    g_feedback.submission_queue = (feedback_entry_t *)kmalloc(sizeof(feedback_entry_t) * 100);
    if (!g_feedback.submission_queue) {
        printk("[FEEDBACK] WARNING: Failed to allocate submission queue\n");
    }
    
    /* Initialize categories */
    feedback_init_categories();
    
    /* Default settings */
    g_feedback.auto_include_logs = true;
    g_feedback.allow_anonymous = true;
    g_feedback.email_notifications = false;
    g_feedback.developer_responses = true;
    strncpy(g_feedback.developer_email, "feedback@nexus-os.dev", 127);
    strncpy(g_feedback.feedback_server, "https://feedback.nexus-os.dev/api", 255);
    
    /* Log integration */
    g_feedback.attach_system_logs = true;
    g_feedback.attach_session_logs = true;
    g_feedback.max_log_size_kb = 1024;  /* 1 MB max */
    
    g_feedback.initialized = true;
    
    printk("[FEEDBACK] Feedback manager initialized\n");
    printk("[FEEDBACK] Max entries: %u\n", g_feedback.max_entries);
    printk("[FEEDBACK] Categories: %u\n", g_feedback.category_count);
    printk("[FEEDBACK] Auto-attach logs: %s\n", g_feedback.auto_include_logs ? "Yes" : "No");
    printk("[FEEDBACK] Anonymous submissions: %s\n", g_feedback.allow_anonymous ? "Yes" : "No");
    printk("[FEEDBACK] Developer email: %s\n", g_feedback.developer_email);
    printk("[FEEDBACK] ========================================\n");
    
    return 0;
}
