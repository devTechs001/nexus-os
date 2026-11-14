/*
 * NEXUS OS - System Application Registry Header
 * gui/apps_nexus.h
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef NEXUS_APPS_H
#define NEXUS_APPS_H

#include "../kernel/include/types.h"

typedef enum {
    NEXUS_APP_NONE = 0,
    NEXUS_APP_VM_MANAGER,
    NEXUS_APP_FILE_MANAGER,
    NEXUS_APP_TASK_MANAGER,
    NEXUS_APP_SETTINGS,
    NEXUS_APP_TERMINAL
} nexus_app_t;

/* Start an application; returns 0 on success. */
int nexus_app_launch(nexus_app_t app);

/* Close whatever is running and return to the desktop. */
void nexus_app_close(void);

/* Repaint the active application, if any. */
void nexus_app_redraw(void);

/* Currently active application (NEXUS_APP_NONE for the desktop). */
nexus_app_t nexus_app_active(void);

/* Human-readable name for an application id. */
const char *nexus_app_name(nexus_app_t app);

#endif /* NEXUS_APPS_H */
