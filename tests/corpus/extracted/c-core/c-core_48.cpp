#include <algorithm>
int main() {
    int values[5] = { 5, 1, 4, 2, 3 };
    std::nth_element(values, values + 2, values + 5);
    if (values[2] != 3) return 1;
    int sorted[5] = { 5, 1, 4, 2, 3 };
    std::partial_sort(sorted, sorted + 3, sorted + 5);
    if (sorted[0] != 1 || sorted[1] != 2 || sorted[2] != 3) return 2;
    char text[4] = "dcba";
    std::nth_element(text, text + 2, text + 4);
    if (text[2] != 'c') return 3;
    double reals[3] = { 2.5, 0.5, 1.5 };
    std::nth_element(reals, reals + 1, reals + 3);
    return reals[1] == 1.5 ? 0 : 4;
}
