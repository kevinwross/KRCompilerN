/* kevinc preprocessor smoke test. */
#include "pp_include.h"

#define SQUARE(x) ((x) * (x))
#define ADD(a, b) ((a) + (b))
#define DECLARE_COUNTER(name) int name = 0

DECLARE_COUNTER(CAT(my, counter));
char *message = STR(hello preprocessor);

int main(void) {
#if SELECTED
    int value = SQUARE(ADD(1, 2));
    return value + mycounter;
#else
    return -1;
#endif
}
