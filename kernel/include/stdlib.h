/*
 * NEXUS OS - Kernel Standard Library Header
 * kernel/include/stdlib.h
 */

#ifndef _NEXUS_STDLIB_H
#define _NEXUS_STDLIB_H

#include "kernel.h"

#define malloc(size)    kmalloc(size)
#define free(ptr)       kfree(ptr)
#define calloc(n, size) kzalloc((n) * (size))

#endif /* _NEXUS_STDLIB_H */
