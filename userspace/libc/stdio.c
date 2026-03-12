/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * stdio.c - Standard Input/Output Library
 *
 * This module implements standard I/O functions for NEXUS OS userspace.
 * It provides printf, scanf, file I/O, and stream operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* File structure */
typedef struct {
    int fd;                     /* File descriptor */
    int flags;                  /* File flags */
    char *buffer;               /* I/O buffer */
    size_t buf_size;            /* Buffer size */
    size_t buf_pos;             /* Current position in buffer */
    size_t buf_count;           /* Valid bytes in buffer */
    bool eof;                   /* End of file flag */
    bool error;                 /* Error flag */
    bool is_buffered;           /* Buffered I/O flag */
} FILE;

/* File flags */
#define FILE_READ       0x0001
#define FILE_WRITE      0x0002
#define FILE_APPEND     0x0004
#define FILE_BINARY     0x0008
#define FILE_EOF        0x0010
#define FILE_ERROR      0x0020

/* Standard streams */
#define BUFFER_SIZE     4096

static char stdin_buffer[BUFFER_SIZE];
static char stdout_buffer[BUFFER_SIZE];
static char stderr_buffer[BUFFER_SIZE];

static FILE _stdin = {
    .fd = 0,
    .flags = FILE_READ,
    .buffer = stdin_buffer,
    .buf_size = BUFFER_SIZE,
    .buf_pos = 0,
    .buf_count = 0,
    .is_buffered = false,
};

static FILE _stdout = {
    .fd = 1,
    .flags = FILE_WRITE,
    .buffer = stdout_buffer,
    .buf_size = BUFFER_SIZE,
    .buf_pos = 0,
    .buf_count = 0,
    .is_buffered = true,
};

static FILE _stderr = {
    .fd = 2,
    .flags = FILE_WRITE,
    .buffer = stderr_buffer,
    .buf_size = BUFFER_SIZE,
    .buf_pos = 0,
    .buf_count = 0,
    .is_buffered = false,  /* Unbuffered */
};

FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

/* Standard file stream array */
#define MAX_OPEN_FILES  256
static FILE *file_streams[MAX_OPEN_FILES];

/*===========================================================================*/
/*                         EXTERNAL SYSCALLS                                 */
/*===========================================================================*/

extern int syscall_read(int fd, void *buf, size_t count);
extern int syscall_write(int fd, const void *buf, size_t count);
extern int syscall_open(const char *pathname, int flags, int mode);
extern int syscall_close(int fd);
extern long syscall_lseek(int fd, long offset, int whence);

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * _itoa - Convert integer to string
 */
