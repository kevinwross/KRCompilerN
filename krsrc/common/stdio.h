#ifndef KRC_STDIO_H
#define KRC_STDIO_H

#include "stddef.h"

/* Minimal stdio.h declarations for krc runtime compilation. */

struct FILE;  // opaque

extern struct FILE* stdin;
extern struct FILE* stdout;
extern struct FILE* stderr;

int printf(const char* fmt, ...);
int fprintf(void* stream, const char* fmt, ...);
int puts(const char* s);
int putchar(int c);
int snprintf(char* s, size_t n, const char* fmt, ...);
int sprintf(char* s, const char* fmt, ...);
int vprintf(const char* fmt, ...);

struct FILE* fopen(const char* path, const char* mode);
int fclose(struct FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, struct FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, struct FILE* stream);
int fgetc(struct FILE* stream);
int fputc(int c, struct FILE* stream);
int fputs(const char* s, struct FILE* stream);

#endif /* KRC_STDIO_H */
