/*
 * NEXUS OS - System Application Registry
 * gui/apps_nexus.c
 *
 * The Virtualization Manager (and friends) existed as fully written GUI
 * modules that nothing ever launched - virt_gui_init() had no callers, so
 * the VM manager could never appear on screen no matter how complete it
 * was. This file gives the desktop a single entry point that can start
 * those applications on demand.
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"
#include "virtualization/virt_manager_gui.h"
#include "apps_nexus.h"

/* Tracks which application currently owns the screen. */
static nexus_app_t g_active_app = NEXUS_APP_NONE;

const char *nexus_app_name(nexus_app_t app)
{
    switch (app) {
    case NEXUS_APP_VM_MANAGER:  return "Virtualization Manager";
    case NEXUS_APP_FILE_MANAGER: return "File Manager";
    case NEXUS_APP_TASK_MANAGER: return "Task Manager";
    case NEXUS_APP_SETTINGS:    return "Settings";
    case NEXUS_APP_NONE:
    default:                    return "Desktop";
    }
}

nexus_app_t nexus_app_active(void)
{
    return g_active_app;
}

int nexus_app_launch(nexus_app_t app)
{
    printk("[APPS] Launching %s\n", nexus_app_name(app));

    switch (app) {
    case NEXUS_APP_VM_MANAGER:
        /*
         * virt_gui_init() binds the manager to the live framebuffer,
         * populates the model and renders the first frame.
         */
        if (virt_gui_init() != 0) {
            printk("[APPS] ERROR: Virtualization Manager failed to start\n");
            return -1;
        }
        break;

    case NEXUS_APP_NONE:
        g_active_app = NEXUS_APP_NONE;
        return 0;

    default:
        printk("[APPS] %s is not wired up yet\n", nexus_app_name(app));
        return -1;
    }

    g_active_app = app;
    return 0;
}

void nexus_app_close(void)
{
    if (g_active_app != NEXUS_APP_NONE) {
        printk("[APPS] Closing %s\n", nexus_app_name(g_active_app));
    }
    g_active_app = NEXUS_APP_NONE;
}

void nexus_app_redraw(void)
{
    switch (g_active_app) {
    case NEXUS_APP_VM_MANAGER:
        /*
         * Poll input before repainting. Nothing dispatched pointer events to
         * this app - apps_nexus.c had no mouse handling at all - so the
         * manager rendered a full UI that could never respond to a click.
         * Polling here keeps the call site in one place and makes the action
         * bar's buttons reachable.
         */
        virt_manager_poll_input();
        virt_manager_refresh_vm_list();
        virt_manager_gui_render();
        break;
    default:
        break;
    }
}
