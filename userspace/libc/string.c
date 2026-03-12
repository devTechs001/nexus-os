/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * string.c - String Manipulation Functions
 *
 * This module implements string manipulation functions for NEXUS OS userspace.
 * It provides string copying, concatenation, comparison, searching, and memory operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Size type */
typedef uint64_t size_t;

/* NULL pointer */
#ifndef NULL
#define NULL ((void *)0)
#endif

/*===========================================================================*/
/*                         MEMORY OPERATIONS                                 */
/*===========================================================================*/

/**
 * memcpy - Copy memory area
 * @dest: Destination memory area
 * @src: Source memory area
 * @n: Number of bytes to copy
 *
 * Copies n bytes from memory area src to memory area dest.
 * The memory areas must not overlap.
 *
 * Returns: Pointer to dest
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    /* Copy word-sized chunks when possible */
    while (n >= sizeof(unsigned long)) {
        *(unsigned long *)d = *(const unsigned long *)s;
        d += sizeof(unsigned long);
        s += sizeof(unsigned long);
        n -= sizeof(unsigned long);
    }

    /* Copy remaining bytes */
    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

/**
 * memmove - Move memory area (handles overlap)
 * @dest: Destination memory area
 * @src: Source memory area
 * @n: Number of bytes to move
 *
 * Copies n bytes from memory area src to memory area dest.
 * Handles overlapping memory areas correctly.
 *
 * Returns: Pointer to dest
 */
void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        /* Copy forward */
        while (n >= sizeof(unsigned long)) {
            *(unsigned long *)d = *(const unsigned long *)s;
            d += sizeof(unsigned long);
            s += sizeof(unsigned long);
            n -= sizeof(unsigned long);
        }
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Copy backward */
        d += n;
        s += n;
        while (n >= sizeof(unsigned long)) {
            d -= sizeof(unsigned long);
            s -= sizeof(unsigned long);
            *(unsigned long *)d = *(const unsigned long *)s;
            n -= sizeof(unsigned long);
        }
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

/**
 * memset - Fill memory area with byte
 * @s: Memory area to fill
 * @c: Byte value to fill with
 * @n: Number of bytes to fill
 *
 * Fills the first n bytes of memory area s with byte c.
 *
 * Returns: Pointer to s
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned char value = (unsigned char)c;

    /* Fill word-sized chunks when possible */
    unsigned long value_long = value;
    value_long |= value_long << 8;
    value_long |= value_long << 16;
    value_long |= value_long << 32;

    while (n >= sizeof(unsigned long)) {
        *(unsigned long *)p = value_long;
        p += sizeof(unsigned long);
        n -= sizeof(unsigned long);
    }

    /* Fill remaining bytes */
    while (n--) {
        *p++ = value;
    }

    return s;
}

/**
 * memcmp - Compare memory areas
 * @s1: First memory area
 * @s2: Second memory area
 * @n: Number of bytes to compare
 *
 * Compares the first n bytes of memory areas s1 and s2.
 *
 * Returns:
 *   < 0 if s1 is less than s2
 *   0 if s1 equals s2
 *   > 0 if s1 is greater than s2
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

/**
 * memchr - Scan memory area for byte
 * @s: Memory area to scan
 * @c: Byte to search for
 * @n: Number of bytes to scan
 *
 * Scans the first n bytes of memory area s for byte c.
 *
 * Returns: Pointer to first occurrence of c, or NULL if not found
 */
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned char value = (unsigned char)c;

    while (n--) {
        if (*p == value) {
            return (void *)p;
        }
        p++;
    }

    return NULL;
}

/**
 * memccpy - Copy memory area until byte found
 * @dest: Destination memory area
 * @src: Source memory area
 * @c: Byte to stop at
 * @n: Maximum number of bytes to copy
 *
 * Copies bytes from src to dest until byte c is found or n bytes copied.
 *
 * Returns: Pointer to byte after c in dest, or NULL if c not found
 */
void *memccpy(void *dest, const void *src, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    unsigned char value = (unsigned char)c;

    while (n--) {
        *d++ = *s;
        if (*s == value) {
            return d;
        }
        s++;
    }

    return NULL;
}

