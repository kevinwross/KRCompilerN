/* crt0.c — KR32 C runtime startup
 *
 * Entry point called by the VM loader. Transfers control to main().
 * After main() returns, halts by spinning forever.
 */

extern int main(void);

void _start(void) {
    main();
halt:
    goto halt;
}
