#include <algorithm>
int main() {
    int left[3] = { 1, 3, 5 };
    int right[3] = { 2, 4, 6 };
    int output[6] = { 0, 0, 0, 0, 0, 0 };
    int* end = std::merge(left, left + 3, right, right + 3, output);
    if (end != output + 6) return 1;
    if (output[0] != 1 || output[1] != 2 || output[2] != 3 || output[5] != 6) return 2;
    char a[3] = "ace";
    char b[3] = "bdf";
    char letters[6] = { 0, 0, 0, 0, 0, 0 };
    if (std::merge(a, a + 3, b, b + 3, letters) != letters + 6) return 3;
    if (letters[0] != 'a' || letters[1] != 'b' || letters[5] != 'f') return 4;
    double x[2] = { 1.5, 3.5 };
    double y[2] = { 2.5, 4.5 };
    double z[4] = { 0.0, 0.0, 0.0, 0.0 };
    return std::merge(x, x + 2, y, y + 2, z) == z + 4 && z[1] == 2.5 ? 0 : 5;
}
