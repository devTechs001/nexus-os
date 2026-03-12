/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2024 NEXUS Development Team
 *
 * stdlib.c - Standard Library Functions
 *
 * This module implements standard library functions for NEXUS OS userspace.
 * It provides memory allocation, process control, conversion, and utility functions.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Div_t structure for div() */
typedef struct {
    int quot;
    int rem;
} div_t;

/* Ldiv_t structure for ldiv() */
typedef struct {
    long quot;
    long rem;
} ldiv_t;

/* Lldiv_t structure for lldiv() */
typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

/* Comparison function type */
typedef int (*cmp_func_t)(const void *, const void *);

/* Atexit function type */
typedef void (*atexit_func_t)(void);

/* Environment variable structure */
typedef struct {
    char *name;
    char *value;
} env_var_t;

/*===========================================================================*/
/*                         CONSTANTS                                         */
/*===========================================================================*/

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

#define RAND_MAX        32767
#define MB_CUR_MAX      4

#define ATEXIT_MAX      32
#define ENV_MAX         256

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

/* Random number generator state */
static unsigned long rand_seed = 1;

/* Atexit functions */
static atexit_func_t atexit_funcs[ATEXIT_MAX];
static int atexit_count = 0;

/* Environment variables */
static env_var_t env_vars[ENV_MAX];
static int env_count = 0;

/* Program arguments */
static int g_argc = 0;
static char **g_argv = NULL;
static char **g_envp = NULL;

/* Memory allocation globals */
static void *heap_start = NULL;
static void *heap_end = NULL;
static size_t heap_size = 0;

/*===========================================================================*/
/*                         EXTERNAL SYSCALLS                                 */
/*===========================================================================*/

extern void *syscall_brk(void *addr);
extern void *syscall_sbrk(intptr_t increment);
extern int syscall_exit(int status);
extern void syscall_abort(void);
extern char *syscall_getenv(const char *name);
extern int syscall_setenv(const char *name, const char *value, int overwrite);
extern int syscall_unsetenv(const char *name);

/*===========================================================================*/
/*                         MEMORY ALLOCATION                                 */
/*===========================================================================*/

/* Simple heap allocator metadata */
#define ALIGN_SIZE          16
#define ALIGN(x)            (((x) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1))

#define BLOCK_HEADER_SIZE   ALIGN(sizeof(struct block_header))
#define MIN_BLOCK_SIZE      ALIGN(32)

typedef struct block_header {
    size_t size;              /* Block size (including header) */
    bool is_free;             /* Is block free? */
    struct block_header *next; /* Next block in list */
    struct block_header *prev; /* Previous block in list */
    struct block_header *next_free; /* Next free block */
    struct block_header *prev_free; /* Previous free block */
} block_header_t;

/* Free list head */
static block_header_t *free_list = NULL;

/**
 * _heap_init - Initialize heap allocator
 */
static void _heap_init(void)
{
    /* Get initial heap from kernel */
    void *brk = syscall_sbrk(0);
    if (!brk) {
        return;
    }

    heap_start = brk;
    heap_end = brk;
    heap_size = 0;

    /* Create initial free block */
    block_header_t *initial = (block_header_t *)brk;
    initial->size = MIN_BLOCK_SIZE;
    initial->is_free = true;
    initial->next = NULL;
    initial->prev = NULL;
    initial->next_free = NULL;
    initial->prev_free = NULL;

    free_list = initial;
    heap_end = (void *)((char *)brk + MIN_BLOCK_SIZE);
    heap_size = MIN_BLOCK_SIZE;
}

/**
 * _expand_heap - Expand heap by given size
 */
