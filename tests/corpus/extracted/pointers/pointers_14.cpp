#include <algorithm>
int main() {
    int left[3] = { 1, 3, 5 };
    int right[3] = { 2, 3, 6 };
    int output[6] = { 0, 0, 0, 0, 0, 0 };
    int* end = std::set_union(left, left + 3, right, right + 3, output);
    if (end != output + 5) return 1;
    if (output[0] != 1 || output[1] != 2 || output[2] != 3 || output[3] != 5 || output[4] != 6) return 2;
    char a[3] = "ace";
    char b[3] = "bcd";
    char letters[6] = { 0, 0, 0, 0, 0, 0 };
    if (std::set_union(a, a + 3, b, b + 3, letters) != letters + 5) return 3;
    if (letters[0] != 'a' || letters[2] != 'c' || letters[4] != 'e') return 4;
    double x[2] = { 1.5, 3.5 };
    double y[2] = { 2.5, 3.5 };
    double z[4] = { 0.0, 0.0, 0.0, 0.0 };
    return std::set_union(x, x + 2, y, y + 2, z) == z + 3 ? 0 : 5;
}
