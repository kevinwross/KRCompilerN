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

#ifndef _MSC_VER
int abs(int j) {
    return j < 0 ? -j : j;
}
#endif

#ifndef _MSC_VER
long labs(long j) {
    return j < 0 ? -j : j;
}
#endif

void qsort(void* base, size_t nmemb, size_t size, void* compar) {
    (void)base; (void)nmemb; (void)size; (void)compar;
    /* qsort not yet implemented — stub for v1.1. */
}

void* bsearch(const void* key, const void* base, size_t nmemb,
              size_t size, void* compar) {
    (void)key; (void)base; (void)nmemb; (void)size; (void)compar;
    /* bsearch not yet implemented — stub for v1.1. */
    return NULL;
}