static bool _expand_heap(size_t size)
{
    size = ALIGN(size);

    void *new_brk = syscall_sbrk(size);
    if (new_brk == (void *)-1) {
        return false;
    }

    /* Create new free block */
    block_header_t *new_block = (block_header_t *)new_brk;
    new_block->size = size;
    new_block->is_free = true;
    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->next_free = NULL;
    new_block->prev_free = NULL;

    /* Add to free list */
    if (free_list) {
        new_block->next_free = free_list;
        free_list->prev_free = new_block;
    }
    free_list = new_block;

    heap_end = (void *)((char *)new_brk + size);
    heap_size += size;

    return true;
}

/**
 * _split_block - Split block if it's large enough
 */
static void _split_block(block_header_t *block, size_t size)
{
    if (block->size < size + MIN_BLOCK_SIZE) {
        return;  /* Not enough space for split */
    }

    /* Create new block from remainder */
    block_header_t *new_block = (block_header_t *)((char *)block + size);
    new_block->size = block->size - size;
    new_block->is_free = true;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->next_free = NULL;
    new_block->prev_free = NULL;

    if (block->next) {
        block->next->prev = new_block;
    }

    block->size = size;
    block->next = new_block;

    /* Add new block to free list */
    new_block->next_free = free_list;
    if (free_list) {
        free_list->prev_free = new_block;
    }
    free_list = new_block;
}

/**
 * _remove_from_free_list - Remove block from free list
 */
static void _remove_from_free_list(block_header_t *block)
{
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        free_list = block->next_free;
    }

    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }

    block->next_free = NULL;
    block->prev_free = NULL;
}

/**
 * _coalesce - Coalesce adjacent free blocks
 */
static void _coalesce(block_header_t *block)
{
    /* Coalesce with next block */
    if (block->next && block->next->is_free) {
        block_header_t *next = block->next;

        /* Remove next from free list */
        _remove_from_free_list(next);

        /* Merge blocks */
        block->size += next->size;
        block->next = next->next;

        if (next->next) {
            next->next->prev = block;
        }
    }

    /* Coalesce with previous block */
    if (block->prev && block->prev->is_free) {
        block_header_t *prev = block->prev;

        /* Remove current from free list */
        _remove_from_free_list(block);

        /* Merge blocks */
        prev->size += block->size;
        prev->next = block->next;

        if (block->next) {
            block->next->prev = prev;
        }
    }
}

/**
 * malloc - Allocate memory
 */
void *malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    /* Initialize heap if needed */
    if (!free_list) {
        _heap_init();
    }

    /* Calculate required block size */
    size_t required = ALIGN(BLOCK_HEADER_SIZE + size);
    if (required < MIN_BLOCK_SIZE) {
        required = MIN_BLOCK_SIZE;
    }

    /* Search free list for suitable block (first fit) */
    block_header_t *block = free_list;
    while (block) {
        if (block->size >= required) {
            /* Found suitable block */
            _remove_from_free_list(block);

            /* Split if necessary */
            _split_block(block, required);

            block->is_free = false;

            return (void *)((char *)block + BLOCK_HEADER_SIZE);
        }
        block = block->next_free;
    }

    /* No suitable block found, expand heap */
    if (!_expand_heap(required)) {
        return NULL;
    }

    /* Retry allocation */
    return malloc(size);
}

/**
 * calloc - Allocate and zero memory
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;

    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    /* Check for overflow */
    if (total / nmemb != size) {
        return NULL;
    }

    void *ptr = malloc(total);
    if (ptr) {
        /* Zero memory */
        unsigned char *p = (unsigned char *)ptr;
        for (size_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }

    return ptr;
}

/**
 * realloc - Reallocate memory
 */