static char *_itoa(char *buf, unsigned long n, int base, int width, char pad, bool upper)
{
    char tmp[33];
    char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    int len;

    if (n == 0) {
        tmp[i++] = '0';
    } else {
        while (n > 0) {
            tmp[i++] = digits[n % base];
            n /= base;
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

    buf[pos] = '\0';
    return buf;
}

/**
 * _ftoa - Convert float to string
 */
static char *_ftoa(char *buf, double value, int precision)
{
    /* Handle sign */
    if (value < 0) {
        *buf++ = '-';
        value = -value;
    }

    /* Integer part */
    unsigned long int_part = (unsigned long)value;
    char int_buf[32];
    _itoa(int_buf, int_part, 10, 0, '0', false);

    /* Copy integer part */
    char *p = int_buf;
    while (*p) {
        *buf++ = *p++;
    }

    /* Decimal part */
    if (precision > 0) {
        *buf++ = '.';

        double frac = value - int_part;
        for (int i = 0; i < precision; i++) {
            frac *= 10;
            int digit = (int)frac;
            *buf++ = '0' + digit;
            frac -= digit;
        }
    }

    *buf = '\0';
    return buf - strlen(buf);  /* Return start of string */
}

/*===========================================================================*/
/*                         FORMATTED OUTPUT                                  */
/*===========================================================================*/

/**
 * _vsnprintf - Format string with variable arguments
 */
static int _vsnprintf(char *str, size_t size, const char *fmt, va_list args)
{
    char *s = str;
    char *end = str + size - 1;

    for (; *fmt && s < end; fmt++) {
        if (*fmt != '%') {
            *s++ = *fmt;
            continue;
        }

        fmt++;

        /* Flags */
        bool left_align = false;
        bool show_sign = false;
        bool show_space = false;
        bool alt_form = false;
        bool zero_pad = false;
        int width = 0;
        int precision = -1;
        int length_mod = 0;  /* 0=none, 1=h, 2=hh, 3=l, 4=ll, 5=L */

        /* Parse flags */
        for (; *fmt; fmt++) {
            switch (*fmt) {
                case '-': left_align = true; continue;
                case '+': show_sign = true; continue;
                case ' ': show_space = true; continue;
                case '#': alt_form = true; continue;
                case '0': zero_pad = true; continue;
                default: break;
            }
            break;
        }

        /* Parse width */
        if (*fmt == '*') {
            width = va_arg(args, int);
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Parse precision */
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') {
                precision = va_arg(args, int);
                fmt++;
            } else {
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        /* Parse length modifier */
        if (*fmt == 'h') {
            length_mod = 1;
            fmt++;
            if (*fmt == 'h') {
                length_mod = 2;
                fmt++;
            }
        } else if (*fmt == 'l') {
            length_mod = 3;
            fmt++;
            if (*fmt == 'l') {
                length_mod = 4;
                fmt++;
            }
        } else if (*fmt == 'L') {
            length_mod = 5;
            fmt++;
        }

        /* Parse conversion specifier */
        char num_buf[64];
        const char *str_val;

        switch (*fmt) {
            case 'd':
            case 'i': {
                long val;
                if (length_mod == 4) val = va_arg(args, long long);
                else if (length_mod == 3) val = va_arg(args, long);
                else if (length_mod == 1) val = (short)va_arg(args, int);
                else if (length_mod == 2) val = (signed char)va_arg(args, int);
                else val = va_arg(args, int);

                if (val < 0) {
                    *s++ = '-';
                    val = -val;
                } else if (show_sign) {
                    *s++ = '+';
                } else if (show_space) {
                    *s++ = ' ';
                }

                _itoa(num_buf, (unsigned long)val, 10, width - (s - str),
                      zero_pad && !left_align ? '0' : ' ', false);

                for (char *p = num_buf; *p && s < end; p++) {
                    *s++ = *p;
                }
                break;
            }

            case 'u': {
                unsigned long val;
                if (length_mod == 4) val = va_arg(args, unsigned long long);
                else if (length_mod == 3) val = va_arg(args, unsigned long);
                else if (length_mod == 1) val = (unsigned short)va_arg(args, int);
                else if (length_mod == 2) val = (unsigned char)va_arg(args, int);
                else val = va_arg(args, unsigned int);

                _itoa(num_buf, val, 10, width, zero_pad && !left_align ? '0' : ' ', false);

                for (char *p = num_buf; *p && s < end; p++) {
                    *s++ = *p;
                }
                break;
            }

            case 'x':
            case 'X': {
                unsigned long val;
                if (length_mod == 4) val = va_arg(args, unsigned long long);
                else if (length_mod == 3) val = va_arg(args, unsigned long);
                else val = va_arg(args, unsigned int);

                if (alt_form && val != 0) {
                    *s++ = '0';
                    *s++ = *fmt;
                }

                _itoa(num_buf, val, 16, width - (s - str),
                      zero_pad && !left_align ? '0' : ' ', *fmt == 'X');

                for (char *p = num_buf; *p && s < end; p++) {
                    *s++ = *p;
                }
                break;
            }

            case 'o': {
                unsigned long val = va_arg(args, unsigned long);

                if (alt_form && val != 0) {
                    *s++ = '0';
                }

                _itoa(num_buf, val, 8, width, zero_pad && !left_align ? '0' : ' ', false);

                for (char *p = num_buf; *p && s < end; p++) {
                    *s++ = *p;
                }
                break;
            }

            case 'p': {
                unsigned long val = (unsigned long)va_arg(args, void *);

                *s++ = '0';
                *s++ = 'x';

                _itoa(num_buf, val, 16, 16, '0', false);

                for (char *p = num_buf; *p && s < end; p++) {
                    *s++ = *p;
                }
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                *s++ = c;
                break;
            }

            case 's': {
                str_val = va_arg(args, const char *);
                if (!str_val) str_val = "(null)";

                int len = 0;
                const char *p = str_val;
                while (*p++) len++;

                if (precision >= 0 && len > precision) {
                    len = precision;
                }

                if (!left_align) {
                    while (width > len && s < end) {
                        *s++ = ' ';
                        width--;
                    }
                }

                for (int i = 0; i < len && s < end; i++) {
                    *s++ = str_val[i];
                }

                if (left_align) {
                    while (width > len && s < end) {
                        *s++ = ' ';
                        width--;
                    }
                }
                break;
            }

            case 'f': {
                double val = va_arg(args, double);

                if (precision < 0) precision = 6;

                char *start = s;
                _ftoa(s, val, precision);

                while (*s) s++;

                /* Apply width */
                /* Simplified - full implementation would handle padding */
                break;
            }

            case '%':
                *s++ = '%';
                break;

            case 'n': {
                int *ptr = va_arg(args, int *);
                *ptr = (int)(s - str);
                break;
            }

            default:
                *s++ = '%';
                *s++ = *fmt;
                break;
        }
    }

    *s = '\0';
    return (int)(s - str);
}

/**
 * vsnprintf - Format string with variable arguments (bounded)
 */
int vsnprintf(char *str, size_t size, const char *fmt, va_list args)
{
    if (!str || size == 0) {
        return 0;
    }

    return _vsnprintf(str, size, fmt, args);
}

/**
 * snprintf - Format string (bounded)
 */
int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(str, size, fmt, args);
    va_end(args);
    return ret;
}

/**
 * vsprintf - Format string (unbounded)
 */
int vsprintf(char *str, const char *fmt, va_list args)
{
    return _vsnprintf(str, SIZE_MAX, fmt, args);
}

/**
 * sprintf - Format string (unbounded)
 */
int sprintf(char *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsprintf(str, fmt, args);
    va_end(args);
    return ret;
}

/**
 * vprintf - Print formatted output to stdout
 */
int vprintf(const char *fmt, va_list args)
{
    char buffer[4096];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        fwrite(buffer, 1, len, stdout);
    }

    return len;
}

/**
 * printf - Print formatted output
 */
int printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}

