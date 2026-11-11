/* string.c — C standard string functions (plan.md §14.3).
 *
 * Implements memcpy, memmove, memset, memcmp, strlen, strcpy, strncpy,
 * strcat, strncat, strcmp, strncmp, strchr, strrchr, strstr, strdup,
 * strerror.
 */

#include "hal.h"

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#  ifndef bool
#    define bool _Bool
#    define true 1
#    define false 0
#  endif
#endif

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char v = (unsigned char)c;
    while (n--) *p++ = v;
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return (int)(*p1 - *p2);
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++)) ;
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n-- && (*d++ = *src++)) ;
    while (n--) *d++ = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++)) ;
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++)) ;
    if (n == 0 || d > dest) *d = '\0';
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n == (size_t)-1) return 0;
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

char* strchr(const char* s, int c) {
    char ch = (char)c;
    while (*s) {
        if (*s == ch) return (char*)s;
        s++;
    }
    if (ch == '\0') return (char*)s;
    return NULL;
}

char* strrchr(const char* s, int c) {
    char ch = (char)c;
    char* found = NULL;
    while (*s) {
        if (*s == ch) found = (char*)s;
        s++;
    }
    if (ch == '\0') return (char*)s;
    return found;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return NULL;
}

char* strdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* d = (char*)krc_hal_alloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* strerror is intentionally not implemented in v1 — errno values are
 * numeric only. The declaration is reserved for v1.1. */

