#include <algorithm>
int main() {
    int source[3] = { 1, 2, 1 };
    int target[3] = { 0, 0, 0 };
    int* end = std::replace_copy(source, source + 3, target, 1, 9);
    if (end != target + 3) return 1;
    if (target[0] != 9 || target[1] != 2 || target[2] != 9) return 2;
    char text[5] = "abaca";
    char letters[5] = { 0, 0, 0, 0, 0 };
    if (std::replace_copy(text, text + 5, letters, 'a', 'z') != letters + 5) return 3;
    if (letters[0] != 'z' || letters[1] != 'b' || letters[4] != 'z') return 4;
    double reals[3] = { 1.5, 2.5, 1.5 };
    double copied[3] = { 0.0, 0.0, 0.0 };
    std::replace_copy(reals, reals + 3, copied, 1.5, 9.0);
    return copied[0] == 9.0 && copied[1] == 2.5 ? 0 : 5;
}
