#include <algorithm>
int main() {
    int left[3] = { 1, 2, 3 };
    int right[3] = { 1, 2, 3 };
    if (!std::equal(left, left + 3, right, right + 3)) return 1;
    int shorter[2] = { 1, 2 };
    if (std::equal(left, left + 3, shorter, shorter + 2)) return 2;
    char a[3] = "ab";
    char b[2] = "ab";
    if (!std::equal(a, a + 2, b, b + 2)) return 3;
    double x[2] = { 1.5, 2.5 };
    double y[2] = { 1.5, 2.5 };
    return std::equal(x, x + 2, y, y + 2) ? 0 : 4;
}
