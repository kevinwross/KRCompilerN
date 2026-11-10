#include <algorithm>
int main() {
    int left[3] = { 1, 3, 5 };
    int right[2] = { 3, 6 };
    int output[3] = { 0, 0, 0 };
    int* end = std::set_difference(left, left + 3, right, right + 2, output);
    if (end != output + 2) return 1;
    if (output[0] != 1 || output[1] != 5) return 2;
    char a[3] = "ace";
    char b[3] = "bcd";
    char letters[3] = { 0, 0, 0 };
    if (std::set_difference(a, a + 3, b, b + 3, letters) != letters + 2) return 3;
    if (letters[0] != 'a' || letters[1] != 'e') return 4;
    double x[2] = { 1.5, 2.5 };
    double y[2] = { 1.5, 3.5 };
    double z[2] = { 0.0, 0.0 };
    return std::set_difference(x, x + 2, y, y + 2, z) == z + 1 && z[0] == 2.5 ? 0 : 5;
}
