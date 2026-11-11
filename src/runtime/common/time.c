/* time.c — Time functions (plan.md §14.3).
 *
 * Implements time() and clock() using the HAL's krc_hal_time() service.
 */

#include "hal.h"

#include <stddef.h>

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000

time_t time(time_t* t) {
    long now = krc_hal_time();
    if (now < 0) return (time_t)-1;
    if (t) *t = (time_t)now;
    return (time_t)now;
}

clock_t clock(void) {
    /* krc_hal_time returns seconds; convert to clock ticks. */
    return (clock_t)(krc_hal_time() * CLOCKS_PER_SEC);
}
