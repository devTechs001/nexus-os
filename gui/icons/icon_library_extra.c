/*
 * NEXUS OS - Additional Icons Part 2
 * gui/icons/icon_library_extra.c
 *
 * More icons for display, desktop, system info
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DISPLAY ICONS                                     */
/*===========================================================================*/

/* Display/Monitor Icon */
static const char *icon_display_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='2' y='3' width='20' height='14' rx='2' fill='#3498DB'/>"
"<rect x='4' y='5' width='16' height='10' fill='#2ECC71'/>"
"<rect x='9' y='19' width='6' height='2' fill='#3498DB'/>"
"<rect x='7' y='21' width='10' height='1' fill='#3498DB'/>"
"</svg>";

/* Brightness Icon */
static const char *icon_brightness_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<circle cx='12' cy='12' r='5' fill='#F39C12'/>"
"<line x1='12' y1='1' x2='12' y2='4' stroke='#F39C12' stroke-width='2'/>"
"<line x1='12' y1='20' x2='12' y2='23' stroke='#F39C12' stroke-width='2'/>"
"<line x1='1' y1='12' x2='4' y2='12' stroke='#F39C12' stroke-width='2'/>"
"<line x1='20' y1='12' x2='23' y2='12' stroke='#F39C12' stroke-width='2'/>"
"<line x1='4.2' y1='4.2' x2='6.3' y2='6.3' stroke='#F39C12' stroke-width='2'/>"
"<line x1='17.7' y1='17.7' x2='19.8' y2='19.8' stroke='#F39C12' stroke-width='2'/>"
"<line x1='4.2' y1='19.8' x2='6.3' y2='17.7' stroke='#F39C12' stroke-width='2'/>"
"<line x1='17.7' y1='6.3' x2='19.8' y2='4.2' stroke='#F39C12' stroke-width='2'/>"
"</svg>";

/* Resolution Icon */
static const char *icon_resolution_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<rect x='3' y='5' width='18' height='14' rx='2' fill='#9B59B6'/>"
"<rect x='5' y='7' width='14' height='10' fill='#2ECC71'/>"
"<text x='12' y='14' text-anchor='middle' fill='white' font-size='6'>HD</text>"
"</svg>";

/* Night Light Icon */
static const char *icon_night_light_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<path d='M12 3C10 3 8 4 7 6C9 6 11 7 12 9C13 7 15 6 17 6C16 4 14 3 12 3Z' fill='#F39C12'/>"
"<circle cx='12' cy='14' r='5' fill='#F39C12'/>"
"<path d='M12 2V4M12 20V22M4 12H2M22 12H20' stroke='#F39C12' stroke-width='2'/>"
"</svg>";

/* HDR Icon */
static const char *icon_hdr_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<rect x='2' y='4' width='20' height='16' rx='2' fill='#E74C3C'/>"
"<text x='12' y='15' text-anchor='middle' fill='white' font-size='8' font-weight='bold'>HDR</text>"
"</svg>";

/*===========================================================================*/
/*                         DESKTOP ICONS                                     */
/*===========================================================================*/

/* Desktop Icon */
static const char *icon_desktop_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='2' y='4' width='20' height='14' rx='2' fill='#3498DB'/>"
"<rect x='4' y='6' width='16' height='10' fill='#2ECC71'/>"
"<circle cx='8' cy='11' r='2' fill='white'/>"
"<rect x='12' y='9' width='6' height='1' fill='white'/>"
"<rect x='12' y='11' width='6' height='1' fill='white'/>"
"<rect x='12' y='13' width='4' height='1' fill='white'/>"
"</svg>";

/* Wallpaper Icon */
static const char *icon_wallpaper_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='3' y='3' width='18' height='18' rx='2' fill='#2ECC71'/>"
"<circle cx='8' cy='8' r='2' fill='#F39C12'/>"
"<path d='M3 18L8 13L12 17L16 12L21 18V19H3V18Z' fill='#3498DB'/>"
"</svg>";

/* Theme Icon */
static const char *icon_theme_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<circle cx='12' cy='12' r='10' fill='#9B59B6'/>"
"<path d='M12 2C12 2 16 6 16 12C16 16 12 22 12 22' stroke='white' stroke-width='2' fill='none'/>"
"<path d='M12 2C12 2 8 6 8 12C8 16 12 22 12 22' stroke='white' stroke-width='2' fill='none' opacity='0.5'/>"
"</svg>";

/* Effects Icon */
static const char *icon_effects_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<circle cx='12' cy='12' r='8' fill='none' stroke='#3498DB' stroke-width='2'/>"
"<circle cx='12' cy='12' r='5' fill='none' stroke='#3498DB' stroke-width='2' opacity='0.7'/>"
"<circle cx='12' cy='12' r='2' fill='#3498DB'/>"
"<path d='M2 12H4M20 12H22M12 2V4M12 20V22' stroke='#3498DB' stroke-width='2'/>"
"</svg>";

