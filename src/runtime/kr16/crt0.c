/* crt0.c — KR16 C runtime startup (plan.md §14.2).
 *
 * Entry point called by the VM loader. Sets up the stack, calls main(),
 * and exits through the HAL. After main() returns, calls exit().
 */

#include "hal.h"

extern int main(int argc, char** argv);

void _start(void) {
    int argc = 0;
    char** argv = NULL;

    int rc = main(argc, argv);
    krc_hal_exit(rc);
    /* Unreachable: krc_hal_exit does not return. */
    for (;;) { /* halt */ }
}
