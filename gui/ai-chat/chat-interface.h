/*
 * NEXUS OS - AI Chat Interface
 * gui/ai-chat/chat-interface.h
 *
 * Main AI conversation area with message bubbles
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _CHAT_INTERFACE_H
#define _CHAT_INTERFACE_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         CHAT CONFIGURATION                                */
/*===========================================================================*/

#define CHAT_INTERFACE_VERSION      "1.0.0"
#define CHAT_MAX_MESSAGES           1000
#define CHAT_MAX_CONVERSATIONS      100

/*===========================================================================*/
/*                         MESSAGE TYPES                                     */
/*===========================================================================*/

#define MESSAGE_TYPE_USER           0
#define MESSAGE_TYPE_AI             1
#define MESSAGE_TYPE_SYSTEM         2
#define MESSAGE_TYPE_ERROR          3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Message Content
 */
typedef struct {
    char text[4096];                /* Text Content */
    char image_path[512];           /* Image Path */
    char code[8192];                /* Code Block */
    char language[32];              /* Code Language */
} message_content_t;

/**
 * Chat Message
 */
typedef struct chat_message {
    u32 message_id;                 /* Message ID */
    u32 type;                       /* Message Type */
    message_content_t content;      /* Message Content */
    u64 timestamp;                  /* Timestamp */
    bool is_editing;                /* Is Editing */
    bool is_copied;                 /* Is Copied */
    bool is_regenerating;           /* Is Regenerating */
    struct chat_message *next;      /* Next Message */
} chat_message_t;

/**
 * Conversation
 */
typedef struct {
    u32 conversation_id;            /* Conversation ID */
    char title[256];                /* Conversation Title */
    chat_message_t *messages;       /* Messages */
    u32 message_count;              /* Message Count */
    u64 created_time;               /* Created Time */
    u64 last_modified;              /* Last Modified */
    char model_name[64];            /* Model Name */
    bool is_pinned;                 /* Is Pinned */
} conversation_t;

/**
 * Chat Interface State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *chat_window;          /* Chat Window */
    widget_t *chat_widget;          /* Chat Widget */
    
    /* Messages */
    chat_message_t *messages;       /* Messages */
    u32 message_count;              /* Message Count */
    chat_message_t *scroll_to_message; /* Scroll to Message */
    
    /* Conversations */
    conversation_t conversations[CHAT_MAX_CONVERSATIONS]; /* Conversations */
    u32 conversation_count;         /* Conversation Count */
    u32 current_conversation;       /* Current Conversation */
    
    /* Input */
    widget_t *input_box;            /* Input Box */
    widget_t *send_button;          /* Send Button */
    widget_t *attach_button;        /* Attach Button */
    char input_text[4096];          /* Input Text */
    bool is_typing;                 /* Is Typing */
    
    /* Model */
    char current_model[64];         /* Current Model */
    char server_url[256];           /* Server URL */
    bool server_connected;          /* Server Connected */
    u32 server_latency;             /* Server Latency */
    
    /* Settings */
    bool show_timestamps;           /* Show Timestamps */
    bool show_copy_button;          /* Show Copy Button */
    bool show_regenerate_button;    /* Show Regenerate Button */
    bool auto_scroll;               /* Auto Scroll */
    bool markdown_enabled;          /* Markdown Enabled */
    bool syntax_highlighting;       /* Syntax Highlighting */
    
    /* Callbacks */
    void (*on_message_sent)(chat_message_t *);
    void (*on_message_received)(chat_message_t *);
    void (*on_conversation_changed)(u32);
    void (*on_model_changed)(const char *);
    
    /* User Data */
    void *user_data;
    
} chat_interface_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Chat lifecycle */
int chat_interface_init(chat_interface_t *chat);
int chat_interface_show(chat_interface_t *chat);
int chat_interface_hide(chat_interface_t *chat);
int chat_interface_toggle(chat_interface_t *chat);
int chat_interface_shutdown(chat_interface_t *chat);

/* Message management */
chat_message_t *chat_interface_add_message(chat_interface_t *chat, u32 type, const char *text);
int chat_interface_remove_message(chat_interface_t *chat, u32 message_id);
int chat_interface_edit_message(chat_interface_t *chat, u32 message_id, const char *text);
int chat_interface_copy_message(chat_interface_t *chat, u32 message_id);
int chat_interface_regenerate_message(chat_interface_t *chat, u32 message_id);
int chat_interface_get_messages(chat_interface_t *chat, chat_message_t *messages, u32 *count);

/* Conversation management */
int chat_interface_new_conversation(chat_interface_t *chat);
int chat_interface_load_conversation(chat_interface_t *chat, u32 conversation_id);
int chat_interface_delete_conversation(chat_interface_t *chat, u32 conversation_id);
int chat_interface_rename_conversation(chat_interface_t *chat, u32 conversation_id, const char *title);
int chat_interface_pin_conversation(chat_interface_t *chat, u32 conversation_id);
int chat_interface_export_conversation(chat_interface_t *chat, u32 conversation_id, const char *path);

/* Input */
int chat_interface_set_input(chat_interface_t *chat, const char *text);
int chat_interface_get_input(chat_interface_t *chat, char *text, u32 size);
int chat_interface_clear_input(chat_interface_t *chat);
int chat_interface_send_message(chat_interface_t *chat);

/* Model management */
int chat_interface_set_model(chat_interface_t *chat, const char *model);
int chat_interface_get_model(chat_interface_t *chat, char *model, u32 size);
int chat_interface_connect_server(chat_interface_t *chat, const char *url);
int chat_interface_disconnect_server(chat_interface_t *chat);

/* Settings */
int chat_interface_set_timestamps(chat_interface_t *chat, bool show);
int chat_interface_set_markdown(chat_interface_t *chat, bool enabled);
int chat_interface_set_syntax_highlighting(chat_interface_t *chat, bool enabled);
int chat_interface_set_auto_scroll(chat_interface_t *chat, bool enabled);

/* Utility functions */
chat_interface_t *chat_interface_get_instance(void);

#endif /* _CHAT_INTERFACE_H */