/* Glassmorphic Icon */
static const char *icon_glassmorphic_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none'>"
"<rect x='3' y='3' width='18' height='18' rx='4' fill='#FFFFFF' fill-opacity='0.3' stroke='#FFFFFF' stroke-width='2'/>"
"<rect x='6' y='6' width='12' height='12' rx='2' fill='#3498DB' fill-opacity='0.5'/>"
"<circle cx='12' cy='12' r='3' fill='white'/>"
"</svg>";

/* Panel Icon */
static const char *icon_panel_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#7F8C8D'>"
"<rect x='2' y='4' width='20' height='4' rx='1' fill='#7F8C8D'/>"
"<rect x='2' y='10' width='20' height='4' rx='1' fill='#7F8C8D' opacity='0.7'/>"
"<rect x='2' y='16' width='20' height='4' rx='1' fill='#7F8C8D' opacity='0.4'/>"
"</svg>";

/* Dock Icon */
static const char *icon_dock_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='2' y='16' width='20' height='6' rx='2' fill='#3498DB'/>"
"<rect x='4' y='18' width='3' height='2' rx='1' fill='white'/>"
"<rect x='8.5' y='18' width='3' height='2' rx='1' fill='white'/>"
"<rect x='13' y='18' width='3' height='2' rx='1' fill='white'/>"
"<rect x='17.5' y='18' width='3' height='2' rx='1' fill='white'/>"
"</svg>";

/*===========================================================================*/
/*                         SYSTEM INFO ICONS                                 */
/*===========================================================================*/

/* Info Icon */
static const char *icon_info_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<circle cx='12' cy='12' r='10' fill='#3498DB'/>"
"<text x='12' y='16' text-anchor='middle' fill='white' font-size='14' font-weight='bold'>i</text>"
"</svg>";

/* Version Icon */
static const char *icon_version_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='4' y='4' width='16' height='16' rx='2' fill='#2ECC71'/>"
"<text x='12' y='15' text-anchor='middle' fill='white' font-size='10' font-weight='bold'>v1</text>"
"</svg>";

/* Build Icon */
static const char *icon_build_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#9B59B6'>"
"<path d='M12 2L2 7V17L12 22L22 17V7L12 2Z' fill='#9B59B6'/>"
"<text x='12' y='15' text-anchor='middle' fill='white' font-size='8' font-weight='bold'>BUILD</text>"
"</svg>";

/* Kernel Icon */
static const char *icon_kernel_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#E74C3C'>"
"<rect x='3' y='3' width='18' height='18' rx='2' fill='#E74C3C'/>"
"<circle cx='12' cy='12' r='5' fill='none' stroke='white' stroke-width='2'/>"
"<circle cx='12' cy='12' r='2' fill='white'/>"
"</svg>";

/* Architecture Icon */
static const char *icon_architecture_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='2' y='6' width='9' height='12' rx='1' fill='#3498DB'/>"
"<rect x='13' y='6' width='9' height='12' rx='1' fill='#3498DB' opacity='0.7'/>"
"<line x1='7' y1='10' x2='7' y2='14' stroke='white' stroke-width='2'/>"
"<line x1='17' y1='10' x2='17' y2='14' stroke='white' stroke-width='2'/>"
"</svg>";

/* Owner Icon */
static const char *icon_owner_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<circle cx='12' cy='8' r='4' fill='#2ECC71'/>"
"<path d='M6 20C6 17 9 15 12 15C15 15 18 17 18 20' fill='#2ECC71'/>"
"</svg>";

/* Install Date Icon */
static const char *icon_install_date_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#F39C12'>"
"<rect x='3' y='4' width='18' height='18' rx='2' fill='#F39C12'/>"
"<line x1='3' y1='10' x2='21' y2='10' stroke='white' stroke-width='2'/>"
"<text x='12' y='18' text-anchor='middle' fill='white' font-size='8'>DATE</text>"
"</svg>";

/*===========================================================================*/
/*                         ADDITIONAL UTILITY ICONS                          */
/*===========================================================================*/

/* Search Icon */
static const char *icon_search_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#7F8C8D'>"
"<circle cx='11' cy='11' r='7' stroke='#7F8C8D' stroke-width='2' fill='none'/>"
"<line x1='16' y1='16' x2='21' y2='21' stroke='#7F8C8D' stroke-width='2'/>"
"</svg>";

/* Tasks Icon */
static const char *icon_tasks_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='3' y='3' width='18' height='18' rx='2' fill='#3498DB'/>"
"<line x1='8' y1='8' x2='16' y2='8' stroke='white' stroke-width='2'/>"
"<line x1='8' y1='12' x2='16' y2='12' stroke='white' stroke-width='2'/>"
"<line x1='8' y1='16' x2='13' y2='16' stroke='white' stroke-width='2'/>"
"</svg>";