/**
 * memmem - Find substring in memory
 * @haystack: Memory area to search
 * @haystack_len: Length of haystack
 * @needle: Memory area to find
 * @needle_len: Length of needle
 *
 * Searches for the first occurrence of needle in haystack.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
void *memmem(const void *haystack, size_t haystack_len,
             const void *needle, size_t needle_len)
{
    if (needle_len == 0) {
        return (void *)haystack;
    }

    if (needle_len > haystack_len) {
        return NULL;
    }

    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (h[i] == n[0] && memcmp(&h[i], n, needle_len) == 0) {
            return (void *)&h[i];
        }
    }

    return NULL;
}

/*===========================================================================*/
/*                         STRING LENGTH                                     */
/*===========================================================================*/

/**
 * strlen - Calculate string length
 * @s: Input string
 *
 * Calculates the length of string s, not including the null terminator.
 *
 * Returns: Length of string
 */
size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

/**
 * strnlen - Calculate string length with limit
 * @s: Input string
 * @maxlen: Maximum length to check
 *
 * Calculates the length of string s, but stops at maxlen.
 *
 * Returns: Length of string, or maxlen if null terminator not found
 */
size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && *s++) {
        len++;
    }
    return len;
}

/**
 * strnlen_s - Safe string length (C11)
 * @s: Input string
 * @maxsize: Maximum size to check
 *
 * Returns 0 if s is NULL, otherwise returns string length up to maxsize.
 *
 * Returns: Length of string, or 0 if s is NULL
 */
size_t strnlen_s(const char *s, size_t maxsize)
{
    if (!s) {
        return 0;
    }
    return strnlen(s, maxsize);
}

/*===========================================================================*/
/*                         STRING COPY                                       */
/*===========================================================================*/

/**
 * strcpy - Copy string
 * @dest: Destination string
 * @src: Source string
 *
 * Copies string src to dest, including null terminator.
 *
 * Returns: Pointer to dest
 */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * strncpy - Copy string with length limit
 * @dest: Destination string
 * @src: Source string
 * @n: Maximum number of bytes to copy
 *
 * Copies at most n bytes from src to dest.
 * If src is shorter than n, dest is padded with null bytes.
 * If src is longer than n, dest is not null-terminated.
 *
 * Returns: Pointer to dest
 */
char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;

    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }

    /* Pad with null bytes */
    while (n-- > 0) {
        *d++ = '\0';
    }

    return dest;
}

/**
 * strdup - Duplicate string
 * @s: String to duplicate
 *
 * Allocates memory and copies string s to new memory.
 *
 * Returns: Pointer to new string, or NULL on failure
 */
char *strdup(const char *s)
{
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s) + 1;
    char *dup = (char *)malloc(len);

    if (dup) {
        memcpy(dup, s, len);
    }

    return dup;
}

/**
 * strndup - Duplicate string with length limit
 * @s: String to duplicate
 * @n: Maximum number of bytes to copy
 *
 * Allocates memory and copies at most n bytes from string s.
 *
 * Returns: Pointer to new string, or NULL on failure
 */
char *strndup(const char *s, size_t n)
{
    if (!s) {
        return NULL;
    }

    size_t len = strnlen(s, n) + 1;
    char *dup = (char *)malloc(len);

    if (dup) {
        memcpy(dup, s, len - 1);
        dup[len - 1] = '\0';
    }

    return dup;
}

/**
 * strcpy_s - Safe string copy (C11)
 * @dest: Destination string
 * @destsz: Size of destination buffer
 * @src: Source string
 *
 * Safe version of strcpy that checks buffer bounds.
 *
 * Returns: 0 on success, non-zero on error
 */
int strcpy_s(char *dest, size_t destsz, const char *src)
{
    if (!dest || !src) {
        return -1;
    }

    size_t len = strlen(src);

    if (len >= destsz) {
        dest[0] = '\0';
        return -1;
    }

    memcpy(dest, src, len + 1);
    return 0;
}

/**
 * strncpy_s - Safe string copy with length (C11)
 * @dest: Destination string
 * @destsz: Size of destination buffer
 * @src: Source string
 * @n: Maximum number of bytes to copy
 *
 * Safe version of strncpy that checks buffer bounds.
 *
 * Returns: 0 on success, non-zero on error
 */
