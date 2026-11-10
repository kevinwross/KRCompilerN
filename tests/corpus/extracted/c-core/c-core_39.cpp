#include <algorithm>
int main() {
    int values[5] = { 5, 3, 4, 1, 9 };
    std::make_heap(values, values + 4);
    std::push_heap(values, values + 5);
    if (!std::is_heap(values, values + 5)) return 1;
    if (values[0] != 9) return 2;
    char text[3] = "ba";
    std::make_heap(text, text + 2);
    text[2] = 'c';
    std::push_heap(text, text + 3);
    if (text[0] != 'c') return 3;
    double reals[3] = { 1.5, 2.5, 3.5 };
    std::make_heap(reals, reals + 2);
    std::push_heap(reals, reals + 3);
    return std::is_heap(reals, reals + 3) && reals[0] == 3.5 ? 0 : 4;
}
