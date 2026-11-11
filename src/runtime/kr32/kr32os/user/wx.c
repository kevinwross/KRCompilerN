#include "user_abi.h"

int main(void) {
    volatile unsigned int *code = (volatile unsigned int *)USER_CODE_BASE;
    *code = 1;
    return 0;
}
