#include <algorithm>
int main() {
    char left[3] = "abc";
    char right[3] = "abd";
    if (!std::lexicographical_compare(left, left + 3, right, right + 3)) return 1;
    if (std::lexicographical_compare(right, right + 3, left, left + 3)) return 2;
    if (std::lexicographical_compare(left, left + 3, left, left + 3)) return 3;
    int shorter[2] = { 1, 2 };
    int longer[3] = { 1, 2, 3 };
    if (!std::lexicographical_compare(shorter, shorter + 2, longer, longer + 3)) return 4;
    double a[2] = { 1.5, 2.5 };
    double b[2] = { 1.5, 3.5 };
    return std::lexicographical_compare(a, a + 2, b, b + 2) ? 0 : 5;
}