int strncpy_s(char *dest, size_t destsz, const char *src, size_t n)
{
    if (!dest || !src) {
        return -1;
    }

    size_t len = strnlen(src, n);

    if (len >= destsz) {
        dest[0] = '\0';
        return -1;
    }

    memcpy(dest, src, len);
    dest[len] = '\0';
    return 0;
}

/*===========================================================================*/
/*                         STRING CONCATENATION                              */
/*===========================================================================*/

/**
 * strcat - Concatenate strings
 * @dest: Destination string
 * @src: Source string
 *
 * Appends string src to dest, overwriting the null terminator of dest.
 *
 * Returns: Pointer to dest
 */
char *strcat(char *dest, const char *src)
{
    char *d = dest;

    /* Find end of dest */
    while (*d) {
        d++;
    }

    /* Copy src */
    while ((*d++ = *src++));

    return dest;
}

/**
 * strncat - Concatenate strings with length limit
 * @dest: Destination string
 * @src: Source string
 * @n: Maximum number of bytes to append
 *
 * Appends at most n bytes from src to dest.
 * Always null-terminates the result.
 *
 * Returns: Pointer to dest
 */
char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;

    /* Find end of dest */
    while (*d) {
        d++;
    }

    /* Copy at most n bytes */
    while (n-- > 0 && *src) {
        *d++ = *src++;
    }

    /* Null terminate */
    *d = '\0';

    return dest;
}

/**
 * strcat_s - Safe string concatenation (C11)
 * @dest: Destination string
 * @destsz: Size of destination buffer
 * @src: Source string
 *
 * Safe version of strcat that checks buffer bounds.
 *
 * Returns: 0 on success, non-zero on error
 */
int strcat_s(char *dest, size_t destsz, const char *src)
{
    if (!dest || !src) {
        return -1;
    }

    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    if (dest_len + src_len >= destsz) {
        dest[0] = '\0';
        return -1;
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

/**
 * strncat_s - Safe string concatenation with length (C11)
 * @dest: Destination string
 * @destsz: Size of destination buffer
 * @src: Source string
 * @n: Maximum number of bytes to append
 *
 * Safe version of strncat that checks buffer bounds.
 *
 * Returns: 0 on success, non-zero on error
 */
int strncat_s(char *dest, size_t destsz, const char *src, size_t n)
{
    if (!dest || !src) {
        return -1;
    }

    size_t dest_len = strlen(dest);
    size_t src_len = strnlen(src, n);

    if (dest_len + src_len >= destsz) {
        dest[0] = '\0';
        return -1;
    }

    memcpy(dest + dest_len, src, src_len);
    dest[dest_len + src_len] = '\0';
    return 0;
}

/*===========================================================================*/
/*                         STRING COMPARISON                                 */
/*===========================================================================*/

/**
 * strcmp - Compare strings
 * @s1: First string
 * @s2: Second string
 *
 * Compares strings s1 and s2 lexicographically.
 *
 * Returns:
 *   < 0 if s1 is less than s2
 *   0 if s1 equals s2
 *   > 0 if s1 is greater than s2
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/**
 * strncmp - Compare strings with length limit
 * @s1: First string
 * @s2: Second string
 * @n: Maximum number of bytes to compare
 *
 * Compares at most n bytes of strings s1 and s2.
 *
 * Returns:
 *   < 0 if s1 is less than s2
 *   0 if s1 equals s2 (up to n bytes)
 *   > 0 if s1 is greater than s2
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n-- > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }

    if (n == (size_t)-1) {
        return 0;
    }

    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/**
 * strcasecmp - Compare strings (case-insensitive)
 * @s1: First string
 * @s2: Second string
 *
 * Compares strings s1 and s2 lexicographically, ignoring case.
 *
 * Returns:
 *   < 0 if s1 is less than s2
 *   0 if s1 equals s2
 *   > 0 if s1 is greater than s2
 */
int strcasecmp(const char *s1, const char *s2)
{
    unsigned char c1, c2;

    do {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;

        /* Convert to lowercase */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

        if (c1 != c2) {
            return c1 - c2;
        }
    } while (c1);

    return 0;
}

/**
 * strncasecmp - Compare strings with length limit (case-insensitive)
 * @s1: First string
 * @s2: Second string
 * @n: Maximum number of bytes to compare
 *
 * Compares at most n bytes of strings s1 and s2, ignoring case.
 *
 * Returns:
 *   < 0 if s1 is less than s2
 *   0 if s1 equals s2 (up to n bytes)
 *   > 0 if s1 is greater than s2
 */
int strncasecmp(const char *s1, const char *s2, size_t n)
{
    unsigned char c1, c2;

    while (n-- > 0) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;

        /* Convert to lowercase */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

        if (c1 != c2) {
            return c1 - c2;
        }

        if (c1 == '\0') {
            return 0;
        }
    }

    return 0;
}

/**
 * strcmp_s - Safe string comparison (C11)
 * @s1: First string
 * @s1max: Maximum length of s1
 * @s2: Second string
 * @diff: Pointer to store difference
 *
 * Safe version of strcmp that checks for null pointers.
 *
 * Returns: 0 on success, non-zero on error
 */
int strcmp_s(const char *s1, size_t s1max, const char *s2, int *diff)
{
    if (!s1 || !s2 || !diff) {
        return -1;
    }

    *diff = 0;

    while (s1max-- > 0 && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }

    *diff = *(const unsigned char *)s1 - *(const unsigned char *)s2;
    return 0;
}

/*===========================================================================*/
/*                         STRING SEARCH                                     */
/*===========================================================================*/

/**
 * strchr - Find character in string
 * @s: String to search
 * @c: Character to find
 *
 * Finds the first occurrence of character c in string s.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strchr(const char *s, int c)
{
    unsigned char value = (unsigned char)c;

    while (*s) {
        if (*s == value) {
            return (char *)s;
        }
        s++;
    }

    /* Check for null terminator */
    if (value == '\0') {
        return (char *)s;
    }

    return NULL;
}

