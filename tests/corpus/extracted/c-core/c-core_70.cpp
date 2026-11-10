#include <algorithm>
int main() {
    int values[4] = { 3, 9, 2, 7 };
    if (std::min_element(values, values + 4) != values + 2) return 1;
    if (std::max_element(values, values + 4) != values + 1) return 2;
    char text[4] = "dcba";
    if (std::min_element(text, text + 4) != text + 3) return 3;
    double reals[3] = { 1.5, 0.5, 2.5 };
    if (std::max_element(reals, reals + 3) != reals + 2) return 4;
    return std::max_element(values, values) == values ? 0 : 5;
}