/* Spacer Icon (invisible) */
static const char *icon_spacer_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'/>";

/* Trash Icon */
static const char *icon_trash_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#95A5A6'>"
"<path d='M3 6H21V19C21 20.5 19.5 22 18 22H6C4.5 22 3 20.5 3 19V6Z' fill='#95A5A6'/>"
"<path d='M8 6V4H16V6' stroke='#95A5A6' stroke-width='2'/>"
"<line x1='10' y1='10' x2='10' y2='17' stroke='white' stroke-width='2'/>"
"<line x1='14' y1='10' x2='14' y2='17' stroke='white' stroke-width='2'/>"
"</svg>";

/* Minimize All Icon */
static const char *icon_minimize_all_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#3498DB'>"
"<rect x='2' y='4' width='20' height='4' rx='1' fill='#3498DB'/>"
"<rect x='2' y='10' width='20' height='4' rx='1' fill='#3498DB' opacity='0.7'/>"
"<rect x='2' y='16' width='20' height='4' rx='1' fill='#3498DB' opacity='0.4'/>"
"<line x1='12' y1='20' x2='12' y2='22' stroke='#3498DB' stroke-width='2'/>"
"</svg>";

/* Show Desktop Icon */
static const char *icon_show_desktop_svg = 
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#2ECC71'>"
"<rect x='2' y='4' width='20' height='14' rx='2' fill='#2ECC71'/>"
"<rect x='4' y='6' width='16' height='10' fill='white'/>"
"<path d='M12 18V22M10 20H14' stroke='#2ECC71' stroke-width='2'/>"
"</svg>";

/*===========================================================================*/
/*                         EXTRA ICON FUNCTIONS                              */
/*===========================================================================*/

/* Get Extra Icon by Name */
const char *icon_get_extra(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    /* Display Icons */
    if (strcmp(name, "display") == 0) return icon_display_svg;
    if (strcmp(name, "brightness") == 0) return icon_brightness_svg;
    if (strcmp(name, "resolution") == 0) return icon_resolution_svg;
    if (strcmp(name, "night-light") == 0) return icon_night_light_svg;
    if (strcmp(name, "hdr") == 0) return icon_hdr_svg;
    
    /* Desktop Icons */
    if (strcmp(name, "desktop") == 0) return icon_desktop_svg;
    if (strcmp(name, "wallpaper") == 0) return icon_wallpaper_svg;
    if (strcmp(name, "theme") == 0) return icon_theme_svg;
    if (strcmp(name, "effects") == 0) return icon_effects_svg;
    if (strcmp(name, "glassmorphic") == 0) return icon_glassmorphic_svg;
    if (strcmp(name, "panel") == 0) return icon_panel_svg;
    if (strcmp(name, "dock") == 0) return icon_dock_svg;
    
    /* System Info Icons */
    if (strcmp(name, "info") == 0) return icon_info_svg;
    if (strcmp(name, "version") == 0) return icon_version_svg;
    if (strcmp(name, "build") == 0) return icon_build_svg;
    if (strcmp(name, "kernel") == 0) return icon_kernel_svg;
    if (strcmp(name, "architecture") == 0) return icon_architecture_svg;
    if (strcmp(name, "owner") == 0) return icon_owner_svg;
    if (strcmp(name, "install-date") == 0) return icon_install_date_svg;
    
    /* Utility Icons */
    if (strcmp(name, "search") == 0) return icon_search_svg;
    if (strcmp(name, "tasks") == 0) return icon_tasks_svg;
    if (strcmp(name, "spacer") == 0) return icon_spacer_svg;
    if (strcmp(name, "trash") == 0) return icon_trash_svg;
    if (strcmp(name, "minimize-all") == 0) return icon_minimize_all_svg;
    if (strcmp(name, "show-desktop") == 0) return icon_show_desktop_svg;
    
    return NULL;
}

/* List Extra Icons */
void icon_list_extra(void)
{
    printk("\n[ICONS] ====== EXTRA ICONS ======\n");
    
    printk("[ICONS] Display:\n");
    printk("[ICONS]   display, brightness, resolution, night-light, hdr\n");
    
    printk("[ICONS] Desktop:\n");
    printk("[ICONS]   desktop, wallpaper, theme, effects, glassmorphic\n");
    printk("[ICONS]   panel, dock\n");
    
    printk("[ICONS] System Info:\n");
    printk("[ICONS]   info, version, build, kernel, architecture\n");
    printk("[ICONS]   owner, install-date\n");
    
    printk("[ICONS] Utility:\n");
    printk("[ICONS]   search, tasks, spacer, trash\n");
    printk("[ICONS]   minimize-all, show-desktop\n");
    
    printk("[ICONS] ==========================\n\n");
}