/**
 * vfprintf - Print formatted output to file
 */
int vfprintf(FILE *stream, const char *fmt, va_list args)
{
    char buffer[4096];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        fwrite(buffer, 1, len, stream);
    }

    return len;
}

/**
 * fprintf - Print formatted output to file
 */
int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(stream, fmt, args);
    va_end(args);
    return ret;
}

/**
 * vfprintf - Print formatted output to stderr
 */
int vfprintf_stderr(const char *fmt, va_list args)
{
    char buffer[4096];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        fwrite(buffer, 1, len, stderr);
    }

    return len;
}

/**
 * fprintf - Print formatted output to stderr
 */
int fprintf_stderr(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf_stderr(fmt, args);
    va_end(args);
    return ret;
}

/*===========================================================================*/
/*                         FILE STREAM OPERATIONS                            */
/*===========================================================================*/

/**
 * fopen - Open file stream
 */
FILE *fopen(const char *pathname, const char *mode)
{
    int flags = 0;
    int fd;

    /* Parse mode string */
    while (*mode) {
        switch (*mode) {
            case 'r':
                flags |= FILE_READ;
                break;
            case 'w':
                flags |= FILE_WRITE;
                break;
            case 'a':
                flags |= FILE_APPEND | FILE_WRITE;
                break;
            case '+':
                /* Read-write mode */
                if (flags & FILE_READ) flags |= FILE_WRITE;
                else flags |= FILE_READ;
                break;
            case 'b':
                flags |= FILE_BINARY;
                break;
        }
        mode++;
    }

    /* Open file */
    fd = syscall_open(pathname, flags, 0644);
    if (fd < 0) {
        return NULL;
    }

    /* Allocate FILE structure */
    FILE *stream = (FILE *)malloc(sizeof(FILE));
    if (!stream) {
        syscall_close(fd);
        return NULL;
    }

    /* Initialize stream */
    stream->fd = fd;
    stream->flags = flags;
    stream->buffer = (char *)malloc(BUFFER_SIZE);
    stream->buf_size = BUFFER_SIZE;
    stream->buf_pos = 0;
    stream->buf_count = 0;
    stream->eof = false;
    stream->error = false;
    stream->is_buffered = !(flags & FILE_BINARY);

    if (!stream->buffer) {
        free(stream);
        syscall_close(fd);
        return NULL;
    }

    /* Store in file stream array */
    if (fd < MAX_OPEN_FILES) {
        file_streams[fd] = stream;
    }

    return stream;
}

/**
 * fclose - Close file stream
 */
int fclose(FILE *stream)
{
    if (!stream) {
        return EOF;
    }

    /* Flush write buffer */
    if (stream->flags & FILE_WRITE) {
        fflush(stream);
    }

    /* Free buffer */
    if (stream->buffer) {
        free(stream->buffer);
    }

    /* Close file descriptor */
    if (stream->fd < MAX_OPEN_FILES) {
        file_streams[stream->fd] = NULL;
    }

    int ret = syscall_close(stream->fd);

    /* Free stream structure */
    free(stream);

    return ret;
}

