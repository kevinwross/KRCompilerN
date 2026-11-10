#include <cmath>
#include <cmath>
int main() {
    if (std::abs(-3) != 3) return 1;
    if (std::abs(-2.5) != 2.5) return 2;
    if (std::sqrt(9.0) != 3.0) return 3;
    if (std::fabs(-2.0) != 2.0) return 4;
    if (std::sqrt(4.0f) != 2.0f) return 5;
    return 0;
}
