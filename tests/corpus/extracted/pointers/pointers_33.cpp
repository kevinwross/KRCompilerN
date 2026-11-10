#include <stdio.h>
int main() {
    long long value = 100000;
    long long big = value * value;
    printf("%lld %llx\n", big, big);
    return 0;
}
