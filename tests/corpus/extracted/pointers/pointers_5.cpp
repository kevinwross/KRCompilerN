#include <algorithm>
int main() {
    int source[5] = { 1, 2, 1, 3, 1 };
    int target[5] = { 0, 0, 0, 0, 0 };
    int* end = std::remove_copy(source, source + 5, target, 1);
    if (end != target + 2) return 1;
    if (target[0] != 2 || target[1] != 3) return 2;
    char text[6] = "abaca";
    char letters[6] = { 0, 0, 0, 0, 0, 0 };
    if (std::remove_copy(text, text + 5, letters, 'a') != letters + 2) return 3;
    if (letters[0] != 'b' || letters[1] != 'c') return 4;
    double reals[3] = { 1.5, 2.5, 1.5 };
    double copied[3] = { 0.0, 0.0, 0.0 };
    return std::remove_copy(reals, reals + 3, copied, 1.5) == copied + 1 ? 0 : 5;
}