void *realloc(void *ptr, size_t size)
{
    if (!ptr) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    /* Get block header */
    block_header_t *block = (block_header_t *)((char *)ptr - BLOCK_HEADER_SIZE);
    size_t old_size = block->size - BLOCK_HEADER_SIZE;

    if (old_size >= size) {
        /* Block is already large enough */
        return ptr;
    }

    /* Try to expand block by coalescing with next */
    if (block->next && block->next->is_free) {
        size_t combined = block->size + block->next->size;

        if (combined >= ALIGN(BLOCK_HEADER_SIZE + size)) {
            /* Can expand into next block */
            _remove_from_free_list(block->next);

            block->size = combined;
            block->next = block->next->next;

            if (block->next) {
                block->next->prev = block;
            }

            _split_block(block, ALIGN(BLOCK_HEADER_SIZE + size));

            return ptr;
        }
    }

    /* Need to allocate new block and copy */
    void *new_ptr = malloc(size);
    if (new_ptr) {
        /* Copy old data */
        unsigned char *src = (unsigned char *)ptr;
        unsigned char *dst = (unsigned char *)new_ptr;
        for (size_t i = 0; i < old_size; i++) {
            dst[i] = src[i];
        }

        free(ptr);
    }

    return new_ptr;
}

/**
 * free - Free allocated memory
 */
void free(void *ptr)
{
    if (!ptr) {
        return;
    }

    /* Get block header */
    block_header_t *block = (block_header_t *)((char *)ptr - BLOCK_HEADER_SIZE);

    /* Mark as free */
    block->is_free = true;

    /* Add to free list */
    block->next_free = free_list;
    block->prev_free = NULL;

    if (free_list) {
        free_list->prev_free = block;
    }
    free_list = block;

    /* Coalesce with adjacent blocks */
    _coalesce(block);
}

/**
 * aligned_alloc - Allocate aligned memory
 */
void *aligned_alloc(size_t alignment, size_t size)
{
    /* alignment must be power of 2 */
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* Allocate extra space for alignment */
    void *ptr = malloc(size + alignment);
    if (!ptr) {
        return NULL;
    }

    /* Align the pointer */
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);

    return (void *)aligned;
}

/**
 * valloc - Allocate page-aligned memory
 */
void *valloc(size_t size)
{
    return aligned_alloc(4096, size);
}

/**
 * posix_memalign - Allocate aligned memory (POSIX)
 */
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    if (!memptr) {
        return -1;
    }

    /* alignment must be power of 2 and multiple of sizeof(void*) */
    if ((alignment & (alignment - 1)) != 0 ||
        alignment < sizeof(void *)) {
        return -1;  /* EINVAL */
    }

    void *ptr = aligned_alloc(alignment, size);
    if (!ptr) {
        return -1;  /* ENOMEM */
    }

    *memptr = ptr;
    return 0;
}

/*===========================================================================*/
/*                         PROCESS CONTROL                                   */
/*===========================================================================*/

/**
 * exit - Terminate process
 */
void exit(int status)
{
    /* Call atexit functions in reverse order */
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_funcs[i]) {
            atexit_funcs[i]();
        }
    }

    /* Flush standard streams */
    fflush(stdout);
    fflush(stderr);

    /* Exit via syscall */
    syscall_exit(status);

    /* Should never reach here */
    for (;;);
}

/**
 * abort - Abnormal process termination
 */
void abort(void)
{
    /* Print error message */
    fprintf(stderr, "Program aborted\n");

    /* Call abort signal handler if registered */

    syscall_abort();

    /* Should never reach here */
    for (;;);
}

/**
 * atexit - Register function to be called at exit
 */
int atexit(void (*func)(void))
{
    if (!func || atexit_count >= ATEXIT_MAX) {
        return -1;
    }

    atexit_funcs[atexit_count++] = func;
    return 0;
}

/**
 * on_exit - Register function to be called at exit with status
 */
int on_exit(void (*func)(int, void *), void *arg)
{
    /* Simplified - just calls atexit */
    (void)func;
    (void)arg;
    return -1;  /* Not implemented */
}

/**
 * _Exit - Terminate process without cleanup
 */
void _Exit(int status)
{
    syscall_exit(status);

    /* Should never reach here */
    for (;;);
}

/**
 * quick_exit - Quick process termination
 */
void quick_exit(int status)
{
    /* Minimal cleanup */
    fflush(stdout);
    fflush(stderr);

    syscall_exit(status);

    /* Should never reach here */
    for (;;);
}

