#include <cstdint>
#include <cstdint>
int main() {
    if (sizeof(std::uint8_t) != 1) return 1;
    if (sizeof(std::uint16_t) != 2) return 2;
    if (sizeof(std::uint32_t) != 4) return 3;
    if (sizeof(std::uint64_t) != 8) return 4;
    std::int64_t value = 123456789;
    std::uint64_t wide = 987654321;
    return value + (std::int64_t)wide == 1111111110 ? 0 : 5;
}