/**
 * fflush - Flush file stream buffer
 */
int fflush(FILE *stream)
{
    if (!stream || !(stream->flags & FILE_WRITE)) {
        return EOF;
    }

    if (stream->buf_pos > 0) {
        int written = syscall_write(stream->fd, stream->buffer, stream->buf_pos);
        if (written < 0) {
            stream->error = true;
            return EOF;
        }
        stream->buf_pos = 0;
    }

    return 0;
}

/**
 * fread - Read from file stream
 */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || !(stream->flags & FILE_READ)) {
        return 0;
    }

    size_t total = size * nmemb;
    size_t nread = 0;
    char *buf = (char *)ptr;

    while (nread < total) {
        /* Read from buffer first */
        while (stream->buf_pos < stream->buf_count && nread < total) {
            buf[nread++] = stream->buffer[stream->buf_pos++];
        }

        if (nread >= total) {
            break;
        }

        /* Buffer empty, read more */
        if (stream->is_buffered) {
            int ret = syscall_read(stream->fd, stream->buffer, stream->buf_size);
            if (ret <= 0) {
                stream->eof = true;
                break;
            }
            stream->buf_count = ret;
            stream->buf_pos = 0;
        } else {
            /* Unbuffered read */
            int ret = syscall_read(stream->fd, buf + nread, total - nread);
            if (ret <= 0) {
                stream->eof = true;
                break;
            }
            nread += ret;
        }
    }

    return nread / size;
}

/**
 * fwrite - Write to file stream
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || !(stream->flags & FILE_WRITE)) {
        return 0;
    }

    size_t total = size * nmemb;
    size_t nwritten = 0;
    const char *buf = (const char *)ptr;

    while (nwritten < total) {
        if (stream->is_buffered) {
            /* Write to buffer */
            while (stream->buf_pos < stream->buf_size && nwritten < total) {
                stream->buffer[stream->buf_pos++] = buf[nwritten++];
            }

            /* Buffer full, flush */
            if (stream->buf_pos >= stream->buf_size) {
                int ret = syscall_write(stream->fd, stream->buffer, stream->buf_pos);
                if (ret < 0) {
                    stream->error = true;
                    return nwritten / size;
                }
                stream->buf_pos = 0;
            }
        } else {
            /* Unbuffered write */
            int ret = syscall_write(stream->fd, buf + nwritten, total - nwritten);
            if (ret < 0) {
                stream->error = true;
                return nwritten / size;
            }
            nwritten += ret;
        }
    }

    return nwritten / size;
}

/**
 * fgetc - Get character from stream
 */
int fgetc(FILE *stream)
{
    unsigned char c;

    if (fread(&c, 1, 1, stream) != 1) {
        return EOF;
    }

    return (int)c;
}

/**
 * fputc - Put character to stream
 */
int fputc(int c, FILE *stream)
{
    unsigned char ch = (unsigned char)c;

    if (fwrite(&ch, 1, 1, stream) != 1) {
        return EOF;
    }

    return c;
}

/**
 * fgets - Get string from stream
 */
char *fgets(char *s, int n, FILE *stream)
{
    if (!s || n <= 0 || !stream) {
        return NULL;
    }

    int i = 0;

    while (i < n - 1) {
        int c = fgetc(stream);

        if (c == EOF) {
            if (i == 0) {
                return NULL;
            }
            break;
        }

        s[i++] = (char)c;

        if (c == '\n') {
            break;
        }
    }

    s[i] = '\0';
    return s;
}

/**
 * fputs - Put string to stream
 */
int fputs(const char *s, FILE *stream)
{
    if (!s || !stream) {
        return EOF;
    }

    size_t len = 0;
    const char *p = s;
    while (*p++) len++;

    if (fwrite(s, 1, len, stream) != len) {
        return EOF;
    }

    return 0;
}

/**
 * gets - Get string from stdin (deprecated, use fgets)
 */
char *gets(char *s)
{
    return fgets(s, INT_MAX, stdin);
}

/**
 * puts - Put string to stdout with newline
 */
int puts(const char *s)
{
    if (!s) {
        return EOF;
    }

    int ret = fputs(s, stdout);
    if (ret != EOF) {
        fputc('\n', stdout);
    }

    return ret;
}

