#include <cstddef>
#include <cstddef>
int main() {
    if (sizeof(std::size_t) != 4) return 1;
    if (sizeof(std::ptrdiff_t) != 4) return 2;
    std::size_t size = 9;
    std::ptrdiff_t delta = -4;
    return size + delta == 5 ? 0 : 3;
}
