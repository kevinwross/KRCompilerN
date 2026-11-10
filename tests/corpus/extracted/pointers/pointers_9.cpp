#include <algorithm>
int main() {
    int source[3] = { 1, 2, 3 };
    int target[3] = { 0, 0, 0 };
    int* end = std::reverse_copy(source, source + 3, target);
    if (end != target + 3) return 1;
    if (target[0] != 3 || target[2] != 1) return 2;
    char text[4] = "abc";
    char letters[4] = { 0, 0, 0, 0 };
    if (std::reverse_copy(text, text + 3, letters) != letters + 3) return 3;
    if (letters[0] != 'c' || letters[2] != 'a') return 4;
    double reals[2] = { 1.5, 2.5 };
    double copied[2] = { 0.0, 0.0 };
    std::reverse_copy(reals, reals + 2, copied);
    return copied[0] == 2.5 && copied[1] == 1.5 ? 0 : 5;
}
