/*
 * NEXUS OS - String Functions Implementation
 * kernel/core/string.c
 *
 * Real implementations of standard string functions for the kernel
 */

#include "../include/kernel.h"
#include "../include/types.h"
#include <stdarg.h>

/*===========================================================================*/
/*                         STRING FUNCTIONS                                  */
/*===========================================================================*/

/**
 * strlen - Calculate length of a string
 * @s: Input string
 *
 * Returns: Length of the string (not including null terminator)
 */
size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return p - s;
}

/**
 * strnlen - Calculate length of a string with maximum
 * @s: Input string
 * @max: Maximum number of characters to check
 *
 * Returns: Length of the string or max, whichever is smaller
 */
size_t strnlen(const char *s, size_t max)
{
    const char *p = s;
    size_t i = 0;
    while (i < max && *p) {
        p++;
        i++;
    }
    return i;
}

/**
 * strcpy - Copy one string to another
 * @dest: Destination buffer
 * @src: Source string
 *
 * Returns: Pointer to destination buffer
 */
char *strcpy(char *dest, const char *src)
{
    char *p = dest;
    while ((*p++ = *src++) != '\0')
        ;
    return dest;
}

/**
 * strncpy - Copy at most n characters from string
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum number of characters to copy
 *
 * Returns: Pointer to destination buffer
 */
char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

/**
 * strcmp - Compare two strings
 * @s1: First string
 * @s2: Second string
 *
 * Returns: 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * strncmp - Compare at most n characters of two strings
 * @s1: First string
 * @s2: Second string
 * @n: Maximum number of characters to compare
 *
 * Returns: 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return 0;
    while (n > 1 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * strcat - Concatenate two strings
 * @dest: Destination buffer (must be large enough)
 * @src: Source string to append
 *
 * Returns: Pointer to destination buffer
 */
char *strcat(char *dest, const char *src)
{
    char *p = dest;
    while (*p)
        p++;
    while ((*p++ = *src++) != '\0')
        ;
    return dest;
}

/**
 * strncat - Concatenate at most n characters
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum number of characters to append
 *
 * Returns: Pointer to destination buffer
 */
char *strncat(char *dest, const char *src, size_t n)
{
    char *p = dest;
    while (*p)
        p++;
    while (n > 0 && *src) {
        *p++ = *src++;
        n--;
    }
    *p = '\0';
    return dest;
}

/**
 * strchr - Find first occurrence of character in string
 * @s: String to search
 * @c: Character to find
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strchr(const char *s, int c)
{
    while (*s && (*s != (char)c))
        s++;
    return (*s == (char)c) ? (char *)s : NULL;
}

/**
 * strrchr - Find last occurrence of character in string
 * @s: String to search
 * @c: Character to find
 *
 * Returns: Pointer to last occurrence, or NULL if not found
 */
char *strrchr(const char *s, int c)
{
    const char *p = NULL;
    while (*s) {
        if (*s == (char)c)
            p = s;
        s++;
    }
    return p;
}

/**
 * strstr - Find first occurrence of substring
 * @haystack: String to search in
 * @needle: Substring to find
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strstr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);
    size_t i;

    if (needle_len == 0)
        return (char *)haystack;

    while (*haystack) {
        for (i = 0; i < needle_len && haystack[i] == needle[i]; i++)
            ;
        if (i == needle_len)
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/**
 * memcpy - Copy memory area
 * @dest: Destination buffer
 * @src: Source buffer
 * @n: Number of bytes to copy
 *
 * Returns: Pointer to destination buffer
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

/**
 * memmove - Copy memory area with overlap handling
 * @dest: Destination buffer
 * @src: Source buffer
 * @n: Number of bytes to copy
 *
 * Returns: Pointer to destination buffer
 */
void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dest;
}

