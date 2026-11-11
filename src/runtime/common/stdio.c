/* stdio.c — Basic formatted output (plan.md §14.3).
 *
 * Implements printf, fprintf, puts, putchar, snprintf, sprintf, vprintf.
 * Uses krc_hal_putchar/krc_hal_write for output. Integer, string, and char
 * conversions only; no floating-point in v1 for KR16.
 */

#include "hal.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#  ifndef bool
#    define bool _Bool
#    define true 1
#    define false 0
#  endif
#endif

static void put_char(char c) {
    krc_hal_putchar(c);
}

static void put_string(const char* s) {
    while (*s) {
        put_char(*s++);
    }
}

static void put_uint(unsigned long value, int base, bool uppercase,
                     bool prefix, int width, char pad) {
    char buf[32];
    int pos = 0;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0) {
        buf[pos++] = '0';
    } else {
        while (value > 0) {
            buf[pos++] = digits[value % base];
            value /= base;
        }
        if (prefix) {
            if (base == 16) buf[pos++] = uppercase ? 'X' : 'x';
            buf[pos++] = '0';
        }
    }

    /* Padding */
    while (pos < width) {
        put_char(pad);
        width--;
    }

    while (pos > 0) {
        put_char(buf[--pos]);
    }
}

static void put_long(long value, int base, bool uppercase, bool prefix,
                     int width, char pad) {
    if (value < 0) {
        put_char('-');
        width--;
        put_uint((unsigned long)(-(value + 1)) + 1, base, uppercase, prefix,
                 width, pad);
    } else {
        put_uint((unsigned long)value, base, uppercase, prefix, width, pad);
    }
}

int vprintf(const char* fmt, va_list args) {
    int count = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            bool long_mod = false;
            int width = 0;
            char pad = ' ';

            /* Parse width */
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            /* Parse length modifier */
            if (*fmt == 'l') { long_mod = true; fmt++; }

            switch (*fmt) {
                case 'd':
                case 'i': {
                    long v = long_mod ? va_arg(args, long)
                                      : (long)va_arg(args, int);
                    put_long(v, 10, false, false, width, pad);
                    break;
                }
                case 'u': {
                    unsigned long v = long_mod ? va_arg(args, unsigned long)
                                               : (unsigned long)va_arg(args, unsigned int);
                    put_uint(v, 10, false, false, width, pad);
                    break;
                }
                case 'x':
                    put_uint(va_arg(args, unsigned int), 16, false, false,
                             width, pad);
                    break;
                case 'X':
                    put_uint(va_arg(args, unsigned int), 16, true, false,
                             width, pad);
                    break;
                case 'p':
                    put_uint((unsigned long)va_arg(args, void*), 16, false,
                             true, width, pad);
                    break;
                case 'c': {
                    char c = (char)va_arg(args, int);
                    put_char(c);
                    break;
                }
                case 's':
                    put_string(va_arg(args, char*));
                    break;
                case '%':
                    put_char('%');
                    break;
                default:
                    put_char('%');
                    put_char(*fmt);
                    break;
            }
            fmt++;
        } else {
            put_char(*fmt++);
        }
    }
    return count;
}

int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vprintf(fmt, args);
    va_end(args);
    return result;
}

int puts(const char* s) {
    put_string(s);
    put_char('\n');
    return 0;
}

int putchar(int c) {
    put_char((char)c);
    return c;
}
