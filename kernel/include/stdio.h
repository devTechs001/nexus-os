/*
 * NEXUS OS - Kernel Standard I/O Header
 * kernel/include/stdio.h
 */

#ifndef _NEXUS_STDIO_H
#define _NEXUS_STDIO_H

#include "kernel.h"

#define printf(...)      printk(__VA_ARGS__)
#define sprintf(...)     snprintf(__VA_ARGS__)
/* snprintf is already in kernel.h */

#endif /* _NEXUS_STDIO_H */
