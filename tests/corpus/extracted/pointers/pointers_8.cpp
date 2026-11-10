#include <algorithm>
int main() {
    int source[5] = { 1, 2, 3, 4, 5 };
    int target[5] = { 0, 0, 0, 0, 0 };
    int* end = std::rotate_copy(source, source + 2, source + 5, target);
    if (end != target + 5) return 1;
    if (target[0] != 3 || target[2] != 5 || target[3] != 1) return 2;
    char text[5] = "abcde";
    char letters[5] = { 0, 0, 0, 0, 0 };
    if (std::rotate_copy(text, text + 1, text + 5, letters) != letters + 5) return 3;
    if (letters[0] != 'b' || letters[4] != 'a') return 4;
    double reals[3] = { 1.5, 2.5, 3.5 };
    double copied[3] = { 0.0, 0.0, 0.0 };
    std::rotate_copy(reals, reals + 2, reals + 3, copied);
    return copied[0] == 3.5 && copied[1] == 1.5 ? 0 : 5;
}
