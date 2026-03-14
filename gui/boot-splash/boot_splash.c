/*
 * NEXUS OS - Boot Splash Screen
 * gui/boot-splash/boot_splash.c
 *
 * Animated boot logo and progress indicator
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SPLASH CONFIGURATION                              */
/*===========================================================================*/

#define SPLASH_WIDTH            800
#define SPLASH_HEIGHT           600
#define LOGO_SIZE               256
#define PROGRESS_WIDTH          400
#define PROGRESS_HEIGHT         8
#define FADE_STEPS              32

/* Boot Stages */
#define BOOT_STAGE_START        0
#define BOOT_STAGE_KERNEL       1
#define BOOT_STAGE_DRIVERS      2
#define BOOT_STAGE_SERVICES     3
#define BOOT_STAGE_GUI          4
#define BOOT_STAGE_COMPLETE     5

/* Splash Colors */
#define COLOR_SPLASH_BG         0x1E3A5F  /* Dark blue */
#define COLOR_LOGO_PRIMARY      0x3498DB  /* Light blue */
#define COLOR_LOGO_SECONDARY    0x2ECC71  /* Green */
#define COLOR_PROGRESS_BG       0x2C3E50  /* Dark slate */
#define COLOR_PROGRESS_FG       0x3498DB  /* Blue */
#define COLOR_TEXT              0xECF0F1  /* Light gray */

/* Animation States */
#define ANIM_STATE_FADE_IN      0
#define ANIM_STATE_PULSE        1
#define ANIM_STATE_PROGRESS     2
#define ANIM_STATE_FADE_OUT     3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Boot Progress Item */
typedef struct {
    const char *message;
    bool completed;
    u64 start_time;
    u64 end_time;
} boot_progress_item_t;

/* Splash Screen State */
typedef struct {
    u32 stage;
    u32 anim_state;
    u32 anim_frame;
    u64 start_time;
    u64 elapsed_ms;
    u32 progress_percent;
    u32 logo_scale;
    u32 logo_rotation;
    u32 alpha;
    bool fade_in_complete;
    bool fade_out_started;
    boot_progress_item_t items[16];
    u32 item_count;
    u32 current_item;
} splash_state_t;

static splash_state_t g_splash;

/*===========================================================================*/
/*                         LOGO RENDERING                                    */
/*===========================================================================*/

/* Draw NEXUS Logo (ASCII Art for Text Mode) */
void splash_draw_logo_text(u32 x, u32 y)
{
    const char *logo[] = {
        "  _   _ _____ _____ _   _  ____  ",
        " | \\ | | ____|_   _| | | |/ ___| ",
        " |  \\| |  _|   | | | | | | |  _  ",
        " | |\\  | |___  | | | |_| | |_| | ",
        " |_| \\_|_____| |_|  \\___/ \\____| ",
        "                                 ",
        "      O P E R A T I N G   S Y S T E M",
        "                                 ",
        "         Genesis Edition v1.0    ",
        NULL
    };
    
    u32 line = 0;
    while (logo[line] != NULL) {
        printk_at(x, y + line * 16, logo[line], COLOR_LOGO_PRIMARY);
        line++;
    }
}

/* Draw NEXUS Logo (Graphical - for framebuffer) */
void splash_draw_logo_graphical(u32 x, u32 y, u32 scale, u32 alpha)
{
    /* Logo would be rendered here using framebuffer */
    /* For now, draw simplified version */
    
    u32 logo_width = LOGO_SIZE * scale / 100;
    u32 logo_height = LOGO_SIZE * scale / 100;
    
    /* Draw outer circle */
    draw_circle(x + logo_width/2, y + logo_height/2, logo_width/2, COLOR_LOGO_PRIMARY, alpha);
    
    /* Draw inner NEXUS symbol */
    draw_text_centered(x + logo_width/2, y + logo_height/2, "N", COLOR_TEXT, 48);
    
    /* Draw orbiting particles */
    u32 particle_angle = (g_splash.anim_frame * 10) % 360;
    u32 px = x + logo_width/2 + cos_lookup(particle_angle) * (logo_width/3);
    u32 py = y + logo_height/2 + sin_lookup(particle_angle) * (logo_width/3);
    draw_circle(px, py, 8, COLOR_LOGO_SECONDARY, alpha);
}

/*===========================================================================*/
/*                         PROGRESS BAR                                      */
/*===========================================================================*/

