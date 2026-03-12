/*
 * NEXUS OS - Kernel Print Functions
 * kernel/core/printk.c
 */

#include "../include/kernel.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define PRINTK_BUFFER_SIZE  4096

static char printk_buffer[PRINTK_BUFFER_SIZE];

static const char hex_digits[] = "0123456789abcdef";
static const char HEX_DIGITS[] = "0123456789ABCDEF";

static int format_uint(char *buf, u64 value, int base, int width, char pad)
{
    char tmp[65];
    int i = 0;
    int len;

    if (value == 0) {
        tmp[i++] = '0';
    } else {
        while (value > 0) {
            tmp[i++] = hex_digits[value % base];
            value /= base;
        }
    }

    len = i;

    /* Reverse and pad */
    int pos = 0;
    int pad_count = width - len;
    
    while (pad_count-- > 0) {
        buf[pos++] = pad;
    }

    while (i > 0) {
        buf[pos++] = tmp[--i];
    }

    return pos;
}

int vprintk(const char *fmt, __builtin_va_list args)
{
    char *str = printk_buffer;
    int len = 0;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            *str++ = *fmt;
            len++;
            continue;
        }

        fmt++;

        /* Flags */
        char pad = ' ';
        int width = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        /* Width */
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        char tmp[128];
        int num_len = 0;

        switch (*fmt) {
        case 'd':
        case 'i': {
            s64 val = __builtin_va_arg(args, s64);
            if (val < 0) {
                *str++ = '-';
                val = -val;
            }
            num_len = format_uint(tmp, val, 10, width, pad);
            memcpy(str, tmp, num_len);
            str += num_len;
            len += num_len + (val < 0 ? 1 : 0);
            break;
        }

        case 'u': {
            u64 val = __builtin_va_arg(args, u64);
            num_len = format_uint(tmp, val, 10, width, pad);
            memcpy(str, tmp, num_len);
            str += num_len;
            len += num_len;
            break;
        }

        case 'x':
        case 'X': {
            u64 val = __builtin_va_arg(args, u64);
            num_len = format_uint(tmp, val, 16, width, pad);
            memcpy(str, tmp, num_len);
            str += num_len;
            len += num_len;
            break;
        }

        case 'p': {
            u64 val = (u64)__builtin_va_arg(args, void*);
            *str++ = '0';
            *str++ = 'x';
            len += 2;
            num_len = format_uint(tmp, val, 16, 16, '0');
            memcpy(str, tmp, num_len);
            str += num_len;
            len += num_len;
            break;
        }

        case 's': {
            const char *s = __builtin_va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s) {
                *str++ = *s++;
                len++;
            }
            break;
        }

        case 'c':
            *str++ = (char)__builtin_va_arg(args, int);
            len++;
            break;

        case '%':
            *str++ = '%';
            len++;
            break;

        default:
            *str++ = '%';
            *str++ = *fmt;
            len += 2;
            break;
        }
    }

    *str = '\0';

    /* Output via VGA console - printf not available in bare-metal */
    /* In real OS: write to serial port or framebuffer */
    (void)printk_buffer;  /* Suppress unused warning */

    return len;
}

int printk(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int ret = vprintk(fmt, args);
    __builtin_va_end(args);
    return ret;
}