/**
 * strrchr - Find last occurrence of character
 * @s: String to search
 * @c: Character to find
 *
 * Finds the last occurrence of character c in string s.
 *
 * Returns: Pointer to last occurrence, or NULL if not found
 */
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    unsigned char value = (unsigned char)c;

    do {
        if (*s == value) {
            last = s;
        }
    } while (*s++);

    return (char *)last;
}

/**
 * strstr - Find substring
 * @haystack: String to search in
 * @needle: Substring to find
 *
 * Finds the first occurrence of substring needle in haystack.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strstr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);

    if (needle_len == 0) {
        return (char *)haystack;
    }

    while (*haystack) {
        if (*haystack == *needle &&
            strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }

    return NULL;
}

/**
 * strcasestr - Find substring (case-insensitive)
 * @haystack: String to search in
 * @needle: Substring to find
 *
 * Finds the first occurrence of substring needle in haystack,
 * ignoring case.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strcasestr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);

    if (needle_len == 0) {
        return (char *)haystack;
    }

    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }

    return NULL;
}

/**
 * strpbrk - Find any character from set
 * @s: String to search
 * @accept: Set of characters to find
 *
 * Finds the first occurrence of any character from accept in s.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *strpbrk(const char *s, const char *accept)
{
    while (*s) {
        const char *a = accept;
        while (*a) {
            if (*s == *a) {
                return (char *)s;
            }
            a++;
        }
        s++;
    }

    return NULL;
}

/**
 * strspn - Get span of characters from set
 * @s: String to scan
 * @accept: Set of characters to match
 *
 * Calculates the length of the initial segment of s that
 * consists entirely of characters from accept.
 *
 * Returns: Length of matching segment
 */
size_t strspn(const char *s, const char *accept)
{
    size_t count = 0;

    while (*s) {
        const char *a = accept;
        bool found = false;

        while (*a) {
            if (*s == *a) {
                found = true;
                break;
            }
            a++;
        }

        if (!found) {
            break;
        }

        s++;
        count++;
    }

    return count;
}

