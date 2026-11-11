int main(void) {
    volatile unsigned int *guard = (volatile unsigned int *)0x00000000;
    *guard = 1;
    return 0;
}
