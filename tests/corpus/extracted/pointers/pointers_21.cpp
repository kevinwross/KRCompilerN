#include <algorithm>
int main() {
    int values[4] = { 0, 0, 0, 0 };
    int* end = std::fill_n(values, 3, 5);
    if (end != values + 3) return 1;
    if (values[2] != 5 || values[3] != 0) return 2;
    double reals[2] = { 0.0, 0.0 };
    std::fill_n(reals, 2, 1.5);
    if (reals[0] != 1.5 || reals[1] != 1.5) return 3;
    return std::fill_n(values, 0, 9) == values ? 0 : 4;
}
