#include <stdio.h>
struct Guard {
    int value;
    Guard() { value = 42; }
    ~Guard() { putchar('T'); }
};
thread_local Guard guard;
int main() { putchar('B'); return guard.value == 42 ? 0 : 1; }