/* Draw Progress Bar */
void splash_draw_progress(u32 x, u32 y, u32 width, u32 height, u32 percent)
{
    /* Background */
    draw_rect(x, y, width, height, COLOR_PROGRESS_BG);
    
    /* Foreground (filled portion) */
    u32 filled_width = (width * percent) / 100;
    draw_rect(x, y, filled_width, height, COLOR_PROGRESS_FG);
    
    /* Border */
    draw_rect_outline(x, y, width, height, COLOR_TEXT, 1);
}

/* Draw Progress Text */
void splash_draw_progress_text(u32 x, u32 y, const char *message, u32 percent)
{
    char buffer[128];
    snprintf(buffer, 127, "%s... %u%%", message, percent);
    draw_text(x, y, buffer, COLOR_TEXT, 16);
}

/*===========================================================================*/
/*                         BOOT STAGES                                       */
/*===========================================================================*/

/* Initialize Boot Progress Items */
void splash_init_items(void)
{
    g_splash.items[0].message = "Loading kernel";
    g_splash.items[1].message = "Initializing hardware";
    g_splash.items[2].message = "Loading drivers";
    g_splash.items[3].message = "Starting services";
    g_splash.items[4].message = "Initializing GUI";
    g_splash.items[5].message = "Complete";
    g_splash.item_count = 6;
    g_splash.current_item = 0;
    
    for (u32 i = 0; i < g_splash.item_count; i++) {
        g_splash.items[i].completed = false;
        g_splash.items[i].start_time = 0;
        g_splash.items[i].end_time = 0;
    }
}

/* Update Boot Stage */
void splash_set_stage(u32 stage)
{
    g_splash.stage = stage;
    
    if (stage < g_splash.item_count) {
        g_splash.items[stage].start_time = 0;  /* Would get from kernel */
        g_splash.items[stage].completed = true;
        g_splash.items[stage].end_time = 0;  /* Would get from kernel */
        g_splash.current_item = stage;
    }
    
    /* Update overall progress */
    g_splash.progress_percent = (stage * 100) / g_splash.item_count;
    
    printk("[SPLASH] Stage %u: %s (%u%%)\n", 
           stage, 
           stage < g_splash.item_count ? g_splash.items[stage].message : "Complete",
           g_splash.progress_percent);
}

/*===========================================================================*/
/*                         ANIMATIONS                                        */
/*===========================================================================*/

/* Fade In Animation */
void splash_anim_fade_in(void)
{
    if (g_splash.alpha < 255) {
        g_splash.alpha += 8;
        if (g_splash.alpha > 255) g_splash.alpha = 255;
    } else {
        g_splash.fade_in_complete = true;
        g_splash.anim_state = ANIM_STATE_PULSE;
    }
}

/* Pulse Animation */
void splash_anim_pulse(void)
{
    g_splash.anim_frame++;
    
    /* Pulse logo scale between 100% and 105% */
    u32 pulse = (sin_lookup(g_splash.anim_frame * 5) + 128) / 256;
    g_splash.logo_scale = 100 + pulse * 5;
    
    /* Transition to progress after 2 seconds */
    if (g_splash.elapsed_ms > 2000) {
        g_splash.anim_state = ANIM_STATE_PROGRESS;
        splash_set_stage(BOOT_STAGE_KERNEL);
    }
}

/* Progress Animation */
void splash_anim_progress(void)
{
    g_splash.elapsed_ms += 16;  /* ~60fps */
    
    /* Smooth progress increment */
    u32 target_progress = (g_splash.stage + 1) * 100 / g_splash.item_count;
    if (g_splash.progress_percent < target_progress) {
        g_splash.progress_percent++;
    }
    
    /* Auto-advance stages */
    u64 stage_times[] = {3000, 5000, 7000, 9000, 11000, 12000};
    if (g_splash.stage < BOOT_STAGE_COMPLETE && 
        g_splash.elapsed_ms > stage_times[g_splash.stage]) {
        splash_set_stage(g_splash.stage + 1);
    }
    
    /* Transition to fade out when complete */
    if (g_splash.stage >= BOOT_STAGE_COMPLETE && g_splash.progress_percent >= 100) {
        g_splash.anim_state = ANIM_STATE_FADE_OUT;
        g_splash.fade_out_started = true;
    }
}

/* Fade Out Animation */
void splash_anim_fade_out(void)
{
    if (g_splash.alpha > 0) {
        g_splash.alpha -= 16;
        if (g_splash.alpha > 255) g_splash.alpha = 0;
    } else {
        /* Splash complete - hide and show desktop/login */
        splash_hide();
    }
}

/*===========================================================================*/
/*                         SPLASH CONTROL                                    */
/*===========================================================================*/

