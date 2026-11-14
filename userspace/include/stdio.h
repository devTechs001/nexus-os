/*
 * NEXUS OS - userspace stdio.h
 * userspace/include/stdio.h
 *
 * There was no userspace libc header set at all. Each of string.c, stdlib.c
 * and stdio.c was a self-contained translation unit that redeclared whatever
 * it needed, so nothing shared a definition of FILE or of the standard
 * streams. stdlib.c calls fflush(stdout) and fprintf(stderr, ...) while having
 * no declaration of either, which only builds because implicit declarations
 * were tolerated - and would have produced a different FILE layout than
 * stdio.c uses if it ever had been included.
 *
 * This header is the single definition. FILE and the stream objects are
 * defined in stdio.c; everything else here is a declaration.
 */

#ifndef _NEXUS_USERSPACE_STDIO_H
#define _NEXUS_USERSPACE_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* File descriptor type used by the syscall layer. */
typedef int32_t fd_t;

/* Opaque to callers; defined in stdio.c. */
typedef struct nexus_file FILE;

/* Standard streams, defined in stdio.c. */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Standard descriptors. */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* End-of-file and error results. */
#ifndef EOF
#define EOF (-1)
#endif

/* Seek origins. */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* Buffer size used for the standard streams. */
#ifndef BUFSIZ
#define BUFSIZ          1024
#endif
#define BUFFER_SIZE     4096

/* setvbuf() buffering modes. */
#define _IONBF          0       /* unbuffered      */
#define _IOLBF          1       /* line buffered   */
#define _IOFBF          2       /* fully buffered  */

/* ---- formatted output ---- */
int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/* ---- character output ---- */
int putchar(int c);
int putc(int c, FILE *stream);
int fputc(int c, FILE *stream);
int puts(const char *s);
int fputs(const char *s, FILE *stream);

/* ---- character input ---- */
int getchar(void);
int getc(FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);

/* ---- blocks ---- */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* ---- files ---- */
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);

/* ---- misc ---- */
void perror(const char *s);

#endif /* _NEXUS_USERSPACE_STDIO_H */
