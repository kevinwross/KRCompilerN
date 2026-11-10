#include <algorithm>
int main() {
    int source[6] = { 1, 1, 2, 3, 3, 4 };
    int target[6] = { 0, 0, 0, 0, 0, 0 };
    int* end = std::unique_copy(source, source + 6, target);
    if (end != target + 4) return 1;
    if (target[0] != 1 || target[1] != 2 || target[2] != 3 || target[3] != 4) return 2;
    char text[7] = "aabbcc";
    char letters[7] = { 0, 0, 0, 0, 0, 0, 0 };
    if (std::unique_copy(text, text + 6, letters) != letters + 3) return 3;
    if (letters[0] != 'a' || letters[2] != 'c') return 4;
    double reals[4] = { 1.5, 1.5, 2.5, 2.5 };
    double copied[4] = { 0.0, 0.0, 0.0, 0.0 };
    return std::unique_copy(reals, reals + 4, copied) == copied + 2 ? 0 : 5;
}