/**
 * getchar - Get character from stdin
 */
int getchar(void)
{
    return fgetc(stdin);
}

/**
 * putchar - Put character to stdout
 */
int putchar(int c)
{
    return fputc(c, stdout);
}

/*===========================================================================*/
/*                         FILE POSITIONING                                  */
/*===========================================================================*/

/**
 * fseek - Seek to position in file
 */
int fseek(FILE *stream, long offset, int whence)
{
    if (!stream) {
        return -1;
    }

    /* Flush write buffer */
    if (stream->flags & FILE_WRITE) {
        fflush(stream);
    }

    /* Reset read buffer */
    stream->buf_pos = 0;
    stream->buf_count = 0;
    stream->eof = false;

    /* Seek in file */
    long ret = syscall_lseek(stream->fd, offset, whence);
    if (ret < 0) {
        stream->error = true;
        return -1;
    }

    return 0;
}

/**
 * ftell - Get current position in file
 */
long ftell(FILE *stream)
{
    if (!stream) {
        return -1;
    }

    return syscall_lseek(stream->fd, 0, 1);  /* SEEK_CUR */
}

/**
 * rewind - Rewind file to beginning
 */
void rewind(FILE *stream)
{
    if (stream) {
        fseek(stream, 0, 0);  /* SEEK_SET */
    }
}

/**
 * feof - Check end-of-file indicator
 */
int feof(FILE *stream)
{
    return stream ? stream->eof : 0;
}

/**
 * ferror - Check error indicator
 */
int ferror(FILE *stream)
{
    return stream ? stream->error : 0;
}

/**
 * clearerr - Clear error and EOF indicators
 */
void clearerr(FILE *stream)
{
    if (stream) {
        stream->eof = false;
        stream->error = false;
    }
}

/*===========================================================================*/
/*                         BUFFER CONTROL                                    */
/*===========================================================================*/

/**
 * setbuf - Set file buffering
 */
void setbuf(FILE *stream, char *buf)
{
    if (stream) {
        if (buf) {
            stream->buffer = buf;
            stream->buf_size = BUFSIZ;
            stream->is_buffered = true;
        } else {
            stream->is_buffered = false;
        }
    }
}

/**
 * setvbuf - Set file buffering with mode
 */
int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
    if (!stream) {
        return -1;
    }

    switch (mode) {
        case _IONBF:  /* Unbuffered */
            stream->is_buffered = false;
            break;

        case _IOLBF:  /* Line buffered */
        case _IOFBF:  /* Fully buffered */
            if (buf) {
                stream->buffer = buf;
            } else {
                stream->buffer = (char *)malloc(size);
                if (!stream->buffer) {
                    return -1;
                }
            }
            stream->buf_size = size;
            stream->is_buffered = true;
            break;

        default:
            return -1;
    }

    return 0;
}

/*===========================================================================*/
/*                         FILE DESCRIPTOR OPERATIONS                        */
/*===========================================================================*/

/**
 * fileno - Get file descriptor from stream
 */
int fileno(FILE *stream)
{
    return stream ? stream->fd : -1;
}

/**
 * fdopen - Open stream from file descriptor
 */
FILE *fdopen(int fd, const char *mode)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return NULL;
    }

    FILE *stream = (FILE *)malloc(sizeof(FILE));
    if (!stream) {
        return NULL;
    }

    stream->fd = fd;
    stream->flags = 0;

    /* Parse mode */
    while (*mode) {
        switch (*mode) {
            case 'r': stream->flags |= FILE_READ; break;
            case 'w': stream->flags |= FILE_WRITE; break;
            case 'a': stream->flags |= FILE_APPEND; break;
            case 'b': stream->flags |= FILE_BINARY; break;
        }
        mode++;
    }

    stream->buffer = (char *)malloc(BUFFER_SIZE);
    stream->buf_size = BUFFER_SIZE;
    stream->buf_pos = 0;
    stream->buf_count = 0;
    stream->eof = false;
    stream->error = false;
    stream->is_buffered = !(stream->flags & FILE_BINARY);

    if (!stream->buffer) {
        free(stream);
        return NULL;
    }

    file_streams[fd] = stream;
    return stream;
}

/*===========================================================================*/
/*                         CONSTANTS                                         */
/*===========================================================================*/

#define EOF         (-1)
#define BUFSIZ      4096
#define FOPEN_MAX   256
#define FILENAME_MAX 256
#define L_tmpnam    256
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
