/*
 * NEXUS OS - GUI Framework Core
 * gui/gui_core.c
 *
 * Core initialization and management for the NEXUS GUI framework.
 */

#include "gui.h"

static gui_framework_t g_gui_framework = {
    .initialized = false,
    .num_displays = 0,
    .num_windows = 0,
    .next_window_id = 1,
    .compositor_active = false,
    .num_animations = 0,
    .frame_count = 0,
    .fps = 0
};

void animation_destroy(animation_t *anim)
{
    /* Stub for now */
    if (anim) {
        /* Free animation data */
    }
}

/**
 * gui_framework_init - Initialize the GUI framework
 */
int gui_framework_init(void)
{
    if (g_gui_framework.initialized) {
        return 0;
    }

    pr_info("Initializing NEXUS GUI Framework...\n");

    /* 1. Initialize Renderer (Software by default) */
    extern int renderer_init(u32 type);
    if (renderer_init(0) != 0) {
        pr_err("  Failed to initialize renderer.\n");
        return -1;
    }

    /* 2. Initialize Compositor */
    extern int compositor_init(void);
    if (compositor_init() != 0) {
        pr_err("  Failed to initialize compositor.\n");
        return -1;
    }

    /* 3. Initialize Display Info */
    g_gui_framework.num_displays = 1;
    g_gui_framework.primary_display = 0;
    display_info_t *primary = &g_gui_framework.displays[0];
    primary->display_id = 0;
    strcpy(primary->name, "Primary Display");
    primary->is_primary = true;
    primary->is_active = true;
    primary->width = 1024;
    primary->height = 768;
    primary->color_depth = 32;
    primary->scale_factor = 1.0f;

    /* 4. Initialize Lock */
    spinlock_init(&g_gui_framework.lock);

    g_gui_framework.initialized = true;
    g_gui_framework.compositor_active = true;

    pr_info("GUI Framework initialized successfully.\n");

    return 0;
}

/**
 * gui_framework_shutdown - Shutdown the GUI framework
 */
void gui_framework_shutdown(void)
{
    if (!g_gui_framework.initialized) {
        return;
    }

    pr_info("Shutting down GUI Framework...\n");

    extern void compositor_shutdown(void);
    compositor_shutdown();

    extern void renderer_shutdown(void);
    renderer_shutdown();

    g_gui_framework.initialized = false;
}

/**
 * gui_framework_is_initialized - Check if GUI framework is initialized
 */
bool gui_framework_is_initialized(void)
{
    return g_gui_framework.initialized;
}

/**
 * gui_get_version - Get GUI framework version
 */
const char *gui_get_version(void)
{
    return GUI_VERSION_STRING;
}

/**
 * gui_get_codename - Get GUI framework codename
 */
const char *gui_get_codename(void)
{
    return GUI_CODENAME;
}
