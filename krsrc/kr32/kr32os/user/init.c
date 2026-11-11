#include "user_abi.h"

#define INIT_MESSAGE "init: hello from user mode\n"
#define INIT_MESSAGE_LENGTH 27
#define INIT_IDENTITY "init: pid 1\n"
#define INIT_IDENTITY_LENGTH 12

int sys_write(int descriptor, const char *buffer, int length);
int sys_read(int descriptor, char *buffer, int length);
int sys_yield(void);
int sys_getpid(void);
int sys_brk(unsigned int address);

int main(void) {
    sys_write(1, INIT_MESSAGE, INIT_MESSAGE_LENGTH);
    if (sys_getpid() == 1) {
        sys_write(1, INIT_IDENTITY, INIT_IDENTITY_LENGTH);
    }
    (void)sys_yield();
    (void)sys_read(0, (char *)0, 0);
    (void)sys_brk(USER_CODE_BASE);
    return 0;
}