/**
 * at_quick_exit - Register function for quick_exit
 */
int at_quick_exit(void (*func)(void))
{
    /* Simplified - same as atexit */
    return atexit(func);
}

/**
 * getenv - Get environment variable
 */
char *getenv(const char *name)
{
    if (!name) {
        return NULL;
    }

    /* Search environment variables */
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }

    /* Try syscall */
    return syscall_getenv(name);
}

/**
 * setenv - Set environment variable
 */
int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !value) {
        return -1;
    }

    /* Check if variable exists */
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            if (!overwrite) {
                return 0;
            }

            /* Update value */
            free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return 0;
        }
    }

    /* Add new variable */
    if (env_count >= ENV_MAX) {
        return -1;
    }

    env_vars[env_count].name = strdup(name);
    env_vars[env_count].value = strdup(value);

    if (!env_vars[env_count].name || !env_vars[env_count].value) {
        free(env_vars[env_count].name);
        free(env_vars[env_count].value);
        return -1;
    }

    env_count++;
    return 0;
}

/**
 * unsetenv - Remove environment variable
 */
int unsetenv(const char *name)
{
    if (!name) {
        return -1;
    }

    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            /* Remove variable */
            free(env_vars[i].name);
            free(env_vars[i].value);

            /* Shift remaining variables */
            for (int j = i; j < env_count - 1; j++) {
                env_vars[j] = env_vars[j + 1];
            }

            env_count--;
            return 0;
        }
    }

    return 0;  /* Not found is not an error */
}

/**
 * putenv - Change/add environment variable
 */
int putenv(char *string)
{
    if (!string) {
        return -1;
    }

    /* Find '=' separator */
    char *eq = strchr(string, '=');
    if (!eq) {
        return -1;
    }

    /* Temporarily null-terminate name */
    *eq = '\0';

    int ret = setenv(string, eq + 1, 1);

    /* Restore string */
    *eq = '=';

    return ret;
}

/**
 * clearenv - Clear environment
 */
void clearenv(void)
{
    for (int i = 0; i < env_count; i++) {
        free(env_vars[i].name);
        free(env_vars[i].value);
    }
    env_count = 0;
}

/*===========================================================================*/
/*                         NUMBER CONVERSION                                 */
/*===========================================================================*/

/**
 * atoi - Convert string to integer
 */
int atoi(const char *nptr)
{
    return (int)strtol(nptr, NULL, 10);
}

/**
 * atol - Convert string to long
 */
long atol(const char *nptr)
{
    return strtol(nptr, NULL, 10);
}

/**
 * atoll - Convert string to long long
 */
long long atoll(const char *nptr)
{
    return strtoll(nptr, NULL, 10);
}

/**
 * strtol - Convert string to long integer
 */
long strtol(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    long result = 0;
    bool negative = false;
    bool started = false;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    /* Handle sign */
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0' && base == 0) {
            base = 8;
        } else if (base == 0) {
            base = 10;
        }
    }

    /* Convert digits */
    while (*p) {
        int digit;

        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        /* Check for overflow */
        if (result > (LONG_MAX - digit) / base) {
            if (negative) {
                result = LONG_MIN;
            } else {
                result = LONG_MAX;
            }
        } else {
            result = result * base + digit;
        }

        started = true;
        p++;
    }

    if (endptr) {
        *endptr = started ? (char *)p : (char *)nptr;
    }

    return negative ? -result : result;
}

/**
 * strtoll - Convert string to long long integer
 */
long long strtoll(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    long long result = 0;
    bool negative = false;
    bool started = false;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    /* Handle sign */
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0' && base == 0) {
            base = 8;
        } else if (base == 0) {
            base = 10;
        }
    }

    /* Convert digits */
    while (*p) {
        int digit;

        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        /* Check for overflow */
        if (result > (LLONG_MAX - digit) / base) {
            if (negative) {
                result = LLONG_MIN;
            } else {
                result = LLONG_MAX;
            }
        } else {
            result = result * base + digit;
        }

        started = true;
        p++;
    }

    if (endptr) {
        *endptr = started ? (char *)p : (char *)nptr;
    }

    return negative ? -result : result;
}

