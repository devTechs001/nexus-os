/*
 * NEXUS OS - userspace string.h
 * userspace/include/string.h
 *
 * The implementations live in userspace/libc/string.c. Each userspace source
 * previously declared these extern by hand or not at all; under the permissive
 * -Wno-implicit-function-declaration used for the kernel build that meant
 * calls compiled as returning int, which silently truncates a pointer on
 * x86_64. Declaring them properly is what makes that class of bug visible.
 */

#ifndef _NEXUS_USERSPACE_STRING_H
#define _NEXUS_USERSPACE_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strtok(char *str, const char *delim);
char *strsep(char **stringp, const char *delim);

#endif /* _NEXUS_USERSPACE_STRING_H */
