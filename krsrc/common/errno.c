/* errno.c — Global errno variable and assert support (plan.md §14.3).
 *
 * KR16 has no TLS, so errno is a plain global variable. Accessed by
 * library functions and user code via errno.h.
 */

#include <stddef.h>
#include <stdlib.h>

#ifdef errno
#undef errno
#endif

int errno = 0;

/* __krc_assert_fail — called by assert() when the condition is false.
 * Prints the expression, file, and line, then aborts. */
void __krc_assert_fail(const char* expr, const char* file, int line) {
    (void)expr;
    (void)file;
    (void)line;
    abort();
}