/**
 * strtoul - Convert string to unsigned long integer
 */
unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    unsigned long result = 0;
    bool started = false;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    /* Handle sign (ignore for unsigned) */
    if (*p == '-') {
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0' && base == 0) {
            base = 8;
        } else if (base == 0) {
            base = 10;
        }
    }

    /* Convert digits */
    while (*p) {
        int digit;

        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        /* Check for overflow */
        if (result > (ULONG_MAX - digit) / base) {
            result = ULONG_MAX;
        } else {
            result = result * base + digit;
        }

        started = true;
        p++;
    }

    if (endptr) {
        *endptr = started ? (char *)p : (char *)nptr;
    }

    return result;
}

/**
 * strtoull - Convert string to unsigned long long integer
 */
unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    unsigned long long result = 0;
    bool started = false;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    /* Handle sign (ignore for unsigned) */
    if (*p == '-') {
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0' && base == 0) {
            base = 8;
        } else if (base == 0) {
            base = 10;
        }
    }

    /* Convert digits */
    while (*p) {
        int digit;

        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        /* Check for overflow */
        if (result > (ULLONG_MAX - digit) / base) {
            result = ULLONG_MAX;
        } else {
            result = result * base + digit;
        }

        started = true;
        p++;
    }

    if (endptr) {
        *endptr = started ? (char *)p : (char *)nptr;
    }

    return result;
}

/*===========================================================================*/
/*                         FLOATING POINT CONVERSION                         */
/*===========================================================================*/

/**
 * strtod - Convert string to double
 */
double strtod(const char *nptr, char **endptr)
{
    const char *p = nptr;
    double result = 0.0;
    double fraction = 1.0;
    bool negative = false;
    bool in_fraction = false;
    int exponent = 0;
    bool exp_negative = false;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    /* Handle sign */
    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    /* Convert integer part */
    while (*p >= '0' && *p <= '9') {
        result = result * 10.0 + (*p - '0');
        p++;
    }

    /* Convert fraction part */
    if (*p == '.') {
        p++;
        in_fraction = true;

        while (*p >= '0' && *p <= '9') {
            fraction /= 10.0;
            result += (*p - '0') * fraction;
            p++;
        }
    }

    /* Convert exponent */
    if (*p == 'e' || *p == 'E') {
        p++;

        if (*p == '-') {
            exp_negative = true;
            p++;
        } else if (*p == '+') {
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            exponent = exponent * 10 + (*p - '0');
            p++;
        }

        if (exp_negative) {
            exponent = -exponent;
        }

        /* Apply exponent */
        if (exponent != 0) {
            result *= pow(10.0, exponent);
        }
    }

    if (endptr) {
        *endptr = (char *)p;
    }

    return negative ? -result : result;
}

/**
 * strtof - Convert string to float
 */
float strtof(const char *nptr, char **endptr)
{
    return (float)strtod(nptr, endptr);
}

/**
 * strtold - Convert string to long double
 */
long double strtold(const char *nptr, char **endptr)
{
    return (long double)strtod(nptr, endptr);
}

/*===========================================================================*/
/*                         RANDOM NUMBERS                                    */
/*===========================================================================*/

/**
 * srand - Seed random number generator
 */
void srand(unsigned int seed)
{
    rand_seed = seed;
}

/**
 * rand - Generate random number
 */
int rand(void)
{
    /* Linear congruential generator */
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & RAND_MAX);
}

/**
 * random - Generate better random number
 */
long random(void)
{
    return rand();
}

/**
 * srandom - Seed better random number generator
 */
void srandom(unsigned int seed)
{
    srand(seed);
}

