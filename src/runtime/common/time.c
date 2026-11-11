/* time.c — Time functions (plan.md §14.3).
 *
 * Implements time() and krc_clock() using the HAL's krc_hal_time() service.
 */

#include "hal.h"

#include <stddef.h>

#define CLOCKS_PER_SEC 1000

long time(long* t) {
    long now = krc_hal_time();
    if (now < 0) return -1;
    if (t) *t = now;
    return now;
}

long krc_clock(void) {
    /* krc_hal_time returns seconds; convert to clock ticks. */
    return krc_hal_time() * CLOCKS_PER_SEC;
}
