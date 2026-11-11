#pragma once

/* Polled serial console. */
#define SERIAL_BASE 0xFFF05000U

struct serial_registers {
    volatile unsigned int data;
    volatile unsigned int status;
};

#define SERIAL_RX_READY 1U
#define SERIAL_TX_READY 2U
#define SERIAL_DATA_MASK 0xFFU

/* Programmable interval timer. */
#define TIMER_BASE 0xFFF00000U

struct timer_registers {
    volatile unsigned int control;
    volatile unsigned int reload;
    volatile unsigned int count;
    volatile unsigned int status;
    volatile unsigned int irq;
};

#define TIMER_CONTROL_ENABLE 1U
#define TIMER_CONTROL_PERIODIC 2U
#define TIMER_CONTROL_IRQ 4U
#define TIMER_STATUS_PENDING 1U

/* IRQ router: route line 0 to core 1. */
#define IRQ_ROUTER_BASE 0xFFF03000U

struct irq_router {
    volatile unsigned int route[16];
};

#define IRQ_ROUTE_TIMER 0U
#define IRQ_CORE1 1U
