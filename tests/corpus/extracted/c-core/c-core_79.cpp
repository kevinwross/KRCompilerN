#include <algorithm>
int main() {
    if (std::clamp(9, 1, 4) != 4) return 1;
    if (std::clamp(-2, 0, 5) != 0) return 2;
    if (std::clamp(3, 1, 4) != 3) return 3;
    if (std::clamp(2.5, 1.0, 2.0) != 2.0) return 4;
    return 0;
}
