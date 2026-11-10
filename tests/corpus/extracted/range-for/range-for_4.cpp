#include <algorithm>
int main() {
    int left[3] = { 1, 2, 3 };
    int right[3] = { 4, 5, 6 };
    int* end = std::swap_ranges(left, left + 3, right);
    if (end != right + 3) return 1;
    if (left[0] != 4 || right[2] != 3) return 2;
    char a[3] = "abc";
    char b[3] = "xyz";
    std::swap_ranges(a, a + 3, b);
    if (a[0] != 'x' || b[2] != 'c') return 3;
    double x[2] = { 1.5, 2.5 };
    double y[2] = { 3.5, 4.5 };
    std::swap_ranges(x, x + 2, y);
    return x[0] == 3.5 && y[1] == 2.5 ? 0 : 4;
}
