#include <algorithm>
int main() {
    int source[3] = { 1, 2, 3 };
    int target[3] = { 0, 0, 0 };
    int* end = std::copy(source, source + 3, target);
    if (end != target + 3) return 1;
    if (target[0] != 1 || target[2] != 3) return 2;
    char text[4] = "abc";
    char output[4] = "xxx";
    std::copy(text, text + 3, output);
    if (output[0] != 'a' || output[2] != 'c') return 3;
    double reals[2] = { 1.5, 2.5 };
    double copied[2] = { 0.0, 0.0 };
    std::copy(reals, reals + 2, copied);
    return copied[0] == 1.5 && copied[1] == 2.5 ? 0 : 4;
}