/* Initialize Splash Screen */
int splash_init(void)
{
    printk("[SPLASH] ========================================\n");
    printk("[SPLASH] NEXUS OS Boot Splash\n");
    printk("[SPLASH] ========================================\n");
    
    memset(&g_splash, 0, sizeof(splash_state_t));
    g_splash.anim_state = ANIM_STATE_FADE_IN;
    g_splash.alpha = 0;
    g_splash.logo_scale = 100;
    g_splash.logo_rotation = 0;
    g_splash.fade_in_complete = false;
    g_splash.fade_out_started = false;
    g_splash.start_time = 0;  /* Would get from kernel */
    g_splash.elapsed_ms = 0;
    g_splash.progress_percent = 0;
    
    splash_init_items();
    
    printk("[SPLASH] Splash initialized\n");
    printk("[SPLASH] Resolution: %ux%u\n", SPLASH_WIDTH, SPLASH_HEIGHT);
    printk("[SPLASH] Animation: Fade-in → Pulse → Progress → Fade-out\n");
    printk("[SPLASH] ========================================\n");
    
    return 0;
}

/* Show Splash Screen */
void splash_show(void)
{
    g_splash.anim_state = ANIM_STATE_FADE_IN;
    g_splash.alpha = 0;
    printk("[SPLASH] Showing splash screen\n");
}

/* Hide Splash Screen */
void splash_hide(void)
{
    printk("[SPLASH] Hiding splash screen\n");
    printk("[SPLASH] Boot complete - transitioning to login/desktop\n");
    /* Would trigger login screen or desktop */
}

/* Update Splash Screen */
void splash_update(void)
{
    if (g_splash.anim_state == ANIM_STATE_FADE_IN) {
        splash_anim_fade_in();
    } else if (g_splash.anim_state == ANIM_STATE_PULSE) {
        splash_anim_pulse();
    } else if (g_splash.anim_state == ANIM_STATE_PROGRESS) {
        splash_anim_progress();
    } else if (g_splash.anim_state == ANIM_STATE_FADE_OUT) {
        splash_anim_fade_out();
    }
}

/* Render Splash Screen */
void splash_render(void)
{
    /* Clear background */
    draw_rect(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, COLOR_SPLASH_BG);
    
    /* Draw logo (centered) */
    u32 logo_x = (SPLASH_WIDTH - LOGO_SIZE) / 2;
    u32 logo_y = SPLASH_HEIGHT / 4;
    splash_draw_logo_graphical(logo_x, logo_y, g_splash.logo_scale, g_splash.alpha);
    
    /* Draw progress bar (below logo) */
    u32 progress_x = (SPLASH_WIDTH - PROGRESS_WIDTH) / 2;
    u32 progress_y = logo_y + LOGO_SIZE + 50;
    splash_draw_progress(progress_x, progress_y, PROGRESS_WIDTH, PROGRESS_HEIGHT, g_splash.progress_percent);
    
    /* Draw current stage text */
    if (g_splash.current_item < g_splash.item_count) {
        splash_draw_progress_text(progress_x, progress_y + 20, 
                                  g_splash.items[g_splash.current_item].message,
                                  g_splash.progress_percent);
    }
    
    /* Draw version text (bottom) */
    draw_text_centered(SPLASH_WIDTH/2, SPLASH_HEIGHT - 40, 
                       "NEXUS OS Genesis Edition v1.0", 
                       COLOR_TEXT, 14);
    
    /* Draw copyright */
    draw_text_centered(SPLASH_WIDTH/2, SPLASH_HEIGHT - 20,
                       "Copyright (c) 2026 NEXUS Development Team",
                       COLOR_TEXT, 10);
}

/* Main Splash Loop */
void splash_run(void)
{
    splash_show();
    
    while (g_splash.anim_state != ANIM_STATE_FADE_OUT || g_splash.alpha > 0) {
        splash_update();
        splash_render();
        
        /* Small delay for animation timing */
        sleep_ms(16);  /* ~60fps */
    }
}

/*===========================================================================*/
/*                         BOOT SEQUENCE INTEGRATION                         */
/*===========================================================================*/

/* Called from kernel init */
int boot_splash_start(void)
{
    splash_init();
    splash_show();
    return 0;
}

/* Called during kernel init stages */
void boot_splash_update_stage(u32 stage)
{
    splash_set_stage(stage);
}

/* Called when boot is complete */
void boot_splash_complete(void)
{
    splash_set_stage(BOOT_STAGE_COMPLETE);
}
