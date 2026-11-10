#include <algorithm>
int main() {
    int left[4] = { 1, 2, 3, 4 };
    int right[4] = { 4, 3, 2, 1 };
    if (!std::is_permutation(left, left + 4, right)) return 1;
    int other[4] = { 1, 2, 3, 5 };
    if (std::is_permutation(left, left + 4, other)) return 2;
    char text[5] = "aabc";
    char letters[5] = "cbaa";
    if (!std::is_permutation(text, text + 4, letters)) return 3;
    char mismatch[5] = "abcc";
    if (std::is_permutation(text, text + 4, mismatch)) return 4;
    double x[2] = { 1.5, 2.5 };
    double y[2] = { 2.5, 1.5 };
    return std::is_permutation(x, x + 2, y) ? 0 : 5;
}