/**
 * strcspn - Get span of characters not from set
 * @s: String to scan
 * @reject: Set of characters to reject
 *
 * Calculates the length of the initial segment of s that
 * consists entirely of characters not in reject.
 *
 * Returns: Length of matching segment
 */
size_t strcspn(const char *s, const char *reject)
{
    size_t count = 0;

    while (*s) {
        const char *r = reject;

        while (*r) {
            if (*s == *r) {
                return count;
            }
            r++;
        }

        s++;
        count++;
    }

    return count;
}

/**
 * strtok - Tokenize string
 * @str: String to tokenize (NULL for subsequent calls)
 * @delim: Delimiter characters
 *
 * Splits string str into tokens separated by characters in delim.
 * Subsequent calls should pass NULL as str to continue tokenizing.
 *
 * Returns: Pointer to next token, or NULL if no more tokens
 */
char *strtok(char *str, const char *delim)
{
    static char *saveptr = NULL;
    char *token;

    if (str == NULL) {
        str = saveptr;
    }

    if (str == NULL) {
        return NULL;
    }

    /* Skip leading delimiters */
    str += strspn(str, delim);

    if (*str == '\0') {
        saveptr = NULL;
        return NULL;
    }

    /* Find end of token */
    token = str;
    str += strcspn(str, delim);

    if (*str == '\0') {
        saveptr = NULL;
    } else {
        *str = '\0';
        saveptr = str + 1;
    }

    return token;
}

/**
 * strtok_r - Reentrant tokenize string
 * @str: String to tokenize (NULL for subsequent calls)
 * @delim: Delimiter characters
 * @saveptr: Pointer to save state
 *
 * Reentrant version of strtok.
 *
 * Returns: Pointer to next token, or NULL if no more tokens
 */
char *strtok_r(char *str, const char *delim, char **saveptr)
{
    char *token;

    if (str == NULL) {
        str = *saveptr;
    }

    if (str == NULL) {
        return NULL;
    }

    /* Skip leading delimiters */
    str += strspn(str, delim);

    if (*str == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    /* Find end of token */
    token = str;
    str += strcspn(str, delim);

    if (*str == '\0') {
        *saveptr = NULL;
    } else {
        *str = '\0';
        *saveptr = str + 1;
    }

    return token;
}

/*===========================================================================*/
/*                         STRING FORMATTING                                 */
/*===========================================================================*/

/**
 * strcoll - Collate strings
 * @s1: First string
 * @s2: Second string
 *
 * Compares strings according to locale collation.
 * (Simplified - uses strcmp)
 *
 * Returns: Comparison result
 */
int strcoll(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

/**
 * strxfrm - Transform string for collation
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum bytes to write
 *
 * Transforms src for locale-aware comparison.
 * (Simplified - copies string)
 *
 * Returns: Length of transformed string
 */
size_t strxfrm(char *dest, const char *src, size_t n)
{
    size_t len = strlen(src);

    if (n > 0 && dest) {
        strncpy(dest, src, n);
    }

    return len;
}

/*===========================================================================*/
/*                         MEMORY SET/SEARCH                                 */
/*===========================================================================*/

/**
 * explicit_bzero - Securely zero memory
 * @s: Memory area to zero
 * @n: Number of bytes to zero
 *
 * Zeros memory in a way that won't be optimized away.
 */
void explicit_bzero(void *s, size_t n)
{
    volatile unsigned char *p = (volatile unsigned char *)s;

    while (n--) {
        *p++ = 0;
    }
}

/**
 * bzero - Zero memory (deprecated)
 * @s: Memory area to zero
 * @n: Number of bytes to zero
 */
void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}

/**
 * bcopy - Copy memory (deprecated)
 * @src: Source memory
 * @dest: Destination memory
 * @n: Number of bytes to copy
 */
void bcopy(const void *src, void *dest, size_t n)
{
    memmove(dest, src, n);
}

/**
 * bcmp - Compare memory (deprecated)
 * @s1: First memory area
 * @s2: Second memory area
 * @n: Number of bytes to compare
 *
 * Returns: 0 if equal, non-zero otherwise
 */
int bcmp(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}

/**
 * index - Find character in string (deprecated)
 * @s: String to search
 * @c: Character to find
 *
 * Same as strchr.
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
char *index(const char *s, int c)
{
    return strchr(s, c);
}

/**
 * rindex - Find last occurrence of character (deprecated)
 * @s: String to search
 * @c: Character to find
 *
 * Same as strrchr.
 *
 * Returns: Pointer to last occurrence, or NULL if not found
 */
char *rindex(const char *s, int c)
{
    return strrchr(s, c);
}

/*===========================================================================*/
/*                         WIDE CHARACTER SUPPORT                            */
/*===========================================================================*/

/**
 * strlen_w - Calculate wide string length
 * @s: Wide string
 *
 * Returns: Length of wide string
 */
size_t wcslen(const wchar_t *s)
{
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

/**
 * wcscpy - Copy wide string
 * @dest: Destination wide string
 * @src: Source wide string
 *
 * Returns: Pointer to dest
 */
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
    wchar_t *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * wcsncpy - Copy wide string with limit
 * @dest: Destination wide string
 * @src: Source wide string
 * @n: Maximum characters to copy
 *
 * Returns: Pointer to dest
 */
wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n)
{
    wchar_t *d = dest;

    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }

    while (n-- > 0) {
        *d++ = L'\0';
    }

    return dest;
}

