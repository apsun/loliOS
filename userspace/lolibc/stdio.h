#ifndef _LOLIBC_STDIO_H
#define _LOLIBC_STDIO_H

#include <stdarg.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/*
 * FILE implementation: holds a file descriptor and a
 * readahead buffer. No write buffer, since caching is
 * done by higher level APIs.
 */
typedef struct {
    int fd;
    char *buf;
    int offset;
    int count;
} FILE;

extern FILE __stdin;
extern FILE __stdout;
extern FILE __stderr;

#define stdin (&__stdin)
#define stdout (&__stdout)
#define stderr (&__stderr)

FILE *fdopen(int fd);
FILE *fopen(const char *name, const char *mode);
int fread(FILE *fp, void *buf, int size);
int fwrite(FILE *fp, const void *buf, int size);
int fclose(FILE *fp);

int fputc(char c, FILE *fp);
int fputs(const char *s, FILE *fp);
int putchar(char c);
int puts(const char *s);

int fgetc(FILE *fp);
char *fgets(char *buf, int size, FILE *fp);
int getchar(void);
char *gets(char *buf, int size);

int vsnprintf(char *buf, int size, const char *format, va_list args);
int snprintf(char *buf, int size, const char *format, ...);
int vfprintf(FILE *fp, const char *format, va_list args);
int fprintf(FILE *fp, const char *format, ...);
int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

#endif /* _LOLIBC_STDIO_H */
