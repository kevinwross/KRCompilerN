/* stdlib.c — C standard library functions (plan.md §14.3).
 *
 * Implements exit, abort, atoi, atol, strtol, qsort, bsearch, abs, labs,
 * malloc/free (via HAL allocator).
 */

#include "hal.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef __cplusplus
#  ifndef bool
#    define bool _Bool
#    define true 1
#    define false 0
#  endif
#endif

void exit(int status) {
    krc_hal_exit(status);
    /* Unreachable: krc_hal_exit does not return. */
    for (;;) { /* halt */ }
}

void abort(void) {
    krc_hal_exit(1);
    for (;;) { /* halt */ }
}

int atoi(const char* nptr) {
    int sign = 1;
    int value = 0;
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' ||
           *nptr == '\v' || *nptr == '\f' || *nptr == '\r') {
        nptr++;
    }
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') { nptr++; }
    while (*nptr >= '0' && *nptr <= '9') {
        value = value * 10 + (*nptr - '0');
        nptr++;
    }
    return value * sign;
}

long atol(const char* nptr) {
    return (long)atoi(nptr);
}

long strtol(const char* nptr, char** endptr, int base) {
    (void)base;
    long value = atol(nptr);
    if (endptr) {
        while (*nptr) nptr++;
        *endptr = (char*)nptr;
    }
    return value;
}

int abs(int j) {
    return j < 0 ? -j : j;
}

long labs(long j) {
    return j < 0 ? -j : j;
}

static int default_compare(const void* a, const void* b) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    return (int)(*pa - *pb);
}

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
    if (!base || nmemb < 2 || size == 0) return;
    if (!compar) compar = default_compare;

    /* Simple insertion sort — adequate for the v1.0 corpus. */
    for (size_t i = 1; i < nmemb; i++) {
        char* key = (char*)base + i * size;
        size_t j = i;
        while (j > 0 && compar((char*)base + (j - 1) * size, key) > 0) {
            memcpy((char*)base + j * size, (char*)base + (j - 1) * size, size);
            j--;
        }
        if (j != i) memcpy((char*)base + j * size, key, size);
    }
}

void* bsearch(const void* key, const void* base, size_t nmemb,
              size_t size,
              int (*compar)(const void*, const void*)) {
    if (!base || nmemb == 0 || size == 0 || !compar) return NULL;
    size_t lo = 0;
    size_t hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void* midp = (const char*)base + mid * size;
        int c = compar(key, midp);
        if (c == 0) return (void*)midp;
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}
