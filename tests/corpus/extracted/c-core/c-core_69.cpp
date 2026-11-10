#include <algorithm>
int main() {
    unsigned int values[3] = { 9, 2, 7 };
    if (std::min_element(values, values + 3) != values + 1) return 1;
    if (std::max_element(values, values + 3) != values) return 2;
    float reals[3] = { 1.5f, 3.5f, 2.5f };
    if (std::min_element(reals, reals + 3) != reals) return 3;
    return std::max_element(reals, reals + 3) == reals + 1 ? 0 : 4;
}
