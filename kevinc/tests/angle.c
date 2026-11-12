#include <angle.h>
#if 0
#error this must not fire
#elif defined(ANGLE_VALUE) && ANGLE_VALUE == 9
int main(void) { return ANGLE_VALUE; }
#else
#error wrong branch
#endif
