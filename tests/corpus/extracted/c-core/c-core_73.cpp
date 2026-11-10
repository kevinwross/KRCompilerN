#include <algorithm>
int main() {
    int left[3] = { 1, 2, 3 };
    int right[3] = { 1, 2, 3 };
    if (!std::equal(left, left + 3, right)) return 1;
    right[1] = 9;
    if (std::equal(left, left + 3, right)) return 2;
    char a[4] = "abc";
    char b[4] = "abc";
    if (!std::equal(a, a + 3, b)) return 3;
    double x[2] = { 1.5, 2.5 };
    double y[2] = { 1.5, 2.5 };
    return std::equal(x, x + 2, y) ? 0 : 4;
}
