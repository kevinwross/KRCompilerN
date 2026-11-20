#ifndef KRC_STDLIB_H
#define KRC_STDLIB_H

#include "stddef.h"

/* Minimal stdlib.h declarations for krc runtime compilation. */

void exit(int status);
void abort(void);
int atoi(const char* nptr);
long atol(const char* nptr);
int abs(int j);
long labs(long j);
void qsort(void* base, size_t nmemb, size_t size, void* compare);
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, void* compare);
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);

#endif /* KRC_STDLIB_H */