/**
 * wcscat - Concatenate wide strings
 * @dest: Destination wide string
 * @src: Source wide string
 *
 * Returns: Pointer to dest
 */
wchar_t *wcscat(wchar_t *dest, const wchar_t *src)
{
    wchar_t *d = dest;

    while (*d) {
        d++;
    }

    while ((*d++ = *src++));

    return dest;
}

/**
 * wcscmp - Compare wide strings
 * @s1: First wide string
 * @s2: Second wide string
 *
 * Returns: Comparison result
 */
int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * wcsncmp - Compare wide strings with limit
 * @s1: First wide string
 * @s2: Second wide string
 * @n: Maximum characters to compare
 *
 * Returns: Comparison result
 */
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
    while (n-- > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }

    if (n == (size_t)-1) {
        return 0;
    }

    return *s1 - *s2;
}

/**
 * wcschr - Find character in wide string
 * @s: Wide string to search
 * @c: Character to find
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
    while (*s) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        s++;
    }

    if (c == L'\0') {
        return (wchar_t *)s;
    }

    return NULL;
}

/**
 * wcsrchr - Find last occurrence in wide string
 * @s: Wide string to search
 * @c: Character to find
 *
 * Returns: Pointer to last occurrence, or NULL if not found
 */
wchar_t *wcsrchr(const wchar_t *s, wchar_t c)
{
    const wchar_t *last = NULL;

    do {
        if (*s == c) {
            last = s;
        }
    } while (*s++);

    return (wchar_t *)last;
}

/**
 * wmemchr - Find character in wide memory
 * @s: Wide memory area
 * @c: Character to find
 * @n: Number of characters to search
 *
 * Returns: Pointer to first occurrence, or NULL if not found
 */
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    while (n--) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        s++;
    }

    return NULL;
}

/**
 * wmemcmp - Compare wide memory areas
 * @s1: First wide memory area
 * @s2: Second wide memory area
 * @n: Number of characters to compare
 *
 * Returns: Comparison result
 */
int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
    while (n--) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }

    return 0;
}

/**
 * wmemcpy - Copy wide memory
 * @dest: Destination wide memory
 * @src: Source wide memory
 * @n: Number of characters to copy
 *
 * Returns: Pointer to dest
 */
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n)
{
    wchar_t *d = dest;
    while (n--) {
        *d++ = *src++;
    }
    return dest;
}

/**
 * wmemmove - Move wide memory
 * @dest: Destination wide memory
 * @src: Source wide memory
 * @n: Number of characters to move
 *
 * Returns: Pointer to dest
 */
wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n)
{
    wchar_t *d = dest;
    const wchar_t *s = src;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

/**
 * wmemset - Fill wide memory
 * @s: Wide memory area
 * @c: Character to fill with
 * @n: Number of characters to fill
 *
 * Returns: Pointer to s
 */
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n)
{
    wchar_t *p = s;
    while (n--) {
        *p++ = c;
    }
    return s;
}
