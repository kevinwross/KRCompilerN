#include <algorithm>
int main() {
    int source[3] = { 1, 2, 3 };
    int target[3] = { 0, 0, 0 };
    int* end = std::copy_n(source, 2, target);
    if (end != target + 2) return 1;
    if (target[1] != 2 || target[2] != 0) return 2;
    double reals[2] = { 1.5, 2.5 };
    double out[2] = { 0.0, 0.0 };
    std::copy_n(reals, 2, out);
    if (out[0] != 1.5 || out[1] != 2.5) return 3;
    return std::copy_n(source, 0, target) == target ? 0 : 4;
}
