/*
 * NEXUS OS - Boot Parameters / Command Line Parser
 * kernel/core/cmdline.c
 *
 * Parses kernel command line from bootloader
 */

#include "../include/kernel.h"

/*===========================================================================*/
/*                         BOOT CONFIGURATION                                */
/*===========================================================================*/

static boot_params_t g_boot_params = {
    .graphics_mode = true,
    .text_mode = false,
    .safe_mode = false,
    .debug_mode = false,
    .native_mode = false,
    .vmware_mode = false,
    .vga_mode = 0,
    .gfx_width = 1024,
    .gfx_height = 768,
    .gfx_depth = 32,
    .nomodeset = false,
    .nosmp = false,
    .noapic = false,
    .nolapic = false,
};

static char g_cmdline[256] = {0};

/*===========================================================================*/
/*                         COMMAND LINE PARSING                              */
/*===========================================================================*/

/**
 * parse_boot_params - Parse kernel command line
 * @cmdline: Command line string from bootloader
 */
void parse_boot_params(const char *cmdline)
{
    char *ptr = (char *)cmdline;
    char *token;
    
    if (!cmdline)
        return;
    
    /* Copy command line */
    strncpy(g_cmdline, cmdline, sizeof(g_cmdline) - 1);
    
    printk("[BOOT] Command line: %s\n", g_cmdline);
    
    /* Parse tokens */
    while ((token = strsep(&ptr, " ")) != NULL) {
        if (strlen(token) == 0)
            continue;
        
        /* Graphics modes */
        if (strcmp(token, "gfxpayload=1024x768x32") == 0 ||
            strcmp(token, "gfxpayload=keep") == 0) {
            g_boot_params.graphics_mode = true;
            g_boot_params.text_mode = false;
            g_boot_params.gfx_width = 1024;
            g_boot_params.gfx_height = 768;
            g_boot_params.gfx_depth = 32;
            printk("[BOOT] Graphics mode: 1024x768x32\n");
        }
        else if (strcmp(token, "gfxpayload=text") == 0) {
            g_boot_params.graphics_mode = false;
            g_boot_params.text_mode = true;
            printk("[BOOT] Text mode enabled\n");
        }
        else if (strncmp(token, "vga=", 4) == 0) {
            g_boot_params.vga_mode = simple_strtoul(token + 4, NULL, 10);
            g_boot_params.text_mode = true;
            printk("[BOOT] VGA mode: %u\n", g_boot_params.vga_mode);
        }
        
        /* Boot modes */
        else if (strcmp(token, "nomodeset") == 0) {
            g_boot_params.nomodeset = true;
            g_boot_params.safe_mode = true;
            printk("[BOOT] Safe mode: nomodeset\n");
        }
        else if (strcmp(token, "nosmp") == 0) {
            g_boot_params.nosmp = true;
            printk("[BOOT] SMP disabled\n");
        }
        else if (strcmp(token, "noapic") == 0) {
            g_boot_params.noapic = true;
            printk("[BOOT] APIC disabled\n");
        }
        else if (strcmp(token, "nolapic") == 0) {
            g_boot_params.nolapic = true;
            printk("[BOOT] Local APIC disabled\n");
        }
        else if (strcmp(token, "debug") == 0) {
            g_boot_params.debug_mode = true;
            printk("[BOOT] Debug mode enabled\n");
        }
        else if (strcmp(token, "loglevel=7") == 0) {
            g_boot_params.debug_mode = true;
            printk("[BOOT] Log level: 7 (debug)\n");
        }
        else if (strcmp(token, "earlyprintk=vga") == 0) {
            g_boot_params.debug_mode = true;
            printk("[BOOT] Early printk on VGA\n");
        }
        else if (strcmp(token, "virt=native") == 0) {
            g_boot_params.native_mode = true;
            printk("[BOOT] Native hardware mode\n");
        }
    }
    
    /* Print boot configuration */
    printk("[BOOT] ========================================\n");
    printk("[BOOT] Boot Configuration:\n");
    printk("[BOOT]   Graphics: %s\n", g_boot_params.graphics_mode ? "Yes" : "No");
    printk("[BOOT]   Text Mode: %s\n", g_boot_params.text_mode ? "Yes" : "No");
    printk("[BOOT]   Safe Mode: %s\n", g_boot_params.safe_mode ? "Yes" : "No");
    printk("[BOOT]   Debug Mode: %s\n", g_boot_params.debug_mode ? "Yes" : "No");
    printk("[BOOT]   Native Mode: %s\n", g_boot_params.native_mode ? "Yes" : "No");
    if (g_boot_params.graphics_mode) {
        printk("[BOOT]   Resolution: %ux%ux%u\n", 
               g_boot_params.gfx_width, g_boot_params.gfx_height, g_boot_params.gfx_depth);
    }
    printk("[BOOT] ========================================\n");
}

/**
 * get_boot_params - Get parsed boot parameters
 */
boot_params_t *get_boot_params(void)
{
    return &g_boot_params;
}

/**
 * is_graphics_mode - Check if graphics mode is enabled
 */
bool is_graphics_mode(void)
{
    return g_boot_params.graphics_mode;
}

/**
 * is_safe_mode - Check if safe mode is enabled
 */
bool is_safe_mode(void)
{
    return g_boot_params.safe_mode;
}

/**
 * is_debug_mode - Check if debug mode is enabled
 */
bool is_debug_mode(void)
{
    return g_boot_params.debug_mode;
}

/**
 * simple_strtoul - Simple string to unsigned long conversion
 */
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
    unsigned long result = 0;
    unsigned int val;
    
    if (!cp)
        return 0;
    
    while (*cp) {
        val = *cp - '0';
        if (val > 9)
            break;
        result = result * 10 + val;
        cp++;
    }
    
    if (endp)
        *endp = (char *)cp;
    
    return result;
}

/**
 * strsep - Split string into tokens
 */
char *strsep(char **stringp, const char *delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;
    
    if ((s = *stringp) == NULL)
        return NULL;
    
    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return tok;
            }
        } while (sc != 0);
    }
}
