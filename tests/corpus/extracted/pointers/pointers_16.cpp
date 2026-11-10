#include <algorithm>
int main() {
    int values[6] = { 1, 1, 2, 3, 3, 3 };
    int* end = std::unique(values, values + 6);
    if (end != values + 3) return 1;
    if (values[0] != 1 || values[1] != 2 || values[2] != 3) return 2;
    char text[7] = "aabbcc";
    char* tail = std::unique(text, text + 6);
    if (tail != text + 3) return 3;
    if (text[0] != 'a' || text[1] != 'b' || text[2] != 'c') return 4;
    double reals[4] = { 1.5, 1.5, 2.5, 2.5 };
    if (std::unique(reals, reals + 4) != reals + 2) return 5;
    return std::unique(values, values) == values ? 0 : 6;
}
