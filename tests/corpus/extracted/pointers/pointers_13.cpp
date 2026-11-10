#include <algorithm>
int main() {
    int left[3] = { 1, 3, 5 };
    int right[3] = { 2, 3, 6 };
    int output[3] = { 0, 0, 0 };
    int* end = std::set_intersection(left, left + 3, right, right + 3, output);
    if (end != output + 1 || output[0] != 3) return 1;
    char a[3] = "ace";
    char b[3] = "bcd";
    char letters[3] = { 0, 0, 0 };
    if (std::set_intersection(a, a + 3, b, b + 3, letters) != letters + 1) return 2;
    if (letters[0] != 'c') return 3;
    double x[2] = { 1.5, 3.5 };
    double y[2] = { 2.5, 3.5 };
    double z[2] = { 0.0, 0.0 };
    return std::set_intersection(x, x + 2, y, y + 2, z) == z + 1 && z[0] == 3.5 ? 0 : 4;
}
