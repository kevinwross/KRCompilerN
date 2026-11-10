#include <cstdio>
#include <cstdio>
int main() {
    char buffer[32];
    std::sprintf(buffer, "%d-%s", 5, "x");
    if (buffer[0] != '5') return 1;
    char other[8];
    int written = std::snprintf(other, 4, "%s", "abcdef");
    if (written != 6) return 2;
    if (other[0] != 'a' || other[2] != 'c' || other[3] != 0) return 3;
    if (sizeof(std::FILE) == 0) return 4;
    return 0;
}
