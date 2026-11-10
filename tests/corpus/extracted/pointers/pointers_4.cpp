#include <algorithm>
int main() {
    int source[3] = { 1, 2, 3 };
    int target[3] = { 0, 0, 0 };
    int* end = std::copy_backward(source, source + 3, target + 3);
    if (end != target) return 1;
    if (target[0] != 1 || target[2] != 3) return 2;
    char text[6] = "abc";
    char letters[6] = { 0, 0, 0, 0, 0, 0 };
    if (std::copy_backward(text, text + 3, letters + 5) != letters + 2) return 3;
    if (letters[2] != 'a' || letters[4] != 'c') return 4;
    double reals[2] = { 1.5, 2.5 };
    double copied[2] = { 0.0, 0.0 };
    std::copy_backward(reals, reals + 2, copied + 2);
    if (copied[0] != 1.5 || copied[1] != 2.5) return 5;
    int values[4] = { 1, 2, 3, 0 };
    std::copy_backward(values, values + 3, values + 4);
    return values[1] == 1 && values[3] == 3 ? 0 : 6;
}