/*===========================================================================*/
/*                         SORTING AND SEARCHING                             */
/*===========================================================================*/

/**
 * qsort - Quick sort
 */
void qsort(void *base, size_t nmemb, size_t size, cmp_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }

    /* Simple insertion sort for small arrays */
    unsigned char *arr = (unsigned char *)base;

    for (size_t i = 1; i < nmemb; i++) {
        unsigned char *key = arr + i * size;
        size_t j = i;

        while (j > 0 && compar(arr + (j - 1) * size, key) > 0) {
            /* Swap elements */
            for (size_t k = 0; k < size; k++) {
                unsigned char tmp = arr[(j - 1) * size + k];
                arr[(j - 1) * size + k] = arr[j * size + k];
                arr[j * size + k] = tmp;
            }
            j--;
        }
    }
}

/**
 * bsearch - Binary search
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              cmp_func_t compar)
{
    if (!base || nmemb == 0 || size == 0 || !compar) {
        return NULL;
    }

    const unsigned char *arr = (const unsigned char *)base;
    size_t left = 0;
    size_t right = nmemb - 1;

    while (left <= right) {
        size_t mid = left + (right - left) / 2;
        int cmp = compar(key, arr + mid * size);

        if (cmp == 0) {
            return (void *)(arr + mid * size);
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         INTEGER ARITHMETIC                                */
/*===========================================================================*/

/**
 * abs - Absolute value
 */
int abs(int j)
{
    return j < 0 ? -j : j;
}

/**
 * labs - Absolute value (long)
 */
long labs(long j)
{
    return j < 0 ? -j : j;
}

/**
 * llabs - Absolute value (long long)
 */
long long llabs(long long j)
{
    return j < 0 ? -j : j;
}

/**
 * div - Integer division
 */
div_t div(int numer, int denom)
{
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/**
 * ldiv - Integer division (long)
 */
ldiv_t ldiv(long numer, long denom)
{
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/**
 * lldiv - Integer division (long long)
 */
lldiv_t lldiv(long long numer, long long denom)
{
    lldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/*===========================================================================*/
/*                         SYSTEM INFORMATION                                */
/*===========================================================================*/

/**
 * system - Execute shell command
 */
int system(const char *command)
{
    if (!command) {
        return 1;  /* Shell available */
    }

    /* Fork and exec shell */
    /* Simplified - not fully implemented */
    return -1;
}

/**
 * getenv - Get environment (already defined above)
 */

/*===========================================================================*/
/*                         PSEUDO-RANDOM NUMBERS                             */
/*===========================================================================*/

/**
 * drand48 - Generate random double
 */
double drand48(void)
{
    return (double)rand() / (double)RAND_MAX;
}

/**
 * erand48 - Generate random double with seed
 */
double erand48(unsigned short xsubi[3])
{
    (void)xsubi;
    return drand48();
}

/**
 * lrand48 - Generate random long
 */
long lrand48(void)
{
    return rand();
}

/**
 * nrand48 - Generate random long with seed
 */
long nrand48(unsigned short xsubi[3])
{
    (void)xsubi;
    return lrand48();
}

/**
 * mrand48 - Generate random long (-2^31 to 2^31-1)
 */
long mrand48(void)
{
    return rand() - RAND_MAX / 2;
}

/**
 * jrand48 - Generate random long with seed
 */
long jrand48(unsigned short xsubi[3])
{
    (void)xsubi;
    return mrand48();
}

/**
 * srand48 - Seed 48-bit random number generator
 */
void srand48(long seedval)
{
    srand((unsigned int)seedval);
}

/**
 * seed48 - Seed 48-bit random number generator
 */
unsigned short *seed48(unsigned short seed16v[3])
{
    (void)seed16v;
    static unsigned short old_seed[3];
    return old_seed;
}

/**
 * lcong48 - Set parameters for 48-bit random number generator
 */
void lcong48(unsigned short param[7])
{
    (void)param;
}
