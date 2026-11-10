#include <algorithm>
int main() {
    int values[5] = { 3, 1, 4, 1, 5 };
    std::make_heap(values, values + 5);
    if (!std::is_heap(values, values + 5)) return 1;
    if (values[0] != 5) return 2;
    char text[3] = "cab";
    std::make_heap(text, text + 3);
    if (!std::is_heap(text, text + 3)) return 3;
    double reals[3] = { 1.5, 3.5, 2.5 };
    std::make_heap(reals, reals + 3);
    return std::is_heap(reals, reals + 3) ? 0 : 4;
}
