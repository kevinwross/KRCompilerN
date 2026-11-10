#include <algorithm>
int main() {
    int values[5] = { 1, 3, 3, 7, 9 };
    if (!std::binary_search(values, values + 5, 3)) return 1;
    if (std::binary_search(values, values + 5, 4)) return 2;
    if (std::lower_bound(values, values + 5, 3) != values + 1) return 3;
    if (std::upper_bound(values, values + 5, 3) != values + 3) return 4;
    char text[5] = "acegi";
    if (!std::binary_search(text, text + 5, 'e')) return 5;
    double reals[4] = { 0.5, 1.5, 2.5, 3.5 };
    return std::binary_search(reals, reals + 4, 2.5) ? 0 : 6;
}
