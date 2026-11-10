#include <algorithm>
int main() {
    int values[5] = { 3, 1, 4, 1, 5 };
    std::make_heap(values, values + 5);
    std::sort_heap(values, values + 5);
    if (values[0] != 1 || values[1] != 1 || values[4] != 5) return 1;
    char text[3] = "cab";
    std::make_heap(text, text + 3);
    std::sort_heap(text, text + 3);
    if (text[0] != 'a' || text[2] != 'c') return 2;
    double reals[3] = { 1.5, 3.5, 2.5 };
    std::make_heap(reals, reals + 3);
    std::sort_heap(reals, reals + 3);
    return reals[0] == 1.5 && reals[2] == 3.5 ? 0 : 3;
}
