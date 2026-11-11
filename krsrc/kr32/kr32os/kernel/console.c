#include "console.h"
#include "mmio.h"

void console_putc(char character) {
    volatile struct serial_registers *serial =
        (volatile struct serial_registers *)SERIAL_BASE;
    while ((serial->status & SERIAL_TX_READY) == 0) {
    }
    serial->data = (unsigned int)character;
}

void console_write(const char *text) {
    while (*text != '\0') {
        console_putc(*text);
        ++text;
    }
}

void console_write_uint(unsigned int value) {
    char buffer[16];
    int length = 0;
    if (value == 0) {
        console_putc('0');
        return;
    }
    while (value > 0 && length < (int)sizeof(buffer)) {
        buffer[length] = (char)('0' + (value % 10));
        value /= 10;
        ++length;
    }
    while (length > 0) {
        --length;
        console_putc(buffer[length]);
    }
}

int console_getc(void) {
    volatile struct serial_registers *serial =
        (volatile struct serial_registers *)SERIAL_BASE;
    if ((serial->status & SERIAL_RX_READY) == 0) {
        return -1;
    }
    return (int)(serial->data & SERIAL_DATA_MASK);
}

void console_clear(void) {
    console_putc('\f');
}
