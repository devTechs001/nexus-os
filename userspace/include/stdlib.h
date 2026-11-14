/*
 * NEXUS OS - userspace stdlib.h
 * userspace/include/stdlib.h
 *
 * Implementations live in userspace/libc/stdlib.c.
 *
 * malloc/free are declared here for the first time. Userspace callers were
 * calling them with no declaration in scope, so the compiler assumed an int
 * return; on x86_64 that truncates the pointer to 32 bits and the resulting
 * "cast to pointer from integer of different size" warnings in stdio.c were
 * the only sign of it.
 */

#ifndef _NEXUS_USERSPACE_STDLIB_H
#define _NEXUS_USERSPACE_STDLIB_H

#include <stddef.h>
#include <stdint.h>

/* ---- memory ---- */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

/* ---- process ---- */
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atexit(void (*fn)(void));

/* ---- conversion ---- */
int atoi(const char *s);
long atol(const char *s);
long long atoll(const char *s);
double atof(const char *s);
long strtol(const char *s, char **endptr, int base);
long long strtoll(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);
unsigned long long strtoull(const char *s, char **endptr, int base);

/* ---- misc ---- */
#define RAND_MAX 32767
int rand(void);
void srand(unsigned int seed);
int abs(int x);
long labs(long x);
char *getenv(const char *name);
int system(const char *command);
int mkstemp(char *template);

#endif /* _NEXUS_USERSPACE_STDLIB_H */