/**
 * memset - Fill memory with a constant byte
 * @s: Memory area to fill
 * @c: Constant byte
 * @n: Number of bytes to fill
 *
 * Returns: Pointer to memory area
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

/**
 * memcmp - Compare memory areas
 * @s1: First memory area
 * @s2: Second memory area
 * @n: Number of bytes to compare
 *
 * Returns: 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

/**
 * memchr - Find character in memory area
 * @s: Memory area to search
 * @c: Character to find
 * @n: Number of bytes to search
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c)
            return (void *)p;
        p++;
    }
    return NULL;
}

/*===========================================================================*/
/*                         FORMATTED OUTPUT                                  */
/*===========================================================================*/

/* Internal buffer for vsnprintf */
#define PRINTF_BUF_SIZE 256

static char *format_number(char *buf, unsigned long num, int base, int uppercase)
{
    static const char *digits_lc = "0123456789abcdef";
    static const char *digits_uc = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_uc : digits_lc;
    char tmp[32];
    char *p = tmp + sizeof(tmp);
    int i;

    *--p = '\0';
    do {
        *--p = digits[num % base];
        num /= base;
    } while (num != 0);

    for (i = 0; (buf[i] = p[i]) != '\0'; i++)
        ;
    return buf + i;
}

/**
 * vsnprintf - Format string with variable arguments
 * @buf: Output buffer
 * @size: Buffer size
 * @fmt: Format string
 * @args: Variable arguments
 *
 * Returns: Number of characters printed (excluding null terminator)
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    char *p = buf;
    char *end = buf + size - 1;
    char c;
    int i;
    long num;
    unsigned long unum;
    char *str;
    int width;
    int pad;
    int left_align;
    int zero_pad;

    while ((c = *fmt++) != '\0' && p < end) {
        if (c != '%') {
            *p++ = c;
            continue;
        }

        /* Parse flags */
        width = 0;
        pad = ' ';
        left_align = 0;
        zero_pad = 0;

        while (1) {
            if (*fmt == '-') {
                left_align = 1;
                fmt++;
            } else if (*fmt == '0') {
                zero_pad = 1;
                fmt++;
            } else {
                break;
            }
        }

        /* Parse width */
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier (simplified) */
        if (*fmt == 'l')
            fmt++;

        c = *fmt++;

        switch (c) {
        case 'c':
            *p++ = (char)va_arg(args, int);
            break;

        case 's':
            str = va_arg(args, char *);
            if (!str)
                str = "(null)";
            while (*str && p < end)
                *p++ = *str++;
            break;

        case 'd':
        case 'i':
            num = va_arg(args, int);
            if (num < 0) {
                if (p < end)
                    *p++ = '-';
                num = -num;
            }
            p = format_number(p, num, 10, 0);
            break;

        case 'u':
            unum = va_arg(args, unsigned int);
            p = format_number(p, unum, 10, 0);
            break;

        case 'x':
            unum = va_arg(args, unsigned int);
            p = format_number(p, unum, 16, 0);
            break;

        case 'X':
            unum = va_arg(args, unsigned int);
            p = format_number(p, unum, 16, 1);
            break;

        case 'p':
            unum = (unsigned long)va_arg(args, void *);
            if (p + 2 < end) {
                *p++ = '0';
                *p++ = 'x';
            }
            p = format_number(p, unum, 16, 0);
            break;

        case '%':
            *p++ = '%';
            break;

        default:
            *p++ = c;
            break;
        }

        (void)width;
        (void)pad;
        (void)left_align;
        (void)zero_pad;
    }

    *p = '\0';
    return p - buf;
}

/**
 * snprintf - Format string output
 * @buf: Output buffer
 * @size: Buffer size
 * @fmt: Format string
 *
 * Returns: Number of characters printed (excluding null terminator)
 */
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return ret;
}

/**
 * sprintf - Format string output (no size limit - use with caution)
 * @buf: Output buffer
 * @fmt: Format string
 *
 * Returns: Number of characters printed (excluding null terminator)
 */
int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, 0x7FFFFFFF, fmt, args);
    va_end(args);

    return ret;
}
