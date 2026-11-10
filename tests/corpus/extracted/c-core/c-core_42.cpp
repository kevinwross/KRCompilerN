#include <algorithm>
int main() {
    int values[5] = { 9, 7, 3, 8, 4 };
    if (std::is_heap_until(values, values + 5) != values + 3) return 1;
    char text[4] = "zyxw";
    if (std::is_heap_until(text, text + 4) != text + 4) return 2;
    char broken[4] = "ayzw";
    if (std::is_heap_until(broken, broken + 4) != broken + 1) return 3;
    double reals[3] = { 3.5, 1.5, 2.5 };
    if (std::is_heap_until(reals, reals + 3) != reals + 3) return 4;
    int empty[1] = { 0 };
    return std::is_heap_until(empty, empty) == empty ? 0 : 5;
}
