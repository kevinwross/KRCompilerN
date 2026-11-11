#include "mmio.h"
#include "timer.h"

#define TIMER_RELOAD_TICKS 10000U

static unsigned int ticks = 0;

void timer_init(void) {
    volatile struct timer_registers *timer =
        (volatile struct timer_registers *)TIMER_BASE;
    volatile struct irq_router *router =
        (volatile struct irq_router *)IRQ_ROUTER_BASE;
    router->route[IRQ_ROUTE_TIMER] = IRQ_CORE1;
    timer->reload = TIMER_RELOAD_TICKS;
    timer->irq = 0;
    timer->control =
        TIMER_CONTROL_ENABLE | TIMER_CONTROL_PERIODIC | TIMER_CONTROL_IRQ;
}

void timer_tick(void) {
    volatile struct timer_registers *timer =
        (volatile struct timer_registers *)TIMER_BASE;
    timer->status = TIMER_STATUS_PENDING;
    ++ticks;
}

unsigned int timer_ticks(void) {
    return ticks;
}
