#include <algorithm>
#include <algorithm>
int main() {
    if (std::min(3, 5) != 3) return 1;
    if (std::max(3, 5) != 5) return 2;
    if (std::min(2.5, 1.5) != 1.5) return 3;
    if (std::max(2.5, 1.5) != 2.5) return 4;
    if (std::min('b', 'a') != 'a') return 5;
    return 0;
}
