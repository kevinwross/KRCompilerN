#include <algorithm>
int main() {
    int values[5] = { 1, 2, 4, 3, 5 };
    if (std::is_sorted_until(values, values + 5) != values + 3) return 1;
    char text[5] = "abdc";
    if (std::is_sorted_until(text, text + 4) != text + 3) return 2;
    double reals[3] = { 1.5, 3.5, 2.5 };
    if (std::is_sorted_until(reals, reals + 3) != reals + 2) return 3;
    int sorted[3] = { 1, 2, 3 };
    if (std::is_sorted_until(sorted, sorted + 3) != sorted + 3) return 4;
    return std::is_sorted_until(sorted, sorted) == sorted ? 0 : 5;
}
